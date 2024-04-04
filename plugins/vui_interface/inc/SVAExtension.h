/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SVA_EXTENSION_H
#define SVA_EXTENSION_H

#include <map>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>

#include "VoiceUIInterface.h"

#define MAX_SVA_KEYWORDS (20)

/*
 * Two types of timeout durations for timer thread
 * TIMEOUT_MS: timeout duration for regular client operation(set/get)
 * CAP_TIMEOUT_MS: timeout duration for client lab reading
 */
#define TIMEOUT_MS (1000)
#define CAP_TIMEOUT_MS (20000)

using ChronoSteadyClock_t = std::chrono::time_point<std::chrono::steady_clock>;

static const struct st_uuid qcva_uuid =
    { 0x68ab2d40, 0xe860, 0x11e3, 0x95ef, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } };

struct va_kwd_info {
    std::string kwd_name;
    uint32_t kwd_level;
};

struct va_detection_info {
    bool is_detected;
    pal_stream_handle_t *handle;
    struct va_kwd_info kwd_info;
};

// structure used for va config parsing
struct va_config {
    uint32_t client_id;
    bool va_on;
    uint32_t num_kwds;
    struct va_kwd_info kwd_info[MAX_SVA_KEYWORDS];
    uint32_t history_buf_sz;
    uint32_t pre_roll_sz;
};

// used for client status/detection info update
struct client_config {
    ChronoSteadyClock_t last_op_ts;
    uint32_t timeout_duration_ms;
    std::vector<struct va_kwd_info *> kwd_info_list;
    struct va_detection_info det_info;
};

// sound model based config
struct sm_config {
    std::string sm_name;
    void *sm_data;
    uint32_t num_active_kwds;
    uint32_t num_kwds;
    struct va_kwd_info kwd_info[MAX_SVA_KEYWORDS];
    uint32_t history_buf_sz;
    uint32_t pre_roll_sz;
    pal_stream_handle_t *handle;
    bool is_restart_needed;
};

static std::map<int32_t, int32_t> client_status_map_;
static std::map<int32_t, struct client_config *> client_config_map_;
static std::map<std::string, struct sm_config *> sm_config_map_;
static std::map<pal_stream_handle_t *, struct sm_config *> pal_hdl_sm_config_map_;
static std::map<std::string, int32_t> kwd_client_map_;

static const std::vector<std::string> kwd_list_ = {
    "Hey Walmart",
    "Hey Zinger",
    "Find My Phone",
    "Hey iHeart",
    "Volume Up",
    "Phone Unlock",
    "Hey Snapdragon",
    "Hey Siri",
    "Hello Mini",
    "Ok BMW"
};

static const std::map<std::string, std::string> kwd_sm_map_ = {
    {"Hey Walmart", "sm8_gr3UsMFCN230612eAIv34ENPUv4Float.uim"},
    {"Hey Zinger", "sm8_gr3UsMFCN230612eAIv34ENPUv4Float.uim"},
    {"Find My Phone", "sm8_gr3UsMFCN230612eAIv34ENPUv4Float.uim"},
    {"Hey iHeart", "sm8_gr3UsMFCN230612eAIv34ENPUv4Float.uim"},
    {"Volume Up", "sm8_gr3UsMFCN230612eAIv34ENPUv4Float.uim"},
    {"Phone Unlock", "sm8_gr1UsPdk6XsMfcn220819Enpu4FloateAIv34.uim"},
    {"Hey Snapdragon", "sm8_gr1UsPdk6XsMfcn220819Enpu4FloateAIv34.uim"},
    {"Hey Siri", "sm8_gr1UsPdk6XsMfcn220819Enpu4FloateAIv34.uim"},
    {"Hello Mini", "sm8_gr1UsPdk6XsMfcn220819Enpu4FloateAIv34.uim"},
    {"Ok BMW", "sm8_gr1UsPdk6XsMfcn220819Enpu4FloateAIv34.uim"}
};

typedef int32_t (*pal_stream_open_t)(struct pal_stream_attributes *attributes,
    uint32_t no_of_devices, struct pal_device *devices,
    uint32_t no_of_modifiers, struct modifier_kv *modifiers,
    pal_stream_callback cb, uint64_t cookie,
    pal_stream_handle_t **stream_handle);

typedef int32_t (*pal_stream_ops_t)(pal_stream_handle_t *stream_handle);

typedef int32_t (*pal_stream_set_param_t)(pal_stream_handle_t *stream_handle,
    uint32_t param_id, pal_param_payload *param_payload);

class SVAExtension {
  public:
    SVAExtension();
    ~SVAExtension();

    static std::shared_ptr<SVAExtension> GetInstance();
    bool IsInitialized() { return is_initialized_; }

    int32_t SetParameters(uint32_t param_id,
        void *param_payload, size_t payload_size);
    int32_t GetParameters(uint32_t param_id,
        void **param_payload, size_t *payload_size);

  private:
    int32_t ParseClientId(void *payload, char **save);
    int32_t HandleSVAOnOff(int32_t client_id, char **save);
    int32_t EnableSVA(int32_t client_id, struct va_config *va_cfg);
    int32_t DisableSVA(int32_t client_id, struct va_config *va_cfg);
    int32_t GetAvailableKeywords(int32_t client_id,
        void **payload, size_t *payload_sz);
    int32_t GetDetectionPayload(int32_t client_id,
        void **payload, size_t *payload_sz);
    int32_t GetPalHandle(int32_t client_id,
        char **save, void **payload, size_t *payload_sz);

    void* GetSoundModel(int32_t client_id, std::string sm_name);
    void* GetRecognitionConfig(struct sm_config *config);

    int32_t UpdateClientConfig(int32_t client_id,
        struct client_config *client_cfg, struct va_config *va_cfg);
    std::vector<struct sm_config *> UpdateSoundModelConfig(
        struct va_config *va_cfg, bool is_enable);

    int32_t StartSoundModel(struct va_config *va_cfg);
    int32_t StopSoundModel(struct va_config *va_cfg);
    int32_t LoadSoundModel(void *sm_data, pal_stream_handle_t **handle);
    int32_t UnloadSoundModel(pal_stream_handle_t *handle);
    int32_t StartRecognition(pal_stream_handle_t *handle);
    int32_t StopRecognition(pal_stream_handle_t *handle);
    int32_t OpenPALStream(pal_stream_handle_t **handle);
    static int32_t pal_callback(pal_stream_handle_t *stream_handle,
        uint32_t event_id, uint32_t *event_data,
        uint32_t event_size, uint64_t cookie);
    static void TimerThread(SVAExtension& ext);

    static std::shared_ptr<SVAExtension> ext_;
    std::thread timer_thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool exit_timer_thread_;

    bool is_initialized_;
    void *pal_lib_handle_;
    pal_stream_open_t pal_stream_open_;
    pal_stream_ops_t pal_stream_close_;
    pal_stream_ops_t pal_stream_start_;
    pal_stream_ops_t pal_stream_stop_;
    pal_stream_set_param_t pal_stream_set_param_;
};

#endif
