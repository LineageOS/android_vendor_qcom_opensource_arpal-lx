/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "PayloadBuilder.h"
#include "ResourceManager.h"
#include "apm/apm_api.h"

static constexpr uint8_t MAX_PA_COUNT = 2;
static constexpr uint8_t DEVICE_ADDRESSES[] = {0x34, 0x35, 0x36, 0x37};

// Referenced from tfadsp_common.h but replaced with reverse engineered parameter values
// Perhaps the best place to put these is in sp_rx.h ?
#define TFADSP_RX_SET_COMMAND 0x1800B921
#define TFADSP_RX_GET_RESULT 0x1800B922
typedef struct tfadsp_rx_enable_cfg_t {
    uint32_t enable_flag;
    /**< Enable flag: 0 = disabled; 1 = enabled. */
} tfadsp_rx_enable_cfg_t;

struct CaliInfo {
    int8_t dev_idx;
    int8_t i2c_addr;
    int32_t cal_imp;
    int32_t def_imp;
    int32_t dsp_imp;
    int32_t min_imp;
    int32_t max_imp;

    bool operator<(const CaliInfo& other) const { return dev_idx < other.dev_idx; }
};

class SpeakerTfa98xx {
  public:
    SpeakerTfa98xx();
    ~SpeakerTfa98xx();

    static bool isTfaDevicePresent(struct mixer* hwMixer);
    void calibrationInfoInit();
    void updateCalibrationValue();
    void payloadSPConfig(uint8_t** payload, size_t* size, uint32_t miid);
    int32_t sendPcmIdAndMiidToDriver(uint32_t miid, int pcmId);

  private:
    std::shared_ptr<ResourceManager> rm;
    bool isInitialized;
    struct mixer* hwMixer;
    struct mixer* virtMixer;

    int8_t speakerCount;
    int8_t powerAmpCount;
    std::vector<CaliInfo> caliInfo;

    struct mixer_ctl* calibratedImpedance;
    struct mixer_ctl* defaultImpedance;

    bool isValidCalibration;

    FILE* openDeviceFile(uint8_t address, const char* type);
    long readDeviceFile(char* buffer, size_t size, FILE* fp);
};
