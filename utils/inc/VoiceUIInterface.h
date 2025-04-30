/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef VOICEUI_INTERFACE_H
#define VOICEUI_INTERFACE_H

#include <vector>
#include <map>
#include <errno.h>

#include "SoundTriggerUtils.h"

// common defs
typedef std::map<void *, struct sound_model_info *> sound_model_info_map_t;
typedef std::vector<std::pair<listen_model_indicator_enum, int32_t>> sec_stage_level_t;

struct buffer_config {
    uint32_t hist_buffer_duration;
    uint32_t pre_roll_duration;
};

struct keyword_index {
    uint32_t start_index;
    uint32_t end_index;
};

struct keyword_stats {
    uint64_t start_ts;
    uint64_t end_ts;
    uint64_t ftrt_duration;
};

/*
 * sound model config including sound model data and other configs
 * sound_model: sound model data passed from framework side
 * module_type: sound model type, can be updated during parsing
 * is_model_merge_enabled: indicate if sva model merge feature is enabled
 * supported_engine_count: number of engine instances supported
 * intf_plug_lib: lib name for interface plugin
 */
typedef struct sound_model_config {
    struct pal_st_sound_model *sound_model;
    st_module_type_t *module_type;
    bool is_model_merge_enabled;
    uint32_t supported_engine_count;
} sound_model_config_t;

// sound model data for each stage
typedef struct sound_model_data {
    listen_model_indicator_enum type;
    uint8_t *data;
    uint32_t size;
} sound_model_data_t;

// sound model list containing data for all stages
typedef struct sound_model_list {
    std::vector<sound_model_data_t *> sm_list;
} sound_model_list_t;

// sound model info for interface operation
struct sound_model_info {
    pal_st_sound_model_type_t type;
    uint32_t recognition_mode;
    uint32_t model_id;
    std::vector<sound_model_data_t *> model_list;
    struct pal_st_sound_model *model;
    struct pal_st_recognition_config *rec_config;
    void *wakeup_config;
    uint32_t wakeup_config_size;
    struct buffer_config buf_config;
    sec_stage_level_t sec_threshold;
    sec_stage_level_t sec_det_level;
    uint32_t det_result;
    SoundModelInfo *info;
    bool state;
};

/*
 * is_multi_model_supported: if interface support loading multi models
 * is_qc_wakeup_config: if interface is using qc wakeup config
 */
typedef struct vui_intf_property {
    bool is_multi_model_supported;
    bool is_qc_wakeup_config;
} vui_intf_property_t;

/*
 * General structure for interface param set/get operation
 * stream: stream pointer
 * data: pointer to data for set/get
 * size: size of data for set/get
 */
typedef struct vui_intf_param {
    void *stream;
    void *data;
    uint32_t size;
} vui_intf_param_t;

typedef enum {
    PARAM_FSTAGE_SOUND_MODEL_TYPE = 0,
    PARAM_FSTAGE_SOUND_MODEL_ID,
    PARAM_FSTAGE_SOUND_MODEL_STATE,
    PARAM_FSTAGE_SOUND_MODEL_ADD,
    PARAM_FSTAGE_SOUND_MODEL_DELETE,
    PARAM_FSTAGE_BUFFERING_CONFIG,
    PARAM_FSTAGE_DETECTION_UV_SCORE,
    PARAM_SSTAGE_KW_CONF_LEVEL,
    PARAM_SSTAGE_UV_CONF_LEVEL,
    PARAM_SSTAGE_KW_DET_LEVEL,
    PARAM_SSTAGE_UV_DET_LEVEL,
    PARAM_SOUND_MODEL_LIST,
    PARAM_RECOGNITION_MODE,
    PARAM_RECOGNITION_CONFIG,
    PARAM_DETECTION_RESULT,
    PARAM_DETECTION_EVENT,
    PARAM_DETECTION_STREAM,
    PARAM_KEYWORD_INDEX,
    PARAM_KEYWORD_STATS,
    PARAM_FTRT_DATA,
    PARAM_FTRT_DATA_SIZE,
    PARAM_LAB_READ_OFFSET,
    PARAM_STREAM_ATTRIBUTES,
    PARAM_KEYWORD_DURATION,
    PARAM_INTERFACE_PROPERTY,
    PARAM_SOUND_MODEL_LOAD,
    PARAM_SOUND_MODEL_UNLOAD,
    PARAM_WAKEUP_CONFIG,
    PARAM_CUSTOM_CONFIG,
    PARAM_BUFFERING_CONFIG,
    PARAM_ENGINE_RESET,
    // new custom param id can be added here
} intf_param_id_t;

typedef enum {
    PROCESS_LAB_DATA = 0,
    // new custom process id can be added here
} intf_process_id_t;

class VoiceUIInterface;

typedef struct vui_intf_t {
    std::shared_ptr<VoiceUIInterface> interface;
} vui_intf_t;

int32_t GetVUIInterface(struct vui_intf_t *intf, vui_intf_param_t *model);

// class defs
class VoiceUIInterface {
  public:
    virtual ~VoiceUIInterface() {}

    /*
     * @brief remove stream from interface
     * @caller StreamSoundTrigger
     *
     * @param[in]  stream  stream pointer
     */
    virtual void DetachStream(void *stream) = 0;

    /*
     * @brief set param to interface
     * @caller StreamSoundTrigger/SoundTriggerEngineGsl
     *
     * @param[in]  param_id  parameter id indicating which param is sent
     * @param[in]  param     payload containing param data/size and stream ptr
     *
     * @return 0 param set successfully
     * @return -EINVAL input data is invalid
     * @return -ENOMEM no memory available
     */
    virtual int32_t SetParameter(intf_param_id_t param_id, vui_intf_param_t *param) = 0;

    /*
     * @brief get information from interface
     * @caller StreamSoundTrigger/SoundTriggerEngineGsl
     *
     * @param[in]      param_id  parameter id indicating which param is being got
     * @param[in|out]  param     payload containing param data/size and stream ptr
     *
     * @return 0 get param successfully
     * @return -EINVAL input data is invalid
     * @return -ENOMEM no memory available
     */
    virtual int32_t GetParameter(intf_param_id_t param_id, vui_intf_param_t *param) = 0;

    /*
     * @brief process data in interface
     * @caller StreamSoundTrigger
     *
     * @param[in]      type          process type indicating how param is processed
     * @param[in|out]  in_out_param  input/out data before/after processing
     *
     * @return 0 param processed successfully
     * @return -EINVAL input data is invalid
     * @return -ENOMEM no memory available
     */
    virtual int32_t Process(intf_process_id_t type,
        vui_intf_param_t *in_out_param) = 0;

    /*
     * @brief register stream/model to interface
     *
     * @param[in]  s        stream pointer
     * @param[in]  model    cached sound model for stream s
     * @param[in]  sm_data  first stage model acquired from ParseSoundModel
     * @param[in]  sm_size  first stage model size
     *
     * @return  0        stream/model registered successfully
     * @return  -ENOMEM  no memory for allocation
     */
    virtual int32_t RegisterModel(void *s,
        struct pal_st_sound_model *model,
        const std::vector<sound_model_data_t *> model_list) = 0;

    /*
     * @brief deregister stream/model to interface
     *
     * @param[in]  s        stream pointer
     */
    virtual void DeregisterModel(void *s) = 0;
};

#endif

