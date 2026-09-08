/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: SoundDoseUtility"

#include "SoundDoseUtility.h"

#include <agm/agm_api.h>

#include <chrono>
#include <ctime>

#include "PalMappings.h"
#include "PayloadBuilder.h"
#include "ResourceManager.h"
#include "SessionAgm.h"
#include "SessionAlsaUtils.h"
#include "kvh2xml.h"
#include "sound_dose_api.h"

#define DEFAULT_PERIOD_SIZE 256
#define DEFAULT_PERIOD_COUNT 4

bool SoundDoseUtility::isEnabled(pal_device_id_t deviceId) {
    if (!ResourceManager::IsSoundDoseEnabled()) {
        PAL_DBG(LOG_TAG, "sounddose is disabled for device %d", deviceId);
        return false;
    }
    return supportsSoundDose(deviceId);
}

bool SoundDoseUtility::supportsSoundDose(pal_device_id_t deviceId) {
    switch (deviceId) {
        case PAL_DEVICE_OUT_WIRED_HEADSET:
        case PAL_DEVICE_OUT_WIRED_HEADPHONE:
        case PAL_DEVICE_OUT_BLUETOOTH_A2DP:
        case PAL_DEVICE_OUT_USB_HEADSET:
        case PAL_DEVICE_OUT_BLUETOOTH_BLE:
        case PAL_DEVICE_OUT_BLUETOOTH_BLE_BROADCAST:
            return true;
        default:
            return false;
    }
}

pal_global_callback SoundDoseUtility::getCallback() {
    return mResourceManager ? mResourceManager->getCallback() : nullptr;
}

uint64_t SoundDoseUtility::getCookie() {
    return mResourceManager ? mResourceManager->getCookie() : 0;
}

struct pal_stream_attributes getDefautStreamAttributes() {
    struct pal_stream_attributes streamAttributes = {};
    streamAttributes.type = PAL_STREAM_LOW_LATENCY;
    streamAttributes.direction = PAL_AUDIO_OUTPUT;
    return streamAttributes;
}

struct pal_device getDefaultDeviceConfig() {
    struct pal_device device = {};
    struct pal_channel_info ch_info = {};
    // setup dummy dev tx graph with sound dose module.
    device.id = PAL_DEVICE_OUT_SOUND_DOSE;
    /* Configure device attributes TODO: check the config? */
    ch_info.channels = CHANNELS_1;
    ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
    device.config.ch_info = ch_info;
    device.config.sample_rate = 48000;
    device.config.bit_width = BITWIDTH_16;
    device.config.aud_fmt_id = PAL_AUDIO_FMT_DEFAULT_COMPRESSED;
    return device;
}

struct pcm_config getDefaultPcmConfig() {
    struct pcm_config config = {};
    config.rate = 48000;
    config.format = PCM_FORMAT_S16_LE;
    config.channels = CHANNELS_1;
    config.period_size = DEFAULT_PERIOD_SIZE;
    config.period_count = DEFAULT_PERIOD_COUNT;
    config.start_threshold = 0;
    config.stop_threshold = INT_MAX;
    config.silence_threshold = 0;
    return config;
}

/**
 * @brief Fills the sound dose information structure.
 *
 * This helper function populates the `pal_sound_dose_info_t` structure with MEL values and
 * timestamps. Ensure that `palSoundDoseInfo->num_mel_values` is set before calling this function.
 * This function is used for both events generated from `param_id_sounddose_flush_mel_values`
 *  and `event_id_sound_dose_mel_values`.
 *
 * @param palSoundDoseInfo Pointer to the sound dose information structure to be filled.
 * @param data Pointer to the data containing MEL values and timestamps.
 * @param isFlushTypeEvent Boolean flag indicating if the event is of
 * param_id_sounddose_flush_mel_values.
 */
void fillSoundDoseInfo(pal_sound_dose_info_t *palSoundDoseInfo, void *data, bool isFlushTypeEvent) {
    uint32_t offsetToMelValues = sizeof(uint32_t) + sizeof(uint32_t);
    if (isFlushTypeEvent) {
        offsetToMelValues = sizeof(uint32_t);
    }

    PAL_DBG(LOG_TAG, "Mel values %d", palSoundDoseInfo->num_mel_values);
    uint32_t offsetToTimestampValues =
            offsetToMelValues + sizeof(float) * palSoundDoseInfo->num_mel_values;
    float *melValuesBase =
            reinterpret_cast<float *>(reinterpret_cast<uint8_t *>(data) + offsetToMelValues);
    uint32_t *timestampBase = reinterpret_cast<uint32_t *>(reinterpret_cast<uint8_t *>(data) +
                                                           offsetToTimestampValues);

    for (int i = 0; i < palSoundDoseInfo->num_mel_values; i++) {
        palSoundDoseInfo->mel_values[i] = melValuesBase[i];
        uint32_t lsbTime = timestampBase[2 * i];
        uint32_t msbTime = timestampBase[2 * i + 1];
        palSoundDoseInfo->timestamp[i] = (static_cast<uint64_t>(msbTime) << 32) | lsbTime;
        palSoundDoseInfo->timestamp[i] = palSoundDoseInfo->timestamp[i] / 1000000LL;
        PAL_VERBOSE(LOG_TAG, "Mel values i %d, timestamp %lu", i, palSoundDoseInfo->timestamp[i]);
    }
}

SoundDoseUtility::SoundDoseUtility(Device *devObj, const struct pal_device device) {
    pal_device_id_t deviceId = device.id;
    int status = 0;
    mPalDevice = device;
    mDevObj = devObj;

    mResourceManager = ResourceManager::getInstance();
    if (!mResourceManager) {
        PAL_ERR(LOG_TAG, "Resource manager instance unavailable, disable sounddose");
        return;
    }

    status = mResourceManager->getVirtualAudioMixer(&mVirtualMixer);
    if (status) {
        PAL_ERR(LOG_TAG, "getVirtualAudioMixer failed %d", status);
        return;
    }

    mIsEnabled = isEnabled(deviceId);
    PAL_INFO(LOG_TAG, "device %s mIsEnabled %d", toString(&mPalDevice).c_str(), mIsEnabled);
}

SoundDoseUtility::~SoundDoseUtility() {
    PAL_DBG(LOG_TAG, "device %s mIsEnabled %d", toString(&mPalDevice).c_str(), mIsEnabled);
}

void SoundDoseUtility::startComputation() {
    if (mIsEnabled) {
        startComputationInternal();
    } else {
        startSimulation();
    }
}

void SoundDoseUtility::stopComputation() {
    // Logic to stop dosage computation for the device
    if (mIsEnabled) {
        stopComputationInternal();
    } else {
        stopSimulation();
    }
}

int SoundDoseUtility::registerMixerEvent(const bool state, const std::string backendName) {
    struct agm_event_reg_cfg event_cfg;

    size_t payloadSize = sizeof(struct agm_event_reg_cfg);
    event_cfg.event_id = EVENT_ID_SOUND_DOSE_MEL_VALUES;
    event_cfg.event_config_payload_size = 0;
    // update is_register based on the state.
    event_cfg.is_register = state;

    PAL_DBG(LOG_TAG, "%s events for SoundDose module", state ? "register" : "unregister");

    int ret = SessionAlsaUtils::registerMixerEvent(mVirtualMixer, mPcmDevIdRx.at(0),
                                                   backendName.c_str(), MODULE_SOUND_DOSE,
                                                   (void *)&event_cfg, payloadSize);
    return ret;
}

void SoundDoseUtility::startComputationInternal() {
    // Logic to start dosage computation for the device
    // Call pcm_open, register for event and pcm_start.
    PAL_INFO(LOG_TAG, "%p startComputation for device %s ", this, toString(&mPalDevice).c_str());
    int ret = 0, dir = RX_HOSTLESS;
    char mSndDeviceName[128] = {0};
    uint32_t devicePropId[] = {0x08000010, 1, 0x2};

    struct pal_device device = getDefaultDeviceConfig();
    struct pal_stream_attributes sAttr = getDefautStreamAttributes();
    struct pcm_config config = getDefaultPcmConfig();

    std::vector<std::pair<int, int>> keyVector;
    std::vector<std::pair<int, int>> calVector;

    struct agmMetaData deviceMetaData(nullptr, 0);
    struct mixer_ctl *beMetaDataMixerCtrl = nullptr;
    struct mixer_ctl *connectCtrl = NULL;
    struct audio_route *audioRoute = NULL;
    std::ostringstream connectCtrlName;
    std::ostringstream connectCtrlNameBe;
    std::string backendName;
    strlcpy(mSndDeviceName, "dummy-dev", DEVICE_NAME_MAX_SIZE);

    ret = mResourceManager->getAudioRoute(&audioRoute);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed in getAudioRoute %d", ret);
        goto exit;
    }

    mResourceManager->getBackendName(device.id, backendName);
    if (backendName.empty()) {
        PAL_ERR(LOG_TAG, "getBackendName failed for id %d", device.id);
        goto exit;
    }

    ret = PayloadBuilder::getDeviceKV(device.id, keyVector);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "getDeviceKV for id %d", device.id);
        goto exit;
    }

    SessionAlsaUtils::getAgmMetaData(keyVector, calVector, (struct prop_data *)devicePropId,
                                     deviceMetaData);
    if (!deviceMetaData.size) {
        PAL_ERR(LOG_TAG, "SoundDose device metadata is zero");
        ret = -ENOMEM;
        goto exit;
    }
    connectCtrlNameBe << backendName << " metadata";
    beMetaDataMixerCtrl = mixer_get_ctl_by_name(mVirtualMixer, connectCtrlNameBe.str().data());
    if (!beMetaDataMixerCtrl) {
        PAL_ERR(LOG_TAG, "invalid mixer control for SoundDose : %s", backendName.c_str());
        ret = -EINVAL;
        goto exit;
    }

    if (deviceMetaData.size) {
        ret = mixer_ctl_set_array(beMetaDataMixerCtrl, (void *)deviceMetaData.buf,
                                  deviceMetaData.size);
        free(deviceMetaData.buf);
        deviceMetaData.buf = nullptr;
    } else {
        PAL_ERR(LOG_TAG, "Device Metadata not set for RX path");
        ret = -EINVAL;
        goto exit;
    }

    ret = mDevObj->setMediaConfig(mResourceManager, backendName, &device);
    if (ret) {
        PAL_ERR(LOG_TAG, "setDeviceMediaConfig for sound dose device failed");
        goto exit;
    }

    mPcmDevIdRx = mResourceManager->allocateFrontEndIds(sAttr, dir);
    if (mPcmDevIdRx.size() == 0) {
        PAL_ERR(LOG_TAG, "allocateFrontEndIds failed");
        ret = -ENOSYS;
        goto exit;
    }

    connectCtrlName << "PCM" << mPcmDevIdRx.at(0) << " connect";
    connectCtrl = mixer_get_ctl_by_name(mVirtualMixer, connectCtrlName.str().data());
    if (!connectCtrl) {
        PAL_ERR(LOG_TAG, "invalid mixer control: %s", connectCtrlName.str().data());
        goto free_fe;
    }
    ret = mixer_ctl_set_enum_by_string(connectCtrl, backendName.c_str());
    if (ret) {
        PAL_ERR(LOG_TAG, "Mixer control %s set with %s failed: %d", connectCtrlName.str().data(),
                backendName.c_str(), ret);
        goto free_fe;
    }

    mRxPcm = pcm_open(mResourceManager->getVirtualSndCard(), mPcmDevIdRx.at(0), PCM_OUT, &config);
    if (!mRxPcm) {
        PAL_ERR(LOG_TAG, "pcm_open failed");
        goto free_fe;
    }

    if (!pcm_is_ready(mRxPcm)) {
        PAL_ERR(LOG_TAG, "pcm_open not ready");
        goto err_pcm_open;
    }

    ret = registerMixerEvent(true, backendName);
    if (ret) {
        PAL_ERR(LOG_TAG, "registerMixerEvent failed");
        goto err_pcm_open;
    }

    // Register to mixtureControlEvents and wait for the MEL values
    ret = mResourceManager->registerMixerEventCallback(mPcmDevIdRx, handleSoundDoseCallback,
                                                       (uint64_t)this, true);
    if (ret != 0) {
        PAL_ERR(LOG_TAG, "registerMixerEventCallback failed");
        goto err_pcm_open;
    }

    PAL_DBG(LOG_TAG, " enable device SoundDose module");
    enableDevice(audioRoute, mSndDeviceName);

    PAL_DBG(LOG_TAG, "pcm start for Sound Dose graph");
    if (pcm_start(mRxPcm) < 0) {
        PAL_ERR(LOG_TAG, "pcm start failed for Sound Dose graph");
        ret = -ENOSYS;
        goto err_pcm_open;
    }

    goto exit;
err_pcm_open:
    if (mRxPcm) {
        pcm_close(mRxPcm);
        disableDevice(audioRoute, mSndDeviceName);
        mRxPcm = NULL;
    }

free_fe:
    mResourceManager->freeFrontEndIds(mPcmDevIdRx, sAttr, dir);
exit:

    PAL_DBG(LOG_TAG, "Exit");
}

void SoundDoseUtility::stopComputationInternal() {
    PAL_INFO(LOG_TAG, "%p stopComputation for device %s", this, toString(&mPalDevice).c_str());
    struct pal_stream_attributes sAttr = getDefautStreamAttributes();
    int ret = 0;

    std::string backendName;
    mResourceManager->getBackendName(PAL_DEVICE_OUT_SOUND_DOSE, backendName);
    if (backendName.empty()) {
        PAL_ERR(LOG_TAG, "getBackendName failed for sounddose");
        return;
    }

    if (!mRxPcm) {
        PAL_ERR(LOG_TAG, "mRxPcm is null");
        return;
    }

    pcm_stop(mRxPcm);

    /* Add getParameter for sounddose for remaining data. parse the data received and
     * invoke callback to hal wih the data */
    getSoundDoseMelValues();

    mResourceManager->freeFrontEndIds(mPcmDevIdRx, sAttr, RX_HOSTLESS);

    /* De-register for the sound dose events. */
    ret = registerMixerEvent(false, backendName);
    if (ret) {
        PAL_ERR(LOG_TAG, "registerMixerEvent failed");
    }
    pcm_close(mRxPcm);
    mRxPcm = NULL;

exit:
    PAL_DBG(LOG_TAG, "Exit");
}

void SoundDoseUtility::getSoundDoseMelValues() {
    PAL_DBG(LOG_TAG, "%p getSoundDoseMelValues for %s", this, toString(&mPalDevice).c_str());
    int ret = 0;
    uint32_t miid = 0;
    const char *getParamControl = "getParam";
    char *pcmDeviceName = NULL;
    uint8_t *payload = NULL;
    size_t payloadSize = 0;
    struct mixer_ctl *ctl;
    std::ostringstream cntrlName;
    PayloadBuilder *builder = new PayloadBuilder();
    param_id_sounddose_flush_mel_values *remainingMelValues;
    uint32_t *eventData = nullptr;
    pal_global_callback_event_t event;
    std::string backendName;

    pcmDeviceName = mResourceManager->getDeviceNameFromID(mPcmDevIdRx.at(0));
    if (pcmDeviceName) {
        cntrlName << pcmDeviceName << " " << getParamControl;
    } else {
        PAL_ERR(LOG_TAG, "Error: %d Unable to get Device name", -EINVAL);
        goto exit;
    }

    ctl = mixer_get_ctl_by_name(mVirtualMixer, cntrlName.str().data());
    if (!ctl) {
        ret = -ENOENT;
        PAL_ERR(LOG_TAG, "Error: %d Invalid mixer control: %s", ret, cntrlName.str().data());
        goto exit;
    }

    mResourceManager->getBackendName(PAL_DEVICE_OUT_SOUND_DOSE, backendName);
    if (backendName.empty()) {
        ret = -ENOENT;
        PAL_ERR(LOG_TAG, "Error: %d Failed to get SoundDose backend name", ret);
        goto exit;
    }

    ret = SessionAlsaUtils::getModuleInstanceId(mVirtualMixer, mPcmDevIdRx.at(0),
                                                backendName.c_str(), MODULE_SOUND_DOSE, &miid);

    if (ret != 0) {
        PAL_ERR(LOG_TAG, "Error: %d Failed to get tag info %x", ret, MODULE_SOUND_DOSE);
        goto exit;
    }

    ret = builder->payloadSoundDoseInfo(&payload, &payloadSize, miid);
    if (ret || !payload) {
        PAL_ERR(LOG_TAG, "payloadSoundDoseInfo failed, ret = %d", ret);
        ret = -ENOMEM;
        goto exit;
    }

    ret = mixer_ctl_set_array(ctl, payload, payloadSize);
    if (ret != 0) {
        PAL_ERR(LOG_TAG, "Set failed ret = %d", ret);
        goto exit;
    }

    memset(payload, 0, payloadSize);

    ret = mixer_ctl_get_array(ctl, payload, payloadSize);
    if (ret != 0) {
        PAL_ERR(LOG_TAG, "Get failed ret = %d", ret);
    } else {
        // Parse payload
        PAL_DBG(LOG_TAG, "Received Sound Dose getParam");

        remainingMelValues =
                (param_id_sounddose_flush_mel_values *)(payload +
                                                        sizeof(struct apm_module_param_data_t));

        std::unique_ptr<pal_sound_dose_info_t> palSoundDoseInfo =
                std::make_unique<pal_sound_dose_info_t>();
        if (!palSoundDoseInfo) {
            PAL_ERR(LOG_TAG, "Memory allocation failed");
            return;
        }
        palSoundDoseInfo->num_mel_values = remainingMelValues->num_mel_values;

        if (remainingMelValues->num_mel_values <= 0) {
            PAL_INFO(LOG_TAG, "no mel values to process");
            return;
        }

        palSoundDoseInfo->device = getPalDevice();

        fillSoundDoseInfo(palSoundDoseInfo.get(), remainingMelValues, true);
        onSoundDoseEvent(palSoundDoseInfo.get());
    }

    if (payload) {
        delete payload;
    }

exit:
    if (builder) {
        delete builder;
    }

    PAL_DBG(LOG_TAG, " Exit");
}

void SoundDoseUtility::onSoundDoseEvent(pal_sound_dose_info_t *callbackData) {
    auto callback = getCallback();
    if (callback) {
        PAL_DBG(LOG_TAG, "Notifying client about sound dose global cb %pK", callback);
        uint32_t *eventData = reinterpret_cast<uint32_t *>(callbackData);
        callback(PAL_SOUND_DOSE_INFO, eventData, getCookie());
    }
}

void SoundDoseUtility::handleSoundDoseCallback(uint64_t hdl, uint32_t event_id, void *data,
                                               uint32_t event_size) {
    uint32_t *eventData = nullptr;
    pal_global_callback_event_t event;
    event_id_sound_dose_mel_values *event_data = nullptr;

    PAL_DBG(LOG_TAG, "Got event from DSP %x", event_id);

    if (event_id != EVENT_ID_SOUND_DOSE_MEL_VALUES) {
        PAL_ERR(LOG_TAG, "Not sound dose mel event");
        return;
    }

    if (data == nullptr) {
        PAL_ERR(LOG_TAG, "data is null");
        return;
    }

    SoundDoseUtility *sounddoseInstance = reinterpret_cast<SoundDoseUtility *>(hdl);
    event_data = static_cast<event_id_sound_dose_mel_values *>(data);
    // use unique_ptr for pal_info
    std::unique_ptr<pal_sound_dose_info_t> palSoundDoseInfo =
            std::make_unique<pal_sound_dose_info_t>();
    if (!palSoundDoseInfo) {
        PAL_ERR(LOG_TAG, "Memory allocation failed for palSoundDoseInfo");
        return;
    }

    palSoundDoseInfo->is_momentary_exposure_warning = event_data->is_momentary_exposure_raised;
    palSoundDoseInfo->num_mel_values = event_data->num_mel_values;

    if (event_data->num_mel_values <= 0) {
        PAL_INFO(LOG_TAG, "no mel values to process");
        return;
    }

    palSoundDoseInfo->device = sounddoseInstance->getPalDevice();

    uint32_t offsetToMelValues = sizeof(uint32_t) + sizeof(uint32_t);
    float *melValuesBase =
            reinterpret_cast<float *>(reinterpret_cast<uint8_t *>(data) + offsetToMelValues);

    /*Parse payload based on momentary exposure warning or not. */
    if (palSoundDoseInfo->is_momentary_exposure_warning) {
        // if momentary exposure warning then only 1 MEL value is raised
        if (event_data->num_mel_values > 0) {
            palSoundDoseInfo->mel_values[0] = melValuesBase[0];
        } else {
            PAL_ERR(LOG_TAG, "Momentary warning raised, but num mel values = 0");
        }
    } else {
        fillSoundDoseInfo(palSoundDoseInfo.get(), event_data, false);
    }

    // notifying about new sounddose info
    sounddoseInstance->onSoundDoseEvent(palSoundDoseInfo.get());

    PAL_DBG(LOG_TAG, " Exit");
}

void SoundDoseUtility::startSimulation() {
    if (!mUseSimulation) {
        return; // don't use Simulation
    }

    PAL_INFO(LOG_TAG, " Enter: %p refcount %d", this, mRefCount.load());

    if (mRefCount.fetch_add(1) == 0) {
        mSimulationRunning = true;
        PAL_INFO(LOG_TAG, " create simulationThreadLoop %p ", this);
        mSimulationThread = std::thread(&SoundDoseUtility::simulationThreadLoop, this);
    } else {
        PAL_INFO(LOG_TAG, " thread is already running %p refCount %d ", this, mRefCount.load());
    }
}

void SoundDoseUtility::stopSimulation() {
    if (!mSimulationRunning) {
        return;
    }

    PAL_INFO(LOG_TAG, " Enter: %p refcount %d", this, mRefCount.load());

    if (mRefCount.fetch_sub(1) == 1) {
        mSimulationRunning = false;
        if (mSimulationThread.joinable()) {
            mSimulationThread.join();
        }
    } else {
        PAL_INFO(LOG_TAG, " refcount %d is not zero yet %p", mRefCount.load(), this);
    }
}

void SoundDoseUtility::simulationThreadLoop() {
    int count = 0;
    while (mSimulationRunning) {
        pal_sound_dose_info_t palSoundDoseInfo;
        palSoundDoseInfo.device = mPalDevice;
        // random value each 5th value set exposure warning
        palSoundDoseInfo.is_momentary_exposure_warning = (count % 5 == 0);
        count++;
        palSoundDoseInfo.num_mel_values = PAL_MAX_SOUND_DOSE_VALUES;

        for (uint32_t i = 0; i < PAL_MAX_SOUND_DOSE_VALUES; ++i) {
            palSoundDoseInfo.mel_values[i] = static_cast<float>(i + 20);
            palSoundDoseInfo.timestamp[i] =
                    std::chrono::system_clock::now().time_since_epoch().count();
        }

        // Simulate event generation (replace with actual callback if needed)
        auto callback = getCallback();

        if (callback) {
            PAL_INFO(LOG_TAG, "Notifying client about sounddose global cb %pK device %s", callback,
                     toString(&mPalDevice).c_str());
            uint32_t *eventData = reinterpret_cast<uint32_t *>(&palSoundDoseInfo);
            pal_global_callback_event_t event = PAL_SOUND_DOSE_INFO;
            callback(event, eventData, getCookie());
        } else {
            PAL_ERR(LOG_TAG, "no valid callback found");
        }

        // generate event every 2 seconds for test
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
