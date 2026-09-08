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
 *
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: SessionAR"

#include "SessionAR.h"
#include "Stream.h"
#include "ResourceManager.h"
#include "PluginManager.h"
#include "SessionAlsaUtils.h"
#include "PayloadBuilder.h"
#include <agm/agm_api.h>
#include "apm_api.h"
#include <sstream>

struct pcm *SessionAR::pcmEcTx = NULL;
std::vector<int> SessionAR::pcmDevEcTxIds = {0};
int SessionAR::extECRefCnt = 0;
std::mutex SessionAR::extECMutex;
std::mutex SessionAR::pauseMutex;
std::condition_variable SessionAR::pauseCV;
std::mutex SessionAR::rmCookieRegistryMutex;
std::vector<SessionAR::SessionArRmCookie*> SessionAR::validRmCookies;

SessionAR::SessionAR() {
    int32_t ret = PayloadBuilder::init();
    if (ret) {
        throw std::runtime_error("Failed to parse usecase manager xml");
    } else {
        PAL_INFO(LOG_TAG, "usecase manager xml parsing successful");
    }
}

void SessionAR::handleSoftPauseCallBack(uint64_t hdl, uint32_t event_id,
                                        void *data __unused,
                                        uint32_t event_size __unused)
{
    PAL_DBG(LOG_TAG,"Event id %x ", event_id);

    if (event_id == EVENT_ID_SOFT_PAUSE_PAUSE_COMPLETE) {
        PAL_DBG(LOG_TAG, "Pause done");
        pauseCV.notify_all();
    }
}

void SessionAR::setPmQosMixerCtl(pmQosVote vote)
{
    int status = 0;
    struct audio_route *audioRoute;

    status = rm->getAudioRoute(&audioRoute);
    if (!status) {
        if (vote == PM_QOS_VOTE_DISABLE) {
            audio_route_reset_and_update_path(audioRoute, "PM_QOS_Vote");
            PAL_DBG(LOG_TAG,"mixer control disabled for PM_QOS Vote \n");
        } else if (vote == PM_QOS_VOTE_ENABLE) {
            audio_route_apply_and_update_path(audioRoute, "PM_QOS_Vote");
            PAL_DBG(LOG_TAG,"mixer control enabled for PM_QOS Vote \n");
        }
    } else {
        PAL_ERR(LOG_TAG,"could not get audioRoute, not setting mixer control for PM_QOS \n");
    }
}

uint32_t SessionAR::getModuleInfo(const char *control, uint32_t tagId, uint32_t *miid, struct mixer_ctl **ctl, int *device)
{
    int status = 0;
    int dev = 0;
    struct mixer_ctl *mixer_ctl = NULL;

    if (!rxAifBackEnds.empty()) { /** search in RX GKV */
        mixer_ctl = getFEMixerCtl(control, &dev, PAL_AUDIO_OUTPUT);
        if (!mixer_ctl) {
            PAL_ERR(LOG_TAG, "Invalid mixer control\n");
            status = -ENOENT;
            goto exit;
        }
        for (int i = 0; i < rxAifBackEnds.size(); i++) {
            status = SessionAlsaUtils::getModuleInstanceId(
                    mixer, dev, rxAifBackEnds[i].second.data(), tagId, miid);
            if (status) { /** if not found, reset miid to 0 again */
                *miid = 0;
            } else {
                break;
            }
        }
    }

    if (!txAifBackEnds.empty() && !(*miid)) { /** search in TX GKV */
        mixer_ctl = getFEMixerCtl(control, &dev, PAL_AUDIO_INPUT);
        if (!mixer_ctl) {
            PAL_ERR(LOG_TAG, "Invalid mixer control\n");
            status = -ENOENT;
            goto exit;
        }
        for (int i = 0; i < txAifBackEnds.size(); i++) {
            status = SessionAlsaUtils::getModuleInstanceId(
                    mixer, dev, txAifBackEnds[i].second.data(), tagId, miid);
            if (status) { /** if not found, reset miid to 0 again */
                *miid = 0;
            } else {
                break;
            }
        }
    }

    if (*miid == 0) {
        PAL_ERR(LOG_TAG, "failed to look for module with tagID 0x%x", tagId);
        status = -EINVAL;
        goto exit;
    }

    if (device)
        *device = dev;

    if (ctl)
        *ctl = mixer_ctl;

    PAL_DBG(LOG_TAG, "got miid = 0x%04x, device = %d", *miid, dev);
exit:
    if (status) {
        if (device)
            *device = 0;
        if (ctl)
            *ctl = NULL;
        *miid = 0;
        PAL_ERR(LOG_TAG, "Exit. status %d", status);
    }
    return status;
}

int SessionAR::setEffectParametersTKV(Stream *s __unused, effect_pal_payload_t *effectPayload)
{
    int status = 0;
    int device = 0;
    uint32_t tag;
    uint32_t nTkvs;
    uint32_t tagConfigSize;
    struct mixer_ctl *ctl;
    pal_key_vector_t *palKVPair;
    struct agm_tag_config* tagConfig = NULL;
    std::vector <std::pair<int, int>> tkv;
    const char *control = "setParamTag";

    PAL_DBG(LOG_TAG, "Enter.");

    if (!effectPayload) {
        status = -EINVAL;
        goto exit;
    }

    palKVPair = (pal_key_vector_t *)effectPayload->payload;

    if (!palKVPair) {
        status = -EINVAL;
        goto exit;
    }

    nTkvs =  palKVPair->num_tkvs;
    tkv.clear();
    for (int i = 0; i < nTkvs; i++) {
        tkv.push_back(std::make_pair(palKVPair->kvp[i].key, palKVPair->kvp[i].value));
    }
    if (tkv.size() == 0) {
        status = -EINVAL;
        goto exit;
    }

    tagConfigSize = sizeof(struct agm_tag_config) + (tkv.size() * sizeof(agm_key_value));
    tagConfig = (struct agm_tag_config *) malloc(tagConfigSize);
    if(!tagConfig) {
        status = -ENOMEM;
        goto exit;
    }

    tag = effectPayload->tag;
    status = SessionAlsaUtils::getTagMetadata(tag, tkv, tagConfig);
    if (0 != status) {
        goto exit;
    }

    /* Prepare mixer control */
    ctl = getFEMixerCtl(control, &device, PAL_AUDIO_OUTPUT);
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Invalid mixer control\n");
        status = -ENOENT;
        goto exit;
    }

    status = mixer_ctl_set_array(ctl, tagConfig, tagConfigSize);
    if (status != 0) {
        PAL_DBG(LOG_TAG, "Unable to set TKV in Rx path, trying in Tx\n");
        /* Rx set failed, Try on Tx path we well */
        ctl = getFEMixerCtl(control, &device, PAL_AUDIO_INPUT);
        if (!ctl) {
            PAL_ERR(LOG_TAG, "Invalid mixer control\n");
            status = -ENOENT;
            goto exit;
        }

        status = mixer_ctl_set_array(ctl, tagConfig, tagConfigSize);
        if (status != 0) {
            PAL_ERR(LOG_TAG, "failed to set the param %d", status);
            goto exit;
        }
    }

exit:
    ctl = NULL;

    if (tagConfig) {
        free(tagConfig);
        tagConfig = NULL;
    }
    PAL_INFO(LOG_TAG, "mixer set tkv status = %d\n", status);
    return status;
}

int SessionAR::setEffectParametersNonTKV(Stream *s __unused, effect_pal_payload_t *effectPayload)
{
    int status = 0;
    int device = 0;
    PayloadBuilder builder;

    uint32_t miid = 0;
    const char *control = "setParam";
    size_t payloadSize = 0;
    uint8_t *payloadData = NULL;
    pal_effect_custom_payload_t *effectCustomPayload = nullptr;

    PAL_DBG(LOG_TAG, "Enter.");

    if (!effectPayload) {
        PAL_ERR(LOG_TAG, "Invalid effectPayload address.\n");
        return -EINVAL;
    }

    /* This is set param call, find out miid first */
    status = getModuleInfo(control, effectPayload->tag, &miid, NULL, &device);
    if (status || !miid) {
        PAL_ERR(LOG_TAG, "failed to look for module with tagID 0x%x, status = %d",
                    effectPayload->tag, status);
        return -EINVAL;
    }

    /* Now we got the miid, build set param payload */
    effectCustomPayload = (pal_effect_custom_payload_t *)effectPayload->payload;
    status = builder.payloadCustomParam(&payloadData, &payloadSize,
            effectCustomPayload->data,
            effectPayload->payloadSize - sizeof(uint32_t),
            miid, effectCustomPayload->paramId);
    if (status != 0) {
        PAL_ERR(LOG_TAG, "payloadCustomParam failed. status = %d",
                status);
        goto exit;
    }
    /* set param through set mixer param */
    status = SessionAlsaUtils::setMixerParameter(mixer,
            device,
            payloadData,
            payloadSize);
    PAL_INFO(LOG_TAG, "mixer set param status = %d\n", status);

exit:
    if (payloadData) {
        free(payloadData);
        payloadData = NULL;
    }
    if (status && effectCustomPayload) {
        PAL_ERR(LOG_TAG, "setEffectParameters for param_id %d failed, status = %d",
                effectCustomPayload->paramId, status);
    }
    return status;

}

int SessionAR::setEffectParameters(Stream *s, effect_pal_payload_t *effectPayload)
{
    int status = 0;

    PAL_DBG(LOG_TAG, "Enter.");

    /* Identify whether this is tkv or set param call */
    if (effectPayload->isTKV) {
        /* This is tkv set call */
        status = setEffectParametersTKV(s, effectPayload);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Get setEffectParameters with TKV payload failed"
                                ", status = %d", status);
            goto exit;
        }
    } else {
        status = setEffectParametersNonTKV(s, effectPayload);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Get setEffectParameters with non TKV payload failed"
                                ", status = %d", status);
            goto exit;
        }
    }

exit:
    if (status)
        PAL_ERR(LOG_TAG, "Exit. status %d", status);

    return status;
}

int SessionAR::getEffectParameters(Stream *s __unused, effect_pal_payload_t *effectPayload)
{
    int status = 0;
    uint8_t *ptr = NULL;

    uint8_t *payloadData = NULL;
    size_t payloadSize = 0;
    uint32_t miid = 0;
    const char *control = "getParam";
    struct mixer_ctl *ctl = NULL;
    pal_effect_custom_payload_t *effectCustomPayload = nullptr;
    PayloadBuilder builder;

    PAL_DBG(LOG_TAG, "Enter.");

    status = getModuleInfo(control, effectPayload->tag, &miid, &ctl, NULL);
    if (status || !miid) {
        PAL_ERR(LOG_TAG, "failed to look for module with tagID 0x%x, status = %d",
                    effectPayload->tag, status);
        status = -EINVAL;
        goto exit;
    }

    effectCustomPayload = (pal_effect_custom_payload_t *)(effectPayload->payload);
    if (effectPayload->payloadSize < sizeof(pal_effect_custom_payload_t)) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "memory for retrieved data is too small");
        goto exit;
    }

    builder.payloadQuery(&payloadData, &payloadSize,
                            miid, effectCustomPayload->paramId,
                            effectPayload->payloadSize - sizeof(uint32_t));
    status = mixer_ctl_set_array(ctl, payloadData, payloadSize);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Set custom config failed, status = %d", status);
        goto exit;
    }

    status = mixer_ctl_get_array(ctl, payloadData, payloadSize);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Get custom config failed, status = %d", status);
        goto exit;
    }

    ptr = (uint8_t *)payloadData + sizeof(struct apm_module_param_data_t);
    ar_mem_cpy(effectCustomPayload->data, effectPayload->payloadSize,
                        ptr, effectPayload->payloadSize);

exit:
    ctl = NULL;
    if (payloadData)
        free(payloadData);
    PAL_ERR(LOG_TAG, "Exit. status %d", status);
    return status;
}

int SessionAR::checkAndSetExtEC(const std::shared_ptr<ResourceManager>& rm,
                              Stream *s, bool is_enable)
{
    struct pcm_config config;
    struct pal_stream_attributes sAttr = {};
    int32_t status = 0;
    std::shared_ptr<Device> dev = nullptr;
    std::vector <std::shared_ptr<Device>> extEcTxDeviceList;
    int32_t extEcbackendId;
    std::vector <std::string> extEcbackendNames;
    struct pal_device device = {};

    PAL_DBG(LOG_TAG, "Enter.");

    extECMutex.lock();
    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"stream get attributes failed");
        status = -EINVAL;
        goto exit;
    }

    device.id = PAL_DEVICE_IN_EXT_EC_REF;
    rm->getDeviceConfig(&device, &sAttr);
    dev = Device::getInstance(&device, rm);
    if (!dev) {
        PAL_ERR(LOG_TAG, "dev get instance failed");
        status = -EINVAL;
        goto exit;
    }

    if(!is_enable) {
        if (extECRefCnt > 0)
            extECRefCnt --;
        if (extECRefCnt == 0) {
            if (pcmEcTx) {
                status = pcm_stop(pcmEcTx);
                if (status) {
                    PAL_ERR(LOG_TAG, "pcm_stop - ec_tx failed %d", status);
                }
                dev->stop();

                status = pcm_close(pcmEcTx);
                if (status) {
                    PAL_ERR(LOG_TAG, "pcm_close - ec_tx failed %d", status);
                }
                dev->close();

                rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
                pcmEcTx = NULL;
                ecRefDevId = PAL_DEVICE_OUT_MIN;
                extECMutex.unlock();
                rm->restoreInternalECRefs();
                extECMutex.lock();
            }
        }
    } else {
        extECRefCnt ++;
        if (extECRefCnt == 1) {
            extECMutex.unlock();
            rm->disableInternalECRefs(s);
            extECMutex.lock();
            extEcTxDeviceList.push_back(dev);
            pcmDevEcTxIds = rm->allocateFrontEndExtEcIds();
            if (pcmDevEcTxIds.size() == 0) {
                PAL_ERR(LOG_TAG, "ResourceManger::getBackEndNames returned no EXT_EC device Ids");
                status = -EINVAL;
                goto exit;
            }
            status = dev->open();
            if (0 != status) {
                PAL_ERR(LOG_TAG, "dev open failed");
                status = -EINVAL;
                rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
                goto exit;
            }
            status = dev->start();
            if (0 != status) {
                PAL_ERR(LOG_TAG, "dev start failed");
                dev->close();
                rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
                status = -EINVAL;
                goto exit;
            }

            extEcbackendId = extEcTxDeviceList[0]->getSndDeviceId();
            extEcbackendNames = rm->getBackEndNames(extEcTxDeviceList);
            status = SessionAlsaUtils::openDev(rm, pcmDevEcTxIds, extEcbackendId,
                extEcbackendNames.at(0).c_str());
            if (0 != status) {
                PAL_ERR(LOG_TAG, "SessionAlsaUtils::openDev failed");
                dev->stop();
                dev->close();
                rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
                status = -EINVAL;
                goto exit;
            }
            pcmEcTx = pcm_open(rm->getVirtualSndCard(), pcmDevEcTxIds.at(0), PCM_IN, &config);
            if (!pcmEcTx) {
                PAL_ERR(LOG_TAG, "Exit pcm-ec-tx open failed");
                dev->stop();
                dev->close();
                rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
                status = -EINVAL;
                goto exit;
            }

            if (!pcm_is_ready(pcmEcTx)) {
                PAL_ERR(LOG_TAG, "Exit pcm-ec-tx open not ready");
                pcmEcTx = NULL;
                dev->stop();
                dev->close();
                rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
                status = -EINVAL;
                goto exit;
            }

            status = pcm_start(pcmEcTx);
            if (status) {
                PAL_ERR(LOG_TAG, "pcm_start ec_tx failed %d", status);
                pcm_close(pcmEcTx);
                pcmEcTx = NULL;
                dev->stop();
                dev->close();
                rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
                status = -EINVAL;
                goto exit;
            }
        }
    }

exit:
    if (is_enable && status) {
        PAL_DBG(LOG_TAG, "Reset extECRefCnt as EXT EC graph fails to setup");
        extECRefCnt = 0;
    }
    extECMutex.unlock();
    PAL_DBG(LOG_TAG, "Exit.");
    return status;
}

/*
 Handle case when charging going is on and PB starts on speaker.
 Below steps are to ensure HW transition from charging->boost->charging
 which will avoid USB collapse due to HW transition in less moment.
 1. Set concurrency bit to update charger driver for respective session
 2. Enable Speaker boost when Audio done voting as part of PCM_open
 3. Set ICL config in device:Speaker module.
*/
int SessionAR::NotifyChargerConcurrency(std::shared_ptr<ResourceManager>rm, bool state)
{
    int status = -EINVAL;

    if (!rm)
        goto exit;

    PAL_DBG(LOG_TAG, "Enter concurrency state %d Notify state %d \n",
            rm->getConcurrentBoostState(), state);

    rm->lockChargerBoostMutex();
    if (rm->getChargerOnlineState()) {
        if (rm->getConcurrentBoostState() ^ state)
            status = rm->chargerListenerSetBoostState(state, CHARGER_ON_PB_STARTS);

        if (0 != status)
            PAL_ERR(LOG_TAG, "Failed to notify PMIC: %d", status);
    }

    PAL_DBG(LOG_TAG, "Exit concurrency state %d with status %d",
            rm->getConcurrentBoostState(), status);
   rm->unlockChargerBoostMutex();
exit:
    return status;
}

/* Handle case when charging going on and PB starts on speaker*/
int SessionAR::EnableChargerConcurrency(std::shared_ptr<ResourceManager>rm, Stream *s)
{
    int status = -EINVAL;

    if (!rm)
        goto exit;

    PAL_DBG(LOG_TAG, "Enter concurrency state %d", rm->getConcurrentBoostState());

    if ((s && rm->getChargerOnlineState()) &&
        (rm->getConcurrentBoostState())) {
         status = rm->setSessionParamConfig(PAL_PARAM_ID_CHARGER_STATE, s,
                                            true);
        if (0 != status) {
            PAL_DBG(LOG_TAG, "Set SessionParamConfig with status %d", status);
            status = rm->chargerListenerSetBoostState(false, CHARGER_ON_PB_STARTS);
            if (0 != status)
                PAL_ERR(LOG_TAG, "Failed to notify PMIC: %d", status);
        }
    }
    PAL_DBG(LOG_TAG, "Exit concurrency state %d with status %d",
            rm->getConcurrentBoostState(), status);
exit:
    return status;
}

int32_t SessionAR::setInitialVolume() {
    int32_t status = 0;
    struct volume_set_param_info vol_set_param_info = {};
    uint16_t volSize = 0;
    uint8_t *volPayload = nullptr;
    struct pal_stream_attributes sAttr = {};
    bool isStreamAvail = false;
    struct pal_vol_ctrl_ramp_param ramp_param = {};
    Session *session = NULL;
    bool forceSetParameters = false;

    PAL_DBG(LOG_TAG, "Enter status: %d", status);

    if (!streamHandle) {
        PAL_ERR(LOG_TAG, "streamHandle is invalid");
        goto exit;
    }
    status = streamHandle->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG, "stream get attributes failed");
        goto exit;
    }

    for (int32_t i = 0; streamHandle->mVolumeData &&
        i < (streamHandle->mVolumeData->no_of_volpair); i++) {
        if((i > 0) &&
            (abs(streamHandle->mVolumeData->volume_pair[0].vol -
                streamHandle->mVolumeData->volume_pair[i].vol) > VOLUME_TOLERANCE)) {
                forceSetParameters = true;
                break;
        }
    }

    memset(&vol_set_param_info, 0, sizeof(struct volume_set_param_info));
    rm->getVolumeSetParamInfo(&vol_set_param_info);
    isStreamAvail = (find(vol_set_param_info.streams_.begin(),
                vol_set_param_info.streams_.end(), sAttr.type) !=
                vol_set_param_info.streams_.end());
    if ((isStreamAvail && vol_set_param_info.isVolumeUsingSetParam) || forceSetParameters) {
        if (sAttr.direction == PAL_AUDIO_OUTPUT) {
           /* DSP default volume is highest value, non-0 rampping period
            * brings volume burst from highest amplitude to new volume
            * at the begining, that makes pop noise heard.
            * set ramp period to 0 ms before pcm_start only for output,
            * so desired volume can take effect instantly at the begining.
            */
            ramp_param.ramp_period_ms = 0;
            status = setParamWithTag(streamHandle, TAG_STREAM_VOLUME,
                                   PAL_PARAM_ID_VOLUME_CTRL_RAMP, &ramp_param);
        }
        // apply if there is any cached volume
        if (streamHandle->mVolumeData) {
            volSize = (sizeof(struct pal_volume_data) +
                      (sizeof(struct pal_channel_vol_kv) *
                      (streamHandle->mVolumeData->no_of_volpair)));
            volPayload = new uint8_t[sizeof(pal_param_payload) +
                volSize]();
            pal_param_payload *pld = (pal_param_payload *)volPayload;
            pld->payload_size = sizeof(struct pal_volume_data);
            memcpy(pld->payload, streamHandle->mVolumeData, volSize);
            status = setParamWithTag(streamHandle, TAG_STREAM_VOLUME,
                    PAL_PARAM_ID_VOLUME_USING_SET_PARAM, (void *)pld);
            delete[] volPayload;
        }
        if (sAttr.direction == PAL_AUDIO_OUTPUT) {
            //set ramp period back to default.
            ramp_param.ramp_period_ms = DEFAULT_RAMP_PERIOD;
            status = setParamWithTag(streamHandle, TAG_STREAM_VOLUME,
                                   PAL_PARAM_ID_VOLUME_CTRL_RAMP, &ramp_param);
        }
    } else {
        // Setting the volume as in stream open, no default volume is set.
        if (sAttr.type != PAL_STREAM_ACD &&
            sAttr.type != PAL_STREAM_VOICE_UI &&
            sAttr.type != PAL_STREAM_CONTEXT_PROXY &&
            sAttr.type != PAL_STREAM_ULTRASOUND &&
            sAttr.type != PAL_STREAM_SENSOR_PCM_DATA &&
            sAttr.type != PAL_STREAM_HAPTICS &&
            sAttr.type != PAL_STREAM_COMMON_PROXY) {

            if (setConfig(streamHandle, CALIBRATION, VOLUME_LVL) != 0) {
                PAL_ERR(LOG_TAG,"Setting volume failed");
            }
        }
    }

exit:
    PAL_DBG(LOG_TAG, "Exit status: %d", status);
    return status;
}

int SessionAR::rwACDBParamTunnel(custom_payload_uc_info_t* uc_info, void *payload,
                                  Stream * s, bool isWrite)
{
    int status = -EINVAL;

    PAL_DBG(LOG_TAG, "Enter");

    PAL_INFO(LOG_TAG, "PAL device id=0x%x", uc_info->pal_device_id);
    status = SessionAlsaUtils::rwACDBTunnel(s, rm, uc_info->pal_device_id, payload, isWrite, uc_info->instance_id);
    if (status) {
        PAL_ERR(LOG_TAG, "session alsa open failed with %d", status);
    }

exit:
    PAL_DBG(LOG_TAG, "Exit status: %d", status);
    return status;
}

int SessionAR::HDRConfigKeyToDevOrientation(const char* hdr_custom_key)
{
    if (!strcmp(hdr_custom_key, "unprocessed-hdr-mic-portrait"))
        return ORIENTATION_0;
    else if (!strcmp(hdr_custom_key, "unprocessed-hdr-mic-landscape"))
        return ORIENTATION_90;
    else if (!strcmp(hdr_custom_key, "unprocessed-hdr-mic-inverted-portrait"))
        return ORIENTATION_180;
    else if (!strcmp(hdr_custom_key, "unprocessed-hdr-mic-inverted-landscape"))
        return ORIENTATION_270;

    PAL_DBG(LOG_TAG,"unknown device orientation %s for HDR record",hdr_custom_key);
    return ORIENTATION_0;
}

int32_t SessionAR::getParameters(Stream *s __unused, uint32_t param_id, void **payload)
{
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "get parameter %u", param_id);
    switch (param_id) {
        case PAL_PARAM_ID_SVA_WAKEUP_MODULE_VERSION:
        {
            status = this->getParamWithTag(nullptr, INVALID_TAG, param_id, payload);
            if (status)
                PAL_ERR(LOG_TAG, "Error:getParam failed with %d",
                    status);
            break;
        }
        case PAL_PARAM_ID_DIRECTION_OF_ARRIVAL:
            status = this->getParamWithTag(nullptr, TAG_ECNS, param_id, payload);
            break;
        default:
            PAL_ERR(LOG_TAG, "Error:Unsupported param id %u", param_id);
            status = -EINVAL;
            break;
    }

    PAL_DBG(LOG_TAG, "get parameter status %d", status);
    return status;
}

int SessionAR::setParameters(Stream *s, uint32_t param_id, void *payload)
{
    int32_t status = 0;
    int32_t setConfigStatus = 0;
    pal_param_payload *param_payload = NULL;
    pal_device_mute_t *deviceMutePayload = nullptr;
    struct pal_stream_attributes sAttr = {};
    struct pal_device dAttr = {};
    std::vector<std::shared_ptr<Device>> associatedDevices;
    void* plugin = nullptr;
    PluginConfig pluginConfig = nullptr;

    PAL_DBG(LOG_TAG, "set parameter %u", param_id);

    switch (param_id) {
        case PAL_PARAM_ID_VOLUME_USING_SET_PARAM:
            status = this->setParamWithTag(s, TAG_STREAM_VOLUME, param_id, payload);
            break;
        case PAL_PARAM_ID_TTY_MODE:
        {
            if (!payload) {
                PAL_ERR(LOG_TAG, "payload is null");
                return -EINVAL;
            }
            param_payload = (pal_param_payload *)payload;
            if (param_payload->payload_size > sizeof(uint32_t)) {
                PAL_ERR(LOG_TAG, "Invalid payload size %d", param_payload->payload_size);
                status = -EINVAL;
                break;
            }
            uint32_t tty_mode = *((uint32_t *)param_payload->payload);
            status = this->setParamWithTag(s, TTY_MODE, param_id, payload);
            if (status)
               PAL_ERR(LOG_TAG, "setParamWithTag for tty mode %d failed with %d",
                       tty_mode, status);
            break;
        }
        case PAL_PARAM_ID_DEVICE_ROTATION:
        {
            try {
                status = s->getStreamAttributes(&sAttr);
                pm = PluginManager::getInstance();
                if(!pm) {
                    PAL_ERR(LOG_TAG, "unable to get plugin manager instance");
                    goto exit;
                }
                status = pm->openPlugin(PAL_PLUGIN_MANAGER_CONFIG, streamNameLUT.at(sAttr.type), plugin);
                if (plugin && !status) {
                    pluginConfig = reinterpret_cast<PluginConfig>(plugin);
                    SetParamPluginPayload ppld;
                    ppld.paramId = param_id;
                    ppld.session = this;
                    ppld.builder = reinterpret_cast<void*>(builder);
                    ppld.payload = payload;
        //call setparam plugin
                    status = pluginConfig(s, PAL_PLUGIN_CONFIG_SETPARAM, reinterpret_cast<void*>(&ppld), sizeof(ppld));
                    if (0 != status) {
                        PAL_ERR(LOG_TAG, "PLUGIN_CONFIG_SETPARAM failed");
                        goto exit;
                    }
                } else {
                    PAL_ERR(LOG_TAG, "unable to get plugin for stream type %s", streamNameLUT.at(sAttr.type).c_str());
                }
            } catch (const std::exception& e) {
                throw std::runtime_error(e.what());
            }
            break;
        }
        case PAL_PARAM_ID_VOLUME_BOOST:
        {
            status = this->setParamWithTag(s, VOICE_VOLUME_BOOST, param_id, payload);
            if (status)
               PAL_ERR(LOG_TAG, "setParamWithTag for volume boost failed with %d",
                       status);
            break;
        }
        case PAL_PARAM_ID_SLOW_TALK:
        {
            bool slow_talk = false;
            param_payload = (pal_param_payload *)payload;
            slow_talk = *((bool *)param_payload->payload);

            uint32_t slow_talk_tag =
                          slow_talk ? VOICE_SLOW_TALK_ON : VOICE_SLOW_TALK_OFF;
            status = this->setParamWithTag(s, slow_talk_tag,
                                         param_id, payload);
            if (status)
               PAL_ERR(LOG_TAG, "setParamWithTag for slow talk failed with %d",
                       status);
            break;
        }
        case PAL_PARAM_ID_DEVICE_MUTE:
        {
            param_payload = (pal_param_payload *)payload;
            deviceMutePayload = (pal_device_mute_t *)(param_payload->payload);
            status = this->setParamWithTag(s, DEVICE_MUTE,
                                            param_id, payload);
            if (status) {
               PAL_ERR(LOG_TAG, "setParam for device mute failed with %d",
                       status);
            } else {
               s->setDeviceMute(deviceMutePayload->dir, deviceMutePayload->mute);
            }
            break;
        }
        case PAL_PARAM_ID_HD_VOICE:
        {
            status = this->setParamWithTag(s, VOICE_HD_VOICE, param_id, payload);
            if (status)
               PAL_ERR(LOG_TAG, "setParamWithTag for hd voice failed with %d",
                       status);
            break;
        }
        case PAL_PARAM_ID_VOLUME_CTRL_RAMP:
            status = this->setParamWithTag(s, TAG_STREAM_VOLUME,
                                        param_id,
                                        payload);
            break;
        /*default send to derived session with no tag*/
        case PAL_PARAM_ID_BT_A2DP_TWS_CONFIG:
        case PAL_PARAM_ID_BT_A2DP_LC3_CONFIG:
            status = this->setParamWithTag(s, BT_PLACEHOLDER_ENCODER, param_id, payload);
            break;
        case PAL_PARAM_ID_MSPP_LINEAR_GAIN:
            status = this->setParamWithTag(s, TAG_MODULE_MSPP,
                               param_id, payload);
            break;
        case PAL_PARAM_ID_ULTRASOUND_RAMPDOWN:
            status = this->setParamWithTag(s, DEVICE_POP_SUPPRESSOR,
                                param_id, NULL);
            break;
        case PAL_PARAM_ID_CHARGER_STATE:
        {
            bool enable = *((bool *) payload);
            int tag = enable ? CHARGE_CONCURRENCY_ON_TAG
                      : CHARGE_CONCURRENCY_OFF_TAG;
            setConfigStatus = this->setConfig(s, MODULE, tag);
            if (setConfigStatus) {
                PAL_INFO(LOG_TAG, "Charger state setConfig failed.");
            }
            break;
        }
        case PAL_PARAM_ID_GAIN_LVL_CAL:
            setConfigStatus = this->setConfig(s, CALIBRATION, GAIN_LVL);
            if (setConfigStatus) {
                PAL_INFO(LOG_TAG, "DevicePP MBDRC Gain setConfig failed.");
            }
            break;
        case PAL_PARAM_ID_ORIENTATION:
            setConfigStatus = this->setConfig(s, MODULE, ORIENTATION_TAG);
            if (setConfigStatus) {
                PAL_INFO(LOG_TAG, "Orientation setConfig failed.");
            }
            break;
        case PAL_PARAM_ID_DTMF_GEN_TONE_CFG:
        {
            pal_param_dtmf_gen_tone_cfg_t *dtmf_payload;
            uint8_t* paramData = NULL;
            size_t paramSize = 0;
            uint32_t miid = 0;
            int dev = 0;
            struct mixer_ctl *mixer_ctl = NULL;
            const char *control = "setParam";
            param_payload = (pal_param_payload *)payload;
            if (param_payload->payload_size > sizeof(pal_param_dtmf_gen_tone_cfg_t)) {
                PAL_ERR(LOG_TAG, "Invalid payload size %d", param_payload->payload_size);
                status = -EINVAL;
                break;
            }
            dtmf_payload = ((pal_param_dtmf_gen_tone_cfg_t *)param_payload->payload);
            if (dtmf_payload->dir == PAL_AUDIO_OUTPUT) {
                if (!rxAifBackEnds.empty()) { /** search in RX GKV */
                    mixer_ctl = getFEMixerCtl(control, &dev, PAL_AUDIO_OUTPUT);
                    if (!mixer_ctl) {
                        PAL_ERR(LOG_TAG, "Invalid mixer control\n");
                        status = -ENOENT;
                        break;
                    }
                }

                status = SessionAlsaUtils::getModuleInstanceId(mixer, dev,
                                   rxAifBackEnds[0].second.data(), DTMF_GENERATOR, &miid);
                if (status) {
                    PAL_ERR(LOG_TAG, "getModuleInstanceId failed %d", status);
                    break;
                }
                builder->payloadDTMFGenConfig(&paramData, &paramSize, miid, dtmf_payload);
                if (paramSize) {
                    status = SessionAlsaUtils::setMixerParameter(mixer, dev,
                                                    paramData, paramSize);
                    if (status != 0) {
                        PAL_ERR(LOG_TAG,"setMixerParameter failed");
                    }
                } else {
                    PAL_ERR(LOG_TAG,"payloadDTMFGenConfig failed");
                }
                if (paramData) {
                    delete[] paramData;
                    paramData = nullptr;
                    paramSize = 0;
                }
            } else {
                status = -EINVAL;
            }
            break;
        }
        /* will remove once deprecation of feature concludes*/
        case PAL_PARAM_ID_UIEFFECT:
            param_payload = (pal_param_payload *)payload;
            status = setEffectParameters(s, (effect_pal_payload_t *)param_payload->payload);
            if (status) {
                PAL_ERR(LOG_TAG, "setEffectParameters failed with %d", status);
            }
            break;
        case PAL_PARAM_ID_DTMF_DETECTION_CFG:
        {
            pal_param_dtmf_detection_cfg* dtmf_detect_payload;

            PAL_ERR(LOG_TAG, "Enter PAL_PARAM_ID_DTMF_DETECTION_CFG");
            dtmf_detect_payload = (pal_param_dtmf_detection_cfg*) payload;

            int tag = dtmf_detect_payload->enable ? DTMF_DETECT_ENABLE
                      : DTMF_DETECT_DISABLE;

            status = this->setParamWithTag(s, tag, param_id, payload);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set Dtmf detect params status = %d",
                        status);
            }
            PAL_ERR(LOG_TAG, "Exit Detection case");
            break;
        }
        default:
            status = this->setParamWithTag(s, INVALID_TAG, param_id, payload);
            break;
    }
exit:
    PAL_DBG(LOG_TAG, "set parameter status %d", status);
    return status;
}

//#define MUTE_TAG 0, #define UNMUTE_TAG 1
int SessionAR::mute(Stream * s, bool state)
{
    int32_t status = 0;
    status = this->setConfig(s, MODULE, state ? MUTE_TAG : UNMUTE_TAG);
    return status;
}

int SessionAR::pause(Stream * s)
{
    int32_t status = 0;
    status = this->setConfig(s, MODULE, PAUSE_TAG);
    return status;
}

int SessionAR::resume(Stream * s)
{
    int32_t status = 0;
    status = this->setConfig(s, MODULE, RESUME_TAG);
    return status;
}

int SessionAR::setVolume(Stream *s)
{
    int32_t status = 0;
    if (rm->IsCRSCallEnabled()) {
        status = this->setConfig(s, MODULE, CRS_CALL_VOLUME, RX_HOSTLESS);
    } else {
        status = this->setConfig(s, CALIBRATION, VOLUME_LVL);
    }
    return status;
}

int32_t SessionAR::setCustomParam(custom_payload_uc_info_t* uc_info, std::string param_str,
                                    void* param_payload, size_t payload_size, Stream *s)
{
    int32_t status = 0;
    if(param_str == PAL_CUSTOM_PARAM_AR_UI_EFFECT) {
        if(uc_info->streamless) {
            status = rwACDBParamTunnel(uc_info, param_payload, s, true);
        } else {
            pal_param_payload *pal_param = (pal_param_payload *)param_payload;
            effect_pal_payload_t *effectPayload = (effect_pal_payload_t *)pal_param->payload;
            status = setEffectParameters(s,effectPayload);
        }
    } else {
        PAL_ERR(LOG_TAG,"unsupported set param %s", param_str.c_str());
    }

    return status;
}

int32_t SessionAR::getCustomParam(custom_payload_uc_info_t* uc_info, std::string param_str,
                                    void* param_payload, size_t* payload_size, Stream *s)
{
    int status = -EINVAL;
    if(param_str == PAL_CUSTOM_PARAM_AR_UI_EFFECT) {
        if(uc_info->streamless) {
            status = rwACDBParamTunnel(uc_info, param_payload, s, false);
        } else {
            pal_param_payload *pal_param = (pal_param_payload *)param_payload;
            effect_pal_payload_t *effectPayload = (effect_pal_payload_t *)pal_param->payload;
            status = getEffectParameters(s,effectPayload);
        }
    } else {
        PAL_ERR(LOG_TAG,"unsupported get param %s", param_str.c_str());
    }
    return status;
}

void SessionAR::combinedCallback(uint64_t hdl, uint32_t event_id, void *data, uint32_t event_size)
{
    SessionArRmCookie *rmCookie = (SessionArRmCookie *)hdl;
    SessionAR *session = nullptr;
    std::vector<int> triggeringIds;

    // Hold rmCookieRegistryMutex for the entire body of the callback, not just
    // the cookie lookup. The unregister path in registerArEvent takes this same
    // lock before removing the cookie from validRmCookies and deleting the
    // SessionAR object, so holding it here prevents the session from being
    // destroyed while we are still using it (and its cbMapMutex).
    // Lock order is always: rmCookieRegistryMutex -> cbMapMutex.
    std::lock_guard<std::mutex> regLock(SessionAR::rmCookieRegistryMutex);

    bool found = false;
    for (auto *entry : SessionAR::validRmCookies) {
        if (entry == rmCookie) {
            found = true;
            break;
        }
    }
    if (!rmCookie || !found) {
        PAL_ERR(LOG_TAG, "Stale cookie 0x%p for event 0x%x, ignoring",
                (void*)rmCookie, event_id);
        return;
    }
    session = rmCookie->session;
    triggeringIds = rmCookie->pcmDevIds;

    if (!session) {
        PAL_ERR(LOG_TAG, "Invalid session handle received");
        return;
    }

    PAL_DBG(LOG_TAG, "Enter: Event 0x%x triggered by PCM IDs size: %zu",
            event_id, triggeringIds.size());

    std::vector<ArCallbackData> handlersToCall;

    {
        std::lock_guard<std::mutex> lock(session->cbMapMutex);

        auto it = session->sessionCbMap.find(event_id);
        if (it != session->sessionCbMap.end()) {
            for (auto &handlerData : it->second) {
                bool matchFound = false;
                for (int tId : triggeringIds) {
                    for (int hId : handlerData.pcmDevIds) {
                        if (tId == hId) {
                            matchFound = true;
                            break;
                        }
                    }
                    if (matchFound) break;
                }
                if (handlerData.cb && matchFound) {
                    handlersToCall.push_back(handlerData);
                }
            }
        }
    }

    // Execute Callbacks outside cbMapMutex but still under regLock so the
    // session object remains valid for the duration of the dispatch.
    for (auto &h : handlersToCall) {
        h.cb(h.cookie, event_id, data, event_size);
    }
    PAL_DBG(LOG_TAG, "Exit: combinedCallback complete");
}

int SessionAR::registerArEvent(uint32_t event_id, session_callback cb, uint64_t cookie,
                               const std::vector<int>& pcmDevIds, bool is_register)
{
    int status = 0;
    std::unique_lock<std::mutex> lock(cbMapMutex);

    if (is_register) {
        bool duplicate = false;
        if (sessionCbMap.find(event_id) != sessionCbMap.end()) {
            for (auto &existing : sessionCbMap[event_id]) {
                if (existing.cookie == cookie && existing.cb == cb) {
                    duplicate = true;
                    break;
                }
            }
        }

        bool idsAlreadyRegistered = false;
        for (auto *regCookie : registeredCookies) {
            if (regCookie->pcmDevIds == pcmDevIds) {
                idsAlreadyRegistered = true;
                PAL_DBG(LOG_TAG, "PCM ID : %d already registered with RM. Skipping RM registration.", pcmDevIds.at(0));
                break;
            }
        }

        if (!idsAlreadyRegistered && !pcmDevIds.empty()) {
            SessionArRmCookie* newCookie = new SessionArRmCookie();
            newCookie->session = this;
            newCookie->pcmDevIds = pcmDevIds;
            lock.unlock();
            status = rm->registerMixerEventCallback(pcmDevIds,
                                                    SessionAR::combinedCallback,
                                                    (uint64_t)newCookie,
                                                    true);
            // Acquire rmCookieRegistryMutex while cbMapMutex is still
            // released, then reacquire cbMapMutex only after the
            // rmCookieRegistryMutex scope exits, to keep the lock order
            // consistent with combinedCallback (rmCookieRegistryMutex ->
            // cbMapMutex) and avoid an ABBA deadlock.
            if (status == 0) {
                std::lock_guard<std::mutex> regLock(rmCookieRegistryMutex);
                validRmCookies.push_back(newCookie);
            }
            lock.lock();
            if (status == 0) {
                registeredCookies.push_back(newCookie);
                PAL_DBG(LOG_TAG, "Registered new PCM ID : %d with RM.", pcmDevIds.at(0));
            } else {
                PAL_ERR(LOG_TAG, "RM Registration failed: %d", status);
                delete newCookie;
                return status;
            }
        }

        if (!duplicate) {
            ArCallbackData data = {cb, cookie, pcmDevIds};
            sessionCbMap[event_id].push_back(data);
            PAL_DBG(LOG_TAG, "Added callback to list: Event 0x%x, Cookie 0x%llx",
                    event_id, (unsigned long long)cookie);
        }
    }
    else {
        auto mapIt = sessionCbMap.find(event_id);
        if (mapIt != sessionCbMap.end()) {
            auto &list = mapIt->second;
            for (auto it = list.begin(); it != list.end(); ) {
                if (it->cookie == cookie && it->cb == cb) {
                    PAL_DBG(LOG_TAG, "Removed callback from internal list: Event 0x%x, Cookie 0x%llx",
                            event_id, (unsigned long long)cookie);
                    it = list.erase(it);
                } else {
                    ++it;
                }
            }
            if (list.empty()) {
                sessionCbMap.erase(mapIt);
            }
        }

        bool isStillInUse = false;
        for (auto &mapEntry : sessionCbMap) {
            for (auto &handler : mapEntry.second) {
                if (handler.pcmDevIds == pcmDevIds) {
                    isStillInUse = true;
                    break;
                }
            }
            if (isStillInUse) break;
        }

        if (!isStillInUse) {
            for (auto it = registeredCookies.begin(); it != registeredCookies.end(); ) {
                SessionArRmCookie* c = *it;
                if (!c) {
                    // Drop cbMapMutex before acquiring rmCookieRegistryMutex to
                    // maintain lock order (rmCookieRegistryMutex -> cbMapMutex)
                    // and avoid deadlock with combinedCallback.
                    lock.unlock();
                    {
                        std::lock_guard<std::mutex> regLock(rmCookieRegistryMutex);
                        for (auto vit = validRmCookies.begin(); vit != validRmCookies.end(); ) {
                            if (*vit == nullptr) {
                                vit = validRmCookies.erase(vit);
                            } else {
                                ++vit;
                            }
                        }
                    }
                    lock.lock();
                    it = registeredCookies.erase(it);
                    continue;
                }
                if (c->pcmDevIds == pcmDevIds) {
                    // Drop cbMapMutex before acquiring rmCookieRegistryMutex to
                    // maintain lock order (rmCookieRegistryMutex -> cbMapMutex)
                    // and avoid deadlock with combinedCallback.
                    lock.unlock();
                    rm->registerMixerEventCallback(pcmDevIds,
                                                   SessionAR::combinedCallback,
                                                   (uint64_t)c,
                                                   false);
                    PAL_DBG(LOG_TAG, "Unregistered PCM ID : %d from RM as no callbacks remain.",
                            pcmDevIds.empty() ? 0 : pcmDevIds.at(0));
                    {
                        std::lock_guard<std::mutex> regLock(rmCookieRegistryMutex);
                        for (auto vit = validRmCookies.begin(); vit != validRmCookies.end(); ++vit) {
                            if (*vit == c) {
                                validRmCookies.erase(vit);
                                break;
                            }
                        }
                    }
                    delete c;
                    // Reacquire cbMapMutex before modifying registeredCookies.
                    lock.lock();
                    it = registeredCookies.erase(it);
                    break;
                } else {
                    ++it;
                }
            }
        } else {
            PAL_DBG(LOG_TAG, "Skipping RM deregister for PCM ID : %d, other callbacks still active.",
                    pcmDevIds.empty() ? 0 : pcmDevIds.at(0));
        }
    }
    return status;
}