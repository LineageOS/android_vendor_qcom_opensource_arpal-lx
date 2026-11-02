/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef OPLUS_SPEAKER_PROTECTION_H
#define OPLUS_SPEAKER_PROTECTION_H

#include "Device.h"

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct mixer;
struct pcm;
struct str_parms;
class Stream;

// Per amplifier gain for a supply voltage threshold, from <pa_vdd_gain_mapping>.
struct OplusVddGain {
    std::vector<int> limiter;
    std::vector<int> gain;
};

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
    // Whether the amplifier controls are present, only used when the type isn't configured.
    virtual bool probe(struct mixer *hwMixer) = 0;
    // Reads the calibration, returns false when it should be read again on the next start.
    virtual bool readCalibration(struct mixer *hwMixer) = 0;
    // Whether the gain follows the battery voltage instead of its capacity.
    virtual bool usesVddGain() const { return false; }
    // Low battery gain, sent along with the calibration.
    virtual void setGain(bool enabled, int level) { (void)enabled; (void)level; }
    virtual void setVddGain(bool enabled, const OplusVddGain *gain) { (void)enabled; (void)gain; }
    // Builds the SP module params, apm_module_param_data_t headers included.
    // Returns -ENODATA when there is nothing usable to send.
    virtual int buildCalibrationParam(uint32_t miid, std::vector<uint8_t> &param) = 0;
    // Params to update the gain of a running speaker.
    virtual int buildGainParam(uint32_t miid, std::vector<uint8_t> &param)
    {
        (void)miid;
        (void)param;
        return -ENODATA;
    }
    // Periodic amplifier monitoring, fills the params to send to a running speaker.
    virtual int monitor(struct mixer *hwMixer, uint32_t miid, std::vector<uint8_t> &param)
    {
        (void)hwMixer;
        (void)miid;
        (void)param;
        return -ENODATA;
    }
};

/*
 * Speaker protection for oplus devices.
 *
 * It needs the amplifier calibration in the RX SP module and live VI feedback
 * on the TX path. Like stock, a missing piece is logged and the speaker plays
 * anyway.
 */
class OplusSpeakerProtection : public Device
{
public:
    OplusSpeakerProtection(struct pal_device *device, std::shared_ptr<ResourceManager> Rm);
    ~OplusSpeakerProtection();

    int32_t start() override;
    int32_t stop() override;

    // resourcemanager.xml config_params keys.
    static void setConfigParams(struct str_parms *parms, char *value, int len);
    // Value of the speaker scene calibration key for the stream.
    static int getSpeakerScene(Stream *stream);
    // resourcemanager.xml <smartpa_gain_mapping> entries.
    static void addBatteryGainLevel(const char **attr);
    // resourcemanager.xml <pa_vdd_gain_mapping> entries.
    static void addVddGain(const char **attr);

private:
    enum SpeakerId {
        SPEAKER_ID_TFA98XX = 1,
        SPEAKER_ID_AWINIC = 3,
        SPEAKER_ID_SIA = 4,
    };

    enum SpeakerType {
        SPEAKER_TYPE_ANALOG = 0,
        SPEAKER_TYPE_SMART_PA = 1,
    };

    int createAmp();
    static int getBatteryGainLevel();
    static int getBatteryVoltage();
    bool updateGain();
    void sendLiveParam(std::vector<uint8_t> &param);
    void monitorThread();
    void updateRxConfig();
    void sendRxInfoToDriver();
    int openViPath();
    void closeViPath();

    static int sSpeakerId;
    static bool sSpeakerIdSet;
    static int sSpeakerType;
    static bool sVoipCkvSupport;
    static bool sLowBatteryGainSupport;
    static std::vector<std::pair<int, int>> sBatteryGainLevels;
    static std::map<int, OplusVddGain> sVddGains;

    std::unique_ptr<OplusSpeakerAmp> mAmp;
    bool mCalibrated;
    struct pal_device mDeviceAttr;
    uint32_t mViChannels;
    uint32_t mViSampleRate;
    uint32_t mViBitWidth;
    struct mixer *mVirtMixer;
    struct mixer *mHwMixer;
    struct pcm *mTxPcm;
    uint32_t mRxMiid;
    int mPcmIdCtrlVal;
    int mActivePcmId;
    uint32_t mMiidCtrlVal;
    int mGainLevel;
    int mVddThreshold;
    bool mSpeakerInUse;
    bool mMonitorExit;
    std::mutex mMonitorMutex;
    std::condition_variable mMonitorCv;
    std::thread mMonitorThread;
    std::vector<int> mPcmDevIdTx;
    std::string mViBackEndName;
    std::string mViSndDevice;
};

#endif
