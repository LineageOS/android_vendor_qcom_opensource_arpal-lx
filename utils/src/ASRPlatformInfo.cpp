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

#include "ASRPlatformInfo.h"
#include "asr_module_calibration_api.h"

#define LOG_TAG "PAL: ASRPlatformInfo"


void ASRCommonConfig::HandleStartTag(const std::string& tag, const char** attribs)
{
    PAL_VERBOSE(LOG_TAG, "Start tag %s", tag.c_str());
    std::string key = attribs[0];
    std::string value = attribs[1];

    if (tag == "param") {
        if (key == "asr_input_buffer_size") {
            input_buffer_size_ = std::stoi(value);
        } else if (key == "asr_input_buffer_size_partial_mode") {
            partial_mode_input_buffer_size_ = std::stoi(value);
        } else if (key == "buffering_mode_out_buf_size") {
            buffering_mode_out_buffer_size_ = std::stoi(value);
        } else if (key == "partial_mode_in_lpi") {
            partial_mode_in_lpi_ = (value == "true");
        } else if (key == "sdz_output_buffer_size") {
            sdz_output_buffer_size_ = std::stoi(value);
        } else {
            PAL_ERR(LOG_TAG, "Invalid attribute %s", key.c_str());
        }
    }
}

void ASRCommonConfig::HandleEndTag(struct xml_userdata *data, const std::string& tag)
{
    PAL_VERBOSE(LOG_TAG, "Got end tag %s, nothing to handle here.", tag.c_str());

    return;
}

ASRCommonConfig::ASRCommonConfig():
    partial_mode_in_lpi_(false),
    input_buffer_size_(0),
    partial_mode_input_buffer_size_(0),
    buffering_mode_out_buffer_size_(0),
    sdz_output_buffer_size_(0)
{
}

uint32_t ASRCommonConfig::GetOutputBufferSize(int mode) {

    if (mode != NON_BUFFERED && mode != TS_NON_BUFFERED)
        return GetBufferingModeOutBufferSize();

    return OUT_BUF_SIZE_DEFAULT;
}

uint32_t ASRCommonConfig::GetInputBufferSize(int mode) {

    if (mode == BUFFERED || mode == TS_BUFFERED)
        return GetInputBufferSize();

    return GetPartialModeInputBufferSize();

}

int32_t ASRStreamConfig::GetIndex(std::string& param_name) {

    int32_t index = -1;

    if (param_name == "asr_input_config_id") {
        index = ASR_INPUT_CONFIG;
    } else if (param_name == "asr_output_config_id") {
        index = ASR_OUTPUT_CONFIG;
    } else if (param_name == "asr_input_buffer_duration_id") {
       index = ASR_INPUT_BUF_DURATON;
    } else if (param_name == "asr_output_id") {
       index = ASR_OUTPUT;
    } else if (param_name == "asr_force_output_id") {
       index = ASR_FORCE_OUTPUT;
    } else if (param_name == "sdz_enable_id") {
       index = SDZ_ENABLE;
    } else if (param_name == "sdz_output_config_id") {
       index = SDZ_OUTPUT_CONFIG;
    } else if (param_name == "sdz_input_buffer_duration_id") {
       index = SDZ_INPUT_BUF_DURATION;
    } else if (param_name == "sdz_output_id") {
       index = SDZ_OUTPUT;
    } else if (param_name == "sdz_force_output_id") {
       index = SDZ_FORCE_OUTPUT;
    } else {
       PAL_ERR(LOG_TAG, "Invalid attribute %s", param_name.c_str());
    }

    return index;
}

void ASRStreamConfig::HandleStartTag(const std::string& tag, const char** attribs)
{
    PAL_VERBOSE(LOG_TAG, "Got start tag %s", tag.c_str());

    if (tag == "operating_modes" || tag == "module_Info" || tag == "name") {
        PAL_DBG(LOG_TAG, "tag:%s appeared, nothing to do", tag.c_str());
        return;
    }

    std::string key = attribs[0];
    std::string value = attribs[1];
    if (tag == "param") {

        if (key == "vendor_uuid") {
            UUID::StringToUUID(value.c_str(), vendor_uuid_);
        } else if (key == "lpi_enable") {
            lpi_enable_ = (value == "true");
        } else {
            int index = GetIndex(key);
            if (index == -1) {
                PAL_ERR(LOG_TAG, "Invalid attribute %s", key.c_str());
                return;
            }
            sscanf(value.c_str(), "%x, %x", &module_tag_ids_[index], &param_ids_[index]);
            PAL_DBG(LOG_TAG, "index : %u, module_id : %x, param : %x",
                        index, module_tag_ids_[index], param_ids_[index]);
        }
    } else {
        std::shared_ptr<SoundTriggerPlatformInfo> st_info = SoundTriggerPlatformInfo::GetInstance();
        if (tag == "low_power") {
            st_info->ReadCapProfileNames(ST_OPERATING_MODE_LOW_POWER, attribs, asr_op_modes_);
        } else if (tag == "high_performance") {
            st_info->ReadCapProfileNames(ST_OPERATING_MODE_HIGH_PERF, attribs, asr_op_modes_);
        }
    }
}

void ASRStreamConfig::HandleEndTag(struct xml_userdata *data, const std::string& tag)
{
    PAL_VERBOSE(LOG_TAG, "Got end tag %s", tag.c_str());

    if (tag == "module_Info" || tag == "operating_modes") {
        PAL_VERBOSE(LOG_TAG, "Exit, Nothing to do for this %s tag.", tag.c_str());
    }

    if (tag == "name") {
        if (data->offs <= 0)
            return;
        data->data_buf[data->offs] = '\0';

        std::string name(data->data_buf);
        name_ = name;
    }

    PAL_VERBOSE(LOG_TAG, "Exit, for %s tag.", tag.c_str());

    return;
}

std::shared_ptr<ASRPlatformInfo> ASRPlatformInfo::me_ = nullptr;

ASRStreamConfig::ASRStreamConfig() :
    lpi_enable_(true),
    curr_child_(nullptr)
{
    for (int i = 0; i < ASR_MAX_PARAM_IDS; i++) {
        module_tag_ids_[i] = 0;
        param_ids_[i] = 0;
    }
}

ASRPlatformInfo::ASRPlatformInfo() :
    curr_child_(nullptr),
    cm_cfg_(nullptr)
{
}

std::shared_ptr<ASRPlatformInfo> ASRPlatformInfo::GetInstance()
{
    if (!me_)
        me_ = std::shared_ptr<ASRPlatformInfo> (new ASRPlatformInfo);

    return me_;
}

std::shared_ptr<ASRStreamConfig> ASRPlatformInfo::GetStreamConfig(const UUID& uuid) const
{
    auto smCfg = stream_cfg_list_.find(uuid);

    if (smCfg != stream_cfg_list_.end())
        return smCfg->second;
    else
        return nullptr;
}

void ASRPlatformInfo::HandleStartTag(const std::string& tag, const char** attribs)
{
    PAL_VERBOSE(LOG_TAG, "Got start tag %s", tag.c_str());

    /* Delegate to child element if currently active */
    if (curr_child_) {
        curr_child_->HandleStartTag(tag, attribs);
        return;
    }

    if (tag == "stream_config") {
        curr_child_ = std::static_pointer_cast<SoundTriggerXml>(
            std::make_shared<ASRStreamConfig>());
        return;
    } else if (tag == "common_config") {
        curr_child_ = std::static_pointer_cast<SoundTriggerXml>(
                           std::make_shared<ASRCommonConfig>());
        return;
    } else {
        PAL_ERR(LOG_TAG, "Invalid tag %s", tag.c_str());
    }

    PAL_VERBOSE(LOG_TAG, "Exit for tag %s.", tag.c_str());
}

void ASRPlatformInfo::HandleEndTag(struct xml_userdata *data, const std::string& tag)
{
    PAL_VERBOSE(LOG_TAG, "Got end tag %s", tag.c_str());

    if (tag == "stream_config") {
        std::shared_ptr<ASRStreamConfig> sm_cfg(
            std::static_pointer_cast<ASRStreamConfig>(curr_child_));
        const auto res = stream_cfg_list_.insert(
            std::make_pair(sm_cfg->GetUUID(), sm_cfg));
        if (!res.second)
            PAL_ERR(LOG_TAG, "Failed to insert to map");
        curr_child_ = nullptr;
    } else if (tag == "common_config") {
        std::shared_ptr<ASRCommonConfig> cm_cfg(
             std::static_pointer_cast<ASRCommonConfig>(curr_child_));
        cm_cfg_ = cm_cfg;
        curr_child_ = nullptr;
    }

    if (curr_child_)
        curr_child_->HandleEndTag(data, tag);

    PAL_VERBOSE(LOG_TAG, "Exit for tag %s.", tag.c_str());

    return;
}
