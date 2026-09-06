/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *   * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef DEVICE_H
#define DEVICE_H
#include <iostream>
#include <map>
#include <mutex>
#include <memory>
#include "PalApi.h"
#include "PalDefs.h"
#include <string.h>
#include "PalCommon.h"
#include "Device.h"
#include "PluginManager.h"

#define DEVICE_NAME_MAX_SIZE 128
#define DUMP_DEV_ATTR 0
#define DEFAULT_OUTPUT_CHANNEL 2

class Stream;
class ResourceManager;

class Device
{
protected:
    std::shared_ptr<Device> devObj;
    std::mutex mDeviceMutex;
    static std::mutex mInstMutex;
    std::string mPALDeviceName;
    struct pal_device deviceAttr;
    std::shared_ptr<ResourceManager> rm;
    int deviceCount = 0;
    int deviceStartStopCount = 0;
    struct audio_route *audioRoute = NULL;   //getAudioRoute() from RM and store
    struct mixer *virtualMixerHandle = NULL;   //getVirtualAudioMixer() from RM and store
    struct mixer *hwMixerHandle = NULL;   //getHwAudioMixer() from RM and store
    char mSndDeviceName[DEVICE_NAME_MAX_SIZE] = {0};
    void *customPayload;
    size_t customPayloadSize;
    std::string UpdatedSndName;
    uint32_t mCurrentPriority;
    //device atrributues per stream are stored by priority in a map
    std::multimap<uint32_t, std::pair<Stream *, struct pal_device *>> mStreamDevAttr;
    uint32_t mSampleRate = 0;
    uint32_t mBitWidth = 0;

    Device(struct pal_device *device, std::shared_ptr<ResourceManager> Rm);
    Device();
    int32_t configureDeviceClockSrc(char const *mixerStrClockSrc, const uint32_t clockSrc);
public:
    virtual int init(pal_param_device_connection_t device_conn);
    virtual int deinit(pal_param_device_connection_t device_conn);
    virtual int getDefaultConfig(pal_param_device_capability_t capability);
    int open();
    virtual int close();
    virtual int start();
    int start_l();
    virtual int stop();
    int stop_l();
    int prepare();
    static std::shared_ptr<Device> getInstance(struct pal_device *device,
                                               std::shared_ptr<ResourceManager> Rm);
    int getSndDeviceId();
    int getDeviceCount();
    std::string getPALDeviceName();
    int setDeviceAttributes(struct pal_device &dattr);
    virtual int getDeviceAttributes(struct pal_device *dattr,
                                    Stream* streamHandle = NULL);
    virtual int getCodecConfig(struct pal_media_config *config);
    int updateCustomPayload(void *payload, size_t size);
    int freeCustomPayload(uint8_t **payload, size_t *payloadSize);
    void* getCustomPayload();
    size_t getCustomPayloadSize();
    virtual int32_t setDeviceParameter(uint32_t param_id, void *param);
    virtual int32_t setParameter(uint32_t param_id, void *param);
    virtual int32_t getDeviceParameter(uint32_t param_id, void **param);
    virtual int32_t getParameter(uint32_t param_id, void **param);
    virtual bool isDeviceReady(pal_device_id_t id) { return true;}
    virtual bool isScoNbWbActive() { return false;}
    virtual int32_t checkAndUpdateSampleRate(uint32_t *sampleRate);
    virtual int32_t checkAndUpdateBitWidth(uint32_t *bitWidth);
    virtual int selectBestConfig(struct pal_device *dattr,
                                   struct pal_stream_attributes *sattr,
                                   bool is_playback, struct pal_device_info *devinfo);
    virtual int getMaxChannel();
    virtual int getHighestSupportedSR();
    virtual int32_t isBitWidthSupported(uint32_t bitWidth);
    virtual bool isSupportedSR(int sr);
    virtual int getHighestSupportedBps();
    virtual bool isDeviceConnected(struct pal_usb_device_address addr);
    virtual int32_t checkDeviceStatus();
    virtual int32_t getDeviceConfig(struct pal_device *deviceattr,
                                    struct pal_stream_attributes *sAttr);
    static unsigned int palToSndDriverFormat(uint32_t fmt_id);
    unsigned int bitsToAlsaFormat(unsigned int bits);
    struct mixer_ctl *getBeMixerControl(struct mixer *am, std::string beName,
        uint32_t idx);
    int setCustomPayload(std::shared_ptr<ResourceManager> rmHandle,
                            std::string backEndName, void *payload, size_t size);
    int setMediaConfig(std::shared_ptr<ResourceManager> rmHandle,
                            std::string backEndName, struct pal_device *dAttr);
    void setSndName (std::string snd_name) { UpdatedSndName = snd_name;}
    void clearSndName () { UpdatedSndName.clear();}
    virtual ~Device();
    void getCurrentSndDevName(char *name);
    void setSampleRate(uint32_t sr){mSampleRate = sr;};
    void setBitWidth(uint32_t bw) {mBitWidth = bw;};
    void lockDeviceMutex() { mDeviceMutex.lock(); };
    void unlockDeviceMutex() { mDeviceMutex.unlock(); };
    bool compareStreamDevAttr(const struct pal_device *inDevAttr,
                        const struct pal_device_info *inDevInfo,
                        struct pal_device *curDevAttr,
                        const struct pal_device_info *curDevInfo);
    int insertStreamDeviceAttr(struct pal_device *deviceAttr,
                                Stream* streamHandle);
    void removeStreamDeviceAttr(Stream* streamHandle);
    int getTopPriorityDeviceAttr(struct pal_device *deviceAttr, uint32_t *streamPrio);
    static int32_t initHdrRoutine(const char *hdr_custom_key);
    virtual bool isPluginDevice(pal_device_id_t id) { return false; }
    virtual bool isDpDevice(pal_device_id_t id) { return false; }
    virtual bool isPluginPlaybackDevice(pal_device_id_t id) { return false; }
    /*
     * A prebuilt libar-pal can carry further virtuals after these. Without
     * them a source built device plugin exports a vtable shorter than the one
     * the prebuilt indexes, and the prebuilt reads past its end.
     */
#if PAL_VENDOR_EXTRA_DEVICE_VIRTUALS > 0
    virtual int32_t palVendorDeviceVirtual1() { return 0; }
#endif
#if PAL_VENDOR_EXTRA_DEVICE_VIRTUALS > 1
    virtual int32_t palVendorDeviceVirtual2() { return 0; }
#endif
#if PAL_VENDOR_EXTRA_DEVICE_VIRTUALS > 2
    virtual int32_t palVendorDeviceVirtual3() { return 0; }
#endif
#if PAL_VENDOR_EXTRA_DEVICE_VIRTUALS > 3
    virtual int32_t palVendorDeviceVirtual4() { return 0; }
#endif
#if PAL_VENDOR_EXTRA_DEVICE_VIRTUALS > 4
#error "declare more slots here to match the prebuilt libar-pal"
#endif

    static std::shared_ptr<PluginManager> pm;
};

#endif //DEVICE_H
