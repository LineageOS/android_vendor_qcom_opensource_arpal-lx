/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: OplusSpeakerSia"

#include "OplusSpeakerSia.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cutils/str_parms.h>
#include <tinyalsa/asoundlib.h>

#include "PalCommon.h"

#define KEY_SIPA_ALGO_VERSION "sipa_algo_version"
#define KEY_SIPA_CAL_CH_NUMBER "sipa_cal_ch_number"
#define KEY_SPEAKER_COUNT "oplus_spk_count"

#define SIPA_CALI_DIR "/mnt/vendor/persist/audio"
#define SIPA_CALI_DIR_FALLBACK "/mnt/vendor/persist/media"
#define SIPA_CALI_FILE "sipa_w.bin"
// Each channel has a 20 bytes calibration record and a 16 bytes f0 record.
#define SIPA_CALI_RECORD_SIZE 20
#define SIPA_F0_RECORD_SIZE 16

#define SIPA_MAX_CHANNELS 6
#define SIPA_DEFAULT_MIN_R0 5000
#define SIPA_DEFAULT_MAX_R0 8000
#define SIPA_NEW_ALGO_VERSION 3.5

#define PARAM_ID_SIPA_CALI 0x1800ea03
#define PARAM_ID_SIPA_CALI_EXT 0x1800ea0a

#define PAYLOAD_ALIGN(num, to) (((num) + ((to) - 1)) & ~((to) - 1))

// Matches apm_module_param_data_t.
struct ParamHeader {
    uint32_t module_instance_id;
    uint32_t param_id;
    uint32_t param_size;
    uint32_t error_code;
};

static const char *const kStereoPrefixes[] = {"SpkrLeft", "SpkrRight"};
static const char *const kMultiPrefixes[SIPA_MAX_CHANNELS] = {
    "SpkrOne", "SpkrTwo", "SpkrThree", "SpkrFour", "SpkrFive", "SpkrSix",
};

// Stock defaults to the legacy payload and to a single speaker.
bool OplusSpeakerSia::sLegacyFormat = true;
bool OplusSpeakerSia::sNewAlgo = false;
uint32_t OplusSpeakerSia::sCaliChannelCount = 0;
uint32_t OplusSpeakerSia::sSpeakerCount = 1;

void OplusSpeakerSia::setConfigParams(struct str_parms *parms, char *value, int len)
{
    if (str_parms_get_str(parms, KEY_SIPA_ALGO_VERSION, value, len) >= 0) {
        const char *number = strpbrk(value, "0123456789");
        sNewAlgo = number && strtod(number, nullptr) > SIPA_NEW_ALGO_VERSION;
        sLegacyFormat = !strncmp(value, "v2", sizeof("v2"));
        str_parms_del(parms, KEY_SIPA_ALGO_VERSION);
    }

    if (str_parms_get_str(parms, KEY_SIPA_CAL_CH_NUMBER, value, len) >= 0) {
        sCaliChannelCount = atoi(value);
        str_parms_del(parms, KEY_SIPA_CAL_CH_NUMBER);
    }

    if (str_parms_get_str(parms, KEY_SPEAKER_COUNT, value, len) >= 0) {
        int count = atoi(value);
        if (count > 0)
            sSpeakerCount = count;
        // Other parts of the HAL may use it too.
    }
}

uint32_t OplusSpeakerSia::prefixCount() const
{
    return sSpeakerCount <= 2 ? 2 : SIPA_MAX_CHANNELS;
}

const char *OplusSpeakerSia::channelPrefix(uint32_t channel) const
{
    return sSpeakerCount <= 2 ? kStereoPrefixes[channel] : kMultiPrefixes[channel];
}

static bool getCtlValue(struct mixer *hwMixer, const char *prefix, const char *name, int *value)
{
    char ctlName[64];

    snprintf(ctlName, sizeof(ctlName), "%s %s", prefix, name);
    struct mixer_ctl *ctl = mixer_get_ctl_by_name(hwMixer, ctlName);
    if (!ctl)
        return false;

    int v = mixer_ctl_get_value(ctl, 0);
    if (v < 0)
        return false;
    *value = v;
    return true;
}

bool OplusSpeakerSia::probe(struct mixer *hwMixer)
{
    int type;
    return getCtlValue(hwMixer, channelPrefix(0), "Sipa Type", &type);
}

void OplusSpeakerSia::initCalibrationInfo(struct mixer *hwMixer)
{
    struct stat st;

    mCaliFile = std::string(stat(SIPA_CALI_DIR, &st) == 0 ? SIPA_CALI_DIR : SIPA_CALI_DIR_FALLBACK) +
                "/" + SIPA_CALI_FILE;

    mChannels.clear();
    for (uint32_t ch = 0; ch < prefixCount(); ch++) {
        const char *prefix = channelPrefix(ch);
        int type;

        if (!getCtlValue(hwMixer, prefix, "Sipa Type", &type))
            break;

        Channel channel = {SIPA_DEFAULT_MIN_R0, SIPA_DEFAULT_MAX_R0, 0, {}};
        getCtlValue(hwMixer, prefix, "Sipa min mohms", &channel.minR0);
        getCtlValue(hwMixer, prefix, "Sipa max mohms", &channel.maxR0);
        getCtlValue(hwMixer, prefix, "Sipa default mohms", &channel.defaultR0);
        PAL_INFO(LOG_TAG, "%s type %d r0 range [%d, %d] default %d", prefix, type,
                 channel.minR0, channel.maxR0, channel.defaultR0);
        mChannels.push_back(channel);
    }

    const uint32_t found = mChannels.size();
    if (sCaliChannelCount > 2 && found > 2 && sCaliChannelCount <= found)
        mChannels.resize(sCaliChannelCount);
}

// The file holds the calibration records of all the channels, then their f0 records.
bool OplusSpeakerSia::readCalibrationFile(uint32_t channel, int32_t record[5]) const
{
    const uint32_t count = mChannels.size();
    struct stat st;

    memset(record, 0, SIPA_CALI_RECORD_SIZE);
    if (count < 1 || count > SIPA_MAX_CHANNELS || stat(mCaliFile.c_str(), &st) ||
        st.st_size != static_cast<off_t>(count * (SIPA_CALI_RECORD_SIZE + SIPA_F0_RECORD_SIZE))) {
        PAL_ERR(LOG_TAG, "Invalid calibration file %s", mCaliFile.c_str());
        return false;
    }

    int fd = open(mCaliFile.c_str(), O_RDONLY);
    if (fd < 0)
        return false;
    ssize_t ret = pread(fd, record, SIPA_CALI_RECORD_SIZE, channel * SIPA_CALI_RECORD_SIZE);
    close(fd);
    return ret == SIPA_CALI_RECORD_SIZE;
}

bool OplusSpeakerSia::readCalibration(struct mixer *hwMixer)
{
    if (!mInitialized) {
        initCalibrationInfo(hwMixer);
        mInitialized = true;
    }

    bool calibrated = true;
    for (uint32_t ch = 0; ch < mChannels.size(); ch++) {
        Channel &channel = mChannels[ch];
        int32_t record[5];

        readCalibrationFile(ch, record);
        const int32_t r0 = record[1];
        if (r0 == 0)
            calibrated = false;

        if (channel.minR0 < r0 && r0 < channel.maxR0) {
            memcpy(channel.cal, &record[1], sizeof(channel.cal));
        } else {
            if (channel.defaultR0)
                channel.cal[0] = channel.defaultR0;
            else if (channel.minR0 >= 1 && channel.maxR0 >= 1)
                channel.cal[0] = (channel.minR0 + channel.maxR0) / 2;
            else
                channel.cal[0] = 0;
            channel.cal[1] = channel.cal[2] = channel.cal[3] = -1;
            PAL_ERR(LOG_TAG, "%s r0 %d not in [%d, %d], using %d", channelPrefix(ch), r0,
                    channel.minR0, channel.maxR0, channel.cal[0]);
        }
        PAL_INFO(LOG_TAG, "%s r0 %d", channelPrefix(ch), channel.cal[0]);
    }

    return calibrated;
}

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

int OplusSpeakerSia::buildCalibrationParam(uint32_t miid, std::vector<uint8_t> &param)
{
    if (mChannels.empty() || mChannels.size() > SIPA_MAX_CHANNELS)
        return -ENODATA;

    param.clear();
    for (uint32_t ch = 0; ch < mChannels.size(); ch++) {
        const Channel &channel = mChannels[ch];

        if (sLegacyFormat) {
            if (!channel.cal[0])
                continue;

            uint32_t data[12] = {ch, 0x49, 0x24, 4};
            memcpy(&data[4], channel.cal, sizeof(channel.cal));
            appendParam(param, miid, PARAM_ID_SIPA_CALI, reinterpret_cast<uint8_t *>(data),
                        sizeof(data));
        } else {
            uint8_t data[0x40] = {};
            const bool ext = ch == 2 || ch == 3;
            const uint16_t size = 0x3c;
            const uint32_t version = 4;

            data[0] = ext ? ch - 2 : ch;
            data[1] = sNewAlgo ? 0x2f : 0x49;
            memcpy(&data[2], &size, sizeof(size));
            memcpy(&data[4], &version, sizeof(version));
            memcpy(&data[8], channel.cal, sizeof(channel.cal));
            appendParam(param, miid, ext ? PARAM_ID_SIPA_CALI_EXT : PARAM_ID_SIPA_CALI, data,
                        sizeof(data));
        }
    }

    // Stock appends the gain after the calibration.
    std::vector<uint8_t> gain;
    if (!buildGainParam(miid, gain))
        param.insert(param.end(), gain.begin(), gain.end());

    return param.empty() ? -ENODATA : 0;
}

void OplusSpeakerSia::setVddGain(bool enabled, const OplusVddGain *gain)
{
    mGainEnabled = enabled;
    mHasGain = gain != nullptr;
    if (gain)
        mGain = *gain;
}

// The supply voltage limiter of each channel.
int OplusSpeakerSia::buildGainParam(uint32_t miid, std::vector<uint8_t> &param)
{
    if (!mGainEnabled || !mHasGain || mChannels.empty() ||
        mGain.limiter.size() != mChannels.size())
        return -ENODATA;

    param.clear();
    for (uint32_t ch = 0; ch < mChannels.size(); ch++) {
        const bool ext = ch == 2 || ch == 3;
        uint8_t data[0x1c] = {};
        const uint16_t size = 0x18;
        const int32_t limiter = mGain.limiter[ch];
        const uint32_t end = 0xffffffff;

        data[0] = ext ? ch - 2 : ch;
        data[1] = sNewAlgo ? 0x39 : 0x42;
        memcpy(&data[2], &size, sizeof(size));
        memcpy(&data[4], &limiter, sizeof(limiter));
        memcpy(&data[0x18], &end, sizeof(end));
        appendParam(param, miid, ext ? PARAM_ID_SIPA_CALI_EXT : PARAM_ID_SIPA_CALI, data,
                    sizeof(data));
    }
    return 0;
}
