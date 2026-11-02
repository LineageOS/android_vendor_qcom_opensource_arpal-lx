/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef OPLUS_SPEAKER_PROTECTION_H
#define OPLUS_SPEAKER_PROTECTION_H

#include "Device.h"

#include <string>
#include <vector>

struct pcm;

/*
 * Speaker protection for Oplus devices with TFA98xx amplifiers.
 *
 * The algorithm runs on the ADSP (libtfadsp). It needs the amplifier calibration
 * in the RX SP module and live VI feedback on the TX path.
 */
class OplusSpeakerProtection : public Device
{
public:
    OplusSpeakerProtection(struct pal_device *device, std::shared_ptr<ResourceManager> Rm);
    ~OplusSpeakerProtection();

    int32_t start() override;
    int32_t stop() override;

private:
    struct TfaCalibration {
        uint32_t address;
        uint32_t index;
        int minImpedance;
        int maxImpedance;
        uint32_t dspImpedance;
    };

    int readCalibration();
    int buildCalibrationPayload(uint32_t miid);
    void sendRxInfoToDriver(Stream *stream, uint32_t miid);
    int updateRxConfig();
    int openViPath();
    void closeViPath();

    std::vector<TfaCalibration> mCalibration;
    uint32_t mF0;
    struct pal_device mDeviceAttr;
    struct mixer *mVirtMixer;
    struct mixer *mHwMixer;
    struct pcm *mTxPcm;
    std::vector<int> mPcmDevIdTx;
    std::string mViBackEndName;
    std::string mViSndDevice;
};

#endif
