/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef OPLUS_SPEAKER_TFA98XX_H
#define OPLUS_SPEAKER_TFA98XX_H

#include "OplusSpeakerProtection.h"

class OplusSpeakerTfa98xx : public OplusSpeakerAmp
{
public:
    const char *name() const override { return "TFA98xx"; }
    bool probe(struct mixer *hwMixer) override;
    bool readCalibration(struct mixer *hwMixer) override;
    void setGain(bool enabled, int level) override;
    int buildCalibrationParam(uint32_t miid, std::vector<uint8_t> &param) override;
    int buildGainParam(uint32_t miid, std::vector<uint8_t> &param) override;

private:
    struct Calibration {
        uint32_t index;
        int minImpedance;
        int maxImpedance;
        int defaultImpedance;
        int calImpedance;
        int dspImpedance;
    };

    void initCalibrationInfo(struct mixer *hwMixer);
    void readF0(struct mixer *hwMixer, bool fromMixer);

    bool mInitialized = false;
    uint32_t mSpeakerCount = 0;
    uint32_t mDspInstanceCount = 0;
    std::vector<Calibration> mCalibration;
    std::vector<uint32_t> mF0;
    bool mGainEnabled = false;
    uint8_t mGain = 0;
};

#endif
