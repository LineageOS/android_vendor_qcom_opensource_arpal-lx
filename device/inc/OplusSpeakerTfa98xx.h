/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef OPLUS_SPEAKER_TFA98XX_H
#define OPLUS_SPEAKER_TFA98XX_H

#include "OplusSpeakerProtection.h"

/*
 * TFA98xx amplifiers, the SP module runs libtfadsp.
 */
class OplusSpeakerTfa98xx : public OplusSpeakerAmp
{
public:
    const char *name() const override { return "TFA98xx"; }
    bool probe(struct mixer *hwMixer) override;
    uint32_t speakerCount() const override;
    int readCalibration(struct mixer *hwMixer) override;
    int buildCalibrationParam(uint32_t miid, std::vector<uint8_t> &param) override;

private:
    struct Calibration {
        uint32_t address;
        uint32_t index;
        int minImpedance;
        int maxImpedance;
        uint32_t dspImpedance;
    };

    std::vector<Calibration> mCalibration;
    uint32_t mF0 = 0;
};

#endif
