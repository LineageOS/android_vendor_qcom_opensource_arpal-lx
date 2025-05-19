/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <PalDefs.h>
#include <PalCommon.h>
#include "pal_client_manager.h"
#include "ipc_interface.h"

#define LOG_TAG         "pal_client_manager"

std::vector<client_info *> g_client_list;
pthread_mutex_t g_client_list_lock;
bool g_client_list_init = false;

class BpPALClient: public ::android:: BpInterface<IPALClient> {
public:
    BpPALClient(const android::sp<android::IBinder>& impl) : BpInterface<IPALClient>(impl) {}
};

IMPLEMENT_META_INTERFACE(PALClient, "PALClient");

android::status_t DummyBnClient::onTransact(uint32_t code,
                                   const android::Parcel& data,
                                   android::Parcel* reply, uint32_t flags) {
    int status = 0;
    data.checkInterface(this);
    switch(code) {
        default:
            return BBinder::onTransact(code, data, reply, flags);
            break;
    }
    return status;
}

void ClientDeathNotifier::binderDied(const android::wp<android::IBinder>& who) {
    client_info *client = NULL;
    pal_client_stream_handle *handle = NULL;

    pthread_mutex_lock(&g_client_list_lock);
    for (int i=0; i < g_client_list.size(); i++) {
        client = g_client_list[i];
        if (client && android::IInterface::asBinder(client->binder).get() == who.unsafe_get()) {
            for (int j=client->pal_client_stream_handle_list.size() - 1; j >= 0 ; j--) {
                handle = client->pal_client_stream_handle_list[j];
                if (handle && handle->stream_handle) {
                    mPal->ipc_pal_stream_close((pal_stream_handle_t *)handle->stream_handle);
                    client->pal_client_stream_handle_list.erase(client->pal_client_stream_handle_list.begin() + j);
                }

                if (handle) delete handle;
            }
            delete client;
            break;
        }
    }
    pthread_mutex_unlock(&g_client_list_lock);
}

ClientDeathNotifier::ClientDeathNotifier(IPalService* server) : mPal(server) {
    android::sp<android::ProcessState> proc(android::ProcessState::self());
    proc->startThreadPool();
}

static client_info *get_client_info_from_list(pid_t pid) {
    client_info *client = NULL;

    pthread_mutex_lock(&g_client_list_lock);
    for (int i=0; i < g_client_list.size(); i++) {
        client = g_client_list[i];
        if (client->pid == pid) {
            break;
        }
    }
    pthread_mutex_unlock(&g_client_list_lock);
    return client;
}

void get_client_stream_handle(uint64_t stream_handle, client_info ** pal_client, pal_client_stream_handle** client_stream_handle) {
    pal_client_stream_handle *handle = NULL;
    client_info *client = NULL;

    pthread_mutex_lock(&g_client_list_lock);
    for (int i=0; i < g_client_list.size(); i++) {
        client = g_client_list[i];
        for (int j=0; j < client->pal_client_stream_handle_list.size(); j++) {
            handle = client->pal_client_stream_handle_list[j];
            if (handle && handle->stream_handle == stream_handle) {
                if (pal_client) *pal_client = client;
                if (client_stream_handle) *client_stream_handle = handle;
                break;
            }
        }
    }
    pthread_mutex_unlock(&g_client_list_lock);
}

void pal_register_client(android::sp<android::IBinder> binder, android::sp<android::IBinder> cb_binder, IPalService* server) {
    client_info *client = NULL;
    pid_t pid = android::IPCThreadState::self()->getCallingPid();
    android::sp<IPALClient> client_binder = android::interface_cast<IPALClient>(binder);
    android::sp<ClientDeathNotifier> Client_death_notifier = new ClientDeathNotifier(server);
    android::IInterface::asBinder(client_binder)->linkToDeath(Client_death_notifier);

    if (g_client_list_init == false) {
        pthread_mutex_init(&g_client_list_lock, (const pthread_mutexattr_t *) NULL);
        g_client_list_init = true;
    }

    client = new client_info;
    pthread_mutex_lock(&g_client_list_lock);
    client->binder = client_binder;
    client->cb_binder = android::interface_cast<ICallback>(cb_binder);
    client->pid = pid;
    client->notifier = Client_death_notifier;
    g_client_list.push_back(client);
    pthread_mutex_unlock(&g_client_list_lock);
}

void pal_add_stream_handle(uint64_t stream_handle, struct pal_stream_attributes* attr, pal_stream_callback cb) {
    client_info *client_handle = NULL;

    client_handle = get_client_info_from_list(android::IPCThreadState::self()->getCallingPid());
    if (client_handle == NULL) {
        return;
    }

    pthread_mutex_lock(&g_client_list_lock);

    pal_client_stream_handle *hndl = new pal_client_stream_handle;

    hndl->stream_handle = stream_handle;
    hndl->cb_func = cb;
    memcpy(&(hndl->stream_attr), attr, sizeof(struct pal_stream_attributes));
    client_handle->pal_client_stream_handle_list.push_back(hndl);

    pthread_mutex_unlock(&g_client_list_lock);
}

int pal_add_shared_fd(uint64_t stream_handle, int client_fd, int binder_fd) {
    pal_client_stream_handle *handle = NULL;
    get_client_stream_handle(stream_handle, NULL, &handle);
    if (!handle) {
        PAL_ERR(LOG_TAG, "invalid stream_handle: 0x%08X", stream_handle);
        return 0;
    }

    if (!handle->shared_fd_list.count(client_fd)) {
        handle->shared_fd_list[client_fd] = dup(binder_fd);
    }

    return handle->shared_fd_list[client_fd];
}

void pal_remove_stream_handle(uint64_t stream_handle) {
    client_info *client_handle = NULL;
    client_handle = get_client_info_from_list(android::IPCThreadState::self()->getCallingPid());

    if (client_handle == NULL) {
        return;
    }

    pal_client_stream_handle * stream_hndl = NULL;

    pthread_mutex_lock(&g_client_list_lock);
    for (int i=0;i<client_handle->pal_client_stream_handle_list.size();i++) {
        stream_hndl = client_handle->pal_client_stream_handle_list[i];
        if (stream_hndl->stream_handle == stream_handle) {
            std::map<int, int>::iterator itr;
            for (itr=stream_hndl->shared_fd_list.begin(); itr != stream_hndl->shared_fd_list.end(); itr++) {
                if (itr->second > 0) {
                    close(itr->second);
                }
            }
            stream_hndl->shared_fd_list.clear();
            client_handle->pal_client_stream_handle_list.erase(client_handle->pal_client_stream_handle_list.begin() + i);
            delete stream_hndl;
            break;
        }
    }
    pthread_mutex_unlock(&g_client_list_lock);
}

void pal_unregister_client(android::sp<android::IBinder> binder) {
    android::sp<IPALClient> client_binder = android::interface_cast<IPALClient>(binder);

    pthread_mutex_lock(&g_client_list_lock);
    for (int i=0; i < g_client_list.size(); i++) {
        client_info *client = g_client_list[i];
        if (client->pid == android::IPCThreadState::self()->getCallingPid()) {
            if (client->notifier != NULL) {
                android::IInterface::asBinder(client_binder)->unlinkToDeath(client->notifier);
                client->notifier.clear();
            }

            while (!client->pal_client_stream_handle_list.empty()) {
                pal_client_stream_handle *stream_hndl = client->pal_client_stream_handle_list[0];
                client->pal_client_stream_handle_list.erase(client->pal_client_stream_handle_list.begin());
                delete stream_hndl;
            }

            g_client_list.erase(g_client_list.begin() + i);
            delete client;
            break;
        }
    }
    pthread_mutex_unlock(&g_client_list_lock);
}
