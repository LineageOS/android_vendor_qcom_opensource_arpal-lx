/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: OplusSpeakerTfa98xx"

#include "OplusSpeakerTfa98xx.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <tinyalsa/asoundlib.h>

#include "PalCommon.h"

#define TFA_DEFAULT_IMPEDANCE_CTL "TFA Default Impedance"
#define TFA_CALIBRATION_CTL "TFA Calibration"
#define TFA_F0_CALIBRATION_CTL "TFA F0 Calibration"

#define TFA_CALI_FILE "/mnt/vendor/persist/media/tfa_cali.bin"
#define TFA_CALI_F0_FILE "/mnt/vendor/persist/media/tfa_cali_f0.bin"
#define TFA_CALI_FILE_FIELD 10

#define TFA_FIRST_ADDRESS 0x34
#define TFA_LAST_ADDRESS 0x37
#define TFA_MAX_INDEX 3
#define TFA_MAX_SPEAKERS 4
#define TFA_MAX_DSP_INSTANCES 2
#define TFA_READ_RETRY_US 50000

#define PARAM_ID_TFA_DSP_CMD 0x1800b921

#define PAYLOAD_ALIGN(num, to) (((num) + ((to) - 1)) & ~((to) - 1))

// Matches apm_module_param_data_t.
struct ParamHeader {
    uint32_t module_instance_id;
    uint32_t param_id;
    uint32_t param_size;
    uint32_t error_code;
};

static FILE *openTfaNode(uint32_t addr, const char *node)
{
    char path[64];

    snprintf(path, sizeof(path), "/proc/tfa98xx-%x/%s", addr, node);
    FILE *fp = fopen(path, "r");
    if (fp)
        return fp;

    snprintf(path, sizeof(path), "/sys/kernel/debug/tfa98xx-%x/%s", addr, node);
    return fopen(path, "r");
}

static bool readTfaLine(FILE *fp, char *buf, int size)
{
    for (int i = 0; i < 3; i++) {
        if (fgets(buf, size, fp))
            return true;
        if (i < 2)
            usleep(TFA_READ_RETRY_US);
    }
    return false;
}

// The calibration files hold one fixed width field per amplifier.
static int readCaliFile(const char *path, uint32_t index)
{
    char buf[TFA_CALI_FILE_FIELD + 1] = {};
    int value = 0;

    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;

    ssize_t ret = pread(fd, buf, TFA_CALI_FILE_FIELD, index * TFA_CALI_FILE_FIELD);
    close(fd);
    if (ret <= 0 || sscanf(buf, "%10d", &value) != 1)
        return 0;
    return value;
}

static int toDspImpedance(int mohms)
{
    return (mohms << 16) / 1000;
}

bool OplusSpeakerTfa98xx::probe(struct mixer *hwMixer)
{
    return mixer_get_ctl_by_name(hwMixer, TFA_DEFAULT_IMPEDANCE_CTL) != nullptr;
}

void OplusSpeakerTfa98xx::initCalibrationInfo(struct mixer *hwMixer)
{
    struct mixer_ctl *defaultCtl = mixer_get_ctl_by_name(hwMixer, TFA_DEFAULT_IMPEDANCE_CTL);
    char line[64];

    mSpeakerCount = defaultCtl ? mixer_ctl_get_num_values(defaultCtl) : 0;
    if (!defaultCtl)
        PAL_ERR(LOG_TAG, "Missing %s", TFA_DEFAULT_IMPEDANCE_CTL);
    mDspInstanceCount = std::min<uint32_t>((mSpeakerCount + 1) / 2, TFA_MAX_DSP_INSTANCES);

    mCalibration.clear();
    for (uint32_t addr = TFA_FIRST_ADDRESS; addr <= TFA_LAST_ADDRESS; addr++) {
        FILE *fp = openTfaNode(addr, "cali_info");
        if (!fp)
            continue;

        bool read = readTfaLine(fp, line, sizeof(line));
        fclose(fp);
        if (!read) {
            PAL_ERR(LOG_TAG, "Failed to read the cali_info of 0x%x", addr);
            continue;
        }

        uint32_t address = 0, index = 0;
        int min = 0, max = 0;
        if (sscanf(line, "0x%x, %u, %d, %d", &address, &index, &min, &max) != 4) {
            PAL_ERR(LOG_TAG, "Failed to parse the cali_info of 0x%x", addr);
            continue;
        }
        if (index > TFA_MAX_INDEX) {
            PAL_ERR(LOG_TAG, "Invalid index %u for 0x%x", index, addr);
            continue;
        }
        if (std::any_of(mCalibration.begin(), mCalibration.end(),
                        [index](const Calibration &c) { return c.index == index; })) {
            PAL_ERR(LOG_TAG, "dev_idx(%u) is equal to an existing one", index);
            continue;
        }
        mCalibration.push_back({index, min, max, 0, 0, 0});
    }

    if (mCalibration.empty()) {
        // Older drivers only report the impedance range.
        for (uint32_t addr = TFA_FIRST_ADDRESS; addr <= TFA_LAST_ADDRESS; addr++) {
            FILE *fp = openTfaNode(addr, "range");
            if (!fp)
                continue;

            int min = 0, max = 0;
            bool read = readTfaLine(fp, line, sizeof(line));
            fclose(fp);
            if (read && sscanf(line, " Min:%d mOhms, Max:%d mOhms", &min, &max) == 2)
                mCalibration.push_back(
                        {static_cast<uint32_t>(mCalibration.size()), min, max, 0, 0, 0});
        }
    } else {
        std::sort(mCalibration.begin(), mCalibration.end(),
                  [](const Calibration &a, const Calibration &b) { return a.index < b.index; });
    }

    if (mCalibration.size() != mSpeakerCount)
        PAL_ERR(LOG_TAG, "Found %zu amplifiers for %u speakers", mCalibration.size(),
                mSpeakerCount);

    for (auto &info : mCalibration) {
        int value = 0;
        if (defaultCtl && info.index < mSpeakerCount)
            value = mixer_ctl_get_value(defaultCtl, info.index);
        info.defaultImpedance =
                (info.minImpedance < value && value < info.maxImpedance) ? value : 0;
        PAL_INFO(LOG_TAG, "idx %u range [%d, %d] default %d", info.index, info.minImpedance,
                 info.maxImpedance, info.defaultImpedance);
    }
}

void OplusSpeakerTfa98xx::readF0(struct mixer *hwMixer, bool fromMixer)
{
    struct mixer_ctl *f0Ctl = mixer_get_ctl_by_name(hwMixer, TFA_F0_CALIBRATION_CTL);

    mF0.assign(mDspInstanceCount, 0);
    if (!f0Ctl)
        return;

    uint32_t count = std::min<uint32_t>((mixer_ctl_get_num_values(f0Ctl) + 1) / 2,
                                        mDspInstanceCount);
    for (uint32_t i = 0; i < count; i++) {
        if (fromMixer) {
            int value = mixer_ctl_get_value(f0Ctl, 2 * i);
            if (value >= 1)
                mF0[i] = (static_cast<uint32_t>(value) << 12) + 0x96000;
            else if (i > 0)
                mF0[i] = mF0[i - 1];
        } else {
            mF0[i] = static_cast<uint32_t>(readCaliFile(TFA_CALI_F0_FILE, i)) << 11;
        }
        PAL_INFO(LOG_TAG, "f0[%u] 0x%x", i, mF0[i]);
    }
}

bool OplusSpeakerTfa98xx::readCalibration(struct mixer *hwMixer)
{
    if (!mInitialized) {
        initCalibrationInfo(hwMixer);
        mInitialized = true;
    }

    struct mixer_ctl *calCtl = mixer_get_ctl_by_name(hwMixer, TFA_CALIBRATION_CTL);
    bool calibrated = true;

    for (auto &info : mCalibration) {
        int value = calCtl ? mixer_ctl_get_value(calCtl, info.index)
                           : readCaliFile(TFA_CALI_FILE, info.index);
        if (value >= 1)
            info.calImpedance = value;

        if (info.minImpedance < value && value < info.maxImpedance) {
            info.dspImpedance = toDspImpedance(value);
        } else {
            // Not calibrated yet, try again on the next start.
            if (value == 0)
                calibrated = false;
            if (info.defaultImpedance)
                info.dspImpedance = toDspImpedance(info.defaultImpedance);
            else if (info.maxImpedance >= 1 && info.minImpedance >= 1)
                info.dspImpedance = toDspImpedance((info.minImpedance + info.maxImpedance) / 2);
            else
                info.dspImpedance = 0;
            PAL_ERR(LOG_TAG, "idx %u calibration %d not in [%d, %d], using 0x%x", info.index,
                    value, info.minImpedance, info.maxImpedance, info.dspImpedance);
        }
        PAL_INFO(LOG_TAG, "idx %u cal_imp %d dsp_imp 0x%x", info.index, value,
                 info.dspImpedance);
    }

    readF0(hwMixer, calCtl != nullptr);
    return calibrated;
}

void OplusSpeakerTfa98xx::setGain(bool enabled, int level)
{
    mGainEnabled = enabled;
    mGain = static_cast<uint8_t>(level);
}

static size_t putTfaValue(uint8_t *buffer, uint32_t value)
{
    buffer[0] = (value >> 16) & 0xff;
    buffer[1] = (value >> 8) & 0xff;
    buffer[2] = value & 0xff;
    return 3;
}

int OplusSpeakerTfa98xx::buildCalibrationParam(uint32_t miid, std::vector<uint8_t> &param)
{
    const size_t count = mCalibration.size();
    if (count < 1 || count > TFA_MAX_SPEAKERS || count != mSpeakerCount) {
        PAL_ERR(LOG_TAG, "No payload for %zu amplifiers and %u speakers", count, mSpeakerCount);
        return -ENODATA;
    }

    int impedance[TFA_MAX_SPEAKERS + 1] = {};
    for (size_t i = 0; i < count; i++) {
        impedance[i] = mCalibration[i].dspImpedance;
        if (!impedance[i]) {
            PAL_ERR(LOG_TAG, "No cal_imp/def_imp for idx %u", mCalibration[i].index);
            return -ENODATA;
        }
    }
    // A single amplifier on an instance gets the same value twice.
    if (count % 2)
        impedance[count] = impedance[count - 1];

    param.clear();
    for (uint32_t i = 0; i < mDspInstanceCount; i++) {
        const uint8_t dev = i << 4;
        const uint32_t f0 = i < mF0.size() ? mF0[i] : 0;
        uint8_t cmd[64] = {};
        size_t size = 0;

        // Several commands in one message start with a header, each next one
        // is preceded by its marker.
        if (f0 || mGainEnabled) {
            const uint8_t header[] = {dev, 0x80, 0x15};
            memcpy(cmd + size, header, sizeof(header));
            size += sizeof(header);
        }
        if (f0) {
            const uint8_t f0Cmd[] = {0x00, 0x00, 0x02, dev, 0x81, 0x1c};
            memcpy(cmd + size, f0Cmd, sizeof(f0Cmd));
            size += sizeof(f0Cmd);
            size += putTfaValue(cmd + size, f0);
        }
        if (mGainEnabled) {
            const uint8_t gainCmd[] = {0x00, 0x00, 0x03, dev,  0x81, 0x04,
                                       0x00, 0x00, mGain, 0x00, 0x00, mGain};
            memcpy(cmd + size, gainCmd, sizeof(gainCmd));
            size += sizeof(gainCmd);
        }

        const bool multi = size != 0;
        if (multi) {
            const uint8_t marker[] = {0x00, 0x00, 0x03};
            memcpy(cmd + size, marker, sizeof(marker));
            size += sizeof(marker);
        }

        const uint8_t impedanceCmd[] = {dev, 0x81, 0x05};
        memcpy(cmd + size, impedanceCmd, sizeof(impedanceCmd));
        size += sizeof(impedanceCmd);
        size += putTfaValue(cmd + size, impedance[2 * i]);
        size += putTfaValue(cmd + size, impedance[2 * i + 1]);

        // Stock pads the message with a zero word when it holds several commands.
        if (multi)
            size += 3;

        const size_t paramSize = PAYLOAD_ALIGN(size, 4);
        const size_t offset = param.size();
        param.resize(offset + PAYLOAD_ALIGN(sizeof(ParamHeader) + paramSize, 8), 0);

        auto *header = reinterpret_cast<ParamHeader *>(param.data() + offset);
        header->module_instance_id = miid;
        header->param_id = PARAM_ID_TFA_DSP_CMD;
        header->param_size = paramSize;
        header->error_code = 0;
        memcpy(param.data() + offset + sizeof(*header), cmd, size);
    }
    return 0;
}

int OplusSpeakerTfa98xx::buildGainParam(uint32_t miid, std::vector<uint8_t> &param)
{
    if (!mGainEnabled || !mDspInstanceCount)
        return -ENODATA;

    param.clear();
    for (uint32_t i = 0; i < mDspInstanceCount; i++) {
        const uint8_t dev = i << 4;
        const uint8_t cmd[] = {dev, 0x81, 0x04, 0x00, 0x00, mGain, 0x00, 0x00, mGain};
        const size_t paramSize = PAYLOAD_ALIGN(sizeof(cmd), 4);
        const size_t offset = param.size();

        param.resize(offset + PAYLOAD_ALIGN(sizeof(ParamHeader) + paramSize, 8), 0);
        auto *header = reinterpret_cast<ParamHeader *>(param.data() + offset);
        header->module_instance_id = miid;
        header->param_id = PARAM_ID_TFA_DSP_CMD;
        header->param_size = paramSize;
        header->error_code = 0;
        memcpy(param.data() + offset + sizeof(*header), cmd, sizeof(cmd));
    }
    return 0;
}
