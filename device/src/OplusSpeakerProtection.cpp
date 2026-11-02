/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: OplusSpeakerProtection"

#include "OplusSpeakerProtection.h"

#include <climits>
#include <sstream>

#include <agm/agm_api.h>
#include <tinyalsa/asoundlib.h>

#include "OplusSpeakerAwinic.h"
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

#define VI_PERIOD_SIZE 256
#define VI_PERIOD_COUNT 4

OplusSpeakerProtection::OplusSpeakerProtection(struct pal_device *device,
                                               std::shared_ptr<ResourceManager> Rm)
    : Device(device, Rm), mCalibrated(false), mVirtMixer(nullptr), mHwMixer(nullptr), mTxPcm(nullptr)
{
    rm = Rm;
    memcpy(&mDeviceAttr, device, sizeof(struct pal_device));

    if (rm->getVirtualAudioMixer(&mVirtMixer))
        PAL_ERR(LOG_TAG, "Failed to get the virtual mixer");
    if (rm->getHwAudioMixer(&mHwMixer))
        PAL_ERR(LOG_TAG, "Failed to get the hw mixer");
}

OplusSpeakerProtection::~OplusSpeakerProtection()
{
}

int OplusSpeakerProtection::probeAmp()
{
    std::unique_ptr<OplusSpeakerAmp> amps[] = {
        std::make_unique<OplusSpeakerTfa98xx>(),
        std::make_unique<OplusSpeakerAwinic>(),
    };

    if (!mHwMixer)
        return -ENODEV;

    for (auto &amp : amps) {
        if (amp->probe(mHwMixer)) {
            PAL_INFO(LOG_TAG, "Using the %s amplifier", amp->name());
            mAmp = std::move(amp);
            return 0;
        }
    }

    PAL_ERR(LOG_TAG, "No supported amplifier found");
    return -ENODEV;
}

int OplusSpeakerProtection::sendCalibration(uint32_t miid)
{
    std::vector<uint8_t> param;

    int ret = mAmp->buildCalibrationParam(miid, param);
    if (ret)
        return ret;

    if (customPayload)
        free(customPayload);
    customPayload = nullptr;
    customPayloadSize = 0;

    return updateCustomPayload(param.data(), param.size());
}

void OplusSpeakerProtection::sendRxInfoToDriver(Stream *stream, uint32_t miid)
{
    Session *session = nullptr;
    struct mixer_ctl *ctl;

    stream->getAssociatedSession(&session);
    int pcmId = session ? session->getPlaybackPcmId() : -1;

    // Only used by the driver for its own DSP access, audio works without them.
    ctl = mixer_get_ctl_by_name(mHwMixer, SP_PCM_ID_CTL);
    if (ctl && pcmId >= 0)
        mixer_ctl_set_value(ctl, 0, pcmId);
    else
        PAL_ERR(LOG_TAG, "Failed to set %s, pcm id %d", SP_PCM_ID_CTL, pcmId);

    ctl = mixer_get_ctl_by_name(mHwMixer, SP_MIID_CTL);
    if (ctl)
        mixer_ctl_set_value(ctl, 0, miid);
    else
        PAL_ERR(LOG_TAG, "Failed to set %s", SP_MIID_CTL);
}

int OplusSpeakerProtection::updateRxConfig()
{
    std::vector<Stream *> activeStreams;
    std::shared_ptr<Device> dev;
    std::string backEndName;
    Session *session = nullptr;
    uint32_t miid = 0;
    int ret;

    if (!mAmp) {
        ret = probeAmp();
        if (ret)
            return ret;
    }

    if (!mCalibrated) {
        ret = mAmp->readCalibration(mHwMixer);
        if (ret)
            return ret;
        mCalibrated = true;
    }

    rm->getBackendName(mDeviceAttr.id, backEndName);
    dev = Device::getInstance(&mDeviceAttr, rm);
    ret = rm->getActiveStream_l(activeStreams, dev);
    if (ret || activeStreams.empty()) {
        PAL_ERR(LOG_TAG, "No active stream available");
        return ret ? ret : -EINVAL;
    }

    Stream *stream = activeStreams[0];
    stream->getAssociatedSession(&session);
    if (!session)
        return -EINVAL;

    ret = session->getMIID(backEndName.c_str(), MODULE_SP, &miid);
    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to get the SP module miid, status %d", ret);
        return ret;
    }

    ret = sendCalibration(miid);
    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to send the calibration, status %d", ret);
        return ret;
    }

    sendRxInfoToDriver(stream, miid);
    return 0;
}

int OplusSpeakerProtection::openViPath()
{
    uint32_t devicePropId[] = {0x08000010, 1, 0x2};
    std::vector<std::pair<int, int>> keyVector, calVector;
    struct agmMetaData deviceMetaData(nullptr, 0);
    struct audio_route *audioRoute = nullptr;
    struct pal_stream_attributes sAttr = {};
    struct pal_device_info viInfo = {};
    struct pal_device device = {};
    struct pcm_config config = {};
    struct mixer_ctl *ctl;
    std::ostringstream metadataCtl, connectCtl;
    int ret;

    rm->getDeviceInfo(PAL_DEVICE_IN_VI_FEEDBACK, PAL_STREAM_PROXY, "", &viInfo);
    if (viInfo.channels != mAmp->speakerCount() || viInfo.channels > CHANNELS_2) {
        PAL_ERR(LOG_TAG, "Unexpected VI channel count %d", viInfo.channels);
        return -EINVAL;
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
    mViSndDevice = viInfo.sndDevName;

    device.config.ch_info.channels = viInfo.channels;
    device.config.ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
    device.config.ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;
    device.config.sample_rate = deviceAttr.config.sample_rate;
    device.config.bit_width = viInfo.bit_width;
    device.config.aud_fmt_id = rm->getAudioFmt(viInfo.bit_width);

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

    config.rate = device.config.sample_rate;
    config.channels = viInfo.channels;
    config.format = viInfo.bit_width == 32 ? PCM_FORMAT_S32_LE : PCM_FORMAT_S16_LE;
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

    sAttr.type = PAL_STREAM_LOW_LATENCY;
    sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
    rm->freeFrontEndIds(mPcmDevIdTx, sAttr, TX_HOSTLESS);
    mPcmDevIdTx.clear();

    if (mTxPcm)
        pcm_close(mTxPcm);
    mTxPcm = nullptr;

    if (!rm->getAudioRoute(&audioRoute))
        disableDevice(audioRoute, const_cast<char *>(mViSndDevice.c_str()));
}

int32_t OplusSpeakerProtection::start()
{
    int ret = 0;

    mDeviceMutex.lock();

    const bool firstStart = deviceStartStopCount == 0;
    if (firstStart) {
        // Fail the speaker path rather than running it unprotected.
        ret = updateRxConfig();
        if (!ret)
            ret = openViPath();
        if (ret) {
            PAL_ERR(LOG_TAG, "Speaker protection unavailable, status %d", ret);
            mDeviceMutex.unlock();
            return ret;
        }
    }

    ret = start_l();
    if (ret && firstStart)
        closeViPath();

    mDeviceMutex.unlock();
    return ret;
}

int32_t OplusSpeakerProtection::stop()
{
    mDeviceMutex.lock();

    int ret = stop_l();
    if (deviceStartStopCount == 0)
        closeViPath();

    mDeviceMutex.unlock();
    return ret;
}
