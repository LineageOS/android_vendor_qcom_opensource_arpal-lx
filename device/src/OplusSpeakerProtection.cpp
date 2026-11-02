/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: OplusSpeakerProtection"

#include "OplusSpeakerProtection.h"

#include <chrono>
#include <climits>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>

#include <agm/agm_api.h>
#include <cutils/str_parms.h>
#include <tinyalsa/asoundlib.h>

#include "OplusSpeakerAwinic.h"
#include "OplusSpeakerSia.h"
#include "OplusSpeakerTfa98xx.h"
#include "PalAudioRoute.h"
#include "PayloadBuilder.h"
#include "ResourceManager.h"
#include "Session.h"
#include "SessionAgm.h"
#include "SessionAlsaUtils.h"
#include "Stream.h"
#include "kvh2xml.h"

#define SP_PCM_ID_CTL "SP PCMID"
#define SP_MIID_CTL "SP MIID"

#define KEY_SPEAKER_ID "oplus_speaker_id"
#define KEY_SPEAKER_TYPE "oplus_speaker_type"
#define KEY_SPEAKER_VOIP_CKV "speaker_voip_ckv"
#define KEY_LOW_BATTERY_GAIN "lowbattery_speaker_profile_support"

#define BATTERY_CAPACITY_NODE "/sys/class/power_supply/battery/capacity"
#define BATTERY_FULL_CAPACITY 100
#define BATTERY_VOLTAGE_NODE "/sys/class/power_supply/battery/voltage_now"
// Stock uses this value when the voltage can't be read.
#define BATTERY_DEFAULT_VOLTAGE 23200

#define MONITOR_INTERVAL std::chrono::milliseconds(5000)

#define VI_DEFAULT_CHANNELS 2
#define VI_DEFAULT_SAMPLE_RATE 48000
#define VI_DEFAULT_BIT_WIDTH 32
#define VI_PERIOD_SIZE 256
#define VI_PERIOD_COUNT 4

// Values of the speaker scene calibration key.
#define SPEAKER_SCENE_DEFAULT 0
#define SPEAKER_SCENE_VOICE 3
#define SPEAKER_SCENE_VOIP 5

// Stock defaults to a TFA98xx smart PA.
int OplusSpeakerProtection::sSpeakerId = SPEAKER_ID_TFA98XX;
bool OplusSpeakerProtection::sSpeakerIdSet = false;
int OplusSpeakerProtection::sSpeakerType = SPEAKER_TYPE_SMART_PA;
bool OplusSpeakerProtection::sVoipCkvSupport = false;
bool OplusSpeakerProtection::sLowBatteryGainSupport = false;
std::vector<std::pair<int, int>> OplusSpeakerProtection::sBatteryGainLevels;
std::map<int, OplusVddGain> OplusSpeakerProtection::sVddGains;

OplusSpeakerProtection::OplusSpeakerProtection(struct pal_device *device,
                                               std::shared_ptr<ResourceManager> Rm)
    : Device(device, Rm),
      mCalibrated(false),
      mVirtMixer(nullptr),
      mHwMixer(nullptr),
      mTxPcm(nullptr),
      mRxMiid(0),
      mPcmIdCtrlVal(-1),
      mActivePcmId(-1),
      mMiidCtrlVal(0),
      mGainLevel(0),
      mVddThreshold(-1),
      mSpeakerInUse(false),
      mMonitorExit(false)
{
    rm = Rm;
    memcpy(&mDeviceAttr, device, sizeof(struct pal_device));

    if (rm->getVirtualAudioMixer(&mVirtMixer))
        PAL_ERR(LOG_TAG, "Failed to get the virtual mixer");
    if (rm->getHwAudioMixer(&mHwMixer))
        PAL_ERR(LOG_TAG, "Failed to get the hw mixer");

    struct pal_device_info viInfo = {};
    rm->getDeviceInfo(PAL_DEVICE_IN_VI_FEEDBACK, PAL_STREAM_PROXY, "", &viInfo);
    mViChannels = viInfo.channels ? viInfo.channels : VI_DEFAULT_CHANNELS;
    mViSampleRate = viInfo.samplerate ? viInfo.samplerate : VI_DEFAULT_SAMPLE_RATE;
    mViBitWidth = viInfo.bit_width ? viInfo.bit_width : VI_DEFAULT_BIT_WIDTH;
    mViSndDevice = viInfo.sndDevName;

    mMonitorThread = std::thread(&OplusSpeakerProtection::monitorThread, this);
}

OplusSpeakerProtection::~OplusSpeakerProtection()
{
    {
        std::lock_guard<std::mutex> lock(mMonitorMutex);
        mMonitorExit = true;
    }
    mMonitorCv.notify_all();
    if (mMonitorThread.joinable())
        mMonitorThread.join();
}

void OplusSpeakerProtection::setConfigParams(struct str_parms *parms, char *value, int len)
{
    if (str_parms_get_str(parms, KEY_SPEAKER_ID, value, len) >= 0) {
        int id = atoi(value);
        if (id == SPEAKER_ID_TFA98XX || id == SPEAKER_ID_AWINIC || id == SPEAKER_ID_SIA) {
            sSpeakerId = id;
            sSpeakerIdSet = true;
        } else {
            PAL_ERR(LOG_TAG, "Unsupported %s %s", KEY_SPEAKER_ID, value);
        }
        str_parms_del(parms, KEY_SPEAKER_ID);
    }

    if (str_parms_get_str(parms, KEY_SPEAKER_TYPE, value, len) >= 0) {
        sSpeakerType = atoi(value) == SPEAKER_TYPE_ANALOG ? SPEAKER_TYPE_ANALOG
                                                          : SPEAKER_TYPE_SMART_PA;
        str_parms_del(parms, KEY_SPEAKER_TYPE);
    }

    if (str_parms_get_str(parms, KEY_SPEAKER_VOIP_CKV, value, len) >= 0) {
        sVoipCkvSupport = !strncmp(value, "true", sizeof("true"));
        str_parms_del(parms, KEY_SPEAKER_VOIP_CKV);
    }

    if (str_parms_get_str(parms, KEY_LOW_BATTERY_GAIN, value, len) >= 0) {
        sLowBatteryGainSupport = !strncmp(value, "true", sizeof("true"));
        str_parms_del(parms, KEY_LOW_BATTERY_GAIN);
    }

    OplusSpeakerSia::setConfigParams(parms, value, len);
}

void OplusSpeakerProtection::addBatteryGainLevel(const char **attr)
{
    int battery = -1, level = -1;

    for (int i = 0; attr[i] && attr[i + 1]; i += 2) {
        if (!strcmp(attr[i], "battery"))
            battery = atoi(attr[i + 1]);
        else if (!strcmp(attr[i], "level"))
            level = atoi(attr[i + 1]);
    }

    if (battery < 0 || level < 0) {
        PAL_ERR(LOG_TAG, "Invalid bat_to_dblvl_map entry");
        return;
    }
    sBatteryGainLevels.emplace_back(battery, level);
}

static std::vector<int> parseIntList(const char *list)
{
    std::vector<int> values;
    std::stringstream ss(list);
    std::string item;

    while (std::getline(ss, item, ','))
        values.push_back(atoi(item.c_str()));
    return values;
}

void OplusSpeakerProtection::addVddGain(const char **attr)
{
    OplusVddGain gain;
    int threshold = -1;

    for (int i = 0; attr[i] && attr[i + 1]; i += 2) {
        if (!strcmp(attr[i], "vdd_threshold"))
            threshold = atoi(attr[i + 1]);
        else if (!strcmp(attr[i], "cmd") || !strcmp(attr[i], "limiter"))
            gain.limiter = parseIntList(attr[i + 1]);
        else if (!strcmp(attr[i], "gain"))
            gain.gain = parseIntList(attr[i + 1]);
    }

    if (threshold < 0) {
        PAL_ERR(LOG_TAG, "Invalid vdd_to_gain_map entry");
        return;
    }
    sVddGains[threshold] = gain;
}

int OplusSpeakerProtection::getBatteryVoltage()
{
    char buf[16] = {};
    int voltage = BATTERY_DEFAULT_VOLTAGE;

    int fd = ::open(BATTERY_VOLTAGE_NODE, O_RDONLY);
    if (fd >= 0) {
        if (::read(fd, buf, sizeof(buf) - 1) > 0)
            voltage = atoi(buf);
        ::close(fd);
    }
    return voltage;
}

// The map is ordered by decreasing battery capacity, the gain level is the one
// of the last entry at or above the current capacity.
int OplusSpeakerProtection::getBatteryGainLevel()
{
    int capacity = BATTERY_FULL_CAPACITY;
    char buf[8] = {};

    int fd = ::open(BATTERY_CAPACITY_NODE, O_RDONLY);
    if (fd >= 0) {
        if (::read(fd, buf, 4) > 0) {
            int value = atoi(buf);
            if (value <= BATTERY_FULL_CAPACITY)
                capacity = value;
        }
        ::close(fd);
    }

    int level = 0;
    for (const auto &entry : sBatteryGainLevels) {
        if (entry.first < capacity)
            break;
        level = entry.second;
    }

    PAL_DBG(LOG_TAG, "Battery capacity %d, gain level %d", capacity, level);
    return level;
}

int OplusSpeakerProtection::getSpeakerScene(Stream *stream)
{
    struct pal_stream_attributes sAttr = {};

    if (!stream || stream->getStreamAttributes(&sAttr))
        return SPEAKER_SCENE_DEFAULT;

    switch (sAttr.type) {
        case PAL_STREAM_VOICE_CALL:
            return SPEAKER_SCENE_VOICE;
        case PAL_STREAM_VOIP_RX:
            return sVoipCkvSupport ? SPEAKER_SCENE_VOIP : SPEAKER_SCENE_VOICE;
        default:
            return SPEAKER_SCENE_DEFAULT;
    }
}

int OplusSpeakerProtection::createAmp()
{
    std::unique_ptr<OplusSpeakerAmp> tfa = std::make_unique<OplusSpeakerTfa98xx>();
    std::unique_ptr<OplusSpeakerAmp> awinic = std::make_unique<OplusSpeakerAwinic>();
    std::unique_ptr<OplusSpeakerAmp> sia = std::make_unique<OplusSpeakerSia>();

    // Stock assumes a TFA98xx when the device doesn't configure the amplifier,
    // look for the other ones only when its controls are missing.
    int id = sSpeakerId;
    if (!sSpeakerIdSet && mHwMixer && !tfa->probe(mHwMixer)) {
        if (awinic->probe(mHwMixer))
            id = SPEAKER_ID_AWINIC;
        else if (sia->probe(mHwMixer))
            id = SPEAKER_ID_SIA;
    }

    switch (id) {
        case SPEAKER_ID_TFA98XX:
            mAmp = std::move(tfa);
            break;
        case SPEAKER_ID_AWINIC:
            mAmp = std::move(awinic);
            break;
        case SPEAKER_ID_SIA:
            mAmp = std::move(sia);
            break;
        default:
            PAL_ERR(LOG_TAG, "Unsupported amplifier %d", id);
            return -ENODEV;
    }

    PAL_INFO(LOG_TAG, "Using the %s amplifier", mAmp->name());
    return 0;
}

void OplusSpeakerProtection::updateRxConfig()
{
    std::vector<Stream *> activeStreams;
    std::shared_ptr<Device> dev;
    std::string backEndName;
    std::vector<uint8_t> param;
    Session *session = nullptr;
    uint32_t miid = 0;
    int ret;

    if (!mAmp && createAmp())
        return;

    if (!mCalibrated)
        mCalibrated = mAmp->readCalibration(mHwMixer);

    // Stock picks the low battery gain on every first start.
    mVddThreshold = -1;
    updateGain();

    rm->getBackendName(mDeviceAttr.id, backEndName);
    dev = Device::getInstance(&mDeviceAttr, rm);
    ret = rm->getActiveStream_l(activeStreams, dev);
    if (ret || activeStreams.empty()) {
        PAL_ERR(LOG_TAG, "No active stream available");
        return;
    }

    activeStreams[0]->getAssociatedSession(&session);
    if (!session || session->getMIID(backEndName.c_str(), MODULE_SP, &miid)) {
        PAL_ERR(LOG_TAG, "Failed to get the SP module miid");
        return;
    }
    mRxMiid = miid;

    ret = mAmp->buildCalibrationParam(miid, param);
    if (ret) {
        PAL_ERR(LOG_TAG, "No calibration sent, status %d", ret);
        return;
    }

    if (customPayload)
        free(customPayload);
    customPayload = nullptr;
    customPayloadSize = 0;

    ret = updateCustomPayload(param.data(), param.size());
    if (ret)
        PAL_ERR(LOG_TAG, "Failed to send the calibration, status %d", ret);
}

// Returns whether the gain changed.
bool OplusSpeakerProtection::updateGain()
{
    if (!mAmp)
        return false;

    if (!mAmp->usesVddGain()) {
        int level = sLowBatteryGainSupport ? getBatteryGainLevel() : 0;
        bool changed = level != mGainLevel;
        if (changed)
            PAL_INFO(LOG_TAG, "Gain level %d -> %d", mGainLevel, level);
        mGainLevel = level;
        mAmp->setGain(sLowBatteryGainSupport, level);
        return changed;
    }

    // The gain of the highest threshold at or below the battery voltage.
    int voltage = getBatteryVoltage();
    int threshold = mVddThreshold;
    for (const auto &entry : sVddGains) {
        if (entry.first <= voltage)
            threshold = entry.first;
    }

    bool changed = threshold != mVddThreshold;
    mVddThreshold = threshold;

    auto it = sVddGains.find(threshold);
    mAmp->setVddGain(sLowBatteryGainSupport, it != sVddGains.end() ? &it->second : nullptr);
    if (changed)
        PAL_INFO(LOG_TAG, "Battery voltage %d, vdd threshold %d", voltage, threshold);
    return changed;
}

// Sends params to the SP module of the running speaker session.
void OplusSpeakerProtection::sendLiveParam(std::vector<uint8_t> &param)
{
    if (param.empty() || mActivePcmId < 0)
        return;

    int ret = SessionAlsaUtils::setMixerParameter(mVirtMixer, mActivePcmId, param.data(),
                                                  param.size());
    if (ret)
        PAL_ERR(LOG_TAG, "Failed to send %zu bytes, status %d", param.size(), ret);
}

void OplusSpeakerProtection::monitorThread()
{
    std::unique_lock<std::mutex> lock(mMonitorMutex);

    while (!mMonitorExit) {
        if (!mSpeakerInUse) {
            mMonitorCv.wait(lock);
            continue;
        }

        mMonitorCv.wait_for(lock, MONITOR_INTERVAL);
        if (mMonitorExit || !mSpeakerInUse)
            continue;
        lock.unlock();

        mDeviceMutex.lock();
        if (deviceStartStopCount > 0 && mAmp && mRxMiid) {
            std::vector<uint8_t> param;

            // Like stock, follow the battery while playing.
            if (sLowBatteryGainSupport && updateGain() &&
                !mAmp->buildGainParam(mRxMiid, param)) {
                PAL_INFO(LOG_TAG, "Updating the speaker gain");
                sendLiveParam(param);
            }

            param.clear();
            if (!mAmp->monitor(mHwMixer, mRxMiid, param))
                sendLiveParam(param);
        }
        mDeviceMutex.unlock();

        lock.lock();
    }
}

void OplusSpeakerProtection::sendRxInfoToDriver()
{
    // Same lookup order as stock.
    static const pal_stream_type_t kStreamTypes[] = {
        PAL_STREAM_DEEP_BUFFER, PAL_STREAM_VOICE_CALL, PAL_STREAM_VOIP_RX,
        PAL_STREAM_COMPRESSED,  PAL_STREAM_LOW_LATENCY,
    };
    std::vector<Stream *> activeStreams;
    struct mixer_ctl *ctl;
    int pcmId = -1;
    pal_stream_type_t type = PAL_STREAM_LOW_LATENCY;

    std::shared_ptr<Device> dev = Device::getInstance(&mDeviceAttr, rm);
    if (rm->getActiveStream_l(activeStreams, dev))
        activeStreams.clear();

    for (pal_stream_type_t t : kStreamTypes) {
        for (Stream *stream : activeStreams) {
            struct pal_stream_attributes sAttr = {};
            Session *session = nullptr;

            if (stream->getCurState() != STREAM_INIT && !stream->isActive())
                continue;
            if (stream->getStreamAttributes(&sAttr) || sAttr.type != t)
                continue;
            stream->getAssociatedSession(&session);
            if (!session)
                continue;
            pcmId = session->getFrontEndId(RX_HOSTLESS);
            if (pcmId >= 0) {
                type = t;
                break;
            }
        }
        if (pcmId >= 0)
            break;
    }

    mActivePcmId = pcmId;
    if (pcmId < 0) {
        PAL_ERR(LOG_TAG, "Failed to get pcmDevId");
        return;
    }

    // The driver uses them for its own DSP access, audio works without them.
    int value = pcmId | (type << 16);
    if (value != mPcmIdCtrlVal) {
        ctl = mixer_get_ctl_by_name(mHwMixer, SP_PCM_ID_CTL);
        if (ctl && !mixer_ctl_set_value(ctl, 0, value))
            mPcmIdCtrlVal = value;
        else
            PAL_ERR(LOG_TAG, "Failed to set %s to 0x%x", SP_PCM_ID_CTL, value);
    }

    if (mRxMiid && mRxMiid != mMiidCtrlVal) {
        ctl = mixer_get_ctl_by_name(mHwMixer, SP_MIID_CTL);
        if (ctl && !mixer_ctl_set_value(ctl, 0, mRxMiid))
            mMiidCtrlVal = mRxMiid;
        else
            PAL_ERR(LOG_TAG, "Failed to set %s", SP_MIID_CTL);
    }
}

static enum pcm_format getViPcmFormat(uint32_t bitWidth)
{
    switch (bitWidth) {
        case 16:
            return PCM_FORMAT_S16_LE;
        case 24:
            return PCM_FORMAT_S24_LE;
        case 32:
            return PCM_FORMAT_S32_LE;
        default:
            PAL_ERR(LOG_TAG, "Unsupported VI bit width %u, using 32", bitWidth);
            return PCM_FORMAT_S32_LE;
    }
}

int OplusSpeakerProtection::openViPath()
{
    uint32_t devicePropId[] = {0x08000010, 1, 0x2};
    std::vector<std::pair<int, int>> keyVector, calVector;
    struct agmMetaData deviceMetaData(nullptr, 0);
    struct audio_route *audioRoute = nullptr;
    struct pal_stream_attributes sAttr = {};
    struct pal_device device = {};
    struct pcm_config config = {};
    struct mixer_ctl *ctl;
    std::ostringstream metadataCtl, connectCtl;
    int ret;

    uint32_t channels = mViChannels;
    if (channels != 1 && channels != 2 && channels != 4 && channels != 8) {
        PAL_ERR(LOG_TAG, "Unsupported VI channel count %u, using 2", channels);
        channels = 2;
    }

    ret = rm->getAudioRoute(&audioRoute);
    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to get the audio route, status %d", ret);
        return ret;
    }

    device.id = PAL_DEVICE_IN_VI_FEEDBACK;
    rm->getBackendName(device.id, mViBackEndName);
    if (mViBackEndName.empty()) {
        PAL_ERR(LOG_TAG, "Failed to get the VI backend name");
        return -EINVAL;
    }

    device.config.ch_info.channels = channels;
    for (uint32_t i = 0; i < channels; i++)
        device.config.ch_info.ch_map[i] = PAL_CHMAP_CHANNEL_FL + i;
    device.config.sample_rate = mViSampleRate;
    device.config.bit_width = mViBitWidth;
    device.config.aud_fmt_id = rm->getAudioFmt(mViBitWidth);

    ret = PayloadBuilder::getDeviceKV(device.id, keyVector);
    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to get the VI device KV, status %d", ret);
        return ret;
    }

    SessionAlsaUtils::getAgmMetaData(keyVector, calVector, (struct prop_data *)devicePropId,
                                     deviceMetaData);
    if (!deviceMetaData.size) {
        PAL_ERR(LOG_TAG, "VI device metadata is zero");
        return -ENOMEM;
    }

    metadataCtl << mViBackEndName << " metadata";
    ctl = mixer_get_ctl_by_name(mVirtMixer, metadataCtl.str().data());
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Invalid mixer control %s", metadataCtl.str().data());
        free(deviceMetaData.buf);
        return -EINVAL;
    }

    ret = mixer_ctl_set_array(ctl, deviceMetaData.buf, deviceMetaData.size);
    free(deviceMetaData.buf);
    deviceMetaData.buf = nullptr;
    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to set the VI metadata, status %d", ret);
        return ret;
    }

    ret = SessionAlsaUtils::setDeviceMediaConfig(rm, mViBackEndName, &device);
    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to set the VI media config, status %d", ret);
        return ret;
    }

    sAttr.type = PAL_STREAM_LOW_LATENCY;
    sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
    mPcmDevIdTx = rm->allocateFrontEndIds(sAttr, TX_HOSTLESS);
    if (mPcmDevIdTx.empty()) {
        PAL_ERR(LOG_TAG, "Failed to allocate the VI front end");
        return -ENOSYS;
    }

    connectCtl << "PCM" << mPcmDevIdTx.at(0) << " connect";
    ctl = mixer_get_ctl_by_name(mVirtMixer, connectCtl.str().data());
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Invalid mixer control %s", connectCtl.str().data());
        ret = -EINVAL;
        goto err_free_fe;
    }

    ret = mixer_ctl_set_enum_by_string(ctl, mViBackEndName.c_str());
    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to connect the VI path, status %d", ret);
        goto err_free_fe;
    }

    enableDevice(audioRoute, const_cast<char *>(mViSndDevice.c_str()));

    config.rate = mViSampleRate;
    config.channels = channels;
    config.format = getViPcmFormat(mViBitWidth);
    config.period_size = VI_PERIOD_SIZE;
    config.period_count = VI_PERIOD_COUNT;
    config.start_threshold = 0;
    config.stop_threshold = INT_MAX;
    config.silence_threshold = 0;

    mTxPcm = pcm_open(rm->getVirtualSndCard(), mPcmDevIdTx.at(0), PCM_IN, &config);
    if (!mTxPcm || !pcm_is_ready(mTxPcm)) {
        PAL_ERR(LOG_TAG, "Failed to open the VI pcm");
        ret = -EIO;
        goto err_close;
    }

    if (pcm_start(mTxPcm) < 0) {
        PAL_ERR(LOG_TAG, "Failed to start the VI pcm");
        ret = -EIO;
        goto err_close;
    }

    PAL_INFO(LOG_TAG, "VI path started on pcm %d", mPcmDevIdTx.at(0));
    return 0;

err_close:
    closeViPath();
    return ret;

err_free_fe:
    rm->freeFrontEndIds(mPcmDevIdTx, sAttr, TX_HOSTLESS);
    mPcmDevIdTx.clear();
    return ret;
}

void OplusSpeakerProtection::closeViPath()
{
    uint32_t devicePropId[] = {0x08000010, 2, 0x2, 0x5};
    std::vector<std::pair<int, int>> emptyKV;
    struct agmMetaData deviceMetaData(nullptr, 0);
    struct audio_route *audioRoute = nullptr;
    struct pal_stream_attributes sAttr = {};
    struct mixer_ctl *ctl;

    mActivePcmId = -1;
    if (mPcmDevIdTx.empty())
        return;

    if (mTxPcm)
        pcm_stop(mTxPcm);

    std::ostringstream disconnectCtl, metadataCtl;
    disconnectCtl << "PCM" << mPcmDevIdTx.at(0) << " disconnect";
    ctl = mixer_get_ctl_by_name(mVirtMixer, disconnectCtl.str().data());
    if (ctl)
        mixer_ctl_set_enum_by_string(ctl, mViBackEndName.c_str());

    SessionAlsaUtils::getAgmMetaData(emptyKV, emptyKV, (struct prop_data *)devicePropId,
                                     deviceMetaData);
    metadataCtl << mViBackEndName << " metadata";
    ctl = mixer_get_ctl_by_name(mVirtMixer, metadataCtl.str().data());
    if (ctl && deviceMetaData.size)
        mixer_ctl_set_array(ctl, deviceMetaData.buf, deviceMetaData.size);
    free(deviceMetaData.buf);

    if (mTxPcm)
        pcm_close(mTxPcm);
    mTxPcm = nullptr;

    sAttr.type = PAL_STREAM_LOW_LATENCY;
    sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
    rm->freeFrontEndIds(mPcmDevIdTx, sAttr, TX_HOSTLESS);
    mPcmDevIdTx.clear();

    if (!rm->getAudioRoute(&audioRoute))
        disableDevice(audioRoute, const_cast<char *>(mViSndDevice.c_str()));
}

int32_t OplusSpeakerProtection::start()
{
    mDeviceMutex.lock();

    // Analog amplifiers have no calibration nor VI feedback.
    const bool smartPa = sSpeakerType == SPEAKER_TYPE_SMART_PA;
    const bool firstStart = deviceStartStopCount == 0;
    if (smartPa && firstStart) {
        updateRxConfig();
        int ret = openViPath();
        if (ret)
            PAL_ERR(LOG_TAG, "VI feedback unavailable, status %d", ret);
    }

    int32_t ret = start_l();
    if (ret && smartPa && firstStart)
        closeViPath();
    else if (smartPa)
        sendRxInfoToDriver();

    const bool inUse = smartPa && deviceStartStopCount > 0;
    mDeviceMutex.unlock();

    {
        std::lock_guard<std::mutex> lock(mMonitorMutex);
        mSpeakerInUse = inUse;
    }
    mMonitorCv.notify_all();
    return ret;
}

int32_t OplusSpeakerProtection::stop()
{
    mDeviceMutex.lock();

    int32_t ret = stop_l();
    if (sSpeakerType == SPEAKER_TYPE_SMART_PA) {
        if (deviceStartStopCount == 0)
            closeViPath();
        else
            sendRxInfoToDriver();
    }

    const bool inUse = sSpeakerType == SPEAKER_TYPE_SMART_PA && deviceStartStopCount > 0;
    mDeviceMutex.unlock();

    {
        std::lock_guard<std::mutex> lock(mMonitorMutex);
        mSpeakerInUse = inUse;
    }
    mMonitorCv.notify_all();
    return ret;
}
