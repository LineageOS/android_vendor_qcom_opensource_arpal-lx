/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: SpeakerProtectionwsa885x"


#include "SpeakerProtection.h"
#include "SpeakerProtectionwsa88xx.h"
#include "PalAudioRoute.h"
#include "ResourceManager.h"
#include "SessionAlsaUtils.h"
#include "kvh2xml.h"
#include <agm/agm_api.h>
#include "SessionAR.h"


std::thread SpeakerProtectionwsa885x::viTxSetupThread;
std::mutex SpeakerProtectionwsa885x::calibrationMutex;
bool SpeakerProtectionwsa885x::viTxSetupThrdCreated;

SpeakerProtectionwsa885x::SpeakerProtectionwsa885x(struct pal_device *device,
                        std::shared_ptr<ResourceManager> Rm):SpeakerProtection(device, Rm)
{
    viTxSetupThrdCreated = false;
    wsaTemp = 0;
}

SpeakerProtectionwsa885x::~SpeakerProtectionwsa885x()
{
    viTxSetupThrdCreated = false;
}

int SpeakerProtectionwsa885x::getSpeakerTemperature(int spkr_pos) {
    int ret;
    struct mixer_ctl *trigger_temp_ctl;
    struct mixer_ctl *temp_ctl;
    int status = 0;

    PAL_DBG(LOG_TAG, "Enter Speaker Get Temperature");

    if (wsaTemp != 0) {
        PAL_DBG(LOG_TAG, "Exiting Speaker Get Temperature %d", wsaTemp);
        return wsaTemp;
    }

    trigger_temp_ctl = mixer_get_ctl_by_name(hwMixer, "SA1 Trigger Die Temperature");
    if (!trigger_temp_ctl) {
       PAL_ERR(LOG_TAG, "Invalid mixer control: SA1 Trigger Die Temperature");
       return -ENOENT;
    }

    ret = mixer_ctl_set_value(trigger_temp_ctl, 0, 1);
    if (ret) {
         PAL_ERR(LOG_TAG, "Could not Enable ctl for mixer cmd - %s ret %d\n",
                  "SA1 Trigger Die Temperature", ret);
        return -EINVAL;
    }

    temp_ctl = mixer_get_ctl_by_name(hwMixer, "SA1 Die Temperature");
    if (!temp_ctl) {
       PAL_ERR(LOG_TAG, "Invalid mixer control: SA1 Die Temperature");
       status = -EINVAL;
       goto exit;
    }

    status = mixer_ctl_get_value(temp_ctl, 0);
    wsaTemp = status;

    PAL_DBG(LOG_TAG, "Exiting Speaker Get Temperature %d", status);
exit:

   ret = mixer_ctl_set_value(trigger_temp_ctl, 0, 0);
   if (ret)
      PAL_ERR(LOG_TAG, "Could not Disable ctl for mixer cmd - %s ret %d\n",
         "SA1 Trigger Die Temperature", ret);

    PAL_DBG(LOG_TAG, "Status : %d", status);
    return status;
}

int SpeakerProtectionwsa885x::spkrStartCalibration()
{
    FILE *fp;
    struct pal_device device, deviceRx;
    struct pal_channel_info ch_info;
    struct pal_device_info deviceRxSpkr;
    struct pal_stream_attributes sAttr;
    struct pcm_config config;
    struct mixer_ctl *connectCtrl = NULL;
    struct audio_route *audioRoute = NULL;
    struct agm_event_reg_cfg event_cfg;
    struct agmMetaData deviceMetaData(nullptr, 0);
    struct mixer_ctl *beMetaDataMixerCtrl = nullptr;
    int ret = 0, status = 0, dir = 0, i = 0, flags = 0, payload_size = 0;
    uint32_t miid = 0;
    char mSndDeviceName_rx[128] = {0};
    char mSndDeviceName_vi[128] = {0};
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    uint32_t devicePropId[] = {0x08000010, 1, 0x2};
    bool isTxStarted = false, isRxStarted = false;
    bool isTxFeandBeConnected = false, isRxFeandBeConnected = false;
    std::string backEndNameTx, backEndNameRx;
    std::vector <std::pair<int, int>> keyVector, calVector;
    std::vector<int> pcmDevIdsRx, pcmDevIdsTx;
    std::shared_ptr<ResourceManager> rm;
    std::ostringstream connectCtrlName;
    std::ostringstream connectCtrlNameRx;
    std::ostringstream connectCtrlNameBe;
    std::ostringstream connectCtrlNameBeVI;
    param_id_sp_vi_op_mode_cfg_t modeConfg;
    param_id_sp_vi_channel_map_cfg_t viChannelMapConfg;
    param_id_sp_op_mode_t spModeConfg;
    param_id_sp_ex_vi_mode_cfg_t viExModeConfg;
    session_callback sessionCb;

    std::unique_lock<std::mutex> calLock(calibrationMutex);

    memset(&device, 0, sizeof(device));
    memset(&deviceRx, 0, sizeof(deviceRx));
    memset(&sAttr, 0, sizeof(sAttr));
    memset(&config, 0, sizeof(config));
    memset(&modeConfg, 0, sizeof(modeConfg));
    memset(&viChannelMapConfg, 0, sizeof(viChannelMapConfg));
    memset(&spModeConfg, 0, sizeof(spModeConfg));
    memset(&viExModeConfg, 0, sizeof(viExModeConfg));

    sessionCb = handleSPCallback;
    PayloadBuilder* builder = new PayloadBuilder();

    keyVector.clear();
    calVector.clear();

    PAL_DBG(LOG_TAG, "Enter");

    if (customPayloadSize) {
        free(customPayload);
        customPayloadSize = 0;
        customPayload = NULL;
    }

    rm = ResourceManager::getInstance();
    if (!rm) {
        PAL_ERR(LOG_TAG, "Error: %d Failed to get resource manager instance", -EINVAL);
        goto exit;
    }

    rm->getDeviceInfo(PAL_DEVICE_IN_VI_FEEDBACK, PAL_STREAM_PROXY, "", &vi_device);

    // Configure device attribute
    rm->getChannelMap(&(ch_info.ch_map[0]), vi_device.channels);
    switch (vi_device.channels) {
        case 1 :
            ch_info.channels = CHANNELS_1;
        break;
        case 2 :
            ch_info.channels = CHANNELS_2;
        break;
        default:
            PAL_DBG(LOG_TAG, "Unsupported channel. Set default as 2");
            ch_info.channels = CHANNELS_2;
        break;
    }

    device.config.ch_info = ch_info;
    device.config.sample_rate = vi_device.samplerate;
    device.config.bit_width = vi_device.bit_width;
    device.config.aud_fmt_id = rm->getAudioFmt(vi_device.bit_width);

    // Setup TX path
    ret = rm->getAudioRoute(&audioRoute);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to get the audio_route name status %d", ret);
        goto exit;
    }

    device.id = PAL_DEVICE_IN_VI_FEEDBACK;
    strlcpy(mSndDeviceName_vi, vi_device.sndDevName.c_str(), DEVICE_NAME_MAX_SIZE);
    PAL_DBG(LOG_TAG, "got the audio_route name %s", mSndDeviceName_vi);

    rm->getBackendName(device.id, backEndNameTx);
    if (!strlen(backEndNameTx.c_str())) {
        PAL_ERR(LOG_TAG, "Failed to obtain tx backend name for %d", device.id);
        goto exit;
    }

    ret = PayloadBuilder::getDeviceKV(device.id, keyVector);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to obtain device KV for %d", device.id);
        goto exit;
    }

    // Enable VI module
    switch(numberOfChannels) {
        case 1 :
            // TODO: check it from RM.xml for left or right configuration
            calVector.push_back(std::make_pair(SPK_PRO_VI_MAP, RIGHT_SPKR));
        break;
        case 2 :
            calVector.push_back(std::make_pair(SPK_PRO_VI_MAP, STEREO_SPKR));
        break;
        default :
            PAL_ERR(LOG_TAG, "Unsupported channel");
            goto exit;
    }

    SessionAlsaUtils::getAgmMetaData(keyVector, calVector,
                    (struct prop_data *)devicePropId, deviceMetaData);
    if (!deviceMetaData.size) {
        PAL_ERR(LOG_TAG, "VI device metadata is zero");
        ret = -ENOMEM;
        goto exit;
    }

    connectCtrlNameBeVI<< backEndNameTx << " metadata";
    beMetaDataMixerCtrl = mixer_get_ctl_by_name(virtMixer, connectCtrlNameBeVI.str().data());
    if (!beMetaDataMixerCtrl) {
        PAL_ERR(LOG_TAG, "invalid mixer control for VI : %s", backEndNameTx.c_str());
        ret = -EINVAL;
        goto exit;
    }

    if (deviceMetaData.size) {
        ret = mixer_ctl_set_array(beMetaDataMixerCtrl, (void *)deviceMetaData.buf,
                    deviceMetaData.size);
        free(deviceMetaData.buf);
        deviceMetaData.buf = nullptr;
    }
    else {
        PAL_ERR(LOG_TAG, "Device Metadata not set for TX path");
        ret = -EINVAL;
        goto exit;
    }

    ret = Device::setMediaConfig(rm, backEndNameTx, &device);
    if (ret) {
        PAL_ERR(LOG_TAG, "setMediaConfig for feedback device failed");
        goto exit;
    }

    sAttr.type = PAL_STREAM_LOW_LATENCY;
    sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
    dir = TX_HOSTLESS;
    pcmDevIdsTx = rm->allocateFrontEndIds(sAttr, dir);
    if (pcmDevIdsTx.size() == 0) {
        PAL_ERR(LOG_TAG, "allocateFrontEndIds failed");
        ret = -ENOSYS;
        goto exit;
    }

    connectCtrlName << "PCM" << pcmDevIdsTx.at(0) << " connect";
    connectCtrl = mixer_get_ctl_by_name(virtMixer, connectCtrlName.str().data());
    if (!connectCtrl) {
        PAL_ERR(LOG_TAG, "invalid mixer control: %s", connectCtrlName.str().data());
        goto free_fe;
    }
    ret = mixer_ctl_set_enum_by_string(connectCtrl, backEndNameTx.c_str());
    if (ret) {
        PAL_ERR(LOG_TAG, "Mixer control %s set with %s failed: %d",
        connectCtrlName.str().data(), backEndNameTx.c_str(), ret);
        goto free_fe;
    }

    isTxFeandBeConnected = true;

    config.rate = vi_device.samplerate;
    switch (vi_device.bit_width) {
        case 32 :
            config.format = PCM_FORMAT_S32_LE;
        break;
        case 24 :
            config.format = PCM_FORMAT_S24_LE;
        break;
        case 16:
            config.format = PCM_FORMAT_S16_LE;
        break;
        default:
            PAL_DBG(LOG_TAG, "Unsupported bit width. Set default as 16");
            config.format = PCM_FORMAT_S16_LE;
        break;
    }

    switch (vi_device.channels) {
        case 1 :
            config.channels = CHANNELS_1;
        break;
        case 2 :
            config.channels = CHANNELS_2;
        break;
        default:
            PAL_DBG(LOG_TAG, "Unsupported channel. Set default as 2");
            config.channels = CHANNELS_2;
        break;
    }
    config.period_size = DEFAULT_PERIOD_SIZE;
    config.period_count = DEFAULT_PERIOD_COUNT;
    config.start_threshold = 0;
    config.stop_threshold = INT_MAX;
    config.silence_threshold = 0;

    flags = PCM_IN;

    // Setting the mode of VI module
    modeConfg.num_speakers = numberOfChannels;
    modeConfg.th_operation_mode = CALIBRATION_MODE;
    if (minIdleTime > 0 && minIdleTime < MIN_SPKR_IDLE_SEC) {
        // Quick calibration set
        modeConfg.th_quick_calib_flag = 1;
    }
    else
        modeConfg.th_quick_calib_flag = 0;

    ret = SessionAlsaUtils::getModuleInstanceId(virtMixer, pcmDevIdsTx.at(0),
                                                backEndNameTx.c_str(),
                                                MODULE_VI, &miid);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_VI, ret);
        goto free_fe;
    }

    builder->payloadSPConfig(&payload, &payloadSize, miid,
            PARAM_ID_SP_VI_OP_MODE_CFG,(void *)&modeConfg);
    if (payloadSize) {
        ret = updateCustomPayload(payload, payloadSize);
        free(payload);
        if (0 != ret) {
            PAL_ERR(LOG_TAG,"updateCustomPayload Failed for VI_OP_MODE_CFG\n");
            // Fatal error as calibration mode will not be set
            goto free_fe;
        }
    }

    // Setting Channel Map configuration for VI module
    // TODO: Move this to ACDB
    viChannelMapConfg.num_ch = numberOfChannels * 2;
    payloadSize = 0;

    builder->payloadSPConfig(&payload, &payloadSize, miid,
            PARAM_ID_SP_VI_CHANNEL_MAP_CFG,(void *)&viChannelMapConfg);
    if (payloadSize) {
        ret = updateCustomPayload(payload, payloadSize);
        free(payload);
        if (0 != ret) {
            PAL_ERR(LOG_TAG," updateCustomPayload Failed for CHANNEL_MAP_CFG\n");
            // Not a fatal error
            ret = 0;
        }
    }
    // Setting Excursion mode
    viExModeConfg.ex_FTM_mode_enable_flag = 0; // Normal Mode
    payloadSize = 0;

    builder->payloadSPConfig(&payload, &payloadSize, miid,
            PARAM_ID_SP_EX_VI_MODE_CFG,(void *)&viExModeConfg);
    if (payloadSize) {
        ret = updateCustomPayload(payload, payloadSize);
        free(payload);
        if (0 != ret) {
            PAL_ERR(LOG_TAG," updateCustomPayload Failed for EX_VI_MODE_CFG\n");
            // Not a fatal error
            ret = 0;
        }
    }

    // Setting the values for VI module
    if (customPayloadSize) {
        ret = Device::setCustomPayload(rm, backEndNameTx,
                    customPayload, customPayloadSize);
        if (ret) {
            PAL_ERR(LOG_TAG, "Unable to set custom param for mode");
            goto free_fe;
        }
    }

    txPcm = pcm_open(rm->getVirtualSndCard(), pcmDevIdsTx.at(0), flags, &config);
    if (!txPcm) {
        PAL_ERR(LOG_TAG, "txPcm open failed");
        goto free_fe;
    }

    if (!pcm_is_ready(txPcm)) {
        PAL_ERR(LOG_TAG, "txPcm open not ready");
        goto err_pcm_open;
    }

    //Register for VI module callback
    PAL_DBG(LOG_TAG, "registering event for VI module");
    payload_size = sizeof(struct agm_event_reg_cfg);

    event_cfg.event_id = EVENT_ID_VI_PER_SPKR_CALIBRATION;
    event_cfg.event_config_payload_size = 0;
    event_cfg.is_register = 1;

    ret = SessionAlsaUtils::registerMixerEvent(virtMixer, pcmDevIdsTx.at(0),
                      backEndNameTx.c_str(), MODULE_VI, (void *)&event_cfg,
                      payload_size);
    if (ret) {
        PAL_ERR(LOG_TAG, "Unable to register event to DSP");
        // Fatal Error. Calibration Won't work
        goto err_pcm_open;
    }

    // Register to mixtureControlEvents and wait for the R0T0 values
    ret = rm->registerMixerEventCallback(pcmDevIdsTx, sessionCb, (uint64_t)this, true);
    if (ret != 0) {
        PAL_ERR(LOG_TAG, "Failed to register callback to rm");
        // Fatal Error. Calibration Won't work
        goto err_pcm_open;
    }

    enableDevice(audioRoute, mSndDeviceName_vi);

    PAL_DBG(LOG_TAG, "pcm start for TX path");
    if (pcm_start(txPcm) < 0) {
        PAL_ERR(LOG_TAG, "pcm start failed for TX path");
        ret = -ENOSYS;
        goto err_pcm_open;
    }
    isTxStarted = true;

    // Setup RX path
    deviceRx.id = PAL_DEVICE_OUT_SPEAKER;
    rm->getDeviceInfo(deviceRx.id, PAL_STREAM_PROXY, "", &deviceRxSpkr);
    strlcpy(mSndDeviceName_rx, deviceRxSpkr.sndDevName.c_str(), DEVICE_NAME_MAX_SIZE);
    rm->getChannelMap(&(deviceRx.config.ch_info.ch_map[0]), numberOfChannels);

    switch (numberOfChannels) {
        case 1 :
            deviceRx.config.ch_info.channels = CHANNELS_1;
        break;
        case 2 :
            deviceRx.config.ch_info.channels = CHANNELS_2;
        break;
        default:
            PAL_DBG(LOG_TAG, "Unsupported channel. Set default as 2");
            deviceRx.config.ch_info.channels = CHANNELS_2;
        break;
    }
    // TODO: Fetch these data from rm.xml
    deviceRx.config.sample_rate = SAMPLINGRATE_48K;
    deviceRx.config.bit_width = BITWIDTH_16;
    deviceRx.config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    rm->getBackendName(deviceRx.id, backEndNameRx);
    if (!strlen(backEndNameRx.c_str())) {
        PAL_ERR(LOG_TAG, "Failed to obtain tx backend name for %d", deviceRx.id);
        goto err_pcm_open;
    }

    keyVector.clear();
    calVector.clear();

    PayloadBuilder::getDeviceKV(deviceRx.id, keyVector);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to obtain device KV for %d", deviceRx.id);
        goto err_pcm_open;
    }

    // Enable the SP module
    switch (numberOfChannels) {
        case 1 :
            // TODO: Fetch the configuration from RM.xml
            calVector.push_back(std::make_pair(SPK_PRO_DEV_MAP, RIGHT_MONO));
        break;
        case 2 :
            calVector.push_back(std::make_pair(SPK_PRO_DEV_MAP, LEFT_RIGHT));
        break;
        default :
            PAL_ERR(LOG_TAG, "Unsupported channels for speaker");
            goto err_pcm_open;
    }

    SessionAlsaUtils::getAgmMetaData(keyVector, calVector,
                    (struct prop_data *)devicePropId, deviceMetaData);
    if (!deviceMetaData.size) {
        PAL_ERR(LOG_TAG, "device metadata is zero");
        ret = -ENOMEM;
        goto err_pcm_open;
    }

    connectCtrlNameBe<< backEndNameRx << " metadata";

    beMetaDataMixerCtrl = mixer_get_ctl_by_name(virtMixer, connectCtrlNameBe.str().data());
    if (!beMetaDataMixerCtrl) {
        PAL_ERR(LOG_TAG, "invalid mixer control: %s", backEndNameRx.c_str());
        ret = -EINVAL;
        goto err_pcm_open;
    }

    if (deviceMetaData.size) {
        ret = mixer_ctl_set_array(beMetaDataMixerCtrl, (void *)deviceMetaData.buf,
                                    deviceMetaData.size);
        free(deviceMetaData.buf);
        deviceMetaData.buf = nullptr;
    }
    else {
        PAL_ERR(LOG_TAG, "Device Metadata not set for RX path");
        ret = -EINVAL;
        goto err_pcm_open;
    }

    ret = Device::setMediaConfig(rm, backEndNameRx, &deviceRx);
    if (ret) {
        PAL_ERR(LOG_TAG, "setMediaConfig for speaker failed");
        goto err_pcm_open;
    }

    /* Retrieve Hostless PCM device id */
    sAttr.type = PAL_STREAM_LOW_LATENCY;
    sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
    dir = RX_HOSTLESS;
    pcmDevIdsRx = rm->allocateFrontEndIds(sAttr, dir);
    if (pcmDevIdsRx.size() == 0) {
        PAL_ERR(LOG_TAG, "allocateFrontEndIds failed");
        ret = -ENOSYS;
        goto err_pcm_open;
    }

    connectCtrlNameRx << "PCM" << pcmDevIdsRx.at(0) << " connect";
    connectCtrl = mixer_get_ctl_by_name(virtMixer, connectCtrlNameRx.str().data());
    if (!connectCtrl) {
        PAL_ERR(LOG_TAG, "invalid mixer control: %s", connectCtrlNameRx.str().data());
        ret = -ENOSYS;
        goto err_pcm_open;
    }
    ret = mixer_ctl_set_enum_by_string(connectCtrl, backEndNameRx.c_str());
    if (ret) {
        PAL_ERR(LOG_TAG, "Mixer control %s set with %s failed: %d",
        connectCtrlNameRx.str().data(), backEndNameRx.c_str(), ret);
        goto err_pcm_open;
    }
    isRxFeandBeConnected = true;

    config.rate = SAMPLINGRATE_48K;
    config.format = PCM_FORMAT_S16_LE;
    if (numberOfChannels > 1)
        config.channels = CHANNELS_2;
    else
        config.channels = CHANNELS_1;
    config.period_size = DEFAULT_PERIOD_SIZE;
    config.period_count = DEFAULT_PERIOD_COUNT;
    config.start_threshold = 0;
    config.stop_threshold = INT_MAX;
    config.silence_threshold = 0;

    flags = PCM_OUT;

    // Set the operation mode for SP module
    spModeConfg.operation_mode = CALIBRATION_MODE;
    ret = SessionAlsaUtils::getModuleInstanceId(virtMixer, pcmDevIdsRx.at(0),
                                                backEndNameRx.c_str(),
                                                MODULE_SP, &miid);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to get miid info %x, status = %d", MODULE_SP, ret);
        goto err_pcm_open;
    }

    payloadSize = 0;
    builder->payloadSPConfig(&payload, &payloadSize, miid,
            PARAM_ID_SP_OP_MODE,(void *)&spModeConfg);
    if (payloadSize) {
        if (customPayloadSize) {
            free(customPayload);
            customPayloadSize = 0;
            customPayload = NULL;
        }

        ret = updateCustomPayload(payload, payloadSize);
        free(payload);
        if (0 != ret) {
            PAL_ERR(LOG_TAG," updateCustomPayload Failed\n");
            // Fatal error as SP module will not run in Calibration mode
            goto err_pcm_open;
        }
    }

    // Setting the values for SP module
    if (customPayloadSize) {
        ret = Device::setCustomPayload(rm, backEndNameRx,
                    customPayload, customPayloadSize);
        if (ret) {
            PAL_ERR(LOG_TAG, "Unable to set custom param for SP mode");
            free(customPayload);
            customPayloadSize = 0;
            customPayload = NULL;
            goto err_pcm_open;
        }
    }

    rxPcm = pcm_open(rm->getVirtualSndCard(), pcmDevIdsRx.at(0), flags, &config);
    if (!rxPcm) {
        PAL_ERR(LOG_TAG, "pcm open failed for RX path");
        ret = -ENOSYS;
        goto err_pcm_open;
    }

    if (!pcm_is_ready(rxPcm)) {
        PAL_ERR(LOG_TAG, "pcm open not ready for RX path");
        ret = -ENOSYS;
        goto err_pcm_open;
    }


    enableDevice(audioRoute, mSndDeviceName_rx);
    PAL_DBG(LOG_TAG, "pcm start for RX path");
    if (pcm_start(rxPcm) < 0) {
        PAL_ERR(LOG_TAG, "pcm start failed for RX path");
        ret = -ENOSYS;
        goto err_pcm_open;
    }
    isRxStarted = true;

    spkrCalState = SPKR_CALIB_IN_PROGRESS;

    PAL_DBG(LOG_TAG, "Waiting for the event from DSP or PAL");

    // TODO: Make this to wait in While loop
    cv.wait(calLock);

    // Store the R0T0 values
    if (mDspCallbackRcvd) {
        if (calibrationCallbackStatus == CALIBRATION_STATUS_SUCCESS) {
            PAL_DBG(LOG_TAG, "Calibration is done");
            fp = fopen(PAL_SP_TEMP_PATH, "wb");
            if (!fp) {
                PAL_ERR(LOG_TAG, "Unable to open file for write");
            } else {
                PAL_DBG(LOG_TAG, "Write the R0T0 value to file");
                for (i = 0; i < numberOfChannels; i++) {
                    fwrite(&callback_data->cali_param[i].r0_cali_q24,
                                sizeof(callback_data->cali_param[i].r0_cali_q24), 1, fp);
                    fwrite(&spkerTempList[i], sizeof(int16_t), 1, fp);
                }
                spkrCalState = SPKR_CALIBRATED;
                free(callback_data);
                fclose(fp);
            }
        }
        else if (calibrationCallbackStatus == CALIBRATION_STATUS_FAILURE) {
            PAL_DBG(LOG_TAG, "Calibration is not done");
            spkrCalState = SPKR_NOT_CALIBRATED;
            // reset the timer for retry
            clock_gettime(CLOCK_BOOTTIME, &spkrLastTimeUsed);
        }
    }

err_pcm_open :
    if (txPcm) {
        event_cfg.is_register = 0;

        status = SessionAlsaUtils::registerMixerEvent(virtMixer, pcmDevIdsTx.at(0),
                        backEndNameTx.c_str(), MODULE_VI, (void *)&event_cfg,
                        payload_size);
        if (status) {
            PAL_ERR(LOG_TAG, "Unable to deregister event to DSP");
        }
        status = rm->registerMixerEventCallback (pcmDevIdsTx, sessionCb, (uint64_t)this, false);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to deregister callback to rm");
        }
        if (isTxStarted)
            pcm_stop(txPcm);

        pcm_close(txPcm);
        disableDevice(audioRoute, mSndDeviceName_vi);

        txPcm = NULL;
    }

    if (rxPcm) {
        if (isRxStarted)
            pcm_stop(rxPcm);
        pcm_close(rxPcm);
        disableDevice(audioRoute, mSndDeviceName_rx);
        rxPcm = NULL;
    }

free_fe:
    if (pcmDevIdsRx.size() != 0) {
        if (isRxFeandBeConnected) {
            disconnectFeandBe(pcmDevIdsRx, backEndNameRx);
        }
        rm->freeFrontEndIds(pcmDevIdsRx, sAttr, RX_HOSTLESS);
    }

    if (pcmDevIdsTx.size() != 0) {
        if (isTxFeandBeConnected) {
            disconnectFeandBe(pcmDevIdsTx, backEndNameTx);
        }
        rm->freeFrontEndIds(pcmDevIdsTx, sAttr, TX_HOSTLESS);
    }
    pcmDevIdsRx.clear();
    pcmDevIdsTx.clear();

exit:

    if (!mDspCallbackRcvd) {
        // the lock is unlocked due to processing mode. It will be waiting
        // for the unlock. So notify it.
        PAL_DBG(LOG_TAG, "Unlocked due to processing mode");
        spkrCalState = SPKR_NOT_CALIBRATED;
        clock_gettime(CLOCK_BOOTTIME, &spkrLastTimeUsed);
    }
    cv.notify_all();

    if (ret != 0) {
        // Error happened. Reset timer
        clock_gettime(CLOCK_BOOTTIME, &spkrLastTimeUsed);
    }

    if(builder) {
       delete builder;
       builder = NULL;
    }
    PAL_DBG(LOG_TAG, "Exiting");
    return ret;
}

int SpeakerProtectionwsa885x::viTxSetupThreadLoop()
{
    int ret = 0, dir = TX_HOSTLESS, flags, viParamId =0;
    std::shared_ptr<ResourceManager> rm;
    char mSndDeviceName_vi[128] = {0};
    char mSndDeviceName_SP[128] = {0};
    uint8_t* payload = NULL;
    uint32_t devicePropId[] = {0x08000010, 1, 0x2};
    uint32_t miid = 0;
    bool isTxFeandBeConnected = true;
    size_t payloadSize = 0;
    struct pal_device device;
    struct pal_channel_info ch_info;
    struct pal_stream_attributes sAttr;
    struct pcm_config config;
    struct mixer_ctl *connectCtrl = NULL;
    struct audio_route *audioRoute = NULL;
    struct vi_r0t0_cfg_t r0t0Array[numberOfChannels];
    struct agmMetaData deviceMetaData(nullptr, 0);
    struct mixer_ctl *beMetaDataMixerCtrl = nullptr;
    FILE *fp;
    std::string backEndName;
    std::vector <std::pair<int, int>> keyVector;
    std::vector <std::pair<int, int>> calVector;
    std::ostringstream connectCtrlNameBeVI;
    std::ostringstream connectCtrlName;
    param_id_sp_th_vi_r0t0_cfg_t *spR0T0confg;
    param_id_sp_vi_op_mode_cfg_t modeConfg;
    param_id_sp_vi_channel_map_cfg_t viChannelMapConfg;
    param_id_sp_ex_vi_mode_cfg_t viExModeConfg;
    PayloadBuilder* builder = new PayloadBuilder();
    struct pal_device rxDevAttr;
    struct agm_event_reg_cfg event_cfg;
    session_callback sessionCb;
    std::shared_ptr<Device> dev = nullptr;
    pal_spkr_prot_payload spkrProtPayload;

    PAL_DBG(LOG_TAG, "Enter: %s", __func__);
    rm = ResourceManager::getInstance();
    if (!rm) {
        PAL_ERR(LOG_TAG, "Failed to get resource manager instance");
        goto exit;
    }

    sessionCb = handleSPCallback;

    memset(&device, 0, sizeof(device));
    memset(&sAttr, 0, sizeof(sAttr));
    memset(&config, 0, sizeof(config));
    memset(&modeConfg, 0, sizeof(modeConfg));
    memset(&viChannelMapConfg, 0, sizeof(viChannelMapConfg));
    memset(&viExModeConfg, 0, sizeof(viExModeConfg));

    keyVector.clear();
    calVector.clear();

    dev = Device::getInstance(&mDeviceAttr, rm);
    dev->getCurrentSndDevName(mSndDeviceName_SP);

    if (mDeviceAttr.id == PAL_DEVICE_OUT_SPEAKER && strstr(mSndDeviceName_SP, "mono"))
        rm->getDeviceInfo(PAL_DEVICE_IN_VI_FEEDBACK, PAL_STREAM_VOICE_CALL, "", &vi_device);
    else if (mDeviceAttr.id == PAL_DEVICE_OUT_SPEAKER)
        rm->getDeviceInfo(PAL_DEVICE_IN_VI_FEEDBACK, PAL_STREAM_PROXY, "", &vi_device);

    strlcpy(mSndDeviceName_vi, vi_device.sndDevName.c_str(), DEVICE_NAME_MAX_SIZE);

    if (mDeviceAttr.id == PAL_DEVICE_OUT_HANDSET) {
        strlcat(mSndDeviceName_vi, FEEDBACK_MONO_1, DEVICE_NAME_MAX_SIZE);
    }
    PAL_DBG(LOG_TAG, "get the audio route %s", mSndDeviceName_vi);

    //Configure device attribute
    rm->getChannelMap(&(ch_info.ch_map[0]), vi_device.channels);
    ch_info.channels = vi_device.channels;

    switch(vi_device.channels) {
    case 1:
        ch_info.channels = CHANNELS_1;
        ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FR;
        config.channels = CHANNELS_1;
    break;
    case 2:
        ch_info.channels = CHANNELS_2;
        ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
        ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;
        config.channels = CHANNELS_2;
    break;
    default:
        PAL_DBG(LOG_TAG, "Unsupported channel. Set defauly as 2");
        ch_info.channels = CHANNELS_2;
        config.channels = CHANNELS_2;
    }

    if (mDeviceAttr.id == PAL_DEVICE_OUT_HANDSET)
        ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;


    this->Device::getDeviceAttributes(&rxDevAttr);
    vi_device.samplerate = rxDevAttr.config.sample_rate;
    device.config.ch_info = ch_info;
    device.config.sample_rate = vi_device.samplerate;
    device.config.bit_width = vi_device.bit_width;
    device.config.aud_fmt_id = rm->getAudioFmt(vi_device.bit_width);

    config.rate = vi_device.samplerate;
    config.period_size = DEFAULT_PERIOD_SIZE;
    config.period_count = DEFAULT_PERIOD_COUNT;
    config.start_threshold = 0;
    config.stop_threshold = INT_MAX;
    config.silence_threshold = 0;

    // Setup TX path
    device.id = PAL_DEVICE_IN_VI_FEEDBACK;

    ret = rm->getAudioRoute(&audioRoute);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to get the audio_route address status %d", ret);
        goto exit;
    }

    dev = Device::getInstance(&mDeviceAttr, rm);
    dev->getCurrentSndDevName(mSndDeviceName_SP);

    rm->getBackendName(device.id, backEndName);
    if (!strlen(backEndName.c_str())) {
        PAL_ERR(LOG_TAG, "Failed to obtain tx backend name for %d",
                device.id);
        goto exit;
    }

    PayloadBuilder::getDeviceKV(device.id, keyVector);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to obtain device KV for %d", device.id);
        goto exit;
    }

    // Enable the VI module
    switch (vi_device.channels) {
        case 1 :
            if (mDeviceAttr.id == PAL_DEVICE_OUT_HANDSET)
                calVector.push_back(std::make_pair(SPK_PRO_VI_MAP, LEFT_SPKR));
            else
                calVector.push_back(std::make_pair(SPK_PRO_VI_MAP, RIGHT_SPKR));
        break;
        case 2 :
            calVector.push_back(std::make_pair(SPK_PRO_VI_MAP, STEREO_SPKR));
        break;
        default :
            PAL_ERR(LOG_TAG, "Unsupported channel");
            goto exit;
    }

    SessionAlsaUtils::getAgmMetaData(keyVector, calVector,
            (struct prop_data *)devicePropId, deviceMetaData);
    if (!deviceMetaData.size) {
        PAL_ERR(LOG_TAG, "VI device metadata is zero");
        ret = -ENOMEM;
        goto exit;
    }

    connectCtrlNameBeVI<< backEndName << " metadata";
    beMetaDataMixerCtrl = mixer_get_ctl_by_name(virtMixer,
                                connectCtrlNameBeVI.str().data());
    if (!beMetaDataMixerCtrl) {
        PAL_ERR(LOG_TAG, "invalid mixer control for VI : %s",
                                            backEndName.c_str());
        ret = -EINVAL;
        goto exit;
    }

    if (!deviceMetaData.size) {
        PAL_ERR(LOG_TAG, "Device Metadata not set for TX path");
        ret = -EINVAL;
        goto exit;
    }

    ret = mixer_ctl_set_array(beMetaDataMixerCtrl, (void*)deviceMetaData.buf,
                deviceMetaData.size);
    free(deviceMetaData.buf);
    deviceMetaData.buf = nullptr;

    ret = Device::setMediaConfig(rm, backEndName, &device);
    if (ret) {
        PAL_ERR(LOG_TAG, "setMediaConfig for feedback device failed");
        goto exit;
    }

    /* Retrieve Hostless PCM device id */
    sAttr.type = PAL_STREAM_LOW_LATENCY;
    sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
    dir = TX_HOSTLESS;
    pcmDevIdTx = rm->allocateFrontEndIds(sAttr, dir);
    if (pcmDevIdTx.size() == 0) {
        PAL_ERR(LOG_TAG, "allocateFrontEndIds failed");
        ret = -ENOSYS;
        goto exit;
    }

    connectCtrlName << "PCM" << pcmDevIdTx.at(0) << " connect";
    connectCtrl = mixer_get_ctl_by_name(virtMixer, connectCtrlName.str().data());
    if (!connectCtrl) {
        PAL_ERR(LOG_TAG, "invalid mixer control: %s", connectCtrlName.str().data());
        goto free_fe;
    }

    ret = mixer_ctl_set_enum_by_string(connectCtrl, backEndName.c_str());
    if (ret) {
        PAL_ERR(LOG_TAG, "Mixer control %s set with %s failed: %d",
        connectCtrlName.str().data(), backEndName.c_str(), ret);
        goto free_fe;
    }

    isTxFeandBeConnected = true;

    switch (vi_device.bit_width) {
        case 32 :
            config.format = PCM_FORMAT_S32_LE;
        break;
        case 24 :
            config.format = PCM_FORMAT_S24_LE;
        break;
        case 16:
            config.format = PCM_FORMAT_S16_LE;
        break;
        default:
            PAL_DBG(LOG_TAG, "Unsupported bit width. Set default as 16");
            config.format = PCM_FORMAT_S16_LE;
        break;
    }

    flags = PCM_IN;

    //Setting the mode of VI module
    modeConfg.num_speakers = vi_device.channels;
    spkrProtPayload = rm->getSpkrProtModeValue();

    switch (spkrProtPayload.operationMode) {
        case PAL_SP_MODE_FACTORY_TEST:
            modeConfg.th_operation_mode = FACTORY_TEST_MODE;
        break;
        case PAL_SP_MODE_V_VALIDATION:
            modeConfg.th_operation_mode = V_VALIDATION_MODE;
        break;
        case PAL_SP_MODE_DYNAMIC_CAL:
        default:
            PAL_INFO(LOG_TAG, "Normal mode being used");
            modeConfg.th_operation_mode = NORMAL_MODE;
    }
    modeConfg.th_quick_calib_flag = 0;

    ret = SessionAlsaUtils::getModuleInstanceId(virtMixer, pcmDevIdTx.at(0),
                    backEndName.c_str(), MODULE_VI, &miid);
    if (ret != 0) {
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_VI,
                                                        ret);
        goto free_fe;
    }

    viCustomPayloadSize = 0;
    viCustomPayload = NULL;

    builder->payloadSPConfig(&payload, &payloadSize, miid,
                            PARAM_ID_SP_VI_OP_MODE_CFG, (void*)&modeConfg);
    if (payloadSize) {
        ret = updateVICustomPayload(payload, payloadSize);
        free(payload);
        if (ret != 0) {
            PAL_ERR(LOG_TAG," updateVICustomPayload Failed for VI_OP_MODE_CFG\n");
            // Not fatal as by default VI module runs in Normal mode
            ret = 0;
        }
    }

    // Setting Channel Map configuration for VI module
    // TODO: Move this to ACDB file
    viChannelMapConfg.num_ch = vi_device.channels * 2;
    payloadSize = 0;

    builder->payloadSPConfig(&payload, &payloadSize, miid,
            PARAM_ID_SP_VI_CHANNEL_MAP_CFG,(void *)&viChannelMapConfg);
    if (payloadSize) {
        ret = updateVICustomPayload(payload, payloadSize);
        free(payload);
        if (0 != ret) {
            PAL_ERR(LOG_TAG," updateVICustomPayload Failed for CHANNEL_MAP_CFG\n");
        }
    }

    // Setting Excursion mode
    if (spkrProtPayload.operationMode == PAL_SP_MODE_FACTORY_TEST)
        viExModeConfg.ex_FTM_mode_enable_flag = 1; // FTM Mode
    else
        viExModeConfg.ex_FTM_mode_enable_flag = 0; // Normal Mode
    payloadSize = 0;

    builder->payloadSPConfig(&payload, &payloadSize, miid,
            PARAM_ID_SP_EX_VI_MODE_CFG,(void *)&viExModeConfg);
    if (payloadSize) {
        ret = updateVICustomPayload(payload, payloadSize);
        free(payload);
        if (0 != ret) {
            PAL_ERR(LOG_TAG," updateVICustomPayload Failed for EX_VI_MODE_CFG\n");
            ret = 0;
        }
    }

    if (spkrProtPayload.operationMode) {
        PAL_DBG(LOG_TAG, "Operation mode %d", spkrProtPayload.operationMode);
        param_id_sp_th_vi_ftm_cfg_t viFtmConfg;
        viFtmConfg.num_ch = vi_device.channels;
        switch (spkrProtPayload.operationMode) {
            case PAL_SP_MODE_FACTORY_TEST:
                viParamId = PARAM_ID_SP_TH_VI_FTM_CFG;
                payloadSize = 0;
                builder->payloadSPConfig (&payload, &payloadSize, miid,
                        viParamId, (void *) &viFtmConfg);
                if (payloadSize) {
                    ret = updateVICustomPayload(payload, payloadSize);
                    free(payload);
                    if (0 != ret) {
                        PAL_ERR(LOG_TAG," Payload Failed for FTM mode\n");
                    }
                }
                viParamId = PARAM_ID_SP_EX_VI_FTM_CFG;
                payloadSize = 0;
                builder->payloadSPConfig (&payload, &payloadSize, miid,
                        viParamId, (void *) &viFtmConfg);
                if (payloadSize) {
                    ret = updateVICustomPayload(payload, payloadSize);
                    free(payload);
                    if (0 != ret) {
                        PAL_ERR(LOG_TAG," Payload Failed for FTM mode\n");
                    }
                }
            break;
            case PAL_SP_MODE_V_VALIDATION:
                viParamId = PARAM_ID_SP_TH_VI_V_VALI_CFG;
                payloadSize = 0;
                builder->payloadSPConfig (&payload, &payloadSize, miid,
                        viParamId, (void *) &viFtmConfg);
                if (payloadSize) {
                    ret = updateVICustomPayload(payload, payloadSize);
                    free(payload);
                    if (0 != ret) {
                        PAL_ERR(LOG_TAG," Payload Failed for FTM mode\n");
                    }
                }
            break;
            case PAL_SP_MODE_DYNAMIC_CAL:
                PAL_ERR(LOG_TAG, "Dynamic cal in Processing mode!!");
            break;
        }
    }

    // Setting the R0T0 values
    PAL_DBG(LOG_TAG, "Read R0T0 from file");
    fp = fopen(PAL_SP_TEMP_PATH, "rb");
    if (fp) {
        for (int i = 0; i < vi_device.channels; i++) {
            fread(&r0t0Array[i].r0_cali_q24,
                    sizeof(r0t0Array[i].r0_cali_q24), 1, fp);
            fread(&r0t0Array[i].t0_cali_q6,
                    sizeof(r0t0Array[i].t0_cali_q6), 1, fp);
        }
        fclose(fp);
    } else {
        PAL_DBG(LOG_TAG, "Speaker not calibrated. Send safe values");
        for (int i = 0; i < vi_device.channels; i++) {
            r0t0Array[i].r0_cali_q24 = MIN_RESISTANCE_SPKR_Q24;
            r0t0Array[i].t0_cali_q6 = SAFE_SPKR_TEMP_Q6;
        }
    }
    spR0T0confg = (param_id_sp_th_vi_r0t0_cfg_t*)calloc(1,
                        sizeof(param_id_sp_th_vi_r0t0_cfg_t) +
                        sizeof(vi_r0t0_cfg_t) * vi_device.channels);

    if (!spR0T0confg) {
        PAL_ERR(LOG_TAG," unable to create speaker config payload\n");
        goto free_fe;
    }
    spR0T0confg->num_ch = vi_device.channels;

    for (int i = 0; i < spR0T0confg->num_ch; i++) {
        spR0T0confg->r0t0_cfg[i].r0_cali_q24 = r0t0Array[i].r0_cali_q24;
        spR0T0confg->r0t0_cfg[i].t0_cali_q6 = r0t0Array[i].t0_cali_q6;
        PAL_DBG (LOG_TAG,"R0 %x ", spR0T0confg->r0t0_cfg[i].r0_cali_q24);
        PAL_DBG (LOG_TAG,"T0 %x ", spR0T0confg->r0t0_cfg[i].t0_cali_q6);

    }

    payloadSize = 0;
    builder->payloadSPConfig(&payload, &payloadSize, miid,
            PARAM_ID_SP_TH_VI_R0T0_CFG,(void *)spR0T0confg);
    if (payloadSize) {
        ret = updateVICustomPayload(payload, payloadSize);
        free(payload);
        free(spR0T0confg);
        if (0 != ret) {
            PAL_ERR(LOG_TAG," updateVICustomPayload Failed\n");
            ret = 0;
        }
    }

    // Setting the values for VI module
    if (customPayloadSize) {
        ret = Device::setCustomPayload(rm, backEndName,
                        viCustomPayload, viCustomPayloadSize);
        if (ret) {
            PAL_ERR(LOG_TAG, "Unable to set custom param for mode");
            goto free_fe;
        }
    }

    txPcm = pcm_open(rm->getVirtualSndCard(), pcmDevIdTx.at(0), flags, &config);
    if (!txPcm) {
        PAL_ERR(LOG_TAG, "vi-tx  pcm_open failed");
        goto free_fe;
    }

    if (!pcm_is_ready(txPcm)) {
        PAL_ERR(LOG_TAG, "txPcm open not ready");
        goto err_pcm_open;
    }

    PAL_DBG(LOG_TAG, "registering DC detection event for VI module");
    payloadSize = sizeof(struct agm_event_reg_cfg);

    /* Register for EVENT_ID_SPv5_SPEAKER_DIAGNOSTICS. */
    event_cfg.event_id = EVENT_ID_SPv5_SPEAKER_DIAGNOSTICS;
    event_cfg.event_config_payload_size = 0;
    event_cfg.is_register = 1;

    ret = SessionAlsaUtils::registerMixerEvent(virtMixer, pcmDevIdTx.at(0),
                backEndName.c_str(), MODULE_VI,
                (void *)&event_cfg, payloadSize);
    if (ret) {
        PAL_ERR(LOG_TAG, "Unable to register event to DSP");
    } else {
        ret = rm->registerMixerEventCallback(pcmDevIdTx, sessionCb, (uint64_t)this, true);
        if (ret != 0)
            PAL_ERR(LOG_TAG, "Failed to register callback to rm");
    }

    enableDevice(audioRoute, mSndDeviceName_vi);
    PAL_DBG(LOG_TAG, "pcm start for VI TX");
    if (pcm_start(txPcm) < 0) {
        PAL_ERR(LOG_TAG, "pcm start failed for TX path");
        goto deregister_cb;
    }

    goto exit;

deregister_cb:
    if (txPcm) {
        event_cfg.is_register = 0;

        ret = SessionAlsaUtils::registerMixerEvent(virtMixer, pcmDevIdTx.at(0),
                        backEndName.c_str(), MODULE_VI, (void *)&event_cfg,
                        payloadSize);
        if (ret) {
            PAL_ERR(LOG_TAG, "Unable to deregister event to DSP");
        }

        ret = rm->registerMixerEventCallback (pcmDevIdTx, sessionCb, (uint64_t)this, false);
        if (ret)
            PAL_ERR(LOG_TAG, "Failed to deregister callback to rm");
    }
err_pcm_open:
    if (pcmDevIdTx.size() != 0) {
        if (isTxFeandBeConnected) {
            disconnectFeandBe(pcmDevIdTx, backEndName);
        }
        rm->freeFrontEndIds(pcmDevIdTx, sAttr, dir);
        pcmDevIdTx.clear();
    }
    if (txPcm) {
        pcm_close(txPcm);
        disableDevice(audioRoute, mSndDeviceName_vi);
        txPcm = NULL;
    }
    goto exit;

free_fe:
    if (pcmDevIdTx.size() != 0) {
        if (isTxFeandBeConnected) {
            disconnectFeandBe(pcmDevIdTx, backEndName);
        }
        rm->freeFrontEndIds(pcmDevIdTx, sAttr, dir);
        pcmDevIdTx.clear();
    }

exit:
    if(builder) {
       delete builder;
       builder = NULL;
    }
    viTxSetupThrdCreated = false;

    if (viCustomPayload) {
        free(viCustomPayload);
        viCustomPayload = NULL;
        viCustomPayloadSize = 0;
    }

    return ret;
}

/*
 * Function to trigger Processing mode.
 * The parameter that it accepts are below:
 * true - Start Processing Mode
 * false - Stop Processing Mode
 */
int32_t SpeakerProtectionwsa885x::spkrProtProcessingMode(bool flag)
{
    int ret = 0, dir = TX_HOSTLESS, flags, viParamId = 0;
    char mSndDeviceName_vi[128] = {0};
    char mSndDeviceName_cps[128] = {0};
    char mSndDeviceName_SP[128] = {0};
    uint8_t* payload = NULL;
    uint32_t devicePropId[] = {0x08000010, 1, 0x2};
    uint32_t miid = 0;
    bool isTxFeandBeConnected = true;
    bool isCPSFeandBeConnected = true;
    bool foundSpkrStream = false;
    size_t payloadSize = 0;
    struct pal_device device, deviceCPS;
    struct pal_channel_info ch_info;
    struct pal_stream_attributes sAttr;
    struct pcm_config config;
    struct mixer_ctl *connectCtrl = NULL;
    struct mixer_ctl *connectCtrl2 = NULL;
    struct audio_route *audioRoute = NULL;
    struct vi_r0t0_cfg_t r0t0Array[numberOfChannels];
    struct agmMetaData deviceMetaData(nullptr, 0);
    struct mixer_ctl *beMetaDataMixerCtrl = nullptr;
    FILE *fp;
    std::string backEndName, backEndNameRx, backEndNameCPS;
    std::vector <std::pair<int, int>> keyVector;
    std::vector <std::pair<int, int>> calVector;
    std::shared_ptr<ResourceManager> rm;
    std::ostringstream connectCtrlNameBeCPS;
    std::ostringstream connectCtrlNameBeVI;
    std::ostringstream connectCtrlNameBeSP;
    std::ostringstream connectCtrlNameCPS;
    std::ostringstream connectCtrlName;
    param_id_sp_th_vi_r0t0_cfg_t *spR0T0confg;
    param_id_sp_vi_op_mode_cfg_t modeConfg;
    param_id_sp_vi_channel_map_cfg_t viChannelMapConfg;
    param_id_sp_op_mode_t spModeConfg;
    param_id_sp_ex_vi_mode_cfg_t viExModeConfg;
    param_id_cps_ch_map_t cpsChannelMapConfg;
    std::shared_ptr<Device> dev = nullptr;
    Stream *stream = NULL;
    Stream *activeSpkrStream = NULL;
    Session *session = NULL;
    std::vector<Stream*> activeStreams;
    std::vector <std::shared_ptr<Device>> devices;
    PayloadBuilder* builder = new PayloadBuilder();
    std::unique_lock<std::mutex> lock(calibrationMutex);
    struct agm_event_reg_cfg event_cfg;
    session_callback sessionCb;
    pal_spkr_prot_payload spkrProtPayload;

    PAL_DBG(LOG_TAG, "Flag %d", flag);
    deviceMutex.lock();

    sessionCb = handleSPCallback;

    if (flag) {
        if (spkrCalState == SPKR_CALIB_IN_PROGRESS) {
            // Close the Graphs
            cv.notify_all();
            // Wait for cleanup
            cv.wait(lock);
            spkrCalState = SPKR_NOT_CALIBRATED;
            txPcm = NULL;
            rxPcm = NULL;
            cpsPcm = NULL;
            PAL_DBG(LOG_TAG, "Stopped calibration mode");
        }
        numberOfRequest++;
        if (numberOfRequest > 1) {
            // R0T0 already set, we don't need to process the request
            goto exit;
        }
        PAL_DBG(LOG_TAG, "Custom payload size %zu, Payload %p", customPayloadSize,
                customPayload);

        if (customPayload) {
            free(customPayload);
        }
        customPayloadSize = 0;
        customPayload = NULL;

        spkrProtSetSpkrStatus(flag);
        // Speaker in use. Start the Processing Mode
        rm = ResourceManager::getInstance();
        if (!rm) {
            PAL_ERR(LOG_TAG, "Failed to get resource manager instance");
            goto exit;
        }

        /* Instantiate the viTxSetupThread
         * Move the complete vi tx setup path to that
         * and return back */
        if(!viTxSetupThrdCreated) {
            viTxSetupThread = std::thread(&SpeakerProtectionwsa885x::viTxSetupThreadLoop,
                    this);
            PAL_DBG(LOG_TAG, " Created vi tx thread :%s ", __func__);
            viTxSetupThrdCreated = true;
        }

        memset(&device, 0, sizeof(device));
        memset(&deviceCPS, 0, sizeof(device));
        memset(&sAttr, 0, sizeof(sAttr));
        memset(&config, 0, sizeof(config));
        memset(&modeConfg, 0, sizeof(modeConfg));
        memset(&viChannelMapConfg, 0, sizeof(viChannelMapConfg));
        memset(&viExModeConfg, 0, sizeof(viExModeConfg));
        memset(&spModeConfg, 0, sizeof(spModeConfg));

        keyVector.clear();
        calVector.clear();

        // Setting up SP mode
        rm->getBackendName(mDeviceAttr.id, backEndNameRx);
        if (!strlen(backEndNameRx.c_str())) {
            PAL_ERR(LOG_TAG, "Failed to obtain rx backend name for %d", mDeviceAttr.id);
            goto err_pcm_open;
        }

        ret = rm->getActiveStream_l(activeStreams, dev);
        if ((0 != ret) || (activeStreams.size() == 0)) {
            PAL_ERR(LOG_TAG, " no active stream available");
            ret = -EINVAL;
            goto err_pcm_open;
        }

        for (auto streamPtr : activeStreams) {
            if (!streamPtr)
                continue;

            devices.clear();
            streamPtr->getAssociatedDevices(devices);

            for (const auto &dev : devices) {
                if (dev && dev->getSndDeviceId() == PAL_DEVICE_OUT_SPEAKER) {
                    activeSpkrStream = streamPtr;
                    foundSpkrStream = true;
                    break;
                }
            }

            if (foundSpkrStream)
                break;
        }

        if (!activeStreams.empty() && activeSpkrStream) {
            stream = static_cast<Stream *>(activeSpkrStream);
            if (stream) {
                stream->getAssociatedSession(&session);
            } else {
                PAL_ERR(LOG_TAG, "Stream cast failed: nullptr after static_cast");
                ret = -EINVAL;
                goto err_pcm_open;
            }
        }
        else {
            PAL_ERR(LOG_TAG, "No active streams or null stream pointer");
            ret = -EINVAL;
            goto err_pcm_open;
        }


        ret = dynamic_cast<SessionAR*>(session)->getMIID(backEndNameRx.c_str(), MODULE_SP, &miid);
        if (ret) {
            PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_SP, ret);
            goto err_pcm_open;
        }

        // Set the operation mode for SP module
        PAL_DBG(LOG_TAG, "Operation mode for SP %d",
                        spkrProtPayload.operationMode);
        switch (spkrProtPayload.operationMode) {
            case PAL_SP_MODE_FACTORY_TEST:
                spModeConfg.operation_mode = FACTORY_TEST_MODE;
            break;
            case PAL_SP_MODE_V_VALIDATION:
                spModeConfg.operation_mode = V_VALIDATION_MODE;
            break;
            default:
                PAL_INFO(LOG_TAG, "Normal mode being used");
                spModeConfg.operation_mode = NORMAL_MODE;
        }

        payloadSize = 0;
        builder->payloadSPConfig(&payload, &payloadSize, miid,
                PARAM_ID_SP_OP_MODE,(void *)&spModeConfg);
        if (payloadSize) {
            if (customPayload) {
                free (customPayload);
                customPayloadSize = 0;
                customPayload = NULL;
            }
            ret = updateCustomPayload(payload, payloadSize);
            free(payload);
            if (0 != ret) {
                PAL_ERR(LOG_TAG," updateCustomPayload Failed\n");
            }
        }

        switch(rm->getCpsMode())
        {
            case 1:
                goto cps_dev_setup;
            case 2:
                // place holer for cps_mode 2
                [[fallthrough]];
            default:
                // Free up the local variables
                goto exit;
        }

cps_dev_setup:
        keyVector.clear();
        calVector.clear();

        dev = Device::getInstance(&mDeviceAttr, rm);
        dev->getCurrentSndDevName(mSndDeviceName_SP);

        if (mDeviceAttr.id == PAL_DEVICE_OUT_SPEAKER && strstr(mSndDeviceName_SP, "mono"))
            rm->getDeviceInfo(PAL_DEVICE_IN_CPS_FEEDBACK, PAL_STREAM_VOICE_CALL, "", &cps_device);
        else if (mDeviceAttr.id == PAL_DEVICE_OUT_SPEAKER)
            rm->getDeviceInfo(PAL_DEVICE_IN_CPS_FEEDBACK, PAL_STREAM_PROXY, "", &cps_device);

        // Configure device attribute
        if (cps_device.channels > 1) {
            ch_info.channels = CHANNELS_2;
            ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
            ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;
        }
        else {
            ch_info.channels = CHANNELS_1;
            if (mDeviceAttr.id == PAL_DEVICE_OUT_HANDSET)
                ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
            else
                ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FR;
        }

        deviceCPS.config.ch_info = ch_info;
        deviceCPS.config.sample_rate = cps_device.samplerate;
        deviceCPS.config.bit_width = cps_device.bit_width;
        deviceCPS.config.aud_fmt_id = rm->getAudioFmt(cps_device.bit_width);

        // Setup CPS path
        deviceCPS.id = PAL_DEVICE_IN_CPS_FEEDBACK;

        ret = rm->getAudioRoute(&audioRoute);
        if (0 != ret) {
            PAL_ERR(LOG_TAG, "Failed to get the audio_route address status %d", ret);
            goto err_pcm_open;
        }
        strlcpy(mSndDeviceName_cps, cps_device.sndDevName.c_str(), DEVICE_NAME_MAX_SIZE);

        if (mDeviceAttr.id == PAL_DEVICE_OUT_HANDSET) {
          strlcat(mSndDeviceName_cps, FEEDBACK_MONO_1, DEVICE_NAME_MAX_SIZE);
        }

        PAL_DBG(LOG_TAG, "get the audio route %s", mSndDeviceName_cps);

        rm->getBackendName(deviceCPS.id, backEndNameCPS);
        if (!strlen(backEndNameCPS.c_str())) {
            PAL_ERR(LOG_TAG, "Failed to obtain CPS backend name for %d", deviceCPS.id);
            goto err_pcm_open;
        }

        PayloadBuilder::getDeviceKV(deviceCPS.id, keyVector);
        if (0 != ret) {
            PAL_ERR(LOG_TAG, "Failed to obtain device KV for %d", device.id);
            goto err_pcm_open;
        }

        // Enable the CPS module
        switch (cps_device.channels) {
            case 1 :
                if (mDeviceAttr.id == PAL_DEVICE_OUT_HANDSET)
                    calVector.push_back(std::make_pair(SPK_PRO_CPS_MAP, L_SPKR));
                else
                    calVector.push_back(std::make_pair(SPK_PRO_CPS_MAP, R_SPKR));
            break;
            case 2 :
                calVector.push_back(std::make_pair(SPK_PRO_CPS_MAP, ST_SPKR));
            break;
            default :
                PAL_ERR(LOG_TAG, "Unsupported channel");
                goto err_pcm_open;
        }

        SessionAlsaUtils::getAgmMetaData(keyVector, calVector,
                (struct prop_data *)devicePropId, deviceMetaData);
        if (!deviceMetaData.size) {
            PAL_ERR(LOG_TAG, "CPS device metadata is zero");
            ret = -ENOMEM;
            goto err_pcm_open;
        }
        connectCtrlNameBeCPS<< backEndNameCPS << " metadata";
        beMetaDataMixerCtrl = mixer_get_ctl_by_name(virtMixer,
                                    connectCtrlNameBeCPS.str().data());
        if (!beMetaDataMixerCtrl) {
            PAL_ERR(LOG_TAG, "invalid mixer control for CPS : %s", backEndNameCPS.c_str());
            ret = -EINVAL;
            goto err_pcm_open;
        }

        if (deviceMetaData.size) {
            ret = mixer_ctl_set_array(beMetaDataMixerCtrl, (void *)deviceMetaData.buf,
                        deviceMetaData.size);
            free(deviceMetaData.buf);
            deviceMetaData.buf = nullptr;
        }
        else {
            PAL_ERR(LOG_TAG, "Device Metadata not set for CPS path");
            ret = -EINVAL;
            goto err_pcm_open;
        }

        ret = Device::setMediaConfig(rm, backEndNameCPS, &deviceCPS);
        if (ret) {
            PAL_ERR(LOG_TAG, "setMediaConfig for feedback device failed");
            goto err_pcm_open;
        }

        /* Retrieve Hostless PCM device id */
        sAttr.type = PAL_STREAM_LOW_LATENCY;
        sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
        dir = TX_HOSTLESS;
        pcmDevIdCPS = rm->allocateFrontEndIds(sAttr, dir);
        if (pcmDevIdCPS.size() == 0) {
            PAL_ERR(LOG_TAG, "allocateFrontEndIds failed");
            ret = -ENOSYS;
            goto err_pcm_open;
        }

        connectCtrlNameCPS << "PCM" << pcmDevIdCPS.at(0) << " connect";
        connectCtrl2 = mixer_get_ctl_by_name(virtMixer, connectCtrlNameCPS.str().data());

        if (!connectCtrl2) {
            PAL_ERR(LOG_TAG, "invalid mixer control: %s", connectCtrlNameCPS.str().data());
            goto free_fe;
        }

        ret = mixer_ctl_set_enum_by_string(connectCtrl2, backEndNameCPS.c_str());
        if (ret) {
            PAL_ERR(LOG_TAG, "Mixer control %s set with %s failed: %d",
            connectCtrlNameCPS.str().data(), backEndNameCPS.c_str(), ret);
            goto free_fe;
        }

        isCPSFeandBeConnected = true;

        config.rate = cps_device.samplerate;
        switch (cps_device.bit_width) {
            case 32 :
                config.format = PCM_FORMAT_S32_LE;
            break;
            default:
                PAL_DBG(LOG_TAG, "Unsupported bit width. Set default as 16");
                config.format = PCM_FORMAT_S16_LE;
            break;
        }

        switch (cps_device.channels) {
            case 1 :
                config.channels = CHANNELS_1;
            break;
            case 2 :
                config.channels = CHANNELS_2;
            break;
            default :
                PAL_DBG(LOG_TAG, "Unsupported channel. Set default as 2");
                config.channels = CHANNELS_2;
            break;
        }
        config.period_size = DEFAULT_PERIOD_SIZE;
        config.period_count = DEFAULT_PERIOD_COUNT;
        config.start_threshold = 0;
        config.stop_threshold = INT_MAX;
        config.silence_threshold = 0;

        flags = PCM_IN;

        ret = SessionAlsaUtils::getModuleInstanceId(virtMixer, pcmDevIdCPS.at(0),
                        backEndNameCPS.c_str(), TAG_MODULE_CPS, &miid);
        if (0 != ret) {
            PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", TAG_MODULE_CPS, ret);
            goto free_fe;
        }

        // Setting Channel Map configuration for CPS module
        // TODO: Move this to ACDB file
        cpsChannelMapConfg.num_ch = cps_device.channels;
        payloadSize = 0;

        builder->payloadSPConfig(&payload, &payloadSize, miid,
                PARAM_ID_CPS_CHANNEL_MAP,(void *)&cpsChannelMapConfg);
        if (payloadSize) {
            ret = updateCustomPayload(payload, payloadSize);
            free(payload);
            if (0 != ret) {
                PAL_ERR(LOG_TAG," updateCustomPayload Failed for CPS CHANNEL_MAP_CFG\n");
            }
        }

        cpsPcm = pcm_open(rm->getVirtualSndCard(), pcmDevIdCPS.at(0), flags, &config);
        if (!cpsPcm) {
            PAL_ERR(LOG_TAG, "cpsPcm open failed");
            goto free_fe;
        }

        if (!pcm_is_ready(cpsPcm)) {
            PAL_ERR(LOG_TAG, "cpsPcm open not ready");
            goto err_pcm_open;
        }

        enableDevice(audioRoute, mSndDeviceName_cps);
        PAL_DBG(LOG_TAG, "pcm start for CPS");
        if (pcm_start(cpsPcm) < 0) {
            PAL_ERR(LOG_TAG, "pcm start failed for CPS path");
            goto err_pcm_open;
        }

        // Free up the local variables
        goto exit;
    }
    else {
        if (numberOfRequest == 0) {
            PAL_ERR(LOG_TAG, "Device not started yet, Stop not expected");
            goto exit;
        }
        numberOfRequest--;
        if (numberOfRequest > 0) {
            // R0T0 already set, we don't need to process the request.
            goto exit;
        }
        spkrProtSetSpkrStatus(flag);
        // Speaker not in use anymore. Stop the processing mode
        PAL_DBG(LOG_TAG, "Closing VI path");
        /* if viTxSetupThread is joinable then wait for it to close
         * and then exit
         */

        if (viTxSetupThread.joinable()) {
            viTxSetupThread.join();
        }
        PAL_DBG(LOG_TAG, "vi tx setup thread joined");
        if (txPcm) {
            rm = ResourceManager::getInstance();
            device.id = PAL_DEVICE_IN_VI_FEEDBACK;

            ret = rm->getAudioRoute(&audioRoute);
            if (0 != ret) {
                PAL_ERR(LOG_TAG, "Failed to get the audio_route address status %d", ret);
                goto exit;
            }

            strlcpy(mSndDeviceName_vi, vi_device.sndDevName.c_str(), DEVICE_NAME_MAX_SIZE);
            rm->getBackendName(device.id, backEndName);
            if (!strlen(backEndName.c_str())) {
                PAL_ERR(LOG_TAG, "Failed to obtain tx backend name for %d", device.id);
                goto exit;
            }
            pcm_stop(txPcm);
            if (pcmDevIdTx.size() != 0) {
                if (isTxFeandBeConnected) {
                    disconnectFeandBe(pcmDevIdTx, backEndName);
                }
                sAttr.type = PAL_STREAM_LOW_LATENCY;
                sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
                rm->freeFrontEndIds(pcmDevIdTx, sAttr, dir);
                pcmDevIdTx.clear();
            }
            pcm_close(txPcm);
            disableDevice(audioRoute, mSndDeviceName_vi);
            txPcm = NULL;
        }
        PAL_DBG(LOG_TAG, "Closing CPS path");
        if (cpsPcm) {
            rm = ResourceManager::getInstance();
            deviceCPS.id = PAL_DEVICE_IN_CPS_FEEDBACK;

            ret = rm->getAudioRoute(&audioRoute);
            if (0 != ret) {
                PAL_ERR(LOG_TAG, "Failed to get the audio_route address status %d", ret);
                goto exit;
            }

            strlcpy(mSndDeviceName_cps, cps_device.sndDevName.c_str(), DEVICE_NAME_MAX_SIZE);
            rm->getBackendName(deviceCPS.id, backEndNameCPS);
            if (!strlen(backEndNameCPS.c_str())) {
                PAL_ERR(LOG_TAG, "Failed to obtain CPS backend name for %d", deviceCPS.id);
                goto exit;
            }
            pcm_stop(cpsPcm);
            if (pcmDevIdCPS.size() != 0) {
                if (isCPSFeandBeConnected) {
                    disconnectFeandBe(pcmDevIdCPS, backEndNameCPS);
                }
                sAttr.type = PAL_STREAM_LOW_LATENCY;
                sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
                rm->freeFrontEndIds(pcmDevIdCPS, sAttr, dir);
                pcmDevIdCPS.clear();
            }
            pcm_close(cpsPcm);
            disableDevice(audioRoute, mSndDeviceName_cps);
            cpsPcm = NULL;
            goto exit;
        }
    }

err_pcm_open :
    if (cpsPcm) {
        pcm_close(cpsPcm);
        disableDevice(audioRoute, mSndDeviceName_cps);
        cpsPcm = NULL;
    }

free_fe:
    if (pcmDevIdCPS.size() != 0) {
        if (isCPSFeandBeConnected) {
            disconnectFeandBe(pcmDevIdCPS, backEndNameCPS);
        }
        rm->freeFrontEndIds(pcmDevIdCPS, sAttr, dir);
        pcmDevIdCPS.clear();
    }
exit:
    deviceMutex.unlock();
    if(builder) {
       delete builder;
       builder = NULL;
    }
    return ret;
}
