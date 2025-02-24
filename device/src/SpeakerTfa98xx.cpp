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

#define LOG_TAG "PAL: SpeakerTfa98xx"
#include "SpeakerTfa98xx.h"
#include <log/log.h>

SpeakerTfa98xx::SpeakerTfa98xx()
    : isInitialized(false),
      speakerCount(0),
      powerAmpCount(0),
      isValidCalibration(false) {
    int status = 0;
    rm = ResourceManager::getInstance();
    if (!rm) {
        PAL_ERR(LOG_TAG, "Failed to get ResourceManager instance");
        return;
    }

    // Getting mixture controls from Resource Manager
    status = rm->getVirtualAudioMixer(&virtMixer);
    if (status) {
        PAL_ERR(LOG_TAG, "virt mixer error %d", status);
    }

    status = rm->getHwAudioMixer(&hwMixer);
    if (status) {
        PAL_ERR(LOG_TAG, "hw mixer error %d", status);
    }

    calibrationInfoInit();
    updateCalibrationValue();
    PAL_INFO(LOG_TAG, "SpeakerTfa98xx initialized");
    isInitialized = true;
}

SpeakerTfa98xx::~SpeakerTfa98xx() {
    hwMixer = nullptr;
    virtMixer = nullptr;
}

bool SpeakerTfa98xx::isTfaDevicePresent(struct mixer* hwMixer) {
    return mixer_get_ctl_by_name(hwMixer, "TFA Calibration");
}

void SpeakerTfa98xx::calibrationInfoInit() {
    calibratedImpedance = mixer_get_ctl_by_name(hwMixer, "TFA Calibration");
    if (!calibratedImpedance) {
        PAL_ERR(LOG_TAG, "Invalid mixer control: TFA Calibration");
        return;
    }

    speakerCount = mixer_ctl_get_num_values(calibratedImpedance);
    if (speakerCount <= 0) {
        PAL_ERR(LOG_TAG, "Invalid speaker count: %d", speakerCount);
        return;
    }
    PAL_INFO(LOG_TAG, "speakerCount:%d, powerAmpCount:%d", speakerCount, powerAmpCount);

    // 2 speakers -> 1 power amp
    // 4 speakers -> 2 power amps
    powerAmpCount = std::min((speakerCount + 1) >> 1, static_cast<int>(MAX_PA_COUNT));

    defaultImpedance = mixer_get_ctl_by_name(hwMixer, "TFA Default Impedance");
    if (!defaultImpedance) {
        PAL_ERR(LOG_TAG, "Invalid mixer control: TFA Default Impedance");
        return;
    }

    caliInfo.clear();
    for (int i = 0; i < speakerCount; i++) {
        if (FILE* fp = openDeviceFile(DEVICE_ADDRESSES[i], "cali_info")) {
            char buffer[64] = {0};
            if (readDeviceFile(buffer, sizeof(buffer), fp) > 0) {
                CaliInfo info = {};
                uint32_t addr;
                if (sscanf(buffer, "0x%x, %hhx, %d, %d\n", &addr, &info.dev_idx, &info.min_imp,
                           &info.max_imp) == 4) {
                    info.i2c_addr = DEVICE_ADDRESSES[i];
                    if (info.dev_idx < speakerCount) {
                        caliInfo.push_back(info);
                    }
                }
            }
            fclose(fp);
        }
    }

    if (!caliInfo.empty()) {
        std::sort(caliInfo.begin(), caliInfo.end());

        for (auto& info : caliInfo) {
            info.def_imp = mixer_ctl_get_value(defaultImpedance, info.dev_idx);
            PAL_INFO(LOG_TAG, "Speaker %hhx: addr=0x%x, impedance:(min=%d, max=%d, default=%d)",
                     info.dev_idx, info.i2c_addr, info.min_imp, info.max_imp, info.def_imp);
        }
    }
}

void SpeakerTfa98xx::updateCalibrationValue() {
    isValidCalibration = false;
    struct mixer_ctl* calCtl = mixer_get_ctl_by_name(hwMixer, "TFA Calibration");
    if (!calCtl) return;

    for (auto& info : caliInfo) {
        int calValue = mixer_ctl_get_value(calCtl, info.dev_idx);
        info.cal_imp = calValue;

        if (calValue > info.min_imp && calValue < info.max_imp) {
            info.dsp_imp = (calValue << 16) / 1000;
            isValidCalibration = true;
            PAL_INFO(LOG_TAG, "i2c=0x%x dev_idx=%i, impedance:(default=%d, cal=%d, dsp=0x%x)",
                     info.i2c_addr, info.dev_idx, info.def_imp, info.cal_imp, info.dsp_imp);
        } else {
            if (info.def_imp > 0) {
                info.dsp_imp = (info.def_imp << 16) / 1000;
            } else if (info.max_imp > 0 && info.min_imp > 0) {
                info.dsp_imp = ((info.max_imp + info.min_imp) >> 1 << 16) / 1000;
            }

            PAL_ERR(LOG_TAG, "i2c=0x%x, dev_idx=%hhx, abnormal impedance=%d", info.i2c_addr,
                    info.dev_idx, calValue);
        }
    }
}

// References: OplusSpeakerTfa98xx::payloadSPConfig and tfa98xx_adsp_send_calib_values() from tfa98xx_v6.c
void SpeakerTfa98xx::payloadSPConfig(uint8_t** payload, size_t* size, uint32_t miid) {
    struct apm_module_param_data_t* header = nullptr;
    uint8_t* payloadInfo = nullptr;
    size_t payloadSize = 0, padBytes = 0;

    *payload = nullptr;
    *size = 0;

    if (caliInfo.empty() || caliInfo.size() != speakerCount) {
        PAL_ERR(LOG_TAG, "Invalid calibration info size: %zu, expected: %d", caliInfo.size(),
                speakerCount);
        return;
    }

    // 11 bytes for calibration data (1 reserved + 10 used)
    payloadSize = sizeof(struct apm_module_param_data_t) + 11;
    padBytes = PAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = (uint8_t*)calloc(1, payloadSize + padBytes);
    if (!payloadInfo) {
        PAL_ERR(LOG_TAG, "Failed to allocate payload memory");
        return;
    }
    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = miid;
    header->param_id = TFADSP_RX_SET_COMMAND;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);
    uint8_t* bytes = payloadInfo + sizeof(struct apm_module_param_data_t);
    
    // Reserve bytes[0] to match kernel implementation
    bytes[1] = 0x00;
    bytes[2] = 0x81;
    bytes[3] = 0x05;

    // Process each speaker's calibration data
    for (const auto& info : caliInfo) {
        if (info.dsp_imp == 0) {
            PAL_ERR(LOG_TAG, "Invalid DSP impedance for speaker %d", info.dev_idx);
            free(payloadInfo);
            return;
        }

        // Calculate array index based on speaker index (4 for left, 7 for right)
        int baseIdx = (info.dev_idx == 0) ? 4 : 7;

        // Store impedance value in big-endian format
        bytes[baseIdx + 0] = (info.dsp_imp >> 16) & 0xFF;
        bytes[baseIdx + 1] = (info.dsp_imp >> 8) & 0xFF;
        bytes[baseIdx + 2] = info.dsp_imp & 0xFF;

        PAL_INFO(LOG_TAG, "Speaker %d: impedance=0x%x", info.dev_idx, info.dsp_imp);
    }

    // For mono case, copy primary channel data to secondary
    if (speakerCount == 1) {
        memcpy(&bytes[7], &bytes[4], 3);
    }

    *size = payloadSize + padBytes;
    *payload = payloadInfo;
}

FILE* SpeakerTfa98xx::openDeviceFile(uint8_t i2c_addr, const char* parameter) {
    char path[128] = {0};
    snprintf(path, sizeof(path), "/proc/tfa98xx-%x/%s", i2c_addr, parameter);
    FILE* fp = fopen(path, "r");
    if (!fp) {
        PAL_INFO(LOG_TAG, "Failed to open %s", path);
    }
    return fp;
}

long SpeakerTfa98xx::readDeviceFile(char* buffer, size_t size, FILE* fp) {
    if (!buffer || !fp || size == 0) return 0;
    size_t bytes = fread(buffer, 1, size - 1, fp);
    if (bytes > 0) buffer[bytes] = '\0';
    return bytes;
}
