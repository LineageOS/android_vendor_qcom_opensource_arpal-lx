/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <memory>
#include <cstring>

typedef char * keywordId_t;
typedef char * userId_t;

typedef struct {
    uint8_t *data;              /* block of memory containing Model data */
    uint32_t size;              /* size of memory allocated for Model data */
} listen_model_type;


typedef struct {
    uint16_t                numKeywords;  /* total number of keywords  */
    uint16_t                numUsers;    /* total number of users  */
    uint16_t                numActiveUserKeywordPairs;    /* total number of active user+keyword pairs in SM */
    bool                    isStripped; /* if corresponding keyword is stripped or not */
    uint16_t                *langPerKw; /* Language code of each keyword */
    /* number active Users per keyword - included as convenience */
    uint16_t                *numUsersSetPerKw;
    bool                    *isUserDefinedKeyword;
    /* Ordered 'truth' table of all possible pairs of users for each keyword.
    * Active entries marked with 1, inactive 0.keywordPhrase
    * 16-bit short (rather than boolean) is used to match SM model data size */
    uint16_t                **userKeywordPairFlags;
    uint16_t                model_indicator; /* for SM 3.0, indicate which models were combined */
} listen_sound_model_header;

typedef enum {
    kSucess = 0,
    kFailed = 1,
    kBadParam,
    kKeywordNotFound,
    kUserNotFound,
    kUserKwPairNotActive,
    kSMVersionUnsupported,
    kUserDataForKwAlreadyPresent,
    kDuplicateKeyword,
    kDuplicateUserKeywordPair,
    kMaxKeywordsExceeded,
    kMaxUsersExceeded,
    kEventStructUnsupported,    // payload contains event data that can not be processed, or mismatches SM version
    kLastKeyword,
    kNoSignal,
    kLowSnr,
    kRecordingTooShort,
    kRecordingTooLong,
    kNeedRetrain,
    kUserUDKPairNotRemoved,
    kCannotCreateUserUDK,
    kOutputArrayTooSmall,
    kTooManyAbnormalUserScores,
    kWrongModel,
    kWrongModelAndIndicator,
    kDuplicateModel,
    kChoppedSample,
    kSecondStageKeywordNotFound,
    kClippedSample,
} listen_status_enum;

typedef listen_status_enum (*smlib_getSoundModelHeader_t)
(
    listen_model_type         *pSoundModel,
    listen_sound_model_header *pListenSoundModelHeader
);

typedef listen_status_enum (*smlib_releaseSoundModelHeader_t)
(
    listen_sound_model_header *pListenSoundModelHeader
);

typedef listen_status_enum (*smlib_getKeywordPhrases_t)
(
    listen_model_type *pSoundModel,
    uint16_t          *numKeywords,
    keywordId_t       *keywords
);

typedef listen_status_enum (*smlib_getUserNames_t)
(
    listen_model_type *pSoundModel,
    uint16_t          *numUsers,
    userId_t          *users
);

typedef listen_status_enum (*smlib_getMergedModelSize_t)
(
     uint16_t          numModels,
     listen_model_type *pModels[],
     uint32_t          *nOutputModelSize
);

typedef listen_status_enum (*smlib_mergeModels_t)
(
     uint16_t          numModels,
     listen_model_type *pModels[],
     listen_model_type *pMergedModel
);

typedef listen_status_enum (*smlib_getSizeAfterDeleting_t)
(
    listen_model_type *pInputModel,
    keywordId_t       keywordId,
    userId_t          userId,
    uint32_t          *nOutputModelSize
);

typedef listen_status_enum (*smlib_deleteFromModel_t)
(
    listen_model_type *pInputModel,
    keywordId_t       keywordId,
    userId_t          userId,
    listen_model_type *pResultModel
);

class SoundModelLib {
 public:
    static std::shared_ptr<SoundModelLib> GetInstance();
    SoundModelLib & operator=(SoundModelLib &rhs) = delete;
    SoundModelLib();
    ~SoundModelLib();
    smlib_getSoundModelHeader_t GetSoundModelHeader_;
    smlib_releaseSoundModelHeader_t ReleaseSoundModelHeader_;
    smlib_getKeywordPhrases_t GetKeywordPhrases_;
    smlib_getUserNames_t GetUserNames_;
    smlib_getMergedModelSize_t GetMergedModelSize_;
    smlib_mergeModels_t MergeModels_;
    smlib_getSizeAfterDeleting_t GetSizeAfterDeleting_;
    smlib_deleteFromModel_t DeleteFromModel_;

 private:
    static std::shared_ptr<SoundModelLib> sml_;
    void *sml_lib_handle_;
};

class SoundModelInfo {
public:
    SoundModelInfo();
    SoundModelInfo(SoundModelInfo &rhs) = delete;
    SoundModelInfo & operator=(SoundModelInfo &rhs);
    ~SoundModelInfo();
    int32_t SetKeyPhrases(listen_model_type *model, uint32_t num_phrases);
    int32_t SetUsers(listen_model_type *model, uint32_t num_users);
    int32_t SetConfLevels(uint16_t num_user_kw_pairs, uint16_t *num_users_per_kw,
                          uint16_t **user_kw_pair_flags);
    void SetModelData(uint8_t *data, uint32_t size) {
        if (sm_data_) {
            free(sm_data_);
            sm_data_ = nullptr;
        }
        sm_size_ = size;
        if (!sm_size_)
            return;
        sm_data_ = (uint8_t*) calloc(1, sm_size_);
        if (!sm_data_)
            return;
        memcpy(sm_data_, data, sm_size_);
    }
    void UpdateConfLevel(uint32_t index, uint8_t conf_level) {
        if (index < cf_levels_size_)
            cf_levels_[index] = conf_level;
    }
    int32_t UpdateConfLevelArray(uint8_t *conf_levels, uint32_t cfl_size);
    void ResetDetConfLevels() {
        memset(det_cf_levels_, 0, cf_levels_size_);
    }
    void UpdateDetConfLevel(uint32_t index, uint8_t conf_level) {
        if (index < cf_levels_size_)
            det_cf_levels_[index] = conf_level;
    }
    uint8_t* GetModelData() { return sm_data_; };
    uint32_t GetModelSize() { return sm_size_; };
    char** GetKeyPhrases() { return keyphrases_; };
    char** GetConfLevelsKwUsers() { return cf_levels_kw_users_; };
    uint8_t* GetConfLevels() { return cf_levels_; };
    uint8_t* GetDetConfLevels() { return det_cf_levels_; };
    uint32_t GetConfLevelsSize() { return cf_levels_size_; };
    uint32_t GetNumKeyPhrases() { return num_keyphrases_; };
    static void AllocArrayPtrs(char ***arr, uint32_t arr_len, uint32_t elem_len);
    static void FreeArrayPtrs(char **arr, uint32_t arr_len);

private:
    uint8_t *sm_data_;
    uint32_t sm_size_;
    uint32_t num_keyphrases_;
    uint32_t num_users_;
    char **keyphrases_;
    char **users_;
    char **cf_levels_kw_users_;
    uint8_t *cf_levels_;
    uint8_t *det_cf_levels_;
    uint32_t cf_levels_size_;
};
