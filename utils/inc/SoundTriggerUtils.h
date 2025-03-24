/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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
 *
 * Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
 *
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SOUND_TRIGGER_UTILS_H
#define SOUND_TRIGGER_UTILS_H

#include "PalDefs.h"
#ifdef __cplusplus
#include <memory>
#include <cstring>
#endif

#define MAX_KW_USERS_NAME_LEN (2 * MAX_STRING_LEN)
#define MAX_CONF_LEVEL_VALUE 100

enum {
    SML_PARSER_SUCCESS = 0,
    SML_PARSER_ERROR = 1,
    SML_PARSER_NOT_SUPPORTED_VERSION,
    SML_PARSER_ID_NOT_EXIST,
    SML_PARSER_NOT_ALIGNED_SIZE,
    SML_PARSER_SIZE_MISSMATCH,
    SML_PARSER_VALUE_MISSMATCH,
    SML_PARSER_REF_UNINIT_VALUE,
    SML_PARSER_OUT_OF_RANGE,
    SML_PARSER_WRONG_MODEL,
    SML_PARSER_WRONG_VALUE,
    SML_PARSER_DELETING_LAST_KEYWORD,
    SML_PARSER_KEYWORD_NOT_FOUND,
    SML_PARSER_USER_NOT_FOUND,
    SML_PARSER_USER_ALREADY_EXIST,
    SML_PARSER_KEYWORD_USER_PAIR_EXIST,
    SML_PARSER_KEYWORD_USER_PAIR_NOT_EXIST,
    SML_PARSER_ACTIVE_USER_REMOVE_UDK,
    SML_PARSER_KEYWORD_ALREADY_EXIST,
    SML_PARSER_NOTHING_TO_CHANGE,
};

enum {
    SML_COMPARATOR_ERROR = -1,
    SML_COMPARATOR_SAME = 0,
    SML_COMPARATOR_DIFF = 1,
};

enum {
    SML_V3_MAX_PAYLOAD = 10000000,
    SML_V3_MIN_KEYWORDS = 1,
    SML_V3_MAX_KEYWORDS = 1,
    SML_V3_MIN_USERS    = 0,
    SML_V3_MAX_USERS    = 1,
};

enum {
    SML_GLOBAL_HEADER_MAGIC_NUMBER = 0x00180cc8,    // SML03
    SML_MAX_MODEL_NUM = 3,
    SML_MAX_STRING_LEN = 200,
};

typedef enum {
    ST_SM_ID_SVA_NONE         = 0x0000,
    ST_SM_ID_SVA_F_STAGE_GMM  = 0x0001,
    ST_SM_ID_SVA_S_STAGE_PDK  = 0x0002,
    ST_SM_ID_SVA_S_STAGE_USER = 0x0004,
    ST_SM_ID_SVA_S_STAGE_RNN  = 0x0008,
    SML_ID_SVA_S_STAGE_UBM    = 0x0010,
    SML_ID_SVA_F_STAGE_INTERNAL = 0x0020,
    ST_SM_ID_SVA_S_STAGE_UDK  = 0x0040,
    ST_SM_ID_SVA_S_STAGE_KWD  = 0x004A, // S_STAGE_PDK | S_STAGE_RNN | S_STAGE_UDK
    ST_SM_ID_SVA_END          = 0x00F0,
    ST_SM_ID_CUSTOM_START     = 0x0100,
    ST_SM_ID_CUSTOM_END       = 0xF000,
} listen_model_indicator_enum;

typedef enum {
    ST_MODULE_TYPE_NONE     = 0,   // Internal Constant for initialization
    ST_MODULE_TYPE_GMM      = 3,
    ST_MODULE_TYPE_PDK5     = 5,
    ST_MODULE_TYPE_PDK6     = 6,
    ST_MODULE_TYPE_MFCN     = 8,
    ST_MODULE_TYPE_PDK      = 100, // Internal constant for local use indicating
                                   // both PDK5 and PDK6
    ST_MODULE_TYPE_HW       = 101, // Internal constant to identify hotword module
    ST_MODULE_TYPE_CUSTOM_1 = 102, // Reserved for Custom Engine 1
    ST_MODULE_TYPE_CUSTOM_2 = 103, // Reserved for Custom Engine 2
} st_module_type_t;

typedef struct _SML_GlobalHeaderType {
    uint32_t    magicNumber;                    // Magic number
    uint32_t    payloadBytes;                   // Payload bytes size
    uint32_t    modelVersion;                   // Model version
} SML_GlobalHeaderType;

typedef struct _SML_HeaderTypeV3 {
    uint32_t    numModels;                      // number of models
    uint32_t    keywordSpellLength;             // length of keyword spell ( include '\0' )
    uint32_t    userNameLength;                 // length of user name ( include '\0' )
    char    keywordSpell[SML_MAX_STRING_LEN];   // keyword spell
    char    userName[SML_MAX_STRING_LEN];       // user name
} SML_HeaderTypeV3;

typedef struct _SML_BigSoundModelTypeV3 {
    uint16_t versionMajor;
    uint16_t versionMinor;
    uint32_t offset;                            // start address of model data
    uint32_t size;                              // model size
    listen_model_indicator_enum type;           // type : Lower 1 byte : 1Stage KW model,
                                                //                       2Stage KW model,
                                                //                       2Stage User Model
                                                //        Upper 1 byte : 3rd Party - 1Stage KW model,
                                                //                       2Stage KW model,
                                                //                       2Stage User Model
}SML_BigSoundModelTypeV3;


typedef struct _SML_ModelTypeV3 {
    SML_HeaderTypeV3 header;                    // header for v3 model
    SML_BigSoundModelTypeV3 *modelInfo;         // model info, used for dynamic memory allocation.
    uint8_t* rawData;                           // actual model data
} SML_ModelTypeV3;

// all of model versions about SML
typedef enum _SML_ModelVersion {
    SML_MODEL_V2 = 0x0200,
    SML_MODEL_V3 = 0x0300,
} SML_ModelVersion;

// universial SML model structure
typedef struct _SML_ModelType {
    SML_GlobalHeaderType header;                // global header

    union _sml_model {
         SML_ModelTypeV3 SMLModelV3;             // sml3.0 -- kwihyuk
    } SMLModel;
} SML_ModelType;

#define ST_MAX_SOUND_MODELS 10
#define ST_MAX_KEYWORDS 10
#define ST_MAX_USERS 10

enum st_param_key {
    ST_PARAM_KEY_CONFIDENCE_LEVELS,
    ST_PARAM_KEY_HISTORY_BUFFER_CONFIG,
    ST_PARAM_KEY_KEYWORD_INDICES,
    ST_PARAM_KEY_TIMESTAMP,
    ST_PARAM_KEY_DETECTION_PERF_MODE,
    ST_PARAM_KEY_CONTEXT_RECOGNITION_INFO,
    ST_PARAM_KEY_CONTEXT_EVENT_INFO,
};

typedef enum st_param_key st_param_key_t;

struct __attribute__((__packed__)) st_param_header
{
    st_param_key_t key_id;
    uint32_t payload_size;
};

typedef enum {
    AUDIO_CONTEXT_EVENT_STOPPED,
    AUDIO_CONTEXT_EVENT_STARTED,
    AUDIO_CONTEXT_EVENT_DETECTED
} AUDIO_CONTEXT_EVENT_TYPE;

struct __attribute__((__packed__)) acd_per_context_cfg {
    uint32_t context_id;
    uint32_t threshold;
    uint32_t step_size;
};

struct __attribute__((__packed__)) acd_recognition_cfg {
     uint32_t version;
     uint32_t num_contexts;
     /* followed by array of acd_per_context_cfg[acd_per_context_cfg] */
};

struct __attribute__((__packed__)) acd_per_context_event_info {
     uint32_t context_id;
     uint32_t event_type;
     uint32_t confidence_score;
     uint64_t detection_ts;
};

struct __attribute__((__packed__)) acd_context_event {
     uint32_t version;
     uint64_t detection_ts;
     uint32_t num_contexts;
     /* followed by an array of acd_per_context_event_info[num_contexts] */
};

struct __attribute__((__packed__)) st_user_levels
{
    uint32_t user_id;
    uint8_t level;
};

struct __attribute__((__packed__)) st_keyword_levels
{
    uint8_t kw_level;
    uint32_t num_user_levels;
    struct st_user_levels user_levels[ST_MAX_USERS];
};

struct __attribute__((__packed__)) st_sound_model_conf_levels
{
    listen_model_indicator_enum sm_id;
    uint32_t num_kw_levels;
    struct st_keyword_levels kw_levels[ST_MAX_KEYWORDS];
};

struct __attribute__((__packed__)) st_confidence_levels_info
{
    uint32_t version; /* value: 0x1 */
    uint32_t num_sound_models;
    struct st_sound_model_conf_levels conf_levels[ST_MAX_SOUND_MODELS];
};

struct __attribute__((__packed__)) st_user_levels_v2
{
    uint32_t user_id;
    int32_t level;
};

struct __attribute__((__packed__)) st_keyword_levels_v2
{
    int32_t kw_level;
    uint32_t num_user_levels;
    struct st_user_levels_v2 user_levels[ST_MAX_USERS];
};

struct __attribute__((__packed__)) st_sound_model_conf_levels_v2
{
    listen_model_indicator_enum sm_id;
    uint32_t num_kw_levels;
    struct st_keyword_levels_v2 kw_levels[ST_MAX_KEYWORDS];
};
struct __attribute__((__packed__)) st_confidence_levels_info_v2
{
    uint32_t version; /* value: 0x02 */
    uint32_t num_sound_models;
    struct st_sound_model_conf_levels_v2 conf_levels[ST_MAX_SOUND_MODELS];
};

struct __attribute__((__packed__)) st_hist_buffer_info
{
    uint32_t version; /* value: 0x02 */
    uint32_t hist_buffer_duration_msec;
    uint32_t pre_roll_duration_msec;
};

struct __attribute__((__packed__)) st_keyword_indices_info
{
    uint32_t version; /* value: 0x01 */
    uint32_t start_index; /* in bytes */
    uint32_t end_index;   /* in bytes */
};

struct __attribute__((__packed__)) st_timestamp_info
{
    uint32_t version; /* value: 0x01 */
    uint64_t first_stage_det_event_time;  /* in nanoseconds */
    uint64_t second_stage_det_event_time; /* in nanoseconds */
};

struct __attribute__((__packed__)) st_det_perf_mode_info
{
    uint32_t version; /* value: 0x01 */
    uint32_t mode; /* 0 -Low Power, 1 -High performance */
};

typedef enum st_sound_model_type {
    ST_SM_TYPE_NONE,
    ST_SM_TYPE_KEYWORD_DETECTION,
    ST_SM_TYPE_USER_VERIFICATION,
    ST_SM_TYPE_CUSTOM_DETECTION,
    ST_SM_TYPE_MAX
}st_sound_model_type_t;

typedef enum st_param_id_type {
    LOAD_SOUND_MODEL = 0,
    UNLOAD_SOUND_MODEL,
    WAKEUP_CONFIG,
    BUFFERING_CONFIG,
    ENGINE_RESET,
    ENGINE_PER_MODEL_RESET,
    MODULE_VERSION,
    CUSTOM_CONFIG,
    MAX_PARAM_IDS
} st_param_id_type_t;

#define MAX_MODELS_SUPPORTED 8
#define BITS_PER_BYTE 8
#define US_PER_SEC 1000000
#define MS_PER_SEC 1000
#define MAX_STRING_LEN (100 * 2)

struct model_stats
{
    uint32_t detected_model_id;
    uint32_t detected_keyword_id;
    uint32_t best_channel_idx;
    uint32_t best_confidence_level;
    uint32_t kw_start_timestamp_lsw;
    uint32_t kw_start_timestamp_msw;
    uint32_t kw_end_timestamp_lsw;
    uint32_t kw_end_timestamp_msw;
    uint32_t detection_timestamp_lsw;
    uint32_t detection_timestamp_msw;
};

struct detection_event_info_pdk
{
    uint32_t num_detected_models;
    struct model_stats detected_model_stats[MAX_MODELS_SUPPORTED];
    uint32_t ftrt_data_length_in_us;
};

struct detection_event_info
{
    uint16_t status;
    uint16_t num_confidence_levels;
    uint8_t confidence_levels[20];
    uint32_t kw_start_timestamp_lsw;
    uint32_t kw_start_timestamp_msw;
    uint32_t kw_end_timestamp_lsw;
    uint32_t kw_end_timestamp_msw;
    uint32_t detection_timestamp_lsw;
    uint32_t detection_timestamp_msw;
    uint32_t ftrt_data_length_in_us;
};

#define IS_MODULE_TYPE_PDK(type) (type == ST_MODULE_TYPE_PDK5 || type == ST_MODULE_TYPE_PDK6 \
                                  || type == ST_MODULE_TYPE_MFCN)

#define ST_DEBUG_DUMP_LOCATION "/data/vendor/audio"
#define ST_DBG_DECLARE(args...) args

#define ST_DBG_FILE_OPEN_WR(fptr, fpath, fname, fextn, fcount) \
do {\
    char fptr_fn[100];\
\
    snprintf(fptr_fn, sizeof(fptr_fn), "%s/%s_%d.%s", fpath, fname, fcount, fextn);\
    fptr = fopen(fptr_fn, "wb");\
    if (!fptr) { \
        ALOGE("%s: File open failed %s: %s", \
              __func__, fptr_fn, strerror(errno)); \
    } \
} while (0)

#define ST_DBG_FILE_CLOSE(fptr) \
do {\
    if (fptr) { fclose (fptr); }\
} while (0)

#define ST_DBG_FILE_WRITE(fptr, buf, buf_size) \
do {\
    if (fptr) {\
        size_t ret_bytes = fwrite(buf, 1, buf_size, fptr);\
        if (ret_bytes != (size_t)buf_size) {\
            ALOGE("%s: fwrite %zu < %zu", __func__,\
                  ret_bytes, (size_t)buf_size);\
        }\
        fflush(fptr);\
    }\
} while (0)



#endif // SOUND_TRIGGER_UTILS_H
