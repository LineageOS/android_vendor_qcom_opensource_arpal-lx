/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: OplusSpeakerAwinic"

#include "OplusSpeakerAwinic.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <tinyalsa/asoundlib.h>

#include "PalCommon.h"

#define AW_MONITOR_TIME_CTL "aw882xx_hal_monitor_time"
#define AW_DEV_MONITOR_CTL "aw_dev_%u_monitor"
#define AW_F0_SWITCH_CTL "aw882xx_f0_switch"

#define AW_SMARTPA_NODE "/dev/aw882xx_smartpa"
#define AW_CALI_RE_FILE "/mnt/vendor/persist/media/aw_cali.bin"
#define AW_CALI_F0_FILE "/mnt/vendor/persist/media/aw_cali_f0.bin"
#define AW_CALI_FILE_FIELD 10

#define AW_MAX_DEV 8

#define PARAM_ID_AW_CALI_RE 0x10013d32
#define PARAM_ID_AW_CALI_RE_F0 0x10013d4d

#define PAYLOAD_ALIGN(num, to) (((num) + ((to) - 1)) & ~((to) - 1))

static const char *const kChannelNames[AW_MAX_DEV] = {
    "pri_l", "pri_r", "sec_l", "sec_r", "tert_l", "tert_r", "quat_l", "quat_r",
};

// Matches apm_module_param_data_t.
struct ParamHeader {
    uint32_t module_instance_id;
    uint32_t param_id;
    uint32_t param_size;
    uint32_t error_code;
};

struct CaliRe {
    int32_t channel;
    int32_t re;
};

struct CaliReF0 {
    int32_t channel;
    int32_t re;
    int32_t f0;
    int32_t reserved[3];
};

bool OplusSpeakerAwinic::probe(struct mixer *hwMixer)
{
    char name[32];

    if (!mixer_get_ctl_by_name(hwMixer, AW_MONITOR_TIME_CTL))
        return false;

    mDevNum = 0;
    for (uint32_t i = 0; i < AW_MAX_DEV; i++) {
        snprintf(name, sizeof(name), AW_DEV_MONITOR_CTL, i);
        if (!mixer_get_ctl_by_name(hwMixer, name))
            break;
        mDevNum++;
    }

    return mDevNum > 0;
}

// The node answers the last command written to it.
std::string OplusSpeakerAwinic::readNode(const char *cmd)
{
    char buf[256] = {};

    int fd = open(AW_SMARTPA_NODE, O_RDWR);
    if (fd < 0) {
        PAL_ERR(LOG_TAG, "Failed to open %s: %s", AW_SMARTPA_NODE, strerror(errno));
        return "";
    }
    ssize_t ret = write(fd, cmd, strlen(cmd) + 1);
    close(fd);
    if (ret < 0) {
        PAL_ERR(LOG_TAG, "Failed to write %s: %s", cmd, strerror(errno));
        return "";
    }

    fd = open(AW_SMARTPA_NODE, O_RDWR);
    if (fd < 0)
        return "";
    ret = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (ret <= 0) {
        PAL_ERR(LOG_TAG, "Failed to read %s", cmd);
        return "";
    }

    return std::string(buf, ret);
}

int OplusSpeakerAwinic::readRanges(const char *cmd, const char *format, std::vector<Range> &ranges)
{
    std::string reply = readNode(cmd);

    ranges.assign(mDevNum, Range{0, 0});
    for (uint32_t i = 0; i < mDevNum; i++) {
        std::string tag = std::string(kChannelNames[i]) + ":";
        size_t pos = reply.find(tag);
        if (pos == std::string::npos ||
            sscanf(reply.c_str() + pos + tag.size(), format, &ranges[i].min, &ranges[i].max) != 2) {
            PAL_ERR(LOG_TAG, "No %s for %s", cmd, kChannelNames[i]);
            continue;
        }
    }
    return 0;
}

int OplusSpeakerAwinic::readDefaults(const char *cmd, const char *format, std::vector<int> &values)
{
    std::string reply = readNode(cmd);

    values.assign(mDevNum, 0);
    for (uint32_t i = 0; i < mDevNum; i++) {
        std::string tag = std::string(kChannelNames[i]) + ":";
        size_t pos = reply.find(tag);
        if (pos == std::string::npos ||
            sscanf(reply.c_str() + pos + tag.size(), format, &values[i]) != 1)
            values[i] = 0;
    }
    return 0;
}

int OplusSpeakerAwinic::readFileValue(const char *path, uint32_t slot, int *value)
{
    char buf[AW_CALI_FILE_FIELD + 1] = {};

    *value = 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -errno;

    ssize_t ret = pread(fd, buf, AW_CALI_FILE_FIELD, slot * AW_CALI_FILE_FIELD);
    close(fd);
    if (ret != AW_CALI_FILE_FIELD || sscanf(buf, "%10d", value) != 1) {
        *value = 0;
        return -EINVAL;
    }
    return 0;
}

// Same order as stock: calibrated value, the driver default, then the middle of the range.
int OplusSpeakerAwinic::selectValue(int fileValue, const Range &range, int defaultValue)
{
    if (fileValue > range.min && fileValue < range.max)
        return fileValue;
    if (defaultValue)
        return defaultValue;
    if (range.min >= 1 && range.max >= 1)
        return (range.min + range.max) / 2;
    return 0;
}

int OplusSpeakerAwinic::readCalibration(struct mixer *hwMixer)
{
    std::vector<Range> reRange, f0Range;
    std::vector<int> reDefault, f0Default;
    struct mixer_ctl *ctl;

    if (!mDevNum)
        return -ENODEV;

    ctl = mixer_get_ctl_by_name(hwMixer, AW_F0_SWITCH_CTL);
    mF0Support = ctl && mixer_ctl_get_value(ctl, 0) != 0;

    readRanges("get_re_range", "re_min:%d re_max:%d", reRange);
    readDefaults("show_re", "%d", reDefault);
    if (mF0Support) {
        readRanges("get_f0_range", "f0_min:%d f0_max:%d", f0Range);
        readDefaults("show_cali_f0", "%d", f0Default);
    }

    std::vector<int> re(mDevNum, 0), f0(mDevNum, 0);
    for (uint32_t i = 0; i < mDevNum; i++) {
        // Stock stores a single amplifier in the second slot.
        uint32_t slot = mDevNum == 1 ? 1 : i;
        int value;

        readFileValue(AW_CALI_RE_FILE, slot, &value);
        re[i] = selectValue(value, reRange[i], reDefault[i]);
        PAL_INFO(LOG_TAG, "%s re file %d range [%d, %d] default %d -> %d", kChannelNames[i], value,
                 reRange[i].min, reRange[i].max, reDefault[i], re[i]);

        // Never run with a value that can't be right, the algorithm depends on it.
        if (re[i] <= 0) {
            PAL_ERR(LOG_TAG, "No usable calibration for %s", kChannelNames[i]);
            return -ERANGE;
        }

        if (mF0Support) {
            readFileValue(AW_CALI_F0_FILE, slot, &value);
            f0[i] = selectValue(value, f0Range[i], f0Default[i]);
            PAL_INFO(LOG_TAG, "%s f0 file %d range [%d, %d] default %d -> %d", kChannelNames[i],
                     value, f0Range[i].min, f0Range[i].max, f0Default[i], f0[i]);
        }
    }

    mRe = std::move(re);
    mF0 = std::move(f0);
    return 0;
}

int OplusSpeakerAwinic::buildCalibrationParam(uint32_t miid, std::vector<uint8_t> &param)
{
    if (mRe.empty())
        return -EINVAL;

    const size_t entrySize = mF0Support ? sizeof(CaliReF0) : sizeof(CaliRe);
    const size_t paramSize = PAYLOAD_ALIGN(entrySize * mDevNum, 4);
    param.assign(PAYLOAD_ALIGN(sizeof(ParamHeader) + paramSize, 8), 0);

    auto *header = reinterpret_cast<ParamHeader *>(param.data());
    header->module_instance_id = miid;
    header->param_id = mF0Support ? PARAM_ID_AW_CALI_RE_F0 : PARAM_ID_AW_CALI_RE;
    header->param_size = paramSize;
    header->error_code = 0;

    uint8_t *data = param.data() + sizeof(*header);
    for (uint32_t i = 0; i < mDevNum; i++) {
        // The DSP takes RE in Q12 ohms and F0 in Q12 Hz.
        int32_t re = static_cast<int32_t>(static_cast<int64_t>(mRe[i]) * 4096 / 1000);

        if (mF0Support) {
            CaliReF0 entry = {};
            entry.channel = i;
            entry.re = re;
            entry.f0 = mF0[i] << 12;
            memcpy(data + i * sizeof(entry), &entry, sizeof(entry));
        } else {
            CaliRe entry = {static_cast<int32_t>(i), re};
            memcpy(data + i * sizeof(entry), &entry, sizeof(entry));
        }
    }
    return 0;
}
