/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef OPLUS_SPEAKER_AWINIC_H
#define OPLUS_SPEAKER_AWINIC_H

#include "OplusSpeakerProtection.h"

#include <string>

/*
 * Awinic aw882xx amplifiers, the SP module runs the Awinic algorithm.
 */
class OplusSpeakerAwinic : public OplusSpeakerAmp
{
public:
    const char *name() const override { return "Awinic"; }
    bool probe(struct mixer *hwMixer) override;
    uint32_t speakerCount() const override { return mDevNum; }
    int readCalibration(struct mixer *hwMixer) override;
    int buildCalibrationParam(uint32_t miid, std::vector<uint8_t> &param) override;

private:
    struct Range {
        int min;
        int max;
    };

    static std::string readNode(const char *cmd);
    static int readFileValue(const char *path, uint32_t slot, int *value);
    static int selectValue(int fileValue, const Range &range, int defaultValue);
    int readRanges(const char *cmd, const char *format, std::vector<Range> &ranges);
    int readDefaults(const char *cmd, const char *format, std::vector<int> &values);

    uint32_t mDevNum = 0;
    bool mF0Support = false;
    std::vector<int> mRe;
    std::vector<int> mF0;
};

#endif
