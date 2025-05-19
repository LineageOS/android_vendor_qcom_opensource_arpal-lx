/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <binder/IInterface.h>
#include <binder/IBinder.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <utils/String8.h>
#include <utils/String16.h>
#include <PalDefs.h>

#define PAL_SERVICE_NAME        "pal.service"
#define TAG_MODULE_LIST_SIZE    1024

class IPalService: public ::android::IInterface {
    public:
        DECLARE_META_INTERFACE(PalService);
        virtual int32_t ipc_pal_init()= 0;
        virtual std::string ipc_pal_get_version() = 0;

        virtual void ipc_pal_deinit() = 0;
        virtual int32_t ipc_pal_stream_open(struct pal_stream_attributes *attributes,
                                            uint32_t no_of_devices, struct pal_device *devices,
                                            uint32_t no_of_modifiers, struct modifier_kv *modifiers,
                                            pal_stream_callback cb, uint64_t cookie,
                                            pal_stream_handle_t **stream_handle) = 0;

        virtual int32_t ipc_pal_stream_close(pal_stream_handle_t *stream_handle) = 0;
        virtual int32_t ipc_pal_stream_start(pal_stream_handle_t *stream_handle) = 0;
        virtual int32_t ipc_pal_stream_stop(pal_stream_handle_t *stream_handle) = 0;
        virtual int32_t ipc_pal_stream_pause(pal_stream_handle_t *stream_handle) = 0;
        virtual int32_t ipc_pal_stream_resume(pal_stream_handle_t *stream_handle) = 0;
        virtual int32_t ipc_pal_stream_flush(pal_stream_handle_t *stream_handle) = 0;

        virtual int32_t ipc_pal_stream_drain(pal_stream_handle_t *stream_handle, pal_drain_type_t type) = 0;
        virtual int32_t ipc_pal_stream_suspend(pal_stream_handle_t *stream_handle) = 0;
        // This api is not yet implemented in PAL
        virtual int32_t ipc_pal_stream_get_buffer_size(pal_stream_handle_t *stream_handle,
                                                       size_t *in_buffer, size_t *out_buffer) = 0;
        virtual int32_t ipc_pal_stream_get_tags_with_module_info(pal_stream_handle_t *stream_handle,
                                                                 size_t *size ,uint8_t *payload) = 0;
        virtual int32_t ipc_pal_stream_set_buffer_size(pal_stream_handle_t *stream_handle,
                                                       pal_buffer_config_t *in_buff_cfg,
                                                       pal_buffer_config_t *out_buff_cfg) = 0;

        virtual ssize_t ipc_pal_stream_read(pal_stream_handle_t *stream_handle, struct pal_buffer *buf) = 0;
        virtual ssize_t ipc_pal_stream_write(pal_stream_handle_t *stream_handle, struct pal_buffer *buf) = 0;
        // This api is not yet implemented in PAL
        virtual int32_t ipc_pal_stream_get_device(pal_stream_handle_t *stream_handle,
                                        uint32_t no_of_devices, struct pal_device *devices) = 0;
        virtual int32_t ipc_pal_stream_set_device(pal_stream_handle_t *stream_handle,
                                        uint32_t no_of_devices, struct pal_device *devices) = 0;
        virtual int32_t ipc_pal_stream_get_param(pal_stream_handle_t *stream_handle,
                                        uint32_t param_id, pal_param_payload **param_payload) = 0;
        virtual int32_t ipc_pal_stream_set_param(pal_stream_handle_t *stream_handle,
                                        uint32_t param_id, pal_param_payload *param_payload) = 0;
        // This api is not yet implemented in PAL
        virtual int32_t ipc_pal_stream_get_volume(pal_stream_handle_t *stream_handle,
                                        struct pal_volume_data *volume) = 0;
        virtual int32_t ipc_pal_stream_set_volume(pal_stream_handle_t *stream_handle,
                                        struct pal_volume_data *volume) = 0;
        // This api is not yet implemented in PAL
        virtual int32_t ipc_pal_stream_get_mute(pal_stream_handle_t *stream_handle, bool *state) = 0;
        virtual int32_t ipc_pal_stream_set_mute(pal_stream_handle_t *stream_handle, bool state) = 0;

        // This api is not yet implemented in PAL
        virtual int32_t ipc_pal_get_mic_mute(bool *state) = 0;
        // This api is not yet implemented in PAL
        virtual int32_t ipc_pal_set_mic_mute(bool state) = 0;
        virtual int32_t ipc_pal_get_timestamp(pal_stream_handle_t *stream_handle, struct pal_session_time *stime) = 0;
        virtual int32_t ipc_pal_add_remove_effect(pal_stream_handle_t *stream_handle,
                                                  pal_audio_effect_t effect, bool enable) = 0;
        virtual int32_t ipc_pal_set_param(uint32_t param_id, void *param_payload, size_t payload_size) = 0;
        virtual int32_t ipc_pal_get_param(uint32_t param_id, void **param_payload,
                                size_t *payload_size, void *query) = 0;

        virtual int32_t ipc_pal_stream_create_mmap_buffer(pal_stream_handle_t *stream_handle,
                                                int32_t min_size_frames,
                                                struct pal_mmap_buffer *info) = 0;
        virtual int32_t ipc_pal_stream_get_mmap_position(pal_stream_handle_t *stream_handle,
                                                struct pal_mmap_position *position) = 0;
        virtual int32_t ipc_pal_register_global_callback(pal_global_callback cb, uint64_t cookie) = 0;
        virtual int32_t ipc_pal_gef_rw_param(uint32_t param_id, void *param_payload,
                                    size_t payload_size, pal_device_id_t pal_device_id,
                                    pal_stream_type_t pal_stream_type, unsigned int dir) = 0;
        virtual int32_t ipc_pal_gef_rw_param_acdb(uint32_t param_id, void *param_payload,
                                        size_t payload_size, pal_device_id_t pal_device_id,
                                        pal_stream_type_t pal_stream_type, uint32_t sample_rate,
                                        uint32_t instance_id, uint32_t dir, bool is_play) = 0;
};

class BnPalService : public ::android::BnInterface<IPalService> {
    android::status_t onTransact(uint32_t code,
                                   const android::Parcel& data,
                                   android::Parcel* reply, uint32_t flags);

};
