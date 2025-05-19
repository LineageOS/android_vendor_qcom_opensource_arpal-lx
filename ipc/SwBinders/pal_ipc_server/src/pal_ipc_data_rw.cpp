/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "pal_ipc_data_rw.h"

void PalReadWriteTool::write_param_payload(uint32_t size, uint8_t *param_payload, android::Parcel& parcel) {
    if (size <= 0) {
        parcel.writeUint32(0);
        return;
    }

    parcel.writeUint32(size);
    android::Parcel::WritableBlob blob;
    parcel.writeBlob(size, false, &blob);
    memset(blob.data(), 0, size);
    memcpy(blob.data(), param_payload, size);
}

void PalReadWriteTool::write_param_payload(uint32_t size, uint8_t *param_payload, android::Parcel* parcel) {
    if (size <= 0) {
        parcel->writeUint32(0);
        return;
    }

    parcel->writeUint32(size);
    android::Parcel::WritableBlob blob;
    parcel->writeBlob(size, false, &blob);
    memset(blob.data(), 0, size);
    memcpy(blob.data(), param_payload, size);
}

void PalReadWriteTool::read_param_payload(uint32_t &size, uint8_t **param_payload, const android::Parcel& parcel) {
    size = parcel.readUint32();

    if (size <= 0) {
        *param_payload = NULL;
        return;
    }

    uint8_t *data = new uint8_t[size];
    android::Parcel::ReadableBlob blob;
    parcel.readBlob(size, &blob);
    memcpy(data, blob.data(), size);
    *param_payload = data;
}

void PalReadWriteTool::read_param_payload(uint32_t &size, uint8_t *param_payload, const android::Parcel& parcel) {
    size = parcel.readUint32();
    if (size <= 0 || (!param_payload)) {
        return;
    }

    android::Parcel::ReadableBlob blob;
    parcel.readBlob(size, &blob);
    memcpy(param_payload, blob.data(), size);
}

void PalReadWriteTool::write_pal_stream_attributes(struct pal_stream_attributes *attributes, android::Parcel& parcel) {
    write_param_payload(attributes?sizeof(struct pal_stream_attributes):0, (uint8_t *)attributes, parcel);
}

void PalReadWriteTool::read_pal_stream_attributes(struct pal_stream_attributes **attributes, const android::Parcel& parcel) {
    uint32_t size = 0;
    read_param_payload(size, (uint8_t **)attributes, parcel);
}

void PalReadWriteTool::write_pal_devices(uint32_t no_of_devices, struct pal_device *devices, android::Parcel& parcel) {
    uint32_t size = no_of_devices * sizeof(struct pal_device);
    write_param_payload(size, (uint8_t *)devices, parcel);
}

void PalReadWriteTool::read_pal_devices(uint32_t &no_of_devices, struct pal_device **devices, const android::Parcel& parcel) {
    uint32_t size = 0;
    read_param_payload(size, (uint8_t **)devices, parcel);
    no_of_devices = size / sizeof(struct pal_device);
}

void PalReadWriteTool::write_modifier_kv(uint32_t no_of_modifiers, struct modifier_kv *modifiers, android::Parcel& parcel) {
    uint32_t size = no_of_modifiers * sizeof(struct modifier_kv);
    write_param_payload(size, (uint8_t *)modifiers, parcel);
}

void PalReadWriteTool::read_modifier_kv(uint32_t &no_of_modifiers, struct modifier_kv **modifiers, const android::Parcel& parcel) {
    uint32_t size = 0;
    read_param_payload(size, (uint8_t **)modifiers, parcel);
    no_of_modifiers = size / sizeof(struct modifier_kv);
}

void PalReadWriteTool::write_pal_buffer_config(pal_buffer_config_t *buff_cfg, android::Parcel& parcel) {
    write_param_payload(buff_cfg?sizeof(pal_buffer_config_t):0, (uint8_t *)buff_cfg, parcel);
}

void PalReadWriteTool::read_pal_buffer_config(pal_buffer_config_t **buff_cfg, const android::Parcel& parcel) {
    uint32_t size = 0;
    read_param_payload(size, (uint8_t **)buff_cfg, parcel);
}

void PalReadWriteTool::write_pal_buffer(struct pal_buffer *pal_buff, android::Parcel& parcel) {
    uint32_t size = sizeof(struct pal_buffer);
    write_param_payload(pal_buff?size:0, (uint8_t *)pal_buff, parcel);
    if (!pal_buff) return;

    size = pal_buff->size;
    write_param_payload(pal_buff->buffer?size:0, pal_buff->buffer, parcel);

    size = sizeof(struct timespec);
    write_param_payload(pal_buff->ts?size:0, (uint8_t *)(pal_buff->ts), parcel);

#if 0
    size = pal_buff->metadata_size;
    write_param_payload(size, pal_buff->metadata, parcel);
#endif
}

void PalReadWriteTool::read_pal_buffer(pal_buffer **pal_buff, const android::Parcel& parcel) {
    uint32_t size = 0;
    read_param_payload(size, (uint8_t **)pal_buff, parcel);
    if (size <= 0) return;

    uint8_t *buffer = NULL;
    read_param_payload(size, &buffer, parcel);
    if (size) (*pal_buff)->buffer = buffer;
    else (*pal_buff)->buffer = NULL;

    uint8_t *ts = NULL;
    read_param_payload(size, &ts, parcel);
    if (size) (*pal_buff)->ts = (struct timespec *)ts;
    else (*pal_buff)->ts = NULL;

#if 0
    uint8_t *metadata = NULL;
    read_param_payload(size, &metadata, parcel);
    if (size) (*pal_buff)->metadata = metadata;
    else (*pal_buff)->metadata = NULL;
#endif
}

void PalReadWriteTool::write_pal_callback_buffer(struct pal_callback_buffer *callback_buff, android::Parcel& parcel) {
    uint32_t size = sizeof(struct pal_callback_buffer);
    write_param_payload(callback_buff?size:0, (uint8_t *)callback_buff, parcel);
    if (!callback_buff) return;

    size = callback_buff->size;
    write_param_payload(callback_buff->buffer?size:0, callback_buff->buffer, parcel);

    size = sizeof(struct timespec);
    write_param_payload(callback_buff->ts?size:0, (uint8_t *)callback_buff->ts, parcel);
}

void PalReadWriteTool::read_pal_callback_buffer(struct pal_callback_buffer **callback_buff, const android::Parcel& parcel) {
    uint32_t size = 0;
    read_param_payload(size, (uint8_t **)callback_buff, parcel);
    if (size < 0) return;

    uint8_t *buffer = NULL;
    read_param_payload(size, &buffer, parcel);
    if (size) (*callback_buff)->buffer = buffer;
    else (*callback_buff)->buffer = NULL;

    uint8_t *ts = NULL;
    read_param_payload(size, &ts, parcel);
    if (size) (*callback_buff)->ts = (struct timespec *)ts;
    else (*callback_buff)->ts = NULL;
}

void PalReadWriteTool::write_pal_volume_data(struct pal_volume_data *volume, android::Parcel& parcel) {
    uint32_t size = sizeof(struct pal_volume_data) + volume->no_of_volpair * sizeof(struct pal_channel_vol_kv);
    write_param_payload(size, (uint8_t *)volume, parcel);
}

void PalReadWriteTool::read_pal_volume_data(struct pal_volume_data **volume, const android::Parcel& parcel) {
    uint32_t size = 0;
    read_param_payload(size, (uint8_t **)volume, parcel);
}

void PalReadWriteTool::write_pal_session_time(struct pal_session_time *stime, android::Parcel* parcel) {
    uint32_t size = sizeof(struct pal_session_time);
    write_param_payload(stime?size:0, (uint8_t *)stime, parcel);
}

void PalReadWriteTool::read_pal_session_time(struct pal_session_time *stime, const android::Parcel& parcel) {
    uint32_t size = 0;
    read_param_payload(size, (uint8_t *)stime, parcel);
}

void PalReadWriteTool::write_pal_mmap_buffer(struct pal_mmap_buffer *info, android::Parcel* parcel) {
    uint32_t size = sizeof(struct pal_mmap_buffer);
    write_param_payload(info?size:0, (uint8_t *)info, parcel);
}

void PalReadWriteTool::read_pal_mmap_buffer(struct pal_mmap_buffer *info, const android::Parcel& parcel) {
    uint32_t size = 0;
    read_param_payload(size, (uint8_t **)info, parcel);
}

void PalReadWriteTool::write_pal_mmap_position(struct pal_mmap_position *position, android::Parcel* parcel) {
    uint32_t size = sizeof(struct pal_mmap_position);
    write_param_payload(position?size:0, (uint8_t *)position, parcel);
}

void PalReadWriteTool::read_pal_mmap_position(struct pal_mmap_position *position, const android::Parcel& parcel) {
    uint32_t size = 0;
    read_param_payload(size, (uint8_t *)position, parcel);
}
