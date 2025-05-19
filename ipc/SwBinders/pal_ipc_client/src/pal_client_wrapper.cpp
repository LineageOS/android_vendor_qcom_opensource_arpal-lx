/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/IInterface.h>
#include <binder/IBinder.h>
#include <palserver/ipc_interface.h>
#include <palserver/pal_client_manager.h>
#include <PalApi.h>

#define LOG_TAG             "pal_client_wrapper"

static char pal_version[32];
static android::sp<IPalService> pal_client = NULL;
static android::sp<ServerDeathNotifier> server_death_notifier = NULL;
static bool pal_server_died = false;

ServerDeathNotifier::ServerDeathNotifier() {
    android::sp<android::ProcessState> proc(android::ProcessState::self());
    proc->startThreadPool();
}

void ServerDeathNotifier::binderDied(const android::wp<android::IBinder>& who __attribute__((unused))) {
    pal_server_died = true;
}

static android::sp<IPalService> get_pal_server() {
    if (!pal_client.get()) {
        android::sp<android::IBinder> binder =
                   android::defaultServiceManager()->getService(android::String16(PAL_SERVICE_NAME));
        pal_client = android::interface_cast<IPalService>(binder);
        server_death_notifier = new ServerDeathNotifier();
        binder->linkToDeath(server_death_notifier);
    }

    return pal_client;
}


int32_t pal_init() {
    return 0;
}

const char* pal_get_version() {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            std::string version = pal->ipc_pal_get_version();
            memcpy(pal_version, version.c_str(), version.size()>31?31:version.size());
        }
    }
    return pal_version;
}

void pal_deinit() {
    if (pal_client.get()) {
        pal_client->ipc_pal_deinit();
        pal_client.clear();
    }
}

int32_t pal_stream_open(struct pal_stream_attributes *attributes,
                            uint32_t no_of_devices, struct pal_device *devices,
                            uint32_t no_of_modifiers, struct modifier_kv *modifiers,
                            pal_stream_callback cb, uint64_t cookie,
                            pal_stream_handle_t **stream_handle) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_open(attributes, no_of_devices, devices, no_of_modifiers, modifiers,
                                            cb, cookie, stream_handle);
        }
    }

    return -1;
}

int32_t pal_stream_close(pal_stream_handle_t *stream_handle) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_close(stream_handle);
        }
    }

    return -1;
}

int32_t pal_stream_start(pal_stream_handle_t *stream_handle) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_start(stream_handle);
        }
    }

    return -1;
}

int32_t pal_stream_stop(pal_stream_handle_t *stream_handle) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_stop(stream_handle);
        }
    }

    return -1;
}

int32_t pal_stream_pause(pal_stream_handle_t *stream_handle) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_pause(stream_handle);
        }
    }

    return -1;
}

int32_t pal_stream_resume(pal_stream_handle_t *stream_handle) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_resume(stream_handle);
        }
    }

    return -1;
}

int32_t pal_stream_flush(pal_stream_handle_t *stream_handle) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_flush(stream_handle);
        }
    }

    return -1;
}

int32_t pal_stream_drain(pal_stream_handle_t *stream_handle, pal_drain_type_t type) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_drain(stream_handle, type);
        }
    }

    return -1;
}

int32_t pal_stream_suspend(pal_stream_handle_t *stream_handle) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_suspend(stream_handle);
        }
    }

    return -1;
}

int32_t pal_stream_get_buffer_size(pal_stream_handle_t *stream_handle,
                                   size_t *in_buffer, size_t *out_buffer) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_get_buffer_size(stream_handle, in_buffer, out_buffer);
        }
    }

    return -1;
}

int32_t pal_stream_get_tags_with_module_info(pal_stream_handle_t *stream_handle,
                                             size_t *size ,uint8_t *payload) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_get_tags_with_module_info(stream_handle, size, payload);
        }
    }

    return -1;
}

int32_t pal_stream_set_buffer_size(pal_stream_handle_t *stream_handle,
                                    pal_buffer_config_t *in_buff_cfg,
                                    pal_buffer_config_t *out_buff_cfg) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_set_buffer_size(stream_handle, in_buff_cfg, out_buff_cfg);
        }
    }

    return -1;
}

ssize_t pal_stream_read(pal_stream_handle_t *stream_handle, struct pal_buffer *buf) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_read(stream_handle, buf);
        }
    }

    return -1;
}

ssize_t pal_stream_write(pal_stream_handle_t *stream_handle, struct pal_buffer *buf) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_write(stream_handle, buf);
        }
    }

    return -1;
}

int32_t pal_stream_get_device(pal_stream_handle_t *stream_handle,
                                            uint32_t no_of_devices, struct pal_device *devices) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_get_device(stream_handle, no_of_devices, devices);
        }
    }

    return -1;
}

int32_t pal_stream_set_device(pal_stream_handle_t *stream_handle,
                                            uint32_t no_of_devices, struct pal_device *devices) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_set_device(stream_handle, no_of_devices, devices);
        }
    }

    return -1;
}

int32_t pal_stream_get_param(pal_stream_handle_t *stream_handle,
                                            uint32_t param_id, pal_param_payload **param_payload) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_get_param(stream_handle, param_id, param_payload);
        }
    }

    return -1;
}

int32_t pal_stream_set_param(pal_stream_handle_t *stream_handle,
                                            uint32_t param_id, pal_param_payload *param_payload) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_set_param(stream_handle, param_id, param_payload);
        }
    }

    return -1;
}

int32_t pal_stream_get_volume(pal_stream_handle_t *stream_handle,
                                            struct pal_volume_data *volume) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_get_volume(stream_handle, volume);
        }
    }

    return -1;
}

int32_t pal_stream_set_volume(pal_stream_handle_t *stream_handle,
                                            struct pal_volume_data *volume) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_set_volume(stream_handle, volume);
        }
    }

    return -1;
}

int32_t pal_stream_get_mute(pal_stream_handle_t *stream_handle, bool *state) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_get_mute(stream_handle, state);
        }
    }

    return -1;
}

int32_t pal_stream_set_mute(pal_stream_handle_t *stream_handle, bool state) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_set_mute(stream_handle, state);
        }
    }

    return -1;
}

int32_t pal_get_mic_mute(bool *state) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_get_mic_mute(state);
        }
    }

    return -1;
}

int32_t pal_set_mic_mute(bool state) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_set_mic_mute(state);
        }
    }

    return -1;
}

int32_t pal_get_timestamp(pal_stream_handle_t *stream_handle, struct pal_session_time *stime) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_get_timestamp(stream_handle, stime);
        }
    }

    return -1;
}

int32_t pal_add_remove_effect(pal_stream_handle_t *stream_handle, pal_audio_effect_t effect, bool enable) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_add_remove_effect(stream_handle, effect, enable);
        }
    }

    return -1;
}

int32_t pal_set_param(uint32_t param_id, void *param_payload, size_t payload_size) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_set_param(param_id, param_payload, payload_size);
        }
    }

    return -1;
}

int32_t pal_get_param(uint32_t param_id, void **param_payload, size_t *payload_size, void *query) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_get_param(param_id, param_payload, payload_size, query);
        }
    }

    return -1;
}

int32_t pal_stream_create_mmap_buffer(pal_stream_handle_t *stream_handle,
                                                int32_t min_size_frames,
                                                struct pal_mmap_buffer *info) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_create_mmap_buffer(stream_handle, min_size_frames, info);
        }
    }

    return -1;
}
int32_t pal_stream_get_mmap_position(pal_stream_handle_t *stream_handle,
                                        struct pal_mmap_position *position) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_stream_get_mmap_position(stream_handle, position);
        }
    }

    return -1;
}

int32_t pal_register_global_callback(pal_global_callback cb, uint64_t cookie) {
    return -1;
}

int32_t pal_gef_rw_param(uint32_t param_id, void *param_payload,
                            size_t payload_size, pal_device_id_t pal_device_id,
                            pal_stream_type_t pal_stream_type, unsigned int dir) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_gef_rw_param(param_id, param_payload, payload_size, pal_device_id, pal_stream_type, dir);
        }
    }

    return -1;
}

int32_t pal_gef_rw_param_acdb(uint32_t param_id, void *param_payload,
                                size_t payload_size, pal_device_id_t pal_device_id,
                                pal_stream_type_t pal_stream_type, uint32_t sample_rate,
                                uint32_t instance_id, uint32_t dir, bool is_play) {
    if (!pal_server_died) {
        android::sp<IPalService> pal = get_pal_server();
        if (pal.get()) {
            return pal->ipc_pal_gef_rw_param_acdb(param_id, param_payload, payload_size, pal_device_id, pal_stream_type,
                                                  sample_rate, instance_id, dir, is_play);
        }
    }

    return -1;
}
