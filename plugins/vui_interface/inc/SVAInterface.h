/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SVA_INTERFACE_H
#define SVA_INTERFACE_H

#include <mutex>

#include "VoiceUIInterface.h"
#include "detection_cmn_api.h"
#include "ar_osal_mem_op.h"

class SVAInterface: public VoiceUIInterface {
  public:
    SVAInterface(st_module_type_t module_type);
    ~SVAInterface();

    static std::shared_ptr<VoiceUIInterface> GetInstance(vui_intf_param_t *model);
    void DetachStream(void *stream) override;

    int32_t SetParameter(intf_param_id_t param_id,
        vui_intf_param_t *param) override;
    int32_t GetParameter(intf_param_id_t param_id,
        vui_intf_param_t *param) override;

    int32_t Process(intf_process_id_t type,
        vui_intf_param_t *in_out_param) { return 0; }

    int32_t RegisterModel(void *s,
        struct pal_st_sound_model *model,
        const std::vector<sound_model_data_t *> model_list) override;
    void DeregisterModel(void *s) override;

  private:
    static int32_t ParseSoundModel(struct pal_st_sound_model *sound_model,
                                   st_module_type_t *first_stage_type,
                                   std::vector<sound_model_data_t *> &model_list);

    int32_t ParseRecognitionConfig(void *s,
                                   struct pal_st_recognition_config *config);

    void GetWakeupConfigs(void *s,
                          void **config, uint32_t *size);
    void GetBufferingConfigs(void *s,
                             struct buffer_config *config);
    void GetSecondStageConfLevels(void *s,
                                  listen_model_indicator_enum type,
                                  int32_t *level);

    void SetSecondStageDetStats(void *s,
                               listen_model_indicator_enum type,
                               struct st_det_engine_stats *info,
                               int32_t level);

    int32_t ParseDetectionPayload(void *s, void *event, uint32_t size);
    void* GetDetectedStream(void *event);
    void* GetDetectionEventInfo(void *stream);
    void GetKeywordIndex(void *s, struct keyword_index *index);
    void GetKeywordStats(void *s, struct keyword_stats *stats);
    void UpdateIndices(void * s, struct keyword_index index);
    void UpdateFtrtData(void * s, uint8_t *data, uint32_t size);
    void UpdateDetectionResult(void *s, uint32_t result);
    int32_t SetDetectionPropList(void *s, detection_prop_list_t *det_prop_list);
    uint32_t GetExtendedPayloadSize(void *s);
    void FillExtendedDetectionPayload(void *s, uint8_t *data, uint32_t size);
    int32_t GenerateCallbackEvent(void *s,
                                  struct pal_st_recognition_event **event,
                                  uint32_t *event_size);

    int32_t UpdateEngineModel(void *s, uint8_t *data,
                uint32_t data_size, bool add);
    int32_t UpdateMergeConfLevelsPayload(SoundModelInfo* src_sm_info,
                               bool set);
    int32_t ParseOpaqueConfLevels(struct sound_model_info *info,
                                  void *opaque_conf_levels,
                                  uint32_t version,
                                  uint8_t **out_conf_levels,
                                  uint32_t *out_num_conf_levels);
    int32_t FillConfLevels(struct sound_model_info *info,
                           struct pal_st_recognition_config *config,
                           uint8_t **out_conf_levels,
                           uint32_t *out_num_conf_levels);
    int32_t FillOpaqueConfLevels(uint32_t model_id,
                                 const void *sm_levels_generic,
                                 uint8_t **out_payload,
                                 uint32_t *out_payload_size,
                                 uint32_t version);
    int32_t ParseDetectionPayloadPDK(void *s, void *event_data);
    int32_t ParseDetectionPayloadGMM(void *s, void *event_data);
    void UpdateKeywordIndex(void *s, uint64_t kwd_start_timestamp,
                            uint64_t kwd_end_timestamp,
                            uint64_t ftrt_start_timestamp);
    void PackEventConfLevels(struct sound_model_info *sm_info,
                             uint8_t *opaque_data);
    void FillCallbackConfLevels(struct sound_model_info *sm_info,
                                uint8_t *opaque_data,
                                uint32_t det_keyword_id,
                                uint32_t best_conf_level);
    void InitCallbackConfLevels(uint32_t version,
                                uint8_t *opaque_data,
                                uint32_t opaque_data_size);
    void CheckAndSetDetectionConfLevels(void *s);
    void* GetPDKDetectedStream(void *event);
    void* GetGMMDetectedStream(void *event);

    int32_t AddSoundModel(void *s, uint8_t *data, uint32_t data_size);
    int32_t DeleteSoundModel(void *s);
    int32_t QuerySoundModel(SoundModelInfo *sm_info,
                            uint8_t *data, uint32_t data_size);
    int32_t MergeSoundModels(uint32_t num_models, listen_model_type *in_models[],
             listen_model_type *out_model);
    int32_t DeleteFromMergedModel(char **keyphrases, uint32_t num_keyphrases,
             listen_model_type *in_model, listen_model_type *out_model);
    int32_t UpdateMergeConfLevelsWithActiveStreams();

    int32_t GetSoundModelLoadPayload(vui_intf_param_t *param);
    int32_t GetSoundModelUnloadPayload(vui_intf_param_t *param);
    int32_t GetWakeUpPayload(vui_intf_param_t *param);
    int32_t GetBufferingPayload(vui_intf_param_t *param);
    int32_t GetEngineResetPayload(vui_intf_param_t *param);

    int32_t SetModelState(void *s, bool state);
    void SetStreamAttributes(struct pal_stream_attributes *attr);

    st_module_type_t GetModuleType(void *s) { return module_type_; }
    SoundModelInfo* GetSoundModelInfo(void *s);
    void SetModelId(void *s, uint32_t model_id);
    void SetSTModuleType(st_module_type_t model_type) {
        module_type_ = model_type;
    }
    void SetRecognitionMode(void *s, uint32_t mode);

    uint32_t GetReadOffset(void *s);
    void SetReadOffset(void *s, uint32_t offset);
    uint32_t UsToBytes(uint64_t input_us);

    st_module_type_t module_type_;
    sound_model_info_map_t sm_info_map_;
    struct pal_stream_attributes str_attr_;
    SoundModelInfo *sound_model_info_;
    bool perf_mode_;

    st_confidence_levels_info *st_conf_levels_;
    st_confidence_levels_info_v2 *st_conf_levels_v2_;

    struct buffer_config default_buf_config_;
    param_id_detection_engine_register_multi_sound_model_t *register_model_;
    struct param_id_detection_engine_deregister_multi_sound_model_t deregister_model_;
    struct param_id_detection_engine_multi_model_buffering_config_t buffering_config_;
    struct param_id_detection_engine_per_model_reset_t per_model_reset_config_;
    struct detection_engine_config_voice_wakeup wakeup_config_;
    uint8_t *wakeup_payload_;
    uint32_t wakeup_payload_size_;
    uint8_t *ftrt_data_;
    uint32_t ftrt_data_size_;

    bool sm_merged_;
    struct detection_event {
        union {
            struct detection_event_info event_info_;
            struct detection_event_info_pdk pdk_event_info_;
        };
        uint32_t det_model_id_;
        uint64_t ftrt_size_us_;
        uint64_t start_ts_;
        uint64_t end_ts_;
        uint32_t start_index_;
        uint32_t end_index_;
    };
    static std::map<st_module_type_t, std::vector<std::shared_ptr<VoiceUIInterface>>> intf_map_;
    static std::mutex intf_create_mutex_;
    // TODO: add below as opaque config for sound model info
    std::map<void *, struct detection_event*> det_event_info_;
    std::map<void *, uint32_t> readOffsets_;
    std::map<void *, uint32_t> origin_hist_buf_duration_;
};

#endif
