/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdio.h>
#include <binder/IInterface.h>
#include <binder/IBinder.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <utils/String8.h>
#include <utils/String16.h>
#include <PalDefs.h>

#define _PLOGD(fmt, args...)        do { \
                                        printf(fmt"\n", ##args); \
                                    } while (0)
#define PLOGD(arg, ...)   _PLOGD("%s %s(%d): "  arg, LOG_TAG, __func__, __LINE__, ##__VA_ARGS__)


class PalReadWriteTool {
public:
    static void write_param_payload(uint32_t size, uint8_t *param_payload, android::Parcel& parcel);
    static void write_param_payload(uint32_t size, uint8_t *param_payload, android::Parcel* parcel);
    static void read_param_payload(uint32_t &size, uint8_t **param_payload, const android::Parcel& parcel);
    static void read_param_payload(uint32_t &size, uint8_t *param_payload, const android::Parcel& parcel);
    static void write_pal_stream_attributes(struct pal_stream_attributes *attributes, android::Parcel& parcel);
    static void read_pal_stream_attributes(struct pal_stream_attributes **attributes, const android::Parcel& parcel);
    static void write_pal_devices(uint32_t no_of_devices, struct pal_device *devices, android::Parcel& parcel);
    static void read_pal_devices(uint32_t &no_of_devices, struct pal_device **devices, const android::Parcel& parcel);
    static void write_modifier_kv(uint32_t no_of_modifiers, struct modifier_kv *modifiers, android::Parcel& parcel);
    static void read_modifier_kv(uint32_t &no_of_modifiers, struct modifier_kv **modifiers, const android::Parcel& parcel);
    static void write_pal_buffer_config(pal_buffer_config_t *buff_cfg, android::Parcel& parcel);
    static void read_pal_buffer_config(pal_buffer_config_t **buff_cfg, const android::Parcel& parcel);
    static void write_pal_buffer(struct pal_buffer *pal_buff, android::Parcel& parcel);
    static void read_pal_buffer(pal_buffer **pal_buff, const android::Parcel& parcel);
    static void write_pal_callback_buffer(struct pal_callback_buffer *callback_buff, android::Parcel& parcel);
    static void read_pal_callback_buffer(struct pal_callback_buffer **callback_buff, const android::Parcel& parcel);
    static void write_pal_volume_data(struct pal_volume_data *volume, android::Parcel& parcel);
    static void read_pal_volume_data(struct pal_volume_data **volume, const android::Parcel& parcel);
    static void write_pal_session_time(struct pal_session_time *stime, android::Parcel* parcel);
    static void read_pal_session_time(struct pal_session_time *stime, const android::Parcel& parcel);
    static void write_pal_mmap_buffer(struct pal_mmap_buffer *info, android::Parcel* parcel);
    static void read_pal_mmap_buffer(struct pal_mmap_buffer *info, const android::Parcel& parcel);
    static void write_pal_mmap_position(struct pal_mmap_position *position, android::Parcel* parcel);
    static void read_pal_mmap_position(struct pal_mmap_position *position, const android::Parcel& parcel);
};
