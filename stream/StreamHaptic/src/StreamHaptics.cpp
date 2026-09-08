/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

 *   * Neither the name of Qualcomm Innovation Center, Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
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
 *
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: StreamHaptics"

#include "StreamHaptics.h"
#include "Session.h"
#include "ResourceManager.h"
#include "Device.h"
#include <unistd.h>
#include "rx_haptics_api.h"
#include "MemLogBuilder.h"
#include "wsa_haptics_vi_api.h"

extern "C" Stream* CreateHapticsStream(const struct pal_stream_attributes *sattr, struct pal_device *dattr,
                               const uint32_t no_of_devices, const struct modifier_kv *modifiers,
                               const uint32_t no_of_modifiers, const std::shared_ptr<ResourceManager> rm) {
    return new StreamHaptics(sattr, dattr, no_of_devices, modifiers, no_of_modifiers, rm);
}

#ifdef PAL_VENDOR_NO_STREAM_MIXER_EVENT_CALLBACK
static void hapticsMixerEventCallbackEntry(uint64_t hdl, uint32_t event_id,
                                           void *data, uint32_t event_size)
{
    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();

    rm->lockActiveStream();
    if (!rm->isActiveStream((pal_stream_handle_t *)hdl)) {
        PAL_ERR(LOG_TAG, "callback called on invalid stream object");
        rm->unlockActiveStream();
        return;
    }
    StreamHaptics *str = reinterpret_cast<StreamHaptics *>(hdl);
    if (rm->increaseStreamUserCounter(str)) {
        rm->unlockActiveStream();
        PAL_ERR(LOG_TAG, "callback called on invalid stream object");
        return;
    }
    rm->unlockActiveStream();

    str->HandleCallback(hdl, event_id, data, event_size);

    rm->lockActiveStream();
    rm->decreaseStreamUserCounter(str);
    rm->unlockActiveStream();
}
#endif

pal_stream_haptics_type_t StreamHaptics::activeHapticsType = PAL_STREAM_HAPTICS_RINGTONE;
std::mutex StreamHaptics::activeHapticsTypeMutex;

StreamHaptics::StreamHaptics(const struct pal_stream_attributes *sattr, struct pal_device *dattr __unused,
                    const uint32_t no_of_devices __unused, const struct modifier_kv *modifiers __unused,
                    const uint32_t no_of_modifiers __unused, const std::shared_ptr<ResourceManager> rm):
                  StreamPCM(sattr,dattr,no_of_devices,modifiers,no_of_modifiers,rm)
{
#ifdef PAL_VENDOR_NO_STREAM_MIXER_EVENT_CALLBACK
    session->registerCallBack(hapticsMixerEventCallbackEntry,((uint64_t) this));
#else
    session->registerCallBack(Stream::mixerEventCallbackEntry,((uint64_t) this));
#endif
}

StreamHaptics::~StreamHaptics()
{
}

int32_t  StreamHaptics::setParameters(uint32_t param_id, void *payload)
{
    int32_t status = -1;
    std::vector <Stream *> activeStreams;
    struct pal_stream_attributes ActivesAttr;
    Stream *stream = nullptr;

    if (!payload)
    {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "invalid params");
        goto error;
    }

    PAL_DBG(LOG_TAG, "Enter, set parameter %u", param_id);
    status = rm->getActiveStream_l(activeStreams, mDevices[0]);
    if ((0 != status) || (activeStreams.size() == 0 )) {
         PAL_DBG(LOG_TAG, "No Haptics stream is active");
         goto error;
    }
    PAL_DBG(LOG_TAG, "activestreams size %zu",activeStreams.size());

    mStreamMutex.lock();
    // Stream may not know about tags, so use setParameters instead of setConfig
    switch (param_id) {
        case PARAM_ID_HAPTICS_WAVE_DESIGNER_STOP_PARAM:
        {
            for (int i = 0; i<activeStreams.size(); i++) {
                stream = static_cast<Stream *>(activeStreams[i]);
                stream->getStreamAttributes(&ActivesAttr);
                if (ActivesAttr.info.opt_stream_info.haptics_type == PAL_STREAM_HAPTICS_RINGTONE &&
                    StreamHaptics::activeHapticsType == PAL_STREAM_HAPTICS_RINGTONE) {
                    PAL_INFO(LOG_TAG, "Ringtone is in running state, skipping stop");
                    mStreamMutex.unlock();
                    return 0;
                }
            }
        }
        [[fallthrough]];
        //fall through this case if above condition is not true.
        case PAL_PARAM_ID_HAPTICS_CNFG:
        case PARAM_ID_HAPTICS_WAVE_DESIGNER_UPDATE_PARAM:
        {
            status = session->setParameters(NULL, param_id, payload);
            if (status)
                PAL_ERR(LOG_TAG, "Error:%d, Failed to setParam", status);
            break;
        }
        case PARAM_ID_HAPTICS_EX_VI_PERSISTENT:
        {
            status =  mDevices[0]->setParameter(param_id, nullptr);
            if (status)
               PAL_ERR(LOG_TAG, "Error:%d, Failed to setParam", status);
            break;
        }
        default:
            PAL_ERR(LOG_TAG, "Error:Unsupported param id %u", param_id);
            status = -EINVAL;
            break;
    }

    mStreamMutex.unlock();
    PAL_DBG(LOG_TAG, "exit, session parameter %u set with status %d", param_id, status);
error:
    return status;
}

int32_t StreamHaptics::start()
{
    int32_t status = 0, devStatus = 0, cachedStatus = 0;
    int32_t tmp = 0;

    PAL_DBG(LOG_TAG, "Enter. session handle - %pK mStreamAttr->direction - %d state %d",
            session, mStreamAttr->direction, currentState);

    /* check for haptic concurrency*/
    if (ResourceManager::IsHapticsThroughWSA())
        UpdateCurrentHapticsStream(mStreamAttr);

    mStreamMutex.lock();
    if (rm->getSoundCardState() == CARD_STATUS_OFFLINE) {
        cachedState = STREAM_STARTED;
        PAL_ERR(LOG_TAG, "Sound card offline. Update the cached state %d",
                cachedState);
        goto exit;
    }

    if (currentState == STREAM_INIT || currentState == STREAM_STOPPED) {
        switch (mStreamAttr->direction) {
        case PAL_AUDIO_OUTPUT:
            PAL_VERBOSE(LOG_TAG, "Inside PAL_AUDIO_OUTPUT device count - %zu",
                            mDevices.size());

            rm->lockGraph();
            /* Any device start success will be treated as positive status.
             * This allows stream be played even if one of devices failed to start.
             */
            status = -EINVAL;
            for (int32_t i=0; i < mDevices.size(); i++) {
                devStatus = mDevices[i]->start();
                if (devStatus == 0) {
                    status = 0;
                } else {
                    cachedStatus = devStatus;

                    tmp = session->disconnectSessionDevice(this, mStreamAttr->type, mDevices[i]);
                    if (0 != tmp) {
                        PAL_ERR(LOG_TAG, "disconnectSessionDevice failed:%d", tmp);
                    }

                    tmp = mDevices[i]->close();
                    if (0 != tmp) {
                        PAL_ERR(LOG_TAG, "device close failed with status %d", tmp);
                    }
                    mDevices.erase(mDevices.begin() + i);
                    i--;
                }
            }
            if (0 != status) {
                status = cachedStatus;
                PAL_ERR(LOG_TAG, "Rx device start failed with status %d", status);
                rm->unlockGraph();
                goto exit;
            } else {
                PAL_VERBOSE(LOG_TAG, "devices started successfully");
            }

            status = session->prepare(this);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Rx session prepare is failed with status %d",
                        status);
                rm->unlockGraph();
                goto session_fail;
            }
            PAL_VERBOSE(LOG_TAG, "session prepare successful");

            status = session->start(this);
            if (errno == -ENETRESET) {
                if (rm->getSoundCardState() != CARD_STATUS_OFFLINE) {
                    PAL_ERR(LOG_TAG, "Sound card offline, informing RM");
                    rm->ssrHandler(CARD_STATUS_OFFLINE);
                }
                cachedState = STREAM_STARTED;
                /* Returning status 0,  hal shouldn't be
                 * informed of failure because we have cached
                 * the state and will start from STARTED state
                 * during SSR up Handling.
                 */
                status = 0;
                rm->unlockGraph();
                goto session_fail;
            }
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Rx session start is failed with status %d",
                        status);
                rm->unlockGraph();
                goto session_fail;
            }
            PAL_VERBOSE(LOG_TAG, "session start successful");
            rm->unlockGraph();

            break;
        default:
            status = -EINVAL;
            PAL_ERR(LOG_TAG, "Stream type is not supported, status %d", status);
            break;
        }
        for (int i = 0; i < mDevices.size(); i++) {
            rm->registerDevice(mDevices[i], this);
        }
        /*pcm_open and pcm_start done at once here,
         *so directly jump to STREAM_STARTED state.
         */
        currentState = STREAM_STARTED;
    } else if (currentState == STREAM_STARTED) {
        PAL_INFO(LOG_TAG, "Stream already started, state %d", currentState);
        goto exit;
    } else {
        PAL_ERR(LOG_TAG, "Stream is not opened yet");
        status = -EINVAL;
        goto exit;
    }
    goto exit;
session_fail:
    for (int32_t i=0; i < mDevices.size(); i++) {
        devStatus = mDevices[i]->stop();
        if (devStatus)
            status = devStatus;
        rm->deregisterDevice(mDevices[i], this);
    }
exit:
    palStateEnqueue(this, PAL_STATE_STARTED, status);
    PAL_DBG(LOG_TAG, "Exit. state %d, status %d", currentState, status);
    mStreamMutex.unlock();
    return status;
}

void StreamHaptics::UpdateCurrentHapticsStream(struct pal_stream_attributes *sattr)
{
    std::lock_guard<std::mutex> lock(activeHapticsTypeMutex);
    StreamHaptics::activeHapticsType =
                        (pal_stream_haptics_type_t)sattr->info.opt_stream_info.haptics_type;
    PAL_DBG(LOG_TAG, "Updating active haptics type to %d", StreamHaptics::activeHapticsType);
}

int32_t  StreamHaptics::registerCallBack(pal_stream_callback cb, uint64_t cookie)
{
    callback_ = cb;
    cookie_ = cookie;

    PAL_VERBOSE(LOG_TAG, "callback_ = %pK", callback_);

    return 0;
}

void StreamHaptics::HandleEvent(uint32_t event_id, void *data, uint32_t event_size) {
    struct param_id_haptics_wave_designer_state *event_info = nullptr;
    event_info = (struct param_id_haptics_wave_designer_state *)data;

    event_type[0] = (uint16_t)event_info->state[0];
    event_type[1] = (uint16_t)event_info->state[1];
    event_size = sizeof(event_type);

    PAL_INFO(LOG_TAG, "event received with value for vib 1 - %d vib 2- %d",
                    event_type[0], event_type[1]);

    if (callback_) {
        PAL_INFO(LOG_TAG, "Notify detection event to client");
        callback_((pal_stream_handle_t *)this, event_id, event_type,
                  event_size, cookie_);
    }
}

void StreamHaptics::HandleCallback(uint64_t hdl, uint32_t event_id,
                                      void *data, uint32_t event_size) {
    StreamHaptics *StreamHAPTICS = nullptr;
    PAL_DBG(LOG_TAG, "Enter, event detected on SPF, event id = 0x%x, event size =%d",
                      event_id, event_size);

    StreamHAPTICS = (StreamHaptics *)hdl;
    // Handle event form DSP
    if (event_id == EVENT_ID_WAVEFORM_STATE) {
        StreamHAPTICS->HandleEvent(event_id, data, event_size);
    }
    PAL_DBG(LOG_TAG, "Exit");
}

int32_t StreamHaptics::ssrDownHandler()
{
    int32_t status = 0;

    mStreamMutex.lock();

    if (false == isStreamSSRDownFeasibile()) {
        mStreamMutex.unlock();
        goto skip_down_handling;
    }

    /* Updating cached state here only if it's STREAM_IDLE,
     * Otherwise we can assume it is updated by hal thread
     * already.
     */
    if (cachedState == STREAM_IDLE)
        cachedState = currentState;
    PAL_DBG(LOG_TAG, "Enter. session handle - %pK cached State %d",
            session, cachedState);

    if (currentState == STREAM_INIT || currentState == STREAM_STOPPED) {
        mStreamMutex.unlock();
        status = close();
        if (0 != status) {
            PAL_ERR(LOG_TAG, "stream close failed. status %d", status);
            goto exit;
        }
    } else if (currentState == STREAM_STARTED || currentState == STREAM_PAUSED) {
        mStreamMutex.unlock();
        rm->unlockActiveStream();
        status = stop();
        if (0 != status)
            PAL_ERR(LOG_TAG, "stream stop failed. status %d",  status);
        status = close();
        rm->lockActiveStream();
        if (0 != status) {
            PAL_ERR(LOG_TAG, "stream close failed. status %d", status);
            goto exit;
        }
    } else {
        PAL_ERR(LOG_TAG, "stream state is %d, nothing to handle", currentState);
        mStreamMutex.unlock();
        goto exit;
    }

exit :
    currentState = STREAM_IDLE;
skip_down_handling :
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t StreamHaptics::ssrUpHandler()
{
    int32_t status = 0;

    if (mStreamAttr->info.opt_stream_info.haptics_type != PAL_STREAM_HAPTICS_RINGTONE)
        goto skip_up_handling;

    mStreamMutex.lock();
    PAL_DBG(LOG_TAG, "Enter. session handle - %pK state %d",
            session, cachedState);

    if (skipSSRHandling) {
        skipSSRHandling = false;
        mStreamMutex.unlock();
        goto skip_up_handling;
    }

    if (cachedState == STREAM_INIT) {
        mStreamMutex.unlock();
        status = open();
        if (0 != status) {
            PAL_ERR(LOG_TAG, "stream open failed. status %d", status);
            goto exit;
        }
    } else if (cachedState == STREAM_STARTED) {
        mStreamMutex.unlock();
        status = open();
        if (0 != status) {
            PAL_ERR(LOG_TAG, "stream open failed. status %d", status);
            goto exit;
        }
        rm->unlockActiveStream();
        status = start();
        rm->lockActiveStream();
        if (0 != status) {
            PAL_ERR(LOG_TAG, "stream start failed. status %d", status);
            goto exit;
        }
        /* For scenario when we get SSR down while handling SSR up,
         * status will be 0, so we need to have this additonal check
         * to keep the cached state as STREAM_STARTED.
         */
        if (currentState != STREAM_STARTED) {
            goto exit;
        }
    } else if (cachedState == STREAM_PAUSED) {
        mStreamMutex.unlock();
        status = open();
        if (0 != status) {
            PAL_ERR(LOG_TAG, "stream open failed. status %d", status);
            goto exit;
        }
        rm->unlockActiveStream();
        status = start();
        rm->lockActiveStream();
        if (0 != status) {
            PAL_ERR(LOG_TAG, "stream start failed. status %d", status);
            goto exit;
        }
        if (currentState != STREAM_STARTED)
            goto exit;
        status = pause();
        if (0 != status) {
           PAL_ERR(LOG_TAG, "stream set pause failed. status %d", status);
            goto exit;
        }
    } else {
        mStreamMutex.unlock();
        PAL_ERR(LOG_TAG, "stream not in correct state to handle %d", cachedState);
    }
exit :
    cachedState = STREAM_IDLE;
skip_up_handling :
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t StreamHaptics::isSampleRateSupported(uint32_t sampleRate)
{
    int32_t rc = 0;
    PAL_DBG(LOG_TAG, "sampleRate %u", sampleRate);
    switch(sampleRate) {
        case SAMPLINGRATE_8K:
        case SAMPLINGRATE_16K:
        case SAMPLINGRATE_22K:
        case SAMPLINGRATE_32K:
        case SAMPLINGRATE_44K:
        case SAMPLINGRATE_48K:
        case SAMPLINGRATE_96K:
        case SAMPLINGRATE_192K:
        case SAMPLINGRATE_384K:
            break;
       default:
            rc = 0;
            PAL_VERBOSE(LOG_TAG, "sample rate received %d rc %d", sampleRate, rc);
            break;
    }
    return rc;
}

int32_t StreamHaptics::isChannelSupported(uint32_t numChannels)
{
    int32_t rc = 0;
    PAL_DBG(LOG_TAG, "numChannels %u", numChannels);
    switch(numChannels) {
        case CHANNELS_1:
        case CHANNELS_2:
        case CHANNELS_3:
        case CHANNELS_4:
        case CHANNELS_5:
        case CHANNELS_5_1:
        case CHANNELS_7:
        case CHANNELS_8:
            break;
        default:
            rc = -EINVAL;
            PAL_ERR(LOG_TAG, "channels not supported %d rc %d", numChannels, rc);
            break;
    }
    return rc;
}

int32_t StreamHaptics::isBitWidthSupported(uint32_t bitWidth)
{
    int32_t rc = 0;
    PAL_DBG(LOG_TAG, "bitWidth %u", bitWidth);
    switch(bitWidth) {
        case BITWIDTH_16:
        case BITWIDTH_24:
        case BITWIDTH_32:
            break;
        default:
            rc = -EINVAL;
            PAL_ERR(LOG_TAG, "bit width not supported %d rc %d", bitWidth, rc);
            break;
    }
    return rc;
}

bool StreamHaptics::isStreamSupported(){
    bool result = true;
    int32_t rc = 0;
    struct pal_device hapticsDattr;
    std::shared_ptr<Device> hapticsDev = nullptr;
    std::vector <Stream *> activeStreams;
    struct pal_stream_attributes ActivesAttr;
    Stream *stream = NULL;
    uint16_t channels;
    uint32_t samplerate, bitwidth;

    hapticsDattr.id = PAL_DEVICE_OUT_HAPTICS_DEVICE;
    hapticsDev = Device::getInstance(&hapticsDattr, rm);
    rm->getActiveStream_l(activeStreams, hapticsDev);
    for (int i = 0; i < activeStreams.size(); i++) {
         stream = static_cast<Stream *>(activeStreams[i]);
         stream->getStreamAttributes(&ActivesAttr);
         if (ActivesAttr.info.opt_stream_info.haptics_type == PAL_STREAM_HAPTICS_RINGTONE) {
             PAL_INFO(LOG_TAG, "Ringtone Haptics is Active but allowing Touch Haptics");
        }
    }
    if (mStreamAttr->info.opt_stream_info.haptics_type == PAL_STREAM_HAPTICS_TOUCH) {
        result = true;
    } else {
        channels = mStreamAttr->out_media_config.ch_info.channels;
        samplerate = mStreamAttr->out_media_config.sample_rate;
        bitwidth = mStreamAttr->out_media_config.bit_width;
        rc = (this->isBitWidthSupported(bitwidth) |
              this->isSampleRateSupported(samplerate) |
              this->isChannelSupported(channels));
        if (0 != rc) {
            PAL_ERR(LOG_TAG, "config not supported rc %d", rc);
            result = false;
        }
    }
    exit:
    return result;
}
