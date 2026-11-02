/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: OplusSpeakerProtection"

#include "OplusSpeakerProtection.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include <agm/agm_api.h>
#include <tinyalsa/asoundlib.h>

#include "PalAudioRoute.h"
#include "PayloadBuilder.h"
#include "ResourceManager.h"
#include "Session.h"
#include "SessionAgm.h"
#include "SessionAlsaUtils.h"
#include "Stream.h"
#include "kvh2xml.h"

#define TFA_DEFAULT_IMPEDANCE_CTL "TFA Default Impedance"
#define TFA_CALIBRATION_CTL "TFA Calibration"
#define TFA_F0_CALIBRATION_CTL "TFA F0 Calibration"
#define SP_PCM_ID_CTL "SP PCMID"
#define SP_MIID_CTL "SP MIID"

#define TFA_FIRST_ADDRESS 0x34
#define TFA_LAST_ADDRESS 0x37
#define TFA_SPEAKER_COUNT 2

#define PARAM_ID_TFA_DSP_CMD 0x1800b921

#define PAYLOAD_ALIGN(num, to) (((num) + ((to) - 1)) & ~((to) - 1))

#define VI_PERIOD_SIZE 256
#define VI_PERIOD_COUNT 4

// Matches apm_module_param_data_t.
struct ParamHeader {
    uint32_t module_instance_id;
    uint32_t param_id;
    uint32_t param_size;
    uint32_t error_code;
};

OplusSpeakerProtection::OplusSpeakerProtection(struct pal_device *device,
                                               std::shared_ptr<ResourceManager> Rm)
    : Device(device, Rm), mF0(0), mVirtMixer(nullptr), mHwMixer(nullptr), mTxPcm(nullptr)
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

int OplusSpeakerProtection::readCalibration()
{
    struct mixer_ctl *defaultCtl, *calCtl, *f0Ctl;

    if (!mHwMixer)
        return -ENODEV;

    defaultCtl = mixer_get_ctl_by_name(mHwMixer, TFA_DEFAULT_IMPEDANCE_CTL);
    calCtl = mixer_get_ctl_by_name(mHwMixer, TFA_CALIBRATION_CTL);
    if (!defaultCtl || !calCtl) {
        PAL_ERR(LOG_TAG, "Missing the TFA calibration controls");
        return -ENODEV;
    }

    if (mixer_ctl_get_num_values(defaultCtl) != TFA_SPEAKER_COUNT) {
        PAL_ERR(LOG_TAG, "Unexpected speaker count %d", mixer_ctl_get_num_values(defaultCtl));
        return -EINVAL;
    }

    std::vector<TfaCalibration> calibration;
    for (uint32_t addr = TFA_FIRST_ADDRESS; addr <= TFA_LAST_ADDRESS; addr++) {
        char path[64];
        TfaCalibration info = {};

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
              [](const TfaCalibration &a, const TfaCalibration &b) { return a.index < b.index; });

    for (uint32_t i = 0; i < calibration.size(); i++) {
        TfaCalibration &info = calibration[i];
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
    f0Ctl = mixer_get_ctl_by_name(mHwMixer, TFA_F0_CALIBRATION_CTL);
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

int OplusSpeakerProtection::buildCalibrationPayload(uint32_t miid)
{
    uint8_t cmd[32] = {};
    size_t size = 0;

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
    const size_t payloadSize = PAYLOAD_ALIGN(sizeof(ParamHeader) + paramSize, 8);
    uint8_t *payload = static_cast<uint8_t *>(calloc(1, payloadSize));
    if (!payload)
        return -ENOMEM;

    auto *header = reinterpret_cast<ParamHeader *>(payload);
    header->module_instance_id = miid;
    header->param_id = PARAM_ID_TFA_DSP_CMD;
    header->param_size = paramSize;
    header->error_code = 0;
    memcpy(payload + sizeof(*header), cmd, size);

    if (customPayload)
        free(customPayload);
    customPayload = nullptr;
    customPayloadSize = 0;

    int ret = updateCustomPayload(payload, payloadSize);
    free(payload);
    return ret;
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

    if (mCalibration.empty()) {
        ret = readCalibration();
        if (ret)
            return ret;
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

    ret = buildCalibrationPayload(miid);
    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to build the calibration payload, status %d", ret);
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
    if (viInfo.channels != TFA_SPEAKER_COUNT) {
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

    device.config.ch_info.channels = CHANNELS_2;
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
