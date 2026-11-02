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

#include <tinyalsa/asoundlib.h>

#include "PalCommon.h"

#define TFA_DEFAULT_IMPEDANCE_CTL "TFA Default Impedance"
#define TFA_CALIBRATION_CTL "TFA Calibration"
#define TFA_F0_CALIBRATION_CTL "TFA F0 Calibration"

#define TFA_FIRST_ADDRESS 0x34
#define TFA_LAST_ADDRESS 0x37
#define TFA_SPEAKER_COUNT 2

#define PARAM_ID_TFA_DSP_CMD 0x1800b921

#define PAYLOAD_ALIGN(num, to) (((num) + ((to) - 1)) & ~((to) - 1))

// Matches apm_module_param_data_t.
struct ParamHeader {
    uint32_t module_instance_id;
    uint32_t param_id;
    uint32_t param_size;
    uint32_t error_code;
};

bool OplusSpeakerTfa98xx::probe(struct mixer *hwMixer)
{
    return mixer_get_ctl_by_name(hwMixer, TFA_CALIBRATION_CTL) != nullptr;
}

uint32_t OplusSpeakerTfa98xx::speakerCount() const
{
    return TFA_SPEAKER_COUNT;
}

int OplusSpeakerTfa98xx::readCalibration(struct mixer *hwMixer)
{
    struct mixer_ctl *defaultCtl, *calCtl, *f0Ctl;

    defaultCtl = mixer_get_ctl_by_name(hwMixer, TFA_DEFAULT_IMPEDANCE_CTL);
    calCtl = mixer_get_ctl_by_name(hwMixer, TFA_CALIBRATION_CTL);
    if (!defaultCtl || !calCtl) {
        PAL_ERR(LOG_TAG, "Missing the TFA calibration controls");
        return -ENODEV;
    }

    if (mixer_ctl_get_num_values(defaultCtl) != TFA_SPEAKER_COUNT) {
        PAL_ERR(LOG_TAG, "Unexpected speaker count %d", mixer_ctl_get_num_values(defaultCtl));
        return -EINVAL;
    }

    std::vector<Calibration> calibration;
    for (uint32_t addr = TFA_FIRST_ADDRESS; addr <= TFA_LAST_ADDRESS; addr++) {
        char path[64];
        Calibration info = {};

        snprintf(path, sizeof(path), "/proc/tfa98xx-%x/cali_info", addr);
        FILE *fp = fopen(path, "r");
        if (!fp)
            continue;

        int count = fscanf(fp, "0x%x, %u, %d, %d", &info.address, &info.index,
                           &info.minImpedance, &info.maxImpedance);
        fclose(fp);
        if (count != 4) {
            PAL_ERR(LOG_TAG, "Failed to parse %s", path);
            return -EINVAL;
        }
        calibration.push_back(info);
    }

    if (calibration.size() != TFA_SPEAKER_COUNT) {
        PAL_ERR(LOG_TAG, "Found %zu amplifiers", calibration.size());
        return -EINVAL;
    }

    std::sort(calibration.begin(), calibration.end(),
              [](const Calibration &a, const Calibration &b) { return a.index < b.index; });

    for (uint32_t i = 0; i < calibration.size(); i++) {
        Calibration &info = calibration[i];
        int value = mixer_ctl_get_value(calCtl, info.index);

        // Never run with a value that can't be right, the algorithm depends on it.
        if (info.index != i || info.minImpedance <= 0 || value < info.minImpedance ||
            value > info.maxImpedance) {
            PAL_ERR(LOG_TAG, "Invalid calibration for 0x%x: idx %u, %d not in [%d, %d]",
                    info.address, info.index, value, info.minImpedance, info.maxImpedance);
            return -ERANGE;
        }

        info.dspImpedance = (static_cast<uint64_t>(value) << 16) / 1000;
        PAL_INFO(LOG_TAG, "addr 0x%x idx %u cal_imp %d dsp_imp 0x%x", info.address, info.index,
                 value, info.dspImpedance);
    }

    mF0 = 0;
    f0Ctl = mixer_get_ctl_by_name(hwMixer, TFA_F0_CALIBRATION_CTL);
    if (f0Ctl) {
        int value = mixer_ctl_get_value(f0Ctl, 0);
        if (value > 0)
            mF0 = (static_cast<uint32_t>(value) << 12) + 0x96000;
    }
    PAL_INFO(LOG_TAG, "f0 0x%x", mF0);

    mCalibration = std::move(calibration);
    return 0;
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
    uint8_t cmd[32] = {};
    size_t size = 0;

    if (mCalibration.empty())
        return -EINVAL;

    if (mF0) {
        const uint8_t f0Cmd[] = {0x00, 0x80, 0x15, 0x00, 0x00, 0x02, 0x00, 0x81, 0x1c};
        const uint8_t separator[] = {0x00, 0x00, 0x03};

        memcpy(cmd, f0Cmd, sizeof(f0Cmd));
        size = sizeof(f0Cmd);
        size += putTfaValue(cmd + size, mF0);
        memcpy(cmd + size, separator, sizeof(separator));
        size += sizeof(separator);
    }

    const uint8_t impedanceCmd[] = {0x00, 0x81, 0x05};
    memcpy(cmd + size, impedanceCmd, sizeof(impedanceCmd));
    size += sizeof(impedanceCmd);
    for (const auto &info : mCalibration)
        size += putTfaValue(cmd + size, info.dspImpedance);

    // Stock pads the message with a zero word when it holds several commands.
    if (mF0)
        size += 3;

    const size_t paramSize = PAYLOAD_ALIGN(size, 4);
    param.assign(PAYLOAD_ALIGN(sizeof(ParamHeader) + paramSize, 8), 0);

    auto *header = reinterpret_cast<ParamHeader *>(param.data());
    header->module_instance_id = miid;
    header->param_id = PARAM_ID_TFA_DSP_CMD;
    header->param_size = paramSize;
    header->error_code = 0;
    memcpy(param.data() + sizeof(*header), cmd, size);
    return 0;
}
