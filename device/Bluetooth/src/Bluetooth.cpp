/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: Bluetooth"
#include "Bluetooth.h"
#include "ResourceManager.h"
#include "PayloadBuilder.h"
#include "Stream.h"
#include "Session.h"
#include "SessionAlsaUtils.h"
#include "Device.h"
#include "BTUtils.h"
#include "kvh2xml.h"
#include <dlfcn.h>
#include <unistd.h>
#include <cutils/properties.h>
#include <sstream>
#include <string>
#include <regex>

#define PARAM_ID_RESET_PLACEHOLDER_MODULE 0x08001173
#define BT_IPC_SOURCE_LIB                 "btaudio_offload_if.so"
#define BT_IPC_SINK_LIB                   "libbthost_if_sink.so"
#define MIXER_SET_FEEDBACK_CHANNEL        "BT set feedback channel"
#define MIXER_SET_CODEC_TYPE              "BT codec type"
#define BT_SLIMBUS_CLK_STR                "BT SLIMBUS CLK SRC"

extern "C" void CreateBtDevice(struct pal_device *device,
                                const std::shared_ptr<ResourceManager> rm,
                                std::shared_ptr<Device> *dev) {
    if (device != nullptr) {
        switch (device->id) {
            case PAL_DEVICE_IN_BLUETOOTH_A2DP:
            case PAL_DEVICE_OUT_BLUETOOTH_A2DP:
            case PAL_DEVICE_IN_BLUETOOTH_BLE:
            case PAL_DEVICE_OUT_BLUETOOTH_BLE:
            case PAL_DEVICE_OUT_BLUETOOTH_BLE_BROADCAST:
                *dev = BtA2dp::getInstance(device, rm);
                break;
            case PAL_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
            case PAL_DEVICE_OUT_BLUETOOTH_SCO:
            case PAL_DEVICE_IN_BLUETOOTH_HFP:
            case PAL_DEVICE_OUT_BLUETOOTH_HFP:
                *dev = BtSco::getInstance(device, rm);
                break;
            default:
                break;
        }
    } else {
        PAL_ERR(LOG_TAG, "Invalid input parameters");
    }
}

Bluetooth::Bluetooth(struct pal_device *device, std::shared_ptr<ResourceManager> Rm)
    : Device(device, Rm),
      mCodecFormat(CODEC_TYPE_INVALID),
      mCodecInfo(NULL),
      mIsAbrEnabled(false),
      mIsConfigured(false),
      mIsLC3MonoModeOn(false),
      mIsTwsMonoModeOn(false),
      mIsScramblingEnabled(false),
      mIsDummySink(false),
      mAbrRefCnt(0),
      mTotalActiveSessionRequests(0)
{
}

Bluetooth::~Bluetooth()
{
}

int Bluetooth::updateDeviceMetadata()
{
    int ret = 0;
    std::string backEndName;
    std::vector <std::pair<int, int>> keyVector;
    struct mixer_ctl *ctrl = NULL;

    if (rm->IsCPEnabled()) {
        ctrl = mixer_get_ctl_by_name(hwMixerHandle,
                                     MIXER_SET_CODEC_TYPE);
        if (!ctrl) {
            PAL_ERR(LOG_TAG, "ERROR %s mixer control not identified",
                    MIXER_SET_CODEC_TYPE);
            return ret;
        }

        ret = mixer_ctl_set_enum_by_string(ctrl, btCodecFormatLUT.at(mCodecFormat).c_str());
        if (ret) {
            PAL_ERR(LOG_TAG, "Mixer control %s set with %s failed: %d",
                    MIXER_SET_CODEC_TYPE, btCodecFormatLUT.at(mCodecFormat).c_str(), ret);
            return ret;
        }
    }
    if (mCodecVersion == APTX_R4_V2) {
        ret = PayloadBuilder::getBtDeviceKV(deviceAttr.id, keyVector, CODEC_TYPE_APTX_AD_R4_V2,
            mIsAbrEnabled, false, false);
    } else {
        ret = PayloadBuilder::getBtDeviceKV(deviceAttr.id, keyVector, mCodecFormat,
            mIsAbrEnabled, false, false);
    }

    if (ret)
        PAL_ERR(LOG_TAG, "No KVs found for device id %d codec format:0x%x",
            deviceAttr.id, mCodecFormat);

    rm->getBackendName(deviceAttr.id, backEndName);
    ret = SessionAlsaUtils::setDeviceMetadata(rm, backEndName, keyVector);
    return ret;
}

void Bluetooth::updateDeviceAttributes()
{
    std::string backEndName;
    bool isMI2SBackend;
    bool isBLEA2DPDevice;
    bool isI2SBLEA2DPPath,isStandardRate;
    deviceAttr.config.sample_rate = mCodecConfig.sample_rate;

    /* Sample rate calculation is done by kernel proxy driver in
     * case of XPAN. Send Encoder sample rate itself as part of
     * device attributes.
     *
     * For SCO devices, update proper sample rate. If there is
     * incoming stream over SCO, it will fetch proper device
     * attributes due to call to updateSampleRate. This will
     * cause unnecessary device switch if current device attributes
     * are not updated properly. Also device sample rate for Voice
     * usecase with APTX_AD_SPEECH and LC3_VOICE is hardcoded, so
     * it won't cause any issues.
     */
    if (rm->IsCPEnabled() && !rm->isBtScoDevice(deviceAttr.id))
        return;
    /* If all BT use cases (A2DP, BLE, SCO) route through MI2S.
     * Detect this via the backend name string. */
    rm->getBackendName(deviceAttr.id, backEndName);
    isMI2SBackend = (backEndName.find("MI2S") != std::string::npos);
    isBLEA2DPDevice = (deviceAttr.id == PAL_DEVICE_OUT_BLUETOOTH_BLE ||
                        deviceAttr.id == PAL_DEVICE_OUT_BLUETOOTH_BLE_BROADCAST ||
                        deviceAttr.id == PAL_DEVICE_OUT_BLUETOOTH_A2DP ||
                        deviceAttr.id == PAL_DEVICE_IN_BLUETOOTH_A2DP ||
                        deviceAttr.id == PAL_DEVICE_IN_BLUETOOTH_BLE);
    isI2SBLEA2DPPath = isMI2SBackend && isBLEA2DPDevice;
    /* [MI2S] MI2S backend accepts 44.1kHz and 48kHz natively.
     * Any other rate (e.g. 96kHz, 192kHz) is invalid on the I2S sink
     * and must be remapped — fallback to 44.1kHz in that case. */
    isStandardRate = (mCodecConfig.sample_rate == 44100 ||
                        mCodecConfig.sample_rate == 48000);

    switch (mCodecFormat) {
    case CODEC_TYPE_AAC:
    case CODEC_TYPE_SBC:
        if ((!isI2SBLEA2DPPath) && (mCodecType == DEC && isStandardRate)) {
            /* Non-MI2S path) */
            deviceAttr.config.sample_rate = mCodecConfig.sample_rate * 2;
        } else if (isI2SBLEA2DPPath && (!isStandardRate)) {
                deviceAttr.config.sample_rate = 44100;
        }
        break;
    case CODEC_TYPE_LDAC:
    case CODEC_TYPE_APTX_AD:
        if ((!isI2SBLEA2DPPath ) &&(mCodecType == ENC && isStandardRate)) {
            /* Non-MI2S path) */
            deviceAttr.config.sample_rate = mCodecConfig.sample_rate * 2;
        } else if (isI2SBLEA2DPPath && (!isStandardRate)) {
                deviceAttr.config.sample_rate = 44100;
        }
        break;
    case CODEC_TYPE_APTX_AD_SPEECH:
    case CODEC_TYPE_LC3:
            if (!isI2SBLEA2DPPath) {
                /* Non-MI2S path) */
                deviceAttr.config.sample_rate = SAMPLINGRATE_96K;
            } else if(!isStandardRate) {
                deviceAttr.config.sample_rate = 44100;
            }
            deviceAttr.config.aud_fmt_id = PAL_AUDIO_FMT_DEFAULT_COMPRESSED;
        break;
    case CODEC_TYPE_APTX_AD_QLEA:
        if (mCodecVersion == V1)
            deviceAttr.config.sample_rate = SAMPLINGRATE_96K;
        else
            deviceAttr.config.sample_rate = SAMPLINGRATE_192K;
        deviceAttr.config.aud_fmt_id = PAL_AUDIO_FMT_DEFAULT_COMPRESSED;
        break;
    default:
        break;
    }

    PAL_DBG(LOG_TAG, "%s: devid %d, sample_rate= %d, isMI2SBackend= %d, isBLEA2DPDevice = %d, ",__func__,deviceAttr.id,
            deviceAttr.config.sample_rate, isMI2SBackend, isBLEA2DPDevice);
}

bool Bluetooth::isPlaceholderEncoder()
{
    switch (mCodecFormat) {
        case CODEC_TYPE_LDAC:
        case CODEC_TYPE_APTX_AD:
        case CODEC_TYPE_APTX_AD_SPEECH:
        case CODEC_TYPE_LC3:
        case CODEC_TYPE_APTX_AD_QLEA:
        case CODEC_TYPE_APTX_AD_R4:
            return false;
        case CODEC_TYPE_AAC:
            return mIsAbrEnabled ? false : true;
        default:
            return true;
    }
}

int Bluetooth::getPluginPayload(void **libHandle, bt_codec_t **btCodec,
              bt_enc_payload_t **out_buf, codec_type codecType)
{
    std::string lib_path;
    open_fn_t plugin_open_fn = NULL;
    int status = 0;
    bt_codec_t *codec = NULL;
    void *handle = NULL;

    lib_path = getBtCodecLib(mCodecFormat, (codecType == ENC ? "enc" : "dec"));
    if (lib_path.empty()) {
        PAL_ERR(LOG_TAG, "fail to get BT codec library");
        return -ENOSYS;
    }

    handle = dlopen(lib_path.c_str(), RTLD_NOW);
    if (handle == NULL) {
        PAL_ERR(LOG_TAG, "failed to dlopen lib %s", lib_path.c_str());
        return -EINVAL;
    }

    dlerror();
    plugin_open_fn = (open_fn_t)dlsym(handle, "plugin_open");
    if (!plugin_open_fn) {
        PAL_ERR(LOG_TAG, "dlsym to open fn failed, err = '%s'", dlerror());
        status = -EINVAL;
        goto error;
    }

    status = plugin_open_fn(&codec, mCodecFormat, codecType);
    if (status) {
        PAL_ERR(LOG_TAG, "failed to open plugin %d", status);
        goto error;
    }

    status = codec->plugin_populate_payload(codec, mCodecInfo, (void **)out_buf);
    if (status != 0) {
        PAL_ERR(LOG_TAG, "fail to pack the encoder config %d", status);
        goto error;
    }
    *btCodec = codec;
    *libHandle = handle;
    goto done;

error:
    if (codec)
        codec->close_plugin(codec);

    if (handle)
        dlclose(handle);
done:
    return status;
}

int Bluetooth::checkAndUpdateCustomPayload(uint8_t **paramData, size_t *paramSize)
{
    int ret = -EINVAL;

    if (paramSize == 0)
        return ret;

    ret = updateCustomPayload(*paramData, *paramSize);
    free(*paramData);
    *paramData = NULL;
    *paramSize = 0;
    return 0;
}

int Bluetooth::configureCOPModule(int32_t pcmId, const char *backendName, uint32_t tagId, uint32_t streamMapDir, bool isFbPayload)
{
    PayloadBuilder* builder = new PayloadBuilder();
    uint8_t* paramData = NULL;
    size_t paramSize = 0;
    uint32_t miid = 0;
    int status = 0;

    if ((tagId == COP_PACKETIZER_V0) && rm->IsCPEnabled())
        return status;

    //if spatial audio headtracking is not enabled, return
    if ((tagId == MODULE_SA_HDT) && !rm->IsSAHDTEnabled())
        return status;

    status = SessionAlsaUtils::getModuleInstanceId(virtualMixerHandle,
                     pcmId, backendName, tagId, &miid);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
        if (tagId == MODULE_SA_HDT) {
            status = 0;
        }
        goto done;
    }

    switch(tagId) {
    case COP_DEPACKETIZER_V2:
    case COP_PACKETIZER_V2:
    case MODULE_SA_HDT:
        if (streamMapDir & STREAM_MAP_IN) {
            if (tagId == MODULE_SA_HDT) {
                builder->payloadHdtStreamInfo(&paramData, &paramSize,
                    miid, mCodecInfo, true /* StreamMapIn */);
            }
            else {
                builder->payloadCopV2StreamInfo(&paramData, &paramSize,
                    miid, mCodecInfo, true /* StreamMapIn */);
            }
            if (isFbPayload)
                status = mFBDev->checkAndUpdateCustomPayload(&paramData, &paramSize);
            else
                status = this->checkAndUpdateCustomPayload(&paramData, &paramSize);
            if (status) {
                PAL_ERR(LOG_TAG, "Invalid COPv2 module param size");
                goto done;
            }
        }
        if (streamMapDir & STREAM_MAP_OUT) {
            if (tagId == MODULE_SA_HDT) {
                builder->payloadHdtStreamInfo(&paramData, &paramSize,
                    miid, mCodecInfo, false /* StreamMapOut */);
            }
            else {
                builder->payloadCopV2StreamInfo(&paramData, &paramSize,
                    miid, mCodecInfo, false /* StreamMapOut */);
            }
            if (isFbPayload)
                status = mFBDev->checkAndUpdateCustomPayload(&paramData, &paramSize);
            else
                status = this->checkAndUpdateCustomPayload(&paramData, &paramSize);
            if (status) {
                PAL_ERR(LOG_TAG, "Invalid COPv2 module param size");
                goto done;
            }
        }
        if (tagId == COP_DEPACKETIZER_V2)
            break;
        [[fallthrough]];
    case COP_PACKETIZER_V0:
        if (rm->IsCPEnabled())
            break;

        // PARAM_ID_COP_PACKETIZER_OUTPUT_MEDIA_FORMAT
        if (isFbPayload) {
            builder->payloadCopPackConfig(&paramData, &paramSize, miid, &mFBDev->deviceAttr.config);
            status = mFBDev->checkAndUpdateCustomPayload(&paramData, &paramSize);
        } else {
            builder->payloadCopPackConfig(&paramData, &paramSize, miid, &deviceAttr.config);
            status = this->checkAndUpdateCustomPayload(&paramData, &paramSize);
        }
        if (status) {
            PAL_ERR(LOG_TAG, "Invalid COP module param size");
            goto done;
        }
        if (mIsScramblingEnabled) {
            builder->payloadScramblingConfig(&paramData, &paramSize, miid, mIsScramblingEnabled);
            if (isFbPayload)
                status = mFBDev->checkAndUpdateCustomPayload(&paramData, &paramSize);
            else
                status = this->checkAndUpdateCustomPayload(&paramData, &paramSize);
            if (status) {
                PAL_ERR(LOG_TAG, "Invalid COP module param size");
                goto done;
            }
        }
        break;
    }
done:
    if (builder) {
       delete builder;
       builder = NULL;
    }
    return status;
}

int Bluetooth::configureRATModule(int32_t pcmId, const char *backendName, uint32_t tagId, bool isFbPayload)
{
    uint32_t miid = 0;
    PayloadBuilder* builder = new PayloadBuilder();
    uint8_t* paramData = NULL;
    size_t paramSize = 0;
    int status = 0;

    status = SessionAlsaUtils::getModuleInstanceId(virtualMixerHandle,
                     pcmId, backendName, tagId, &miid);
    if (status) {
        PAL_INFO(LOG_TAG, "Failed to get tag info %x, status = %d", RAT_RENDER, status);
        status = 0;
        goto done;
    } else {
        if (isFbPayload) {
            builder->payloadRATConfig(&paramData, &paramSize, miid, &mFBDev->mCodecConfig);
            status = mFBDev->checkAndUpdateCustomPayload(&paramData, &paramSize);
        } else {
            builder->payloadRATConfig(&paramData, &paramSize, miid, &mCodecConfig);
            status = this->checkAndUpdateCustomPayload(&paramData, &paramSize);
        }
        if (status) {
            PAL_ERR(LOG_TAG, "Invalid RAT module param size");
            goto done;
        }
    }
done:
    if (builder) {
       delete builder;
       builder = NULL;
    }
    return status;
}

int Bluetooth::configurePCMConverterModule(int32_t pcmId, const char *backendName, uint32_t tagId, bool isFbPayload)
{
    uint32_t miid = 0;
    PayloadBuilder* builder = new PayloadBuilder();
    bool isRx;
    uint8_t* paramData = NULL;
    size_t paramSize = 0;
    int status = 0;

    if (isFbPayload) { /* For Feedback path, isRx will be true if normal path is DECODER */
        isRx = (mCodecType == DEC) ? true : false;
    } else {
        isRx = (mCodecType == ENC) ? true : false;
    }
    status = SessionAlsaUtils::getModuleInstanceId(virtualMixerHandle,
                     pcmId, backendName, tagId, &miid);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d",
                BT_PCM_CONVERTER, status);
        goto done;
    }

    if (isFbPayload) {
        builder->payloadPcmCnvConfig(&paramData, &paramSize, miid, &mFBDev->mCodecConfig, isRx);
        status = mFBDev->checkAndUpdateCustomPayload(&paramData, &paramSize);
    } else {
        builder->payloadPcmCnvConfig(&paramData, &paramSize, miid, &mCodecConfig, isRx);
        status = this->checkAndUpdateCustomPayload(&paramData, &paramSize);
    }
    if (status) {
        PAL_ERR(LOG_TAG, "Invalid PCM CNV module param size");
        goto done;
    }
done:
    if (builder) {
       delete builder;
       builder = NULL;
    }
    return status;
}

int32_t Bluetooth::getPCMId()
{
    Stream *stream = NULL;
    Session *session = NULL;
    std::shared_ptr<Device> dev = nullptr;
    std::vector<Stream*> activestreams;
    std::vector<int> pcmIds;
    int32_t status = -EINVAL;

    dev = Device::getInstance(&deviceAttr, rm);
    if (dev == nullptr) {
        PAL_ERR(LOG_TAG, "device_id[%d] Instance query failed", deviceAttr.id );
        goto done;
    }
    status = rm->getActiveStream_l(activestreams, dev);
    if ((0 != status) || (activestreams.size() == 0)) {
        PAL_ERR(LOG_TAG, "no active stream available");
        goto done;
    }
    stream = static_cast<Stream *>(activestreams[0]);
    stream->getAssociatedSession(&session);
    status = session->getFrontEndIds(pcmIds, (mCodecType == ENC) ? RX_HOSTLESS : TX_HOSTLESS);
done:
    return status == 0 ? pcmIds.at(0) : status;
}

int Bluetooth::configureGraphModules()
{
    int status = 0, i;
    int32_t pcmId;
    bt_enc_payload_t *out_buf = NULL;
    PayloadBuilder* builder = new PayloadBuilder();
    std::string backEndName;
    uint8_t* paramData = NULL;
    size_t paramSize = 0;
    uint32_t tagId = 0, streamMapDir = 0;
    uint32_t miid = 0;
    uint32_t num_payloads = 0;

    PAL_DBG(LOG_TAG, "Enter");
    PAL_INFO(LOG_TAG, "choose BT codec format %x", mCodecFormat);
    mIsConfigured = false;
    rm->getBackendName(deviceAttr.id, backEndName);
    pcmId = getPCMId();
    if (pcmId < 0) {
        PAL_ERR(LOG_TAG, "unable to get frontend ID");
        status = -EINVAL;
        goto error;
    }

    /* Retrieve plugin library from resource manager.
     * Map to interested symbols.
     */
    status = getPluginPayload(&mPluginHandler, &mPluginCodec, &out_buf, mCodecType);
    if (status) {
        PAL_ERR(LOG_TAG, "failed to payload from plugin");
        goto error;
    }

    mCodecConfig.sample_rate = out_buf->sample_rate;
    mCodecConfig.bit_width = out_buf->bit_format;
    mCodecConfig.ch_info.channels = out_buf->channel_count;

    mIsAbrEnabled = out_buf->is_abr_enabled;
    mCodecVersion = out_buf->codec_version;

    /* Reset device GKV for AAC ABR */
    if ((mCodecFormat == CODEC_TYPE_AAC) && mIsAbrEnabled)
        updateDeviceMetadata();

    /* Reset device GKV for R4 V2 codec based on codec version */
    if ((mCodecFormat == CODEC_TYPE_APTX_AD_R4) && (mCodecVersion == APTX_R4_V2))
        updateDeviceMetadata();


    /* Update Device sampleRate based on encoder config */
    updateDeviceAttributes();

    tagId = (mCodecType == ENC ? BT_PLACEHOLDER_ENCODER : BT_PLACEHOLDER_DECODER);
    status = SessionAlsaUtils::getModuleInstanceId(virtualMixerHandle,
                     pcmId, backEndName.c_str(), tagId, &miid);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
        goto error;
    }

    if (isPlaceholderEncoder()) {
        PAL_DBG(LOG_TAG, "Resetting placeholder module");
        builder->payloadCustomParam(&paramData, &paramSize, NULL, 0,
                                    miid, PARAM_ID_RESET_PLACEHOLDER_MODULE);
        status = checkAndUpdateCustomPayload(&paramData, &paramSize);
        if (status) {
            PAL_ERR(LOG_TAG, "Invalid reset placeholder param size");
            goto error;
        }
    }

    /* BT Encoder & Decoder Module Configuration */
    num_payloads = out_buf->num_blks;
    for (i = 0; i < num_payloads; i++) {
        custom_block_t *blk = out_buf->blocks[i];
        builder->payloadCustomParam(&paramData, &paramSize,
                  (uint32_t *)blk->payload, blk->payload_sz, miid, blk->param_id);
        status = checkAndUpdateCustomPayload(&paramData, &paramSize);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to populateAPMHeader");
            goto error;
        }
    }

    /* ---------------------------------------------------------------------------
     *       |        Encoder       | PSPD MFC/RAT/PCM CNV | COP Packetizer/HW EP
     * ---------------------------------------------------------------------------
     * SBC   | E_SR = SR of encoder | Same as encoder      | SR:E_SR BW:16 CH:1
     * ------|                      |----------------------|----------------------
     * AAC   | E_CH = CH of encoder | Same as encoder      | SR:E_SR BW:16 CH:1
     * ------|                      |----------------------|----------------------
     * LDAC  | E_BW = BW of encoder | Same as encoder      | if E_SR = 44.1/48KHz
     *       |                      |                      |   SR:E_SR*2 BW:16 CH:1
     *       |                      |                      | else
     *       |                      |                      |   SR:E_SR BW:16 CH:1
     * ------|                      |----------------------|----------------------
     * APTX  |                      | Same as encoder      | SR:E_SR BW:16 CH:1
     * ------|                      |----------------------|----------------------
     * APTX  |                      | Same as encoder      | SR:E_SR BW:16 CH:1
     * HD    |                      |                      |
     * ------|                      |----------------------|----------------------
     * APTX  |                      | Same as encoder      | if E_SR = 44.1/48KHz
     * AD    |                      |                      |   SR:E_SR*2 BW:16 CH:1
     *       |                      |                      | else
     *       |                      |                      |   SR:E_SR BW:16 CH:1
     * ------|----------------------|----------------------|----------------------
     * LC3   | E_SR = SR of encoder | Same as encoder      | SR:96KHz BW:16 CH:1
     *       | E_CH = CH of encoder |                      |
     *       | E_BW = 24            |                      |
     * ---------------------------------------------------------------------------
     * APTX      | E_SR = 32KHz     | Same as encoder      | SR:96KHz BW:16 CH:1
     * AD Speech | E_CH = 1         |                      |
     *           | E_BW = 16        |                      |
     * ---------------------------------------------------------------------------
     * LC3       | E_SR = SR of encoder | Same as encoder  | SR:96KHz BW:16 CH:1
     * Voice     | E_CH = CH of encoder |                  |
     *           | E_BW = 24            |                  |
     * --------------------------------------------------------------------------- */
    switch (mCodecFormat) {
    case CODEC_TYPE_APTX_AD_SPEECH:
        PAL_DBG(LOG_TAG, "Skip the rest, static configurations coming from ACDB");
        break;
    case CODEC_TYPE_LC3:
    case CODEC_TYPE_APTX_AD_QLEA:
    case CODEC_TYPE_APTX_AD_R4:
        builder->payloadLC3Config(&paramData, &paramSize, miid,
                                  mIsLC3MonoModeOn);
        status = checkAndUpdateCustomPayload(&paramData, &paramSize);
        if (status) {
            PAL_ERR(LOG_TAG, "Invalid LC3 param size");
            goto error;
        }
        status = configureRATModule(pcmId, backEndName.c_str(), RAT_RENDER, false);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to configure RAT module");
            goto error;
        }
        status = configurePCMConverterModule(pcmId, backEndName.c_str(), BT_PCM_CONVERTER, false);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to configure PCM Converter");
            goto error;
        }
        tagId = (mCodecType == DEC) ? COP_DEPACKETIZER_V2 : COP_PACKETIZER_V2;
        streamMapDir = (mCodecType == DEC) ? STREAM_MAP_IN | STREAM_MAP_OUT : STREAM_MAP_OUT;
        status = configureCOPModule(pcmId, backEndName.c_str(), tagId, streamMapDir, false);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to configure COP module 0x%x", tagId);
            goto error;
        }
        break;
    case CODEC_TYPE_APTX_DUAL_MONO:
    case CODEC_TYPE_APTX_AD:
        builder->payloadTWSConfig(&paramData, &paramSize, miid,
                                  mIsTwsMonoModeOn, mCodecFormat);
        status = checkAndUpdateCustomPayload(&paramData, &paramSize);
        if (status) {
            PAL_ERR(LOG_TAG, "Invalid TWS param size");
            goto error;
        }
        [[fallthrough]];
    default:
        switch(mCodecType) {
        case ENC:
            status = configureCOPModule(pcmId, backEndName.c_str(), COP_PACKETIZER_V0, 0/*don't care */, false);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to configure COP Packetizer");
                goto error;
            }
            status = configureRATModule(pcmId, backEndName.c_str(), RAT_RENDER, false);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to configure RAT module");
                goto error;
            }
            status = configurePCMConverterModule(pcmId, backEndName.c_str(), BT_PCM_CONVERTER, false);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to configure pcm Converter");
                goto error;
            }
            break;
        case DEC:
        default:
            break;
        }
    }
done:
    mIsConfigured = true;

error:
    if (builder) {
       delete builder;
       builder = NULL;
    }
    PAL_DBG(LOG_TAG, "Exit");
    return status;
}


int Bluetooth::configureNrecParameters(bool isNrecEnabled)
{
    int status = 0, i;
    int32_t pcmId;
    PayloadBuilder* builder = new PayloadBuilder();
    std::string backEndName;
    uint8_t* paramData = NULL;
    size_t paramSize = 0;
    uint32_t miid = 0;
    uint32_t num_payloads = 0;

    PAL_DBG(LOG_TAG, "Enter");
    if (!builder) {
        PAL_ERR(LOG_TAG, "Failed to new PayloadBuilder()");
        status = -ENOMEM;
        goto exit;
    }
    rm->getBackendName(deviceAttr.id, backEndName);
    pcmId = getPCMId();
    if (pcmId < 0) {
        PAL_ERR(LOG_TAG, "unable to get frontend ID");
        status = -EINVAL;
        goto exit;
    }

    status = SessionAlsaUtils::getModuleInstanceId(virtualMixerHandle,
                     pcmId, backEndName.c_str(), TAG_ECNS, &miid);
    if (!status) {
        PAL_DBG(LOG_TAG, "Setting NREC Configuration");
        builder->payloadNRECConfig(&paramData, &paramSize,
            miid, isNrecEnabled);
        status = checkAndUpdateCustomPayload(&paramData, &paramSize);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to payloadNRECConfig");
            goto exit;
        }
    } else {
        PAL_ERR(LOG_TAG, "Failed to find ECNS module info %x, status = %d"
            "cannot set NREC parameters",
            TAG_ECNS, status);
    }
exit:
    if (builder) {
       delete builder;
       builder = NULL;
    }
    PAL_DBG(LOG_TAG, "Exit");
    return status;
}

int Bluetooth::getCodecConfig(struct pal_media_config *config)
{
    if (!config) {
        PAL_ERR(LOG_TAG, "Invalid codec config");
        return -EINVAL;
    }

    if (mIsConfigured) {
        ar_mem_cpy(config, sizeof(struct pal_media_config),
                &mCodecConfig, sizeof(struct pal_media_config));
    }

    return 0;
}

void Bluetooth::startAbr()
{
    int ret = 0, dir;
    struct pal_device fbDevice;
    struct pal_channel_info ch_info;
    struct pal_stream_attributes sAttr;
    std::string backEndName;
    std::vector <std::pair<int, int>> keyVector;
    struct pcm_config config;
    struct mixer_ctl *connectCtrl = NULL;
    struct mixer_ctl *disconnectCtrl = NULL;
    struct mixer_ctl *btSetFeedbackChannelCtrl = NULL;
    std::ostringstream connectCtrlName;
    std::ostringstream disconnectCtrlName;
    std::string bEName;
    unsigned int flags;
    uint32_t tagId = 0, miid = 0, streamMapDir = 0;
    void *pluginLibHandle = NULL;
    bt_codec_t *codec = NULL;
    bt_enc_payload_t *out_buf = NULL;
    custom_block_t *blk = NULL;
    uint8_t* paramData = NULL;
    size_t paramSize = 0;
    PayloadBuilder* builder = NULL;
    bool isDeviceLocked = false;
    audio_lc3_codec_cfg_t* bt_ble_codec = NULL;
    bool isHWSpatializerEnabled = false;
    bool isHDTEnabled = rm->IsSAHDTEnabled();

    memset(&fbDevice, 0, sizeof(fbDevice));
    memset(&sAttr, 0, sizeof(sAttr));
    memset(&config, 0, sizeof(config));

    mAbrMutex.lock();
    if (mAbrRefCnt > 0) {
        mAbrRefCnt++;
        mAbrMutex.unlock();
        return;
    }
    /* Configure device attributes */
    rm->getBackendName(deviceAttr.id, bEName);
    bool isMI2SBackend = (bEName.find("MI2S") != std::string::npos);
    bool isBLEA2DPDevice   = (deviceAttr.id == PAL_DEVICE_OUT_BLUETOOTH_BLE ||
                        deviceAttr.id == PAL_DEVICE_OUT_BLUETOOTH_BLE_BROADCAST ||
                        deviceAttr.id == PAL_DEVICE_OUT_BLUETOOTH_A2DP ||
                        deviceAttr.id == PAL_DEVICE_IN_BLUETOOTH_A2DP ||
                        deviceAttr.id == PAL_DEVICE_IN_BLUETOOTH_BLE);
    bool isI2SBLEA2DPPath = isMI2SBackend && isBLEA2DPDevice;
    bool isStandardRate = (deviceAttr.config.sample_rate == 44100 ||
                        deviceAttr.config.sample_rate == 48000);
    PAL_DBG(LOG_TAG, "%s: isMI2SBackend= %d isBLEA2DPDevice = %d",__func__,isMI2SBackend, isBLEA2DPDevice);
    if (isI2SBLEA2DPPath) {
        ch_info.channels = CHANNELS_2;
        ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
        ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;
    } else {
        ch_info.channels = CHANNELS_1;
        ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
    }
    fbDevice.config.ch_info = ch_info;
    fbDevice.config.bit_width = BITWIDTH_16;
    fbDevice.config.aud_fmt_id = PAL_AUDIO_FMT_DEFAULT_COMPRESSED;

    if ((mCodecFormat == CODEC_TYPE_APTX_AD_SPEECH) ||
            (mCodecFormat == CODEC_TYPE_LC3) ||
            (mCodecFormat == CODEC_TYPE_APTX_AD_QLEA) ||
            (mCodecFormat == CODEC_TYPE_APTX_AD_R4)) {
        fbDevice.config.sample_rate = deviceAttr.config.sample_rate;
    } else if (isI2SBLEA2DPPath && ((mCodecFormat == CODEC_TYPE_APTX_AD) ||
            (mCodecFormat == CODEC_TYPE_LDAC))) {
        /* APTX_AD and LDAC over the MI2S backend do not support
         * sample rates used on CP paths. Fix feedback device
         * rate to 44.1kHz — the safe standard rate for the MI2S sink. */
        if (!isStandardRate) {
            fbDevice.config.sample_rate = 44100;
        } else {
            fbDevice.config.sample_rate = deviceAttr.config.sample_rate;
        }
        PAL_DBG(LOG_TAG, "%s: devid %d, sample_rate= %d, ",__func__,deviceAttr.id,
                fbDevice.config.sample_rate);
    } else {
        fbDevice.config.sample_rate = SAMPLINGRATE_8K;
    }

    if (mCodecType == DEC) { /* Usecase is TX, feedback device will be RX */
        if (deviceAttr.id == PAL_DEVICE_IN_BLUETOOTH_A2DP) {
            fbDevice.id = PAL_DEVICE_OUT_BLUETOOTH_A2DP;
        } else if (deviceAttr.id == PAL_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
            fbDevice.id = PAL_DEVICE_OUT_BLUETOOTH_SCO;
        } else if (deviceAttr.id == PAL_DEVICE_IN_BLUETOOTH_BLE) {
            fbDevice.id = PAL_DEVICE_OUT_BLUETOOTH_BLE;
        }
        dir = RX_HOSTLESS;
        flags = PCM_OUT;
    } else {
        if (deviceAttr.id == PAL_DEVICE_OUT_BLUETOOTH_A2DP) {
            fbDevice.id = PAL_DEVICE_IN_BLUETOOTH_A2DP;
        } else if (deviceAttr.id == PAL_DEVICE_OUT_BLUETOOTH_SCO) {
            fbDevice.id = PAL_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
        } else if (deviceAttr.id == PAL_DEVICE_OUT_BLUETOOTH_BLE ||
                   deviceAttr.id == PAL_DEVICE_OUT_BLUETOOTH_BLE_BROADCAST) {
            fbDevice.id = PAL_DEVICE_IN_BLUETOOTH_BLE;
        }
        dir = TX_HOSTLESS;
        flags = PCM_IN;
    }

    if ((fbDevice.id == PAL_DEVICE_OUT_BLUETOOTH_SCO) ||
        (fbDevice.id == PAL_DEVICE_IN_BLUETOOTH_SCO_HEADSET))
        mFBDev = std::dynamic_pointer_cast<BtSco>(BtSco::getInstance(&fbDevice, rm));
    else
        mFBDev = std::dynamic_pointer_cast<BtA2dp>(BtA2dp::getInstance(&fbDevice, rm));

    if (!mFBDev) {
        PAL_ERR(LOG_TAG, "failed to get Bt device object for %d", fbDevice.id);
        goto done;
    }

    bt_ble_codec = (audio_lc3_codec_cfg_t *)mCodecInfo;
    if (bt_ble_codec) {
        if (fbDevice.id == PAL_DEVICE_IN_BLUETOOTH_BLE && mCodecFormat == CODEC_TYPE_LC3 && isHDTEnabled) {
            if (bt_ble_codec->streaming_DSA_HW) {
                PAL_DBG(LOG_TAG, "setting tx enabled true for ABR GKV");
                isHWSpatializerEnabled = true;
            }
        }
    }

    builder = new PayloadBuilder();

    if (mCodecVersion == APTX_R4_V2)
        ret = PayloadBuilder::getBtDeviceKV(fbDevice.id, keyVector, CODEC_TYPE_APTX_AD_R4_V2,
              true, true, isHWSpatializerEnabled);
    else
        ret = PayloadBuilder::getBtDeviceKV(fbDevice.id, keyVector, mCodecFormat,
              true, true, isHWSpatializerEnabled);
    if (ret)
        PAL_ERR(LOG_TAG, "No KVs found for device id %d codec format:0x%x",
            fbDevice.id, mCodecFormat);

    /* Configure Device Metadata */
    rm->getBackendName(fbDevice.id, backEndName);
    ret = SessionAlsaUtils::setDeviceMetadata(rm, backEndName, keyVector);
    if (ret) {
        PAL_ERR(LOG_TAG, "setDeviceMetadata for feedback device failed");
        goto done;
    }
    ret = Device::setMediaConfig(rm, backEndName, &fbDevice);
    if (ret) {
        PAL_ERR(LOG_TAG, "setMediaConfig for feedback device failed");
        goto done;
    }

    /* Retrieve Hostless PCM device id */
    sAttr.type = PAL_STREAM_LOW_LATENCY;
    sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
    mFBPcmDevIds = rm->allocateFrontEndIds(sAttr, dir);
    if (mFBPcmDevIds.size() == 0) {
        PAL_ERR(LOG_TAG, "allocateFrontEndIds failed");
        ret = -ENOSYS;
        goto done;
    }

    connectCtrlName << "PCM" << mFBPcmDevIds.at(0) << " connect";
    connectCtrl = mixer_get_ctl_by_name(virtualMixerHandle, connectCtrlName.str().data());
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

    if (!(isBLEA2DPDevice && isMI2SBackend)) {
        // Notify ABR usecase information to BT driver to distinguish
        // between SCO and feedback usecase
        btSetFeedbackChannelCtrl = mixer_get_ctl_by_name(hwMixerHandle,
                                            MIXER_SET_FEEDBACK_CHANNEL);
        if (!btSetFeedbackChannelCtrl) {
            PAL_ERR(LOG_TAG, "ERROR %s mixer control not identified",
                    MIXER_SET_FEEDBACK_CHANNEL);
            goto disconnect_fe;
        }

        if (mixer_ctl_set_value(btSetFeedbackChannelCtrl, 0, 1) != 0) {
            PAL_ERR(LOG_TAG, "Failed to set BT usecase");
            goto disconnect_fe;
        }
    }
    mFBDev->lockDeviceMutex();
    isDeviceLocked = true;

    if (mFBDev->mIsConfigured == true) {
        PAL_INFO(LOG_TAG, "feedback path is already configured");
        goto start_pcm;
    }

    /* update device attributes to reflect proper device configuration */
    mFBDev->deviceAttr.config = fbDevice.config;

    switch (fbDevice.id) {
    case PAL_DEVICE_OUT_BLUETOOTH_SCO:
    case PAL_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
    case PAL_DEVICE_OUT_BLUETOOTH_BLE:
        tagId = (mCodecType == DEC ? BT_PLACEHOLDER_ENCODER : BT_PLACEHOLDER_DECODER);
        ret = SessionAlsaUtils::getModuleInstanceId(virtualMixerHandle,
                     mFBPcmDevIds.at(0), backEndName.c_str(), tagId, &miid);
        if (ret) {
            PAL_ERR(LOG_TAG, "getMiid for feedback device failed");
            goto disconnect_fe;
        }

        ret = getPluginPayload(&pluginLibHandle, &codec, &out_buf, (mCodecType == DEC ? ENC : DEC));
        if (ret) {
            PAL_ERR(LOG_TAG, "getPluginPayload failed");
            goto disconnect_fe;
        }

        /* SWB Encoder/Decoder has only 1 param, read block 0 */
        if (out_buf->num_blks != 1) {
            PAL_ERR(LOG_TAG, "incorrect block size %d", out_buf->num_blks);
            goto disconnect_fe;
        }
        mFBDev->mCodecConfig.sample_rate = out_buf->sample_rate;
        mFBDev->mCodecConfig.bit_width = out_buf->bit_format;
        mFBDev->mCodecConfig.ch_info.channels = out_buf->channel_count;
        mFBDev->mIsAbrEnabled = out_buf->is_abr_enabled;

        blk = out_buf->blocks[0];
        builder->payloadCustomParam(&paramData, &paramSize,
                  (uint32_t *)blk->payload, blk->payload_sz, miid, blk->param_id);

        codec->close_plugin(codec);
        dlclose(pluginLibHandle);

        if (!paramData) {
            PAL_ERR(LOG_TAG, "Failed to populateAPMHeader");
            ret = -ENOMEM;
            goto disconnect_fe;
        }
        ret = mFBDev->checkAndUpdateCustomPayload(&paramData, &paramSize);
        if (ret) {
            PAL_ERR(LOG_TAG, "Invalid COPv2 module param size");
            goto done;
        }
        switch (mCodecFormat) {
        case CODEC_TYPE_LC3:
        case CODEC_TYPE_APTX_AD_QLEA:
        case CODEC_TYPE_APTX_AD_R4:
            tagId = (flags == PCM_IN) ? COP_DEPACKETIZER_V2 : COP_PACKETIZER_V2;
            streamMapDir = (flags == PCM_IN) ? STREAM_MAP_IN | STREAM_MAP_OUT : STREAM_MAP_OUT;
            ret = configureCOPModule(mFBPcmDevIds.at(0), backEndName.c_str(), tagId, streamMapDir, true);
            if (ret) {
                PAL_ERR(LOG_TAG, "Failed to configure COP module");
                goto disconnect_fe;
            }
            ret = configureRATModule(mFBPcmDevIds.at(0), backEndName.c_str(), RAT_RENDER, true);
            if (ret) {
                PAL_ERR(LOG_TAG, "Failed to configure RAT module");
                goto disconnect_fe;
            }
            ret = configurePCMConverterModule(mFBPcmDevIds.at(0), backEndName.c_str(), BT_PCM_CONVERTER, true);
            if (ret) {
                PAL_ERR(LOG_TAG, "Failed to configure PCM Converter");
                goto disconnect_fe;
            }
            break;
        default:
            break;
        }
        break;
    case PAL_DEVICE_IN_BLUETOOTH_A2DP:
    case PAL_DEVICE_IN_BLUETOOTH_BLE:
        switch (mCodecFormat) {
        case CODEC_TYPE_LC3:
        case CODEC_TYPE_APTX_AD_QLEA:
        case CODEC_TYPE_APTX_AD_R4:
             if ((mCodecFormat == CODEC_TYPE_LC3 && isHDTEnabled && isHWSpatializerEnabled) ||
                  ((mCodecFormat == CODEC_TYPE_APTX_AD_R4) && mCodecVersion == APTX_R4_V2))
                ret = configureCOPModule(mFBPcmDevIds.at(0), backEndName.c_str(), COP_DEPACKETIZER_V2, STREAM_MAP_IN | STREAM_MAP_OUT, true);
            else
                ret = configureCOPModule(mFBPcmDevIds.at(0), backEndName.c_str(), COP_DEPACKETIZER_V2, STREAM_MAP_OUT, true);
            if (ret) {
                PAL_ERR(LOG_TAG, "Failed to configure 0x%x", COP_DEPACKETIZER_V2);
                goto disconnect_fe;
            }
            if (mCodecFormat == CODEC_TYPE_LC3) {
                ret = configureCOPModule(mFBPcmDevIds.at(0), backEndName.c_str(), MODULE_SA_HDT, STREAM_MAP_IN, true);
                if (ret) {
                    PAL_ERR(LOG_TAG, "Failed to configure 0x%x", MODULE_SA_HDT);
                    goto disconnect_fe;
                }
            }
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    if (mFBDev->customPayloadSize) {
        ret = Device::setCustomPayload(rm, backEndName,
                                    mFBDev->customPayload, mFBDev->customPayloadSize);
        if (ret) {
            PAL_ERR(LOG_TAG, "Error: Dev setParam failed for %d", fbDevice.id);
            goto disconnect_fe;
        }
        free(mFBDev->customPayload);
        mFBDev->customPayload = NULL;
        mFBDev->customPayloadSize = 0;
    }
start_pcm:
    config.rate = SAMPLINGRATE_8K;
    config.format = PCM_FORMAT_S16_LE;
    config.channels = CHANNELS_1;
    config.period_size = 240;
    config.period_count = 2;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;
    mFBPcm = pcm_open(rm->getVirtualSndCard(), mFBPcmDevIds.at(0), flags, &config);
    if (!mFBPcm) {
        PAL_ERR(LOG_TAG, "pcm open failed");
        goto disconnect_fe;
    }

    if (!pcm_is_ready(mFBPcm)) {
        PAL_ERR(LOG_TAG, "pcm open not ready");
        goto err_pcm_open;
    }

    ret = pcm_start(mFBPcm);
    if (ret) {
        PAL_ERR(LOG_TAG, "pcm_start rx failed %d", ret);
        goto err_pcm_open;
    }

    if ((mCodecFormat == CODEC_TYPE_APTX_AD_SPEECH) ||
        ((mCodecFormat == CODEC_TYPE_LC3) &&
         (fbDevice.id == PAL_DEVICE_IN_BLUETOOTH_SCO_HEADSET ||
          fbDevice.id == PAL_DEVICE_OUT_BLUETOOTH_SCO ||
          fbDevice.id == PAL_DEVICE_OUT_BLUETOOTH_BLE)) ||
        ((mCodecFormat == CODEC_TYPE_APTX_AD_R4) &&
         (fbDevice.id == PAL_DEVICE_OUT_BLUETOOTH_BLE))) {
        mFBDev->mIsConfigured = true;
        mFBDev->deviceStartStopCount++;
        mFBDev->deviceCount++;
        PAL_DBG(LOG_TAG, " deviceCount %d deviceStartStopCount %d for device id %d",
                mFBDev->deviceCount, mFBDev->deviceStartStopCount, mFBDev->deviceAttr.id);
    }

    mAbrRefCnt++;
    PAL_INFO(LOG_TAG, "Feedback Device started successfully");
    goto done;
err_pcm_open:
    pcm_close(mFBPcm);
    mFBPcm = NULL;
disconnect_fe:
    disconnectCtrlName << "PCM" << mFBPcmDevIds.at(0) << " disconnect";
    disconnectCtrl = mixer_get_ctl_by_name(virtualMixerHandle, disconnectCtrlName.str().data());
    if(disconnectCtrl != NULL){
       mixer_ctl_set_enum_by_string(disconnectCtrl, backEndName.c_str());
    }
free_fe:
    rm->freeFrontEndIds(mFBPcmDevIds, sAttr, dir);
    mFBPcmDevIds.clear();
done:
    if (isDeviceLocked) {
        isDeviceLocked = false;
        mFBDev->unlockDeviceMutex();
    }
    mAbrMutex.unlock();
    if (builder) {
       delete builder;
       builder = NULL;
    }
    return;
}

void Bluetooth::stopAbr()
{
    struct pal_stream_attributes sAttr;
    struct mixer_ctl *btSetFeedbackChannelCtrl = NULL;
    int dir, ret = 0;
    bool isfbDeviceLocked = false;

    mAbrMutex.lock();
    if (!mFBPcm) {
        PAL_ERR(LOG_TAG, "mFBPcm is null");
        mAbrMutex.unlock();
        return;
    }

    if (mAbrRefCnt == 0) {
        PAL_DBG(LOG_TAG, "skip as mAbrRefCnt is zero");
        mAbrMutex.unlock();
        return;
    }

    if (--mAbrRefCnt > 0) {
        PAL_DBG(LOG_TAG, "mAbrRefCnt is %d", mAbrRefCnt);
        mAbrMutex.unlock();
        return;
    }

    memset(&sAttr, 0, sizeof(sAttr));
    sAttr.type = PAL_STREAM_LOW_LATENCY;
    sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;

    if (mFBDev) {
        mFBDev->lockDeviceMutex();
        isfbDeviceLocked = true;
    }
    pcm_stop(mFBPcm);
    pcm_close(mFBPcm);
    mFBPcm = NULL;

    // Reset BT driver mixer control for ABR usecase
    btSetFeedbackChannelCtrl = mixer_get_ctl_by_name(hwMixerHandle,
                                        MIXER_SET_FEEDBACK_CHANNEL);
    if (!btSetFeedbackChannelCtrl) {
        PAL_ERR(LOG_TAG, "%s mixer control not identified",
                MIXER_SET_FEEDBACK_CHANNEL);
    } else if (mixer_ctl_set_value(btSetFeedbackChannelCtrl, 0, 0) != 0) {
        PAL_ERR(LOG_TAG, "Failed to reset BT usecase");
    }

    if ((mCodecFormat == CODEC_TYPE_APTX_AD_SPEECH) && mFBDev) {
        if ((mFBDev->deviceStartStopCount > 0) &&
            (--mFBDev->deviceStartStopCount == 0)) {
            mFBDev->mIsConfigured = false;
            mFBDev->mIsAbrEnabled = false;
        }
        if (mFBDev->deviceCount > 0)
            mFBDev->deviceCount--;
        PAL_DBG(LOG_TAG, " deviceCount %d deviceStartStopCount %d for device id %d",
                mFBDev->deviceCount, mFBDev->deviceStartStopCount, mFBDev->deviceAttr.id);
    }
    if ((((mCodecFormat == CODEC_TYPE_LC3) &&
        (deviceAttr.id == PAL_DEVICE_OUT_BLUETOOTH_SCO ||
         deviceAttr.id == PAL_DEVICE_IN_BLUETOOTH_SCO_HEADSET ||
         deviceAttr.id == PAL_DEVICE_IN_BLUETOOTH_BLE)) ||
        ((mCodecFormat == CODEC_TYPE_APTX_AD_R4) &&
         (deviceAttr.id == PAL_DEVICE_IN_BLUETOOTH_BLE))) &&
         mFBDev) {
        if ((mFBDev->deviceStartStopCount > 0) &&
            (--mFBDev->deviceStartStopCount == 0)) {
            mFBDev->mIsConfigured = false;
            mFBDev->mIsAbrEnabled = false;
        }
        if (mFBDev->deviceCount > 0)
            mFBDev->deviceCount--;
        PAL_DBG(LOG_TAG, " deviceCount %d deviceStartStopCount %d for device id %d",
                mFBDev->deviceCount, mFBDev->deviceStartStopCount, mFBDev->deviceAttr.id);
    }

free_fe:
    dir = ((mCodecType == DEC) ? RX_HOSTLESS : TX_HOSTLESS);
    if (mFBPcmDevIds.size()) {
        rm->freeFrontEndIds(mFBPcmDevIds, sAttr, dir);
        mFBPcmDevIds.clear();
    }

    /* Check for deviceStartStopCount, to avoid false reset of isAbrEnabled flag in
     * case of BLE playback path stops during ongoing capture session
     */
    if (deviceStartStopCount == 1) {
        mIsAbrEnabled = false;
    }

    if (isfbDeviceLocked) {
        isfbDeviceLocked = false;
        mFBDev->unlockDeviceMutex();
    }
    mAbrMutex.unlock();
}

int32_t Bluetooth::configureSlimbusClockSrc(void)
{
    return configureDeviceClockSrc(BT_SLIMBUS_CLK_STR,
                getBtSlimClockSrc(mCodecFormat));
}


/* Scope of BtA2dp class */
// definition of static BtA2dp member variables
std::shared_ptr<Device> BtA2dp::sObjRx = nullptr;
std::shared_ptr<Device> BtA2dp::sObjTx = nullptr;
std::shared_ptr<Device> BtA2dp::sObjBleRx = nullptr;
std::shared_ptr<Device> BtA2dp::sObjBleTx = nullptr;
std::shared_ptr<Device> BtA2dp::sObjBleBroadcastRx = nullptr;
void *BtA2dp::bt_lib_source_handle = nullptr;
void *BtA2dp::bt_lib_sink_handle = nullptr;
bt_audio_pre_init_t BtA2dp::bt_audio_pre_init = nullptr;
audio_source_open_t BtA2dp::audio_source_open = nullptr;
audio_source_close_t BtA2dp::audio_source_close = nullptr;
audio_source_start_t BtA2dp::audio_source_start = nullptr;
audio_source_stop_t BtA2dp::audio_source_stop = nullptr;
audio_source_suspend_t BtA2dp::audio_source_suspend = nullptr;
audio_source_handoff_triggered_t BtA2dp::audio_source_handoff_triggered = nullptr;
clear_source_a2dpsuspend_flag_t BtA2dp::clear_source_a2dpsuspend_flag = nullptr;
audio_get_enc_config_t BtA2dp::audio_get_enc_config = nullptr;
audio_source_check_a2dp_ready_t BtA2dp::audio_source_check_a2dp_ready = nullptr;
audio_is_tws_mono_mode_enable_t BtA2dp::audio_is_tws_mono_mode_enable = nullptr;
audio_sink_get_a2dp_latency_t BtA2dp::audio_sink_get_a2dp_latency = nullptr;
audio_sink_start_t BtA2dp::audio_sink_start = nullptr;
audio_sink_stop_t BtA2dp::audio_sink_stop = nullptr;
audio_get_dec_config_t BtA2dp::audio_get_dec_config = nullptr;
audio_sink_session_setup_complete_t BtA2dp::audio_sink_session_setup_complete = nullptr;
audio_sink_check_a2dp_ready_t BtA2dp::audio_sink_check_a2dp_ready = nullptr;
audio_is_scrambling_enabled_t BtA2dp::audio_is_scrambling_enabled = nullptr;
audio_sink_suspend_t BtA2dp::audio_sink_suspend = nullptr;
audio_sink_open_t BtA2dp::audio_sink_open = nullptr;
audio_sink_close_t BtA2dp::audio_sink_close = nullptr;

btoffload_update_metadata_api_t BtA2dp::btoffload_update_metadata_api = nullptr;
audio_source_open_api_t BtA2dp::audio_source_open_api = nullptr;
audio_source_close_api_t BtA2dp::audio_source_close_api = nullptr;
audio_source_start_api_t BtA2dp::audio_source_start_api = nullptr;
audio_source_stop_api_t BtA2dp::audio_source_stop_api = nullptr;
audio_source_suspend_api_t BtA2dp::audio_source_suspend_api = nullptr;
audio_get_enc_config_api_t BtA2dp::audio_get_enc_config_api = nullptr;
audio_source_check_a2dp_ready_api_t BtA2dp::audio_source_check_a2dp_ready_api = nullptr;
audio_sink_get_a2dp_latency_api_t BtA2dp::audio_sink_get_a2dp_latency_api = nullptr;
audio_sink_start_api_t BtA2dp::audio_sink_start_api = nullptr;
audio_sink_stop_api_t BtA2dp::audio_sink_stop_api = nullptr;
audio_sink_suspend_api_t BtA2dp::audio_sink_suspend_api = nullptr;
audio_sink_open_api_t BtA2dp::audio_sink_open_api = nullptr;
audio_sink_close_api_t BtA2dp::audio_sink_close_api = nullptr;
audio_source_get_supported_latency_modes_api_t BtA2dp::audio_source_get_supported_latency_modes_api = nullptr;
audio_source_set_latency_mode_api_t BtA2dp::audio_source_set_latency_mode_api = nullptr;

BtA2dp::BtA2dp(struct pal_device *device, std::shared_ptr<ResourceManager> Rm)
      : Bluetooth(device, Rm),
        mA2dpState(A2DP_STATE_DISCONNECTED)
{
    mA2dpRole = ((device->id == PAL_DEVICE_IN_BLUETOOTH_A2DP) || (device->id == PAL_DEVICE_IN_BLUETOOTH_BLE)) ? SINK : SOURCE;
    mCodecType = ((device->id == PAL_DEVICE_IN_BLUETOOTH_A2DP) || (device->id == PAL_DEVICE_IN_BLUETOOTH_BLE)) ? DEC : ENC;
    mPluginHandler = NULL;
    mPluginCodec = NULL;

    mParamBtA2dp.reconfig = false;
    mParamBtA2dp.a2dp_suspended = false;
    mParamBtA2dp.a2dp_capture_suspended = false;
    mParamBtA2dp.is_force_switch = false;
    mParamBtA2dp.is_suspend_setparam = false;
    mIsA2dpOffloadSupported =
            property_get_bool("ro.bluetooth.a2dp_offload.supported", false) &&
            !property_get_bool("persist.bluetooth.a2dp_offload.disabled", false);

    PAL_DBG(LOG_TAG, "A2DP offload supported = %d",
            mIsA2dpOffloadSupported);
    mParamBtA2dp.reconfig_supported = mIsA2dpOffloadSupported;
    mParamBtA2dp.latency = 0;
    mCodecLatency = 0;
#ifdef FEATURE_IPQ_OPENWRT
    mA2dpLatencyMode = AUDIO_LATENCY_NORMAL;
#else
    mA2dpLatencyMode = AUDIO_LATENCY_MODE_FREE;
#endif

    mSoundDose = std::make_unique<SoundDoseUtility>(this, *device);

    if (mIsA2dpOffloadSupported) {
        init();
    }
}

BtA2dp::~BtA2dp()
{
}

tSESSION_TYPE BtA2dp::get_session_type()
{
    tSESSION_TYPE session_type = A2DP_HARDWARE_OFFLOAD_DATAPATH;
    if (mA2dpRole == SOURCE) {
        if (deviceAttr.id == PAL_DEVICE_OUT_BLUETOOTH_BLE) {
            session_type = LE_AUDIO_HARDWARE_OFFLOAD_ENCODING_DATAPATH;
        } else if (deviceAttr.id == PAL_DEVICE_OUT_BLUETOOTH_BLE_BROADCAST) {
            session_type = LE_AUDIO_BROADCAST_HARDWARE_OFFLOAD_ENCODING_DATAPATH;
        } else {
            session_type = A2DP_HARDWARE_OFFLOAD_DATAPATH;
        }
    } else {
        session_type = LE_AUDIO_HARDWARE_OFFLOAD_DECODING_DATAPATH;
    }

    return session_type;
}

void BtA2dp::open_a2dp_source()
{
    int ret = 0;

    PAL_DBG(LOG_TAG, "Open A2DP source start");
    if (bt_lib_source_handle && (audio_source_open_api ||
        audio_source_open)) {
        if (mA2dpState == A2DP_STATE_DISCONNECTED) {
            PAL_DBG(LOG_TAG, "calling BT stream open");
            /*To support backward compatibility check for BT IPC API's
             * with session_type or w/o session_type*/
            if (audio_source_open_api) {
                ret = audio_source_open_api(get_session_type());
                if (ret != 0) {
                    PAL_ERR(LOG_TAG, "Failed to open source stream for a2dp: status %d", ret);
                    return;
                }
            } else {
                ret = audio_source_open();
                if (ret != 0) {
                    PAL_ERR(LOG_TAG, "Failed to open source stream for a2dp: status %d", ret);
                    return;
                }
            }
            mA2dpState = A2DP_STATE_CONNECTED;
        } else {
            PAL_DBG(LOG_TAG, "Called a2dp open with improper state %d", mA2dpState);
        }
    }
}

int BtA2dp::close_audio_source()
{
    PAL_VERBOSE(LOG_TAG, "Enter");

    if (!(bt_lib_source_handle && (audio_source_close_api ||
        audio_source_close))) {
        PAL_ERR(LOG_TAG, "a2dp source handle is not identified, Ignoring close request");
        return -ENOSYS;
    }

    if (mA2dpState != A2DP_STATE_DISCONNECTED) {
        PAL_DBG(LOG_TAG, "calling BT source stream close");
        mDeviceMutex.lock();
        if (audio_source_close_api) {
            if (audio_source_close_api(get_session_type()) == false)
                PAL_ERR(LOG_TAG, "failed close a2dp source control path from BT library");
        } else {
            if (audio_source_close() == false)
                PAL_ERR(LOG_TAG, "failed close a2dp source control path from BT library");
        }
        mDeviceMutex.unlock();
    }
    mTotalActiveSessionRequests = 0;
    mParamBtA2dp.latency = 0;
    mA2dpState = A2DP_STATE_DISCONNECTED;
    mIsConfigured = false;

    return 0;
}

void BtA2dp::init_a2dp_source()
{
    PAL_DBG(LOG_TAG, "init_a2dp_source START");
    if (bt_lib_source_handle == nullptr) {
        PAL_DBG(LOG_TAG, "Requesting for BT lib handle");
        bt_lib_source_handle = dlopen(BT_IPC_SOURCE_LIB, RTLD_NOW);
        if (bt_lib_source_handle == nullptr) {
            PAL_ERR(LOG_TAG, "dlopen failed for %s", BT_IPC_SOURCE_LIB);
            return;
        }
    }
    bt_audio_pre_init = (bt_audio_pre_init_t)
                  dlsym(bt_lib_source_handle, "bt_audio_pre_init");
    audio_source_open_api = (audio_source_open_api_t)
                  dlsym(bt_lib_source_handle, "audio_stream_open_api");
    audio_source_start_api = (audio_source_start_api_t)
                  dlsym(bt_lib_source_handle, "audio_start_stream_api");
    audio_get_enc_config_api = (audio_get_enc_config_api_t)
                  dlsym(bt_lib_source_handle, "audio_get_codec_config_api");
    audio_source_suspend_api = (audio_source_suspend_api_t)
                  dlsym(bt_lib_source_handle, "audio_suspend_stream_api");
    audio_source_handoff_triggered = (audio_source_handoff_triggered_t)
                  dlsym(bt_lib_source_handle, "audio_handoff_triggered");
    clear_source_a2dpsuspend_flag = (clear_source_a2dpsuspend_flag_t)
                  dlsym(bt_lib_source_handle, "clear_a2dpsuspend_flag");
    audio_source_stop_api = (audio_source_stop_api_t)
                  dlsym(bt_lib_source_handle, "audio_stop_stream_api");
    audio_source_close_api = (audio_source_close_api_t)
                  dlsym(bt_lib_source_handle, "audio_stream_close_api");
    audio_source_check_a2dp_ready_api = (audio_source_check_a2dp_ready_api_t)
                  dlsym(bt_lib_source_handle, "audio_check_a2dp_ready_api");
    audio_sink_get_a2dp_latency_api = (audio_sink_get_a2dp_latency_api_t)
                  dlsym(bt_lib_source_handle, "audio_sink_get_a2dp_latency_api");
    audio_is_tws_mono_mode_enable = (audio_is_tws_mono_mode_enable_t)
                  dlsym(bt_lib_source_handle, "isTwsMonomodeEnable");
    audio_is_scrambling_enabled = (audio_is_scrambling_enabled_t)
                  dlsym(bt_lib_source_handle, "audio_is_scrambling_enabled");
    btoffload_update_metadata_api = (btoffload_update_metadata_api_t)
                  dlsym(bt_lib_source_handle, "update_metadata");
    audio_source_get_supported_latency_modes_api = (audio_source_get_supported_latency_modes_api_t)
                  dlsym(bt_lib_source_handle, "audio_stream_get_supported_latency_modes_api");
    audio_source_set_latency_mode_api = (audio_source_set_latency_mode_api_t)
                  dlsym(bt_lib_source_handle, "audio_stream_set_latency_mode_api");

    audio_source_open = (audio_source_open_t)
        dlsym(bt_lib_source_handle, "audio_stream_open");
    audio_source_start = (audio_source_start_t)
        dlsym(bt_lib_source_handle, "audio_start_stream");
    audio_get_enc_config = (audio_get_enc_config_t)
        dlsym(bt_lib_source_handle, "audio_get_codec_config");
    audio_source_suspend = (audio_source_suspend_t)
        dlsym(bt_lib_source_handle, "audio_suspend_stream");

    audio_source_stop = (audio_source_stop_t)
        dlsym(bt_lib_source_handle, "audio_stop_stream");
    audio_source_close = (audio_source_close_t)
        dlsym(bt_lib_source_handle, "audio_stream_close");
    audio_source_check_a2dp_ready = (audio_source_check_a2dp_ready_t)
        dlsym(bt_lib_source_handle, "audio_check_a2dp_ready");
    audio_sink_get_a2dp_latency = (audio_sink_get_a2dp_latency_t)
        dlsym(bt_lib_source_handle, "audio_sink_get_a2dp_latency");


    if (bt_lib_source_handle && bt_audio_pre_init) {
        PAL_DBG(LOG_TAG, "calling BT module preinit");
        bt_audio_pre_init();
    }
}

void BtA2dp::init_a2dp_sink()
{
    PAL_DBG(LOG_TAG, "Open A2DP input start");
    if (bt_lib_sink_handle == nullptr) {
        PAL_DBG(LOG_TAG, "Requesting for BT lib handle");
        bt_lib_sink_handle = dlopen(BT_IPC_SINK_LIB, RTLD_NOW);

        if (bt_lib_sink_handle == nullptr) {
#ifndef LINUX_ENABLED
            // On Mobile LE VoiceBackChannel implemented as A2DPSink Profile.
            // However - All the BT-Host IPC calls are exposed via Source LIB itself.
            PAL_DBG(LOG_TAG, "Requesting for BT lib source handle");
            bt_lib_sink_handle = dlopen(BT_IPC_SOURCE_LIB, RTLD_NOW);
            if (bt_lib_sink_handle == nullptr) {
                PAL_ERR(LOG_TAG, "DLOPEN failed");
                return;
            }
            audio_get_enc_config_api = (audio_get_enc_config_api_t)
                  dlsym(bt_lib_sink_handle, "audio_get_codec_config_api");
            audio_sink_get_a2dp_latency_api = (audio_sink_get_a2dp_latency_api_t)
                dlsym(bt_lib_sink_handle, "audio_sink_get_a2dp_latency_api");
            audio_sink_start_api = (audio_sink_start_api_t)
                  dlsym(bt_lib_sink_handle, "audio_start_stream_api");
            audio_sink_stop_api = (audio_sink_stop_api_t)
                  dlsym(bt_lib_sink_handle, "audio_stop_stream_api");
            audio_source_check_a2dp_ready_api = (audio_source_check_a2dp_ready_api_t)
                  dlsym(bt_lib_sink_handle, "audio_check_a2dp_ready_api");
            audio_sink_suspend_api = (audio_sink_suspend_api_t)
                dlsym(bt_lib_sink_handle, "audio_suspend_stream_api");
            audio_sink_open_api = (audio_sink_open_api_t)
                dlsym(bt_lib_sink_handle, "audio_stream_open_api");
            audio_sink_close_api = (audio_sink_close_api_t)
                dlsym(bt_lib_sink_handle, "audio_stream_close_api");
            btoffload_update_metadata_api = (btoffload_update_metadata_api_t)
                  dlsym(bt_lib_sink_handle, "update_metadata");

            audio_get_enc_config = (audio_get_enc_config_t)
                dlsym(bt_lib_sink_handle, "audio_get_codec_config");
            audio_sink_get_a2dp_latency = (audio_sink_get_a2dp_latency_t)
                dlsym(bt_lib_sink_handle, "audio_sink_get_a2dp_latency");
            audio_sink_start = (audio_sink_start_t)
                dlsym(bt_lib_sink_handle, "audio_start_stream");
            audio_sink_stop = (audio_sink_stop_t)
                dlsym(bt_lib_sink_handle, "audio_stop_stream");
            audio_source_check_a2dp_ready = (audio_source_check_a2dp_ready_t)
                dlsym(bt_lib_sink_handle, "audio_check_a2dp_ready");
            audio_sink_suspend = (audio_sink_suspend_t)
                dlsym(bt_lib_sink_handle, "audio_suspend_stream");
            audio_sink_open = (audio_sink_open_t)
                dlsym(bt_lib_sink_handle, "audio_stream_open");
            audio_sink_close = (audio_sink_close_t)
                dlsym(bt_lib_sink_handle, "audio_stream_close");
#else
            // On Linux Builds - A2DP Sink Profile is supported via different lib
            PAL_ERR(LOG_TAG, "DLOPEN failed for %s", BT_IPC_SINK_LIB);
#endif
        } else {
            audio_sink_start = (audio_sink_start_t)
                          dlsym(bt_lib_sink_handle, "audio_sink_start_capture");
            audio_get_dec_config = (audio_get_dec_config_t)
                          dlsym(bt_lib_sink_handle, "audio_get_decoder_config");
            audio_sink_stop = (audio_sink_stop_t)
                          dlsym(bt_lib_sink_handle, "audio_sink_stop_capture");
            audio_sink_check_a2dp_ready = (audio_sink_check_a2dp_ready_t)
                          dlsym(bt_lib_sink_handle, "audio_sink_check_a2dp_ready");
            audio_sink_session_setup_complete = (audio_sink_session_setup_complete_t)
                          dlsym(bt_lib_sink_handle, "audio_sink_session_setup_complete");
        }
    }

#ifndef LINUX_ENABLED
    mIsDummySink = true;
#endif

}

void BtA2dp::open_a2dp_sink()
{
    int ret = 0;

    PAL_DBG(LOG_TAG, "Open A2DP sink start");
    if (bt_lib_sink_handle && (audio_sink_open_api ||
        audio_sink_open)) {
        if (mA2dpState == A2DP_STATE_DISCONNECTED) {
            PAL_DBG(LOG_TAG, "calling BT stream open");
            if (audio_sink_open_api) {
                ret = audio_sink_open_api(get_session_type());
                if (ret != 0) {
                    PAL_ERR(LOG_TAG, "Failed to open sink stream for a2dp: status %d", ret);
                }
            } else {
                ret = audio_sink_open();
                if (ret != 0) {
                    PAL_ERR(LOG_TAG, "Failed to open sink stream for a2dp: status %d", ret);
                }
            }
            mA2dpState = A2DP_STATE_CONNECTED;
        }
        else {
            PAL_DBG(LOG_TAG, "Called a2dp open with improper state %d", mA2dpState);
        }
    }
}

int BtA2dp::close_audio_sink()
{
    PAL_VERBOSE(LOG_TAG, "Enter");

    if (!(bt_lib_sink_handle && (audio_sink_close_api ||
        audio_sink_close))) {
        PAL_ERR(LOG_TAG, "a2dp sink handle is not identified, Ignoring close request");
        return -ENOSYS;
    }

    if (mA2dpState != A2DP_STATE_DISCONNECTED) {
        PAL_DBG(LOG_TAG, "calling BT sink stream close");
        mDeviceMutex.lock();
        if (audio_sink_close_api) {
            if (audio_sink_close_api(get_session_type()) == false)
                PAL_ERR(LOG_TAG, "failed close a2dp sink control path from BT library");
        } else {
            if (audio_sink_close() == false)
                PAL_ERR(LOG_TAG, "failed close a2dp sink control path from BT library");
        }
        mDeviceMutex.unlock();
    }
    mTotalActiveSessionRequests = 0;
    mParamBtA2dp.latency = 0;
    mA2dpState = A2DP_STATE_DISCONNECTED;
    mIsConfigured = false;

    return 0;
}

bool BtA2dp::a2dp_send_sink_setup_complete()
{
    uint64_t system_latency = 0;
    bool is_complete = false;

    /* TODO : Replace this with call to plugin */
    system_latency = 200;

    if (audio_sink_session_setup_complete(system_latency) == 0) {
        is_complete = true;
    }
    return is_complete;
}

void BtA2dp::init()
{
    (mA2dpRole == SOURCE) ? init_a2dp_source() : init_a2dp_sink();
}

int BtA2dp::start()
{
    int status = 0;
    mDeviceMutex.lock();

    status = (mA2dpRole == SOURCE) ? startPlayback() : startCapture();
    if (status) {
        goto exit;
    }

    if (mTotalActiveSessionRequests == 1) {
        status = configureSlimbusClockSrc();
        if (status) {
            goto exit;
        }
    }

    // start computation for first start instance
    if (deviceStartStopCount == 0) {
        mSoundDose->startComputation();
    }

    status = Device::start_l();

    if (customPayload) {
        free(customPayload);
        customPayload = NULL;
        customPayloadSize = 0;
    }

    if (!status && mIsAbrEnabled)
        startAbr();
exit:
    mDeviceMutex.unlock();
    return status;
}

int BtA2dp::stop()
{
    int status = 0;
    PAL_DBG(LOG_TAG, " Enter %s",__func__);

    mDeviceMutex.lock();
    if (mIsAbrEnabled)
        stopAbr();

    // stop computation only when 1 instance is left
    if (deviceStartStopCount == 1) {
        mSoundDose->stopComputation();
    }

    Device::stop_l();

    /* Stop sound dose graph & de-register for the events.*/
    status = (mA2dpRole == SOURCE) ? stopPlayback() : stopCapture();
    mDeviceMutex.unlock();

    PAL_DBG(LOG_TAG, "Exit %s",__func__);
    return status;
}

int BtA2dp::startPlayback()
{
    int ret = 0;
    uint8_t multi_cast = 0, num_dev = 1;

    PAL_DBG(LOG_TAG, "a2dp_start_playback start");

    if (!(bt_lib_source_handle && (audio_source_start_api ||
         audio_source_start) && (audio_get_enc_config_api ||
         audio_get_enc_config))) {
        PAL_ERR(LOG_TAG, "a2dp handle is not identified, Ignoring start playback request");
        return -ENOSYS;
    }

    if (mParamBtA2dp.a2dp_suspended) {
        // session will be restarted after suspend completion
        PAL_ERR(LOG_TAG, "a2dp start requested during suspend state");
        return -EAGAIN;
    } else if (mA2dpState == A2DP_STATE_DISCONNECTED) {
        // update device status, if still disconnected, return error.
        if (!(rm->isDeviceAvailable(deviceAttr.id) &&
              checkDeviceStatus() != A2DP_STATE_DISCONNECTED)) {
            PAL_ERR(LOG_TAG, "a2dp start requested when a2dp source stream is failed to open");
            return -ENOSYS;
        }
    }

    if (mA2dpState != A2DP_STATE_STARTED && !mTotalActiveSessionRequests) {
        mCodecFormat = CODEC_TYPE_INVALID;

        if (!mIsConfigured)
            mIsAbrEnabled = false;

        PAL_DBG(LOG_TAG, "calling BT module stream start");
        /* This call indicates BT IPC lib to start playback */
        if (audio_source_start_api) {
            ret = audio_source_start_api(get_session_type());
        } else {
            ret = audio_source_start();
        }
        if (ret != 0) {
            // TODO: CTRL_ACK_RECONFIGURATION needs retry design
            if (ret <= CTRL_SKT_DISCONNECTED && ret >= CTRL_ACK_FAILURE)
                ret = -ENODEV;
            PAL_ERR(LOG_TAG, "BT controller start failed");
            return ret;
        }
        PAL_INFO(LOG_TAG, "BT controller start return = %d", ret);

        if (audio_source_set_latency_mode_api) {
            ret = audio_source_set_latency_mode_api(get_session_type(), mA2dpLatencyMode);
            if (ret) {
                PAL_DBG(LOG_TAG, "Warning: Set latency mode failed for value %d with exit status %d", mA2dpLatencyMode, ret);
                ret = 0;
            }
        }

        PAL_DBG(LOG_TAG, "configure_a2dp_encoder_format start");
        if (audio_get_enc_config_api) {
            mCodecInfo = audio_get_enc_config_api(get_session_type(), &multi_cast, &num_dev, (audio_format_t*)&mCodecFormat);
        }
        else {
            mCodecInfo = audio_get_enc_config(&multi_cast, &num_dev, (audio_format_t*)&mCodecFormat);
        }

        if (mCodecInfo == NULL || mCodecFormat == CODEC_TYPE_INVALID) {
            PAL_ERR(LOG_TAG, "invalid encoder config");
            if (audio_source_stop_api) {
                audio_source_stop_api(get_session_type());
            } else {
                audio_source_stop();
            }
            return -EINVAL;
        }

        if (mCodecFormat == CODEC_TYPE_APTX_DUAL_MONO && audio_is_tws_mono_mode_enable)
            mIsTwsMonoModeOn = audio_is_tws_mono_mode_enable();

        if (audio_is_scrambling_enabled)
            mIsScramblingEnabled = audio_is_scrambling_enabled();
        PAL_INFO(LOG_TAG, "mIsScramblingEnabled = %d", mIsScramblingEnabled);

        /* Update Device GKV based on Encoder type */
        updateDeviceMetadata();
        if (!mIsConfigured) {
            ret = configureGraphModules();
            if (ret) {
                PAL_ERR(LOG_TAG, "unable to configure DSP encoder");
                if (audio_source_stop_api) {
                    audio_source_stop_api(get_session_type());
                } else {
                    audio_source_stop();
                }
                return ret;
            }
        }

        if (mPluginCodec) {
            mCodecLatency = mPluginCodec->plugin_get_codec_latency(mPluginCodec);
        }

        mA2dpState = A2DP_STATE_STARTED;
    } else {
        /* Update Device GKV based on Already received encoder. */
        /* This is required for getting tagged module info in session class. */
        updateDeviceMetadata();
    }

    mTotalActiveSessionRequests++;
    PAL_DBG(LOG_TAG, "start A2DP playback total active sessions :%d",
            mTotalActiveSessionRequests);
    return ret;
}

int BtA2dp::stopPlayback()
{
    int ret =0;

    PAL_VERBOSE(LOG_TAG, "a2dp_stop_playback start");
    if (!(bt_lib_source_handle && (audio_source_stop_api ||
        audio_source_stop))) {
        PAL_ERR(LOG_TAG, "a2dp handle is not identified, Ignoring stop request");
        return -ENOSYS;
    }

    if (mTotalActiveSessionRequests > 0)
        mTotalActiveSessionRequests--;
    else
        PAL_ERR(LOG_TAG, "No active playback session requests on A2DP");

    if (mA2dpState == A2DP_STATE_STARTED && !mTotalActiveSessionRequests) {
        PAL_VERBOSE(LOG_TAG, "calling BT module stream stop");
        if (audio_source_stop_api) {
            ret = audio_source_stop_api(get_session_type());
        } else {
            ret = audio_source_stop();
        }

        if (ret < 0) {
            PAL_ERR(LOG_TAG, "stop stream to BT IPC lib failed");
        } else {
            PAL_VERBOSE(LOG_TAG, "stop steam to BT IPC lib successful");
        }

        if (deviceStartStopCount == 0) {
            mIsConfigured = false;
        }
        mA2dpState = A2DP_STATE_STOPPED;
#ifdef FEATURE_IPQ_OPENWRT
    mA2dpLatencyMode = AUDIO_LATENCY_NORMAL;
#else
    if(!mParamBtA2dp.reconfig || !mA2dpLatencyUpdatedFromFramework || mParamBtA2dp.is_in_call)
    {
        mA2dpLatencyMode = AUDIO_LATENCY_MODE_FREE;
    }
    if (mA2dpLatencyUpdatedFromFramework){
        mA2dpLatencyUpdatedFromFramework = false;
    }
    mParamBtA2dp.reconfig = false;
    mParamBtA2dp.is_in_call = false;
#endif
        mCodecInfo = NULL;
        mParamBtA2dp.latency = 0;
        mCodecLatency = 0;

        /* Reset mIsTwsMonoModeOn and isLC3MonoModeOn during stop */
        if (!mParamBtA2dp.a2dp_suspended) {
            mIsTwsMonoModeOn = false;
            mIsLC3MonoModeOn = false;
            mIsScramblingEnabled = false;
        }

        if (mPluginCodec) {
            mPluginCodec->close_plugin(mPluginCodec);
            mPluginCodec = NULL;
        }
        if (mPluginHandler) {
            dlclose(mPluginHandler);
            mPluginHandler = NULL;
        }
    }

    PAL_DBG(LOG_TAG, "Stop A2DP playback, total active sessions :%d",
            mTotalActiveSessionRequests);
    return 0;
}

bool BtA2dp::isDeviceReady(pal_device_id_t id)
{
    bool ret = false;

    if (!rm->isDeviceAvailable(id))
        return ret;

    if (mA2dpRole == SOURCE) {
        if (mParamBtA2dp.a2dp_suspended)
            return ret;
    } else if (mA2dpRole == SINK) {
        if (mParamBtA2dp.a2dp_capture_suspended)
            return ret;
    }

    if ((mA2dpState != A2DP_STATE_DISCONNECTED) &&
        (mIsA2dpOffloadSupported)) {
        if ((mA2dpRole == SOURCE) || mIsDummySink) {
            if (audio_source_check_a2dp_ready_api) {
                ret = audio_source_check_a2dp_ready_api(get_session_type());
            } else {
                ret = audio_source_check_a2dp_ready();
            }
        } else {
            if (audio_sink_check_a2dp_ready)
                ret = audio_sink_check_a2dp_ready();
        }
    }
    return ret;
}

int BtA2dp::startCapture()
{
    int ret = 0;

    PAL_DBG(LOG_TAG, "a2dp_start_capture start");

    if (!mIsDummySink) {
        if (!(bt_lib_sink_handle && (audio_sink_start_api ||
            audio_sink_start) && audio_get_dec_config)) {
            PAL_ERR(LOG_TAG, "a2dp handle is not identified, Ignoring start capture request");
            return -ENOSYS;
        }

        if (mA2dpState != A2DP_STATE_STARTED  && !mTotalActiveSessionRequests) {
            mCodecFormat = CODEC_TYPE_INVALID;
            PAL_DBG(LOG_TAG, "calling BT module stream start");
            /* This call indicates BT IPC lib to start capture */
            if (audio_sink_start_api) {
                ret = audio_sink_start_api(get_session_type());
            } else {
                ret = audio_sink_start();
            }

            PAL_INFO(LOG_TAG, "BT controller start capture return = %d",ret);
            if (ret != 0 ) {
                // TODO: CTRL_ACK_RECONFIGURATION needs retry design
                if (ret <= CTRL_SKT_DISCONNECTED && ret >= CTRL_ACK_FAILURE)
                    ret = -ENODEV;
                PAL_ERR(LOG_TAG, "BT controller start capture failed");
                return ret;
            }

            mCodecInfo = audio_get_dec_config((audio_format_t *)&mCodecFormat);
            if (mCodecInfo == NULL || mCodecFormat == CODEC_TYPE_INVALID) {
                PAL_ERR(LOG_TAG, "invalid codec config");
                if (audio_sink_stop_api) {
                    audio_sink_stop_api(get_session_type());
                } else {
                    audio_sink_stop();
                }
                return -EINVAL;
            }

            /* Update Device GKV based on Decoder type */
            updateDeviceMetadata();

            ret = configureGraphModules();
            if (ret) {
                PAL_ERR(LOG_TAG, "unable to configure DSP decoder");
                if (audio_sink_stop_api) {
                    audio_sink_stop_api(get_session_type());
                } else {
                    audio_sink_stop();
                }
                return ret;
            }
        }
    } else {
        uint8_t multi_cast = 0, num_dev = 1;

        if (!(bt_lib_sink_handle && (audio_sink_start_api ||
            audio_sink_start) && (audio_get_enc_config_api ||
            audio_get_enc_config))) {
            PAL_ERR(LOG_TAG, "a2dp handle is not identified, Ignoring start capture request");
            return -ENOSYS;
        }

        if (mParamBtA2dp.a2dp_capture_suspended) {
            // session will be restarted after suspend completion
            PAL_INFO(LOG_TAG, "a2dp start capture requested during suspend state");
            return -EAGAIN;
        }

        if (mA2dpState != A2DP_STATE_STARTED  && !mTotalActiveSessionRequests) {
            mCodecFormat = CODEC_TYPE_INVALID;
            PAL_DBG(LOG_TAG, "calling BT module stream start");
            /* This call indicates BT IPC lib to start */
            if (audio_sink_start_api) {
                ret = audio_sink_start_api(get_session_type());
            }
            else {
                ret = audio_sink_start();
            }

            PAL_INFO(LOG_TAG, "BT controller start return = %d",ret);
            if (ret != 0 ) {
                // TODO: CTRL_ACK_RECONFIGURATION needs retry design
                if (ret <= CTRL_SKT_DISCONNECTED && ret >= CTRL_ACK_FAILURE)
                    ret = -ENODEV;
                PAL_ERR(LOG_TAG, "BT controller start failed");
                return ret;
            }

            if (audio_get_enc_config_api) {
                mCodecInfo = audio_get_enc_config_api(get_session_type(), &multi_cast, &num_dev, (audio_format_t*)&mCodecFormat);
            } else {
                mCodecInfo = audio_get_enc_config(&multi_cast, &num_dev, (audio_format_t*)&mCodecFormat);
            }

            if (mCodecInfo == NULL || mCodecFormat == CODEC_TYPE_INVALID) {
                PAL_ERR(LOG_TAG, "invalid codec config");
                if (audio_sink_stop_api) {
                    audio_sink_stop_api(get_session_type());
                } else {
                    audio_sink_stop();
                }
                return -EINVAL;
            }

            /* Update Device GKV based on Decoder type */
            updateDeviceMetadata();

            ret = configureGraphModules();
            if (ret) {
                PAL_ERR(LOG_TAG, "unable to configure DSP decoder");
                if (audio_sink_stop_api) {
                    audio_sink_stop_api(get_session_type());
                } else {
                    audio_sink_stop();
                }
                return ret;
            }
        }
    }

    if (!mIsDummySink) {
        if (!a2dp_send_sink_setup_complete()) {
            PAL_ERR(LOG_TAG, "sink_setup_complete not successful");
            if (audio_sink_stop_api) {
                audio_sink_stop_api(get_session_type());
            } else {
                audio_sink_stop();
            }
            ret = -ETIMEDOUT;
        }
    }

    mA2dpState = A2DP_STATE_STARTED;
    mTotalActiveSessionRequests++;

    PAL_DBG(LOG_TAG, "start A2DP sink total active sessions :%d",
                      mTotalActiveSessionRequests);
    return ret;
}

int BtA2dp::stopCapture()
{
    int ret =0;

    PAL_VERBOSE(LOG_TAG, "a2dp_stop_capture start");
    if (!(bt_lib_sink_handle && (audio_sink_stop_api ||
         audio_sink_stop))) {
        PAL_ERR(LOG_TAG, "a2dp handle is not identified, Ignoring stop request");
        return -ENOSYS;
    }

    if (mTotalActiveSessionRequests > 0)
        mTotalActiveSessionRequests--;

    if (!mTotalActiveSessionRequests) {
        PAL_VERBOSE(LOG_TAG, "calling BT module stream stop");
        mIsConfigured = false;
        if (audio_sink_stop_api) {
            ret = audio_sink_stop_api(get_session_type());
        } else {
            ret = audio_sink_stop();
        }

        if (ret < 0) {
            PAL_ERR(LOG_TAG, "stop stream to BT IPC lib failed");
        } else {
            PAL_VERBOSE(LOG_TAG, "stop steam to BT IPC lib successful");
        }

        // It can be in A2DP_STATE_DISCONNECTED, if device disconnect happens prior to Stop.
        if (mA2dpState == A2DP_STATE_STARTED)
            mA2dpState = A2DP_STATE_STOPPED;

        mParamBtA2dp.latency = 0;

        if (mPluginCodec) {
            mPluginCodec->close_plugin(mPluginCodec);
            mPluginCodec = NULL;
        }
        if (mPluginHandler) {
            dlclose(mPluginHandler);
            mPluginHandler = NULL;
        }
    }
    PAL_DBG(LOG_TAG, "Stop A2DP capture, total active sessions :%d",
            mTotalActiveSessionRequests);
    return 0;
}

int32_t BtA2dp::setDeviceParameter(uint32_t param_id, void *param)
{
    int32_t status = 0;
    pal_param_bta2dp_t* param_a2dp = (pal_param_bta2dp_t *)param;
    bool skip_switch = false;

    if (mIsA2dpOffloadSupported == false) {
       PAL_VERBOSE(LOG_TAG, "no supported encoders identified,ignoring a2dp setparam");
       status = -EINVAL;
       goto exit;
    }

    switch(param_id) {
    case PAL_PARAM_ID_DEVICE_CONNECTION:
    {
        pal_param_device_connection_t *device_connection =
            (pal_param_device_connection_t *)param;
        if (device_connection->connection_state == true) {
            mSoundDose->setDevice(device_connection->device);
            if (mA2dpRole == SOURCE)
                open_a2dp_source();

            else {
#ifdef A2DP_SINK_SUPPORTED

                open_a2dp_sink();
#else
                mA2dpState = A2DP_STATE_CONNECTED;
#endif
            }
        } else {
            if (mA2dpRole == SOURCE) {
                status = close_audio_source();
            } else {
#ifdef A2DP_SINK_SUPPORTED
                status = close_audio_sink();
#else
                mTotalActiveSessionRequests = 0;
                mParamBtA2dp.latency = 0;
                mA2dpState = A2DP_STATE_DISCONNECTED;
#endif
            }
        }
        break;
    }
    case PAL_PARAM_ID_BT_A2DP_RECONFIG:
    {
        if (mA2dpState != A2DP_STATE_DISCONNECTED) {
            mParamBtA2dp.reconfig = param_a2dp->reconfig;
        }
        break;
    }
    case PAL_PARAM_ID_BT_A2DP_SUSPENDED:
    {
        if (bt_lib_source_handle == nullptr)
            goto exit;

        mParamBtA2dp.is_suspend_setparam = param_a2dp->is_suspend_setparam;
        mParamBtA2dp.reconfig = param_a2dp->reconfig;
        mParamBtA2dp.is_in_call = param_a2dp->is_in_call;

        if (mParamBtA2dp.a2dp_suspended == param_a2dp->a2dp_suspended)
            goto exit;

        if (param_a2dp->a2dp_suspended == true) {
            mParamBtA2dp.a2dp_suspended = true;
            if (mA2dpState == A2DP_STATE_DISCONNECTED)
                goto exit;
            if (rm->IsDummyDevEnabled()) {
                status = a2dpSuspendToDummy(param_a2dp->dev_id);
            } else {
                status = a2dpSuspend(param_a2dp->dev_id);
            }
            if (audio_source_suspend_api)
                audio_source_suspend_api(get_session_type());
            else
                audio_source_suspend();
        } else {
            mParamBtA2dp.a2dp_suspended = false;
            if (mA2dpState == A2DP_STATE_DISCONNECTED)
                goto exit;
            if (clear_source_a2dpsuspend_flag)
                clear_source_a2dpsuspend_flag();

            if (mTotalActiveSessionRequests > 0) {
                if (audio_source_start_api) {
                    status = audio_source_start_api(get_session_type());
                } else {
                    status = audio_source_start();
                }
                if (status) {
                    PAL_ERR(LOG_TAG, "BT controller start failed");
                    goto exit;
                }
            }

            if (param_a2dp->is_suspend_setparam && param_a2dp->is_in_call)
                skip_switch = true;

            if (!skip_switch) {
                if (rm->IsDummyDevEnabled()) {
                    status = a2dpResumeFromDummy(param_a2dp->dev_id);
                } else {
                    status = a2dpResume(param_a2dp->dev_id);
                }
            }
        }
        break;
    }
    case PAL_PARAM_ID_BT_A2DP_TWS_CONFIG:
    {
        mIsTwsMonoModeOn = param_a2dp->is_tws_mono_mode_on;
        if (mA2dpState == A2DP_STATE_STARTED) {
            std::shared_ptr<Device> dev = nullptr;
            Stream *stream = NULL;
            Session *session = NULL;
            std::vector<Stream*> activestreams;
            pal_bt_tws_payload param_tws;

            dev = Device::getInstance(&deviceAttr, rm);
            status = rm->getActiveStream_l(activestreams, dev);
            if ((0 != status) || (activestreams.size() == 0)) {
                PAL_ERR(LOG_TAG, "no active stream available");
                return -EINVAL;
            }
            stream = static_cast<Stream *>(activestreams[0]);
            stream->getAssociatedSession(&session);
            param_tws.isTwsMonoModeOn = mIsTwsMonoModeOn;
            param_tws.codecFormat = (uint32_t)mCodecFormat;
            session->setParameters(stream, param_id, &param_tws);
        }
        break;
    }
    case PAL_PARAM_ID_BT_A2DP_LC3_CONFIG:
    {
        mIsLC3MonoModeOn = param_a2dp->is_lc3_mono_mode_on;
        if (mA2dpState == A2DP_STATE_STARTED) {
            std::shared_ptr<Device> dev = nullptr;
            Stream *stream = NULL;
            Session *session = NULL;
            std::vector<Stream*> activestreams;
            pal_bt_lc3_payload param_lc3;

            dev = Device::getInstance(&deviceAttr, rm);
            status = rm->getActiveStream_l(activestreams, dev);
            if ((0 != status) || (activestreams.size() == 0)) {
                PAL_ERR(LOG_TAG, "no active stream available");
                return -EINVAL;
            }
            stream = static_cast<Stream *>(activestreams[0]);
            stream->getAssociatedSession(&session);
            param_lc3.isLC3MonoModeOn = mIsLC3MonoModeOn;
            session->setParameters(stream, param_id, &param_lc3);
        }
        break;
    }
    case PAL_PARAM_ID_BT_A2DP_CAPTURE_SUSPENDED:
    {
        if (bt_lib_sink_handle == nullptr)
            goto exit;

        mParamBtA2dp.is_suspend_setparam = param_a2dp->is_suspend_setparam;

        if (mParamBtA2dp.a2dp_capture_suspended == param_a2dp->a2dp_capture_suspended)
            goto exit;

        if (param_a2dp->a2dp_capture_suspended == true) {
            mParamBtA2dp.a2dp_capture_suspended = true;
            if (mA2dpState == A2DP_STATE_DISCONNECTED)
                goto exit;

            if (rm->IsDummyDevEnabled()) {
                a2dpCaptureSuspendToDummy(param_a2dp->dev_id);
            } else {
                a2dpCaptureSuspend(param_a2dp->dev_id);
            }
            if (audio_sink_suspend_api)
                audio_sink_suspend_api(get_session_type());
            else
                audio_sink_suspend();
        } else {
            mParamBtA2dp.a2dp_capture_suspended = false;
            if (mA2dpState == A2DP_STATE_DISCONNECTED)
                goto exit;

            if (clear_source_a2dpsuspend_flag)
                clear_source_a2dpsuspend_flag();

            if (mTotalActiveSessionRequests > 0) {
                if (audio_sink_start_api) {
                    status = audio_sink_start_api(get_session_type());
                } else {
                    status = audio_sink_start();
                }

                if (status) {
                    PAL_ERR(LOG_TAG, "BT controller start failed");
                    goto exit;
                }
            }

            if (param_a2dp->is_suspend_setparam && param_a2dp->is_in_call)
                skip_switch = true;

            if (!skip_switch) {
                if (rm->IsDummyDevEnabled()) {
                    a2dpCaptureResumeFromDummy(param_a2dp->dev_id);
                } else {
                    a2dpCaptureResume(param_a2dp->dev_id);
                }
            }
        }
        break;
    }
    case PAL_PARAM_ID_SET_SINK_METADATA:
    {
        if (btoffload_update_metadata_api) {
            PAL_INFO(LOG_TAG, "sending sink metadata to BT API");
            btoffload_update_metadata_api(get_session_type(), param);
        }
        break;
    }
    case PAL_PARAM_ID_SET_SOURCE_METADATA:
    {
        if (btoffload_update_metadata_api) {
            PAL_INFO(LOG_TAG, "sending source metadata to BT API");
            btoffload_update_metadata_api(get_session_type(), param);
        }
        break;
    }
    case PAL_PARAM_ID_LATENCY_MODE:
    {
        if (audio_source_set_latency_mode_api) {
            mA2dpLatencyMode = ((pal_param_latency_mode_t *)param)->modes[0];
            mA2dpLatencyUpdatedFromFramework = true;
            status = audio_source_set_latency_mode_api(get_session_type(), mA2dpLatencyMode);
            if (status) {
                PAL_ERR(LOG_TAG, "Set Parameter %d failed for value %d with exit status %d", param_id, mA2dpLatencyMode, status);
                goto exit;
            }
        } else {
            status = -ENOSYS;
        }
        break;
    }
    default:
        return -EINVAL;
    }

exit:
    return status;
}

uint32_t BtA2dp::getLatency(uint32_t slatency)
{
    uint32_t latency = mCodecLatency;
    /**
     * based on the observed AVsync latency, latency is adjusted as per test.
     * Although such latency adjustment is not ideal, we tend to as per platform
     * test
     **/
    uint32_t tunedLatency = 0;

    switch (mCodecType) {
    case ENC:
        switch (mCodecFormat) {
        case CODEC_TYPE_SBC:
            latency += (slatency == 0) ? DEFAULT_SINK_LATENCY_SBC : slatency;
            tunedLatency = 110;
            break;
        case CODEC_TYPE_AAC:
            latency += (slatency == 0) ? DEFAULT_SINK_LATENCY_AAC : slatency;
            tunedLatency = 50;
            break;
        case CODEC_TYPE_LDAC:
            latency += (slatency == 0) ? DEFAULT_SINK_LATENCY_LDAC : slatency;
            tunedLatency = 0;
            break;
        case CODEC_TYPE_APTX:
            latency += (slatency == 0) ? DEFAULT_SINK_LATENCY_APTX : slatency;
            tunedLatency = 50;
            break;
        case CODEC_TYPE_APTX_HD:
            latency += (slatency == 0) ? DEFAULT_SINK_LATENCY_APTX_HD : slatency;
            tunedLatency = 0;
            break;
        case CODEC_TYPE_APTX_AD:
        case CODEC_TYPE_LC3:
             tunedLatency = 50;
             break;
        case CODEC_TYPE_APTX_AD_QLEA:
        case CODEC_TYPE_APTX_AD_R4:
            latency += slatency;
            tunedLatency = 0;
            break;
        default:
            latency = DEFAULT_SINK_LATENCY;
            tunedLatency = 0;
            break;
        }
        break;
    case DEC:
        switch (mCodecFormat) {
        case CODEC_TYPE_SBC:
            latency = DEFAULT_SINK_LATENCY_SBC;
            tunedLatency = 0;
            break;
        case CODEC_TYPE_AAC:
            latency = DEFAULT_SINK_LATENCY_AAC;
            tunedLatency = 0;
            break;
        case CODEC_TYPE_LC3:
            latency = slatency;
            tunedLatency = 0;
            break;
        default:
            latency = DEFAULT_SINK_LATENCY;
            tunedLatency = 0;
            break;
        }
        break;
    default:
        break;
    }
    latency += tunedLatency;
    PAL_INFO(LOG_TAG, "codecLatency %d, sink latency:%d, total latency:%d, codec format: 0x%x",
             mCodecLatency, slatency, latency, mCodecFormat);
    return latency;
}

int32_t BtA2dp::getDeviceParameter(uint32_t param_id, void **param)
{
    switch (param_id) {
    case PAL_PARAM_ID_BT_A2DP_RECONFIG:
    case PAL_PARAM_ID_BT_A2DP_RECONFIG_SUPPORTED:
    case PAL_PARAM_ID_BT_A2DP_SUSPENDED:
    case PAL_PARAM_ID_BT_A2DP_CAPTURE_SUSPENDED:
        *param = &mParamBtA2dp;
        break;
    case PAL_PARAM_ID_BT_A2DP_DECODER_LATENCY:
    case PAL_PARAM_ID_BT_A2DP_ENCODER_LATENCY:
    {
        uint32_t slatency = 0;

        if (mA2dpState == A2DP_STATE_STARTED && mTotalActiveSessionRequests &&
            ((mParamBtA2dp.latency == 0) || (mCodecFormat == CODEC_TYPE_APTX_AD) ||
             (mCodecFormat == CODEC_TYPE_APTX_AD_R4))) {
            if (audio_sink_get_a2dp_latency_api) {
                slatency = audio_sink_get_a2dp_latency_api(get_session_type());
            } else if (audio_sink_get_a2dp_latency) {
                slatency = audio_sink_get_a2dp_latency();
            }
            mParamBtA2dp.latency = getLatency(slatency);
        }
        *param = &mParamBtA2dp;
        break;
    }
    case PAL_PARAM_ID_BT_A2DP_FORCE_SWITCH:
    {
        if (mTotalActiveSessionRequests == 0 && deviceStartStopCount) {
            mParamBtA2dp.is_force_switch = true;
            PAL_DBG(LOG_TAG, "Force BT device switch for no total active BT sessions");
        } else {
            mParamBtA2dp.is_force_switch = false;
        }

        *param = &mParamBtA2dp;
        break;
    }
    case PAL_PARAM_ID_LATENCY_MODE:
    {
        int32_t status = 0;
        pal_param_latency_mode_t* param_latency_mode_ptr = (pal_param_latency_mode_t *)*param;
        if (audio_source_get_supported_latency_modes_api) {
            status = audio_source_get_supported_latency_modes_api(get_session_type(),
                        &(param_latency_mode_ptr->num_modes), param_latency_mode_ptr->num_modes,
                        param_latency_mode_ptr->modes);
            if (status) {
                PAL_ERR(LOG_TAG, "get Parameter param id %d failed", param_id);
                return status;
            }
        } else {
            param_latency_mode_ptr->num_modes = 0;
            status = -ENOSYS;
            return status;
        }
        break;
    }
    default:
        return -EINVAL;
    }
    return 0;
}

std::shared_ptr<Device>
BtA2dp::getInstance(struct pal_device *device, std::shared_ptr<ResourceManager> Rm)
{
    if (device->id == PAL_DEVICE_OUT_BLUETOOTH_A2DP) {
        if (!sObjRx) {
            std::lock_guard<std::mutex> lock(Device::mInstMutex);
            if (!sObjRx) {
                PAL_INFO(LOG_TAG, "creating instance for  %d", device->id);
                std::shared_ptr<Device> sp(new BtA2dp(device, Rm));
                sObjRx = sp;
            }
        }
        return sObjRx;
    } else if (device->id == PAL_DEVICE_IN_BLUETOOTH_A2DP) {
        if (!sObjTx) {
            std::lock_guard<std::mutex> lock(Device::mInstMutex);
            if (!sObjTx) {
                PAL_INFO(LOG_TAG, "creating instance for  %d", device->id);
                std::shared_ptr<Device> sp(new BtA2dp(device, Rm));
                sObjTx = sp;
            }
        }
        return sObjTx;
    } else if (device->id == PAL_DEVICE_OUT_BLUETOOTH_BLE) {
        if (!sObjBleRx) {
            std::lock_guard<std::mutex> lock(Device::mInstMutex);
            if (!sObjBleRx) {
                PAL_INFO(LOG_TAG, "creating instance for  %d", device->id);
                std::shared_ptr<Device> sp(new BtA2dp(device, Rm));
                sObjBleRx = sp;
            }
        }
        return sObjBleRx;
    } else if (device->id == PAL_DEVICE_OUT_BLUETOOTH_BLE_BROADCAST) {
        if (!sObjBleBroadcastRx) {
            std::lock_guard<std::mutex> lock(Device::mInstMutex);
            if (!sObjBleBroadcastRx) {
                PAL_INFO(LOG_TAG, "creating instance for  %d", device->id);
                std::shared_ptr<Device> sp(new BtA2dp(device, Rm));
                sObjBleBroadcastRx = sp;
            }
        }
        return sObjBleBroadcastRx;
    } else {
        if (!sObjBleTx) {
            std::lock_guard<std::mutex> lock(Device::mInstMutex);
            if (!sObjBleTx) {
                PAL_INFO(LOG_TAG, "creating instance for  %d", device->id);
                std::shared_ptr<Device> sp(new BtA2dp(device, Rm));
                sObjBleTx = sp;
            }
        }
        return sObjBleTx;
    }
}
int32_t BtA2dp::checkDeviceStatus() {
    if (mA2dpState == A2DP_STATE_DISCONNECTED) {
        PAL_INFO(LOG_TAG, "retry to open a2dp source");
        if (mA2dpRole == SOURCE)
            open_a2dp_source();
        else {
#ifdef A2DP_SINK_SUPPORTED
            open_a2dp_sink();
#else
            mA2dpState = A2DP_STATE_CONNECTED;
#endif
        }
    }
    PAL_DBG(LOG_TAG, "mA2dpState: %d", mA2dpState);
    return mA2dpState;
}

int32_t BtA2dp::getDeviceConfig(struct pal_device *deviceattr,
                                struct pal_stream_attributes *sAttr) {

    deviceattr->config.aud_fmt_id = PAL_AUDIO_FMT_DEFAULT_COMPRESSED;
    return 0;
}

/* Scope of BtScoRX/Tx class */
// definition of static BtSco member variables
std::shared_ptr<Device> BtSco::sObjRx = nullptr;
std::shared_ptr<Device> BtSco::sObjTx = nullptr;
bool BtSco::sIsWbSpeechEnabled = false;
int  BtSco::sSwbSpeechMode = SPEECH_MODE_INVALID;
bool BtSco::sIsSwbLc3Enabled = false;
audio_lc3_codec_cfg_t BtSco::sLc3CodecInfo = {};
bool BtSco::sIsNrecEnabled = false;
std::shared_ptr<Device> BtSco::sObjHfpRx = nullptr;
std::shared_ptr<Device> BtSco::sObjHfpTx = nullptr;

BtSco::BtSco(struct pal_device *device, std::shared_ptr<ResourceManager> Rm)
    : Bluetooth(device, Rm)
{
    mCodecType = (device->id == PAL_DEVICE_OUT_BLUETOOTH_SCO) ? ENC : DEC;
    mPluginHandler = NULL;
    mPluginCodec = NULL;
}

BtSco::~BtSco()
{
    if (sLc3CodecInfo.enc_cfg.streamMapOut != NULL)
        delete [] sLc3CodecInfo.enc_cfg.streamMapOut;
    if (sLc3CodecInfo.dec_cfg.streamMapIn != NULL)
        delete [] sLc3CodecInfo.dec_cfg.streamMapIn;
}

bool BtSco::isDeviceReady(pal_device_id_t id)
{
    bool ret = false;

    if (!rm->isDeviceAvailable(id))
        return ret;

    return mIsScoOn;
}

bool BtSco::isHFPRunning()
{
    return mIsHfpOn;
}

int32_t BtSco::checkAndUpdateSampleRate(uint32_t *sampleRate)
{
    if (sIsWbSpeechEnabled)
        *sampleRate = SAMPLINGRATE_16K;
    else if (sSwbSpeechMode != SPEECH_MODE_INVALID)
        *sampleRate = SAMPLINGRATE_96K;
    else if (sIsSwbLc3Enabled)
        *sampleRate = SAMPLINGRATE_96K;
    else
        *sampleRate = SAMPLINGRATE_8K;

    return 0;
}

int32_t BtSco::setDeviceParameter(uint32_t param_id, void *param)
{
    pal_param_btsco_t* param_bt_sco = (pal_param_btsco_t *)param;

    switch (param_id) {
    case PAL_PARAM_ID_BT_SCO:
        mIsScoOn = param_bt_sco->bt_sco_on;
        mIsHfpOn = param_bt_sco->is_bt_hfp;
        break;
    case PAL_PARAM_ID_BT_SCO_WB:
        sIsWbSpeechEnabled = param_bt_sco->bt_wb_speech_enabled;
        PAL_DBG(LOG_TAG, "sIsWbSpeechEnabled = %d", sIsWbSpeechEnabled);
        break;
    case PAL_PARAM_ID_BT_SCO_SWB:
        sSwbSpeechMode = param_bt_sco->bt_swb_speech_mode;
        PAL_DBG(LOG_TAG, "sSwbSpeechMode = %d", sSwbSpeechMode);
        break;
    case PAL_PARAM_ID_BT_SCO_LC3:
        sIsSwbLc3Enabled = param_bt_sco->bt_lc3_speech_enabled;
        if (sIsSwbLc3Enabled) {
            // parse sco lc3 parameters and pack into codec info
            convertCodecInfo(sLc3CodecInfo, param_bt_sco->lc3_cfg);
        }
        PAL_DBG(LOG_TAG, "sIsSwbLc3Enabled = %d", sIsSwbLc3Enabled);
        break;
    case PAL_PARAM_ID_BT_SCO_NREC:
        sIsNrecEnabled = param_bt_sco->bt_sco_nrec;
        PAL_DBG(LOG_TAG, "sIsNrecEnabled = %d", sIsNrecEnabled);
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

bool BtSco::isScoNbWbActive()
{
    return mCodecFormat == CODEC_TYPE_INVALID;
}

void BtSco::convertCodecInfo(audio_lc3_codec_cfg_t &lc3CodecInfo,
                             btsco_lc3_cfg_t &lc3Cfg)
{
    std::vector<lc3_stream_map_t> steamMapIn;
    std::vector<lc3_stream_map_t> steamMapOut;
    uint32_t audio_location = 0;
    uint8_t stream_id = 0;
    uint8_t direction = 0;
    uint8_t value = 0;
    int idx = 0;
    std::string vendorStr(lc3Cfg.vendor);
    std::string streamMapStr(lc3Cfg.streamMap);
    std::regex vendorPattern("([0-9a-fA-F]{2})[,[:s:]]?");
    std::regex streamMapPattern("([0-9])[,[:s:]]+([0-9])[,[:s:]]+([MLR])");
    std::smatch match;

    // convert and fill in encoder cfg
    lc3CodecInfo.enc_cfg.toAirConfig.sampling_freq        = LC3_CSC[lc3Cfg.txconfig_index].sampling_freq;
    lc3CodecInfo.enc_cfg.toAirConfig.max_octets_per_frame = LC3_CSC[lc3Cfg.txconfig_index].max_octets_per_frame;
    lc3CodecInfo.enc_cfg.toAirConfig.frame_duration       = LC3_CSC[lc3Cfg.txconfig_index].frame_duration;
    lc3CodecInfo.enc_cfg.toAirConfig.bit_depth            = LC3_CSC[lc3Cfg.txconfig_index].bit_depth;
    if (lc3Cfg.fields_map & LC3_FRAME_DURATION_BIT)
        lc3CodecInfo.enc_cfg.toAirConfig.frame_duration   = lc3Cfg.frame_duration;
    lc3CodecInfo.enc_cfg.toAirConfig.api_version          = lc3Cfg.api_version;
    lc3CodecInfo.enc_cfg.toAirConfig.num_blocks           = lc3Cfg.num_blocks;
    lc3CodecInfo.enc_cfg.toAirConfig.default_q_level      = 0;
    lc3CodecInfo.enc_cfg.toAirConfig.mode                 = lc3Cfg.mode;
    lc3CodecInfo.is_enc_config_set                        = true;

    // convert and fill in decoder cfg
    lc3CodecInfo.dec_cfg.fromAirConfig.sampling_freq        = LC3_CSC[lc3Cfg.rxconfig_index].sampling_freq;
    lc3CodecInfo.dec_cfg.fromAirConfig.max_octets_per_frame = LC3_CSC[lc3Cfg.rxconfig_index].max_octets_per_frame;
    lc3CodecInfo.dec_cfg.fromAirConfig.frame_duration       = LC3_CSC[lc3Cfg.rxconfig_index].frame_duration;
    lc3CodecInfo.dec_cfg.fromAirConfig.bit_depth            = LC3_CSC[lc3Cfg.rxconfig_index].bit_depth;
    if (lc3Cfg.fields_map & LC3_FRAME_DURATION_BIT)
        lc3CodecInfo.dec_cfg.fromAirConfig.frame_duration   = lc3Cfg.frame_duration;
    lc3CodecInfo.dec_cfg.fromAirConfig.api_version          = lc3Cfg.api_version;
    lc3CodecInfo.dec_cfg.fromAirConfig.num_blocks           = lc3Cfg.num_blocks;
    lc3CodecInfo.dec_cfg.fromAirConfig.default_q_level      = 0;
    lc3CodecInfo.dec_cfg.fromAirConfig.mode                 = lc3Cfg.mode;
    lc3CodecInfo.is_dec_config_set                          = true;

    // parse vendor specific string
    idx = 15;
    while (std::regex_search(vendorStr, match, vendorPattern)) {
        if (idx < 0) {
            PAL_ERR(LOG_TAG, "wrong vendor info length, string %s", lc3Cfg.vendor);
            break;
        }
        value = (uint8_t)strtol(match[1].str().c_str(), NULL, 16);
        lc3CodecInfo.enc_cfg.toAirConfig.vendor_specific[idx] = value;
        lc3CodecInfo.dec_cfg.fromAirConfig.vendor_specific[idx--] = value;
        vendorStr = match.suffix().str();
    }
    if (idx != -1)
        PAL_ERR(LOG_TAG, "wrong vendor info length, string %s", lc3Cfg.vendor);

    // parse stream map string and append stream map structures
    while (std::regex_search(streamMapStr, match, streamMapPattern)) {
        stream_id = atoi(match[1].str().c_str());
        direction = atoi(match[2].str().c_str());
        if (!strcmp(match[3].str().c_str(), "M")) {
            audio_location = 0;
        } else if (!strcmp(match[3].str().c_str(), "L")) {
            audio_location = 1;
        } else if (!strcmp(match[3].str().c_str(), "R")) {
            audio_location = 2;
        }

        if ((stream_id > 1) || (direction > 1) || (audio_location > 2)) {
            PAL_ERR(LOG_TAG, "invalid stream info (%d, %d, %d)", stream_id, direction, audio_location);
            continue;
        }
        if (direction == TO_AIR)
            steamMapOut.push_back({audio_location, stream_id, direction});
        else
            steamMapIn.push_back({audio_location, stream_id, direction});

        streamMapStr = match.suffix().str();
    }

    PAL_DBG(LOG_TAG, "stream map out size: %zu, stream map in size: %zu", steamMapOut.size(), steamMapIn.size());
    if ((steamMapOut.size() == 0) || (steamMapIn.size() == 0)) {
        PAL_ERR(LOG_TAG, "invalid size steamMapOut.size %zu, steamMapIn.size %zu",
                steamMapOut.size(), steamMapIn.size());
        return;
    }

    idx = 0;
    lc3CodecInfo.enc_cfg.stream_map_size = steamMapOut.size();
    if (lc3CodecInfo.enc_cfg.streamMapOut != NULL)
        delete [] lc3CodecInfo.enc_cfg.streamMapOut;
    lc3CodecInfo.enc_cfg.streamMapOut = new lc3_stream_map_t[steamMapOut.size()];
    for (auto &it : steamMapOut) {
        lc3CodecInfo.enc_cfg.streamMapOut[idx].audio_location = it.audio_location;
        lc3CodecInfo.enc_cfg.streamMapOut[idx].stream_id = it.stream_id;
        lc3CodecInfo.enc_cfg.streamMapOut[idx++].direction = it.direction;
        PAL_DBG(LOG_TAG, "streamMapOut: audio_location %d, stream_id %d, direction %d",
                it.audio_location, it.stream_id, it.direction);
    }

    idx = 0;
    lc3CodecInfo.dec_cfg.stream_map_size = steamMapIn.size();
    if (lc3CodecInfo.dec_cfg.streamMapIn != NULL)
        delete [] lc3CodecInfo.dec_cfg.streamMapIn;
    lc3CodecInfo.dec_cfg.streamMapIn = new lc3_stream_map_t[steamMapIn.size()];
    for (auto &it : steamMapIn) {
        lc3CodecInfo.dec_cfg.streamMapIn[idx].audio_location = it.audio_location;
        lc3CodecInfo.dec_cfg.streamMapIn[idx].stream_id = it.stream_id;
        lc3CodecInfo.dec_cfg.streamMapIn[idx++].direction = it.direction;
        PAL_DBG(LOG_TAG, "steamMapIn: audio_location %d, stream_id %d, direction %d",
                it.audio_location, it.stream_id, it.direction);
    }

    if (lc3CodecInfo.dec_cfg.streamMapIn[0].audio_location == 0)
        lc3CodecInfo.dec_cfg.decoder_output_channel = CH_MONO;
    else
        lc3CodecInfo.dec_cfg.decoder_output_channel = CH_STEREO;
}

int BtSco::startSwb()
{
    int ret = 0;

    if (!mIsConfigured) {
        ret = configureGraphModules();
    } else {
        /* isAbrEnabled flag assignment will be skipped if
         * path is already configured.
         * Force isAbrEnabled flag for SWB use case. Ideally,
         * this flag should be populated from plugin.
         */
        if (mCodecFormat == CODEC_TYPE_APTX_AD_SPEECH)
            mIsAbrEnabled = true;
    }

    return ret;
}

int BtSco::start()
{
    int status = 0;
    mDeviceMutex.lock();

    if (customPayload)
        free(customPayload);

    customPayload = NULL;
    customPayloadSize = 0;

    if (sSwbSpeechMode != SPEECH_MODE_INVALID) {
        mCodecFormat = CODEC_TYPE_APTX_AD_SPEECH;
        mCodecInfo = (void *)&sSwbSpeechMode;
    } else if (sIsSwbLc3Enabled) {
        mCodecFormat = CODEC_TYPE_LC3;
        mCodecInfo = (void *)&sLc3CodecInfo;
    } else {
        mCodecFormat = CODEC_TYPE_INVALID;
        mIsAbrEnabled = false;
    }

    updateDeviceMetadata();
    if ((mCodecFormat == CODEC_TYPE_APTX_AD_SPEECH) ||
        (mCodecFormat == CODEC_TYPE_LC3)) {
        status = startSwb();
        if (status)
            goto exit;
    } else {
        // For SCO NB and WB that don't have encoder and decoder in place,
        // just override codec configurations with device attributions.
        mCodecConfig.bit_width = deviceAttr.config.bit_width;
        mCodecConfig.sample_rate = deviceAttr.config.sample_rate;
        mCodecConfig.aud_fmt_id = PAL_AUDIO_FMT_DEFAULT_PCM;
        mCodecConfig.ch_info.channels = deviceAttr.config.ch_info.channels;
        mIsConfigured = true;
        PAL_DBG(LOG_TAG, "SCO WB/NB codecConfig is same as deviceAttr bw = %d,sr = %d,ch = %d",
            mCodecConfig.bit_width, mCodecConfig.sample_rate, mCodecConfig.ch_info.channels);
    }

    // Configure NREC only on Tx path & First session request only.
    if ((mIsConfigured == true) &&
        (deviceAttr.id == PAL_DEVICE_IN_BLUETOOTH_SCO_HEADSET)) {
        if (deviceStartStopCount == 0) {
            configureNrecParameters(sIsNrecEnabled);
        }
    }

    if (deviceStartStopCount == 0) {
        status = configureSlimbusClockSrc();
        if (status)
            goto exit;
    }

    status = Device::start_l();
    if (customPayload) {
        free(customPayload);
        customPayload = NULL;
        customPayloadSize = 0;
    }
    if (!status && mIsAbrEnabled)
        startAbr();

exit:
    mDeviceMutex.unlock();
    return status;
}

int BtSco::stop()
{
    int status = 0;
    mDeviceMutex.lock();

    if (mIsAbrEnabled)
        stopAbr();

    if (mPluginCodec) {
        mPluginCodec->close_plugin(mPluginCodec);
        mPluginCodec = NULL;
    }
    if (mPluginHandler) {
        dlclose(mPluginHandler);
        mPluginHandler = NULL;
    }

    Device::stop_l();
    if (mIsAbrEnabled == false)
        mCodecFormat = CODEC_TYPE_INVALID;
    if (deviceStartStopCount == 0)
        mIsConfigured = false;

    mDeviceMutex.unlock();
    return status;
}

std::shared_ptr<Device> BtSco::getInstance(struct pal_device *device,
                                           std::shared_ptr<ResourceManager> Rm)
{
    if (device->id == PAL_DEVICE_OUT_BLUETOOTH_SCO) {
        if (!sObjRx) {
            std::lock_guard<std::mutex> lock(Device::mInstMutex);
            if (!sObjRx) {
                std::shared_ptr<Device> sp(new BtSco(device, Rm));
                sObjRx = sp;
            }
        }
        return sObjRx;
    } else if (device->id == PAL_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
        if (!sObjTx) {
            std::lock_guard<std::mutex> lock(Device::mInstMutex);
            if (!sObjTx) {
                PAL_DBG(LOG_TAG,  "creating instance for  %d", device->id);
                std::shared_ptr<Device> sp(new BtSco(device, Rm));
                sObjTx = sp;
            }
        }
        return sObjTx;
    } else if (device->id == PAL_DEVICE_OUT_BLUETOOTH_HFP) {
        if (!sObjHfpRx) {
            std::lock_guard<std::mutex> lock(Device::mInstMutex);
            if (!sObjHfpRx) {
                PAL_DBG(LOG_TAG, "creating instance for  %d", device->id);
                std::shared_ptr<Device> sp(new BtSco(device, Rm));
                sObjHfpRx = sp;
            }
        }
        return sObjHfpRx;
    } else if (device->id == PAL_DEVICE_IN_BLUETOOTH_HFP) {
        if (!sObjHfpTx) {
            std::lock_guard<std::mutex> lock(Device::mInstMutex);
            if (!sObjHfpTx) {
                PAL_DBG(LOG_TAG, "creating instance for  %d", device->id);
                std::shared_ptr<Device> sp(new BtSco(device, Rm));
                sObjHfpTx = sp;
            }
        }
        return sObjHfpTx;
    }
    return nullptr;
}

int32_t BtSco::getDeviceConfig(struct pal_device *deviceattr,
                                struct pal_stream_attributes *sAttr) {

    this->checkAndUpdateSampleRate(&deviceattr->config.sample_rate);
    PAL_DBG(LOG_TAG, "BT SCO device samplerate %d, bitwidth %d",
            deviceattr->config.sample_rate, deviceattr->config.bit_width);
    return 0;
}
/* BtSco class end */
