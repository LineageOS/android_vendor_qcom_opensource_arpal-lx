/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <PalApi.h>
#include <PalCommon.h>
#include <memory>
#include <mutex>
#include <map>
#include <cmath>
#include <pal/MetadataParser.h>
#include "pal_server_wrapper.h"
#include "pal_client_manager.h"

#define LOG_TAG         "pal_server_wrapper"
#define MAX_METADATA_SIZE        2048

// map<FD, map<offset, input_frame_id>>
std::map<int, std::map<uint32_t, uint64_t>> gInputsPendingAck;
std::mutex gInputsPendingAckLock;

uint8_t METADATA[MAX_METADATA_SIZE];
uint32_t metadata_size_used = 0;

enum {
    REG_CLIENT = 0x1001,
    UNREG_CLIENT,
    GET_VERSION,
    INIT,
    DEINIT,
    STREAM_OPEN,
    STREAM_CLOSE,
    STREAM_START,
    STREAM_STOP,
    STREAM_PAUSE,
    STREAM_RESUME,
    STREAM_FLUSH,
    STREAM_DRAIN,
    STREAM_SUSPEND,
    STREAM_GET_BUFFER_SIZE,
    STREAM_GET_TAGS_WITH_MODULE_INFO,
    STREAM_SET_BUFFER_SIZE,
    STREAM_READ,
    STREAM_WRITE,
    STREAM_GET_DEVICE,
    STREAM_SET_DEVICE,
    STREAM_GET_PARAM,
    STREAM_SET_PARAM,
    STREAM_GET_VOLUME,
    STREAM_SET_VOLUME,
    STREAM_GET_MUTE,
    STREAM_SET_MUTE,
    GET_MIC_MUTE,
    SET_MIC_MUTE,
    GET_TIMESTAMP,
    ADD_REMOVE_EFFECT,
    GET_PARAM,
    SET_PARAM,
    STREAM_CREATE_MMAP_BUFFER,
    STREAM_GET_MMAP_POSITION,
    REGISTER_GLOBAL_CALLBACK,
    GEF_RW_PARAM,
    GEF_RW_PARAM_ACDB,
};

#if 0
#include <sys/mman.h>
#include <error.h>

static void print_fd_buffer(uint32_t frame_index, int fd, uint32_t size, uint32_t alloc_size, struct timespec *ts, const char * msg) {
    printf("**************************************************\n");
    printf("===================%s(%2d)========================\n", msg, frame_index);
    if (ts) printf("size: %d, alloc_size: %d, fd: %d, sec: %d nsec: %d\n", size, alloc_size, fd, ts->tv_sec, ts->tv_nsec);
    else printf("size: %d, alloc_size: %d, fd: %d\n", size, alloc_size, fd);
    int temp_fd = dup(fd);
    void * addr = mmap(0, size, PROT_READ, MAP_SHARED, temp_fd, 0);
    if (addr == MAP_FAILED) {
        PLOGD("MAP_FAILED, error: %s", strerror(errno));
    } else {
        uint32_t *input_data = (uint32_t *)addr;
        uint32_t input_size = size / sizeof(uint32_t);

        for (int i=0;i<input_size;i++) {
            if ((i % 4) == 0) printf("0x%08X: ", i * 4);
            printf("%08X ", *(input_data + i));
            if ((i % 4) == 3) printf("\n");
        }

        if (input_size % 4) printf("\n");
    }

    if (temp_fd > 0) close(temp_fd);
    printf("===================%s(%2d)========================\n", msg, frame_index);
    printf("**************************************************\n");
}
#endif

void addToPendingInputs(int fd, uint32_t offset, uint32_t ip_frame_id) {
    std::lock_guard<std::mutex> pendingAcksLock(gInputsPendingAckLock);
    auto itFd = gInputsPendingAck.find(fd);
    if (itFd != gInputsPendingAck.end()) {
        gInputsPendingAck[fd][offset] = ip_frame_id;
    } else {
        //create new map<offset,input_buffer_index> and add to FD map
        gInputsPendingAck.insert(std::make_pair(fd, std::map<uint32_t, uint64_t>()));
        gInputsPendingAck[fd].insert(std::make_pair(offset, ip_frame_id));
    }
}

int getInputBufferIndex(int fd, uint32_t offset, uint64_t &buf_index) {
    int status = 0;

    std::lock_guard<std::mutex> pendingAcksLock(gInputsPendingAckLock);
    std::map<int, std::map<uint32_t, uint64_t>>::iterator itFd = gInputsPendingAck.find(fd);
    if (itFd != gInputsPendingAck.end()) {
        std::map<uint32_t, uint64_t> offsetToFrameIdxMap = itFd->second;
        auto itOffsetFrameIdxPair = offsetToFrameIdxMap.find(offset);
        if (itOffsetFrameIdxPair != offsetToFrameIdxMap.end()){
            buf_index = itOffsetFrameIdxPair->second;
        } else {
            status = -EINVAL;
        }
        gInputsPendingAck.erase(itFd);
    }
    return status;
}

static int32_t pal_event_callback(pal_stream_handle_t *stream_handle,
                           uint32_t event_id, uint32_t *event_data,
                           uint32_t event_data_size,
                           uint64_t cookie) {
    pal_client_stream_handle *handle = NULL;
    client_info *client = NULL;
    get_client_stream_handle((uint64_t)stream_handle, &client, &handle);
    if (!client || !(client->cb_binder.get())){
        return -1;
    }

    int32_t rc = -1;

    if (handle->stream_attr.type == PAL_STREAM_NON_TUNNEL && (event_id == PAL_STREAM_CBK_EVENT_READ_DONE || event_id == PAL_STREAM_CBK_EVENT_WRITE_READY)) {
        struct pal_event_read_write_done_payload *rw_done_payload = (struct pal_event_read_write_done_payload *)event_data;
        struct pal_callback_buffer callback_buff = {0};
        memset(&callback_buff, 0, sizeof(struct pal_callback_buffer));

        callback_buff.status = rw_done_payload->status?rw_done_payload->status:rw_done_payload->md_status;
        if (!callback_buff.status) {
            if (event_id == PAL_STREAM_CBK_EVENT_READ_DONE) {
                auto cb_buf_info = std::make_unique<pal_clbk_buffer_info>();
                auto metadataParser = std::make_unique<MetadataParser>();

                callback_buff.status = metadataParser->parseMetadata(
                            rw_done_payload->buff.metadata,
                            rw_done_payload->buff.metadata_size,
                            cb_buf_info.get());
                callback_buff.cb_buf_info.frame_index = cb_buf_info->frame_index;
                callback_buff.cb_buf_info.sample_rate = cb_buf_info->sample_rate;
                callback_buff.cb_buf_info.channel_count = cb_buf_info->channel_count;
                callback_buff.cb_buf_info.bit_width = cb_buf_info->bit_width;
            } else {
                callback_buff.status = getInputBufferIndex(
                            rw_done_payload->buff.alloc_info.alloc_handle, 0,
                            callback_buff.cb_buf_info.frame_index);
            }
        }
        callback_buff.size = rw_done_payload->buff.size;

        callback_buff.ts = NULL;
        if (rw_done_payload->buff.ts) {
            struct timespec ts = {0};
            ts.tv_sec = rw_done_payload->buff.ts->tv_sec;
            ts.tv_nsec = rw_done_payload->buff.ts->tv_nsec;
            callback_buff.ts = &ts;
        }

        callback_buff.buffer = NULL;
        if ((rw_done_payload->buff.buffer) &&
             !(handle->stream_attr.flags & PAL_STREAM_FLAG_EXTERN_MEM)) {
            callback_buff.buffer = new uint8_t[callback_buff.size];
            memcpy(callback_buff.buffer, rw_done_payload->buff.buffer, callback_buff.size);
        }
        int dup_fd = rw_done_payload->buff.alloc_info.alloc_handle;

#if 0
        if (event_id == PAL_STREAM_CBK_EVENT_READ_DONE)
            print_fd_buffer(callback_buff.cb_buf_info.frame_index, dup_fd,
                            rw_done_payload->buff.size,
                            rw_done_payload->buff.alloc_info.alloc_size,
                            callback_buff.ts, "cbk");
#endif
        // if (dup_fd > 0) close(dup_fd);

        rc = client->cb_binder->event_cb(stream_handle, event_id,
                                         (uint32_t *)(&callback_buff),
                                         sizeof(struct pal_callback_buffer), cookie,
                                         handle->cb_func, &(handle->stream_attr));
        if (callback_buff.buffer) delete [] callback_buff.buffer;
    } else {
        rc = client->cb_binder->event_cb(stream_handle, event_id, event_data, event_data_size,
                                         cookie, handle->cb_func, &(handle->stream_attr));
    }

    return rc;
}

PalService::PalService():
            pal_initialized(false) {
    // ipc_pal_init();
}


PalService::~PalService() {
    // ipc_pal_deinit();
}

int32_t PalService::ipc_pal_init() {
    return pal_init();
}

std::string PalService::ipc_pal_get_version() {
    return pal_get_version();
}


void PalService::ipc_pal_deinit() {
    pal_deinit();
}

int32_t PalService::ipc_pal_stream_open(struct pal_stream_attributes *attributes,
                                        uint32_t no_of_devices, struct pal_device *devices,
                                        uint32_t no_of_modifiers, struct modifier_kv *modifiers,
                                        pal_stream_callback cb, uint64_t cookie,
                                        pal_stream_handle_t **stream_handle) {
    return pal_stream_open(attributes, no_of_devices, devices, no_of_modifiers, modifiers, cb, cookie, stream_handle);
}

int32_t PalService::ipc_pal_stream_close(pal_stream_handle_t *stream_handle) {
    return pal_stream_close(stream_handle);
}

int32_t PalService::ipc_pal_stream_start(pal_stream_handle_t *stream_handle) {
    return pal_stream_start(stream_handle);
}

int32_t PalService::ipc_pal_stream_stop(pal_stream_handle_t *stream_handle) {
    return pal_stream_stop(stream_handle);
}

int32_t PalService::ipc_pal_stream_pause(pal_stream_handle_t *stream_handle) {
    return pal_stream_pause(stream_handle);
}

int32_t PalService::ipc_pal_stream_resume(pal_stream_handle_t *stream_handle) {
    return pal_stream_resume(stream_handle);
}

int32_t PalService::ipc_pal_stream_flush(pal_stream_handle_t *stream_handle) {
    return pal_stream_flush(stream_handle);
}

int32_t PalService::ipc_pal_stream_drain(pal_stream_handle_t *stream_handle, pal_drain_type_t type) {
    return pal_stream_drain(stream_handle, type);
}

int32_t PalService::ipc_pal_stream_suspend(pal_stream_handle_t *stream_handle) {
    return pal_stream_suspend(stream_handle);
}

int32_t PalService::ipc_pal_stream_get_buffer_size(pal_stream_handle_t *stream_handle,
                                                size_t *in_buffer, size_t *out_buffer) {
    // This api is not yet implemented in PAL
    return -ENOSYS;
}

int32_t PalService::ipc_pal_stream_get_tags_with_module_info(pal_stream_handle_t *stream_handle,
                                                            size_t *size ,uint8_t *payload) {
    return pal_stream_get_tags_with_module_info(stream_handle, size, payload);
}

int32_t PalService::ipc_pal_stream_set_buffer_size(pal_stream_handle_t *stream_handle,
                                                   pal_buffer_config_t *in_buff_cfg,
                                                   pal_buffer_config_t *out_buff_cfg) {
    return pal_stream_set_buffer_size(stream_handle, in_buff_cfg, out_buff_cfg);
}

ssize_t PalService::ipc_pal_stream_read(pal_stream_handle_t *stream_handle, struct pal_buffer *buf) {
    // if (buf) print_fd_buffer((uint32_t)(buf->frame_index), buf->alloc_info.alloc_handle, buf->size, buf->alloc_info.alloc_size, buf->ts, "out");
    return pal_stream_read(stream_handle, buf);
}

ssize_t PalService::ipc_pal_stream_write(pal_stream_handle_t *stream_handle, struct pal_buffer *buf) {
    // if (buf) print_fd_buffer((uint32_t)(buf->frame_index), buf->alloc_info.alloc_handle, buf->size, buf->alloc_info.alloc_size, buf->ts, "in ");
    return pal_stream_write(stream_handle, buf);
}

int32_t PalService::ipc_pal_stream_get_device(pal_stream_handle_t *stream_handle,
                                              uint32_t no_of_devices, struct pal_device *devices) {
    // This api is not yet implemented in PAL
    return -ENOSYS;
}

int32_t PalService::ipc_pal_stream_set_device(pal_stream_handle_t *stream_handle,
                                              uint32_t no_of_devices, struct pal_device *devices) {
    return pal_stream_set_device(stream_handle, no_of_devices, devices);
}

int32_t PalService::ipc_pal_stream_get_param(pal_stream_handle_t *stream_handle,
                                             uint32_t param_id, pal_param_payload **param_payload) {
    return pal_stream_get_param(stream_handle, param_id, param_payload);
}

int32_t PalService::ipc_pal_stream_set_param(pal_stream_handle_t *stream_handle,
                                             uint32_t param_id, pal_param_payload *param_payload) {
    return pal_stream_set_param(stream_handle, param_id, param_payload);
}

int32_t PalService::ipc_pal_stream_get_volume(pal_stream_handle_t *stream_handle,
                                              struct pal_volume_data *volume) {
    // This api is not yet implemented in PAL
    return -ENOSYS;
}

int32_t PalService::ipc_pal_stream_set_volume(pal_stream_handle_t *stream_handle,
                                              struct pal_volume_data *volume) {
    return pal_stream_set_volume(stream_handle, volume);
}

int32_t PalService::ipc_pal_stream_get_mute(pal_stream_handle_t *stream_handle, bool *state) {
    // This api is not yet implemented in PAL
    return -ENOSYS;
}

int32_t PalService::ipc_pal_stream_set_mute(pal_stream_handle_t *stream_handle, bool state) {
    return pal_stream_set_mute(stream_handle, state);
}

int32_t PalService::ipc_pal_get_mic_mute(bool *state) {
    // This api is not yet implemented in PAL
    return -ENOSYS;
}

int32_t PalService::ipc_pal_set_mic_mute(bool state) {
    // This api is not yet implemented in PAL
    return -ENOSYS;
}

int32_t PalService::ipc_pal_get_timestamp(pal_stream_handle_t *stream_handle, struct pal_session_time *stime) {
    return pal_get_timestamp(stream_handle, stime);
}

int32_t PalService::ipc_pal_add_remove_effect(pal_stream_handle_t *stream_handle, pal_audio_effect_t effect, bool enable) {
    return pal_add_remove_effect(stream_handle, effect, enable);
}

int32_t PalService::ipc_pal_set_param(uint32_t param_id, void *param_payload, size_t payload_size) {
    return pal_set_param(param_id, param_payload, payload_size);
}

int32_t PalService::ipc_pal_get_param(uint32_t param_id, void **param_payload, size_t *payload_size, void *query) {
    return pal_get_param(param_id, param_payload, payload_size, query);
}

int32_t PalService::ipc_pal_stream_create_mmap_buffer(pal_stream_handle_t *stream_handle,
                                                int32_t min_size_frames,
                                                struct pal_mmap_buffer *info) {
    return pal_stream_create_mmap_buffer(stream_handle, min_size_frames, info);
}

int32_t PalService::ipc_pal_stream_get_mmap_position(pal_stream_handle_t *stream_handle,
                                        struct pal_mmap_position *position) {
    return pal_stream_get_mmap_position(stream_handle, position);
}

int32_t PalService::ipc_pal_register_global_callback(pal_global_callback cb, uint64_t cookie) {
    return pal_register_global_callback(cb, cookie);
}

int32_t PalService::ipc_pal_gef_rw_param(uint32_t param_id, void *param_payload,
                            size_t payload_size, pal_device_id_t pal_device_id,
                            pal_stream_type_t pal_stream_type, unsigned int dir) {
    return pal_gef_rw_param(param_id, param_payload, payload_size, pal_device_id, pal_stream_type, dir);
}

int32_t PalService::ipc_pal_gef_rw_param_acdb(uint32_t param_id, void *param_payload,
                                size_t payload_size, pal_device_id_t pal_device_id,
                                pal_stream_type_t pal_stream_type, uint32_t sample_rate,
                                uint32_t instance_id, uint32_t dir, bool is_play) {
    return pal_gef_rw_param_acdb(param_id, param_payload, payload_size, pal_device_id, pal_stream_type, sample_rate, instance_id, dir, is_play);
}

class BpPalService : public ::android::BpInterface<IPalService> {
    private:
        android::sp<IPALClient> clt_binder;
    public:
        BpPalService(const android::sp<android::IBinder>& impl) : BpInterface<IPalService>(impl) {
            android::Parcel data, reply;

            android::sp<android::IBinder> binder = new DummyBnClient();
            // android::ProcessState::self()->startThreadPool();
            clt_binder = android::interface_cast<IPALClient>(binder);
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeStrongBinder(android::IInterface::asBinder(clt_binder));

            android::sp<android::IBinder> cb_binder = new BnCallback();
            android::ProcessState::self()->startThreadPool();
            android::sp<ICallback> clbk = android::interface_cast<ICallback>(cb_binder);
            data.writeStrongBinder(android::IInterface::asBinder(clbk));

            remote()->transact(REG_CLIENT, data, &reply);
        }

        ~BpPalService() {
        }

        virtual int32_t ipc_pal_init() override {
            // finished by server
            return 0;
        }

        virtual std::string ipc_pal_get_version() override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            remote()->transact(GET_VERSION, data, &reply);

            char version[32];
            uint32_t size = 0;
            PalReadWriteTool::read_param_payload(size, (uint8_t *)version, reply);
            return version;
        }

        virtual void ipc_pal_deinit() override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeStrongBinder(android::IInterface::asBinder(clt_binder));
            // deinit finished by server, client call to this API, unreigster Bp client
            remote()->transact(UNREG_CLIENT, data, &reply);
        }

        virtual int32_t ipc_pal_stream_open(struct pal_stream_attributes *attributes,
                                            uint32_t no_of_devices, struct pal_device *devices,
                                            uint32_t no_of_modifiers, struct modifier_kv *modifiers,
                                            pal_stream_callback cb, uint64_t cookie,
                                            pal_stream_handle_t **stream_handle) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());

            PalReadWriteTool::write_pal_stream_attributes(attributes, data);
            PalReadWriteTool::write_pal_devices(no_of_devices, devices, data);
            PalReadWriteTool::write_modifier_kv(no_of_modifiers, modifiers, data);

            data.writeUint64(cookie);
            data.write(&cb, sizeof(pal_stream_callback *));

            remote()->transact(STREAM_OPEN, data, &reply);
            int rc = reply.readInt32();
            if (rc) *stream_handle = 0;
            else *stream_handle = (pal_stream_handle_t *)reply.readUint64();

            return rc;
        }

        virtual int32_t ipc_pal_stream_close(pal_stream_handle_t *stream_handle) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            remote()->transact(STREAM_CLOSE, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_stream_start(pal_stream_handle_t *stream_handle) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            remote()->transact(STREAM_START, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_stream_stop(pal_stream_handle_t *stream_handle) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            remote()->transact(STREAM_STOP, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_stream_pause(pal_stream_handle_t *stream_handle) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            remote()->transact(STREAM_PAUSE, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_stream_resume(pal_stream_handle_t *stream_handle) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            remote()->transact(STREAM_RESUME, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_stream_flush(pal_stream_handle_t *stream_handle) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            remote()->transact(STREAM_FLUSH, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_stream_drain(pal_stream_handle_t *stream_handle, pal_drain_type_t type) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            data.writeUint32((uint32_t)type);
            remote()->transact(STREAM_DRAIN, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_stream_suspend(pal_stream_handle_t *stream_handle) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            remote()->transact(STREAM_SUSPEND, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_stream_get_buffer_size(pal_stream_handle_t *stream_handle,
                                                       size_t *in_buffer, size_t *out_buffer) override {
#if 0   // This api is not yet implemented in PAL
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            remote()->transact(STREAM_GET_BUFFER_SIZE, data, &reply);
            return reply.readInt32();
#endif
            return -1;
        }

        virtual int32_t ipc_pal_stream_get_tags_with_module_info(pal_stream_handle_t *stream_handle,
                                                                 size_t *size ,uint8_t *payload) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            data.writeUint32((uint32_t)(*size));
            remote()->transact(STREAM_GET_TAGS_WITH_MODULE_INFO, data, &reply);

            int rc = reply.readInt32();
            if (rc) return rc;

            uint32_t temp_size = 0;
            PalReadWriteTool::read_param_payload(temp_size, payload, reply);
            *size = temp_size;

            return rc;
        }

        virtual int32_t ipc_pal_stream_set_buffer_size(pal_stream_handle_t *stream_handle,
                                                        pal_buffer_config_t *in_buff_cfg,
                                                        pal_buffer_config_t *out_buff_cfg) override {
            android::Parcel data, reply;

            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            PalReadWriteTool::write_pal_buffer_config(in_buff_cfg, data);
            PalReadWriteTool::write_pal_buffer_config(out_buff_cfg, data);

            remote()->transact(STREAM_SET_BUFFER_SIZE, data, &reply);
            return reply.readInt32();
        }

        virtual ssize_t ipc_pal_stream_read(pal_stream_handle_t *stream_handle, struct pal_buffer *buf) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);

            // print_pal_buffer(buf);

            PalReadWriteTool::write_pal_buffer(buf, data);

            if (buf->alloc_info.alloc_size > 0 && buf->alloc_info.alloc_handle > 0) {
                data.writeFileDescriptor(buf->alloc_info.alloc_handle);
            }

            remote()->transact(STREAM_READ, data, &reply);

            uint32_t size = 0;
            if (buf->buffer) {
                PalReadWriteTool::read_param_payload(size, buf->buffer, reply);
            } else {
                size = reply.readUint32();
            }

            return size;
        }

        virtual ssize_t ipc_pal_stream_write(pal_stream_handle_t *stream_handle, struct pal_buffer *buf) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);

            PalReadWriteTool::write_pal_buffer(buf, data);

            if (buf->alloc_info.alloc_size > 0 && buf->alloc_info.alloc_handle > 0) {
                data.writeFileDescriptor(buf->alloc_info.alloc_handle);
            }

            remote()->transact(STREAM_WRITE, data, &reply);
            ssize_t write = reply.readInt32();
            return write;
        }

        virtual int32_t ipc_pal_stream_get_device(pal_stream_handle_t *stream_handle,
                                                  uint32_t no_of_devices, struct pal_device *devices) override {
#if 0       // This api is not yet implemented in PAL
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            remote()->transact(STREAM_GET_DEVICE, data, &reply);
            return reply.readInt32();
#endif
            return -1;
        }

        virtual int32_t ipc_pal_stream_set_device(pal_stream_handle_t *stream_handle,
                                                  uint32_t no_of_devices, struct pal_device *devices) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);

            PalReadWriteTool::write_pal_devices(no_of_devices, devices, data);

            remote()->transact(STREAM_SET_DEVICE, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_stream_get_param(pal_stream_handle_t *stream_handle,
                                                 uint32_t param_id, pal_param_payload **param_payload) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            data.writeUint32(param_id);
            remote()->transact(STREAM_GET_PARAM, data, &reply);

            int32_t rc = reply.readUint32();
            if (rc) return rc;

            uint32_t temp_size = 0;
            PalReadWriteTool::read_param_payload(temp_size, (uint8_t **)param_payload, reply);
            return rc;
        }

        virtual int32_t ipc_pal_stream_set_param(pal_stream_handle_t *stream_handle,
                                                 uint32_t param_id, pal_param_payload *param_payload) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            data.writeUint32(param_id);
            uint32_t size = param_payload?param_payload->payload_size + sizeof(pal_param_payload):0;
            PalReadWriteTool::write_param_payload(size, (uint8_t *)param_payload, data);
            remote()->transact(STREAM_SET_PARAM, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_stream_get_volume(pal_stream_handle_t *stream_handle,
                                                 struct pal_volume_data *volume) override {
#if 0       // This api is not yet implemented in PAL
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            remote()->transact(STREAM_GET_VOLUME, data, &reply);
            return reply.readInt32();
#endif
            return -1;
        }

        virtual int32_t ipc_pal_stream_set_volume(pal_stream_handle_t *stream_handle,
                                                  struct pal_volume_data *volume) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);

            PalReadWriteTool::write_pal_volume_data(volume, data);
            remote()->transact(STREAM_SET_VOLUME, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_stream_get_mute(pal_stream_handle_t *stream_handle, bool *state) override {
#if 0       // This api is not yet implemented in PAL
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            remote()->transact(STREAM_GET_MUTE, data, &reply);
            return reply.readInt32();
#endif
            return -1;
        }

        virtual int32_t ipc_pal_stream_set_mute(pal_stream_handle_t *stream_handle, bool state) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            data.write(&state, sizeof(bool));
            remote()->transact(STREAM_SET_MUTE, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_get_mic_mute(bool *state) override {
#if 0       // This api is not yet implemented in PAL
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            remote()->transact(GET_MIC_MUTE, data, &reply);
            return reply.readInt32();
#endif
            return -1;
        }

        virtual int32_t ipc_pal_set_mic_mute(bool state) override {
#if 0       // This api is not yet implemented in PAL
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            remote()->transact(SET_MIC_MUTE, data, &reply);
            return reply.readInt32();
#endif
            return -1;
        }

        virtual int32_t ipc_pal_get_timestamp(pal_stream_handle_t *stream_handle, struct pal_session_time *stime) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);

            remote()->transact(GET_TIMESTAMP, data, &reply);
            int32_t rc = reply.readInt32();
            if (rc || !stime) return rc;

            PalReadWriteTool::read_pal_session_time(stime, reply);
            return rc;
        }

        virtual int32_t ipc_pal_add_remove_effect(pal_stream_handle_t *stream_handle,
                                                  pal_audio_effect_t effect, bool enable) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            data.writeUint32((uint32_t)effect);
            data.write(&enable, sizeof(bool));
            remote()->transact(ADD_REMOVE_EFFECT, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_set_param(uint32_t param_id, void *param_payload, size_t payload_size) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint32(param_id);
            PalReadWriteTool::write_param_payload(payload_size, (uint8_t *)param_payload, data);

            remote()->transact(SET_PARAM, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_get_param(uint32_t param_id, void **param_payload,
                                size_t *payload_size, void *query) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint32(param_id);
            remote()->transact(GET_PARAM, data, &reply);
            int rc = reply.readInt32();
            if (rc || !param_payload || !(*param_payload)) return rc;

            uint32_t size = 0;
            PalReadWriteTool::read_param_payload(size, (uint8_t *)(*param_payload), reply);
            *payload_size = size;

            return rc;
        }

        virtual int32_t ipc_pal_stream_create_mmap_buffer(pal_stream_handle_t *stream_handle,
                                                int32_t min_size_frames,
                                                struct pal_mmap_buffer *info) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            data.writeInt32(min_size_frames);

            remote()->transact(STREAM_CREATE_MMAP_BUFFER, data, &reply);
            int32_t rc = reply.readInt32();
            if (!rc) {
                PalReadWriteTool::read_pal_mmap_buffer(info, reply);
            }
            return rc;
        }

        virtual int32_t ipc_pal_stream_get_mmap_position(pal_stream_handle_t *stream_handle,
                                                struct pal_mmap_position *position) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint64((uint64_t)stream_handle);
            remote()->transact(STREAM_GET_MMAP_POSITION, data, &reply);
            int32_t rc = reply.readInt32();
            if (rc || !position) return rc;
            PalReadWriteTool::read_pal_mmap_position(position, reply);
            return rc;
        }

        virtual int32_t ipc_pal_register_global_callback(pal_global_callback cb, uint64_t cookie) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            remote()->transact(REGISTER_GLOBAL_CALLBACK, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_gef_rw_param(uint32_t param_id, void *param_payload,
                                    size_t payload_size, pal_device_id_t pal_device_id,
                                    pal_stream_type_t pal_stream_type, unsigned int dir) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint32(param_id);
            PalReadWriteTool::write_param_payload(payload_size, (uint8_t *)param_payload, data);
            data.writeUint32((uint32_t)pal_device_id);
            data.writeUint32((uint32_t)pal_stream_type);
            data.writeUint32((uint32_t)dir);

            remote()->transact(GEF_RW_PARAM, data, &reply);
            return reply.readInt32();
        }

        virtual int32_t ipc_pal_gef_rw_param_acdb(uint32_t param_id, void *param_payload,
                                        size_t payload_size, pal_device_id_t pal_device_id,
                                        pal_stream_type_t pal_stream_type, uint32_t sample_rate,
                                        uint32_t instance_id, uint32_t dir, bool is_play) override {
            android::Parcel data, reply;
            data.writeInterfaceToken(IPalService::getInterfaceDescriptor());
            data.writeUint32(param_id);
            PalReadWriteTool::write_param_payload(payload_size, (uint8_t *)param_payload, data);
            data.writeUint32((uint32_t)pal_device_id);
            data.writeUint32((uint32_t)pal_stream_type);
            data.writeUint32(sample_rate);
            data.writeUint32(instance_id);
            data.writeUint32(dir);
            data.write(&is_play, sizeof(bool));

            remote()->transact(GEF_RW_PARAM_ACDB, data, &reply);
            return reply.readInt32();
        }
};

IMPLEMENT_META_INTERFACE(PalService, "PalService");

android::status_t BnPalService::onTransact(uint32_t code,
                            const android::Parcel& data,
                            android::Parcel* reply, uint32_t flags) {
    int rc = -EINVAL;
    data.checkInterface(this);

    switch(code) {
        case REG_CLIENT : {
            android::sp<android::IBinder> binder = data.readStrongBinder();
            android::sp<android::IBinder> cb_binder = data.readStrongBinder();
            pal_register_client(binder, cb_binder, this);
            break;
        }
        case UNREG_CLIENT : {
            android::sp<android::IBinder> binder = data.readStrongBinder();
            pal_unregister_client(binder);
            break;
        }
        case GET_VERSION: {
            std::string version = ipc_pal_get_version();
            if (version.size() > 0) rc = 0;
            PalReadWriteTool::write_param_payload(version.size(), (uint8_t *)version.c_str(), reply);
            break;
        }
        case INIT: {
            rc = ipc_pal_init();
            reply->writeInt32(rc);
            break;
        }
        case DEINIT: {
            ipc_pal_deinit();
            break;
        }
        case STREAM_OPEN: {
            pal_stream_callback cb = NULL;
            uint64_t cookie;
            pal_stream_handle_t* stream_handle = NULL;
            uint32_t temp_size = 0;

            struct pal_stream_attributes* attributes = NULL;
            PalReadWriteTool::read_pal_stream_attributes(&attributes, data);

            uint32_t no_of_devices = 0;
            struct pal_device *devices = NULL;
            PalReadWriteTool::read_pal_devices(no_of_devices, &devices, data);

            uint32_t no_of_modifiers = 0;
            struct modifier_kv *modifiers = NULL;
            PalReadWriteTool::read_modifier_kv(no_of_modifiers, &modifiers, data);

            cookie = data.readUint64();
            data.read(&cb, sizeof(pal_stream_callback *));

            rc = ipc_pal_stream_open(attributes,
                                     no_of_devices, devices,
                                     no_of_modifiers, modifiers,
                                     pal_event_callback, cookie, &stream_handle);
            if (stream_handle && !rc) {
                pal_add_stream_handle((uint64_t)stream_handle, attributes, cb);
            }

            reply->writeInt32(rc);
            if (rc == 0) reply->writeUint64((uint64_t)stream_handle);

            if (devices) delete [] devices;
            if (modifiers) delete [] modifiers;
            if (attributes) delete [] attributes;
            break;
        }
        case STREAM_CLOSE: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            rc = ipc_pal_stream_close(stream_handle);
            pal_remove_stream_handle((uint64_t)stream_handle);
            reply->writeInt32(rc);
            break;
        }
        case STREAM_START: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            rc = ipc_pal_stream_start(stream_handle);
            reply->writeInt32(rc);
            break;
        }
        case STREAM_STOP: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            rc = ipc_pal_stream_stop(stream_handle);
            reply->writeInt32(rc);
            break;
        }
        case STREAM_PAUSE: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            rc = ipc_pal_stream_pause(stream_handle);
            reply->writeInt32(rc);
            break;
        }
        case STREAM_RESUME: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            rc = ipc_pal_stream_resume(stream_handle);
            reply->writeInt32(rc);
            break;
        }
        case STREAM_FLUSH: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            rc = ipc_pal_stream_flush(stream_handle);
            reply->writeInt32(rc);
            break;
        }
        case STREAM_DRAIN: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            pal_drain_type_t type = (pal_drain_type_t)data.readUint32();
            rc = ipc_pal_stream_drain(stream_handle, type);
            reply->writeInt32(rc);
            break;
        }
        case STREAM_SUSPEND: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            rc = ipc_pal_stream_suspend(stream_handle);
            reply->writeInt32(rc);
            break;
        }
        case STREAM_GET_BUFFER_SIZE: {
#if 0       // This api is not yet implemented in PAL
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            rc = ipc_pal_stream_get_buffer_size(stream_handle, nullptr, nullptr);
            reply->writeInt32(rc);
#endif
            break;
        }
        case STREAM_GET_TAGS_WITH_MODULE_INFO: {
            size_t size = 0;
            uint8_t *payload = NULL;
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            size = data.readUint32();
            if (size) payload = new uint8_t[size];

            rc = ipc_pal_stream_get_tags_with_module_info(stream_handle, &size, payload);
            reply->writeInt32(rc);

            if (!rc) {
                PalReadWriteTool::write_param_payload(size, payload, reply);
            }

            if (payload) delete [] payload;
            break;
        }
        case STREAM_SET_BUFFER_SIZE: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            pal_buffer_config_t *p_in_buff_cfg = NULL, *p_out_buff_cfg = NULL;

            PalReadWriteTool::read_pal_buffer_config(&p_in_buff_cfg, data);
            PalReadWriteTool::read_pal_buffer_config(&p_out_buff_cfg, data);
            if (p_in_buff_cfg) {
                if (!p_in_buff_cfg->max_metadata_size) p_in_buff_cfg->max_metadata_size = MetadataParser::WRITE_METADATA_MAX_SIZE();
            }

            if (p_out_buff_cfg) {
                if (!p_out_buff_cfg->max_metadata_size) p_out_buff_cfg->max_metadata_size = MetadataParser::READ_METADATA_MAX_SIZE();
            }

            rc = ipc_pal_stream_set_buffer_size(stream_handle, p_in_buff_cfg, p_out_buff_cfg);
            reply->writeInt32(rc);

            if (p_in_buff_cfg) delete [] p_in_buff_cfg;
            if (p_out_buff_cfg) delete [] p_out_buff_cfg;
            break;
        }
        case STREAM_READ: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();

            struct pal_buffer * p_buffer = NULL;
            PalReadWriteTool::read_pal_buffer(&p_buffer, data);

            if (p_buffer) {
                if (p_buffer->alloc_info.alloc_size > 0 && p_buffer->alloc_info.alloc_handle > 0) {
                    int binder_fd = data.readFileDescriptor();
                    int input_fd = pal_add_shared_fd((uint64_t)stream_handle, p_buffer->alloc_info.alloc_handle, binder_fd);
                    PAL_DBG(LOG_TAG, "read: client_fd: %d, binder fd: %d, dup_fd: %d, frame_id: %d\n", p_buffer->alloc_info.alloc_handle, binder_fd, input_fd, p_buffer->frame_index);
                    p_buffer->alloc_info.alloc_handle = input_fd;
                }

                p_buffer->metadata_size = MetadataParser::READ_METADATA_MAX_SIZE();
                if (metadata_size_used + p_buffer->metadata_size > MAX_METADATA_SIZE) metadata_size_used = 0;
                p_buffer->metadata = METADATA + metadata_size_used;
                metadata_size_used += p_buffer->metadata_size;
            }
            rc = ipc_pal_stream_read(stream_handle, p_buffer);
            if (p_buffer->buffer) PalReadWriteTool::write_param_payload(rc > 0?rc:0, p_buffer->buffer, reply);
            else reply->writeUint32(0);

            if (p_buffer && p_buffer->buffer) delete [] p_buffer->buffer;
            if (p_buffer) delete [] p_buffer;
            break;
        }
        case STREAM_WRITE: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();

            struct pal_buffer * p_buffer = NULL;
            PalReadWriteTool::read_pal_buffer(&p_buffer, data);

            if (p_buffer) {
                if (p_buffer->alloc_info.alloc_size > 0 && p_buffer->alloc_info.alloc_handle > 0) {
                    int binder_fd = data.readFileDescriptor();
                    int input_fd = pal_add_shared_fd((uint64_t)stream_handle, p_buffer->alloc_info.alloc_handle, binder_fd);
                    PAL_DBG(LOG_TAG, "write: client_fd: %d, binder fd: %d, dup_fd: %d, frame_id: %d\n", p_buffer->alloc_info.alloc_handle, binder_fd, input_fd, p_buffer->frame_index);
                    p_buffer->alloc_info.alloc_handle = input_fd;
                }

                pal_client_stream_handle *handle = NULL;
                get_client_stream_handle((uint64_t)stream_handle, NULL, &handle);
                if (handle) {
                    p_buffer->metadata_size = MetadataParser::WRITE_METADATA_MAX_SIZE();
                    p_buffer->metadata = new uint8_t[p_buffer->metadata_size];
                    auto metadataParser = std::make_unique<MetadataParser>();
                    metadataParser->fillMetaData(p_buffer->metadata, p_buffer->frame_index, p_buffer->size, &(handle->stream_attr.out_media_config));
                }
            }

            rc = ipc_pal_stream_write(stream_handle, p_buffer);
            if (p_buffer) addToPendingInputs(p_buffer->alloc_info.alloc_handle, p_buffer->alloc_info.offset, p_buffer->frame_index);

            reply->writeInt32(rc);
            if (p_buffer && p_buffer->buffer) delete [] p_buffer->buffer;
            if (p_buffer && p_buffer->ts) delete [] p_buffer->ts;
            if (p_buffer && p_buffer->metadata) delete [] p_buffer->metadata;
            if (p_buffer) delete [] p_buffer;
            break;
        }
        case STREAM_GET_DEVICE: {
#if 0       // This api is not yet implemented in PAL
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            rc = ipc_pal_stream_get_device(stream_handle, 0, nullptr);
            reply->writeInt32(rc);
#endif
            break;
        }
        case STREAM_SET_DEVICE: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();

            uint32_t no_of_devices = 0;
            struct pal_device *devices = NULL;
            PalReadWriteTool::read_pal_devices(no_of_devices, &devices, data);

            rc = ipc_pal_stream_set_device(stream_handle, no_of_devices, devices);

            reply->writeInt32(rc);
            if (devices) delete [] devices;
            break;
        }
        case STREAM_GET_PARAM: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            uint32_t param_id = data.readUint32();
            pal_param_payload *payload = NULL;
            rc = ipc_pal_stream_get_param(stream_handle, param_id, &payload);
            reply->writeInt32(rc);
            if (!rc) {
                uint32_t temp_size = payload->payload_size + sizeof(pal_param_payload);
                PalReadWriteTool::write_param_payload(payload?temp_size:0, (uint8_t *)payload, reply);
            }
            if (payload) free(payload);
            break;
        }
        case STREAM_SET_PARAM: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            uint32_t param_id = data.readUint32();
            uint32_t temp_size = 0;
            uint8_t* payload = NULL;

            PalReadWriteTool::read_param_payload(temp_size, &payload, data);

            rc = ipc_pal_stream_set_param(stream_handle, param_id, (pal_param_payload *)payload);
            reply->writeInt32(rc);
            if (payload) delete [] payload;
            break;
        }
        case STREAM_GET_VOLUME: {
#if 0       // This api is not yet implemented in PAL
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            rc = ipc_pal_stream_get_volume(stream_handle, nullptr);
            reply->writeInt32(rc);
#endif
            break;
        }
        case STREAM_SET_VOLUME: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            struct pal_volume_data* volume = NULL;

            PalReadWriteTool::read_pal_volume_data(&volume, data);
            rc = ipc_pal_stream_set_volume(stream_handle, volume);
            if (volume) delete [] volume;
            reply->writeInt32(rc);
            break;
        }
        case STREAM_GET_MUTE: {
#if 0       // This api is not yet implemented in PAL
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            rc = ipc_pal_stream_get_mute(stream_handle, nullptr);
            reply->writeInt32(rc);
#endif
            break;
        }
        case STREAM_SET_MUTE: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            bool state;
            data.read(&state, sizeof(bool));
            rc = ipc_pal_stream_set_mute(stream_handle, state);
            reply->writeInt32(rc);
            break;
        }
        case SET_MIC_MUTE: {
#if 0       // This api is not yet implemented in PAL
            rc = ipc_pal_set_mic_mute(false);
            reply->writeInt32(rc);
#endif
            break;
        }
        case GET_MIC_MUTE: {
#if 0       // This api is not yet implemented in PAL
            rc = ipc_pal_get_mic_mute(nullptr);
            reply->writeInt32(rc);
#endif
            break;
        }
        case GET_TIMESTAMP: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            struct pal_session_time stime = {0};
            struct pal_session_time *sess_time = &stime;

            rc = ipc_pal_get_timestamp(stream_handle, sess_time);
            reply->writeInt32(rc);
            if (!rc) {
                PalReadWriteTool::write_pal_session_time(sess_time, reply);
            }
            break;
        }
        case ADD_REMOVE_EFFECT: {
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            pal_audio_effect_t effect = (pal_audio_effect_t)data.readUint32();
            bool enable;
            data.read(&enable, sizeof(bool));
            rc = ipc_pal_add_remove_effect(stream_handle, effect, enable);
            reply->writeInt32(rc);
            break;
        }
        case SET_PARAM: {
            uint32_t param_id = data.readUint32();
            uint32_t payload_size = 0;
            uint8_t* payload = NULL;

            PalReadWriteTool::read_param_payload(payload_size, &payload, data);
            rc = ipc_pal_set_param(param_id, payload, payload_size);

            reply->writeInt32(rc);
            if (payload) delete [] payload;
            break;
        }
        case GET_PARAM: {
            uint32_t param_id = data.readUint32();
            size_t payload_size = 0;
            void * payload = NULL;

            rc = ipc_pal_get_param(param_id, &payload, &payload_size, nullptr);
            reply->writeInt32(rc);
            if (!rc) {
                PalReadWriteTool::write_param_payload(payload_size, (uint8_t *)payload, reply);
            }
            break;
        }
        case STREAM_CREATE_MMAP_BUFFER: {
            struct pal_mmap_buffer info = {0};
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            int32_t min_size_frames = data.readInt32();

            rc = ipc_pal_stream_create_mmap_buffer(stream_handle, min_size_frames, &info);
            reply->writeInt32(rc);
            if (!rc) PalReadWriteTool::write_pal_mmap_buffer(&info, reply);
            break;
        }
        case STREAM_GET_MMAP_POSITION: {
            struct pal_mmap_position position = {0};
            struct pal_mmap_position* p_position = &position;
            pal_stream_handle_t* stream_handle = (pal_stream_handle_t*)data.readUint64();
            rc = ipc_pal_stream_get_mmap_position(stream_handle, p_position);
            reply->writeInt32(rc);
            if (!rc) {
                PalReadWriteTool::write_pal_mmap_position(p_position, reply);
            }
            break;
        }
        case REGISTER_GLOBAL_CALLBACK: {
            rc = ipc_pal_register_global_callback(nullptr, 0);
            reply->writeInt32(rc);
            break;
        }
        case GEF_RW_PARAM: {
            uint32_t param_id = data.readUint32();
            uint32_t payload_size = 0;
            uint8_t *param_payload = NULL;
            pal_device_id_t pal_device_id;
            pal_stream_type_t pal_stream_type;
            uint32_t dir;

            PalReadWriteTool::read_param_payload(payload_size, &param_payload, data);
            pal_device_id = (pal_device_id_t)data.readUint32();
            pal_stream_type = (pal_stream_type_t)data.readUint32();
            dir = data.readUint32();
            rc = ipc_pal_gef_rw_param(param_id, param_payload, (size_t)payload_size, pal_device_id, pal_stream_type, dir);

            reply->writeInt32(rc);
            if (param_payload) delete [] param_payload;
            break;
        }
        case GEF_RW_PARAM_ACDB: {
            uint32_t param_id = data.readUint32();
            uint32_t payload_size = 0;
            uint8_t *param_payload = NULL;
            pal_device_id_t pal_device_id;
            pal_stream_type_t pal_stream_type;
            uint32_t sample_rate;
            uint32_t instance_id;
            uint32_t dir;
            bool is_play;

            PalReadWriteTool::read_param_payload(payload_size, &param_payload, data);
            pal_device_id = (pal_device_id_t)data.readUint32();
            pal_stream_type = (pal_stream_type_t)data.readUint32();
            sample_rate = data.readUint32();
            instance_id = data.readUint32();
            dir = data.readUint32();
            data.read(&is_play, sizeof(bool));
            rc = ipc_pal_gef_rw_param_acdb(param_id, param_payload, (size_t)payload_size, pal_device_id, pal_stream_type,
                                           sample_rate, instance_id, dir, is_play);

            reply->writeInt32(rc);
            if (param_payload) delete [] param_payload;
            break;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
    return rc;
}

extern "C" void load_pal_service() {
    android::sp<android::IServiceManager> sm = android::defaultServiceManager();
    sm->addService(android::String16(PAL_SERVICE_NAME), new PalService(), false);
    android::ProcessState::self()->startThreadPool();
}
