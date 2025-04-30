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

#include "ACDPlatformInfo.h"

#define LOG_TAG "PAL: ACDPlatformInfo"

ACDContextInfo::ACDContextInfo(uint32_t context_id, uint32_t type) :
    context_id_(context_id),
    context_type_(type)
{
}

ACDSoundModelInfo::ACDSoundModelInfo(ACDStreamConfig *sm_cfg) :
    model_bin_name_(""),
    is_parsing_contexts(false),
    sm_cfg_(sm_cfg)
{
}

void ACDSoundModelInfo::HandleStartTag(const std::string& tag, const char** attribs)
{
    PAL_VERBOSE(LOG_TAG, "Got start tag %s", tag.c_str());

    if (is_parsing_contexts) {
        if (tag == "context") {
            std::string key = attribs[0];
            std::string value = attribs[1];
            uint32_t id;

            if (key == "id") {
                id = ResourceManager::convertCharToHex(value);
                std::shared_ptr<ACDContextInfo> context_info =
                    std::make_shared<ACDContextInfo>(id, model_id_);
                acd_context_info_list_.push_back(context_info);
                sm_cfg_->UpdateContextModelMap(id);
            }
        }
    }

    if (tag == "contexts")
        is_parsing_contexts = true;
}

void ACDSoundModelInfo::HandleEndTag(struct xml_userdata *data, const std::string& tag)
{
    PAL_VERBOSE(LOG_TAG, "Got end tag %s", tag.c_str());

    if (tag == "contexts")
        is_parsing_contexts = false;

    if (data->offs <= 0)
        return;

    data->data_buf[data->offs] = '\0';

    if (tag == "name") {
        std::string type(data->data_buf);

        if (type.find("ACD_SOUND_MODEL") == std::string::npos) {
            PAL_ERR(LOG_TAG, "Error: invalid sound model: %s", type.c_str());
        } else {
            model_type_ = type;
            model_id_ = sm_cfg_->GetAndUpdateSndMdlCnt();
            PAL_VERBOSE(LOG_TAG, "Sound model type: %s\t\tid: %d\n",
                        model_type_.c_str(), model_id_);
        }
    } else if (tag == "bin") {
        std::string bin_name(data->data_buf);
        model_bin_name_ = bin_name;
    } else if (tag == "uuid") {
        std::string uuid(data->data_buf);
        model_uuid_ = ResourceManager::convertCharToHex(uuid);
    }

    return;
}

ACDStreamConfig::ACDStreamConfig() :
    lpi_enable_(true),
    curr_child_(nullptr),
    sound_model_cnt(0)
{
}

void ACDStreamConfig::UpdateContextModelMap(uint32_t context_id)
{
     std::shared_ptr<ACDSoundModelInfo> sm_info(
            std::static_pointer_cast<ACDSoundModelInfo>(curr_child_));
     context_model_map_.insert(std::make_pair(context_id, sm_info));
}

std::shared_ptr<ACDSoundModelInfo> ACDStreamConfig::GetSoundModelInfoByContextId(uint32_t context_id)
{
    auto contextModel = context_model_map_.find(context_id);

    if (contextModel != context_model_map_.end())
        return contextModel->second;
    else
        return nullptr;
}

std::shared_ptr<ACDSoundModelInfo> ACDStreamConfig::GetSoundModelInfoByModelId(uint32_t model_id)
{
    auto modelInfo = acd_modelinfo_map_.find(model_id);

    if (modelInfo != acd_modelinfo_map_.end())
        return modelInfo->second;
    else
        return nullptr;
}

int32_t ACDStreamConfig::GetOperatingMode(const std::string& tag) {

    int32_t mode = -1;

    if (tag == "low_power") {
        mode = ST_OPERATING_MODE_LOW_POWER;
    } else if (tag == "low_power_ns") {
        mode = ST_OPERATING_MODE_LOW_POWER_NS;
    } else if (tag == "low_power_tx_macro") {
        mode = ST_OPERATING_MODE_LOW_POWER_TX_MACRO;
    } else if (tag == "high_performance") {
        mode = ST_OPERATING_MODE_HIGH_PERF;
    } else if (tag == "high_performance_ns") {
        mode = ST_OPERATING_MODE_HIGH_PERF_NS;
    } else if (tag == "high_performance_tx_macro") {
        mode = ST_OPERATING_MODE_HIGH_PERF_TX_MACRO;
    } else {
        PAL_ERR(LOG_TAG, "Invalid operating mode %s", tag.c_str());
    }

    return mode;
}

void ACDStreamConfig::HandleStartTag(const std::string& tag, const char** attribs)
{
    PAL_VERBOSE(LOG_TAG, "Got start tag %s", tag.c_str());

    /* Delegate to child element if currently active */
    if (curr_child_) {
        curr_child_->HandleStartTag(tag, attribs);
        return;
    }

    if (tag == "sound_model") {
        curr_child_ = std::static_pointer_cast<SoundTriggerXml>(
            std::make_shared<ACDSoundModelInfo>(this));
        return;
    }

    if (tag == "operating_modes" ||
        tag == "sound_model_info" ||
        tag == "name") {
        PAL_VERBOSE(LOG_TAG, "tag:%s appeared, nothing to do", tag.c_str());
        return;
    }

    std::string key = attribs[0];
    if (tag == "param") {
        std::string value = attribs[1];

        if (key == "vendor_uuid") {
            UUID::StringToUUID(value, vendor_uuid_);
        } else if (key == "sample_rate") {
            sample_rate_ = std::stoi(value);
        } else if (key == "bit_width") {
            bit_width_ = std::stoi(value);
        } else if (key == "out_channels") {
            if (std::stoi(value) <= MAX_MODULE_CHANNELS)
                out_channels_ = std::stoi(value);
        } else if (key == "lpi_enable") {
            lpi_enable_ = (value == "true");
        } else {
            PAL_ERR(LOG_TAG, "Invalid attribute %s", key.c_str());
        }
    } else {
        int32_t mode = GetOperatingMode(tag);
        if (mode != -1) {
            std::shared_ptr<SoundTriggerPlatformInfo> st_info =
                                                 SoundTriggerPlatformInfo::GetInstance();
            st_info->ReadCapProfileNames((StOperatingModes)mode, attribs, acd_op_modes_);
        } else {
            PAL_ERR(LOG_TAG, "Invalid tag %s", tag.c_str());
        }
    }
}

void ACDStreamConfig::HandleEndTag(struct xml_userdata *data, const std::string& tag)
{
    PAL_VERBOSE(LOG_TAG, "Got end tag %s", tag.c_str());

    if (tag == "sound_model") {
       std::shared_ptr<ACDSoundModelInfo> acd_sm_info(
            std::static_pointer_cast<ACDSoundModelInfo>(curr_child_));
        acd_soundmodel_info_list_.push_back(acd_sm_info);
        acd_modelinfo_map_.insert(std::make_pair(acd_sm_info->GetModelId(), acd_sm_info));
        curr_child_ = nullptr;
    }

    if (curr_child_) {
        curr_child_->HandleEndTag(data, tag);
        return;
    }

    if (tag == "name") {
        if (data->offs <= 0)
            return;
        data->data_buf[data->offs] = '\0';

        std::string name(data->data_buf);
        name_ = name;
    }

    return;
}

std::shared_ptr<ACDPlatformInfo> ACDPlatformInfo::me_ = nullptr;

ACDPlatformInfo::ACDPlatformInfo() :
    acd_enable_(true),
    curr_child_(nullptr)
{
}

std::shared_ptr<ACDStreamConfig> ACDPlatformInfo::GetStreamConfig(const UUID& uuid) const
{
    auto streamCfg = acd_cfg_list_.find(uuid);

    if (streamCfg != acd_cfg_list_.end())
        return streamCfg->second;
    else
        return nullptr;
}

std::shared_ptr<ACDPlatformInfo> ACDPlatformInfo::GetInstance()
{
    if (!me_)
        me_ = std::shared_ptr<ACDPlatformInfo> (new ACDPlatformInfo);

    return me_;
}

void ACDPlatformInfo::HandleStartTag(const std::string& tag, const char** attribs)
{
    /* Delegate to child element if currently active */
    if (curr_child_) {
        curr_child_->HandleStartTag(tag, attribs);
        return;
    }

    PAL_VERBOSE(LOG_TAG, "Got start tag %s", tag.c_str());

    if (tag == "stream_config") {
        curr_child_ = std::static_pointer_cast<SoundTriggerXml>(
            std::make_shared<ACDStreamConfig>());
        return;
    }

    if (tag == "config") {
        PAL_VERBOSE(LOG_TAG, "tag:%s appeared, nothing to do", tag.c_str());
        return;
    }

    if (tag == "param") {
        std::string key = attribs[0];
        std::string value = attribs[1];
        if (key == "acd_enable") {
            acd_enable_ = (value == "true");
        } else {
            PAL_ERR(LOG_TAG, "Invalid attribute %s", key.c_str());
        }
    } else {
        PAL_ERR(LOG_TAG, "Invalid tag %s", tag.c_str());
    }
}

void ACDPlatformInfo::HandleEndTag(struct xml_userdata *data, const std::string& tag)
{
    PAL_VERBOSE(LOG_TAG, "Got end tag %s", tag.c_str());

    if (tag == "stream_config") {
        std::shared_ptr<ACDStreamConfig> acd_cfg(
            std::static_pointer_cast<ACDStreamConfig>(curr_child_));
        const auto res = acd_cfg_list_.insert(
            std::make_pair(acd_cfg->GetUUID(), acd_cfg));

        if (!res.second)
            PAL_ERR(LOG_TAG, "Error:%d Failed to insert to map", -EINVAL);
        curr_child_ = nullptr;
    }

    if (curr_child_)
        curr_child_->HandleEndTag(data, tag);

    return;
}
