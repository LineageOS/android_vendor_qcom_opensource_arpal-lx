/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <vector>
#include <map>
#include <binder/IInterface.h>
#include <binder/IBinder.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include "pal_callback.h"

class IPalService;

class IPALClient : public ::android::IInterface {
    public:
        DECLARE_META_INTERFACE(PALClient);
};

class DummyBnClient : public ::android::BnInterface<IPALClient> {
    public:
        DummyBnClient(){}
        ~DummyBnClient(){}
    private:
        android::status_t onTransact(uint32_t code,
                                    const android::Parcel& data,
                                    android::Parcel* reply, uint32_t flags);
};

class ServerDeathNotifier : public android::IBinder::DeathRecipient {
    public:
        ServerDeathNotifier();
        // DeathRecipient
        virtual void binderDied(const android::wp<android::IBinder>& who);
};

class ClientDeathNotifier : public android::IBinder::DeathRecipient {
    public:
        ClientDeathNotifier(IPalService* server);
        // DeathRecipient
        virtual void binderDied(const android::wp<android::IBinder>& who);

    private:
        IPalService* mPal;
};

typedef struct {
    uint64_t stream_handle;
    struct pal_stream_attributes stream_attr;
    pal_stream_callback cb_func;
    std::map<int, int> shared_fd_list;
 } pal_client_stream_handle;

typedef struct {
    android::sp<IPALClient> binder;
    android::sp<ICallback> cb_binder;
    pid_t pid;
    android::sp<ClientDeathNotifier> notifier;
    std::vector<pal_client_stream_handle *> pal_client_stream_handle_list;
} client_info;

void pal_register_client(android::sp<android::IBinder> binder, android::sp<android::IBinder> cb_binder, IPalService* server);
void pal_unregister_client(android::sp<android::IBinder> binder);
void pal_add_stream_handle(uint64_t stream_handle, struct pal_stream_attributes* attr, pal_stream_callback cb);
int pal_add_shared_fd(uint64_t stream_handle, int client_fd, int binder_fd);
void get_client_stream_handle(uint64_t stream_handle, client_info ** pal_client, pal_client_stream_handle** client_stream_handle);
void pal_remove_stream_handle(uint64_t stream_handle);
