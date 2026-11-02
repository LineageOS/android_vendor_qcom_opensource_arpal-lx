/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef OPLUS_SPEAKER_SIA_H
#define OPLUS_SPEAKER_SIA_H

#include "OplusSpeakerProtection.h"

#include <string>

class OplusSpeakerSia : public OplusSpeakerAmp
{
public:
    const char *name() const override { return "Sia"; }
    bool probe(struct mixer *hwMixer) override;
    bool readCalibration(struct mixer *hwMixer) override;
    int buildCalibrationParam(uint32_t miid, std::vector<uint8_t> &param) override;
    bool usesVddGain() const override { return true; }
    void setVddGain(bool enabled, const OplusVddGain *gain) override;
    int buildGainParam(uint32_t miid, std::vector<uint8_t> &param) override;

    // resourcemanager.xml config_params keys.
    static void setConfigParams(struct str_parms *parms, char *value, int len);

private:
    struct Channel {
        int minR0;
        int maxR0;
        int defaultR0;
        // Values sent to the SP module, r0 first.
        int32_t cal[4];
    };

    const char *channelPrefix(uint32_t channel) const;
    uint32_t prefixCount() const;
    void initCalibrationInfo(struct mixer *hwMixer);
    bool readCalibrationFile(uint32_t channel, int32_t record[5]) const;

    static bool sLegacyFormat;
    static bool sNewAlgo;
    static uint32_t sCaliChannelCount;
    static uint32_t sSpeakerCount;

    bool mInitialized = false;
    std::string mCaliFile;
    std::vector<Channel> mChannels;
    bool mGainEnabled = false;
    bool mHasGain = false;
    OplusVddGain mGain;
};

#endif
