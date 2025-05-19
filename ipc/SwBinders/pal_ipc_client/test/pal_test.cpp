/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdio.h>
#include <PalApi.h>
#include <signal.h>
#include <cstring>

pal_stream_handle_t *stream_handle = NULL;

static void sigint_handler(int sig __unused) {
    printf("pal_ipc_test received signal\n");
    // pal_deinit();
    exit(0);
}

static int32_t pal_callback(pal_stream_handle_t *stream_handle,
                            uint32_t event_id, uint32_t *event_data,
                            uint32_t event_data_size,
                            uint64_t cookie) {
    printf("stream_handle: %p, event_id: 0x%x, event_data_size: %d, cookie: 0x%x\n", stream_handle, event_id, event_data_size, cookie);
    uint8_t* event_data_temp = (uint8_t *)event_data;
    for (int i=0; i<event_data_size;i++) {
        printf("%d ", *(event_data_temp + i));
    }
    printf("\n");
    return 0;
}

static void print_ut_start(const char * msg) {
    printf("\n==================%s begin=========================\n", msg);
    printf("*********************************************************\n");
}

static void print_ut_end(const char * msg) {
    printf("*******************************************************\n");
    printf("==================%s end=========================\n", msg);
}

static int pal_stream_open_test() {
    struct pal_stream_attributes stream_attr;
    stream_attr.type = PAL_STREAM_DEEP_BUFFER;
    stream_attr.flags = PAL_STREAM_FLAG_EXTERN_MEM;
    stream_attr.direction = PAL_AUDIO_OUTPUT;
    stream_attr.out_media_config.sample_rate = 44100;
    stream_attr.out_media_config.bit_width = 16;
    struct pal_channel_info ch_info;
    ch_info.channels = 2;

    stream_attr.out_media_config.ch_info = ch_info;
    stream_attr.out_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    struct pal_device devices[2];
    devices[0].id = PAL_DEVICE_OUT_SPEAKER;
    devices[0].config.sample_rate = 48000;
    uint8_t ch_map[] = {PAL_CHMAP_CHANNEL_FL, PAL_CHMAP_CHANNEL_FR, PAL_CHMAP_CHANNEL_C, PAL_CHMAP_CHANNEL_LS,
                      PAL_CHMAP_CHANNEL_RS, PAL_CHMAP_CHANNEL_LFE, PAL_CHMAP_CHANNEL_LB, PAL_CHMAP_CHANNEL_RB };
    struct pal_channel_info ch_info1;
    ch_info1.channels = 2;
    memcpy(ch_info1.ch_map, (const void *)ch_map, 8);
    devices[0].config.ch_info = ch_info1;
    devices[0].config.bit_width = 16;

    devices[1].id = PAL_DEVICE_OUT_SPEAKER;
    devices[1].config.sample_rate = 44100;
    devices[1].config.ch_info = ch_info1;
    devices[1].config.bit_width = 16;
    return pal_stream_open(&stream_attr, 2, devices, 0, nullptr, pal_callback, 0x12345678, &stream_handle);
}

static int pal_stream_close_test() {
    return pal_stream_close(stream_handle);
}

static int pal_stream_start_test() {
    return pal_stream_start(stream_handle);
}

static int pal_stream_stop_test() {
    return pal_stream_stop(stream_handle);
}


static int pal_stream_pause_test() {
    return pal_stream_pause(stream_handle);
}

static int pal_stream_resume_test() {
    return pal_stream_resume(stream_handle);
}

static int pal_stream_flush_test() {
    return pal_stream_flush(stream_handle);
}

static int pal_stream_drain_test() {
    return pal_stream_drain(stream_handle, PAL_DRAIN_PARTIAL);
}

static int pal_stream_suspend_test() {
    return pal_stream_suspend(stream_handle);
}

static int pal_stream_get_buffer_size_test() {
    return pal_stream_get_buffer_size(stream_handle, nullptr, nullptr);
}

static int pal_stream_get_tags_with_module_info_test() {
    size_t size = 0;
    uint8_t payload[1024];
    int rc = pal_stream_get_tags_with_module_info(stream_handle, &size, payload);
    printf("size: %d\n", size);
    if (size > 0) printf("%d-%d-%d-%d\n", payload[0], payload[1], payload[2], payload[3]);
    return rc;
}

static int pal_stream_set_buffer_size_test() {
    pal_buffer_config_t in_buff_cfg, out_buff_cfg;
    in_buff_cfg.buf_count = 10086;
    in_buff_cfg.buf_size = 230;
    out_buff_cfg.buf_count = 10000;
    out_buff_cfg.buf_size = 130;
    return pal_stream_set_buffer_size(stream_handle, &in_buff_cfg, &out_buff_cfg);
}

static int pal_stream_read_test() {
    struct pal_buffer buffer = {0};
    uint8_t buf[128], metadata[20];
    for (int i=0;i<128;i++) buf[i] = i;
    buffer.buffer = buf;
    buffer.size = 64;
    buffer.offset = 64;
    buffer.frame_index = 9;
    buffer.metadata_size = 20;
    buffer.metadata = metadata;
    buffer.flags = 0x101;
    return pal_stream_read(stream_handle, &buffer);
}

static int pal_stream_write_test() {
    struct pal_buffer buffer = {0};
    uint8_t buf[128], metadata[20];
    for (int i=0;i<128;i++) buf[i] = i;
    buffer.buffer = NULL;
    buffer.size = 64;
    buffer.offset = 64;
    buffer.frame_index = 9;
    buffer.metadata_size = 20;
    buffer.metadata = metadata;
    buffer.flags = 0x101;
    return pal_stream_write(stream_handle, &buffer);
}

static int pal_stream_get_device_test() {
    return pal_stream_get_device(stream_handle, 0, nullptr);
}

static int pal_stream_set_device_test() {
    struct pal_device devices[2];
    devices[0].id = PAL_DEVICE_OUT_SPEAKER;
    devices[0].config.sample_rate = 48000;
    uint8_t ch_map[] = {PAL_CHMAP_CHANNEL_FL, PAL_CHMAP_CHANNEL_FR, PAL_CHMAP_CHANNEL_C, PAL_CHMAP_CHANNEL_LS,
                      PAL_CHMAP_CHANNEL_RS, PAL_CHMAP_CHANNEL_LFE, PAL_CHMAP_CHANNEL_LB, PAL_CHMAP_CHANNEL_RB };
    struct pal_channel_info ch_info1;
    ch_info1.channels = 2;
    memcpy(ch_info1.ch_map, (const void *)ch_map, 8);
    devices[0].config.ch_info = ch_info1;
    devices[0].config.bit_width = 16;

    devices[1].id = PAL_DEVICE_OUT_BLUETOOTH_SCO;
    devices[1].config.sample_rate = 44100;
    devices[1].config.ch_info = ch_info1;
    devices[1].config.bit_width = 16;
    return pal_stream_set_device(stream_handle, 2, devices);
}

static int pal_stream_get_param_test() {
    pal_param_payload *payload = NULL;
    int rc = pal_stream_get_param(stream_handle, 0x101, &payload);
    if (payload) {
        printf("%s(%d) size: %d, [2]: %d", __func__, __LINE__, payload->payload_size, payload->payload[2]);
        free(payload);
    } else printf("%s(%d) error get payload.\n", __func__, __LINE__);
    return rc;
}

static int pal_stream_set_param_test() {
    uint32_t payload_size = 4;
    uint8_t* payload_data = (uint8_t *)calloc(1, payload_size + sizeof(pal_param_payload));
    pal_param_payload *payload = (pal_param_payload *)payload_data;

    printf("%s(%d) pal_param_payload size: %d, payload: %p, payload->payload: %p\n", __func__, __LINE__, sizeof(pal_param_payload), payload, payload->payload);
    payload->payload_size = payload_size;
    payload->payload[0] = 87;
    payload->payload[1] = 39;
    payload->payload[2] = 42;
    payload->payload[3] = 128;
    int rc = pal_stream_set_param(stream_handle, 0x100, payload);
    free(payload_data);
    return rc;
}

static int pal_stream_get_volume_test() {
    return pal_stream_get_volume(stream_handle, nullptr);
}

static int pal_stream_set_volume_test() {
    uint32_t no_of_volpair = 2;
    uint8_t* volume_data = (uint8_t *)calloc(1, no_of_volpair * sizeof(struct pal_channel_vol_kv) + sizeof(struct pal_volume_data));
    struct pal_volume_data *volume = (struct pal_volume_data *)volume_data;
    volume->no_of_volpair = no_of_volpair;
    for (int i=0;i<no_of_volpair;i++) {
        volume->volume_pair[i].channel_mask = 0x200 | (i + 3);
        volume->volume_pair[i].vol = 0.709 + i;
    }
    return pal_stream_set_volume(stream_handle, volume);
}

static int pal_stream_get_mute_test() {
    return pal_stream_get_mute(stream_handle, nullptr);
}

static int pal_stream_set_mute_test() {
    return pal_stream_set_mute(stream_handle, false);
}

static int pal_get_mic_mute_test() {
    return pal_get_mic_mute(nullptr);
}

static int pal_set_mic_mute_test() {
    return pal_set_mic_mute(false);
}

static int pal_get_timestamp_test() {
    struct pal_session_time stime;
    stime.timestamp.value_lsw = 10;
    stime.timestamp.value_msw = 11;
    stime.session_time.value_lsw = 20;
    stime.session_time.value_msw = 21;
    stime.absolute_time.value_lsw = 30;
    stime.absolute_time.value_msw = 31;
    return pal_get_timestamp(stream_handle, &stime);
}

static int pal_add_remove_effect_test() {
    return pal_add_remove_effect(stream_handle, PAL_AUDIO_EFFECT_NONE, false);
}

static int pal_set_param_test() {
    return pal_set_param(0, nullptr, 0);
}

static int pal_get_param_test() {
    card_status_t hw_state;
    card_status_t *hw_state_ptr = &hw_state;
    size_t size = 0;
    int rc = pal_get_param(PAL_PARAM_ID_SNDCARD_STATE, (void **)&hw_state_ptr, &size, nullptr);
    printf("size: %d\n", size);
    printf("size: %d, state: %d\n", size, *hw_state_ptr);
    return rc;
}

static int pal_stream_create_mmap_buffer_test() {
    struct pal_mmap_buffer mmap_buffer;
    mmap_buffer.buffer = (void *)0x55896743;
    mmap_buffer.buffer_size_frames = 100;
    mmap_buffer.burst_size_frames = 20;
    mmap_buffer.fd = 34;
    mmap_buffer.flags = 0x200;
    return pal_stream_create_mmap_buffer(stream_handle, 120, &mmap_buffer);
}

static int pal_stream_get_mmap_position_test() {
    struct pal_mmap_position position = {0};
    int rc = pal_stream_get_mmap_position(stream_handle, &position);
    printf("na: %lld, frames: %d\n", position.time_nanoseconds, position.position_frames);
    return rc;
}

static int pal_register_global_callback_test() {
    return pal_register_global_callback(nullptr, 0);
}

static int pal_gef_rw_param_test() {
    uint8_t payload[8];
    for (int i=0;i<8;i++) payload[i] = i + 200;
    return pal_gef_rw_param(0x324, (void *)payload, 8, PAL_DEVICE_OUT_SPEAKER, PAL_STREAM_DEEP_BUFFER, 2);
}

static int pal_gef_rw_param_acdb_test() {
    uint8_t payload[16];
    for (int i=0;i<16;i++) payload[i] = i + 100;
    return pal_gef_rw_param_acdb(0x367, payload, 16, PAL_DEVICE_NONE, PAL_STREAM_VOICE_CALL_MUSIC, 48000, 10086, 4, true);
}

int main() {
    signal(SIGINT, sigint_handler);
    // printf("pal_init ret: %d\n", pal_init());
    // printf("pal_version: %s\n", pal_get_version());
    // printf("pal_stream_open: %d\n", pal_stream_open_test());
    // printf("pal_stream_start: %d\n", pal_stream_start_test());
    // printf("pal_stream_stop: %d\n", pal_stream_stop_test());
    // printf("pal_stream_pause: %d\n", pal_stream_pause_test());
    // printf("pal_stream_resume: %d\n", pal_stream_resume_test());
    // printf("pal_stream_flush: %d\n", pal_stream_flush_test());
    // printf("pal_stream_drain: %d\n", pal_stream_drain_test());
    // printf("pal_stream_suspend: %d\n", pal_stream_suspend_test());
    // printf("pal_stream_get_buffer_size: %d\n", pal_stream_get_buffer_size_test());
    printf("pal_stream_get_tags_with_module_info: %d\n", pal_stream_get_tags_with_module_info_test());
    // printf("pal_stream_set_buffer_size: %d\n", pal_stream_set_buffer_size_test());
    // printf("pal_stream_read: %d\n", pal_stream_read_test());
    // printf("pal_stream_write: %d\n", pal_stream_write_test());
    // printf("pal_stream_get_device: %d\n", pal_stream_get_device_test());
    // printf("pal_stream_set_device: %d\n", pal_stream_set_device_test());
    // printf("pal_stream_get_param: %d\n", pal_stream_get_param_test());
    // printf("pal_stream_set_param: %d\n", pal_stream_set_param_test());
    // printf("pal_stream_get_volume: %d\n", pal_stream_get_volume_test());
    // printf("pal_stream_set_volume: %d\n", pal_stream_set_volume_test());
    // printf("pal_stream_get_mute: %d\n", pal_stream_get_mute_test());
    // printf("pal_stream_set_mute: %d\n", pal_stream_set_mute_test());
    // printf("pal_get_mic_mute: %d\n", pal_get_mic_mute_test());
    // printf("pal_set_mic_mute: %d\n", pal_set_mic_mute_test());
    // printf("pal_get_timestamp: %d\n", pal_get_timestamp_test());
    // printf("pal_add_remove_effect: %d\n", pal_add_remove_effect_test());
    // printf("pal_set_param: %d\n", pal_set_param_test());
    printf("pal_get_param: %d\n", pal_get_param_test());
    // printf("pal_stream_create_mmap_buffer: %d\n", pal_stream_create_mmap_buffer_test());
    // printf("pal_stream_get_mmap_position: %d\n", pal_stream_get_mmap_position_test());
    // printf("pal_register_global_callback: %d\n", pal_register_global_callback_test());
    // printf("pal_gef_rw_param: %d\n", pal_gef_rw_param_test());
    // printf("pal_gef_rw_param_acdb: %d\n", pal_gef_rw_param_acdb_test());
    // printf("pal_stream_close: %d\n", pal_stream_close_test());

    while (1);
    return 0;
}

