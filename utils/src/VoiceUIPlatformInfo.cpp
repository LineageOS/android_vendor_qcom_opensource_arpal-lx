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

#include "VoiceUIPlatformInfo.h"
#include "PalCommon.h"
#include "STUtils.h"
#ifdef FEATURE_IPQ_OPENWRT
#include <algorithm>
#endif

#define LOG_TAG "PAL: VoiceUIPlatformInfo"

static const struct st_uuid qcva_uuid =
    { 0x68ab2d40, 0xe860, 0x11e3, 0x95ef, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } };

VUISecondStageConfig::VUISecondStageConfig() :
    detection_type_(ST_SM_TYPE_NONE),
    sm_id_(0),
    module_lib_(""),
    sample_rate_(16000),
    bit_width_(16),
    channels_(1),
    proc_frame_size_(120)
{
}

int32_t VUISecondStageConfig::GetDetectionType(const std::string& tag) {

    int32_t type = -1;

    if (tag == "KEYWORD_DETECTION") {
        type = ST_SM_TYPE_KEYWORD_DETECTION;
    } else if (tag == "USER_VERIFICATION") {
        type = ST_SM_TYPE_USER_VERIFICATION;
    } else if (tag == "CUSTOM_DETECTION") {
        type = ST_SM_TYPE_CUSTOM_DETECTION;
    } else {
        PAL_ERR(LOG_TAG, "Invalid detection type %s", tag.c_str());
    }
    return type;
}

void VUISecondStageConfig::HandleStartTag(const std::string& tag, const char **attribs)
{
    PAL_VERBOSE(LOG_TAG, "Got start tag %s", tag.c_str());

    if (tag == "param") {
        std::string key = attribs[0];
        std::string value = attribs[1];
        if (key ==  "sm_detection_type") {
            int32_t type = GetDetectionType(value);
            if (type == -1) {
                PAL_ERR(LOG_TAG, "Invalid detection type %s", value.c_str());
                return;
            }
            detection_type_ = (st_sound_model_type)type;
        } else if (key == "sm_id") {
            sm_id_ = std::strtoul(value.c_str(), nullptr, 16);
        } else if (key == "module_lib") {
            module_lib_ = value;
        } else if (key == "sample_rate") {
            sample_rate_ = std::stoi(value);
        } else if (key == "bit_width") {
            bit_width_ = std::stoi(value);
        } else if (key == "channel_count") {
            channels_ = std::stoi(value);
        } else if (key == "proc_frame_size") {
            proc_frame_size_ = std::stoi(value);
        }
    } else {
        PAL_ERR(LOG_TAG, "Invalid tag %s", tag.c_str());
    }
}

VUIFirstStageConfig::VUIFirstStageConfig() :
    lpi_supported_(true),
    module_type_(ST_MODULE_TYPE_GMM),
    module_name_("GMM")
{
    for (int i = 0; i < MAX_PARAM_IDS; i++) {
        module_tag_ids_[i] = 0;
        param_ids_[i] = 0;
    }
}

int32_t VUIFirstStageConfig::GetIndex(std::string param_name) {

    int32_t index = -1;

    if (param_name == "load_sound_model_ids") {
        index = LOAD_SOUND_MODEL;
    } else if (param_name == "unload_sound_model_ids") {
        index = UNLOAD_SOUND_MODEL;
    } else if (param_name == "wakeup_config_ids") {
        index = WAKEUP_CONFIG;
    } else if (param_name == "buffering_config_ids") {
        index = BUFFERING_CONFIG;
    } else if (param_name == "engine_reset_ids") {
        index = ENGINE_RESET;
    } else if (param_name == "custom_config_ids") {
        index = CUSTOM_CONFIG;
    } else if (param_name == "version_ids") {
        index = MODULE_VERSION;
    } else if (param_name == "engine_per_model_reset_ids") {
        index = ENGINE_PER_MODEL_RESET;
    } else if (param_name == "mode_bit_config_ids") {
        index = MMA_MODE_BIT_CONFIG;
    } else if (param_name == "trigger_detection_ids") {
        index = TRIGGER_DETECTION_CONFIG;
    } else if (param_name == "buffering_mode_ids") {
        index = BUFFERING_MODE_CONFIG;
    } else {
        PAL_ERR(LOG_TAG, "Invalid param name %s", param_name.c_str());
    }
    return index;
}

int32_t VUIFirstStageConfig::GetModuleType(std::string tag) {

    int32_t type = -1;

    if (tag == "GMM") {
        type = ST_MODULE_TYPE_GMM;
    } else if (tag == "PDK") {
        type = ST_MODULE_TYPE_PDK;
    } else if (tag == "HOTWORD") {
        type = ST_MODULE_TYPE_HW;
    } else if (tag == "CUSTOM1") {
        type = ST_MODULE_TYPE_CUSTOM_1;
    } else if (tag == "CUSTOM2") {
        type = ST_MODULE_TYPE_CUSTOM_2;
    } else if (tag == "MMA") {
        type = ST_MODULE_TYPE_MMA;
    } else if (tag == "HIST_CAP") {
        type = ST_MODULE_TYPE_HIST_CAP;
    } else {
        PAL_ERR(LOG_TAG, "Invalid module type %s", tag.c_str());
    }
    return type;
}

void VUIFirstStageConfig::HandleStartTag(const std::string& tag, const char **attribs)
{
    PAL_VERBOSE(LOG_TAG, "Got start tag %s", tag.c_str());
    std::string key = attribs[0];
    std::string value = attribs[1];

    if (tag == "param") {
        if (key == "module_type") {
            module_name_ = value;
            int32_t type = GetModuleType(value);
            if (type == -1) {
                PAL_ERR(LOG_TAG, "Invalid module type %s", value.c_str());
                return;
            }
            module_type_ = (st_module_type_t)type;
            PAL_VERBOSE(LOG_TAG, "Module name:%s, type:%d",
                    module_name_.c_str(), module_type_);
        } else if (key == "lpi_supported") {
            lpi_supported_ = (value == "true");
        } else {
            uint32_t index = GetIndex(key);
            if (index == -1) {
                PAL_ERR(LOG_TAG, "Invalid param name %s", key.c_str());
                return;
            }
            sscanf(value.c_str(), "%x, %x", &module_tag_ids_[index],
                   &param_ids_[index]);
            PAL_VERBOSE(LOG_TAG, "index : %u, module_id : %x, param : %x",
                    index, module_tag_ids_[index], param_ids_[index]);
        }
    } else {
        PAL_ERR(LOG_TAG, "Invalid tag %s", tag.c_str());
    }
}

VUIStreamConfig::VUIStreamConfig() :
    vui_intf_plugin_lib_name_(""),
    is_qcva_uuid_(false),
    merge_first_stage_sound_models_(false),
    capture_keyword_(2000),
    client_capture_read_delay_(2000),
    pre_roll_duration_(0),
    supported_first_stage_engine_count_(1),
    enable_intra_concurrent_detection_(false),
    lpi_enable_(true),
    batch_size_in_ms_(0),
    curr_child_(nullptr),
    enable_amd_(false)
{
    ext_det_prop_list_.clear();
}

/*
 * Below functions GetVUIFirstStageConfig(), GetVUIModuleType(),
 * and GetVUIModuleName() are to be used only for getting module
 * info and module type/name for third party or custom sound model
 * engines. It assumes only one module type per vendor UUID.
 */
std::shared_ptr<VUIFirstStageConfig> VUIStreamConfig::GetVUIFirstStageConfig()
{
    auto smCfg = vui_uuid_1st_stage_cfg_list_.find(vendor_uuid_);

    if(smCfg != vui_uuid_1st_stage_cfg_list_.end())
         return smCfg->second;
    else
        return nullptr;
}

st_module_type_t VUIStreamConfig::GetVUIModuleType()
{
    std::shared_ptr<VUIFirstStageConfig> sTModuleInfo = GetVUIFirstStageConfig();

    if (sTModuleInfo != nullptr)
        return sTModuleInfo->GetModuleType();
    else
        return ST_MODULE_TYPE_NONE;
}

std::string VUIStreamConfig::GetVUIModuleName()
{
    std::shared_ptr<VUIFirstStageConfig> sTModuleInfo = GetVUIFirstStageConfig();

    if (sTModuleInfo != nullptr)
        return sTModuleInfo->GetModuleName();
    else
        return std::string();
}

std::shared_ptr<VUISecondStageConfig> VUIStreamConfig::GetVUISecondStageConfig(
    const listen_model_indicator_enum& sm_type) const
{
    uint32_t sm_id = static_cast<uint32_t>(sm_type);
    auto ss_config = vui_2nd_stage_cfg_list_.find(sm_id);

    if (ss_config != vui_2nd_stage_cfg_list_.end())
        return ss_config->second;
    else
        return nullptr;
}

std::shared_ptr<VUIFirstStageConfig> VUIStreamConfig::GetVUIFirstStageConfig(const uint32_t type) const
{
    uint32_t module_type = type;

    PAL_VERBOSE(LOG_TAG, "search module for model type %u", type);
    if (IS_MODULE_TYPE_PDK(type)) {
        PAL_VERBOSE(LOG_TAG, "PDK module");
        module_type = ST_MODULE_TYPE_PDK;
    }

    auto module_config = vui_1st_stage_cfg_list_.find(module_type);
    if (module_config != vui_1st_stage_cfg_list_.end())
        return module_config->second;
    else
        return nullptr;
}

void VUIStreamConfig::GetDetectionPropertyList(
    std::vector<uint32_t> &list) {

    for (int i = 0; i < ext_det_prop_list_.size(); i++)
        list.push_back(ext_det_prop_list_[i]);
}

void VUIStreamConfig::ReadDetectionPropertyList(const char *prop_string)
{
    int ret = 0;
    char *token = nullptr;
    char delims[] = ",";
    char *save = nullptr;

    PAL_VERBOSE(LOG_TAG, "Detection property list %s", prop_string);

    token = strtok_r(const_cast<char *>(prop_string), delims, &save);
    while (token) {
        ext_det_prop_list_.push_back(std::strtoul(token, nullptr, 16));
        token = strtok_r(NULL, delims, &save);
    }

    for (int i = 0; i < ext_det_prop_list_.size(); i++) {
        PAL_VERBOSE(LOG_TAG, "Found extension detection property 0x%x",
            ext_det_prop_list_[i]);
    }
}

bool VUIStreamConfig::IsDetPropSupported(uint32_t prop) const {

    auto iter =
        std::find(ext_det_prop_list_.begin(), ext_det_prop_list_.end(), prop);

    return iter != ext_det_prop_list_.end();
}
int32_t VUIStreamConfig::GetOperatingMode(std::string tag) {

    int32_t mode = -1;

    if (tag == "low_power") {
       mode = ST_OPERATING_MODE_LOW_POWER;
    } else if (tag == "high_performance") {
       mode = ST_OPERATING_MODE_HIGH_PERF;
    } else if (tag == "high_performance_and_charging") {
        mode = ST_OPERATING_MODE_HIGH_PERF_AND_CHARGING;
    } else {
        PAL_ERR(LOG_TAG, "Invalid tag %s", tag.c_str());
    }

    return mode;
}
void VUIStreamConfig::HandleStartTag(const std::string& tag, const char** attribs)
{
    /* Delegate to child element if currently active */
    if (curr_child_) {
        curr_child_->HandleStartTag(tag, attribs);
        return;
    }

    PAL_VERBOSE(LOG_TAG, "Got start tag %s", tag.c_str());
    if (tag == "operating_modes" ||
        tag == "sound_model_info" ||
        tag == "name") {
        PAL_VERBOSE(LOG_TAG, "tag:%s appeared, nothing to do", tag.c_str());
        return;
    }

    if (tag == "first_stage_module_params") {
        auto st_module_info_ = std::make_shared<VUIFirstStageConfig>();
        vui_uuid_1st_stage_cfg_list_.insert(std::make_pair(vendor_uuid_, st_module_info_));
        curr_child_ = std::static_pointer_cast<SoundTriggerXml>(st_module_info_);
        return;
    }

    if (tag == "arm_ss_module_params") {
        curr_child_ = std::static_pointer_cast<SoundTriggerXml>(
            std::make_shared<VUISecondStageConfig>());
        return;
    }

    if (tag == "param") {
        std::string key = attribs[0];
        std::string value = attribs[1];
        if (key == "vendor_uuid") {
            UUID::StringToUUID(value, vendor_uuid_);
            if (vendor_uuid_.CompareUUID(qcva_uuid))
                is_qcva_uuid_ = true;
        } else if (key == "lpi_enable") {
            lpi_enable_ = (value == "true");
        } else if (key == "interface_plugin_lib") {
            vui_intf_plugin_lib_name_ = value;
        } else if (key == "get_module_version") {
            get_module_version_supported_ = (value == "true");
        } else if (key == "merge_first_stage_sound_models") {
            merge_first_stage_sound_models_ = (value == "true");
        } else if (key == "pdk_first_stage_max_engine_count") {
            supported_first_stage_engine_count_ = std::stoi(value);
        } else if (key == "enable_intra_va_engine_concurrent_detection") {
            enable_intra_concurrent_detection_ = (value == "true");
        } else if (key == "capture_keyword") {
            capture_keyword_ = std::stoi(value);
        } else if (key == "client_capture_read_delay") {
            client_capture_read_delay_ = std::stoi(value);
        } else if (key == "pre_roll_duration") {
            pre_roll_duration_ = std::stoi(value);
        } else if (key == "kw_start_tolerance") {
            kw_start_tolerance_ = std::stoi(value);
        } else if (key == "kw_end_tolerance") {
            kw_end_tolerance_ = std::stoi(value);
        } else if (key == "data_before_kw_start") {
            data_before_kw_start_ = std::stoi(value);
        } else if (key == "data_after_kw_end") {
            data_after_kw_end_ = std::stoi(value);
        } else if (key == "sample_rate") {
            sample_rate_ = std::stoi(value);
        } else if (key == "bit_width") {
            bit_width_ = std::stoi(value);
        } else if (key == "out_channels") {
            if (std::stoi(value) <= MAX_MODULE_CHANNELS)
                out_channels_ = std::stoi(value);
        } else if (key == "detection_property_list") {
            ReadDetectionPropertyList(value.c_str());
        } else if (key == "batch_size_in_ms") {
            batch_size_in_ms_ = std::stoi(value);
        } else if (key == "enable_amd") {
            enable_amd_ = (value == "true");
        } else {
            PAL_ERR(LOG_TAG, "Invalid attribute %s", key.c_str());
       }
    } else {
        std::shared_ptr<SoundTriggerPlatformInfo> st_info = SoundTriggerPlatformInfo::GetInstance();
        int32_t mode = GetOperatingMode(tag);
        if (mode != -1) {
            std::shared_ptr<SoundTriggerPlatformInfo> st_info =
                                                 SoundTriggerPlatformInfo::GetInstance();
            st_info->ReadCapProfileNames((StOperatingModes)mode, attribs, vui_op_modes_);
        } else {
            PAL_ERR(LOG_TAG, "Invalid operating mode %s", tag.c_str());
        }
    }
}

void VUIStreamConfig::HandleEndTag(struct xml_userdata *data, const std::string& tag)
{
    PAL_VERBOSE(LOG_TAG, "Got end tag %s", tag.c_str());

    if (tag == "first_stage_module_params") {
        std::shared_ptr<VUIFirstStageConfig> st_module_info(
            std::static_pointer_cast<VUIFirstStageConfig>(curr_child_));
        const auto res = vui_1st_stage_cfg_list_.insert(
            std::make_pair(st_module_info->GetModuleType(), st_module_info));

        if (!res.second)
            PAL_ERR(LOG_TAG, "Failed to insert to map");
        curr_child_ = nullptr;
    } else if (tag == "arm_ss_module_params") {
        std::shared_ptr<VUISecondStageConfig> ss_cfg(
            std::static_pointer_cast<VUISecondStageConfig>(curr_child_));
        const auto res = vui_2nd_stage_cfg_list_.insert(
            std::make_pair(ss_cfg->GetSoundModelID(), ss_cfg));

        if (!res.second)
            PAL_ERR(LOG_TAG, "Failed to insert to map");
        curr_child_ = nullptr;
    }

    if (curr_child_)
        curr_child_->HandleEndTag(data, tag);

    return;
}

std::shared_ptr<VoiceUIPlatformInfo> VoiceUIPlatformInfo::me_ = nullptr;

VoiceUIPlatformInfo::VoiceUIPlatformInfo() :
    enable_failure_detection_(false),
    transit_to_non_lpi_on_charging_(false),
    notify_second_stage_failure_(false),
    enable_inter_concurrent_detection_(true),
    mmap_enable_(false),
    mmap_buffer_duration_(0),
    mmap_frame_length_(0),
    sound_model_lib_("liblistensoundmodel2vendor.so"),
    curr_child_(nullptr)
{
}

std::shared_ptr<VUIStreamConfig> VoiceUIPlatformInfo::GetStreamConfig(const UUID& uuid) const
{
    auto smCfg = stream_cfg_list_.find(uuid);

    if (smCfg != stream_cfg_list_.end())
        return smCfg->second;
    else
        return nullptr;
}

// We can assume only Hotword sm config supports version api
void VoiceUIPlatformInfo::GetStreamConfigForVersionQuery(
    std::vector<std::shared_ptr<VUIStreamConfig>> &sm_cfg_list) const
{
    std::shared_ptr<VUIStreamConfig> sm_cfg = nullptr;

    for (auto iter = stream_cfg_list_.begin();
         iter != stream_cfg_list_.end(); iter++) {
        sm_cfg = iter->second;
        if (sm_cfg && sm_cfg->GetModuleVersionSupported())
            sm_cfg_list.push_back(sm_cfg);
    }
}

std::shared_ptr<VoiceUIPlatformInfo> VoiceUIPlatformInfo::GetInstance()
{
    if (!me_)
        me_ = std::shared_ptr<VoiceUIPlatformInfo> (new VoiceUIPlatformInfo);

    return me_;
}

void VoiceUIPlatformInfo::HandleStartTag(const std::string& tag, const char** attribs)
{
    /* Delegate to child element if currently active */
    if (curr_child_) {
        curr_child_->HandleStartTag(tag, attribs);
        return;
    }

    PAL_VERBOSE(LOG_TAG, "Got start tag %s", tag.c_str());

    if (tag == "stream_config") {
        curr_child_ = std::static_pointer_cast<SoundTriggerXml>(
            std::make_shared<VUIStreamConfig>());
        return;
    }

    if (tag == "config") {
        PAL_VERBOSE(LOG_TAG, "tag:%s appeared, nothing to do", tag.c_str());
        return;
    }

    if (tag == "param") {
        std::string key = attribs[0];
        std::string value = attribs[1];
        if (key == "version") {
            vui_version_ = std::strtoul(value.c_str(), nullptr, 16);
        } else if (key == "enable_failure_detection") {
            enable_failure_detection_ = (value == "true");
        } else if (key == "transit_to_non_lpi_on_charging") {
            transit_to_non_lpi_on_charging_ = (value == "true");
        } else if (key == "notify_second_stage_failure") {
            notify_second_stage_failure_ = (value == "true");
        } else if (key == "enable_inter_va_engine_concurrent_detection") {
            enable_inter_concurrent_detection_ = (value == "true");
        } else if (key == "mmap_enable") {
            mmap_enable_ = (value == "true");
        } else if (key == "mmap_buffer_duration") {
            mmap_buffer_duration_ = std::stoi(value);
        } else if (key == "mmap_frame_length") {
            mmap_frame_length_ = std::stoi(value);
        } else if (key == "sound_model_lib") {
            sound_model_lib_ = value;
        } else {
            PAL_ERR(LOG_TAG, "Invalid attribute %s", key.c_str());
        }
    } else {
        PAL_ERR(LOG_TAG, "Invalid tag %s", tag.c_str());
    }
}

void VoiceUIPlatformInfo::HandleEndTag(struct xml_userdata *data, const std::string& tag)
{
    PAL_VERBOSE(LOG_TAG, "Got end tag %s", tag.c_str());

    if (tag == "stream_config") {
        std::shared_ptr<VUIStreamConfig> sm_cfg(
            std::static_pointer_cast<VUIStreamConfig>(curr_child_));
        const auto res = stream_cfg_list_.insert(
            std::make_pair(sm_cfg->GetUUID(), sm_cfg));

        if (!res.second)
            PAL_ERR(LOG_TAG, "Failed to insert to map");
        curr_child_ = nullptr;
    }

    if (curr_child_)
        curr_child_->HandleEndTag(data, tag);

    return;
}
