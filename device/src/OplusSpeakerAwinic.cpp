/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: OplusSpeakerAwinic"

#include "OplusSpeakerAwinic.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <tinyalsa/asoundlib.h>

#include "PalCommon.h"

#define AW_MONITOR_TIME_CTL "aw882xx_hal_monitor_time"
#define AW_DEV_MONITOR_CTL "aw_dev_%u_monitor"
#define AW_F0_SWITCH_CTL "aw882xx_f0_switch"
#define AW_DEV_OFFSET_CTL "aw_dev_%u_offset"
#define AW_DEV_MON_WORK_CTL "aw_dev_%u_hal_mon_work"

#define AW_SMARTPA_NODE "/dev/aw882xx_smartpa"
#define AW_CALI_RE_FILE "/mnt/vendor/persist/media/aw_cali.bin"
#define AW_CALI_F0_FILE "/mnt/vendor/persist/media/aw_cali_f0.bin"
#define AW_CALI_FILE_FIELD 10

#define AW_MAX_DEV 8

#define PARAM_ID_AW_CALI_RE 0x10013d32
#define PARAM_ID_AW_CALI_RE_F0 0x10013d4d
#define PARAM_ID_AW_MSG 0x10013d37
#define AW_MSG_VERSION 0x10000000
#define AW_MSG_TYPE_VOLTAGE_OFFSET 0x1f
#define AW_MSG_ID_VOLUME 0x10013d54
#define AW_MSG_ID_VMAX_EVEN 0x10013d17
#define AW_MSG_ID_VMAX_ODD 0x10013d18
#define AW_MSG_HEADER_SIZE 0x28

#define PAYLOAD_ALIGN(num, to) (((num) + ((to) - 1)) & ~((to) - 1))

// Vmax params, one per amplifier pair.
static const uint32_t kVmaxParamIds[AW_MAX_DEV / 2] = {
    0x10013d2a, 0x10013d2b, 0x10013d36, 0x10013d33,
};

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

static void appendParam(std::vector<uint8_t> &param, uint32_t miid, uint32_t paramId,
                        const uint8_t *data, size_t size)
{
    const size_t paramSize = PAYLOAD_ALIGN(size, 4);
    const size_t offset = param.size();
    param.resize(offset + PAYLOAD_ALIGN(sizeof(ParamHeader) + paramSize, 8), 0);

    auto *header = reinterpret_cast<ParamHeader *>(param.data() + offset);
    header->module_instance_id = miid;
    header->param_id = paramId;
    header->param_size = paramSize;
    header->error_code = 0;
    memcpy(param.data() + offset + sizeof(*header), data, size);
}

// Messages through PARAM_ID_AW_MSG, the checksum covers everything after it.
static void appendMsg(std::vector<uint8_t> &param, uint32_t miid, uint32_t msgId,
                      const void *values, uint32_t count, uint32_t valueSize)
{
    std::vector<uint8_t> msg(AW_MSG_HEADER_SIZE + count * valueSize, 0);
    auto *words = reinterpret_cast<uint32_t *>(msg.data());

    words[1] = AW_MSG_VERSION;
    words[2] = 1;
    words[3] = msgId;
    words[4] = 0;
    words[5] = count;
    words[6] = valueSize;
    memcpy(msg.data() + AW_MSG_HEADER_SIZE, values, count * valueSize);

    uint32_t checksum = 0;
    for (size_t b = sizeof(uint32_t); b < msg.size(); b++)
        checksum += msg[b];
    words[0] = checksum;

    appendParam(param, miid, PARAM_ID_AW_MSG, msg.data(), msg.size());
}

static void appendVmax(std::vector<uint8_t> &param, uint32_t miid, uint32_t dev, int32_t vmax)
{
    const uint32_t msgId = dev % 2 ? AW_MSG_ID_VMAX_ODD : AW_MSG_ID_VMAX_EVEN;
    uint32_t data[7] = {0, msgId, 1, 0, 0, 0, static_cast<uint32_t>(vmax)};
    appendParam(param, miid, kVmaxParamIds[dev / 2], reinterpret_cast<uint8_t *>(data),
                sizeof(data));
}

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

// Stock handles a single amplifier as the right channel, in the second slot.
uint32_t OplusSpeakerAwinic::channelIndex(uint32_t dev) const
{
    return mDevNum == 1 ? 1 : dev;
}

int OplusSpeakerAwinic::readRanges(const char *cmd, const char *format, std::vector<Range> &ranges)
{
    std::string reply = readNode(cmd);

    ranges.assign(mDevNum, Range{0, 0});
    for (uint32_t i = 0; i < mDevNum; i++) {
        const char *channel = kChannelNames[channelIndex(i)];
        std::string tag = std::string(channel) + ":";
        size_t pos = reply.find(tag);
        if (pos == std::string::npos ||
            sscanf(reply.c_str() + pos + tag.size(), format, &ranges[i].min, &ranges[i].max) != 2) {
            PAL_ERR(LOG_TAG, "No %s for %s", cmd, channel);
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
        std::string tag = std::string(kChannelNames[channelIndex(i)]) + ":";
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
    if (ret <= 0 || sscanf(buf, "%10d", value) != 1) {
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

bool OplusSpeakerAwinic::readCalibration(struct mixer *hwMixer)
{
    std::vector<Range> reRange, f0Range;
    std::vector<int> reDefault, f0Default;
    struct mixer_ctl *ctl;

    if (!mDevNum && !probe(hwMixer)) {
        PAL_ERR(LOG_TAG, "No amplifier found");
        return false;
    }

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
        uint32_t slot = channelIndex(i);
        int value;

        readFileValue(AW_CALI_RE_FILE, slot, &value);
        re[i] = selectValue(value, reRange[i], reDefault[i]);
        PAL_INFO(LOG_TAG, "%s re file %d range [%d, %d] default %d -> %d", kChannelNames[slot], value,
                 reRange[i].min, reRange[i].max, reDefault[i], re[i]);


        if (mF0Support) {
            readFileValue(AW_CALI_F0_FILE, slot, &value);
            f0[i] = selectValue(value, f0Range[i], f0Default[i]);
            PAL_INFO(LOG_TAG, "%s f0 file %d range [%d, %d] default %d -> %d", kChannelNames[slot],
                     value, f0Range[i].min, f0Range[i].max, f0Default[i], f0[i]);
        }
    }

    mRe = std::move(re);
    mF0 = std::move(f0);

    ctl = mixer_get_ctl_by_name(hwMixer, AW_MONITOR_TIME_CTL);
    mMonitorEnabled = ctl && mixer_ctl_get_value(ctl, 0) != 0;
    mMonitorVmax.assign(mDevNum, INT32_MIN);

    // The DSP compensates the amplifier voltage offset, only aw882xx reports it.
    mVoltageOffset.clear();
    for (uint32_t i = 0; i < mDevNum; i++) {
        char name[32];

        snprintf(name, sizeof(name), AW_DEV_OFFSET_CTL, i);
        ctl = mixer_get_ctl_by_name(hwMixer, name);
        if (!ctl) {
            mVoltageOffset.clear();
            break;
        }
        mVoltageOffset.push_back(mixer_ctl_get_value(ctl, 0));
        PAL_INFO(LOG_TAG, "dev %u voltage offset %d", i, mVoltageOffset.back());
    }
    return true;
}

int OplusSpeakerAwinic::buildCalibrationParam(uint32_t miid, std::vector<uint8_t> &param)
{
    if (mRe.empty())
        return -ENODATA;

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

    if (!mVoltageOffset.empty())
        appendMsg(param, miid, AW_MSG_TYPE_VOLTAGE_OFFSET, mVoltageOffset.data(),
                  mVoltageOffset.size(), sizeof(int32_t));

    // Stock appends the gain after the calibration.
    std::vector<uint8_t> gain;
    if (!buildGainParam(miid, gain))
        param.insert(param.end(), gain.begin(), gain.end());
    return 0;
}

void OplusSpeakerAwinic::setVddGain(bool enabled, const OplusVddGain *gain)
{
    mGainEnabled = enabled;
    mHasGain = gain != nullptr;
    if (gain)
        mGain = *gain;
}

int OplusSpeakerAwinic::buildGainParam(uint32_t miid, std::vector<uint8_t> &param)
{
    if (!mGainEnabled || !mHasGain || !mDevNum)
        return -ENODATA;

    param.clear();
    if (mGain.limiter.size() == mDevNum) {
        for (uint32_t i = 0; i < mDevNum; i++)
            appendVmax(param, miid, i, mGain.limiter[i]);
    }

    if (mGain.gain.size() == mDevNum) {
        std::vector<int32_t> volume;
        for (uint32_t i = 0; i < mDevNum; i++) {
            volume.push_back(mGain.gain[i]);
            volume.push_back(0);
        }
        appendMsg(param, miid, AW_MSG_ID_VOLUME, volume.data(), mDevNum, 2 * sizeof(int32_t));
    }

    return param.empty() ? -ENODATA : 0;
}

// The driver reports the vmax the DSP should use for each monitored amplifier.
int OplusSpeakerAwinic::monitor(struct mixer *hwMixer, uint32_t miid, std::vector<uint8_t> &param)
{
    if (!mMonitorEnabled)
        return -ENODATA;

    param.clear();
    for (uint32_t i = 0; i < mDevNum && i < mMonitorVmax.size(); i++) {
        char name[32];

        snprintf(name, sizeof(name), AW_DEV_MONITOR_CTL, i);
        struct mixer_ctl *ctl = mixer_get_ctl_by_name(hwMixer, name);
        if (!ctl || !mixer_ctl_get_value(ctl, 0))
            continue;

        snprintf(name, sizeof(name), AW_DEV_MON_WORK_CTL, i);
        ctl = mixer_get_ctl_by_name(hwMixer, name);
        if (!ctl)
            continue;

        int vmax = mixer_ctl_get_value(ctl, 0);
        if (vmax == mMonitorVmax[i])
            continue;

        PAL_INFO(LOG_TAG, "dev %u vmax 0x%x", i, vmax);
        mMonitorVmax[i] = vmax;
        appendVmax(param, miid, i, vmax);
    }

    return param.empty() ? -ENODATA : 0;
}
