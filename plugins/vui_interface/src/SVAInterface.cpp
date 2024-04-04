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
 * Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
 *
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: SVAInterface"
//#define LOG_NDEBUG 0

#ifdef VUI_USE_SYSLOG
#include <stdint.h>
#include <syslog.h>

#ifndef ALOGD
#define ALOGD(fmt, arg...) syslog (LOG_ERR, fmt, ##arg)
#endif

#ifndef ALOGI
#define ALOGI(fmt, arg...) syslog (LOG_ERR, fmt, ##arg)
#endif

#ifndef ALOGE
#define ALOGE(fmt, arg...) syslog (LOG_ERR, fmt, ##arg)
#endif

#ifndef ALOGV
#define ALOGV(fmt, arg...) syslog (LOG_ERR, fmt, ##arg)
#endif

#else
#include <log/log.h>
#endif

#include <list>
#include "SVAInterface.h"

#define ST_MAX_FSTAGE_CONF_LEVEL  (100)
#define CUSTOM_CONFIG_OPAQUE_DATA_SIZE 12
#define CONF_LEVELS_INTF_VERSION_0002 0x02

std::map<st_module_type_t, std::vector<std::shared_ptr<VoiceUIInterface>>>
    SVAInterface::intf_map_;
std::mutex SVAInterface::intf_create_mutex_;

std::shared_ptr<VoiceUIInterface> SVAInterface::GetInstance(
    vui_intf_param_t *model) {

    int32_t status = 0;
    std::shared_ptr<VoiceUIInterface> interface = nullptr;
    struct pal_st_sound_model *sound_model = nullptr;
    std::vector<sound_model_data_t *> model_list;
    st_module_type_t key;
    st_module_type_t module_type;
    sound_model_config_t *config = nullptr;

    if (!model || !model->data) {
        ALOGE("%s: %d: Invalid input", __func__, __LINE__);
        goto exit;
    }

    config = (sound_model_config_t *)model->data;
    sound_model = (struct pal_st_sound_model *)config->sound_model;
    module_type = config->module_type;
    status = SVAInterface::ParseSoundModel(sound_model, &module_type, model_list);
    if (status) {
        ALOGE("%s: %d: Failed to parse sound model, status = %d",
            __func__, __LINE__, status);
        goto exit;
    }

    // check if existing interface can be reused
    intf_create_mutex_.lock();
    if (IS_MODULE_TYPE_PDK(module_type)) {
        key = ST_MODULE_TYPE_PDK;
    } else {
        key = module_type;
    }

    if (intf_map_.find(key) != intf_map_.end() && intf_map_[key].size() > 0) {
        if ((IS_MODULE_TYPE_PDK(module_type) &&
             intf_map_[key].size() >= config->supported_engine_count) ||
            (!IS_MODULE_TYPE_PDK(module_type) &&
             config->is_model_merge_enabled))
            interface = intf_map_[key][intf_map_[key].size() - 1];
    }

    if (!interface) {
        ALOGI("%s: %d: Create new SVAInterface", __func__, __LINE__);
        interface = std::make_shared<SVAInterface>(module_type);
        if (!interface) {
            ALOGE("%s: %d: Failed to create SVA interface", __func__, __LINE__);
            intf_create_mutex_.unlock();
            goto exit;
        }
        intf_map_[key].push_back(interface);
    }
    intf_create_mutex_.unlock();

    status = interface->RegisterModel(model->stream, sound_model, model_list);
    if (status) {
        ALOGE("%s: %d: Failed to register sound model, status = %d",
            __func__, __LINE__, status);
        goto exit;
    }

exit:
    return interface;
}

void SVAInterface::DetachStream(void *stream) {
    st_module_type_t key;
    std::shared_ptr<VoiceUIInterface> interface = nullptr;

    DeregisterModel(stream);

    if (!sm_info_map_.size()) {
        key = module_type_;
        if (IS_MODULE_TYPE_PDK(module_type_)) {
            key = ST_MODULE_TYPE_PDK;
        }

        intf_create_mutex_.lock();
        for (auto iter = intf_map_[key].begin();
             iter != intf_map_[key].end(); iter++) {
            interface = *iter;
            if (interface.get() == this) {
                intf_map_[key].erase(iter);
                break;
            }
        }
        if (!intf_map_[key].size())
            intf_map_.erase(key);
        intf_create_mutex_.unlock();
    }
}

SVAInterface::SVAInterface(st_module_type_t module_type) {

    module_type_ = module_type;
    st_conf_levels_ = nullptr;
    st_conf_levels_v2_ = nullptr;
    register_model_ = nullptr;
    sm_merged_ = false;
    wakeup_payload_ = nullptr;
    sound_model_info_ = new SoundModelInfo();
    memset(&default_buf_config_, 0, sizeof(default_buf_config_));
    std::memset(&register_model_, 0, sizeof(register_model_));
    std::memset(&deregister_model_, 0, sizeof(deregister_model_));
    std::memset(&buffering_config_, 0, sizeof(buffering_config_));
    std::memset(&wakeup_config_, 0, sizeof(wakeup_config_));
}

SVAInterface::~SVAInterface() {
    ALOGD("%s: %d: Enter", __func__, __LINE__);

    if (st_conf_levels_) {
        free(st_conf_levels_);
        st_conf_levels_ = nullptr;
    }
    if (st_conf_levels_v2_) {
        free(st_conf_levels_v2_);
        st_conf_levels_v2_ = nullptr;
    }
    if (sound_model_info_) {
        delete sound_model_info_;
    }
    for (auto const& x: det_event_info_) {
        free(x.second);
    }
    det_event_info_.clear();
    if (wakeup_payload_) {
        free(wakeup_payload_);
    }
    if (register_model_) {
        free(register_model_);
    }
    readOffsets_.clear();
    origin_hist_buf_duration_.clear();
    ALOGD("%s: %d: Exit", __func__, __LINE__);
}

int32_t SVAInterface::SetParameter(intf_param_id_t param_id,
        vui_intf_param_t *param) {

    int32_t status = 0;

    ALOGV("%s: %d: Enter", __func__, __LINE__);

    if (!param) {
        ALOGE("%s: %d: Invalid param", __func__, __LINE__);
        return -EINVAL;
    }

    switch (param_id) {
        case PARAM_FSTAGE_SOUND_MODEL_ADD: {
            if (!IS_MODULE_TYPE_PDK(module_type_)) {
                status = UpdateEngineModel(param->stream,
                    (uint8_t *)param->data, param->size, true);
            }
            break;
        }
        case PARAM_FSTAGE_SOUND_MODEL_DELETE: {
            if (!IS_MODULE_TYPE_PDK(module_type_)) {
                status = UpdateEngineModel(param->stream, nullptr, 0, false);
            }
            break;
        }
        case PARAM_FSTAGE_SOUND_MODEL_ID: {
            SetModelId(param->stream, *(uint32_t *)param->data);
            break;
        }
        case PARAM_FSTAGE_SOUND_MODEL_STATE: {
            status = SetModelState(param->stream, *(bool *)param->data);
            break;
        }
        case PARAM_RECOGNITION_MODE: {
            SetRecognitionMode(param->stream, *(uint32_t *)param->data);
            break;
        }
        case PARAM_RECOGNITION_CONFIG: {
            status = ParseRecognitionConfig(param->stream,
                (struct pal_st_recognition_config *)param->data);
            break;
        }
        case PARAM_SSTAGE_KW_DET_LEVEL: {
            SetSecondStageDetLevels(param->stream,
                ST_SM_ID_SVA_S_STAGE_KWD, *(int32_t *)param->data);
            break;
        }
        case PARAM_SSTAGE_UV_DET_LEVEL: {
            SetSecondStageDetLevels(param->stream,
                ST_SM_ID_SVA_S_STAGE_USER, *(int32_t *)param->data);
            break;
        }
        case PARAM_DETECTION_EVENT: {
            status = ParseDetectionPayload(param->stream, param->data, param->size);
            break;
        }
        case PARAM_DETECTION_RESULT: {
            UpdateDetectionResult(param->stream, *(uint32_t *)param->data);
            break;
        }
        case PARAM_KEYWORD_INDEX: {
            UpdateIndices(param->stream, *(struct keyword_index *)param->data);
            break;
        }
        case PARAM_LAB_READ_OFFSET: {
            SetReadOffset(param->stream, *(uint32_t *)param->data);
            break;
        }
        case PARAM_STREAM_ATTRIBUTES: {
            SetStreamAttributes((struct pal_stream_attributes *)param->data);
            break;
        }
        case PARAM_DEFAULT_BUFFER_CONFIG: {
            struct buffer_config *buf_config =
                (struct buffer_config *)param->data;
            default_buf_config_.hist_buffer_duration =
                buf_config->hist_buffer_duration;
            default_buf_config_.pre_roll_duration =
                buf_config->pre_roll_duration;
            break;
        }
        default:
            ALOGE("%s: %d: Unsupported param id %d",
                __func__, __LINE__, param_id);
            break;
    }

    ALOGV("%s: %d: Exit", __func__, __LINE__);
    return status;
}

int32_t SVAInterface::GetParameter(intf_param_id_t param_id,
    vui_intf_param_t *param) {

    int32_t status = 0;

    ALOGV("%s: %d: Enter", __func__, __LINE__);

    if (!param) {
        ALOGE("%s: %d: Invalid param", __func__, __LINE__);
        return -EINVAL;
    }

    switch (param_id) {
        case PARAM_INTERFACE_PROPERTY: {
            vui_intf_property_t *property = (vui_intf_property_t *)param->data;
            if (property) {
                property->is_qc_wakeup_config = true;
                property->is_multi_model_supported =
                    IS_MODULE_TYPE_PDK(module_type_);
            } else {
                ALOGE("%s: %d: Invalid property", __func__, __LINE__);
                status = -EINVAL;
            }
            break;
        }
        case PARAM_FSTAGE_SOUND_MODEL_TYPE: {
            *(st_module_type_t *)param->data = GetModuleType(param->stream);
            break;
        }
        case PARAM_SOUND_MODEL_LIST: {
            void *s = param->stream;
            sound_model_list_t *sm_list = (sound_model_list_t *)param->data;
            if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
                for (int i = 0; i < sm_info_map_[s]->model_list.size(); i++) {
                    sm_list->sm_list.push_back(sm_info_map_[s]->model_list[i]);
                }
            } else {
                ALOGE("%s: %d: stream not registered", __func__, __LINE__);
                status = -EINVAL;
            }
            break;
        }
        case PARAM_FSTAGE_BUFFERING_CONFIG: {
            struct buffer_config *config = (struct buffer_config *)param->data;
            GetBufferingConfigs(param->stream, config);
            break;
        }
        case PARAM_FSTAGE_DETECTION_UV_SCORE: {
            struct detection_event_info *info =
                (struct detection_event_info *)GetDetectionEventInfo(param->stream);
            if (!info) {
                ALOGE("%s: %d: Failed to get detection event info",
                    __func__, __LINE__);
                status = -EINVAL;
            } else {
                *(uint32_t *)param->data = info->confidence_levels[1];
            }
            break;
        }
        case PARAM_SSTAGE_KW_CONF_LEVEL: {
            GetSecondStageConfLevels(param->stream,
                ST_SM_ID_SVA_S_STAGE_KWD, (int32_t *)param->data);
            break;
        }
        case PARAM_SSTAGE_UV_CONF_LEVEL: {
            GetSecondStageConfLevels(param->stream,
                ST_SM_ID_SVA_S_STAGE_USER, (int32_t *)param->data);
            break;
        }
        case PARAM_DETECTION_EVENT: {
            status = GenerateCallbackEvent(param->stream,
                (struct pal_st_recognition_event **)param->data, &param->size);
            break;
        }
        case PARAM_DETECTION_STREAM:
        case PARAM_DETECTION_STREAM_LIST: {
            param->stream = GetDetectedStream(param->data);
            break;
        }
        case PARAM_KEYWORD_INDEX: {
            GetKeywordIndex(param->stream, (struct keyword_index *)param->data);
            break;
        }
        case PARAM_KEYWORD_STATS: {
            GetKeywordStats(param->stream, (struct keyword_stats *)param->data);
            break;
        }
        case PARAM_LAB_READ_OFFSET: {
            *(uint32_t *)param->data = GetReadOffset(param->stream);
            break;
        }
        case PARAM_SOUND_MODEL_LOAD:
            status = GetSoundModelLoadPayload(param);
            break;
        case PARAM_SOUND_MODEL_UNLOAD:
            status = GetSoundModelUnloadPayload(param);
            break;
        case PARAM_WAKEUP_CONFIG:
            status = GetWakeUpPayload(param);
            break;
        case PARAM_BUFFERING_CONFIG:
            status = GetBufferingPayload(param);
            break;
        case PARAM_ENGINE_RESET:
            status = GetEngineResetPayload(param);
            break;
        default:
            ALOGE("%s: %d: Unsupported param id %d",
                __func__, __LINE__, param_id);
            break;
    }

    ALOGV("%s: %d: Exit", __func__, __LINE__);
    return status;
}

int32_t SVAInterface::ParseSoundModel(
    struct pal_st_sound_model *sound_model,
    st_module_type_t *first_stage_type,
    std::vector<sound_model_data_t *> &model_list) {

    int32_t status = 0;
    int32_t i = 0;
    struct pal_st_phrase_sound_model *phrase_sm = nullptr;
    struct pal_st_sound_model *common_sm = nullptr;
    uint8_t *ptr = nullptr;
    uint8_t *sm_payload = nullptr;
    uint8_t *sm_data = nullptr;
    int32_t sm_size = 0;
    SML_GlobalHeaderType *global_hdr = nullptr;
    SML_HeaderTypeV3 *hdr_v3 = nullptr;
    SML_BigSoundModelTypeV3 *big_sm = nullptr;
    uint32_t offset = 0;
    sound_model_data_t *model_data = nullptr;

    ALOGD("%s: %d: Enter", __func__, __LINE__);

    if (sound_model->type == PAL_SOUND_MODEL_TYPE_KEYPHRASE) {
        phrase_sm = (struct pal_st_phrase_sound_model *)sound_model;
        sm_payload = (uint8_t *)phrase_sm + phrase_sm->common.data_offset;
        global_hdr = (SML_GlobalHeaderType *)sm_payload;
        if (global_hdr->magicNumber == SML_GLOBAL_HEADER_MAGIC_NUMBER) {
            hdr_v3 = (SML_HeaderTypeV3 *)(sm_payload +
                                          sizeof(SML_GlobalHeaderType));
            ALOGI("%s: %d: num of sound models = %u",
                __func__, __LINE__, hdr_v3->numModels);
            for (i = 0; i < hdr_v3->numModels; i++) {
                big_sm = (SML_BigSoundModelTypeV3 *)(
                    sm_payload + sizeof(SML_GlobalHeaderType) +
                    sizeof(SML_HeaderTypeV3) +
                    (i * sizeof(SML_BigSoundModelTypeV3)));

                ALOGI("%s: %d: type = %u, size = %u, version = %u.%u",
                    __func__, __LINE__, big_sm->type, big_sm->size,
                    big_sm->versionMajor, big_sm->versionMinor);
                if (big_sm->type == ST_SM_ID_SVA_F_STAGE_GMM) {
                    *first_stage_type = (st_module_type_t)big_sm->versionMajor;
                    sm_size = big_sm->size;
                    sm_data = (uint8_t *)calloc(1, sm_size);
                    if (!sm_data) {
                        status = -ENOMEM;
                        ALOGE("%s: %d: sm_data allocation failed, status %d",
                            __func__, __LINE__, status);
                        goto error_exit;
                    }
                    ptr = (uint8_t *)sm_payload +
                        sizeof(SML_GlobalHeaderType) +
                        sizeof(SML_HeaderTypeV3) +
                        (hdr_v3->numModels * sizeof(SML_BigSoundModelTypeV3)) +
                        big_sm->offset;
                    ar_mem_cpy(sm_data, sm_size, ptr, sm_size);

                    model_data = (sound_model_data_t *)calloc(1, sizeof(sound_model_data_t));
                    if (!model_data) {
                        status = -ENOMEM;
                        ALOGE("%s: %d: model_data allocation failed, status %d",
                            __func__, __LINE__, status);
                        goto error_exit;
                    }
                    model_data->type = big_sm->type;
                    model_data->data = sm_data;
                    model_data->size = sm_size;
                    model_list.push_back(model_data);
                } else if (big_sm->type != SML_ID_SVA_S_STAGE_UBM) {
                    if (big_sm->type == SML_ID_SVA_F_STAGE_INTERNAL ||
                        (big_sm->type == ST_SM_ID_SVA_S_STAGE_USER &&
                         !(phrase_sm->phrases[0].recognition_mode &
                         PAL_RECOGNITION_MODE_USER_IDENTIFICATION)))
                        continue;
                    sm_size = big_sm->size;
                    ptr = (uint8_t *)sm_payload +
                        sizeof(SML_GlobalHeaderType) +
                        sizeof(SML_HeaderTypeV3) +
                        (hdr_v3->numModels * sizeof(SML_BigSoundModelTypeV3)) +
                        big_sm->offset;
                    sm_data = (uint8_t *)calloc(1, sm_size);
                    if (!sm_data) {
                        status = -ENOMEM;
                        ALOGE("%s: %d: Failed to alloc memory for sm_data",
                            __func__, __LINE__);
                        goto error_exit;
                    }
                    ar_mem_cpy(sm_data, sm_size, ptr, sm_size);

                    model_data = (sound_model_data_t *)calloc(1, sizeof(sound_model_data_t));
                    if (!model_data) {
                        status = -ENOMEM;
                        ALOGE("%s: %d: model_data allocation failed, status %d",
                            __func__, __LINE__, status);
                        goto error_exit;
                    }
                    model_data->type = big_sm->type;
                    model_data->data = sm_data;
                    model_data->size = sm_size;
                    model_list.push_back(model_data);
                }
            }
        } else {
            // Parse sound model 2.0
            sm_size = phrase_sm->common.data_size;
            sm_data = (uint8_t *)calloc(1, sm_size);
            if (!sm_data) {
                ALOGE("%s: %d: Failed to allocate memory for sm_data",
                    __func__, __LINE__);
                status = -ENOMEM;
                goto error_exit;
            }
            ptr = (uint8_t*)phrase_sm + phrase_sm->common.data_offset;
            ar_mem_cpy(sm_data, sm_size, ptr, sm_size);

            model_data = (sound_model_data_t *)calloc(1, sizeof(sound_model_data_t));
            if (!model_data) {
                status = -ENOMEM;
                ALOGE("%s: %d: model_data allocation failed, status %d",
                    __func__, __LINE__, status);
                goto error_exit;
            }
            model_data->type = ST_SM_ID_SVA_F_STAGE_GMM;
            model_data->data = sm_data;
            model_data->size = sm_size;
            model_list.push_back(model_data);
        }
    } else {
        // handle for generic sound model
        common_sm = sound_model;
        sm_size = common_sm->data_size;
        sm_data = (uint8_t *)calloc(1, sm_size);
        if (!sm_data) {
            ALOGE("%s: %d: Failed to allocate memory for sm_data",
                __func__, __LINE__);
            status = -ENOMEM;
            goto error_exit;
        }
        ptr = (uint8_t*)common_sm + common_sm->data_offset;
        ar_mem_cpy(sm_data, sm_size, ptr, sm_size);

        model_data = (sound_model_data_t *)calloc(1, sizeof(sound_model_data_t));
        if (!model_data) {
            status = -ENOMEM;
            ALOGE("%s: %d: model_data allocation failed, status %d",
                __func__, __LINE__, status);
            goto error_exit;
        }
        model_data->type = ST_SM_ID_SVA_F_STAGE_GMM;
        model_data->data = sm_data;
        model_data->size = sm_size;
        model_list.push_back(model_data);
    }
    ALOGD("%s: %d: Exit, status %d", __func__, __LINE__, status);
    return status;

error_exit:
    // clean up memory added to model_list in failure case
    for (int i = 0; i < model_list.size(); i++) {
        model_data = model_list[i];
        if (model_data) {
            if (model_data->data)
                free(model_data->data);
            free(model_data);
        }
    }
    model_list.clear();
    if (sm_data)
        free(sm_data);

    ALOGD("%s: %d: Exit", __func__, __LINE__);
    return status;
}

int32_t SVAInterface::ParseRecognitionConfig(void *s,
    struct pal_st_recognition_config *config) {

    int32_t status = 0;
    struct sound_model_info *sm_info = nullptr;
    struct st_param_header *param_hdr = NULL;
    struct st_hist_buffer_info *hist_buf = NULL;
    struct st_det_perf_mode_info *det_perf_mode = NULL;
    uint8_t *opaque_ptr = NULL;
    unsigned int opaque_size = 0, conf_levels_payload_size = 0;
    uint32_t hist_buffer_duration = 0;
    uint32_t pre_roll_duration = 0;
    uint8_t *conf_levels = NULL;
    uint32_t num_conf_levels = 0;

    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        sm_info = sm_info_map_[s];
        sm_info->rec_config = config;
    } else {
        ALOGE("%s: %d: Stream not registered to interface", __func__, __LINE__);
        return -EINVAL;
    }

    ALOGD("%s: %d: Enter", __func__, __LINE__);
    if (!config) {
        ALOGE("%s: %d: Invalid config", __func__, __LINE__);
        return -EINVAL;
    }

    // Parse recognition config
    if (config->data_size > CUSTOM_CONFIG_OPAQUE_DATA_SIZE) {
        opaque_ptr = (uint8_t *)config + config->data_offset;
        while (opaque_size < config->data_size) {
            param_hdr = (struct st_param_header *)opaque_ptr;
            ALOGV("%s: %d: key %d, payload size %d", __func__, __LINE__,
                param_hdr->key_id, param_hdr->payload_size);

            switch (param_hdr->key_id) {
                case ST_PARAM_KEY_CONFIDENCE_LEVELS:
                    sm_info->conf_levels_intf_version = *(uint32_t *)(
                        opaque_ptr + sizeof(struct st_param_header));
                    ALOGV("%s: %d: conf_levels_intf_version = %u",
                        __func__, __LINE__, sm_info->conf_levels_intf_version);
                    if (sm_info->conf_levels_intf_version !=
                        CONF_LEVELS_INTF_VERSION_0002) {
                        conf_levels_payload_size =
                            sizeof(struct st_confidence_levels_info);
                    } else {
                        conf_levels_payload_size =
                            sizeof(struct st_confidence_levels_info_v2);
                    }
                    if (param_hdr->payload_size != conf_levels_payload_size) {
                        ALOGE("%s: %d: Conf level format error, exiting",
                            __func__, __LINE__);
                        status = -EINVAL;
                        goto error_exit;
                    }
                    status = ParseOpaqueConfLevels(sm_info, opaque_ptr,
                                                   sm_info->conf_levels_intf_version,
                                                   &conf_levels,
                                                   &num_conf_levels);
                    if (status) {
                        ALOGE("%s: %d: Failed to parse opaque conf levels",
                            __func__, __LINE__);
                        goto error_exit;
                    }

                    opaque_size += sizeof(struct st_param_header) +
                        conf_levels_payload_size;
                    opaque_ptr += sizeof(struct st_param_header) +
                        conf_levels_payload_size;
                    if (status) {
                        ALOGE("%s: %d: Parse conf levels failed(status=%d)",
                            __func__, __LINE__, status);
                        status = -EINVAL;
                        goto error_exit;
                    }
                    break;
                case ST_PARAM_KEY_HISTORY_BUFFER_CONFIG:
                    if (param_hdr->payload_size !=
                        sizeof(struct st_hist_buffer_info)) {
                        ALOGE("%s: %d: History buffer config format error",
                            __func__, __LINE__);
                        status = -EINVAL;
                        goto error_exit;
                    }
                    hist_buf = (struct st_hist_buffer_info *)(opaque_ptr +
                        sizeof(struct st_param_header));
                    hist_buffer_duration = hist_buf->hist_buffer_duration_msec;
                    pre_roll_duration = hist_buf->pre_roll_duration_msec;

                    opaque_size += sizeof(struct st_param_header) +
                        sizeof(struct st_hist_buffer_info);
                    opaque_ptr += sizeof(struct st_param_header) +
                        sizeof(struct st_hist_buffer_info);
                    break;
                case ST_PARAM_KEY_DETECTION_PERF_MODE:
                    if (param_hdr->payload_size !=
                        sizeof(struct st_det_perf_mode_info)) {
                        ALOGE("%s: %d: Opaque data format error, exiting",
                            __func__, __LINE__);
                        status = -EINVAL;
                        goto error_exit;
                    }
                    det_perf_mode = (struct st_det_perf_mode_info *)
                        (opaque_ptr + sizeof(struct st_param_header));
                    ALOGD("%s: %d: set perf mode %d", det_perf_mode->mode,
                        __func__, __LINE__);
                    opaque_size += sizeof(struct st_param_header) +
                        sizeof(struct st_det_perf_mode_info);
                    opaque_ptr += sizeof(struct st_param_header) +
                        sizeof(struct st_det_perf_mode_info);
                    break;
                default:
                    ALOGE("%s: %d: Unsupported opaque data key id, exiting",
                        __func__, __LINE__);
                    status = -EINVAL;
                    goto error_exit;
            }
        }
    } else {
        status = FillConfLevels(sm_info, config, &conf_levels, &num_conf_levels);
        if (status) {
            ALOGE("%s: %d: Failed to parse conf levels from rc config",
                __func__, __LINE__);
            goto error_exit;
        }
    }

    // get history buffer duration from sound trigger platform xml
    origin_hist_buf_duration_[s] = hist_buffer_duration;
    if (!hist_buffer_duration)
        hist_buffer_duration = default_buf_config_.hist_buffer_duration;
    if (!pre_roll_duration)
        pre_roll_duration = default_buf_config_.pre_roll_duration;

    sm_info_map_[s]->buf_config.hist_buffer_duration = hist_buffer_duration;
    sm_info_map_[s]->buf_config.pre_roll_duration = pre_roll_duration;

    if (sm_info_map_[s]->wakeup_config)
        free(sm_info_map_[s]->wakeup_config);
    sm_info_map_[s]->wakeup_config = conf_levels;
    sm_info_map_[s]->wakeup_config_size = num_conf_levels;
    goto exit;

error_exit:
    if (st_conf_levels_) {
        free(st_conf_levels_);
        st_conf_levels_ = nullptr;
    }
    if (st_conf_levels_v2_) {
        free(st_conf_levels_v2_);
        st_conf_levels_v2_ = nullptr;
    }

exit:
    ALOGD("%s: %d: Exit, status %d", __func__, __LINE__, status);
    return status;
}

void SVAInterface::GetBufferingConfigs(void *s,
                                       struct buffer_config *config) {

    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        config->hist_buffer_duration = sm_info_map_[s]->buf_config.hist_buffer_duration;
        config->pre_roll_duration = sm_info_map_[s]->buf_config.pre_roll_duration;
    } else {
        ALOGE("%s: %d: Stream not registered to interface", __func__, __LINE__);
    }
}

void SVAInterface::GetSecondStageConfLevels(void *s,
                                            listen_model_indicator_enum type,
                                            int32_t *level) {

    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        for (auto iter = sm_info_map_[s]->sec_threshold.begin();
            iter != sm_info_map_[s]->sec_threshold.end(); iter++) {
            if ((*iter).first == type)
                *level = (*iter).second;
        }
    } else {
        ALOGE("%s: %d: Stream not registered to interface", __func__, __LINE__);
    }
}

void SVAInterface::SetSecondStageDetLevels(void *s,
                                           listen_model_indicator_enum type,
                                           int32_t level) {

    bool sec_det_level_exist = false;

    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        for (auto &iter: sm_info_map_[s]->sec_det_level) {
            if (iter.first == type) {
                iter.second = level;
                sec_det_level_exist = true;
                break;
            }
        }
        if (!sec_det_level_exist)
            sm_info_map_[s]->sec_det_level.push_back(std::make_pair(type, level));
    } else {
        ALOGE("%s: %d: Stream not registered to interface", __func__, __LINE__);
    }
}

int32_t SVAInterface::ParseDetectionPayload(void *s, void *event, uint32_t size) {

    int32_t status = 0;

    if (sm_info_map_.find(s) == sm_info_map_.end()) {
        ALOGE("%s: %d: Stream not attached", __func__, __LINE__);
        return -EINVAL;
    }

    if (!IS_MODULE_TYPE_PDK(module_type_)) {
        status = ParseDetectionPayloadGMM(s, event);
        CheckAndSetDetectionConfLevels(GetDetectedStream(event));
    } else {
        status = ParseDetectionPayloadPDK(s, event);
    }
    if (status) {
        ALOGE("%s: %d: Failed to parse detection payload, status %d",
            __func__, __LINE__, status);
    }

    return status;
}

void SVAInterface::GetKeywordIndex(void *s, struct keyword_index *index) {

    if (det_event_info_.find(s) == det_event_info_.end() || !index) {
        ALOGE("%s: %d: Invalid stream", __func__, __LINE__);
        return;
    }

    index->start_index = det_event_info_[s]->start_index_;
    index->end_index = det_event_info_[s]->end_index_;
}

void SVAInterface::GetKeywordStats(void *s, struct keyword_stats *stats) {

    if (det_event_info_.find(s) == det_event_info_.end() || !stats) {
        ALOGE("%s: %d: Invalid stream", __func__, __LINE__);
        return;
    }

    stats->start_ts = det_event_info_[s]->start_ts_;
    stats->end_ts = det_event_info_[s]->end_ts_;
    stats->ftrt_duration = det_event_info_[s]->ftrt_size_us_;
}

void* SVAInterface::GetPDKDetectedStream(void *event) {

    int32_t status = 0;
    uint32_t payload_size = 0;
    uint32_t parsed_size = 0;
    uint32_t event_size = 0;
    uint32_t keyId = 0;
    uint32_t model_id = 0;
    uint32_t best_conf_level = 0;
    uint8_t *ptr = nullptr;
    struct event_id_detection_engine_generic_info_t *generic_info = nullptr;
    struct detection_event_info_header_t *event_header = nullptr;
    struct model_stats *model_stat = nullptr;
    void *st = nullptr;
    struct sound_model_info *sm_info = nullptr;
    struct voice_ui_multi_model_result_info_t *result_info = nullptr;
    std::list<void *> *det_list = nullptr;

    generic_info = (struct event_id_detection_engine_generic_info_t *)event;
    payload_size = sizeof(struct event_id_detection_engine_generic_info_t);
    event_size = generic_info->payload_size;
    ptr = (uint8_t *)event + payload_size;

    if (!event_size) {
        ALOGE("%s: %d: Invalid detection payload", __func__, __LINE__);
        return nullptr;
    }

    ALOGI("%s: %d: event_size = %u", __func__, __LINE__, event_size);
    while (parsed_size < event_size) {
        ALOGD("%s: %d: parsed_size = %u, event_size = %u",
            __func__, __LINE__, parsed_size, event_size);
        event_header = (struct detection_event_info_header_t *)ptr;
        keyId = event_header->key_id;
        payload_size = event_header->payload_size;
        ptr += sizeof(struct detection_event_info_header_t);
        parsed_size += sizeof(struct detection_event_info_header_t);

        switch (keyId) {
            case KEY_ID_FTRT_DATA_INFO :
                break;
            case KEY_ID_VOICE_UI_MULTI_MODEL_RESULT_INFO :
                ALOGI("%s: %d: voice_ui_multi_model_result_info: %u",
                    __func__, __LINE__, payload_size);

                det_list = new std::list<void *>;
                if (!det_list) {
                    ALOGE("%s: %d: Failed to allocate detected stream list",
                        __func__, __LINE__);
                    break;
                }

                result_info = (struct voice_ui_multi_model_result_info_t *)ptr;
                for (int i = 0; i < result_info->num_detected_models; i++) {
                    model_stat = (struct model_stats *)(ptr +
                        sizeof(struct voice_ui_multi_model_result_info_t) +
                        i * sizeof(struct model_stats));
                    model_id = model_stat->detected_model_id;
                    for (auto &iter : sm_info_map_) {
                        sm_info = iter.second;
                        if (sm_info->model_id == model_id) {
                            st = iter.first;
                            break;
                        }
                    }
                    if (!st) {
                        ALOGE("%s: %d: Invalid model id %d, no matched stream found",
                            __func__, __LINE__, model_id);
                        break;
                    }

                    if (model_stat->best_confidence_level > best_conf_level) {
                        best_conf_level = model_stat->best_confidence_level;
                        det_list->push_front(st);
                    } else {
                        det_list->push_back(st);
                    }
                }
                parsed_size = event_size; //break out of while loop
                ALOGI("%s: %d: model id: %u", __func__, __LINE__, model_id);
                break;
            default :
                ALOGE("%s: %d: Invalid key id %u", __func__, __LINE__, keyId);
                return nullptr;
        }
        ptr += payload_size;
        parsed_size += payload_size;
    }

    return (void *)det_list;
}

void* SVAInterface::GetGMMDetectedStream(void *event) {

    int32_t status = 0;
    uint32_t payload_size = 0;
    uint32_t parsed_size = 0;
    uint32_t event_size = 0;
    uint32_t keyId = 0;
    uint8_t *ptr = nullptr;
    struct event_id_detection_engine_generic_info_t *generic_info = nullptr;
    struct detection_event_info_header_t *event_header = nullptr;
    uint16_t num_confidence_levels = 0;
    uint8_t confidence_levels[20] = {0};
    struct confidence_level_info_t *confidence_info = nullptr;
    void *st = nullptr;

    generic_info = (struct event_id_detection_engine_generic_info_t *)event;
    payload_size = sizeof(struct event_id_detection_engine_generic_info_t);
    event_size = generic_info->payload_size;
    ptr = (uint8_t *)event + payload_size;

    if (!event_size || generic_info->status) {
        ALOGE("%s: %d: Invalid detection payload, event_size %d status %d",
            __func__, __LINE__, event_size, generic_info->status);
        return nullptr;
    }

    if (sm_info_map_.size() == 1) {
        return sm_info_map_.begin()->first;
    }

    ALOGI("%s: %d: event_size = %u", __func__, __LINE__, event_size);
    while (parsed_size < event_size) {
        ALOGD("%s: %d: parsed_size = %u, event_size = %u",
            __func__, __LINE__, parsed_size, event_size);
        event_header = (struct detection_event_info_header_t *)ptr;
        keyId = event_header->key_id;
        payload_size = event_header->payload_size;
        ptr += sizeof(struct detection_event_info_header_t);
        parsed_size += sizeof(struct detection_event_info_header_t);

        switch (keyId) {
        case KEY_ID_CONFIDENCE_LEVELS_INFO:
            confidence_info = (struct confidence_level_info_t *)ptr;
            num_confidence_levels =
                confidence_info->number_of_confidence_values;
            ALOGI("%s: %d: num_confidence_levels = %u",
                __func__, __LINE__, num_confidence_levels);
            for (int i = 0; i < num_confidence_levels; i++) {
                confidence_levels[i] =
                    confidence_info->confidence_levels[i];
                ALOGI("%s: %d: confidence_levels[%d] = %u",
                    __func__, __LINE__, i, confidence_levels[i]);
            }
            parsed_size = event_size; //break out of while loop
            break;
        case KEY_ID_KWD_POSITION_INFO:
        case KEY_ID_TIMESTAMP_INFO:
        case KEY_ID_FTRT_DATA_INFO:
            break;
        default:
            ALOGE("%s: %d: Invalid key id %u status %d",
                __func__, __LINE__, keyId, status);
            return nullptr;
        }
        ptr += payload_size;
        parsed_size += payload_size;
    }

    if (num_confidence_levels < sound_model_info_->GetNumKeyPhrases()) {
        ALOGE("%s: %d: detection event conf levels %d < num of keyphrases %d",
            __func__, __LINE__, num_confidence_levels,
            sound_model_info_->GetNumKeyPhrases());
        return nullptr;
    }

    /*
     * The DSP payload contains the keyword conf levels from the beginning.
     * Only one keyword conf level is expected to be non-zero from keyword
     * detection. Find non-zero conf level up to number of keyphrases and
     * if one is found, match it to the corresponding keyphrase from list
     * of streams to obtain the detected stream.
     */
    for (uint32_t i = 0; i < sound_model_info_->GetNumKeyPhrases(); i++) {
        if (!confidence_levels[i])
            continue;
        for (auto &iter: sm_info_map_) {
            for (uint32_t k = 0; k < iter.second->info->GetNumKeyPhrases(); k++) {
                if (!strcmp(sound_model_info_->GetKeyPhrases()[i],
                            iter.second->info->GetKeyPhrases()[k])) {
                    return iter.first;
                }
            }
        }
    }

    return nullptr;
}

void* SVAInterface::GetDetectedStream(void *event) {

    ALOGD("%s: %d: Enter", __func__, __LINE__);
    if (sm_info_map_.empty()) {
        ALOGE("%s: %d: Unexpected, No streams attached to engine!",
            __func__, __LINE__);
        return nullptr;
    }
    if (IS_MODULE_TYPE_PDK(module_type_)) {
         return GetPDKDetectedStream(event);
    } else {
         return GetGMMDetectedStream(event);
    }
}

void* SVAInterface::GetDetectionEventInfo(void *s) {
    if (IS_MODULE_TYPE_PDK(module_type_)) {
       return &det_event_info_[s]->pdk_event_info_;
    }
    return &det_event_info_[s]->event_info_;
}

void SVAInterface::InitCallbackConfLevels(uint32_t version,
    uint8_t *opaque_data, uint32_t opaque_data_size) {

    if (!opaque_data) {
        ALOGE("%s: %d: invalid input params", __func__, __LINE__);
        return;
    }

    int i = 0, j = 0, k = 0, num_user_levels = 0;
    struct st_confidence_levels_info_v2 *conf_levels_v2_opaque = nullptr;
    struct st_confidence_levels_info *conf_levels_opaque = nullptr;

    memset(opaque_data, 0, opaque_data_size);

    if (version != CONF_LEVELS_INTF_VERSION_0002) {
        if (!st_conf_levels_) {
            ALOGD("%s: %d: invalid conf levels", __func__, __LINE__);
            return;
        }
        conf_levels_opaque = (struct st_confidence_levels_info *)opaque_data;
        conf_levels_opaque->num_sound_models = st_conf_levels_->num_sound_models;

        for (i = 0; i < st_conf_levels_->num_sound_models; i++) {
            conf_levels_opaque->conf_levels[i].sm_id =
            st_conf_levels_->conf_levels[i].sm_id;

            conf_levels_opaque->conf_levels[i].num_kw_levels =
            st_conf_levels_->conf_levels[i].num_kw_levels;
            for (j = 0; j < st_conf_levels_->conf_levels[i].num_kw_levels; j++) {

                 conf_levels_opaque->conf_levels[i].kw_levels[j].num_user_levels =
                 st_conf_levels_->conf_levels[i].kw_levels[j].num_user_levels;

                 num_user_levels = st_conf_levels_->conf_levels[i].kw_levels[j].num_user_levels;
                 for (k = 0; k < num_user_levels; k++) {
                     conf_levels_opaque->conf_levels[i].kw_levels[j].user_levels[k].user_id =
                         st_conf_levels_->conf_levels[i].kw_levels[j].user_levels[k].user_id;
                }
            }
        }
    }
    else {
        if (!st_conf_levels_v2_) {
            ALOGD("%s: %d: invalid conf levels", __func__, __LINE__);
            return;
        }
        conf_levels_v2_opaque = (struct st_confidence_levels_info_v2 *)opaque_data;
        conf_levels_v2_opaque->num_sound_models = st_conf_levels_v2_->num_sound_models;

        for (i = 0; i < st_conf_levels_v2_->num_sound_models; i++) {
            conf_levels_v2_opaque->conf_levels[i].sm_id =
            st_conf_levels_v2_->conf_levels[i].sm_id;

            conf_levels_v2_opaque->conf_levels[i].num_kw_levels =
            st_conf_levels_v2_->conf_levels[i].num_kw_levels;
            for (j = 0; j < st_conf_levels_v2_->conf_levels[i].num_kw_levels; j++) {

                 conf_levels_v2_opaque->conf_levels[i].kw_levels[j].num_user_levels =
                 st_conf_levels_v2_->conf_levels[i].kw_levels[j].num_user_levels;

                 num_user_levels = st_conf_levels_v2_->conf_levels[i].kw_levels[j].num_user_levels;
                 for (k = 0; k < num_user_levels; k++) {
                     conf_levels_v2_opaque->conf_levels[i].kw_levels[j].user_levels[k].user_id =
                         st_conf_levels_v2_->conf_levels[i].kw_levels[j].user_levels[k].user_id;
                 }
            }
        }
    }
}

int32_t SVAInterface::GenerateCallbackEvent(void *s,
    struct pal_st_recognition_event **event, uint32_t *size) {

    struct sound_model_info *sm_info = nullptr;
    struct pal_st_phrase_recognition_event *phrase_event = nullptr;
    struct pal_st_generic_recognition_event *generic_event = nullptr;
    struct st_param_header *param_hdr = nullptr;
    struct st_keyword_indices_info *kw_indices = nullptr;
    struct st_timestamp_info *timestamps = nullptr;
    struct model_stats *det_model_stat = nullptr;
    struct detection_event_info_pdk *det_ev_info_pdk = nullptr;
    struct detection_event_info *det_ev_info = nullptr;
    size_t opaque_size = 0;
    size_t event_size = 0, conf_levels_size = 0;
    uint8_t *opaque_data = nullptr;
    uint8_t *custom_event = nullptr;
    uint32_t det_keyword_id = 0;
    uint32_t best_conf_level = 0;
    uint32_t detection_timestamp_lsw = 0;
    uint32_t detection_timestamp_msw = 0;
    int32_t status = 0;
    int32_t num_models = 0;

    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        sm_info = sm_info_map_[s];
    } else {
        ALOGE("%s: %d: Stream not registered to interface", __func__, __LINE__);
        return -EINVAL;
    }

    ALOGD("%s: %d: Enter", __func__, __LINE__);
    *event = nullptr;
    if (sm_info->type == PAL_SOUND_MODEL_TYPE_KEYPHRASE) {
        if (sm_info->model_id > 0) {
            det_ev_info_pdk = &det_event_info_[s]->pdk_event_info_;
            if (!det_ev_info_pdk) {
                ALOGE("%s: %d: detection info multi model not available",
                    __func__, __LINE__);
                status = -EINVAL;
                goto exit;
            }
        } else {
            det_ev_info = &det_event_info_[s]->event_info_;
            if (!det_ev_info) {
                ALOGE("%s: %d: detection info not available",
                    __func__, __LINE__);
                status = -EINVAL;
                goto exit;
            }
        }

        if (sm_info->conf_levels_intf_version != CONF_LEVELS_INTF_VERSION_0002)
            conf_levels_size = sizeof(struct st_confidence_levels_info);
        else
            conf_levels_size = sizeof(struct st_confidence_levels_info_v2);

        opaque_size = (3 * sizeof(struct st_param_header)) +
            sizeof(struct st_timestamp_info) +
            sizeof(struct st_keyword_indices_info) +
            conf_levels_size;

        event_size = sizeof(struct pal_st_phrase_recognition_event) +
                     opaque_size;
        phrase_event = (struct pal_st_phrase_recognition_event *)
                       calloc(1, event_size);
        if (!phrase_event) {
            ALOGE("%s: %d: Failed to alloc memory for recognition event",
                __func__, __LINE__);
            status =  -ENOMEM;
            goto exit;
        }

        if (!sm_info->rec_config) {
            ALOGE("%s: %d: recognition config is NULL");
            status = -EINVAL;
            goto exit;
        }

        phrase_event->num_phrases = sm_info->rec_config->num_phrases;
        memcpy(phrase_event->phrase_extras, sm_info->rec_config->phrases,
               phrase_event->num_phrases *
               sizeof(struct pal_st_phrase_recognition_extra));

        *event = &(phrase_event->common);
        (*event)->status = sm_info->det_result;
        (*event)->type = sm_info->type;
        (*event)->st_handle = (pal_st_handle_t *)this;
        (*event)->capture_available = sm_info->rec_config->capture_requested;
        // TODO: generate capture session
        (*event)->capture_session = 0;
        (*event)->capture_delay_ms = 0;
        (*event)->capture_preamble_ms = 0;
        (*event)->trigger_in_data = true;
        (*event)->data_size = opaque_size;
        (*event)->data_offset = sizeof(struct pal_st_phrase_recognition_event);
        (*event)->media_config.sample_rate =
            str_attr_.in_media_config.sample_rate;
        (*event)->media_config.bit_width =
            str_attr_.in_media_config.bit_width;
        (*event)->media_config.ch_info.channels =
            str_attr_.in_media_config.ch_info.channels;
        (*event)->media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;
        // Filling Opaque data
        opaque_data = (uint8_t *)phrase_event +
                       phrase_event->common.data_offset;

        /* Pack the opaque data confidence levels structure */
        param_hdr = (struct st_param_header *)opaque_data;
        param_hdr->key_id = ST_PARAM_KEY_CONFIDENCE_LEVELS;
        if (sm_info->conf_levels_intf_version !=  CONF_LEVELS_INTF_VERSION_0002)
            param_hdr->payload_size = sizeof(struct st_confidence_levels_info);
        else
            param_hdr->payload_size = sizeof(struct st_confidence_levels_info_v2);
        opaque_data += sizeof(struct st_param_header);

        /* Copy the cached conf levels from recognition config */
        InitCallbackConfLevels(sm_info->conf_levels_intf_version,
            opaque_data, param_hdr->payload_size);

        if (sm_info->model_id > 0) {
            num_models = det_ev_info_pdk->num_detected_models;
            for (int i = 0; i < num_models; ++i) {
                det_model_stat = &det_ev_info_pdk->detected_model_stats[i];
                if (sm_info->model_id == det_model_stat->detected_model_id) {
                    det_keyword_id = det_model_stat->detected_keyword_id;
                    best_conf_level = det_model_stat->best_confidence_level;
                    detection_timestamp_lsw =
                        det_model_stat->detection_timestamp_lsw;
                    detection_timestamp_msw =
                        det_model_stat->detection_timestamp_msw;
                    ALOGI("%s: %d: keywordID: %u, best_conf_level: %u",
                        __func__, __LINE__, det_keyword_id, best_conf_level);
                    break;
                }
            }
            FillCallbackConfLevels(sm_info, opaque_data, det_keyword_id, best_conf_level);
        } else {
            PackEventConfLevels(sm_info, opaque_data);
        }
        opaque_data += param_hdr->payload_size;

        /* Pack the opaque data keyword indices structure */
        param_hdr = (struct st_param_header *)opaque_data;
        param_hdr->key_id = ST_PARAM_KEY_KEYWORD_INDICES;
        param_hdr->payload_size = sizeof(struct st_keyword_indices_info);
        opaque_data += sizeof(struct st_param_header);
        kw_indices = (struct st_keyword_indices_info *)opaque_data;
        kw_indices->version = 0x1;
        if (sm_info->rec_config &&
            sm_info->rec_config->capture_requested &&
            !origin_hist_buf_duration_[s]) {
            SetReadOffset(s, det_event_info_[s]->end_index_);
        } else {
            SetReadOffset(s, 0);
        }

        kw_indices->start_index = det_event_info_[s]->start_index_;
        kw_indices->end_index = det_event_info_[s]->end_index_;
        opaque_data += sizeof(struct st_keyword_indices_info);

        /*
         * Pack the opaque data detection time structure
         * TODO: add support for 2nd stage detection timestamp
         */
        param_hdr = (struct st_param_header *)opaque_data;
        param_hdr->key_id = ST_PARAM_KEY_TIMESTAMP;
        param_hdr->payload_size = sizeof(struct st_timestamp_info);
        opaque_data += sizeof(struct st_param_header);
        timestamps = (struct st_timestamp_info *)opaque_data;
        timestamps->version = 0x1;
        if (sm_info->model_id > 0) {
            timestamps->first_stage_det_event_time = 1000 *
                        ((uint64_t)detection_timestamp_lsw +
                        ((uint64_t)detection_timestamp_msw<<32));
        } else {
            timestamps->first_stage_det_event_time = 1000 *
                ((uint64_t)det_ev_info->detection_timestamp_lsw +
                ((uint64_t)det_ev_info->detection_timestamp_msw << 32));
        }
    } else if (sm_info->type == PAL_SOUND_MODEL_TYPE_GENERIC) {
        event_size = sizeof(struct pal_st_generic_recognition_event);
        generic_event = (struct pal_st_generic_recognition_event *)
                       calloc(1, event_size);
        if (!generic_event) {
            ALOGE("%s: %d: Failed to alloc memory for recognition event",
                __func__, __LINE__);
            status =  -ENOMEM;
            goto exit;
        }

        *event = &(generic_event->common);
        (*event)->status = PAL_RECOGNITION_STATUS_SUCCESS;
        (*event)->type = sm_info->type;
        (*event)->st_handle = (pal_st_handle_t *)this;
        (*event)->capture_available = sm_info->rec_config->capture_requested;
        // TODO: generate capture session
        (*event)->capture_session = 0;
        (*event)->capture_delay_ms = 0;
        (*event)->capture_preamble_ms = 0;
        (*event)->trigger_in_data = true;
        (*event)->data_size = 0;
        (*event)->data_offset = sizeof(struct pal_st_generic_recognition_event);
        (*event)->media_config.sample_rate =
            str_attr_.in_media_config.sample_rate;
        (*event)->media_config.bit_width =
            str_attr_.in_media_config.bit_width;
        (*event)->media_config.ch_info.channels =
            str_attr_.in_media_config.ch_info.channels;
        (*event)->media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;
    }
    *size = event_size;
exit:
    ALOGD("%s: %d: Exit", __func__, __LINE__);
    return status;
}

// Protected APIs
int32_t SVAInterface::ParseOpaqueConfLevels(
    struct sound_model_info *info,
    void *opaque_conf_levels,
    uint32_t version,
    uint8_t **out_conf_levels,
    uint32_t *out_num_conf_levels) {

    int32_t status = 0;
    struct st_confidence_levels_info *conf_levels = nullptr;
    struct st_confidence_levels_info_v2 *conf_levels_v2 = nullptr;
    struct st_sound_model_conf_levels *sm_levels = nullptr;
    struct st_sound_model_conf_levels_v2 *sm_levels_v2 = nullptr;
    int32_t confidence_level = 0;
    int32_t confidence_level_v2 = 0;
    bool gmm_conf_found = false;

    ALOGD("%s: %d: Enter", __func__, __LINE__);
    if (version != CONF_LEVELS_INTF_VERSION_0002) {
        conf_levels = (struct st_confidence_levels_info *)
            ((char *)opaque_conf_levels + sizeof(struct st_param_header));

        st_conf_levels_ = (struct st_confidence_levels_info *)realloc(st_conf_levels_,
                sizeof(struct st_confidence_levels_info));
        if (!st_conf_levels_) {
            ALOGE("%s: %d: failed to alloc stream conf_levels_",
                __func__, __LINE__);
            status = -ENOMEM;
            goto exit;
        }

        /* Cache to use during detection event processing */
        ar_mem_cpy((uint8_t *)st_conf_levels_, sizeof(struct st_confidence_levels_info),
            (uint8_t *)conf_levels, sizeof(struct st_confidence_levels_info));

        for (int i = 0; i < conf_levels->num_sound_models; i++) {
            sm_levels = &conf_levels->conf_levels[i];
            if (sm_levels->sm_id == ST_SM_ID_SVA_F_STAGE_GMM) {
                gmm_conf_found = true;
                status = FillOpaqueConfLevels(info->model_id, (void *)sm_levels,
                    out_conf_levels, out_num_conf_levels, version);
            } else if (sm_levels->sm_id & ST_SM_ID_SVA_S_STAGE_KWD ||
                       sm_levels->sm_id & ST_SM_ID_SVA_S_STAGE_USER) {
                confidence_level =
                    (sm_levels->sm_id & ST_SM_ID_SVA_S_STAGE_KWD) ?
                    sm_levels->kw_levels[0].kw_level:
                    sm_levels->kw_levels[0].user_levels[0].level;
                if (sm_levels->sm_id & ST_SM_ID_SVA_S_STAGE_KWD) {
                    ALOGI("%s: %d: second stage keyword confidence level = %d",
                        __func__, __LINE__, confidence_level);
                    info->sec_threshold.push_back(
                        std::make_pair(ST_SM_ID_SVA_S_STAGE_KWD, confidence_level));
                } else {
                    ALOGI("%s: %d: second stage user confidence level = %d",
                        __func__, __LINE__, confidence_level);
                    info->sec_threshold.push_back(
                        std::make_pair(ST_SM_ID_SVA_S_STAGE_USER, confidence_level));
                }
            }
        }
    } else {
        conf_levels_v2 = (struct st_confidence_levels_info_v2 *)
            ((char *)opaque_conf_levels + sizeof(struct st_param_header));

        st_conf_levels_v2_ = (struct st_confidence_levels_info_v2 *)realloc(st_conf_levels_v2_,
            sizeof(struct st_confidence_levels_info_v2));
        if (!st_conf_levels_v2_) {
            ALOGE("%s: %d: failed to alloc stream conf_levels_",
                __func__, __LINE__);
            status = -ENOMEM;
            goto exit;
        }

        /* Cache to use during detection event processing */
        ar_mem_cpy((uint8_t *)st_conf_levels_v2_, sizeof(struct st_confidence_levels_info_v2),
            (uint8_t *)conf_levels_v2, sizeof(struct st_confidence_levels_info_v2));

        for (int i = 0; i < conf_levels_v2->num_sound_models; i++) {
            sm_levels_v2 = &conf_levels_v2->conf_levels[i];
            if (sm_levels_v2->sm_id == ST_SM_ID_SVA_F_STAGE_GMM) {
                gmm_conf_found = true;
                status = FillOpaqueConfLevels(info->model_id, (void *)sm_levels_v2,
                    out_conf_levels, out_num_conf_levels, version);
            } else if (sm_levels_v2->sm_id & ST_SM_ID_SVA_S_STAGE_KWD ||
                       sm_levels_v2->sm_id & ST_SM_ID_SVA_S_STAGE_USER) {
                confidence_level_v2 =
                    (sm_levels_v2->sm_id & ST_SM_ID_SVA_S_STAGE_KWD) ?
                    sm_levels_v2->kw_levels[0].kw_level:
                    sm_levels_v2->kw_levels[0].user_levels[0].level;
                if (sm_levels_v2->sm_id & ST_SM_ID_SVA_S_STAGE_KWD) {
                    ALOGI("%s: %d: second stage keyword confidence level = %d",
                        __func__, __LINE__, confidence_level_v2);
                    info->sec_threshold.push_back(
                        std::make_pair(ST_SM_ID_SVA_S_STAGE_KWD, confidence_level_v2));
                } else {
                    ALOGI("%s: %d: second stage user confidence level = %d",
                        __func__, __LINE__, confidence_level_v2);
                    info->sec_threshold.push_back(
                        std::make_pair(ST_SM_ID_SVA_S_STAGE_USER, confidence_level_v2));
                }
            }
        }
    }

    if (!gmm_conf_found || status) {
        ALOGE("%s: %d: Did not receive GMM confidence threshold, error!",
            __func__, __LINE__);
        status = -EINVAL;
    }

exit:
    ALOGD("%s: %d: Exit", __func__, __LINE__);

    return status;
}

int32_t SVAInterface::FillConfLevels(
    struct sound_model_info *info,
    struct pal_st_recognition_config *config,
    uint8_t **out_conf_levels,
    uint32_t *out_num_conf_levels) {

    int32_t status = 0;
    uint32_t num_conf_levels = 0;
    unsigned int user_level, user_id;
    unsigned int i = 0, j = 0;
    uint8_t *conf_levels = nullptr;
    unsigned char *user_id_tracker = nullptr;
    struct pal_st_phrase_sound_model *phrase_sm = nullptr;

    ALOGD("%s: %d: Enter", __func__, __LINE__);

    if (!config) {
        status = -EINVAL;
        ALOGE("%s: %d: invalid input status %d", __func__, __LINE__, status);
        goto exit;
    }

    phrase_sm = (struct pal_st_phrase_sound_model *)info->model;

    if ((config->num_phrases == 0) ||
        (phrase_sm && config->num_phrases > phrase_sm->num_phrases)) {
        status = -EINVAL;
        ALOGE("%s: %d: Invalid phrase data status %d",
            __func__, __LINE__, status);
        goto exit;
    }

    for (i = 0; i < config->num_phrases; i++) {
        num_conf_levels++;
        if (info->model_id == 0) {
            for (j = 0; j < config->phrases[i].num_levels; j++)
                num_conf_levels++;
        }
    }

    conf_levels = (unsigned char*)calloc(1, num_conf_levels);
    if (!conf_levels) {
        status = -ENOMEM;
        ALOGE("%s: %d: conf_levels calloc failed, status %d",
            __func__, __LINE__, status);
        goto exit;
    }

    user_id_tracker = (unsigned char *)calloc(1, num_conf_levels);
    if (!user_id_tracker) {
        status = -ENOMEM;
        ALOGE("%s: %d: failed to allocate user_id_tracker status %d",
            __func__, __LINE__, status);
        goto exit;
    }

    for (i = 0; i < config->num_phrases; i++) {
        ALOGV("%s: %d: [%d] kw level %d",
            __func__, __LINE__, i, config->phrases[i].confidence_level);
        if (config->phrases[i].confidence_level > ST_MAX_FSTAGE_CONF_LEVEL) {
            ALOGE("%s: %d: Invalid kw level %d", __func__, __LINE__,
                config->phrases[i].confidence_level);
            status = -EINVAL;
            goto exit;
        }
        for (j = 0; j < config->phrases[i].num_levels; j++) {
            ALOGV("%s: %d: [%d] user_id %d level %d ", __func__, __LINE__,
                i, config->phrases[i].levels[j].user_id,
                config->phrases[i].levels[j].level);
            if (config->phrases[i].levels[j].level > ST_MAX_FSTAGE_CONF_LEVEL) {
                ALOGE("%s: %d: Invalid user level %d", __func__, __LINE__,
                    config->phrases[i].levels[j].level);
                status = -EINVAL;
                goto exit;
            }
        }
    }

    /* Example: Say the recognition structure has 3 keywords with users
     *      [0] k1 |uid|
     *              [0] u1 - 1st trainer
     *              [1] u2 - 4th trainer
     *              [3] u3 - 3rd trainer
     *      [1] k2
     *              [2] u2 - 2nd trainer
     *              [4] u3 - 5th trainer
     *      [2] k3
     *              [5] u4 - 6th trainer
     *    Output confidence level array will be
     *    [k1, k2, k3, u1k1, u2k1, u2k2, u3k1, u3k2, u4k3]
     */

    for (i = 0; i < config->num_phrases; i++) {
        conf_levels[i] = config->phrases[i].confidence_level;
        if (info->model_id == 0) {
            for (j = 0; j < config->phrases[i].num_levels; j++) {
                user_level = config->phrases[i].levels[j].level;
                user_id = config->phrases[i].levels[j].user_id;
                if ((user_id < config->num_phrases) ||
                     (user_id >= num_conf_levels)) {
                    status = -EINVAL;
                    ALOGE("%s: %d: Invalid params user id %d status %d",
                        __func__, __LINE__, user_id, status);
                    goto exit;
                } else {
                    if (user_id_tracker[user_id] == 1) {
                        status = -EINVAL;
                        ALOGE("%s: %d: Duplicate user id %d status %d",
                            __func__, __LINE__, user_id, status);
                        goto exit;
                    }
                    conf_levels[user_id] = (user_level < ST_MAX_FSTAGE_CONF_LEVEL) ?
                        user_level : ST_MAX_FSTAGE_CONF_LEVEL;
                    user_id_tracker[user_id] = 1;
                    ALOGV("%s: %d: user_conf_levels[%d] = %d",
                        __func__, __LINE__, user_id, conf_levels[user_id]);
                }
            }
        }
    }

    *out_conf_levels = conf_levels;
    *out_num_conf_levels = num_conf_levels;

exit:
    if (status && conf_levels) {
        free(conf_levels);
        *out_conf_levels = nullptr;
        *out_num_conf_levels = 0;
    }

    if (user_id_tracker)
        free(user_id_tracker);

    ALOGD("%s: %d: Exit, status %d", __func__, __LINE__, status);

    return status;
}

int32_t SVAInterface::FillOpaqueConfLevels(
    uint32_t model_id,
    const void *sm_levels_generic,
    uint8_t **out_payload,
    uint32_t *out_payload_size,
    uint32_t version) {

    int status = 0;
    int32_t level = 0;
    unsigned int num_conf_levels = 0;
    unsigned int user_level = 0, user_id = 0;
    unsigned char *conf_levels = nullptr;
    unsigned int i = 0, j = 0;
    unsigned char *user_id_tracker = nullptr;
    struct st_sound_model_conf_levels *sm_levels = nullptr;
    struct st_sound_model_conf_levels_v2 *sm_levels_v2 = nullptr;

    ALOGV("%s: %d: Enter", __func__, __LINE__);

    /*  Example: Say the recognition structure has 3 keywords with users
     *  |kid|
     *  [0] k1 |uid|
     *         [3] u1 - 1st trainer
     *         [4] u2 - 4th trainer
     *         [6] u3 - 3rd trainer
     *  [1] k2
     *         [5] u2 - 2nd trainer
     *         [7] u3 - 5th trainer
     *  [2] k3
     *         [8] u4 - 6th trainer
     *
     *  Output confidence level array will be
     *  [k1, k2, k3, u1k1, u2k1, u2k2, u3k1, u3k2, u4k3]
     */

    if (version != CONF_LEVELS_INTF_VERSION_0002) {
        sm_levels = (struct st_sound_model_conf_levels *)sm_levels_generic;
        if (!sm_levels) {
            status = -EINVAL;
            ALOGE("%s: %d: ERROR. Invalid inputs", __func__, __LINE__);
            goto exit;
        }

        for (i = 0; i < sm_levels->num_kw_levels; i++) {
            level = sm_levels->kw_levels[i].kw_level;
            if (level < 0 || level > ST_MAX_FSTAGE_CONF_LEVEL) {
                ALOGE("%s: %d: Invalid First stage [%d] kw level %d",
                    __func__, __LINE__, i, level);
                status = -EINVAL;
                goto exit;
            } else {
                ALOGD("%s: %d: First stage [%d] kw level %d",
                    __func__, __LINE__, i, level);
            }
            for (j = 0; j < sm_levels->kw_levels[i].num_user_levels; j++) {
                level = sm_levels->kw_levels[i].user_levels[j].level;
                if (level < 0 || level > ST_MAX_FSTAGE_CONF_LEVEL) {
                    ALOGE("%s: %d: Invalid First stage [%d] user_id %d level %d",
                        __func__, __LINE__, i,
                        sm_levels->kw_levels[i].user_levels[j].user_id, level);
                    status = -EINVAL;
                    goto exit;
                } else {
                    ALOGD("%s: %d: First stage [%d] user_id %d level %d ",
                        __func__, __LINE__, i,
                        sm_levels->kw_levels[i].user_levels[j].user_id, level);
                }
            }
        }

        for (i = 0; i < sm_levels->num_kw_levels; i++) {
            num_conf_levels++;
            if (model_id == 0) {
                for (j = 0; j < sm_levels->kw_levels[i].num_user_levels; j++)
                    num_conf_levels++;
            }
        }

        ALOGD("%s: %d: Number of confidence levels : %d",
            __func__, __LINE__, num_conf_levels);

        if (!num_conf_levels) {
            status = -EINVAL;
            ALOGE("%s: %d: ERROR. Invalid num_conf_levels input",
                __func__, __LINE__);
            goto exit;
        }

        conf_levels = (unsigned char*)calloc(1, num_conf_levels);
        if (!conf_levels) {
            status = -ENOMEM;
            ALOGE("%s: %d: conf_levels calloc failed, status %d",
                __func__, __LINE__, status);
            goto exit;
        }

        user_id_tracker = (unsigned char *)calloc(1, num_conf_levels);
        if (!user_id_tracker) {
            status = -ENOMEM;
            ALOGE("%s: %d: failed to allocate user_id_tracker status %d",
                __func__, __LINE__, status);
            goto exit;
        }

        for (i = 0; i < sm_levels->num_kw_levels; i++) {
            if (i < num_conf_levels) {
                conf_levels[i] = sm_levels->kw_levels[i].kw_level;
            } else {
                status = -EINVAL;
                ALOGE("%s: %d: ERROR. Invalid numver of kw levels",
                    __func__, __LINE__);
                goto exit;
            }
            if (model_id == 0) {
                for (j = 0; j < sm_levels->kw_levels[i].num_user_levels; j++) {
                    user_level = sm_levels->kw_levels[i].user_levels[j].level;
                    user_id = sm_levels->kw_levels[i].user_levels[j].user_id;
                    if ((user_id < sm_levels->num_kw_levels) ||
                        (user_id >= num_conf_levels)) {
                        status = -EINVAL;
                        ALOGE("%s: %d: ERROR. Invalid params user id %d > %d",
                            __func__, __LINE__, user_id, num_conf_levels);
                        goto exit;
                    } else {
                        if (user_id_tracker[user_id] == 1) {
                            status = -EINVAL;
                            ALOGE("%s: %d: ERROR. Duplicate user id %d",
                                __func__, __LINE__, user_id);
                            goto exit;
                        }
                        conf_levels[user_id] = user_level;
                        user_id_tracker[user_id] = 1;
                        ALOGE("%s: %d: user_conf_levels[%d] = %d",
                            __func__, __LINE__, user_id, conf_levels[user_id]);
                    }
                }
            }
        }
    } else {
        sm_levels_v2 =
            (struct st_sound_model_conf_levels_v2 *)sm_levels_generic;
        if (!sm_levels_v2) {
            status = -EINVAL;
            ALOGE("%s: %d: ERROR. Invalid inputs", __func__, __LINE__);
            goto exit;
        }

        for (i = 0; i < sm_levels_v2->num_kw_levels; i++) {
            level = sm_levels_v2->kw_levels[i].kw_level;
            if (level < 0 || level > ST_MAX_FSTAGE_CONF_LEVEL) {
                ALOGE("%s: %d: Invalid First stage [%d] kw level %d",
                    __func__, __LINE__, i, level);
                status = -EINVAL;
                goto exit;
            } else {
                ALOGD("%s: %d: First stage [%d] kw level %d",
                    __func__, __LINE__, i, level);
            }
            for (j = 0; j < sm_levels_v2->kw_levels[i].num_user_levels; j++) {
                level = sm_levels_v2->kw_levels[i].user_levels[j].level;
                if (level < 0 || level > ST_MAX_FSTAGE_CONF_LEVEL) {
                    ALOGE("%s: %d: Invalid First stage [%d] user_id %d level %d",
                        __func__, __LINE__, i,
                        sm_levels_v2->kw_levels[i].user_levels[j].user_id, level);
                    status = -EINVAL;
                    goto exit;
                } else {
                    ALOGD("%s: %d: First stage [%d] user_id %d level %d ",
                        __func__, __LINE__, i,
                        sm_levels_v2->kw_levels[i].user_levels[j].user_id, level);
                }
            }
        }

        for (i = 0; i < sm_levels_v2->num_kw_levels; i++) {
            num_conf_levels++;
            if (model_id == 0) {
                for (j = 0; j < sm_levels_v2->kw_levels[i].num_user_levels; j++)
                    num_conf_levels++;
            }
        }

        ALOGD("%s: %d: number of confidence levels : %d",
            __func__, __LINE__, num_conf_levels);

        if (!num_conf_levels) {
            status = -EINVAL;
            ALOGE("%s: %d: ERROR. Invalid num_conf_levels input",
                __func__, __LINE__);
            goto exit;
        }

        conf_levels = (unsigned char*)calloc(1, num_conf_levels);
        if (!conf_levels) {
            status = -ENOMEM;
            ALOGE("%s: %d: conf_levels calloc failed, status %d",
                __func__, __LINE__, status);
            goto exit;
        }

        user_id_tracker = (unsigned char *)calloc(1, num_conf_levels);
        if (!user_id_tracker) {
            status = -ENOMEM;
            ALOGE("%s: %d: failed to allocate user_id_tracker status %d",
                __func__, __LINE__, status);
            goto exit;
        }

        for (i = 0; i < sm_levels_v2->num_kw_levels; i++) {
            if (i < num_conf_levels) {
                conf_levels[i] = sm_levels_v2->kw_levels[i].kw_level;
            } else {
                status = -EINVAL;
                ALOGE("%s: %d: ERROR. Invalid numver of kw levels",
                    __func__, __LINE__);
                goto exit;
            }
            if (model_id == 0) {
                for (j = 0; j < sm_levels_v2->kw_levels[i].num_user_levels; j++) {
                    user_level = sm_levels_v2->kw_levels[i].user_levels[j].level;
                    user_id = sm_levels_v2->kw_levels[i].user_levels[j].user_id;
                    if ((user_id < sm_levels_v2->num_kw_levels) ||
                         (user_id >= num_conf_levels)) {
                        status = -EINVAL;
                        ALOGE("%s: %d: ERROR. Invalid params user id %d > %d",
                            __func__, __LINE__, user_id, num_conf_levels);
                        goto exit;
                    } else {
                        if (user_id_tracker[user_id] == 1) {
                            status = -EINVAL;
                            ALOGE("%s: %d: ERROR. Duplicate user id %d",
                                __func__, __LINE__, user_id);
                            goto exit;
                        }
                        conf_levels[user_id] = user_level;
                        user_id_tracker[user_id] = 1;
                        ALOGV("%s: %d: user_conf_levels[%d] = %d",
                            __func__, __LINE__, user_id, conf_levels[user_id]);
                    }
                }
            }
        }
    }

    *out_payload = conf_levels;
    *out_payload_size = num_conf_levels;
    ALOGD("%s: %d: Returning number of conf levels : %d",
        __func__, __LINE__, *out_payload_size);
exit:
    if (status && conf_levels) {
        free(conf_levels);
        *out_payload = nullptr;
        *out_payload_size = 0;
    }

    if (user_id_tracker)
        free(user_id_tracker);

    ALOGD("%s: %d: Exit, status %d", __func__, __LINE__, status);
    return status;
}

int32_t SVAInterface::ParseDetectionPayloadPDK(void *s, void *event_data) {
    int32_t status = 0;
    uint32_t payload_size = 0;
    uint32_t parsed_size = 0;
    uint32_t event_size = 0;
    uint32_t keyId = 0;
    uint64_t kwd_start_timestamp = 0;
    uint64_t kwd_end_timestamp = 0;
    uint64_t ftrt_start_timestamp = 0;
    uint8_t *ptr = nullptr;
    struct event_id_detection_engine_generic_info_t *generic_info = nullptr;
    struct detection_event_info_header_t *event_header = nullptr;
    struct ftrt_data_info_t *ftrt_info = nullptr;
    struct voice_ui_multi_model_result_info_t *multi_model_result = nullptr;
    struct model_stats *model_stat = nullptr;
    struct model_stats *detected_model_stat = nullptr;

    ALOGD("%s: %d: Enter", __func__, __LINE__);
    if (!event_data) {
        ALOGE("%s: %d: Invalid event data", __func__, __LINE__);
        return -EINVAL;
    }
    generic_info = (struct event_id_detection_engine_generic_info_t *) event_data;
    payload_size = sizeof(struct event_id_detection_engine_generic_info_t);
    event_size = generic_info->payload_size;
    ptr = (uint8_t *)event_data + payload_size;
    if (!event_size) {
        ALOGE("%s: %d: Invalid detection payload", __func__, __LINE__);
        return -EINVAL;
    }
    struct detection_event* det_ev =
        (struct detection_event*)calloc(1, sizeof(struct detection_event));
    if (det_ev == nullptr) {
        ALOGE("%s: %d: Failed to allocate memory for event",
            __func__, __LINE__);
        return -ENOMEM;
    }
    ALOGI("%s: %d: event_size = %u", __func__, __LINE__, event_size);

    while (parsed_size < event_size) {
        ALOGD("%s: %d: parsed_size = %u, event_size = %u",
            __func__, __LINE__, parsed_size, event_size);
        event_header = (struct detection_event_info_header_t *)ptr;
        keyId = event_header->key_id;
        payload_size = event_header->payload_size;
        ptr += sizeof(struct detection_event_info_header_t);
        parsed_size += sizeof(struct detection_event_info_header_t);

        switch (keyId) {
            case KEY_ID_FTRT_DATA_INFO :
                ALOGI("%s: %d: ftrt structure size: %u",
                    __func__, __LINE__, payload_size);

                ftrt_info = (struct ftrt_data_info_t *)ptr;
                det_ev->pdk_event_info_.ftrt_data_length_in_us =
                                        ftrt_info->ftrt_data_length_in_us;
                ALOGI("%s: %d: ftrt_data_length_in_us = %u", __func__, __LINE__,
                det_ev->pdk_event_info_.ftrt_data_length_in_us);
                det_ev->ftrt_size_us_ = ftrt_info->ftrt_data_length_in_us;
                break;

            case KEY_ID_VOICE_UI_MULTI_MODEL_RESULT_INFO :
                ALOGI("%s: %d: voice_ui_multi_model_result_info: %u",
                    __func__, __LINE__, payload_size );

                multi_model_result = (struct voice_ui_multi_model_result_info_t *)
                                      ptr;
                det_ev->pdk_event_info_.num_detected_models =
                    multi_model_result->num_detected_models;
                ALOGI("%s: %d: Number of detected models: %d",
                    __func__, __LINE__,
                    det_ev->pdk_event_info_.num_detected_models);

                model_stat = (struct model_stats *)(ptr +
                             sizeof(struct voice_ui_multi_model_result_info_t));
                det_ev->det_model_id_ = model_stat->detected_model_id;
                for (int i = 0; i < det_ev->pdk_event_info_.
                                    num_detected_models; ++i) {

                    det_ev->pdk_event_info_.detected_model_stats[i].
                    detected_model_id = model_stat->detected_model_id;

                    det_ev->pdk_event_info_.detected_model_stats[i].
                    detected_keyword_id = model_stat->detected_keyword_id;
                    ALOGI("%s: %d: detected keyword id: %u",
                        __func__, __LINE__,
                        det_ev->pdk_event_info_.detected_model_stats[i].
                            detected_keyword_id);

                    det_ev->pdk_event_info_.detected_model_stats[i].
                    best_channel_idx = model_stat->best_channel_idx;

                    det_ev->pdk_event_info_.detected_model_stats[i].
                    best_confidence_level = model_stat->best_confidence_level;
                    ALOGI("%s: %d: detected best conf level: %u",
                        __func__, __LINE__,
                        det_ev->pdk_event_info_.detected_model_stats[i].
                            best_confidence_level);

                    det_ev->pdk_event_info_.detected_model_stats[i].
                    kw_start_timestamp_lsw = model_stat->kw_start_timestamp_lsw;
                    ALOGI("%s: %d: kw_start_timestamp_lsw: %u",
                        __func__, __LINE__,
                        det_ev->pdk_event_info_.detected_model_stats[i].
                            kw_start_timestamp_lsw);

                    det_ev->pdk_event_info_.detected_model_stats[i].
                    kw_start_timestamp_msw = model_stat->kw_start_timestamp_msw;
                    ALOGI("%s: %d: kw_start_timestamp_msw: %u",
                        __func__, __LINE__,
                        det_ev->pdk_event_info_.detected_model_stats[i].
                            kw_start_timestamp_msw);

                    det_ev->pdk_event_info_.detected_model_stats[i].
                    kw_end_timestamp_lsw = model_stat->kw_end_timestamp_lsw;
                    ALOGI("%s: %d: kw_end_timestamp_lsw: %u",
                        __func__, __LINE__,
                        det_ev->pdk_event_info_.detected_model_stats[i].
                            kw_end_timestamp_lsw);


                    det_ev->pdk_event_info_.detected_model_stats[i].
                    kw_end_timestamp_msw = model_stat->kw_end_timestamp_msw;
                    ALOGI("%s: %d: kw_end_timestamp_msw: %u",
                        __func__, __LINE__,
                        det_ev->pdk_event_info_.detected_model_stats[i].
                            kw_end_timestamp_msw);


                    det_ev->pdk_event_info_.detected_model_stats[i].
                    detection_timestamp_lsw = model_stat->detection_timestamp_lsw;
                    ALOGI("%s: %d: detection_timestamp_lsw: %u",
                        __func__, __LINE__,
                        det_ev->pdk_event_info_.detected_model_stats[i].
                            detection_timestamp_lsw);

                    det_ev->pdk_event_info_.detected_model_stats[i].
                    detection_timestamp_msw = model_stat->detection_timestamp_msw;
                    ALOGI("%s: %d: detection_timestamp_msw: %u",
                        __func__, __LINE__,
                        det_ev->pdk_event_info_.detected_model_stats[i].
                            detection_timestamp_msw);

                    ALOGI("%s: %d: Detection made for model id: %x",
                        __func__, __LINE__,
                        det_ev->pdk_event_info_.detected_model_stats[i].
                            detected_model_id);
                    model_stat += sizeof(struct model_stats);
                }
                break;
            default :
                status = -EINVAL;
                ALOGE("%s: %d: Invalid key id %u status %d",
                    __func__, __LINE__, keyId, status);
                goto exit;
        }
        ptr += payload_size;
        parsed_size += payload_size;

    }

    detected_model_stat =
        &det_ev->pdk_event_info_.detected_model_stats[0];

    kwd_start_timestamp =
        (uint64_t)detected_model_stat->kw_start_timestamp_lsw +
        ((uint64_t)detected_model_stat->kw_start_timestamp_msw << 32);
    kwd_end_timestamp =
        (uint64_t)detected_model_stat->kw_end_timestamp_lsw +
        ((uint64_t)detected_model_stat->kw_end_timestamp_msw << 32);
    ftrt_start_timestamp =
        (uint64_t)detected_model_stat->detection_timestamp_lsw +
        ((uint64_t)detected_model_stat->detection_timestamp_msw << 32) -
        det_ev->pdk_event_info_.ftrt_data_length_in_us;

    det_ev->start_ts_ = kwd_start_timestamp;
    det_ev->end_ts_ = kwd_end_timestamp;
    det_event_info_[s] = det_ev;

    UpdateKeywordIndex(s, kwd_start_timestamp, kwd_end_timestamp,
        ftrt_start_timestamp);

exit:
    ALOGD("%s: %d: Exit, status %d", __func__, __LINE__, status);
    return status;
}

int32_t SVAInterface::ParseDetectionPayloadGMM(void *s, void *event_data) {
    int32_t status = 0;
    int32_t i = 0;
    uint32_t parsed_size = 0;
    uint32_t payload_size = 0;
    uint32_t event_size = 0;
    uint64_t kwd_start_timestamp = 0;
    uint64_t kwd_end_timestamp = 0;
    uint64_t ftrt_start_timestamp = 0;
    uint8_t *ptr = nullptr;
    struct event_id_detection_engine_generic_info_t *generic_info = nullptr;
    struct detection_event_info_header_t *event_header = nullptr;
    struct confidence_level_info_t *confidence_info = nullptr;
    struct keyword_position_info_t *keyword_position_info = nullptr;
    struct detection_timestamp_info_t *detection_timestamp_info = nullptr;
    struct ftrt_data_info_t *ftrt_info = nullptr;

    ALOGD("%s: %d: Enter", __func__, __LINE__);
    if (!event_data) {
        ALOGE("%s: %d: Invalid event data", __func__, __LINE__);
        return -EINVAL;
    }

    generic_info = (struct event_id_detection_engine_generic_info_t *)event_data;
    payload_size = sizeof(struct event_id_detection_engine_generic_info_t);
    event_size = generic_info->payload_size;
    ptr = (uint8_t *)event_data + payload_size;
    ALOGI("%s: %d: status = %u, event_size = %u",
        __func__, __LINE__, generic_info->status, event_size);
    if (!event_size || generic_info->status) {
        ALOGE("%s: %d: Invalid detection payload", __func__, __LINE__);
        return -EINVAL;
    }

    struct detection_event* det_ev =
        (struct detection_event*)calloc(1, sizeof(struct detection_event));
    if (det_ev == nullptr) {
        ALOGE("%s: %d: Failed to allocate memory for event",
            __func__, __LINE__);
        return -ENOMEM;
    }
    det_ev->event_info_.status = generic_info->status;

    // parse variable payload
    while (parsed_size < event_size) {
        ALOGD("%s: %d: parsed_size = %u, event_size = %u",
            __func__, __LINE__, parsed_size, event_size);
        event_header = (struct detection_event_info_header_t *)ptr;
        uint32_t keyId = event_header->key_id;
        payload_size = event_header->payload_size;
        ALOGD("%s: %d: key id = %u, payload_size = %u",
            __func__, __LINE__, keyId, payload_size);
        ptr += sizeof(struct detection_event_info_header_t);
        parsed_size += sizeof(struct detection_event_info_header_t);

        switch (keyId) {
        case KEY_ID_CONFIDENCE_LEVELS_INFO:
            confidence_info = (struct confidence_level_info_t *)ptr;
            det_ev->event_info_.num_confidence_levels =
                confidence_info->number_of_confidence_values;
            ALOGI("%s: %d: num_confidence_levels = %u",
                __func__, __LINE__, det_ev->event_info_.num_confidence_levels);
            for (i = 0; i < det_ev->event_info_.num_confidence_levels; i++) {
                det_ev->event_info_.confidence_levels[i] =
                    confidence_info->confidence_levels[i];
                ALOGI("%s: %d: confidence_levels[%d] = %u",
                    __func__, __LINE__, i,
                    det_ev->event_info_.confidence_levels[i]);
            }
            break;
        case KEY_ID_KWD_POSITION_INFO:
            keyword_position_info = (struct keyword_position_info_t *)ptr;
            det_ev->event_info_.kw_start_timestamp_lsw =
                keyword_position_info->kw_start_timestamp_lsw;
            det_ev->event_info_.kw_start_timestamp_msw =
                keyword_position_info->kw_start_timestamp_msw;
            det_ev->event_info_.kw_end_timestamp_lsw =
                keyword_position_info->kw_end_timestamp_lsw;
            det_ev->event_info_.kw_end_timestamp_msw =
                keyword_position_info->kw_end_timestamp_msw;
            ALOGI("%s: %d: start_lsw = %u, start_msw = %u, "
                    "end_lsw = %u, end_msw = %u", __func__, __LINE__,
                    det_ev->event_info_.kw_start_timestamp_lsw,
                    det_ev->event_info_.kw_start_timestamp_msw,
                    det_ev->event_info_.kw_end_timestamp_lsw,
                    det_ev->event_info_.kw_end_timestamp_msw);
            break;
        case KEY_ID_TIMESTAMP_INFO:
            detection_timestamp_info = (struct detection_timestamp_info_t *)ptr;
            det_ev->event_info_.detection_timestamp_lsw =
                detection_timestamp_info->detection_timestamp_lsw;
            det_ev->event_info_.detection_timestamp_msw =
                detection_timestamp_info->detection_timestamp_msw;
            ALOGI("%s: %d: timestamp_lsw = %u, timestamp_msw = %u",
                __func__, __LINE__,
                det_ev->event_info_.detection_timestamp_lsw,
                det_ev->event_info_.detection_timestamp_msw);
            break;
        case KEY_ID_FTRT_DATA_INFO:
            ftrt_info = (struct ftrt_data_info_t *)ptr;
            det_ev->ftrt_size_us_ = ftrt_info->ftrt_data_length_in_us;
            det_ev->event_info_.ftrt_data_length_in_us =
                ftrt_info->ftrt_data_length_in_us;
            ALOGI("%s: %d: ftrt_data_length_in_us = %u",
                __func__, __LINE__,
                det_ev->event_info_.ftrt_data_length_in_us);
            break;
        default:
            status = -EINVAL;
            ALOGE("%s: %d: Invalid key id %u status %d",
                __func__, __LINE__, keyId, status);
            goto exit;
        }
        ptr += payload_size;
        parsed_size += payload_size;
    }

    kwd_start_timestamp =
        (uint64_t)det_ev->event_info_.kw_start_timestamp_lsw +
        ((uint64_t)det_ev->event_info_.kw_start_timestamp_msw << 32);
    kwd_end_timestamp =
        (uint64_t)det_ev->event_info_.kw_end_timestamp_lsw +
        ((uint64_t)det_ev->event_info_.kw_end_timestamp_msw << 32);
    ftrt_start_timestamp =
        (uint64_t)det_ev->event_info_.detection_timestamp_lsw +
        ((uint64_t)det_ev->event_info_.detection_timestamp_msw << 32) -
        det_ev->event_info_.ftrt_data_length_in_us;

    det_ev->start_ts_ = kwd_start_timestamp;
    det_ev->end_ts_ = kwd_end_timestamp;
    det_ev->det_model_id_ = 0;
    det_event_info_[s] = det_ev;

    UpdateKeywordIndex(s, kwd_start_timestamp, kwd_end_timestamp,
        ftrt_start_timestamp);
exit:
    ALOGD("%s: %d: Exit, status %d", __func__, __LINE__, status);
    return status;
}

void SVAInterface::UpdateKeywordIndex(void *s, uint64_t kwd_start_timestamp,
                                      uint64_t kwd_end_timestamp,
                                      uint64_t ftrt_start_timestamp) {

    ALOGV("%s: %d: kwd start timestamp: %llu, kwd end timestamp: %llu",
        __func__, __LINE__,
        (long long)kwd_start_timestamp, (long long)kwd_end_timestamp);
    ALOGV("%s: %d: Ftrt data start timestamp : %llu", __func__, __LINE__,
        (long long)ftrt_start_timestamp);

    if (kwd_start_timestamp > kwd_end_timestamp) {
        ALOGD("%s: %d: Invalid timestamp, cannot compute keyword index",
            __func__, __LINE__);
        return;
    }

    if (kwd_start_timestamp > ftrt_start_timestamp) {
        det_event_info_[s]->start_index_ = UsToBytes(kwd_start_timestamp - ftrt_start_timestamp);
        det_event_info_[s]->end_index_ = UsToBytes(kwd_end_timestamp - ftrt_start_timestamp);
        ALOGI("%s: %d: start_index : %u, end_index : %u", __func__, __LINE__,
            det_event_info_[s]->start_index_, det_event_info_[s]->end_index_);
    }
   // else this could be a consecutive event. Expect the indices to be set by GSL engine later.

}

void SVAInterface::UpdateIndices(void *s, struct keyword_index index) {

    if (det_event_info_.find(s) == det_event_info_.end()) {
        ALOGE("%s: %d: Invalid stream", __func__, __LINE__);
        return;
    }

    det_event_info_[s]->start_index_ = index.start_index;
    det_event_info_[s]->end_index_ = index.end_index;
    ALOGI("%s: %d: start_index : %u, end_index : %u", __func__, __LINE__,
        det_event_info_[s]->start_index_, det_event_info_[s]->end_index_);
}

void SVAInterface::PackEventConfLevels(struct sound_model_info *sm_info,
                                       uint8_t *opaque_data) {

    struct st_confidence_levels_info *conf_levels = nullptr;
    struct st_confidence_levels_info_v2 *conf_levels_v2 = nullptr;
    uint32_t i = 0, j = 0, k = 0, user_id = 0, num_user_levels = 0;

    ALOGV("%s: %d: Enter", __func__, __LINE__);

    /*
     * Update the opaque data of callback event with confidence levels
     * accordingly for all users and keywords from the detection event
     */
    if (sm_info->conf_levels_intf_version != CONF_LEVELS_INTF_VERSION_0002) {
        conf_levels = (struct st_confidence_levels_info *)opaque_data;
        for (i = 0; i < conf_levels->num_sound_models; i++) {
            if (conf_levels->conf_levels[i].sm_id == ST_SM_ID_SVA_F_STAGE_GMM) {
                for (j = 0; j < conf_levels->conf_levels[i].num_kw_levels; j++) {
                    if (j <= sm_info->info->GetConfLevelsSize())
                        conf_levels->conf_levels[i].kw_levels[j].kw_level =
                            sm_info->info->GetDetConfLevels()[j];
                    else
                        ALOGE("%s: %d: unexpected conf size %d < %d",
                            __func__, __LINE__,
                            sm_info->info->GetConfLevelsSize(), j);

                    num_user_levels =
                        conf_levels->conf_levels[i].kw_levels[j].num_user_levels;
                    for (k = 0; k < num_user_levels; k++) {
                        user_id = conf_levels->conf_levels[i].kw_levels[j].
                            user_levels[k].user_id;
                        if (user_id <= sm_info->info->GetConfLevelsSize())
                            conf_levels->conf_levels[i].kw_levels[j].user_levels[k].
                                level = sm_info->info->GetDetConfLevels()[user_id];
                        else
                            ALOGE("%s: %d: Unexpected conf size %d < %d",
                                __func__, __LINE__,
                                sm_info->info->GetConfLevelsSize(), user_id);
                    }
                }
            } else if (conf_levels->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_KWD ||
                       conf_levels->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_USER) {
                /* Update confidence levels for second stage */
                for (auto &iter: sm_info->sec_det_level) {
                    if ((conf_levels->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_KWD) &&
                        (iter.first & ST_SM_ID_SVA_S_STAGE_KWD)) {
                        conf_levels->conf_levels[i].kw_levels[0].kw_level = iter.second;
                        conf_levels->conf_levels[i].kw_levels[0].user_levels[0].level = 0;
                    } else if ((conf_levels->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_USER) &&
                               (iter.first == conf_levels->conf_levels[i].sm_id)) {
                        conf_levels->conf_levels[i].kw_levels[0].kw_level = iter.second;
                        conf_levels->conf_levels[i].kw_levels[0].user_levels[0].level = iter.second;
                    }
                }
            }
        }
    } else {
        conf_levels_v2 = (struct st_confidence_levels_info_v2 *)opaque_data;
        for (i = 0; i < conf_levels_v2->num_sound_models; i++) {
            if (conf_levels_v2->conf_levels[i].sm_id == ST_SM_ID_SVA_F_STAGE_GMM) {
                for (j = 0; j < conf_levels_v2->conf_levels[i].num_kw_levels; j++) {
                    if (j <= sm_info->info->GetConfLevelsSize())
                            conf_levels_v2->conf_levels[i].kw_levels[j].kw_level =
                                    sm_info->info->GetDetConfLevels()[j];
                    else
                        ALOGE("%s: %d: unexpected conf size %d < %d",
                            __func__, __LINE__,
                            sm_info->info->GetConfLevelsSize(), j);

                    ALOGI("%s: %d: First stage KW Conf levels[%d]-%d",
                        __func__, __LINE__,
                        j, sm_info->info->GetDetConfLevels()[j]);

                    num_user_levels =
                        conf_levels_v2->conf_levels[i].kw_levels[j].num_user_levels;
                    for (k = 0; k < num_user_levels; k++) {
                        user_id = conf_levels_v2->conf_levels[i].kw_levels[j].
                            user_levels[k].user_id;
                        if (user_id <=  sm_info->info->GetConfLevelsSize())
                            conf_levels_v2->conf_levels[i].kw_levels[j].user_levels[k].
                                level = sm_info->info->GetDetConfLevels()[user_id];
                        else
                            ALOGE("%s: %d: Unexpected conf size %d < %d",
                                __func__, __LINE__,
                                sm_info->info->GetConfLevelsSize(), user_id);

                        ALOGI("%s: %d: First stage User Conf levels[%d]-%d",
                            __func__, __LINE__,
                            k, sm_info->info->GetDetConfLevels()[user_id]);
                    }
                }
            } else if (conf_levels_v2->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_KWD ||
                       conf_levels_v2->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_USER) {
                /* Update confidence levels for second stage */
                for (auto &iter: sm_info->sec_det_level) {
                    if ((conf_levels_v2->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_KWD) &&
                        (iter.first & ST_SM_ID_SVA_S_STAGE_KWD)) {
                        conf_levels_v2->conf_levels[i].kw_levels[0].kw_level = iter.second;
                        conf_levels_v2->conf_levels[i].kw_levels[0].user_levels[0].level = 0;
                    } else if ((conf_levels_v2->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_USER) &&
                               (iter.first == ST_SM_ID_SVA_S_STAGE_USER)) {
                        conf_levels_v2->conf_levels[i].kw_levels[0].kw_level = iter.second;
                        conf_levels_v2->conf_levels[i].kw_levels[0].user_levels[0].level = iter.second;
                    }
                }
            }
        }
    }
    ALOGV("%s: %d: Exit", __func__, __LINE__);
}

void SVAInterface::FillCallbackConfLevels(struct sound_model_info *sm_info,
                                          uint8_t *opaque_data,
                                          uint32_t det_keyword_id,
                                          uint32_t best_conf_level) {
    int i = 0;
    struct st_confidence_levels_info_v2 *conf_levels_v2 = nullptr;
    struct st_confidence_levels_info *conf_levels = nullptr;

    if (sm_info->conf_levels_intf_version != CONF_LEVELS_INTF_VERSION_0002) {
        conf_levels = (struct st_confidence_levels_info *)opaque_data;
        for (i = 0; i < conf_levels->num_sound_models; i++) {
            if (conf_levels->conf_levels[i].sm_id == ST_SM_ID_SVA_F_STAGE_GMM) {
                conf_levels->conf_levels[i].kw_levels[det_keyword_id].
                    kw_level = best_conf_level;
                conf_levels->conf_levels[i].kw_levels[det_keyword_id].
                    user_levels[0].level = 0;
                ALOGI("%s: %d: First stage returning conf level : %d",
                    __func__, __LINE__, best_conf_level);
            } else if (conf_levels->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_KWD) {
                for (auto iter: sm_info->sec_det_level) {
                    if (iter.first & ST_SM_ID_SVA_S_STAGE_KWD) {
                        conf_levels->conf_levels[i].kw_levels[0].kw_level = iter.second;
                        conf_levels->conf_levels[i].kw_levels[0].user_levels[0].level = 0;
                        ALOGI("%s: %d: Second stage keyword conf level: %d",
                            __func__, __LINE__, iter.second);
                    }
                }
            } else if (conf_levels->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_USER) {
                for (auto iter: sm_info->sec_det_level) {
                    if (iter.first & ST_SM_ID_SVA_S_STAGE_USER) {
                        conf_levels->conf_levels[i].kw_levels[0].kw_level = iter.second;
                        conf_levels->conf_levels[i].kw_levels[0].user_levels[0].level = iter.second;
                        ALOGI("%s: %d: Second stage user conf level: %d",
                            __func__, __LINE__, iter.second);
                    }
                }
            }
        }
    } else {
        conf_levels_v2 = (struct st_confidence_levels_info_v2 *)opaque_data;
        for (i = 0; i < conf_levels_v2->num_sound_models; i++) {
            if (conf_levels_v2->conf_levels[i].sm_id == ST_SM_ID_SVA_F_STAGE_GMM) {
                conf_levels_v2->conf_levels[i].kw_levels[det_keyword_id].
                    kw_level = best_conf_level;
                conf_levels_v2->conf_levels[i].kw_levels[det_keyword_id].
                    user_levels[0].level = 0;
                ALOGI("%s: %d: First stage returning conf level: %d",
                    __func__, __LINE__, best_conf_level);
            } else if (conf_levels_v2->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_KWD) {
                for (auto iter: sm_info->sec_det_level) {
                    if (iter.first & ST_SM_ID_SVA_S_STAGE_KWD) {
                        conf_levels_v2->conf_levels[i].kw_levels[0].kw_level = iter.second;
                        conf_levels_v2->conf_levels[i].kw_levels[0].user_levels[0].level = 0;
                        ALOGI("%s: %d: Second stage keyword conf level: %d",
                            __func__, __LINE__, iter.second);
                    }
                }
            } else if (conf_levels_v2->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_USER) {
                for (auto iter: sm_info->sec_det_level) {
                    if (iter.first & ST_SM_ID_SVA_S_STAGE_USER) {
                        conf_levels_v2->conf_levels[i].kw_levels[0].kw_level = iter.second;
                        conf_levels_v2->conf_levels[i].kw_levels[0].user_levels[0].level = iter.second;
                        ALOGI("%s: %d: Second stage user conf level: %d",
                            __func__, __LINE__, iter.second);
                    }
                }
            }
        }
    }
}

void SVAInterface::CheckAndSetDetectionConfLevels(void *s) {
    ALOGD("%s: %d: Enter", __func__, __LINE__);

    if (!s) {
        ALOGE("%s: %d: Invalid detected stream", __func__, __LINE__);
        return;
    }

    if (det_event_info_[s]->event_info_.num_confidence_levels <
        sound_model_info_->GetConfLevelsSize()) {
        ALOGE("%s: %d: detection event conf lvls %d < eng conf lvl size %d",
            __func__, __LINE__,
            det_event_info_[s]->event_info_.num_confidence_levels,
            sound_model_info_->GetConfLevelsSize());
        return;
    }
    /* Reset any cached previous detection conf level values */
    sm_info_map_[s]->info->ResetDetConfLevels();

    /* Extract the stream conf level values from SPF detection payload */
    for (uint32_t i = 0; i < sound_model_info_->GetConfLevelsSize(); i++) {
        if (!det_event_info_[s]->event_info_.confidence_levels[i])
            continue;
        for (uint32_t j = 0; j < sm_info_map_[s]->info->GetConfLevelsSize(); j++) {
            if (!strcmp(sm_info_map_[s]->info->GetConfLevelsKwUsers()[j],
                 sound_model_info_->GetConfLevelsKwUsers()[i])) {
                 sm_info_map_[s]->info->UpdateDetConfLevel(j,
                   det_event_info_[s]->event_info_.confidence_levels[i]);
            }
        }
    }

    for (uint32_t i = 0; i < sm_info_map_[s]->info->GetConfLevelsSize(); i++)
        ALOGI("%s: %d: det_cf_levels[%d]-%d", __func__, __LINE__, i,
            sm_info_map_[s]->info->GetDetConfLevels()[i]);
}

int32_t SVAInterface::QuerySoundModel(
    SoundModelInfo *sm_info,
    uint8_t *data,
    uint32_t data_size) {

    listen_sound_model_header sml_header = {};
    listen_model_type model = {};
    listen_status_enum sml_ret = kSucess;
    uint32_t status = 0;
    std::shared_ptr<SoundModelLib>sml = SoundModelLib::GetInstance();

    ALOGV("%s: %d: Enter: sound model size %d", __func__, __LINE__, data_size);

    if (!sml || !sm_info) {
        ALOGE("%s: %d: soundmodel lib handle or model info NULL",
            __func__, __LINE__);
        return -ENOSYS;
    }

    model.data = data;
    model.size = data_size;

    sml_ret = sml->GetSoundModelHeader_(&model, &sml_header);
    if (sml_ret != kSucess) {
        ALOGE("%s: %d: GetSoundModelHeader_ failed, err %d",
            __func__, __LINE__, sml_ret);
        return -EINVAL;
    }
    if (sml_header.numKeywords == 0) {
        ALOGE("%s: %d: num keywords zero!", __func__, __LINE__);
        return -EINVAL;
    }

    if (sml_header.numActiveUserKeywordPairs < sml_header.numUsers) {
        ALOGE("%s: %d: smlib activeUserKwPairs(%d) < total users (%d)",
            __func__, __LINE__,
            sml_header.numActiveUserKeywordPairs, sml_header.numUsers);
        goto cleanup;
    }
    if (sml_header.numUsers && !sml_header.userKeywordPairFlags) {
        ALOGE("%s: %d: userKeywordPairFlags is NULL, numUsers (%d)",
            __func__, __LINE__, sml_header.numUsers);
        goto cleanup;
    }

    ALOGV("%s: %d: SML model.data %pK, model.size %d",
        __func__, __LINE__, model.data, model.size);
    status = sm_info->SetKeyPhrases(&model, sml_header.numKeywords);
    if (status)
        goto cleanup;

    status = sm_info->SetUsers(&model, sml_header.numUsers);
    if (status)
        goto cleanup;

    status = sm_info->SetConfLevels(sml_header.numActiveUserKeywordPairs,
                                    sml_header.numUsersSetPerKw,
                                    sml_header.userKeywordPairFlags);
    if (status)
        goto cleanup;

    sml_ret = sml->ReleaseSoundModelHeader_(&sml_header);
    if (sml_ret != kSucess) {
        ALOGE("%s: %d: ReleaseSoundModelHeader failed, err %d",
            __func__, __LINE__, sml_ret);
        status = -EINVAL;
        goto cleanup_1;
    }
    ALOGV("%s: %d: exit", __func__, __LINE__);
    return 0;

cleanup:
    sml_ret = sml->ReleaseSoundModelHeader_(&sml_header);
    if (sml_ret != kSucess)
        ALOGE("%s: %d: ReleaseSoundModelHeader_ failed, err %d",
            __func__, __LINE__, sml_ret);

cleanup_1:
    return status;
}

int32_t SVAInterface::MergeSoundModels(
    uint32_t num_models,
    listen_model_type *in_models[],
    listen_model_type *out_model) {

    listen_status_enum sm_ret = kSucess;
    int32_t status = 0;
    std::shared_ptr<SoundModelLib>sml = SoundModelLib::GetInstance();

    if (!sml) {
        ALOGE("%s: %d: soundmodel lib handle NULL", __func__, __LINE__);
        return -ENOSYS;
    }

    ALOGV("%s: %d: num_models to merge %d", __func__, __LINE__, num_models);
    sm_ret = sml->GetMergedModelSize_(num_models, in_models,
        &out_model->size);
    if ((sm_ret != kSucess) || !out_model->size) {
        ALOGE("%s: %d: GetMergedModelSize failed, err %d, size %d",
            __func__, __LINE__, sm_ret, out_model->size);
        return -EINVAL;
    }
    ALOGI("%s: %d: merged sound model size %d",
        __func__, __LINE__, out_model->size);

    out_model->data = (uint8_t *)calloc(1, out_model->size * sizeof(char));
    if (!out_model->data) {
        ALOGE("%s: %d: Merged sound model allocation failed",
            __func__, __LINE__);
        return -ENOMEM;
    }

    sm_ret = sml->MergeModels_(num_models, in_models, out_model);
    if (sm_ret != kSucess) {
        ALOGE("%s: %d: MergeModels failed, err %d",
            __func__, __LINE__, sm_ret);
        status = -EINVAL;
        goto cleanup;
    }
    if (!out_model->data || !out_model->size) {
        ALOGE("%s: %d: MergeModels returned NULL data or size %d",
            __func__, __LINE__, out_model->size);
        status = -EINVAL;
        goto cleanup;
    }

    ALOGD("%s: %d: Exit, status: %d", __func__, __LINE__, status);
    return 0;

cleanup:
    if (out_model->data) {
        free(out_model->data);
        out_model->data = nullptr;
        out_model->size = 0;
    }
    return status;
}

int32_t SVAInterface::AddSoundModel(void *s,
                                    uint8_t *data,
                                    uint32_t data_size){

    int32_t status = 0;
    uint32_t num_models = 0;
    listen_model_type **in_models = nullptr;
    listen_model_type out_model = {};
    SoundModelInfo *sm_info = nullptr;

    ALOGV("%s: %d: Enter", __func__, __LINE__);
    if (GetSoundModelInfo(s)->GetModelData()) {
        ALOGD("%s: %d: Stream model already added", __func__, __LINE__);
        return 0;
    }

    /* Populate sound model info for the incoming stream model */
    status = QuerySoundModel(GetSoundModelInfo(s), data, data_size);
    if (status) {
        ALOGE("%s: %d: QuerySoundModel failed status: %d",
            __func__, __LINE__, status);
        return status;
    }

    GetSoundModelInfo(s)->SetModelData(data, data_size);

    /* Check for remaining stream sound models to merge */
    for (auto &iter: sm_info_map_) {
        if (s != iter.first && iter.first && GetSoundModelInfo(iter.first)->GetModelData())
             num_models++;
    }

    if (!num_models) {
        ALOGD("%s: %d: Copy model info from incoming stream to engine",
            __func__, __LINE__);
        *sound_model_info_ = *GetSoundModelInfo(s);
        sm_merged_ = false;
        return 0;
    }

    ALOGV("%s: %d: number of existing models: %d",
        __func__, __LINE__, num_models);
    /*
     * Merge this stream model with already existing merged model due to other
     * streams models.
     */
    if (!sound_model_info_) {
        ALOGE("%s: %d: eng_sm_info is NULL", __func__, __LINE__);
        status = -EINVAL;
        goto cleanup;
    }

    if (!sound_model_info_->GetModelData()) {
        if (num_models == 1) {
            /*
             * Its not a merged model yet, but engine sm_data is valid
             * and must be pointing to single stream sm_data
             */
            ALOGE("%s: %d: Model data is NULL, num_models: %d",
                __func__, __LINE__, num_models);
            status = -EINVAL;
            goto cleanup;
        } else if (!sm_merged_) {
            ALOGE("%s: %d: Unexpected, no pre-existing merged model,"
                  "num current models %d", __func__, __LINE__, num_models);
            status = -EINVAL;
            goto cleanup;
        }
    }

    /* Merge this stream model with remaining streams models */
    num_models = 2;
    SoundModelInfo::AllocArrayPtrs((char***)&in_models, num_models,
                                   sizeof(listen_model_type));
    if (!in_models) {
        ALOGE("%s: %d: in_models allocation failed", __func__, __LINE__);
        status = -ENOMEM;
        goto cleanup;
    }
    /* Add existing model */
    in_models[0]->data = sound_model_info_->GetModelData();
    in_models[0]->size = sound_model_info_->GetModelSize();
    /* Add incoming stream model */
    in_models[1]->data = data;
    in_models[1]->size = data_size;

    status = MergeSoundModels(num_models, in_models, &out_model);
    if (status) {
        ALOGE("%s: %d: merge models failed", __func__, __LINE__);
        goto cleanup;
    }
    sm_info = new SoundModelInfo();
    sm_info->SetModelData(out_model.data, out_model.size);

    /* Populate sound model info for the merged stream models */
    status = QuerySoundModel(sm_info, out_model.data, out_model.size);
    if (status) {
        goto cleanup;
    }

    if (out_model.size < sound_model_info_->GetModelSize()) {
        ALOGE("%s: %d: Unexpected, merged model sz %d < current sz %d",
            __func__, __LINE__,
            out_model.size, sound_model_info_->GetModelSize());
        status = -EINVAL;
        goto cleanup;
    }
    SoundModelInfo::FreeArrayPtrs((char **)in_models, num_models);
    in_models = nullptr;

    /* Update the new merged model */
    ALOGI("%s: %d: Updated sound model: current size %d, new size %d",
        __func__, __LINE__, sound_model_info_->GetModelSize(), out_model.size);
    *sound_model_info_ = *sm_info;
    sm_merged_ = true;

    /*
     * Sound model merge would have changed the order of merge conf levels,
     * which need to be re-updated for all current active streams, if any.
     */
    status = UpdateMergeConfLevelsWithActiveStreams();
    if (status) {
        ALOGE("%s: %d: Failed to update merge conf levels, status = %d",
            __func__, __LINE__, status);
        goto cleanup;
    }

    delete sm_info;
    ALOGD("%s: %d: Exit: status %d", __func__, __LINE__, status);
    return 0;

cleanup:
    if (out_model.data)
        free(out_model.data);

    if (in_models)
        SoundModelInfo::FreeArrayPtrs((char **)in_models, num_models);

    if (sm_info)
        delete sm_info;

    return status;
}

int32_t SVAInterface::DeleteFromMergedModel(
    char **keyphrases,
    uint32_t num_keyphrases,
    listen_model_type *in_model,
    listen_model_type *out_model) {

    listen_model_type merge_model = {};
    listen_status_enum sm_ret = kSucess;
    uint32_t out_model_sz = 0;
    int32_t status = 0;
    std::shared_ptr<SoundModelLib>sml = SoundModelLib::GetInstance();

    out_model->data = nullptr;
    out_model->size = 0;
    merge_model.data = in_model->data;
    merge_model.size = in_model->size;

    for (uint32_t i = 0; i < num_keyphrases; i++) {
        sm_ret = sml->GetSizeAfterDeleting_(&merge_model, keyphrases[i],
                                                   nullptr, &out_model_sz);
        if (sm_ret != kSucess) {
            ALOGE("%s: %d: GetSizeAfterDeleting failed %d",
                __func__, __LINE__, sm_ret);
            status = -EINVAL;
            goto cleanup;
        }
        if (out_model_sz >= in_model->size) {
            ALOGE("%s: %d: unexpected, GetSizeAfterDeleting returned size %d"
                  "not less than merged model size %d", __func__, __LINE__,
                  out_model_sz, in_model->size);
            status = -EINVAL;
            goto cleanup;
        }
        ALOGV("%s: %d: Size after deleting kw[%d] = %d",
            __func__, __LINE__, i, out_model_sz);
        if (!out_model->data) {
            /* Valid if deleting multiple keyphrases one after other */
            out_model->size = 0;
        }
        out_model->data = (uint8_t *)calloc(1, out_model_sz * sizeof(char));
        if (!out_model->data) {
            ALOGE("%s: %d: Merge sound model allocation failed, size %d",
                __func__, __LINE__, out_model_sz);
            status = -ENOMEM;
            goto cleanup;
        }
        out_model->size = out_model_sz;

        sm_ret = sml->DeleteFromModel_(&merge_model, keyphrases[i],
                                              nullptr, out_model);
        if (sm_ret != kSucess) {
            ALOGE("%s: %d: DeleteFromModel failed %d",
                __func__, __LINE__, sm_ret);
            status = -EINVAL;
            goto cleanup;
        }
        if (out_model->size != out_model_sz) {
            ALOGE("%s: %d: unexpected, out_model size %d != expected size %d",
                __func__, __LINE__, out_model->size, out_model_sz);
            status = -EINVAL;
            goto cleanup;
        }
        /* Used if deleting multiple keyphrases one after other */
        merge_model.data = out_model->data;
        merge_model.size = out_model->size;
    }

    return 0;

cleanup:
    if (out_model->data) {
        free(out_model->data);
        out_model->data = nullptr;
    }
    return status;
}

int32_t SVAInterface::DeleteSoundModel(void *s) {

    int32_t status = 0;
    uint32_t num_models = 0;
    void *rem_st = nullptr;
    listen_model_type in_model = {};
    listen_model_type out_model = {};
    SoundModelInfo *sm_info = nullptr;

    ALOGV("%s: %d: Enter", __func__, __LINE__);
    if (!GetSoundModelInfo(s)->GetModelData()) {
        ALOGI("%s: %d: Stream model data already deleted", __func__, __LINE__);
        return 0;
    }

    ALOGV("%s: %d: sm_data %pK, sm_size %d", __func__, __LINE__,
        GetSoundModelInfo(s)->GetModelData(),
        GetSoundModelInfo(s)->GetModelSize());

    /* Check for remaining streams sound models to merge */
    for (auto &iter: sm_info_map_) {
        if (s != iter.first && iter.first) {
            if (GetSoundModelInfo(iter.first) &&
                GetSoundModelInfo(iter.first)->GetModelData()) {
                rem_st = iter.first;
                num_models++;
                ALOGD("%s: %d: num_models: %d",__func__, __LINE__, num_models);
            }
        }
    }

    if (num_models == 0) {
        ALOGD("%s: %d: No remaining models", __func__, __LINE__);
        return 0;
    }
    if (num_models == 1) {
        ALOGD("%s: %d: reuse only remaining stream model, size %d",
            __func__, __LINE__, GetSoundModelInfo(rem_st)->GetModelSize());
        /* If only one remaining stream model exists, re-use it */
        *sound_model_info_ = *GetSoundModelInfo(rem_st);
        wakeup_config_.num_active_models = sound_model_info_->GetConfLevelsSize();
        for (int i = 0; i < sound_model_info_->GetConfLevelsSize(); i++) {
            if (sound_model_info_->GetConfLevels()) {
                wakeup_config_.confidence_levels[i] = sound_model_info_->GetConfLevels()[i];
                wakeup_config_.keyword_user_enables[i] =
                    (wakeup_config_.confidence_levels[i] == 100) ? 0 : 1;
                ALOGD("%s: %d: cf levels[%d] = %d",
                    __func__, __LINE__, i, wakeup_config_.confidence_levels[i]);
            }
        }
        sm_merged_ = false;
        return 0;
    }

    /*
     * Delete this stream model with already existing merged model due to other
     * streams models.
     */
    if (!sm_merged_ || !(sound_model_info_->GetModelData())) {
        ALOGE("%s: %d: Unexpected, no pre-existing merged model to delete from,"
              "num current models %d", __func__, __LINE__, num_models);
        goto cleanup;
    }

    /* Existing merged model from which the current stream model to be deleted */
    in_model.data = sound_model_info_->GetModelData();
    in_model.size = sound_model_info_->GetModelSize();

    status = DeleteFromMergedModel(GetSoundModelInfo(s)->GetKeyPhrases(),
        GetSoundModelInfo(s)->GetNumKeyPhrases(), &in_model, &out_model);

    if (status)
        goto cleanup;
    sm_info = new SoundModelInfo();
    sm_info->SetModelData(out_model.data, out_model.size);

    /* Update existing merged model info with new merged model */
    status = QuerySoundModel(sm_info, out_model.data,
                               out_model.size);
    if (status)
        goto cleanup;

    if (out_model.size > sound_model_info_->GetModelSize()) {
        ALOGE("%s: %d: Unexpected, merged model sz %d > current sz %d",
            __func__, __LINE__,
            out_model.size, sound_model_info_->GetModelSize());
        status = -EINVAL;
        goto cleanup;
    }

    ALOGI("%s: %d: Updated sound model: current size %d, new size %d",
        __func__, __LINE__,
        sound_model_info_->GetModelSize(), out_model.size);

    *sound_model_info_ = *sm_info;
    sm_merged_ = true;

    /*
     * Sound model merge would have changed the order of merge conf levels,
     * which need to be re-updated for all current active streams, if any.
     */
    status = UpdateMergeConfLevelsWithActiveStreams();
    if (status) {
        ALOGE("%s: %d: Failed to update merge conf levels, status = %d",
            __func__, __LINE__, status);
        goto cleanup;
    }

    delete sm_info;
    ALOGD("%s: %d: Exit: status %d", __func__, __LINE__, status);
    return 0;

cleanup:
    if (out_model.data)
        free(out_model.data);

    if (sm_info)
        delete sm_info;

    return status;
}

int32_t SVAInterface::UpdateEngineModel(
    void *s,
    uint8_t *data,
    uint32_t data_size,
    bool add) {

    int32_t status = 0;

    if (add)
        status = AddSoundModel(s, data, data_size);
    else
        status = DeleteSoundModel(s);

    ALOGD("%s: %d: Exit, status: %d", __func__, __LINE__, status);
    return status;
}

int32_t SVAInterface::UpdateMergeConfLevelsPayload(
    SoundModelInfo *src_sm_info,
    bool set) {

    if (!src_sm_info) {
        ALOGE("%s: %d: src sm info NULL", __func__, __LINE__);
        return -EINVAL;
    }

    if (!sm_merged_) {
        ALOGD("%s: %d: Soundmodel is not merged, use source sm info",
            __func__, __LINE__);
        *sound_model_info_ = *src_sm_info;
        for (uint32_t i = 0; i < sound_model_info_->GetConfLevelsSize(); i++) {
            if (!set) {
                sound_model_info_->UpdateConfLevel(i, MAX_CONF_LEVEL_VALUE);
                ALOGI("%s: %d: reset: cf_levels[%d] = %d", __func__, __LINE__,
                    i, sound_model_info_->GetConfLevels()[i]);
            }
        }
        return 0;
    }

    if (src_sm_info->GetConfLevelsSize() > sound_model_info_->GetConfLevelsSize()) {
        ALOGE("%s: %d: Unexpected, stream conf levels sz > eng conf levels sz",
            __func__, __LINE__);
        return -EINVAL;
    }

    for (uint32_t i = 0; i < src_sm_info->GetConfLevelsSize(); i++)
        ALOGV("%s: %d: source cf levels[%d] = %d for %s", __func__, __LINE__, i,
            src_sm_info->GetConfLevels()[i], src_sm_info->GetConfLevelsKwUsers()[i]);

    /* Populate DSP merged sound model conf levels */
    for (uint32_t i = 0; i < src_sm_info->GetConfLevelsSize(); i++) {
        for (uint32_t j = 0; j < sound_model_info_->GetConfLevelsSize(); j++) {
            if (!strcmp(sound_model_info_->GetConfLevelsKwUsers()[j],
                        src_sm_info->GetConfLevelsKwUsers()[i])) {
                if (set) {
                    sound_model_info_->UpdateConfLevel(j, src_sm_info->GetConfLevels()[i]);
                    ALOGD("%s: %d: set: cf_levels[%d] = %d", __func__, __LINE__,
                          j, sound_model_info_->GetConfLevels()[j]);
                } else {
                    sound_model_info_->UpdateConfLevel(j, MAX_CONF_LEVEL_VALUE);
                    ALOGD("%s: %d: reset: cf_levels[%d] = %d", __func__, __LINE__,
                          j, sound_model_info_->GetConfLevels()[j]);
                }
            }
        }
    }

    for (uint32_t i = 0; i < sound_model_info_->GetConfLevelsSize(); i++)
        ALOGI("%s: %d: engine cf_levels[%d] = %d", __func__, __LINE__,
            i, sound_model_info_->GetConfLevels()[i]);

    return 0;
}

int32_t SVAInterface::UpdateMergeConfLevelsWithActiveStreams() {

    int32_t status = 0;
    for (auto &iter: sm_info_map_) {
        if (iter.second->state == true) {
            ALOGV("%s: %d: update merge conf levels with other active streams",
                __func__, __LINE__);
            status = UpdateMergeConfLevelsPayload(GetSoundModelInfo(iter.first),
                        true);
            if (status)
                return status;
        }
    }
    return status;
}

void SVAInterface::UpdateDetectionResult(void *s, uint32_t result) {
    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        sm_info_map_[s]->det_result = result;
    }
}

int32_t SVAInterface::GetSoundModelLoadPayload(vui_intf_param_t *param) {
    void *s = nullptr;
    uint32_t model_size = 0;
    uint32_t fixed_size = 0;

    if (!param) {
        ALOGE("%s: %d: Invalid param", __func__, __LINE__);
        return -EINVAL;
    }

    if (!IS_MODULE_TYPE_PDK(module_type_)) {
        if (!sound_model_info_) {
            ALOGE("%s: %d: No sound model info", __func__, __LINE__);
            return -EINVAL;
        }

        param->data = (void *)sound_model_info_->GetModelData();
        param->size = sound_model_info_->GetModelSize();
    } else {
        s = param->stream;
        if (register_model_) {
            free(register_model_);
            register_model_ = nullptr;
        }
        if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
            for (auto iter: sm_info_map_[s]->model_list) {
                if ((*iter).type == ST_SM_ID_SVA_F_STAGE_GMM) {
                    model_size = (*iter).size;
                    fixed_size =
                        sizeof(param_id_detection_engine_register_multi_sound_model_t);
                    register_model_ = (param_id_detection_engine_register_multi_sound_model_t *)
                        calloc(1, fixed_size + model_size);
                    if (!register_model_) {
                        ALOGE("%s: %d: Failed to alloc memory for register_model_",
                            __func__, __LINE__);
                        return -ENOMEM;
                    }
                    register_model_->model_id = sm_info_map_[s]->model_id;
                    register_model_->model_size = model_size;
                    ar_mem_cpy(register_model_->model,
                        model_size, (*iter).data , model_size);
                    param->data = (void *)register_model_;
                    param->size = fixed_size + model_size;
                    break;
                }
            }
        } else {
            ALOGE("%s: %d: Stream not registered to interface",
                __func__, __LINE__);
            return -EINVAL;
        }
    }

    return 0;
}

int32_t SVAInterface::GetSoundModelUnloadPayload(vui_intf_param_t *param) {
    void *s = nullptr;

    if (!IS_MODULE_TYPE_PDK(module_type_)) {
        ALOGD("%s: %d: No unload payloaded needed for non-pdk model",
            __func__, __LINE__);
        return 0;
    }

    if (!param || !param->stream) {
        ALOGE("%s: %d: Invalid param or stream", __func__, __LINE__);
        return -EINVAL;
    }

    s = param->stream;
    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        deregister_model_.model_id = sm_info_map_[s]->model_id;
        param->data = (void *)&deregister_model_;
        param->size =
            sizeof(struct param_id_detection_engine_deregister_multi_sound_model_t);
    } else {
        ALOGE("%s: %d: Stream not registered to interface", __func__, __LINE__);
        return -EINVAL;
    }

    return 0;
}

int32_t SVAInterface::GetWakeUpPayload(vui_intf_param_t *param) {
    size_t fixed_wakeup_payload_size = 0;
    uint8_t *confidence_level = nullptr;
    uint8_t *kw_user_enable = nullptr;
    uint32_t *pdk_confidence_level = nullptr;
    void *s = nullptr;
    uint8_t *conf_levels = nullptr;

    if (!param) {
        ALOGE("%s: %d: Invalid param", __func__, __LINE__);
        return -EINVAL;
    }

    s = param->stream;
    if (sm_info_map_.find(s) == sm_info_map_.end()) {
        ALOGE("%s: %d: Stream not registered", __func__, __LINE__);
        return -EINVAL;
    }

    if (!IS_MODULE_TYPE_PDK(module_type_)) {
        fixed_wakeup_payload_size =
            sizeof(struct detection_engine_config_voice_wakeup) -
            PAL_SOUND_TRIGGER_MAX_USERS * 2;
        wakeup_payload_size_ = fixed_wakeup_payload_size +
            wakeup_config_.num_active_models * 2;

        if (!wakeup_payload_)
            wakeup_payload_ = (uint8_t *)calloc(1, wakeup_payload_size_);
        else
            wakeup_payload_ = (uint8_t *)realloc(wakeup_payload_, wakeup_payload_size_);
        if (!wakeup_payload_) {
            ALOGE("%s: %d: payload malloc failed %s",
                __func__, __LINE__, strerror(errno));
            return -EINVAL;
        }

        ar_mem_cpy(wakeup_payload_, fixed_wakeup_payload_size,
            &wakeup_config_, fixed_wakeup_payload_size);

        confidence_level = wakeup_payload_ + fixed_wakeup_payload_size;
        kw_user_enable = wakeup_payload_ + fixed_wakeup_payload_size +
            wakeup_config_.num_active_models;
        for (int i = 0; i < wakeup_config_.num_active_models; i++) {
            confidence_level[i] = wakeup_config_.confidence_levels[i];
            kw_user_enable[i] = wakeup_config_.keyword_user_enables[i];
            ALOGD("%s: %d: confidence_level[%d] = %d KW_User_enable[%d] = %d",
                __func__, __LINE__,
                i, confidence_level[i], i, kw_user_enable[i]);
        }

        param->data = (void *)wakeup_payload_;
        param->size = wakeup_payload_size_;
    } else {
        struct detection_engine_config_stage1_pdk pdk_wakeup_config;
        memset(&pdk_wakeup_config, 0, sizeof(pdk_wakeup_config));

        conf_levels = (uint8_t *)sm_info_map_[s]->wakeup_config;
        pdk_wakeup_config.mode = sm_info_map_[s]->recognition_mode;
        pdk_wakeup_config.num_keywords = sm_info_map_[s]->wakeup_config_size;
        pdk_wakeup_config.model_id = sm_info_map_[s]->model_id;
        pdk_wakeup_config.custom_payload_size = 0;

        ALOGD("%s: %d: pdk mode : %u, num_keywords : %u, model_id : %u",
            __func__, __LINE__, pdk_wakeup_config.mode,
            pdk_wakeup_config.num_keywords, pdk_wakeup_config.model_id);

        for (int i = 0; i < pdk_wakeup_config.num_keywords; ++i) {
            pdk_wakeup_config.confidence_levels[i] =
                sm_info_map_[s]->state ? conf_levels[i] : 100;
            ALOGD("%s: %d: %dth keyword confidence level : %u",
                __func__, __LINE__, i, pdk_wakeup_config.confidence_levels[i]);
        }
        fixed_wakeup_payload_size =
            sizeof(struct detection_engine_config_stage1_pdk) -
            (MAX_KEYWORD_SUPPORTED * sizeof(uint32_t));
        wakeup_payload_size_ = fixed_wakeup_payload_size +
            (pdk_wakeup_config.num_keywords * sizeof(uint32_t));

        if (!wakeup_payload_)
            wakeup_payload_ = (uint8_t *)calloc(1, wakeup_payload_size_);
        else
            wakeup_payload_ = (uint8_t *)realloc(wakeup_payload_, wakeup_payload_size_);
        if (!wakeup_payload_) {
            ALOGE("%s: %d: payload malloc failed %s",
                __func__, __LINE__, strerror(errno));
            return -EINVAL;
        }
        ar_mem_cpy(wakeup_payload_, fixed_wakeup_payload_size,
                &pdk_wakeup_config, fixed_wakeup_payload_size);
        pdk_confidence_level = (uint32_t *)(wakeup_payload_ +
            fixed_wakeup_payload_size);

        for (int i = 0; i < pdk_wakeup_config.num_keywords; ++i) {
            pdk_confidence_level[i] = pdk_wakeup_config.confidence_levels[i];
        }
        param->data = (void *)wakeup_payload_;
        param->size = wakeup_payload_size_;
    }

    return 0;
}

int32_t SVAInterface::GetBufferingPayload(vui_intf_param_t *param) {
    void *s = nullptr;
    struct sound_model_info *info = nullptr;

    if (!param) {
        ALOGE("%s: %d: Invalid param", __func__, __LINE__);
        return -EINVAL;
    }

    memset(&buffering_config_, 0, sizeof(buffering_config_));
    if (!IS_MODULE_TYPE_PDK(module_type_)) {
        for (auto iter: sm_info_map_) {
            info = iter.second;
            if (info->state) {
                if (info->buf_config.hist_buffer_duration >
                    buffering_config_.hist_buffer_duration_msec)
                    buffering_config_.hist_buffer_duration_msec =
                        info->buf_config.hist_buffer_duration;
                if (info->buf_config.pre_roll_duration >
                    buffering_config_.pre_roll_duration_msec)
                    buffering_config_.pre_roll_duration_msec =
                        info->buf_config.pre_roll_duration;
            }
        }
        param->data = (void *)&buffering_config_.hist_buffer_duration_msec;
        param->size = sizeof(buffering_config_) - sizeof(uint32_t);
    } else {
        s = param->stream;
        if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
            buffering_config_.model_id = sm_info_map_[s]->model_id;
            buffering_config_.hist_buffer_duration_msec =
                sm_info_map_[s]->buf_config.hist_buffer_duration;
            buffering_config_.pre_roll_duration_msec =
                sm_info_map_[s]->buf_config.pre_roll_duration;
            param->data = (void *)&buffering_config_;
            param->size = sizeof(buffering_config_);
        } else {
            ALOGE("%s: %d: Stream not registered to interface", __func__, __LINE__);
            return -EINVAL;
        }
    }

    return 0;
}

int32_t SVAInterface::GetEngineResetPayload(vui_intf_param_t *param) {
    void *s = nullptr;

    if (!param) {
        ALOGE("%s: %d: Invalid param", __func__, __LINE__);
        return -EINVAL;
    }

    memset(&per_model_reset_config_, 0, sizeof(per_model_reset_config_));
    if (!IS_MODULE_TYPE_PDK(module_type_)) {
        param->data = nullptr;
        param->size = 0;
    } else {
        s = param->stream;
        if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
            per_model_reset_config_.model_id = sm_info_map_[s]->model_id;
            param->data = (void *)&per_model_reset_config_;
            param->size = sizeof(per_model_reset_config_);
        } else {
            ALOGE("%s: %d: Stream not registered to interface", __func__, __LINE__);
            return -EINVAL;
        }
    }
    return 0;
}

int32_t SVAInterface::SetModelState(void *s, bool state) {
    int32_t status = 0;
    uint8_t *conf_levels = nullptr;
    uint32_t num_conf_levels = 0;

    if (sm_info_map_.find(s) == sm_info_map_.end()) {
        ALOGD("%s: %d: Stream not registered", __func__, __LINE__);
        goto exit;
    }

    if (sm_info_map_[s]->state != state) {
        ALOGD("%s: %d: Update model state from %d to %d",
            __func__, __LINE__, sm_info_map_[s]->state, state);
        sm_info_map_[s]->state = state;

        conf_levels = (uint8_t *)sm_info_map_[s]->wakeup_config;
        num_conf_levels = sm_info_map_[s]->wakeup_config_size;
        if (sm_info_map_[s]->info) {
            // GMM usecase
            sm_info_map_[s]->info->UpdateConfLevelArray(
                conf_levels, num_conf_levels);
            status = UpdateMergeConfLevelsPayload(sm_info_map_[s]->info, state);
            if (status) {
                ALOGE("%s: %d: Update merge conf levels failed %d",
                    __func__, __LINE__, status);
                goto exit;
            }

            // update wakeup_config_
            if (sm_info_map_.size() == 1) {
                wakeup_config_.mode = sm_info_map_[s]->recognition_mode;
                wakeup_config_.custom_payload_size = 0;
                wakeup_config_.num_active_models = num_conf_levels;
                wakeup_config_.reserved = 0;
                for (int i = 0; i < wakeup_config_.num_active_models; i++) {
                    wakeup_config_.confidence_levels[i] = conf_levels[i];
                    wakeup_config_.keyword_user_enables[i] =
                        (wakeup_config_.confidence_levels[i] == 100) ? 0 : 1;
                    ALOGD("%s: %d: cf levels[%d] = %d", __func__, __LINE__, i,
                        wakeup_config_.confidence_levels[i]);
                }
            } else {
                wakeup_config_.mode |= sm_info_map_[s]->recognition_mode;
                wakeup_config_.custom_payload_size = 0;
                wakeup_config_.num_active_models =
                    sound_model_info_->GetConfLevelsSize();
                wakeup_config_.reserved = 0;
                for (int i = 0; i < wakeup_config_.num_active_models; i++) {
                    wakeup_config_.confidence_levels[i] =
                        sound_model_info_->GetConfLevels()[i];
                    wakeup_config_.keyword_user_enables[i] =
                        (wakeup_config_.confidence_levels[i] == 100) ? 0 : 1;
                    ALOGD("%s: %d: cf levels[%d] = %d", __func__, __LINE__, i,
                        wakeup_config_.confidence_levels[i]);
                }
            }
        }
        /*
         * for PDK case, wakeup config is dynamically updated
         * by stream in GetWakeUpPayload
         */
    } else {
        ALOGV("%s: %d: State no change, skip", __func__, __LINE__);
    }

exit:
    return status;
}

void SVAInterface::SetStreamAttributes(
    struct pal_stream_attributes *attr) {

    if (!attr) {
        ALOGE("%s: %d: Invalid stream attributes", __func__, __LINE__);
        return;
    }

    ar_mem_cpy(&str_attr_, sizeof(struct pal_stream_attributes),
        attr, sizeof(struct pal_stream_attributes));
}

SoundModelInfo* SVAInterface::GetSoundModelInfo(void *s) {
    if (!s) {
        return sound_model_info_;
    } else {
        if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
            return sm_info_map_[s]->info;
        } else {
            return nullptr;
        }
    }
}

void SVAInterface::SetModelId(void *s, uint32_t model_id) {
    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        sm_info_map_[s]->model_id = model_id;
    }
}

void SVAInterface::SetRecognitionMode(void *s, uint32_t mode) {
    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        sm_info_map_[s]->recognition_mode = mode;
    }
}

int32_t SVAInterface::RegisterModel(void *s,
    struct pal_st_sound_model *model,
    const std::vector<sound_model_data_t *> model_list) {

    int32_t status = 0;

    if (sm_info_map_.find(s) == sm_info_map_.end()) {
        sm_info_map_[s] = (struct sound_model_info *)calloc(1,
            sizeof(struct sound_model_info));
        if (!sm_info_map_[s]) {
            ALOGE("%s: %d: Failed to allocate memory for sm data",
                __func__, __LINE__);
            status = -ENOMEM;
            goto exit;
        }
    }

    if (module_type_ == ST_MODULE_TYPE_GMM) {
        sm_info_map_[s]->info = new SoundModelInfo();
        if (!sm_info_map_[s]->info) {
            ALOGE("%s: %d: Failed to allocate memory for SoundModelInfo",
                __func__, __LINE__);
            status = -ENOMEM;
            free(sm_info_map_[s]);
            goto exit;
        }
    }

    sm_info_map_[s]->model_list.clear();
    for (auto iter: model_list) {
        sm_info_map_[s]->model_list.push_back(iter);
    }
    sm_info_map_[s]->model = model;
    sm_info_map_[s]->type = model->type;

exit:
    return status;
}

void SVAInterface::DeregisterModel(void *s) {
    sound_model_data_t *sm_data = nullptr;

    auto iter = sm_info_map_.find(s);
    if (iter != sm_info_map_.end() && sm_info_map_[s]) {
        if (sm_info_map_[s]->wakeup_config)
            free(sm_info_map_[s]->wakeup_config);
        if (sm_info_map_[s]->info)
            delete(sm_info_map_[s]->info);
        sm_info_map_[s]->sec_threshold.clear();
        sm_info_map_[s]->sec_det_level.clear();

        for (int i = 0; i < sm_info_map_[s]->model_list.size(); i++) {
            sm_data = sm_info_map_[s]->model_list[i];
            if (sm_data) {
                if (sm_data->data)
                    free(sm_data->data);
                free(sm_data);
            }
        }
        sm_info_map_[s]->model_list.clear();
        free(sm_info_map_[s]);
        sm_info_map_.erase(iter);
    } else {
        ALOGD("%s: %d: Cannot deregister unregistered model",
            __func__, __LINE__);
    }

    if (origin_hist_buf_duration_.find(s) != origin_hist_buf_duration_.end()) {
        origin_hist_buf_duration_.erase(s);
    }
}

uint32_t SVAInterface::UsToBytes(uint64_t input_us) {
    uint32_t bytes = 0;

    bytes = str_attr_.in_media_config.sample_rate *
            str_attr_.in_media_config.bit_width *
            str_attr_.in_media_config.ch_info.channels * input_us /
            (BITS_PER_BYTE * US_PER_SEC);

    return bytes;
}

uint32_t SVAInterface::GetReadOffset(void *s) {
    return readOffsets_[s];
}

void SVAInterface::SetReadOffset(void *s, uint32_t offset) {
    readOffsets_[s] = offset;
}
