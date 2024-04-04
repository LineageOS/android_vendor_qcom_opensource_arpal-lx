/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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
 * Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
 *
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <dlfcn.h>
#include <errno.h>

#ifdef VUI_USE_SYSLOG
#include <stdint.h>
#include <syslog.h>

#ifndef ALOGD
#define ALOGD(fmt, arg...) syslog (LOG_DEBUG, fmt, ##arg)
#endif

#ifndef ALOGI
#define ALOGI(fmt, arg...) syslog (LOG_INFO, fmt, ##arg)
#endif

#ifndef ALOGE
#define ALOGE(fmt, arg...) syslog (LOG_ERR, fmt, ##arg)
#endif

#ifndef ALOGV
#define ALOGV(fmt, arg...) syslog (LOG_DEBUG, fmt, ##arg)
#endif

#else
#include <log/log.h>
#endif

#include <memory>
#include <algorithm>
#include "SoundTriggerUtils.h"

#define LOG_TAG "PAL: SoundTriggerUtils"
//#define LOG_NDEBUG 0
#define SML_LIB "liblistensoundmodel2vendor.so"

std::shared_ptr<SoundModelLib> SoundModelLib::sml_ =
    nullptr;

std::shared_ptr<SoundModelLib> SoundModelLib::GetInstance() {
    if (!sml_)
        sml_ = std::make_shared<SoundModelLib>();

    return sml_;
}

SoundModelLib::SoundModelLib() :
    GetSoundModelHeader_(nullptr),
    ReleaseSoundModelHeader_(nullptr),
    GetKeywordPhrases_(nullptr),
    GetUserNames_(nullptr),
    GetMergedModelSize_(nullptr),
    MergeModels_(nullptr),
    GetSizeAfterDeleting_(nullptr),
    DeleteFromModel_(nullptr)
{
    int32_t status = 0;

    sml_lib_handle_ = NULL;
    sml_lib_handle_ = dlopen(SML_LIB, RTLD_NOW);
    if (!sml_lib_handle_) {
        status = -ENOMEM;
        ALOGE("%s: %d: failed to open SML so = %s",
            __func__, __LINE__, dlerror());
        goto exit;
    }

    GetSoundModelHeader_ = (smlib_getSoundModelHeader_t)
                    dlsym(sml_lib_handle_, "getSoundModelHeader");
    if (!GetSoundModelHeader_) {
        ALOGE("%s: %d: failed to map getSoundModelHeader function = %s",
            __func__, __LINE__, dlerror());
        goto exit;
    }

    ReleaseSoundModelHeader_ = (smlib_releaseSoundModelHeader_t)
                    dlsym(sml_lib_handle_, "releaseSoundModelHeader");
    if (!ReleaseSoundModelHeader_) {
        ALOGE("%s: %d: failed to map releaseSoundModelHeader function = %s",
            __func__, __LINE__, dlerror());
        goto exit;
    }

    GetKeywordPhrases_ = (smlib_getKeywordPhrases_t)
                    dlsym(sml_lib_handle_, "getKeywordPhrases");
    if (!GetKeywordPhrases_) {
        ALOGE("%s: %d: failed to map getKeywordPhrases function = %s",
            __func__, __LINE__, dlerror());
        goto exit;
    }

    GetUserNames_ = (smlib_getUserNames_t)
                    dlsym(sml_lib_handle_, "getUserNames");
    if (!GetUserNames_) {
        ALOGE("%s: %d: failed to map getUserNames function = %s",
            __func__, __LINE__, dlerror());
        goto exit;
    }

    GetMergedModelSize_ = (smlib_getMergedModelSize_t)
                    dlsym(sml_lib_handle_, "getMergedModelSize");
    if (!GetMergedModelSize_) {
        ALOGE("%s: %d: failed to map getMergedModelSize function = %s",
            __func__, __LINE__, dlerror());
        goto exit;
    }

    MergeModels_ = (smlib_mergeModels_t)
                    dlsym(sml_lib_handle_, "mergeModels");
    if (!MergeModels_) {
        ALOGE("%s: %d: failed to map mergeModels function = %s",
            __func__, __LINE__, dlerror());
        goto exit;
    }

    GetSizeAfterDeleting_ = (smlib_getSizeAfterDeleting_t)
                    dlsym(sml_lib_handle_, "getSizeAfterDeleting");
    if (!GetSizeAfterDeleting_) {
        ALOGE("%s: %d: failed to map getSizeAfterDeleting function = %s",
            __func__, __LINE__, dlerror());
        goto exit;
    }

    DeleteFromModel_ = (smlib_deleteFromModel_t)
                    dlsym(sml_lib_handle_, "deleteFromModel");
    if (!DeleteFromModel_) {
        ALOGE("%s: %d: failed to map deleteFromModel function = %s",
            __func__, __LINE__, dlerror());
        goto exit;
    }

exit:
    ALOGD("%s: %d: constructor exit status = %d",
        __func__, __LINE__, status);
}

SoundModelLib::~SoundModelLib()
{
    ALOGD("%s: %d: Enter", __func__, __LINE__);
    if (sml_lib_handle_) {
        dlclose(sml_lib_handle_);
        sml_lib_handle_ = nullptr;
    }
}

SoundModelInfo::SoundModelInfo() :
    sm_data_(nullptr),
    sm_size_(0),
    num_keyphrases_(0),
    num_users_(0),
    keyphrases_(nullptr),
    users_(nullptr),
    cf_levels_kw_users_(nullptr),
    cf_levels_(nullptr),
    det_cf_levels_(nullptr),
    cf_levels_size_(0)
{
}

SoundModelInfo::~SoundModelInfo() {
    if (sm_data_) {
        free(sm_data_);
        sm_data_ = nullptr;
    }
    if (cf_levels_) {
        free(cf_levels_);
        cf_levels_ = nullptr;
        det_cf_levels_ = nullptr;
    }
    SoundModelInfo::FreeArrayPtrs(keyphrases_, num_keyphrases_);
    SoundModelInfo::FreeArrayPtrs(users_, num_users_);
    SoundModelInfo::FreeArrayPtrs(cf_levels_kw_users_, cf_levels_size_);
}

SoundModelInfo & SoundModelInfo::operator =(SoundModelInfo &smi) {

    if (this == &smi)
        return *this;

    ALOGV("%s: %d: Entry", __func__, __LINE__);
    /* Free sm_data if it exists, then create and copy it */
    sm_size_ = smi.sm_size_;
    if (sm_data_)
        free(sm_data_);
    sm_data_ = (uint8_t *)calloc(1, sm_size_);
    if (!sm_data_) {
        ALOGE("%s: %d: sm_data calloc allocation failed",
            __func__, __LINE__);
        goto exit;
    }
    memcpy(sm_data_, smi.sm_data_, sm_size_);

    /* Free cf_levels and det_cf_levels if they exists, then create and copy them */
    if (cf_levels_)
        free(cf_levels_);
    cf_levels_ = (uint8_t *)calloc(1, 2 * smi.cf_levels_size_);
    if (!cf_levels_) {
        ALOGE("%s: %d: cf_levels calloc allocation failed",
            __func__, __LINE__);
        free(sm_data_);
        sm_data_ = nullptr;
        cf_levels_size_ = 0;
        goto exit;
    }
    memcpy(cf_levels_, smi.cf_levels_, smi.cf_levels_size_);

    det_cf_levels_ = cf_levels_ + smi.cf_levels_size_;
    if (det_cf_levels_)
        memcpy(det_cf_levels_, smi.det_cf_levels_, smi.cf_levels_size_);

    /* Free key phrases if it exists and then create and copy it */
    if (keyphrases_)
        SoundModelInfo::FreeArrayPtrs(keyphrases_, num_keyphrases_);
    num_keyphrases_ = smi.num_keyphrases_;
    SoundModelInfo::AllocArrayPtrs(&keyphrases_, num_keyphrases_, MAX_STRING_LEN);
    if (keyphrases_) {
        for (uint32_t i = 0; i < num_keyphrases_; i++)
            std::copy(&smi.keyphrases_[i][0],
                &smi.keyphrases_[i][0] + MAX_STRING_LEN,
                &keyphrases_[i][0]);
    }

    /* Free users if it exists and then create and copy it */
    if (users_)
        SoundModelInfo::FreeArrayPtrs(users_, num_users_);
    num_users_ = smi.num_users_;
    SoundModelInfo::AllocArrayPtrs(&users_, num_users_, MAX_STRING_LEN);
    if (users_) {
        for (uint32_t i = 0; i < num_users_; i++)
            std::copy(&smi.users_[i][0],
                &smi.users_[i][0] + MAX_STRING_LEN,
                &users_[i][0]);
    }

    /* Free cf_levels_kw_users if it exists and then create and copy it */
    if (cf_levels_kw_users_)
        SoundModelInfo::FreeArrayPtrs(cf_levels_kw_users_, cf_levels_size_);
    cf_levels_size_ = smi.cf_levels_size_;
    SoundModelInfo::AllocArrayPtrs(&cf_levels_kw_users_, cf_levels_size_,
                     MAX_KW_USERS_NAME_LEN);
    if (cf_levels_kw_users_) {
        for (uint32_t i = 0; i < cf_levels_size_; i++)
            std::copy(&smi.cf_levels_kw_users_[i][0],
                &smi.cf_levels_kw_users_[i][0] + MAX_KW_USERS_NAME_LEN,
                &cf_levels_kw_users_[i][0]);
    }
exit:
    ALOGV("%s: %d: Exit", __func__, __LINE__);
    return *this;
}

void SoundModelInfo::FreeArrayPtrs(char **arr, uint32_t arr_len)
{
    if (!arr)
        return;

    for (uint32_t i = 0; i < arr_len; i++) {
        if (arr[i]) {
            free(arr[i]);
            arr[i] = NULL;
        }
    }
    free(arr);
    arr = NULL;
}

void SoundModelInfo::AllocArrayPtrs(char ***arr, uint32_t arr_len,
    uint32_t elem_len)
{
    char **str_arr = NULL;

    str_arr = (char **) calloc(arr_len, sizeof(char *));

    if (!str_arr) {
        *arr = NULL;
        return;
    }

    for (uint32_t i = 0; i < arr_len; i++) {
         str_arr[i] = (char *) calloc(elem_len, sizeof(char));
         if (str_arr[i] == NULL) {
             FreeArrayPtrs(str_arr, i);
             *arr = NULL;
             return;
         }
    }
    *arr = str_arr;
    ALOGV("%s: %d: string array %p", __func__, __LINE__, *arr);
    for (uint32_t i = 0; i < arr_len; i++)
        ALOGV("%s: %d: string array[%d] %p", __func__, __LINE__, i, (*arr)[i]);
}

int32_t SoundModelInfo::SetKeyPhrases(listen_model_type *model, uint32_t num_phrases) {

    std::shared_ptr<SoundModelLib>sml = SoundModelLib::GetInstance();
    uint16_t sml_numphrases = 0;
    listen_status_enum sml_ret = kSucess;

    if (!sml) {
        ALOGE("%s: %d: SML handle is NULL", __func__, __LINE__);
        return -EINVAL;
    }
    num_keyphrases_ = num_phrases;

    if (num_keyphrases_) {
        SoundModelInfo::AllocArrayPtrs(&keyphrases_, num_keyphrases_, MAX_STRING_LEN);
        if (!keyphrases_) {
            ALOGE("%s: %d: keyphrases allocation failed", __func__, __LINE__);
            return -ENOMEM;
        }

        sml_numphrases = num_keyphrases_;
        sml_ret = sml->GetKeywordPhrases_(model, &sml_numphrases, keyphrases_);
        if (sml_ret) {
            ALOGE("%s: %d: GetKeywordPhrases failed, err %d",
                __func__, __LINE__, sml_ret);
            return -EINVAL;
        }
        if (sml_numphrases != num_keyphrases_) {
            ALOGE("%s: %d: GetkeywordPhrases(%d) != sml header (%d)",
                __func__, __LINE__, sml_numphrases, num_keyphrases_);
            return -EINVAL;
        }
        for (uint32_t i = 0; i < num_keyphrases_; i++)
            ALOGV("%s: %d: keyphrases names = %s",
                __func__, __LINE__, keyphrases_[i]);
    }
    return 0;
}

int32_t SoundModelInfo::SetUsers(listen_model_type *model, uint32_t num_users) {

    std::shared_ptr<SoundModelLib>sml = SoundModelLib::GetInstance();
    uint16_t sml_numusers = 0;
    listen_status_enum sml_ret = kSucess;

    if (!sml) {
        ALOGE("%s: %d: SML handle is NULL", __func__, __LINE__);
        return -EINVAL;
    }
    num_users_ = num_users;

    if (num_users_) {
        SoundModelInfo::AllocArrayPtrs(&users_, num_users_, MAX_STRING_LEN);
        if (!users_) {
            ALOGE("%s: %d: users allocation failed", __func__, __LINE__);
            return -ENOMEM;
        }

        sml_numusers = num_users_;
        sml_ret = sml->GetUserNames_(model, &sml_numusers, users_);
        if (sml_ret) {
            ALOGE("%s: %d: GetUserNames_ failed, err %d",
                __func__, __LINE__, sml_ret);
            return -EINVAL;
        }
        if (sml_numusers != num_users_) {
            ALOGE("%s: %d: GetUserNames_(%d) != sml header (%d)",
                __func__, __LINE__, sml_numusers, num_users_);
            return -EINVAL;
        }
        for (uint32_t i = 0; i < num_users_; i++)
            ALOGV("%s: %d: users names = %s", __func__, __LINE__, users_[i]);
    }
    return 0;
}

int32_t SoundModelInfo::SetConfLevels(uint16_t num_user_kw_pairs,
                                   uint16_t *num_users_per_kw,
                                   uint16_t **user_kw_pair_flags) {

    uint32_t i = 0, j = 0, k = 0;

    cf_levels_size_ = num_keyphrases_ + num_user_kw_pairs;

    ALOGV("%s: %d: cf_levels_size_: %d", __func__, __LINE__, cf_levels_size_);
    if (cf_levels_size_) {
        SoundModelInfo::AllocArrayPtrs(&cf_levels_kw_users_, cf_levels_size_,
                         MAX_KW_USERS_NAME_LEN);
        if (!cf_levels_kw_users_) {
            ALOGE("%s: %d: cf_levels_kw_users_ allocation failed",
                __func__, __LINE__);
            return -ENOMEM;
        }

        /* Used later for mapping client to/from merged DSP confidence levels */
        cf_levels_ = (uint8_t *)calloc(1, 2 * cf_levels_size_);
        if (!cf_levels_) {
            ALOGE("%s: %d: cf_levels allocation failed",
                __func__, __LINE__);
            return -ENOMEM;
        }

        /*
         * Used for updating detection confidence level values from DSP merged
         * detection conf levels
         */
        det_cf_levels_ = cf_levels_ + cf_levels_size_;

        /*
         * Used for conf level setting to DSP. Reset the conf value to max value,
         * so that the keyword of a loaded and in-active model in a merged model
         * doesn't detect.
         */
        memset(cf_levels_, MAX_CONF_LEVEL_VALUE, cf_levels_size_);

        /*
         * Derive the confidence level payload for keyword and user pairs.
         * Store the user-keyword pair names in an array that will be used for
         * mapping the DSP detection and confidence levels to the client.
         */
        char **kw_names = cf_levels_kw_users_;
        char **ukw_names = &cf_levels_kw_users_[num_keyphrases_];
        int32_t ukw_idx = 0;

        for (i = 0; i < num_keyphrases_; i++) {
            strlcpy(kw_names[i], keyphrases_[i], MAX_KW_USERS_NAME_LEN);
            if (!num_users_per_kw)
                continue;
            for (j = 0; j < num_users_; j++) {
                if (k >= num_users_per_kw[i])
                    break;
                if (user_kw_pair_flags[j][i]) {
                    strlcpy(ukw_names[ukw_idx], users_[j], MAX_KW_USERS_NAME_LEN);
                    strlcat(ukw_names[ukw_idx], keyphrases_[i],
                         MAX_KW_USERS_NAME_LEN);
                    ukw_idx++;
                    k++;
                }
            }
        }
        for (i = 0; i < cf_levels_size_; i++)
            ALOGV("%s: %d: cf_levels_kw_users = %s, cf_levels[%d] = %d",
                __func__, __LINE__, cf_levels_kw_users_[i], i, cf_levels_[i]);
    }
    return 0;
}

int32_t SoundModelInfo::UpdateConfLevelArray(uint8_t *conf_levels, uint32_t cfl_size) {
    if (cfl_size > cf_levels_size_ || !conf_levels) {
        ALOGE("%s: %d: cfl_size: %d, expected size: %d",
            __func__, __LINE__, cfl_size, cf_levels_size_);
        return -EINVAL;
    }
    if (cf_levels_) {
        memcpy(cf_levels_, conf_levels, cfl_size);
        for (uint32_t i = 0; i < cf_levels_size_; i++)
            ALOGV("%s: %d: cf_levels[%d] = %d",
                __func__, __LINE__, i, cf_levels_[i]);
    }
    return 0;
}
