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
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: StreamDummy"

#include "StreamDummy.h"
#include "SessionAR.h"
#include "kvh2xml.h"
#include "ResourceManager.h"
#include "Device.h"
#include "PalMappings.h"
#include "PluginManager.h"
#include <unistd.h>
#include <chrono>

extern "C" Stream* CreateDummyStream(const struct pal_stream_attributes *sattr, struct pal_device *dattr,
                               uint32_t instance_id, const std::shared_ptr<ResourceManager> rm) {
    try {
        return new StreamDummy(sattr, dattr, instance_id, rm);
    } catch (const std::exception& e) {
         PAL_ERR(LOG_TAG, "Stream create failed for stream type %s: %s",
                streamNameLUT.at(sattr->type).c_str(), e.what());
        return nullptr;
    }
}

StreamDummy::StreamDummy(const struct pal_stream_attributes *sattr, struct pal_device *dattr,
        uint32_t instance_id, const std::shared_ptr<ResourceManager> rm)
{
    mStreamMutex.lock();
    uint32_t in_channels = 0, out_channels = 0;
    uint32_t attribute_size = 0;
    std::shared_ptr<PluginManager> pm;
    int32_t status;
    void* plugin = nullptr;
    SessionCreate sessionCreate = NULL;

    session = NULL;
    mGainLevel = -1;
    std::shared_ptr<Device> dev = nullptr;
    mStreamAttr = (struct pal_stream_attributes *)nullptr;
    mDevices.clear();
    currentState = STREAM_INIT;
    bool isDeviceConfigUpdated = false;

    PAL_DBG(LOG_TAG, "Enter");

    //TBD handle modifiers later
    mNoOfModifiers = 0; //no_of_modifiers;
    mModifiers = (struct modifier_kv *) (NULL);

    if (!sattr || !dattr) {
        PAL_ERR(LOG_TAG,"invalid arguments");
        mStreamMutex.unlock();
        throw std::runtime_error("invalid arguments");
    }

    attribute_size = sizeof(struct pal_stream_attributes);
    mStreamAttr = (struct pal_stream_attributes *) calloc(1, attribute_size);
    if (!mStreamAttr) {
        PAL_ERR(LOG_TAG, "malloc for stream attributes failed %s", strerror(errno));
        mStreamMutex.unlock();
        throw std::runtime_error("failed to malloc for stream attributes");
    }

    ar_mem_cpy(mStreamAttr, sizeof(pal_stream_attributes), sattr, sizeof(pal_stream_attributes));

    PAL_INFO(LOG_TAG, "Create new DBSession");
    try {
        pm = PluginManager::getInstance();
        if (!pm) {
            PAL_ERR(LOG_TAG, "Unable to get plugin manager instance");
        } else {
            /* Send PAL_STREAM_NON_TUNNEL stream type to force plugin
             * plugin manager to open an AGM session.
             */
            status = pm->openPlugin(PAL_PLUGIN_MANAGER_SESSION,
                                    "PAL_STREAM_DUMMY", plugin);
            if (plugin && !status) {
                sessionCreate = reinterpret_cast<SessionCreate>(plugin);
                session = sessionCreate(rm);
            }
            else {
                PAL_ERR(LOG_TAG, "unable to get session plugin for stream type %s",
                streamNameLUT.at(mStreamAttr->type).c_str());
            }
        }
    } catch (const std::exception& e) {
        PAL_ERR(LOG_TAG, "Session create failed for stream type %s",
            streamNameLUT.at(mStreamAttr->type).c_str());
    }

    mStreamMutex.unlock();

    PAL_DBG(LOG_TAG, "Exit. state %d", currentState);

    return;
}

StreamDummy::~StreamDummy()
{
    if (session) {
        delete session;
        session = nullptr;
        if (Session::pm)
            Session::pm->closePlugin(PAL_PLUGIN_MANAGER_SESSION, "PAL_STREAM_DUMMY");
    }
}
