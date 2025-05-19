/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "pal_callback.h"

#define     EVENT_CALLBACK          0x1000
#define     LOG_TAG                 "pal_callback"

class BpCallback: public ::android:: BpInterface<ICallback> {
public:
    BpCallback(const android::sp<android::IBinder>& impl) :  BpInterface<ICallback>(impl){}

    virtual int32_t event_cb(pal_stream_handle_t *stream_handle,
                             uint32_t event_id, uint32_t *event_data,
                             uint32_t event_data_size,
                             uint64_t cookie, pal_stream_callback cb_func,
                             struct pal_stream_attributes *attr = NULL) override {
        android::Parcel data, reply;
        data.writeUint64((uint64_t)stream_handle);
        data.writeUint64(cookie);
        data.write(&cb_func, sizeof(pal_stream_callback *));

        if (attr->type == PAL_STREAM_NON_TUNNEL && (event_id == PAL_STREAM_CBK_EVENT_READ_DONE || event_id == PAL_STREAM_CBK_EVENT_WRITE_READY)) {
            struct pal_callback_buffer *payload = (struct pal_callback_buffer *)event_data;
            PalReadWriteTool::write_pal_callback_buffer(payload, data);
        } else {
            event_id = 0x10086;
            data.writeUint32(event_data_size);
            if (event_data_size) {
                android::Parcel::WritableBlob blob;
                data.writeBlob(event_data_size, false, &blob);
                memset(blob.data(), 0x0, event_data_size);
                memcpy(blob.data(), event_data, event_data_size);
            }
        }

        remote()->transact(event_id, data, &reply);
        return reply.readInt32();
   }
};

IMPLEMENT_META_INTERFACE(Callback, "Callback");

android::status_t BnCallback::onTransact(uint32_t code,
                            const android::Parcel& data,
                            android::Parcel* reply, uint32_t flags) {

    pal_stream_handle_t *stream_handle = (pal_stream_handle_t *)data.readUint64();
    uint64_t cookie = data.readUint64();
    pal_stream_callback cb = NULL;
    data.read(&cb, sizeof(pal_stream_callback *));

    uint32_t event_data_size = 0;
    void *event_data = NULL;

    switch (code) {
        case PAL_STREAM_CBK_EVENT_READ_DONE:
        case PAL_STREAM_CBK_EVENT_WRITE_READY: {
            PalReadWriteTool::read_pal_callback_buffer((struct pal_callback_buffer **)(&event_data), data);
            event_data_size = sizeof(struct pal_callback_buffer);
            break;
        }
        default: {
            event_data_size = data.readUint32();
            event_data = new uint8_t[event_data_size];
            if (event_data) {
                android::Parcel::ReadableBlob blob;
                data.readBlob(event_data_size, &blob);
                memcpy(event_data, blob.data(), event_data_size);
            }
            break;
        }
    }
    int rc = event_cb(stream_handle, code, (uint32_t *)event_data, event_data_size, cookie, cb);
    if (event_data) delete [] event_data;
    return rc;
}

int32_t BnCallback::event_cb(pal_stream_handle_t *stream_handle,
                             uint32_t event_id, uint32_t *event_data,
                             uint32_t event_data_size,
                             uint64_t cookie, pal_stream_callback cb_func,
                             struct pal_stream_attributes *attr) {
    return cb_func(stream_handle, event_id, event_data, event_data_size, cookie);
}