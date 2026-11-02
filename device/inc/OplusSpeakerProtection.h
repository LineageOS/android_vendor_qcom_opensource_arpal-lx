/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef OPLUS_SPEAKER_PROTECTION_H
#define OPLUS_SPEAKER_PROTECTION_H

#include "Device.h"

#include <memory>
#include <string>
#include <vector>

struct mixer;
struct pcm;

/*
 * Amplifier specific part of the oplus speaker protection.
 *
 * The algorithm runs on the ADSP, each amplifier vendor has its own way to
 * read the speaker calibration and to pass it to the SP module.
 */
class OplusSpeakerAmp
{
public:
    virtual ~OplusSpeakerAmp() = default;

    virtual const char *name() const = 0;
    // Whether this amplifier is present on the device.
    virtual bool probe(struct mixer *hwMixer) = 0;
    virtual uint32_t speakerCount() const = 0;
    virtual int readCalibration(struct mixer *hwMixer) = 0;
    // Builds the SP module param, apm_module_param_data_t header included.
    virtual int buildCalibrationParam(uint32_t miid, std::vector<uint8_t> &param) = 0;
};

/*
 * Speaker protection for oplus devices.
 *
 * It needs the amplifier calibration in the RX SP module and live VI feedback
 * on the TX path.
 */
class OplusSpeakerProtection : public Device
{
public:
    OplusSpeakerProtection(struct pal_device *device, std::shared_ptr<ResourceManager> Rm);
    ~OplusSpeakerProtection();

    int32_t start() override;
    int32_t stop() override;

private:
    int probeAmp();
    int sendCalibration(uint32_t miid);
    void sendRxInfoToDriver(Stream *stream, uint32_t miid);
    int updateRxConfig();
    int openViPath();
    void closeViPath();

    std::unique_ptr<OplusSpeakerAmp> mAmp;
    bool mCalibrated;
    struct pal_device mDeviceAttr;
    struct mixer *mVirtMixer;
    struct mixer *mHwMixer;
    struct pcm *mTxPcm;
    std::vector<int> mPcmDevIdTx;
    std::string mViBackEndName;
    std::string mViSndDevice;
};

#endif
