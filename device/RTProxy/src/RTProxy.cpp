/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: RTProxy"
#define LOG_TAG_OUT "PAL: RTProxyOut"
#include "RTProxy.h"
#include "ResourceManager.h"
#include "Device.h"
#include "kvh2xml.h"

#include "PayloadBuilder.h"
#include "Stream.h"
#include "Session.h"
#include "SessionAR.h"

extern "C" void CreateRTProxyDevice(struct pal_device *device,
                                    const std::shared_ptr<ResourceManager> rm,
                                    std::shared_ptr<Device> *dev) {
    if (device != nullptr) {
        switch (device->id) {
            case PAL_DEVICE_OUT_PROXY:
            case PAL_DEVICE_OUT_RECORD_PROXY:
            case PAL_DEVICE_OUT_HEARING_AID:
                *dev = RTProxyOut::getInstance(device, rm);
                break;
            case PAL_DEVICE_IN_PROXY:
            case PAL_DEVICE_IN_RECORD_PROXY:
            case PAL_DEVICE_IN_TELEPHONY_RX:
                *dev = RTProxyIn::getInstance(device, rm);
                break;
            default:
                break;
        }
    } else {
        PAL_ERR(LOG_TAG, "Invalid input parameters");
    }
}

RTProxyIn::RTProxyIn(struct pal_device *device, std::shared_ptr<ResourceManager> Rm) :
Device(device, Rm)
{
    rm = Rm;
    setDeviceAttributes(*device);
}

RTProxyIn::~RTProxyIn()
{

}

int32_t RTProxyIn::isSampleRateSupported(uint32_t sampleRate)
{
    PAL_DBG(LOG_TAG, "sampleRate %u", sampleRate);
    /* Proxy supports all sample rates, accept by default */
    return 0;
}

int32_t RTProxyIn::isChannelSupported(uint32_t numChannels)
{
    PAL_DBG(LOG_TAG, "numChannels %u", numChannels);
    /* Proxy supports all channels, accept by default */
    return 0;
}

int32_t RTProxyIn::isBitWidthSupported(uint32_t bitWidth)
{
    PAL_DBG(LOG_TAG, "bitWidth %u", bitWidth);
    /* Proxy supports all bitwidths, accept by default */
    return 0;
}


std::shared_ptr<Device> RTProxyIn::objPlay = nullptr;
std::shared_ptr<Device> RTProxyIn::objRecord = nullptr;

std::shared_ptr<Device> RTProxyIn::getInstance(struct pal_device *device,
                                             std::shared_ptr<ResourceManager> Rm)
{
    std::shared_ptr<Device> obj = nullptr;
    if (device->id == PAL_DEVICE_IN_TELEPHONY_RX ||
       device->id == PAL_DEVICE_IN_PROXY) {
        if (!objPlay) {
            std::lock_guard<std::mutex> lock(Device::mInstMutex);
            if (!objPlay) {
                std::shared_ptr<Device> sp(new RTProxyIn(device, Rm));
                objPlay = sp;
            }
        }
        obj = objPlay;
    }
    if (device->id == PAL_DEVICE_IN_RECORD_PROXY) {
        if (!objRecord) {
            std::lock_guard<std::mutex> lock(Device::mInstMutex);
            if (!objRecord) {
                std::shared_ptr<Device> sp(new RTProxyIn(device, Rm));
                objRecord = sp;
            }
        }
        obj = objRecord;
    }
    return obj;
}

int RTProxyIn::start() {
    int status = 0;
    uint8_t* paramData = NULL;
    size_t paramSize = 0;
    uint32_t ratMiid = 0;
    //struct pal_media_config config;
    Stream *stream = NULL;
    Session *session = NULL;
    std::vector<Stream*> activestreams;
    std::shared_ptr<Device> dev = nullptr;
    std::string backEndName;
    PayloadBuilder* builder = new PayloadBuilder();

   if (customPayload)
        free(customPayload);

    customPayload = NULL;
    customPayloadSize = 0;
    mDeviceMutex.lock();
    if (0 < deviceStartStopCount) {
        goto start;
    }
    rm->getBackendName(deviceAttr.id, backEndName);

    dev = getInstance(&deviceAttr, rm);

    status = rm->getActiveStream_l(activestreams, dev);
    if ((0 != status) || (activestreams.size() == 0)) {
        PAL_ERR(LOG_TAG, "no active stream available");
        status = -EINVAL;
        goto error;
    }
    stream = static_cast<Stream *>(activestreams[0]);
    stream->getAssociatedSession(&session);

    status = dynamic_cast<SessionAR*>(session)->getMIID(backEndName.c_str(), RAT_RENDER, &ratMiid);
    if (status) {
        PAL_INFO(LOG_TAG,
         "Failed to get tag info %x Skipping RAT Configuration Setup, status = %d",
          RAT_RENDER, status);
        //status = -EINVAL;
        status = 0;
        goto start;
    }

    builder->payloadRATConfig(&paramData, &paramSize, ratMiid, &deviceAttr.config);
    if (dev && paramSize) {
        dev->updateCustomPayload(paramData, paramSize);
        if (paramData)
            free(paramData);
        paramData = NULL;
        paramSize = 0;
    } else {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Invalid RAT module param size");
        goto error;
    }

start:
    status = Device::start_l();
error:
    mDeviceMutex.unlock();
    if (builder) {
       delete builder;
       builder = NULL;
    }
    return status;
}

int32_t RTProxyIn::getDeviceConfig(struct pal_device *deviceattr,
                                  struct pal_stream_attributes *sAttr) {
   int32_t status = 0;
   std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();

   /* For PAL_DEVICE_IN_PROXY, copy all config from stream attributes */
   if (!sAttr) {
       PAL_ERR(LOG_TAG, "Invalid parameter.");
       return -EINVAL;
   }

   struct pal_media_config *candidateConfig = &sAttr->in_media_config;
   PAL_DBG(LOG_TAG, "sattr chn=0x%x fmt id=0x%x rate = 0x%x width=0x%x",
       sAttr->in_media_config.ch_info.channels,
       sAttr->in_media_config.aud_fmt_id,
       sAttr->in_media_config.sample_rate,
       sAttr->in_media_config.bit_width);

   if (!rm->ifVoiceorVoipCall(sAttr->type) && rm->isDeviceAvailable(PAL_DEVICE_OUT_PROXY)) {
       PAL_DBG(LOG_TAG, "This is NOT voice call. out proxy is available");
       std::shared_ptr<Device> devOut = nullptr;
       struct pal_device proxyOut_dattr;
       proxyOut_dattr.id = PAL_DEVICE_OUT_PROXY;
       devOut = Device::getInstance(&proxyOut_dattr, rm);
       if (devOut) {
           status = devOut->getDeviceAttributes(&proxyOut_dattr);
           if (status) {
               PAL_ERR(LOG_TAG, "getDeviceAttributes for OUT_PROXY failed %d", status);
               return 0;
           }

           if (proxyOut_dattr.config.ch_info.channels &&
                   proxyOut_dattr.config.sample_rate) {
               PAL_INFO(LOG_TAG, "proxy out attr is used");
               candidateConfig = &proxyOut_dattr.config;
           }
       }
   }
   deviceattr->config.ch_info = candidateConfig->ch_info;
   if (isPalPCMFormat(candidateConfig->aud_fmt_id))
       deviceattr->config.bit_width =
                 rm->palFormatToBitwidthLookup(candidateConfig->aud_fmt_id);
   else
       deviceattr->config.bit_width = candidateConfig->bit_width;

   deviceattr->config.aud_fmt_id = candidateConfig->aud_fmt_id;
   deviceattr->config.sample_rate = candidateConfig->sample_rate;

   PAL_INFO(LOG_TAG, "in proxy chn=0x%x fmt id=0x%x rate = 0x%x width=0x%x",
               deviceattr->config.ch_info.channels,
               deviceattr->config.aud_fmt_id,
               deviceattr->config.sample_rate,
               deviceattr->config.bit_width);

    return status;
}


std::shared_ptr<Device> RTProxyOut::objPlay = nullptr;
std::shared_ptr<Device> RTProxyOut::objRecord = nullptr;

RTProxyOut::RTProxyOut(struct pal_device *device, std::shared_ptr<ResourceManager> Rm) :
    Device(device, Rm)
{
    rm = Rm;
    setDeviceAttributes(*device);
}

std::shared_ptr<Device> RTProxyOut::getInstance(struct pal_device *device,
                                             std::shared_ptr<ResourceManager> Rm)
{
    std::shared_ptr<Device> obj = nullptr;

    if (device->id == PAL_DEVICE_OUT_HEARING_AID ||
       device->id == PAL_DEVICE_OUT_PROXY) {
        if (!objPlay) {
            std::lock_guard<std::mutex> lock(Device::mInstMutex);
            if (!objPlay) {
                std::shared_ptr<Device> sp(new RTProxyOut(device, Rm));
                objPlay = sp;
            }
        }
        obj = objPlay;
    }
    if (device->id == PAL_DEVICE_OUT_RECORD_PROXY) {
        if (!objRecord) {
            std::lock_guard<std::mutex> lock(Device::mInstMutex);
            if (!objRecord) {
                std::shared_ptr<Device> sp(new RTProxyOut(device, Rm));
                objRecord = sp;
            }
        }
        obj = objRecord;
    }
    return obj;
}

int32_t RTProxyOut::isSampleRateSupported(uint32_t sampleRate)
{
    PAL_DBG(LOG_TAG, "sampleRate %u", sampleRate);
    /* Proxy supports all sample rates, accept by default */
    return 0;
}

int32_t RTProxyOut::isChannelSupported(uint32_t numChannels)
{
    PAL_DBG(LOG_TAG, "numChannels %u", numChannels);
    /* Proxy supports all channels, accept by default */
    return 0;
}

int32_t RTProxyOut::isBitWidthSupported(uint32_t bitWidth)
{
    PAL_DBG(LOG_TAG, "bitWidth %u", bitWidth);
    /* Proxy supports all bitwidths, accept by default */
    return 0;
}
int RTProxyOut::start() {
    if (customPayload)
        free(customPayload);

    customPayload = NULL;
    customPayloadSize = 0;

    return Device::start();
}

int32_t RTProxyOut::getDeviceConfig(struct pal_device *deviceattr,
                                    struct pal_stream_attributes *sAttr) {
   int32_t status = 0;
   struct pal_stream_attributes tx_attr;
   bool is_wfd_in_progress = false;
   std::list <Stream*> activeStreams;
   std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();

   if (!sAttr) {
       PAL_ERR(LOG_TAG, "Invalid parameter.");
       return -EINVAL;
   }

   if (deviceattr->id == PAL_DEVICE_OUT_HEARING_AID) {
        deviceattr->config.ch_info = sAttr->out_media_config.ch_info;
        deviceattr->config.bit_width = sAttr->out_media_config.bit_width;
        deviceattr->config.aud_fmt_id = sAttr->out_media_config.aud_fmt_id;
        PAL_DBG(LOG_TAG, "device %d sample rate %d bitwidth %d",
                deviceattr->id,deviceattr->config.sample_rate,
                deviceattr->config.bit_width);
    }
    else {
        activeStreams = rm->getActiveStreamList();
        // check if wfd session in progress
        for (auto& tx_str: activeStreams) {
            tx_str->getStreamAttributes(&tx_attr);
            if (tx_attr.direction == PAL_AUDIO_INPUT &&
                tx_attr.info.opt_stream_info.tx_proxy_type == PAL_STREAM_PROXY_TX_WFD) {
                is_wfd_in_progress = true;
                break;
            }
        }

        if (is_wfd_in_progress) {
            PAL_INFO(LOG_TAG, "wfd TX is in progress");
            std::shared_ptr<Device> dev = nullptr;
            struct pal_device proxyIn_dattr;
            proxyIn_dattr.id = PAL_DEVICE_IN_PROXY;
            dev = Device::getInstance(&proxyIn_dattr, rm);
            if (dev) {
                status = dev->getDeviceAttributes(&proxyIn_dattr);
                if (status) {
                    PAL_ERR(LOG_TAG, "OUT_PROXY getDeviceAttributes failed %d", status);
                    return status;
                }
                deviceattr->config.ch_info = proxyIn_dattr.config.ch_info;
                deviceattr->config.sample_rate = proxyIn_dattr.config.sample_rate;
                if (isPalPCMFormat(proxyIn_dattr.config.aud_fmt_id))
                    deviceattr->config.bit_width =
                          rm->palFormatToBitwidthLookup(proxyIn_dattr.config.aud_fmt_id);
                else
                    deviceattr->config.bit_width = proxyIn_dattr.config.bit_width;

                deviceattr->config.aud_fmt_id = proxyIn_dattr.config.aud_fmt_id;
            }
        }
        else {
            PAL_INFO(LOG_TAG, "wfd TX is not in progress");
            if (rm->getProxyChannels()) {
                PAL_INFO(LOG_TAG, "proxy device channel number: %d", rm->getProxyChannels());
                deviceattr->config.ch_info.channels = rm->getProxyChannels();
            }
        }
        PAL_INFO(LOG_TAG, "PAL_DEVICE_OUT_PROXY sample rate %d bitwidth %d ch:%d",
                deviceattr->config.sample_rate, deviceattr->config.bit_width,
                deviceattr->config.ch_info.channels);
    }

   return status;
}

RTProxyOut::~RTProxyOut()
{

}
