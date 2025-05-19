/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <cutils/list.h>
#include <PalDefs.h>
#include "pal_ipc_data_rw.h"

class ICallback : public ::android::IInterface {
    public:
        DECLARE_META_INTERFACE(Callback);
        virtual int32_t event_cb(pal_stream_handle_t *stream_handle,
                                 uint32_t event_id, uint32_t *event_data,
                                 uint32_t event_data_size,
                                 uint64_t cookie, pal_stream_callback cb_func,
                                 struct pal_stream_attributes *attr = NULL) = 0;
};


class BnCallback : public ::android::BnInterface<ICallback> {
public:
    BnCallback(){};
    ~BnCallback(){};
private:
    android::status_t onTransact(uint32_t code,
                                    const android::Parcel& data,
                                    android::Parcel* reply, uint32_t flags);
    int32_t event_cb(pal_stream_handle_t *stream_handle,
                     uint32_t event_id, uint32_t *event_data,
                     uint32_t event_data_size,
                     uint64_t cookie, pal_stream_callback cb_func,
                     struct pal_stream_attributes *attr = NULL) override;
};
