/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <dlfcn.h>

#ifdef VUI_USE_SYSLOG
#include <algorithm>
#include <stdint.h>
#include <syslog.h>

#ifndef ALOGD
#define ALOGD(fmt, arg...) syslog (LOG_DEBUG, fmt, ##arg)
#endif

#ifndef ALOGI
#define ALOGI(fmt, arg...) syslog (LOG_INFO, fmt, ##arg)
#endif

#ifndef ALOGE
#define ALOGE(fmt, arg...) syslog (LOG_ERR, fmt, ##arg)
#endif

#ifndef ALOGV
#define ALOGV(fmt, arg...) syslog (LOG_DEBUG, fmt, ##arg)
#endif

#ifndef ALOGV_IF
#define ALOGV_IF(cond,fmt, arg...) cond ? syslog (LOG_DEBUG, fmt, ##arg) : (void)0
#endif


#else
#include <log/log.h>
#endif

#include <fstream>
#include <sstream>
#include "SVAExtension.h"

#define LOG_TAG "PAL: SVAExtension"
//#define LOG_NDEBUG 0

#define SM_FILE_PATH "/vendor/etc/models/vui/"
#define PAL_LIB_NAME "/vendor/lib64/libar-pal.so"

std::shared_ptr<SVAExtension> SVAExtension::ext_ = nullptr;

std::shared_ptr<SVAExtension> SVAExtension::GetInstance() {

    if (!ext_) {
        std::shared_ptr<SVAExtension> ext(new SVAExtension());
        if (ext->IsInitialized())
            ext_ = ext;
    }
    return ext_;
}

SVAExtension::SVAExtension() {

    is_initialized_ = false;
    pal_lib_handle_ = dlopen(PAL_LIB_NAME, RTLD_NOW);
    if (!pal_lib_handle_) {
        ALOGE("%s: %d: failed to open pal lib", __func__, __LINE__);
        return;
    }

    pal_stream_open_ =
        (pal_stream_open_t)dlsym(pal_lib_handle_, "pal_stream_open");
    pal_stream_close_ =
        (pal_stream_ops_t)dlsym(pal_lib_handle_, "pal_stream_close");
    pal_stream_start_ =
        (pal_stream_ops_t)dlsym(pal_lib_handle_, "pal_stream_start");
    pal_stream_stop_ =
        (pal_stream_ops_t)dlsym(pal_lib_handle_, "pal_stream_stop");
    pal_stream_set_param_ =
        (pal_stream_set_param_t)dlsym(pal_lib_handle_, "pal_stream_set_param");

    timer_thread_ = std::thread(TimerThread, std::ref(*this));
    exit_timer_thread_ = false;
    is_initialized_ = true;
}

SVAExtension::~SVAExtension() {

    if (pal_lib_handle_) {
        dlclose(pal_lib_handle_);
        pal_lib_handle_ = nullptr;
    }

    {
        std::unique_lock<std::mutex> lck(mutex_);
        exit_timer_thread_ = true;
        cv_.notify_one();
    }

    if (timer_thread_.joinable()) {
        ALOGD("%s: %d: Join timer thread", __func__, __LINE__);
        timer_thread_.join();
    }
}

int32_t SVAExtension::SetParameters(uint32_t param_id,
    void *param_payload, size_t payload_size) {

    int32_t status = 0, client_id = 0;
    char *save = nullptr;

    std::lock_guard<std::mutex> lck(mutex_);
    ALOGV("%s: %d: Enter, set param: %s",
        __func__, __LINE__, (char *)param_payload);

    switch (param_id) {
        case PAL_PARAM_ID_VUI_SET_META_DATA:
            client_id = ParseClientId(param_payload, &save);
            if (client_id && save) {
                status = HandleSVAOnOff(client_id, &save);
                if (client_config_map_.find(client_id) !=
                    client_config_map_.end()) {
                    client_config_map_[client_id]->last_op_ts =
                        std::chrono::steady_clock::now();
                    client_config_map_[client_id]->timeout_duration_ms =
                        TIMEOUT_MS;
                }
            }
            break;
        default:
            ALOGV("%s: %d: unknown param %d", __func__, __LINE__, param_id);
            break;
    }

    ALOGV("%s: %d: Exit, status = %d", __func__, __LINE__, status);
    return status;
}

int32_t SVAExtension::GetParameters(uint32_t param_id,
    void **param_payload, size_t *payload_size) {

    int32_t status = 0, client_id = 0;
    char *save = nullptr;

    if (!param_payload || !*param_payload || !payload_size) {
        return -EINVAL;
    }

    std::lock_guard<std::mutex> lck(mutex_);
    ALOGV("%s: %d: Enter, get param: %s",
        __func__, __LINE__, (char *)*param_payload);

    switch (param_id) {
        case PAL_PARAM_ID_VUI_GET_META_DATA:
            client_id = ParseClientId(*param_payload, &save);
            if (client_id && save) {
                if (!strncmp(save, "detection_event", strlen("detection_event"))) {
                    status = GetDetectionPayload(client_id, param_payload, payload_size);
                    if (client_config_map_.find(client_id) !=
                        client_config_map_.end()) {
                        client_config_map_[client_id]->last_op_ts =
                            std::chrono::steady_clock::now();
                    }
                } else if (!strncmp(save, "available_keywords", strlen("available_keywords"))) {
                    status = GetAvailableKeywords(client_id, param_payload, payload_size);
                }
            }
            break;
        case PAL_PARAM_ID_VUI_CAPTURE_META_DATA:
            client_id = ParseClientId(*param_payload, &save);
            if (client_id && save) {
                status = GetPalHandle(client_id,
                    &save, param_payload, payload_size);
                if (client_config_map_.find(client_id) !=
                    client_config_map_.end()) {
                    client_config_map_[client_id]->last_op_ts =
                        std::chrono::steady_clock::now();
                    client_config_map_[client_id]->timeout_duration_ms =
                        CAP_TIMEOUT_MS;
                }
            }
            break;
        default:
            ALOGV("%s: %d: unknown param %d", __func__, __LINE__, param_id);
            break;
    }

    ALOGV("%s: %d: Exit, status = %d", __func__, __LINE__, status);
    return status;
}

int32_t SVAExtension::ParseClientId(void *payload, char **save) {

    int32_t client_id = 0;
    char *delims = ",:";
    char *token = nullptr;

    if (!payload)
        return -EINVAL;

    token = strtok_r((char*)payload, delims, save);
    if (token && !strncmp(token, "client_id", strlen("client_id"))) {
        token = strtok_r(NULL, delims, save);
        client_id = token ? strtol(token, NULL, 0): 0;
    }
    if (client_id == 0) {
        //TODO: tricky one, we don't have client id to cache the error for it.
        ALOGE("%s: %d: invalid client id", __func__, __LINE__);
    }
    return client_id;
}

int32_t SVAExtension::HandleSVAOnOff(int32_t client_id, char **save) {

    int32_t status = 0;
    bool va_on = false;
    char *delims = ",:";
    char *token = nullptr;
    struct va_config va_cfg = {};

    token = strtok_r(NULL, delims, save);
    if (token && !strncmp(token, "va_on", strlen("va_on"))) {
        token = strtok_r(NULL, delims, save);
        if (token && !strncmp(token, "true", strlen("true"))) {
            va_cfg.va_on = true;
        } else if (token && !strncmp(token, "false", strlen("false"))) {
            va_cfg.va_on = false;
        } else {
            ALOGE("%s: %d: invalid va_on", __func__, __LINE__);
            status = -EINVAL;
            goto va_error;
        }
    } else {
        ALOGE("%s: %d: va_on not passed", __func__, __LINE__);
        status = -EINVAL;
        goto va_error;
    }

    // Parse the input config and enable sva
    va_cfg.client_id = client_id;
    token = strtok_r(NULL, delims, save);
    if (token && !strncmp(token, "va_kwd_info", strlen("va_kwd_info"))) {
        token = strtok_r(NULL, delims, save);
        if (token) {
            va_cfg.num_kwds = atoi(token);
            if (!va_cfg.num_kwds || va_cfg.num_kwds > MAX_SVA_KEYWORDS) {
                ALOGE("%s: %d: invalid num kwds", __func__, __LINE__);
                status = -EINVAL;
                goto va_error;
            }
            for (int i = 0; i < va_cfg.num_kwds; i++) {
                token = strtok_r(NULL, delims, save);
                if (token) {
                    va_cfg.kwd_info[i].kwd_name = token;
                } else {
                    ALOGE("%s: %d: kwd name not passed", __func__, __LINE__);
                    status = -EINVAL;
                    goto va_error;
                }
                if (va_cfg.va_on) {
                    token = strtok_r(NULL, delims, save);
                    if (token) {
                        va_cfg.kwd_info[i].kwd_level = atoi(token);
                        if (va_cfg.kwd_info[i].kwd_level < 0 ||
                            va_cfg.kwd_info[i].kwd_level > 100) {
                            ALOGE("%s: %d: invalid kwd level", __func__, __LINE__);
                            status = -EINVAL;
                            goto va_error;
                        }
                    } else {
                        ALOGE("%s: %d: invalid kwd level", __func__, __LINE__);
                        status = -EINVAL;
                        goto va_error;
                    }
                }
            }
        } else {
            ALOGE("%s: %d: va_kwd_info not passed", __func__, __LINE__);
            status = -EINVAL;
            goto va_error;
        }
    } else {
        ALOGE("%s: %d: va_kwd_info not passed", __func__, __LINE__);
        status = -EINVAL;
        goto va_error;
    }

    if (va_cfg.va_on) {
        token = strtok_r(NULL, delims, save);
        if (token && !strncmp(token, "history_buf_sz", strlen("history_buf_sz"))) {
            token = strtok_r(NULL, delims, save);
            if (token) {
                va_cfg.history_buf_sz = atoi(token);  // history buf size is optional
            }
            token = strtok_r(NULL, delims, save);
            if (token && !strncmp(token, "pre_roll_sz", strlen("pre_roll_sz"))) {
                token = strtok_r(NULL, delims, save);
                if (token) {
                    va_cfg.pre_roll_sz = atoi(token); // preroll is optional
                }
            }
        }
    }

    if (va_cfg.va_on) {
        status = EnableSVA(client_id, &va_cfg);
    } else {
        status = DisableSVA(client_id, &va_cfg);
    }

va_error:
    return status;
}

int32_t SVAExtension::EnableSVA(int32_t client_id, struct va_config *va_cfg) {

    int32_t status = 0;
    char *delims = ",:";
    char *token = nullptr;
    struct client_config *client_cfg = nullptr;

    ALOGI("%s: %d: enable client %d", __func__, __LINE__, client_id);
    if (client_config_map_.find(client_id) != client_config_map_.end()) {
        client_cfg = client_config_map_[client_id];
        client_status_map_[client_id] = 0;  // Clear any previous errors.
        ALOGI("%s: %d: restart client %d for next detection",
            __func__, __LINE__, client_id);
    } else {
        client_cfg = (struct client_config *)calloc(1,
            sizeof(struct client_config));
        if (!client_cfg) {
            status = -ENOMEM;
            ALOGE("%s: %d: client cfg allocation failed", __func__, __LINE__);
            goto va_error;
        }
    }

    status = StartSoundModel(va_cfg);
    if (status) {
        ALOGE("%s: %d: failed to start sound model, status = %d",
            __func__, __LINE__, status);
        goto va_error;
    }

    status = UpdateClientConfig(client_id, client_cfg, va_cfg);
    if (status) {
        ALOGE("%s: %d: failed to update client config, status = %d",
            __func__, __LINE__, status);
        goto va_error;
    }

    client_status_map_[client_id] = 0;
    client_config_map_[client_id] = client_cfg;
    return 0;

va_error:
    client_status_map_[client_id] = status;

    return status;
}

int32_t SVAExtension::DisableSVA(int32_t client_id, struct va_config *va_cfg) {

    int32_t status = 0;
    struct client_config *client_cfg = nullptr;

    if (client_config_map_.find(client_id) != client_config_map_.end()) {
        status = StopSoundModel(va_cfg);
        if (status) {
            ALOGE("%s: %d: failed to stop sound model, status = %d",
                __func__, __LINE__, status);
            client_status_map_[client_id] = status;
        }
        status = UpdateClientConfig(client_id,
            client_config_map_[client_id], va_cfg);
        if (status) {
            ALOGE("%s: %d: failed to update client config, status = %d",
                __func__, __LINE__, status);
            client_status_map_[client_id] = status;
        }
        if (client_config_map_[client_id]->kwd_info_list.size() == 0) {
            client_cfg = client_config_map_[client_id];
            client_config_map_.erase(client_id);
            if (client_cfg)
                free(client_cfg);
            client_status_map_.erase(client_id);
            if (client_config_map_.size() == 0)
                ext_ = nullptr;
        }
    } else {
        ALOGE("%s: %d: client %d not found", __func__, __LINE__, client_id);
        status = -EINVAL;
    }

    return status;
}

int32_t SVAExtension::GetAvailableKeywords(int32_t client_id,
    void **payload, size_t *payload_sz) {

    std::ostringstream ostr;

    ostr << "client_id," << client_id << ":";
    ostr << "status,success:available_keywords";

    for (auto iter: kwd_sm_map_) {
        ostr << "," << iter.first;
    }

    // We expect client passing the array as input payload hence copy into it,
    // instead of allocating here and deallocating in hal.
    *payload_sz = ostr.str().length() + 1;
    strlcpy(static_cast<char*>(*payload), ostr.str().c_str(), *payload_sz);

    ALOGV("%s: %d: GetAvailableKeywords returned payload %s",
            __func__, __LINE__, *payload);
    return 0;
}

int32_t SVAExtension::GetDetectionPayload(int32_t client_id,
    void **payload, size_t *payload_sz) {

    bool is_detected = false;
    struct client_config *client_cfg = nullptr;
    struct va_detection_info *det_info = nullptr;
    std::ostringstream ostr;

    ostr << "client_id," << client_id << ":";

    if (client_config_map_.find(client_id) != client_config_map_.end()) {
        client_cfg = client_config_map_[client_id];
        if (!client_cfg || client_status_map_[client_id] != 0) {
            ostr << "status,fail";
            goto done;
        }
        det_info = &client_cfg->det_info;
        if (det_info->is_detected)
            is_detected = true;
    } else {
        ostr << "status,fail";
        ALOGE("%s: %d: client %d not registered",
            __func__, __LINE__, client_id);
        goto done;
    }

    ostr << "status,success:";
    if (!is_detected) {
        ostr << "detection_event,false";
    } else {
        ostr << "detection_event,true:" << "va_kwd_info,";
        ostr << det_info->kwd_info.kwd_name
             << "," << det_info->kwd_info.kwd_level;
    }

done:
    // We expect client passing the array as input payload hence copy into it,
    // instead of allocating here and deallocating in hal.
    *payload_sz = ostr.str().length() + 1;
    strlcpy(static_cast<char*>(*payload), ostr.str().c_str(), *payload_sz);
    if (det_info)
        memset((void *)det_info, 0, sizeof(struct va_detection_info));

    ALOGV_IF(is_detected, "%s: %d: GetDetectionPayload returned payload %s",
            __func__, __LINE__, *payload);
    return 0;
}

int32_t SVAExtension::GetPalHandle(int32_t client_id __unused,
    char **save, void **payload, size_t *payload_sz) {

    char *delims = ",:";
    char *token = nullptr;
    struct va_config *va_cfg;
    std::string kwd_name;
    std::string sm_name;

    token = strtok_r(NULL, delims, save);
    if (token && !strncmp(token, "kwd_name", strlen("kwd_name"))) {
        token = strtok_r(NULL, delims, save);
        if (token) {
            kwd_name = token;
            if (kwd_sm_map_.find(kwd_name) != kwd_sm_map_.end()) {
                sm_name = kwd_sm_map_.at(kwd_name);
                if (sm_config_map_.find(sm_name) != sm_config_map_.end()) {
                    if (*payload_sz > sizeof(pal_stream_handle_t *)) {
                        *(uint64_t *)*payload =  (uint64_t)sm_config_map_[sm_name]->handle;
                        *payload_sz = sizeof(pal_stream_handle_t *);
                    } else {
                        *payload = nullptr;
                        *payload_sz = 0;
                    }
                }
            }
        }
    }

    return 0;
}

void* SVAExtension::GetSoundModel(int32_t client_id __unused,
    std::string sm_name) {

    FILE *fp = nullptr;
    size_t size = 0, payload_size = 0, ret_bytes = 0, kwd_count = 0;
    void *sm_data = nullptr;
    struct pal_st_phrase_sound_model *pal_phrase_sm = nullptr;
    pal_param_payload *param_payload = nullptr;

    if (sm_config_map_.find(sm_name) != sm_config_map_.end() &&
        sm_config_map_[sm_name] && sm_config_map_[sm_name]->sm_data) {
        return sm_config_map_[sm_name]->sm_data;
    }

    for (auto iter: kwd_sm_map_) {
        if (iter.second == sm_name)
            kwd_count++;
    }
    ALOGI("%s: %d: kwd num %d", __func__, __LINE__, kwd_count);

    std::string filename = SM_FILE_PATH + sm_name;
    fp = fopen(filename.c_str(), "rb");
    if (!fp) {
        ALOGE("%s: %d: file %s open failed",
            __func__, __LINE__, filename.c_str());
        return nullptr;
    }
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    payload_size = size + sizeof(pal_param_payload) +
        sizeof(struct pal_st_phrase_sound_model);
    param_payload = (pal_param_payload *)calloc(1, payload_size);
    if (!param_payload) {
        ALOGE("%s: %d: sm payload allocation failed, size %d",
            __func__, __LINE__, payload_size);
        goto va_error;
    }

    param_payload->payload_size =
        size + sizeof(struct pal_st_phrase_sound_model);
    pal_phrase_sm = (struct pal_st_phrase_sound_model *)param_payload->payload;

    pal_phrase_sm->common.type = PAL_SOUND_MODEL_TYPE_KEYPHRASE;
    memcpy(&pal_phrase_sm->common.vendor_uuid, &qcva_uuid, sizeof(struct st_uuid));
    pal_phrase_sm->common.data_size = size;
    pal_phrase_sm->common.data_offset =
        sizeof(struct pal_st_phrase_sound_model);

    pal_phrase_sm->num_phrases = kwd_count;
    for (int i = 0; i < pal_phrase_sm->num_phrases; i++) {
        pal_phrase_sm->phrases[i].id = i;
        pal_phrase_sm->phrases[i].recognition_mode =
            PAL_RECOGNITION_MODE_VOICE_TRIGGER;
        pal_phrase_sm->phrases[i].num_users = 0;
        for (int j = 0; j < pal_phrase_sm->phrases[i].num_users; j++)
            pal_phrase_sm->phrases[i].users[j] = j;
    }

    sm_data = (uint8_t *)pal_phrase_sm + pal_phrase_sm->common.data_offset;
    ret_bytes = fread(sm_data, 1, size, fp);
    if (ret_bytes != size) {
        ALOGE("%s: %d: data read failed", __func__, __LINE__);
        goto va_error;
    }
    sm_config_map_[sm_name]->sm_data = (void *)param_payload;
    return (void *)param_payload;

va_error:
    if (param_payload) {
        free(param_payload);
    }
    fclose(fp);
    return nullptr;
}

void* SVAExtension::GetRecognitionConfig(struct sm_config *config) {

    pal_param_payload *param_payload = nullptr;
    struct pal_st_recognition_config *rc_config = nullptr;
    uint8_t *opaque_ptr = nullptr;
    struct st_param_header *param_hdr = nullptr;
    uint32_t payload_size = 0, rc_config_data_sz = 0;
    struct st_confidence_levels_info_v2 *conf_levels_v2 = nullptr;
    struct st_hist_buffer_info *hist_buf_info = nullptr;
    int i = 0, j = 0;

    if (config->history_buf_sz || config->pre_roll_sz) {
        rc_config_data_sz = sizeof(struct st_confidence_levels_info_v2) +
            sizeof(struct st_hist_buffer_info) +
            2 * sizeof(struct st_param_header);
    } else {
        rc_config_data_sz = sizeof(struct st_confidence_levels_info_v2) +
            sizeof(struct st_param_header);
    }
    payload_size = sizeof(pal_param_payload) +
        sizeof(struct pal_st_recognition_config) +
        rc_config_data_sz;

    param_payload = (pal_param_payload *)calloc(1, payload_size);
    if (!param_payload) {
        ALOGE("%s: %d: recognition config payload allocation failed, size %d",
            __func__, __LINE__, payload_size);
        return nullptr;
    }

    param_payload->payload_size = rc_config_data_sz +
        sizeof(struct pal_st_recognition_config);

    rc_config = (struct pal_st_recognition_config *)param_payload->payload;
    rc_config->cookie = (uint8_t *)(ext_.get());
    rc_config->data_size = rc_config_data_sz;
    rc_config->data_offset = sizeof(struct pal_st_recognition_config);
    rc_config->capture_requested = true;
    rc_config->num_phrases = config->num_kwds;
    for (i = 0; i < rc_config->num_phrases; i++) {
        rc_config->phrases[i].id = i;
        rc_config->phrases[i].recognition_modes =
            PAL_RECOGNITION_MODE_VOICE_TRIGGER;
        rc_config->phrases[i].confidence_level =
            config->kwd_info[i].kwd_level;
    }

    opaque_ptr = (uint8_t *)rc_config + rc_config->data_offset;
    param_hdr = (struct st_param_header *)opaque_ptr;
    param_hdr->key_id = ST_PARAM_KEY_CONFIDENCE_LEVELS;
    param_hdr->payload_size = sizeof(struct st_confidence_levels_info_v2);
    opaque_ptr += sizeof(struct st_param_header);
    conf_levels_v2 = (struct st_confidence_levels_info_v2 *)opaque_ptr;
    conf_levels_v2->version = 0x2;
    conf_levels_v2->num_sound_models = 1;
    for (i = 0; i < conf_levels_v2->num_sound_models; i++) {
        conf_levels_v2->conf_levels[i].sm_id = ST_SM_ID_SVA_F_STAGE_GMM;
        conf_levels_v2->conf_levels[i].num_kw_levels = config->num_kwds;
        for (j = 0; j < conf_levels_v2->conf_levels[i].num_kw_levels; j++) {
            conf_levels_v2->conf_levels[i].kw_levels[j].kw_level =
                config->kwd_info[j].kwd_level;
        }
    }
    opaque_ptr += sizeof(struct st_confidence_levels_info_v2);

    if (config->history_buf_sz || config->pre_roll_sz) {
        param_hdr = (struct st_param_header *)opaque_ptr;
        param_hdr->key_id = ST_PARAM_KEY_HISTORY_BUFFER_CONFIG;
        param_hdr->payload_size = sizeof(struct st_hist_buffer_info);
        opaque_ptr += sizeof(struct st_param_header);
        hist_buf_info = (struct st_hist_buffer_info *)opaque_ptr;
        hist_buf_info->version = 0x2;
        hist_buf_info->hist_buffer_duration_msec = config->history_buf_sz;
        hist_buf_info->pre_roll_duration_msec = config->pre_roll_sz;
    }

    return (void *)param_payload;
}

int32_t SVAExtension::UpdateClientConfig(int32_t client_id,
    struct client_config *client_cfg, struct va_config *va_cfg) {

    int32_t status = 0;
    int i = 0, j = 0;
    std::string kwd_name;
    bool kwd_found = false;
    struct va_kwd_info *kwd_info = nullptr;

    if (!client_cfg || !va_cfg) {
        return -EINVAL;
    }

    if (va_cfg->va_on) {
        for (i = 0; i < va_cfg->num_kwds; i++) {
            kwd_name = va_cfg->kwd_info[i].kwd_name;
            if (kwd_client_map_.find(kwd_name) == kwd_client_map_.end() ||
                kwd_client_map_[kwd_name] != client_id) {
                kwd_client_map_[kwd_name] = client_id;
            }
            kwd_found = false;
            for (j = 0; j < client_cfg->kwd_info_list.size(); j++) {
                if (client_cfg->kwd_info_list[j]->kwd_name == kwd_name) {
                    client_cfg->kwd_info_list[j]->kwd_level =
                        va_cfg->kwd_info[i].kwd_level;
                    kwd_found = true;
                    break;
                }
            }
            if (!kwd_found) {
                kwd_info = (struct va_kwd_info *)calloc(1,
                    sizeof(struct va_kwd_info));
                if (!kwd_info) {
                    status = -ENOMEM;
                    goto error;
                }
                kwd_info->kwd_name = kwd_name;
                kwd_info->kwd_level = va_cfg->kwd_info[i].kwd_level;
                client_cfg->kwd_info_list.push_back(kwd_info);
            }
        }
    } else {
        for (i = 0; i < va_cfg->num_kwds; i++) {
            kwd_name = va_cfg->kwd_info[i].kwd_name;
            if (kwd_client_map_.find(kwd_name) == kwd_client_map_.end() ||
                kwd_client_map_[kwd_name] != client_id) {
                continue;
            }
            kwd_found = false;
            for (j = 0; j < client_cfg->kwd_info_list.size(); j++) {
                kwd_info = client_cfg->kwd_info_list[j];
                if (kwd_info->kwd_name == kwd_name) {
                    kwd_found = true;
                    break;
                }
            }
            if (kwd_found) {
                client_cfg->kwd_info_list.erase(
                    client_cfg->kwd_info_list.begin() + j);
                if (kwd_info)
                    free(kwd_info);
                kwd_client_map_.erase(kwd_name);
            }
        }
    }

error:

    return status;
}

std::vector<struct sm_config *> SVAExtension::UpdateSoundModelConfig(
    struct va_config *va_cfg, bool is_enable) {

    int32_t status = 0;
    std::string kwd_name;
    std::vector<struct sm_config *> sm_config_list;
    std::vector<struct sm_config *>::iterator it;
    struct sm_config *sm_cfg = nullptr;
    std::string sm_name;
    uint32_t num_kwds = 0, num_kwds_enabled = 0;
    int i = 0, j = 0;

    for (int i = 0; i < va_cfg->num_kwds; i++) {
        kwd_name = va_cfg->kwd_info[i].kwd_name;
        if (kwd_sm_map_.find(kwd_name) != kwd_sm_map_.end()) {
            sm_name = kwd_sm_map_.at(kwd_name);
            if (sm_config_map_.find(sm_name) != sm_config_map_.end()) {
                sm_cfg = sm_config_map_[sm_name];
            } else {
                ALOGV("%s: %d: create new sound model config",
                    __func__, __LINE__);
                sm_cfg = (struct sm_config *)calloc(1, sizeof(struct sm_config));
                if (!sm_cfg) {
                    ALOGE("%s: %d: failed to allocate for sound model config",
                        __func__, __LINE__);
                    goto error;
                }
                num_kwds = 0;
                for (auto iter = kwd_list_.begin(); iter != kwd_list_.end(); iter++) {
                    if (kwd_sm_map_.find(*iter) != kwd_sm_map_.end()) {
                        if (kwd_sm_map_.at(*iter) == sm_name) {
                            sm_cfg->kwd_info[num_kwds].kwd_name = *iter;
                            sm_cfg->kwd_info[num_kwds].kwd_level = 100;
                            num_kwds++;
                        }
                    }
                }
                sm_cfg->num_kwds = num_kwds;
                sm_cfg->history_buf_sz =
                    (sm_cfg->history_buf_sz > va_cfg->history_buf_sz) ?
                        sm_cfg->history_buf_sz : va_cfg->history_buf_sz;
                sm_cfg->pre_roll_sz =
                    (sm_cfg->pre_roll_sz > va_cfg->pre_roll_sz) ?
                        sm_cfg->pre_roll_sz : va_cfg->pre_roll_sz;
                sm_cfg->sm_name = sm_name;

                sm_config_map_[sm_name] = sm_cfg;
            }
        } else {
            ALOGE("%s: %d: Invalid keyword provided", __func__, __LINE__);
            goto error;
        }

        for (int j = 0; j < sm_cfg->num_kwds; j++) {
            if (sm_cfg->kwd_info[j].kwd_name == kwd_name) {
                sm_cfg->kwd_info[j].kwd_level =
                    is_enable ? va_cfg->kwd_info[i].kwd_level : 100;
            }
        }

        it = std::find(sm_config_list.begin(), sm_config_list.end(), sm_cfg);
        if (it == sm_config_list.end()) {
            sm_config_list.push_back(sm_cfg);
        }
    }

    for (i = 0; i < sm_config_list.size(); i++) {
        sm_cfg = sm_config_list[i];
        num_kwds_enabled = 0;
        for (j = 0; j < sm_cfg->num_kwds; j++) {
            if (sm_cfg->kwd_info[j].kwd_level != 100)
                num_kwds_enabled++;
        }
        if (is_enable) {
            if (sm_cfg->num_active_kwds) {
                sm_cfg->is_restart_needed = true;
            }
        } else {
            if (num_kwds_enabled)
                sm_cfg->is_restart_needed = true;
        }
        sm_cfg->num_active_kwds = num_kwds_enabled;
    }

    return sm_config_list;

error:
    for (auto iter: sm_config_list) {
        if (iter)
            free(iter);
    }
    sm_config_list.clear();
    return sm_config_list;
}

int32_t SVAExtension::StartSoundModel(struct va_config *va_cfg) {

    int32_t status = 0;
    pal_stream_handle_t *handle = nullptr;
    void *rc_config = nullptr;
    void *sm_data = nullptr;
    std::vector<struct sm_config *> sm_config_list;
    struct sm_config *sm_cfg = nullptr;

    sm_config_list = UpdateSoundModelConfig(va_cfg, true);
    if (sm_config_list.size() == 0) {
        ALOGE("%s: %d: Failed to update sound model config",
            __func__, __LINE__);
        return -EINVAL;
    }

    for (int i = 0; i < sm_config_list.size(); i++) {
        sm_cfg = sm_config_list[i];
        if (!sm_cfg->is_restart_needed) {
            sm_data = GetSoundModel(va_cfg->client_id, sm_cfg->sm_name);
            if (!sm_data) {
                status = -ENOMEM;
                goto exit;
            }

            status = LoadSoundModel(sm_data, &handle);
            if (status || !handle) {
                ALOGE("%s: %d: failed to open pal stream, status = %d",
                    __func__, __LINE__, status);
                goto exit;
            }
            sm_cfg->handle = handle;
        } else {
            sm_cfg->is_restart_needed = false;
        }

        if (sm_cfg->handle) {
            rc_config = GetRecognitionConfig(sm_cfg);
            if (!rc_config) {
                ALOGE("%s: %d: failed to generate recognition config",
                    __func__, __LINE__);
                status = -ENOMEM;
                goto exit;
            }

            status = pal_stream_set_param_(
                sm_cfg->handle, PAL_PARAM_ID_RECOGNITION_CONFIG,
                (pal_param_payload *)rc_config);
            if (rc_config) {
                free(rc_config);
            }
            if (status) {
                ALOGE("%s: %d: failed to set recognition config, status = %d",
                    __func__, __LINE__, status);
                goto exit;
            }

            status = StartRecognition(sm_cfg->handle);
            if (status) {
                ALOGE("%s: %d: failed to start recognition, status = %d",
                    __func__, __LINE__, status);
                goto exit;
            }

            if (handle) {
                pal_hdl_sm_config_map_[handle] = sm_cfg;
            }
        }
    }

exit:
    return status;
}

int32_t SVAExtension::StopSoundModel(struct va_config *va_cfg) {

    int32_t status = 0;
    pal_stream_handle_t *handle = nullptr;
    std::vector<struct sm_config *> sm_config_list;
    void *rc_config = nullptr;
    struct sm_config *sm_cfg = nullptr;

    if (!va_cfg) {
        ALOGE("%s: %d: invalid va config", __func__, __LINE__);
        return -EINVAL;
    }

    sm_config_list = UpdateSoundModelConfig(va_cfg, false);

    for (int i = 0; i < sm_config_list.size(); i++) {
        handle = sm_config_list[i]->handle;
        status = StopRecognition(handle);
        if (status) {
            ALOGE("%s: %d: failed to stop recognition, status = %d",
                __func__, __LINE__, status);
            goto exit;
        }

        if (sm_config_list[i]->is_restart_needed) {
            sm_config_list[i]->is_restart_needed = false;
            rc_config = GetRecognitionConfig(sm_config_list[i]);
            if (!rc_config) {
                ALOGE("%s: %d: failed to generate recognition config",
                    __func__, __LINE__);
                status = -ENOMEM;
                goto exit;
            }

            status = pal_stream_set_param_(
                handle, PAL_PARAM_ID_RECOGNITION_CONFIG,
                (pal_param_payload *)rc_config);
            if (rc_config) {
                free(rc_config);
                rc_config = nullptr;
            }
            if (status) {
                ALOGE("%s: %d: failed to set recognition config, status = %d",
                    __func__, __LINE__, status);
                goto exit;
            }

            status = StartRecognition(handle);
            if (status) {
                ALOGE("%s: %d: failed to start recognition, status = %d",
                    __func__, __LINE__, status);
                goto exit;
            }
        } else {
            status = UnloadSoundModel(handle);
            if (status) {
                ALOGE("%s: %d: failed to unload sound model, status = %d",
                    __func__, __LINE__, status);
                goto exit;
            }
            sm_cfg = pal_hdl_sm_config_map_[handle];
            for (auto iter = sm_config_map_.begin();
                 iter != sm_config_map_.end(); ) {
                if (iter->second == sm_cfg) {
                    sm_config_map_.erase(iter++);
                } else {
                    iter++;
                }
            }
            if (sm_cfg->sm_data)
                free(sm_cfg->sm_data);
            pal_hdl_sm_config_map_.erase(handle);
        }
    }

exit:
    return status;
}

int32_t SVAExtension::LoadSoundModel(void *sm_data,
    pal_stream_handle_t **handle) {

    int32_t status = 0;
    pal_param_payload *param_payload = nullptr;

    ALOGV("%s: %d: Enter", __func__, __LINE__);
    status = OpenPALStream(handle);
    if (status) {
        ALOGE("%s: %d: failed to open pal stream, status = %d",
            __func__, __LINE__, status);
        return status;
    }

    status = pal_stream_set_param_(*handle,
        PAL_PARAM_ID_LOAD_SOUND_MODEL, (pal_param_payload *)sm_data);
    if (status) {
        ALOGE("%s: %d: failed to set param to pal stream, status = %d",
            __func__, __LINE__, status);
    }

    ALOGV("%s: %d: Exit, status = %d", __func__, __LINE__, status);
    return status;
}

int32_t SVAExtension::UnloadSoundModel(pal_stream_handle_t *handle) {

    int32_t status = 0;

    ALOGV("%s: %d: Enter", __func__, __LINE__);

    status = pal_stream_close_(handle);
    if (status) {
        ALOGE("%s: %d: failed to close pal stream, status = %d",
            __func__, __LINE__, status);
    }

    ALOGV("%s: %d: Exit, status = %d", __func__, __LINE__, status);
    return status;
}

int32_t SVAExtension::StartRecognition(pal_stream_handle_t *handle) {

    int32_t status = 0;

    ALOGV("%s: %d: Enter", __func__, __LINE__);

    status = pal_stream_start_(handle);
    if (status) {
        ALOGE("%s: %d: failed to start recognition, status = %d",
            __func__, __LINE__, status);
    }

    ALOGV("%s: %d: Exit, status = %d", __func__, __LINE__, status);
    return status;
}

int32_t SVAExtension::StopRecognition(pal_stream_handle_t *handle) {

    int32_t status = 0;

    ALOGV("%s: %d: Enter", __func__, __LINE__);

    status = pal_stream_stop_(handle);
    if (status) {
        ALOGE("%s: %d: failed to stop recognition, status = %d",
            __func__, __LINE__, status);
    }

    ALOGV("%s: %d: Exit, status = %d", __func__, __LINE__, status);
    return status;
}

int32_t SVAExtension::OpenPALStream(pal_stream_handle_t **handle) {

    int status = 0;
    struct pal_stream_attributes stream_attributes = {};
    struct pal_device device = {};

    ALOGV("%s: %d: Enter", __func__, __LINE__);

    device.id = PAL_DEVICE_IN_HANDSET_VA_MIC;
    device.config.sample_rate = 48000;
    device.config.bit_width = 16;
    device.config.ch_info.channels = 2;
    device.config.ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
    device.config.ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;
    device.config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    stream_attributes.type = PAL_STREAM_VOICE_UI;
    stream_attributes.flags = (pal_stream_flags_t)0;
    stream_attributes.direction = PAL_AUDIO_INPUT;
    stream_attributes.in_media_config.sample_rate = 16000;
    stream_attributes.in_media_config.bit_width = 16;
    stream_attributes.in_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;
    stream_attributes.in_media_config.ch_info.channels = 1;
    stream_attributes.in_media_config.ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;

    status = pal_stream_open_(&stream_attributes,
                              1,
                              &device,
                              0,
                              nullptr,
                              &pal_callback,
                              (uint64_t)this,
                              handle);

    ALOGD("%s: %d: (%d:status)", __func__, __LINE__, status);

    if (status) {
        ALOGE("%s: %d: Pal Stream Open Error (%d)", __func__, __LINE__, status);
        status = -EINVAL;
        goto exit;
    }

exit:
    ALOGV("%s: %d: Exit, status = %d", __func__, __LINE__, status);

    return status;
}

int32_t SVAExtension::pal_callback(pal_stream_handle_t *stream_handle,
    uint32_t event_id, uint32_t *event_data,
    uint32_t event_size, uint64_t cookie) {

    int32_t status = 0;
    struct pal_st_phrase_recognition_event *phrase_event = nullptr;
    uint8_t *opaque_data = nullptr;
    struct st_param_header *param_hdr = nullptr;
    struct st_confidence_levels_info_v2 *conf_levels = nullptr;
    struct st_sound_model_conf_levels_v2 *sm_conf_levels = nullptr;
    struct sm_config *sm_cfg = nullptr;
    int32_t client_id = 0;
    struct client_config *client_cfg = nullptr;
    SVAExtension *ext = (SVAExtension *)cookie;
    bool is_kwd_found = false;

    std::lock_guard<std::mutex> lck(ext->mutex_);
    if (!stream_handle ||
        (pal_hdl_sm_config_map_.find(stream_handle) ==
         pal_hdl_sm_config_map_.end())) {
        ALOGE("%s: %d: invalid stream handle", __func__, __LINE__);
        return -EINVAL;
    }

    if (!event_data) {
        ALOGE("%s: %d: invalid detection event", __func__, __LINE__);
        status = -EINVAL;
        goto error;
    }

    phrase_event = (struct pal_st_phrase_recognition_event *)event_data;
    opaque_data = (uint8_t *)phrase_event + phrase_event->common.data_offset;

    param_hdr = (struct st_param_header *)opaque_data;
    if (param_hdr->key_id != ST_PARAM_KEY_CONFIDENCE_LEVELS) {
        ALOGE("%s: %d: invalid opaque data", __func__, __LINE__);
        status = -EINVAL;
        goto error;
    }

    // Assume conf levels always come first in opaque data
    opaque_data += sizeof(struct st_param_header);
    conf_levels = (struct st_confidence_levels_info_v2 *)opaque_data;
    sm_conf_levels = &conf_levels->conf_levels[0];

    sm_cfg = pal_hdl_sm_config_map_[stream_handle];
    for (int i = 0; i < sm_cfg->num_kwds; i++) {
        if (sm_conf_levels->kw_levels[i].kw_level > 0) {
            is_kwd_found = true;
            ALOGD("%s: %d: keyword %s is detected",
                __func__, __LINE__, sm_cfg->kwd_info[i].kwd_name.c_str());
            if (kwd_client_map_.find(sm_cfg->kwd_info[i].kwd_name) !=
                kwd_client_map_.end()) {
                client_id = kwd_client_map_[sm_cfg->kwd_info[i].kwd_name];
                client_cfg = client_config_map_[client_id];
                client_status_map_[client_id] = 0;

                if (client_cfg->det_info.is_detected &&
                    client_cfg->det_info.handle &&
                    client_cfg->det_info.handle != stream_handle) {
                    ext->StartRecognition(client_cfg->det_info.handle);
                }
                client_cfg->det_info.is_detected = true;
                client_cfg->det_info.handle = stream_handle;
                client_cfg->det_info.kwd_info.kwd_name =
                    sm_cfg->kwd_info[i].kwd_name;
                client_cfg->det_info.kwd_info.kwd_level =
                    sm_conf_levels->kw_levels[i].kw_level;
            }
        }
    }
    if (!is_kwd_found)
        ext->StartRecognition(stream_handle);

    return 0;

error:
    // mark status to -EINVAL for all related clients
    if (pal_hdl_sm_config_map_.find(stream_handle) !=
        pal_hdl_sm_config_map_.end()) {
        sm_cfg = pal_hdl_sm_config_map_[stream_handle];
        for (int i = 0; i < sm_cfg->num_kwds; i++) {
            if (kwd_client_map_.find(sm_cfg->kwd_info[i].kwd_name) !=
                kwd_client_map_.end()) {
                client_id = kwd_client_map_[sm_cfg->kwd_info[i].kwd_name];
                client_status_map_[client_id] = -EINVAL;
            }
        }
    }
    return status;
}

void SVAExtension::TimerThread(SVAExtension& ext) {
    ChronoSteadyClock_t current_ts;
    uint64_t duration = 0;
    int32_t client_id = 0;
    struct client_config *client_cfg = nullptr;
    struct va_config va_cfg = {};

    ALOGD("%s: %d: Enter", __func__, __LINE__);

    std::unique_lock<std::mutex> lck(ext.mutex_);
    while (!ext.exit_timer_thread_) {
        ext.cv_.wait_for(lck, std::chrono::milliseconds(TIMEOUT_MS));

        if (ext.exit_timer_thread_)
            break;

        for (auto iter: client_config_map_) {
            client_id = iter.first;
            client_cfg = iter.second;
            current_ts = std::chrono::steady_clock::now();
            duration = std::chrono::duration_cast<std::chrono::milliseconds>
                (current_ts - client_cfg->last_op_ts).count();

            if (duration > client_cfg->timeout_duration_ms) {
                va_cfg.client_id = client_id;
                va_cfg.va_on = false;
                va_cfg.num_kwds = client_cfg->kwd_info_list.size();
                for (int i = 0; i < va_cfg.num_kwds; i++) {
                    va_cfg.kwd_info[i].kwd_name = client_cfg->kwd_info_list[i]->kwd_name;
                    va_cfg.kwd_info[i].kwd_level = client_cfg->kwd_info_list[i]->kwd_level;
                }
                ext.DisableSVA(client_id, &va_cfg);
            }
        }
    }

    ALOGD("%s: %d: Exit", __func__, __LINE__);
    return;
}
