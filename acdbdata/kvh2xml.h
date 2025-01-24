/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * kvh2xml.h
 *
 * This header file defines the keys to be used in acdbdata.
 *
 */

 #ifndef __KVH_2_XML_H__
 #define __KVH_2_XML_H__

typedef enum {
    PLATFORM_LA = 1,    /**< @h2xmle_name {LA} */
    PLATFORM_LE = 2,    /**< @h2xmle_name {LE} */
}platforms;
/**
    @h2xml_platforms{PLATFORM_LA,PLATFORM_LE}
*/

/**
    @AllKeyIds
    @description { Unique KeyId Defintions that can be used as graph keys, cal keys, tag keys.
                         Bits:[16-31]: Reserved for Key Definitions
                         (Start at 0xA000 with exception of 0xC000 )
                         Bits:[ 0-15]: 0x0000 (Fixed)
                         Lower Range: 0xA000 ---- to 0xBFFF ----
                         Upper Range: 0xC100 ---- to 0xFFFF ----
                         Total: 24318
                }
*/
enum AllKeyIds{
    STREAMRX            = 0xA1000000,    /**< @h2xmle_name{StreamRX} */
    DEVICERX            = 0xA2000000,    /**< @h2xmle_name{DeviceRX} */
    DEVICETX            = 0xA3000000,    /**< @h2xmle_name{DeviceTX} */
    VOLUME              = 0xA4000000,    /**< @h2xmle_name{Volume} */
    SAMPLINGRATE        = 0xA5000000,    /**< @h2xmle_name{SamplingRate} */
    BITWIDTH            = 0xA6000000,    /**< @h2xmle_name{BitWidth} */
    PAUSE               = 0xA7000000,    /**< @h2xmle_name{Pause} */
    MUTE                = 0xA8000000,    /**< @h2xmle_name{Mute} */
    CHANNELS            = 0xA9000000,    /**< @h2xmle_name{Channels} */
    ECNS                = 0xAA000000,    /**< @h2xmle_name{ECNS} */
    INSTANCE            = 0xAB000000,    /**< @h2xmle_name{Instance} */
    DEVICEPP_RX         = 0xAC000000,    /**< @h2xmle_name{DevicePP_Rx} */
    DEVICEPP_TX         = 0xAD000000,    /**< @h2xmle_name{DevicePP_Tx} */
    MEDIA_FMT_ID        = 0xAE000000,    /**< @h2xmle_name{MediaFmtID} */
    STREAMPP_RX         = 0xAF000000,    /**< @h2xmle_name{StreamPP_RX} */
    STREAMPP_TX         = 0xB0000000,    /**< @h2xmle_name{StreamPP_TX} */
    STREAMTX            = 0xB1000000,    /**< @h2xmle_name{StreamTX} */
    EQUALIZER_SWITCH    = 0xB2000000,    /**< @h2xmle_name{Equalizer} */
    VSID                = 0xB3000000,    /**< @h2xmle_name{VSID} */
    BT_PROFILE          = 0xB4000000,    /**< @h2xmle_name{BtProfile} */
    BT_FORMAT           = 0xB5000000,    /**< @h2xmle_name{BtFormat} */
    PBE_SWITCH          = 0xB6000000,    /**< @h2xmle_name{PBE_Switch} */
    BASS_BOOST_SWITCH   = 0xB7000000,    /**< @h2xmle_name{BASS_BOOST_Switch} */
    REVERB_SWITCH       = 0xB8000000,    /**< @h2xmle_name{Reverb_Switch} */
    VIRTUALIZER_SWITCH  = 0xB9000000,    /**< @h2xmle_name{Virtualizer_Switch} */
    SW_SIDETONE         = 0xBA000000,    /**< @h2xmle_name{SW_Sidetone} */
    TAG_KEY_SLOW_TALK   = 0xBB000000,    /**< @h2xmle_name{Stream_SlowTalk} */
    STREAM_CONFIG       = 0xBC000000,    /**< @h2xmle_name{Stream_Config} */
    TAG_KEY_MUX_DEMUX_CONFIG = 0xBD000000,    /**< @h2xmle_name{Stream_MuxDemux} */
    SPK_PRO_DEV_MAP     = 0xBE000000,    /**< @h2xmle_name{SP_Dev_Map} */
    SPK_PRO_VI_MAP      = 0xBF000000,    /**< @h2xmle_name{SP_VI_Map} */
    RAS_SWITCH          = 0xD0000000,    /**< @h2xmle_name{RAS_Switch} */
    PROXY_TX_TYPE       = 0xD1000000,    /**< @h2xmle_name{ProxyTxType} */
    GAIN                = 0xD2000000,    /**< @h2xmle_name{Gain} */
    STREAM              = 0xD3000000,    /**< @h2xmle_name{Stream} */
    STREAM_CHANNELS     = 0xD4000000,    /**< @h2xmle_name{StreamChannels} */
    ICL                 = 0xD5000000,    /**< @h2xmle_name{Input_Current_Limiter} */
    ASPHERE_SWITCH      = 0xD6000000,    /**< @h2xmle_name{Asphere} */
    DEVICETX_EXT        = 0xD7000000,    /**< @h2xmle_name{DeviceTX_EXT} */
    LOGGING             = 0xD8000000,    /**< @h2xmle_name{DataLogging} */
    BMT                 = 0xD9000000,    /**< @h2xmle_name{BMT} */
    FNB                 = 0xDA000000,    /**< @h2xmle_name{FNB} */
    SUMX                = 0xDB000000,    /**< @h2xmle_name{SUMX} */
    AVC                 = 0xDC000000,    /**< @h2xmle_name{AVC} */
    VMI                 = 0xDD000000,     /**< @h2xmle_name{VMID} */
    TAG_KEY_DTMF_SWITCH   = 0xDE000000,    /**< @h2xmle_name{Dtmf} */
    TAG_KEY_DTMF_GEN_TONE = 0xDF000000,    /**< @h2xmle_name{Dtmf_Gen_KeyTone} */
    TAG_KEY_SLOT_MASK     = 0xE0000000,    /**< @h2xmle_name{SlotMask} */
    TAG_KEY_DUTY_CYCLE    = 0xE1000000,    /**< @h2xmle_name{DutyCycle} */
    TAG_KEY_ORIENTATION   = 0xE2000000,  /**< @h2xmle_name{Orientation} */
    SPK_PRO_CPS_MAP       = 0xE3000000,    /**< @h2xmle_name{SP_CPS_Map} */
    HAPTICS_PRO_VI_MAP    = 0xE4000000,    /**< @h2xmle_name{HAPTICS_VI_Map} */
    HAPTICS_PRO_DEV_MAP   = 0xE5000000,    /**< @h2xmle_name{HAPTICS_Dev_Map} */
    USB_VENDOR_ID         = 0xE6000000,    /**< @h2xmle_name{USB_Vendor_Id} */
    TAG_KEY_ULTRASOUND_GAIN = 0xE7000000,   /**< @h2xmle_name{UltrasoundGain} */
    PROXY_RX_TYPE         = 0xE7010000,   /**< @h2xmle_name{ProxyRxType} */
};

/**
    @h2xmlk_key {STREAMRX}
    @h2xmlk_description {Type of Rx Stream}
    @range              {0xA1000001..0xA100FFFF}
*/
enum Key_StreamRX {
    PCM_DEEP_BUFFER                 = 0xA1000001,    /**< @h2xmle_name {PCM_Deep_Buffer}*/
    PCM_RX_LOOPBACK                 = 0xA1000003,    /**< @h2xmle_name {PCM_Rx_Loopback}*/
    VOIP_RX_PLAYBACK                = 0xA1000005,    /**< @h2xmle_name {Voip_Rx}*/
    COMPRESSED_OFFLOAD_PLAYBACK     = 0xA100000A,    /**< @h2xmle_name {Compress_Offload_Playback}*/
    HFP_RX_PLAYBACK                 = 0xA100000C,    /**< @h2xmle_name {HFP_Rx_Playback}*/
    HFP_TX_PLAYBACK                 = 0xA100000D,    /**< @h2xmle_name {HFP_Tx_Playback}*/
    PCM_LL_PLAYBACK                 = 0xA100000E,    /**< @h2xmle_name {PCM_LL_Playback}*/
    PCM_OFFLOAD_PLAYBACK            = 0xA100000F,    /**< @h2xmle_name {PCM_Offload}*/
    VOICE_CALL_RX                   = 0xA1000010,    /**< @h2xmle_name {Voice_Call_Rx}*/
    PCM_ULL_PLAYBACK                = 0xA1000011,    /**< @h2xmle_name {PCM_ULL_Playback}*/
    PCM_PROXY_PLAYBACK              = 0xA1000012,    /**< @h2xmle_name {PCM_Proxy_Playback}*/
    INCALL_MUSIC                    = 0xA1000013,    /**< @h2xmle_name {Incall_Music}*/
    GENERIC_PLAYBACK                = 0xA1000014,    /**< @h2xmle_name {Generic_Playback}*/
    HAPTICS_PLAYBACK                = 0xA1000015,    /**< @h2xmle_name {Haptics_Playback}*/
    VOICE_CALL_RX_HPCM_PLAYBACK     = 0xA1000016,    /**< @h2xmle_name {Voice_Call_Rx_HPCM_Playback}*/
    VOICE_CALL_TX_HPCM_PLAYBACK     = 0xA1000017,    /**< @h2xmle_name {VOICE_CALL_Tx_HPCM_Playback}*/
    SPATIAL_AUDIO_PLAYBACK          = 0xA1000018,    /**< @h2xmle_name {Spatial_Audio_Playback}*/
    RAW_PLAYBACK                    = 0xA1000019,    /**< @h2xmle_name {RAW_Playback}*/
    INCALL_MUSIC_PLUS               = 0xA100001A,    /**< @h2xmle_name {Incall_Music_Plus}*/
    INCALL_MUSIC_COMPRESS_UPLINK    = 0xA100001B,    /**< @h2xmle_name {Incall_Music_Compress_Uplink}*/
    INCALL_MUSIC_COMPRESS_DOWNLINK  = 0xA100001C,    /**< @h2xmle_name {Incall_Music_Compress_Downlink}*/
};

/**
    @h2xmlk_key         {STREAMTX}
    @h2xmlk_description {Type of Tx Stream}
    @range              {0xB1000001..0xB100FFFF}
*/
enum Key_StreamTX {
    PCM_RECORD                      = 0xB1000001,    /**< @h2xmle_name {PCM_Record}*/
    PCM_TX_LOOPBACK                 = 0xB1000002,    /**< @h2xmle_name {PCM_Tx_Loopback}*/
    VOICE_UI                        = 0xB1000003,    /**< @h2xmle_name {Voice_UI}*/
    VOIP_TX_RECORD                  = 0xB1000004,    /**< @h2xmle_name {Voip_Tx}*/
    HFP_RX_CAPTURE                  = 0xB1000005,    /**< @h2xmle_name {HFP_Rx_Capture}*/
    HFP_TX_CAPTURE                  = 0xB1000006,    /**< @h2xmle_name {HFP_Tx_Capture}*/
    VOICE_CALL_TX                   = 0xB1000007,    /**< @h2xmle_name {Voice_Call_Tx}*/
    DEEPBUFFER_RECORD               = 0xB1000008,    /**< @h2xmle_name {DeepBuffer_Record}*/
    RAW_RECORD                      = 0xB1000009,    /**< @h2xmle_name {RAW_Record}*/
    PCM_ULL_RECORD                  = 0xB100000A,    /**< @h2xmle_name {PCM_ULL_Record}*/
    PCM_PROXY_RECORD                = 0xB100000B,    /**< @h2xmle_name {PCM_Proxy_Record}*/
    INCALL_RECORD                   = 0xB100000C,    /**< @h2xmle_name {Incall_Record}*/
    ACD                             = 0xB100000D,    /**< @h2xmle_name {ACD}*/
    SENSOR_PCM_DATA                 = 0xB100000E,    /**< @h2xmle_name {SENSOR_PCM_DATA}*/
    VOICE_CALL_RX_HPCM_RECORD       = 0xB100000F,    /**< @h2xmle_name {VOICE_CALL_Rx_HPCM_Record}*/
    VOICE_CALL_TX_HPCM_RECORD       = 0xB1000010,    /**< @h2xmle_name {Voice_Call_Tx_HPCM_Record}*/
    VOICE_RECOGNITION_RECORD        = 0xB1000011,    /**< @h2xmle_name {Voice_Recognition_Record}*/
    COMPRESS_CAPTURE                = 0xB1000012,    /**< @h2xmle_name {COMPRESS_CAPTURE}*/
};

/**
    @h2xmlk_key {INSTANCE}
    @h2xmlk_description {Stream Instance Id}
*/
enum Key_Instance {
    INSTANCE_1 = 1, /**< @h2xmle_name {Instance_1}*/
    INSTANCE_2 = 2, /**< @h2xmle_name {Instance_2}*/
    INSTANCE_3 = 3, /**< @h2xmle_name {Instance_3}*/
    INSTANCE_4 = 4, /**< @h2xmle_name {Instance_4}*/
    INSTANCE_5 = 5, /**< @h2xmle_name {Instance_5}*/
    INSTANCE_6 = 6, /**< @h2xmle_name {Instance_6}*/
    INSTANCE_7 = 7, /**< @h2xmle_name {Instance_7}*/
    INSTANCE_8 = 8, /**< @h2xmle_name {Instance_8}*/
};

/**
    @h2xmlk_key         {DEVICERX}
    @h2xmlk_description {Rx Device}
    @range       {0xA2000001..0xA200FFFF}
*/
enum Key_DeviceRX {
    SPEAKER        = 0xA2000001, /**< @h2xmle_name {Speaker}*/
    HEADPHONES     = 0xA2000002, /**< @h2xmle_name {Headphones}*/
    BT_RX          = 0xA2000003, /**< @h2xmle_name {BT_Rx}*/
    HANDSET        = 0xA2000004, /**< @h2xmle_name {Handset}*/
    USB_RX         = 0xA2000005, /**< @h2xmle_name {USB_Rx}*/
    HDMI_RX        = 0xA2000006, /**< @h2xmle_name {HDMI_Rx}*/
    PROXY_RX       = 0xA2000007, /**< @h2xmle_name {Proxy_Rx}*/
    PROXY_RX_VOICE = 0xA2000008, /**< @h2xmle_name {Proxy_Rx_Voice}*/
    HAPTICS_DEVICE = 0xA2000009, /**< @h2xmle_name {Haptics_Device}*/
    ULTRASOUND_RX  = 0xA200000A, /**< @h2xmle_name {ULTRASOUND_RX}*/
    ULTRASOUND_RX_DEDICATED = 0xA200000B, /**< @h2xmle_name {ULTRASOUND_RX_DEDICATED}*/
    DUMMY_RX       = 0xA200000C,   /**< @h2xmle_name {Dummy_Rx}*/
};

/**
    @h2xmlk_key         {DEVICETX}
    @h2xmlk_description {Tx Device}
    @range       {0xA3000001..0xA300FFFF}
*/
enum Key_DeviceTX {
     SPEAKER_MIC     = 0xA3000001, /**< @h2xmle_name {Speaker_Mic}*/
     BT_TX           = 0xA3000002, /**< @h2xmle_name {BT_Tx}*/
     HEADPHONE_MIC   = 0xA3000003, /**< @h2xmle_name {Headphone_Mic}*/
     HANDSETMIC      = 0xA3000004, /**< @h2xmle_name {Handset_Mic}*/
     USB_TX          = 0xA3000005, /**< @h2xmle_name {USB_Tx}*/
     HANDSETMIC_VA   = 0xA3000006, /**< @h2xmle_name {VoiceActivation_Mic}*/
     HEADSETMIC_VA   = 0xA3000007, /**< @h2xmle_name {VoiceActivation_Headset_Mic}*/
     PROXY_TX        = 0xA3000008, /**< @h2xmle_name {Proxy_Tx} */
     VI_TX           = 0xA3000009, /**< @h2xmle_name {VI_TX} */
     FM_TX           = 0xA300000A, /**< @h2xmle_name {FM_TX} */
     ULTRASOUND_TX   = 0xA300000B, /**< @h2xmle_name {Ultrasound_TX} */
     HAPTICS_VI_TX   = 0xA300000C, /**< @h2xmle_name {Haptics_VI_Tx} */
     ECHO_REF_TX     = 0xA300000D, /**< @h2xmle_name {EchoReference_TX} */
     CPS_TX          = 0xA300000E, /**< @h2xmle_name {CPS_TX} */
     CPS2_TX         = 0xA300000F, /**< @h2xmle_name {CPS2_TX} */
     DUMMY_TX        = 0xA3000010, /**< @h2xmle_name {Dummy_Tx}*/
};

/**
    @h2xmlk_key         {DEVICETX_EXT}
    @h2xmlk_description {Ext Tx Device}
    @range       {0xD7000001..0xD700FFFF}
*/
enum Key_DeviceTX_Ext {
    EXT_EC_TX    = 0xD7000001, /**< @h2xmle_name {Ext_EC_Tx}*/
};

/**
    @h2xmlk_key         {DEVICEPP_RX}
    @h2xmlk_description {Rx Device Post/Pre Processing Chain}
    @range       {0xAC000001..0xAC00FFFF}
*/
enum Key_DevicePP_RX {
    DEVICEPP_RX_DEFAULT            = 0xAC000001, /**< @h2xmle_name {DevicePP_Rx_Default} @h2xmlk_description {Default PP}*/
    DEVICEPP_RX_AUDIO_MBDRC        = 0xAC000002, /**< @h2xmle_name {Audio_MBDRC} @h2xmlk_description {Audio Rx MBDRC PP }*/
    DEVICEPP_RX_VOIP_MBDRC         = 0xAC000003, /**< @h2xmle_name {Voip_MBDRC} @h2xmlk_description {Voip Rx MBDRC}*/
    DEVICEPP_RX_HFPSINK            = 0xAC000004, /**< @h2xmle_name {HFP_Sink} @h2xmlk_description {HFP Sink}*/
    DEVICEPP_RX_VOICE_DEFAULT      = 0xAC000005, /**< @h2xmle_name {DevicePP_RX_Voice_Default} @h2xmlk_description {Voice call default}*/
    DEVICEPP_RX_ULTRASOUND_GENERATOR  = 0xAC000006, /**< @h2xmle_name {DevicePP_RX_Ultrasound_Gen} @h2xmlk_description {Ultrasound GEN}*/
    DEVICEPP_RX_VOICE_RVE          = 0xAC000007, /**< @h2xmle_name {DevicePP_RX_Voice_RVE} @h2xmlk_description {Voice call RVE}*/
    DEVICEPP_RX_HPCM               = 0xAC000008, /**< @h2xmle_name {DEVICEPP_RX_HPCM} @h2xmlk_description {HPCM RX}*/
    DEVICEPP_RX_VOICE_Fluence_NN_NS  = 0xAC000009, /**< @h2xmle_name {DEVICEPP_RX_VOICE_Fluence_NN_NS} @h2xmlk_description {Voice Rx Fluence_NN_NS}*/
    DEVICEPP_RX_VOIP_Fluence_NN_NS   = 0xAC00000A, /**< @h2xmle_name {DEVICEPP_RX_VOIP_Fluence_NN_NS} @h2xmlk_description {VoIP Rx Fluence_NN_NS}*/
    DEVICEPP_RX_AUDIO_MSPP         = 0xAC00000B, /**< @h2xmle_name {Audio_MSPP} @h2xmlk_description {Audio Rx MSPP PP}*/
    DEVICEPP_RX_BTSINK             = 0xAC00000C, /**< @h2xmle_name {{Device_PP_RX_BtSink} @h2xmlk_description {BT Sink Device PP RX} */
    DEVICEPP_RX_HAPTICS_GENERATOR  = 0xAC00000D, /**< @h2xmle_name {Haptics_generator} @h2xmlk_description {Haptics_gen}*/
};

/**
    @h2xmlk_key         {DEVICEPP_TX}
    @h2xmlk_description {Tx Device Post/Pre Processing Chain}
    @range       {0xAD000001..0xAD00FFFF}
*/
enum Key_DevicePP_TX {
    DEVICEPP_TX_FLUENCE_FFECNS          = 0xAD000001, /**< @h2xmle_name {Fluence_FFECNS} @h2xmlk_description {Used in NLPI use-cases for VUI, ACD, SENSOR_PCM_DATA}*/
    DEVICEPP_TX_AUDIO_FLUENCE_SMECNS    = 0xAD000002, /**< @h2xmle_name {Audio_Fluence_SMECNS} @h2xmlk_description {Single Mic ECNS }*/
    DEVICEPP_TX_AUDIO_FLUENCE_ENDFIRE   = 0xAD000003, /**< @h2xmle_name {Audio_Fluence_Endfire} @h2xmlk_description {EndFire_ECNS - Typically used for dual mic capture scenarios}*/
    DEVICEPP_TX_AUDIO_FLUENCE_PRO       = 0xAD000004, /**< @h2xmle_name {Audio_Fluence_Pro} @h2xmlk_description {Audio Multi MIC scenarios ; at least 3 or more Micss}*/
    DEVICEPP_TX_VOIP_FLUENCE_PRO        = 0xAD000005, /**< @h2xmle_name {Voip_Fluence_Pro} @h2xmlk_description {Voip Multi MIC scenarios ; at least 3 or more Micss}*/
    DEVICEPP_TX_HFP_SINK_FLUENCE_SMECNS = 0xAD000006, /**< @h2xmle_name {HFP_Sink_Fluence_SMECNS} @h2xmlk_description {HFP Sink Single Mic ECNS }*/
    DEVICEPP_TX_VOIP_FLUENCE_SMECNS     = 0xAD000007, /**< @h2xmle_name {Voip_Fluence_SMECNS} @h2xmlk_description {Voip SMECNS scenarios ;1 mic}*/
    DEVICEPP_TX_VOICE_FLUENCE_SMECNS    = 0xAD000008, /**< @h2xmle_name {Voice_Fluence_SMECNS} @h2xmlk_description {Single Mic ECNS in voice call}*/
    DEVICEPP_TX_VOICE_FLUENCE_ENDFIRE   = 0xAD000009, /**< @h2xmle_name {Voice_Fluence_Endfire} @h2xmlk_description {EndFire_ECNS - Typically used for dual mic capture scenarios in voice call}*/
    DEVICEPP_TX_VOICE_FLUENCE_PRO       = 0xAD00000A, /**< @h2xmle_name {Voice_Fluence_Pro} @h2xmlk_description {Multi MIC scenarios ; at least 3 or more Micss in a voice call}*/
    DEVICEPP_TX_FLUENCE_FFNS            = 0xAD00000B, /**< @h2xmle_name {Fluence_FFNS} @h2xmlk_description {Used in LPI use-cases for VUI, SENSOR_PCM_DATA}*/
    DEVICEPP_TX_RAW_LPI                 = 0xAD00000C, /**< @h2xmle_name {RAW_LPI} @h2xmlk_description {Used in LPI use-cases for 3rd party wakeup module in VUI and SENSOR_PCM_DATA}*/
    DEVICEPP_TX_VOIP_FLUENCE_ENDFIRE    = 0xAD00000D, /**< @h2xmle_name {Voip_Fluence_Endfire} @h2xmlk_description {Voip Endfire scenarios ;2 mic}*/
    DEVICEPP_TX_RAW_NLPI                = 0xAD00000E, /**< @h2xmle_name {RAW_NLPI} @h2xmlk_description {Used in NLPI use-cases for 3rd party wakeup module}*/
    DEVICEPP_TX_VOICE_FLUENCE_NN        = 0xAD00000F, /**< @h2xmle_name {Voice_Fluence_NN} @h2xmlk_description {Fluence NN in voice call ; single and dual mic}*/
    DEVICEPP_TX_VOIP_FLUENCE_NN         = 0xAD000010, /**< @h2xmle_name {Voip_Fluence_NN} @h2xmlk_description {Fluence NN in Voip call ; single and dual mic}*/
    DEVICEPP_TX_ULTRASOUND_DETECTOR     = 0xAD000011, /**< @h2xmle_name {Ultrasound_Engine} @h2xmlk_description {Used in Ultrasound Engine use-cases}*/
    DEVICEPP_TX_FLUENCE_FFEC            = 0xAD000012, /**< @h2xmle_name {Fluence_FFEC} @h2xmlk_description {Used in NLPI use-cases for SENSOR_PCM_DATA}*/
    DEVICEPP_TX_VOICE_FLUENCE_ENDFIRE_RVE = 0xAD000013, /**< @h2xmle_name {Voice_Fluence_Endfire_RVE} @h2xmlk_description {EndFire_ECNS - Typically used for dual mic capture scenarios in voice call}*/
    DEVICEPP_TX_HPCM                      = 0xAD000014, /**< @h2xmle_name {DEVICEPP_TX_HPCM} @h2xmlk_description {HPCM TX}*/
    DEVICEPP_TX_HFP_SINK_FLUENCE_ENDFIRE  = 0xAD000015, /**< @h2xmle_name {HFP_Sink_Fluence_ENDFIRE} @h2xmlk_description {EndFire_ECNS - Typically used for dual mic capture scenarios in BT HFP client voice call}*/
    DEVICEPP_TX_AUDIO_FLUENCE_NN          = 0xAD000016, /**< @h2xmle_name {Audio_Fluence_NN} @h2xmlk_description {Fluence NN in Audio Record ; single and dual mic}*/
    DEVICEPP_TX_VOICE_RECOGNITION       = 0xAD000017, /**< @h2xmle_name {Voice_Recognition} @h2xmlk_description {Voice_Recognition - Specifically used for voice recognition}*/
    DEVICEPP_TX_HFP_SINK_FLUENCE_NN_SM  = 0xAD000018, /**< @h2xmle_name {HFP_Sink_Fluence_NN_SM} @h2xmlk_description {Single Mic fluence NN in HFP Sink}*/
    DEVICEPP_TX_AAD                       = 0xAD000019, /**< @h2xmle_name {AAD} @h2xmlk_description {Used in LPI use-cases for ACD}*/
    DEVICEPP_TX_FLUENCE_FFNS_AAD          = 0xAD00001A, /**< @h2xmle_name {Fluence_FFNS_AAD} @h2xmlk_description {Used in LPI use-cases for VUI, SENSOR_PCM_DATA}*/
    DEVICEPP_TX_RAW_LPI_AAD               = 0xAD00001B, /**< @h2xmle_name {RAW_LPI_AAD} @h2xmlk_description {Used in LPI use-cases for 3rd party wakeup module in VUI and SENSOR_PCM_DATA}*/
    DEVICEPP_TX_VOICE_FLUENCE_NN_DM     = 0xAD00001C, /**< @h2xmle_name {Voice_Fluence_NN_DM} @h2xmlk_description {Dual Mic fluence NN in voice call}*/
    DEVICEPP_TX_HFP_SINK_FLUENCE_NN_DM  = 0xAD00001D, /**< @h2xmle_name {HFP_Sink_Fluence_NN_DM} @h2xmlk_description {Dual Mic fluence NN in HFP Sink}*/
    DEVICEPP_TX_AUDIO_FLUENCE_NN_DM       = 0xAD00001E, /**< @h2xmle_name {Audio_Fluence_NN_DM} @h2xmlk_description {Dual Mic fluence NN in Audio Record}*/
    DEVICEPP_TX_VOICE_FLUENCE_ENDFIRE_FNN_NS  = 0xAD00001F, /**< @h2xmle_name {Voice_Fluence_Endfire_FNN} @h2xmlk_description {Endfire_FNN in voice call}*/
    DEVICEPP_TX_VOICE_FLUENCE_PRO_FNN_NS      = 0xAD000020, /**< @h2xmle_name {Voice_Fluence_Pro_FNN} @h2xmlk_description {Pro_FNN in voice call}*/
    DEVICEPP_TX_VOIP_FLUENCE_ENDFIRE_FNN_NS   = 0xAD000021, /**< @h2xmle_name {Voip_Fluence_Endfire_FNN} @h2xmlk_description {Endfire_FNN in voip call}*/
    DEVICEPP_TX_VOIP_FLUENCE_PRO_FNN_NS       = 0xAD000022, /**< @h2xmle_name {Voip_Fluence_Pro_FNN} @h2xmlk_description {Pro_FNN in voip call}*/
    DEVICEPP_TX_AUDIO_RECORD_ENQORE     = 0xAD000023, /**< @h2xmle_name {Audio_Record_Enqore} @h2xmlk_description {Enqore Audio Record}*/
    DEVICEPP_TX_VOICE_FLUENCE_NN_RVE        = 0xAD000024, /**< @h2xmle_name {Voice_Fluence_NN_RVE} @h2xmlk_description {Fluence NN in voice call ; single and dual mic}*/
    DEVICEPP_TX_CUSTOM_ECNS                 = 0xAD000025, /**< @h2xmle_name {CUSTOM_ECNS} @h2xmlk_description {Used in NLPI use-cases for VUI with custom config}*/
    DEVICEPP_TX_CUSTOM_NS                   = 0xAD000026, /**< @h2xmle_name {CUSTOM_NS} @h2xmlk_description {Used in LPI use-cases for VUI with custom config}*/
    DEVICEPP_TX_AUDIO_FLUENCE_SMECNS_EXT_EC   = 0xAD000027, /**< @h2xmle_name {Audio_Fluence_SMECNS_EXT_EC} @h2xmlk_description {Single Mic ECNS EXT_EC}*/
    DEVICEPP_TX_AUDIO_FLUENCE_ENDFIRE_EXT_EC  = 0xAD000028, /**< @h2xmle_name {Audio_Fluence_Endfire_EXT_EC} @h2xmlk_description {EndFire_ECNS EXT_EC - Typically used for dual mic capture scenarios}*/
    DEVICEPP_TX_AUDIO_FLUENCE_PRO_EXT_EC      = 0xAD000029, /**< @h2xmle_name {Audio_Fluence_Pro_EXT_EC} @h2xmlk_description {Audio Multi MIC EXT_EC scenarios ; at least 3 or more Micss}*/
    DEVICEPP_TX_VOIP_FLUENCE_SMECNS_EXT_EC    = 0xAD00002A, /**< @h2xmle_name {Voip_Fluence_SMECNS_EXT_EC} @h2xmlk_description {Voip SMECNS EXT_EC scenarios ;1 mic}*/
    DEVICEPP_TX_VOIP_FLUENCE_ENDFIRE_EXT_EC   = 0xAD00002B, /**< @h2xmle_name {Voip_Fluence_Endfire_EXT_EC} @h2xmlk_description {Voip Endfire EXT_EC scenarios ;2 mic}*/
    DEVICEPP_TX_VOIP_FLUENCE_PRO_EXT_EC       = 0xAD00002C, /**< @h2xmle_name {Voip_Fluence_Pro_EXT_EC} @h2xmlk_description {Voip Multi MIC EXT_EC scenarios ; at least 3 or more Micss}*/
    DEVICEPP_TX_VOICE_FLUENCE_SMECNS_EXT_EC   = 0xAD00002D, /**< @h2xmle_name {Voice_Fluence_SMECNS_EXT_EC} @h2xmlk_description {Single Mic ECNS EXT_EC in voice call}*/
    DEVICEPP_TX_VOICE_FLUENCE_ENDFIRE_EXT_EC  = 0xAD00002E, /**< @h2xmle_name {Voice_Fluence_Endfire_EXT_EC} @h2xmlk_description {EndFire_ECNS EXT_EC - Typically used for dual mic capture scenarios in voice call}*/
    DEVICEPP_TX_VOICE_FLUENCE_PRO_EXT_EC      = 0xAD00002F, /**< @h2xmle_name {Voice_Fluence_Pro_EXT_EC} @h2xmlk_description {Multi MIC EXT_EC scenarios ; at least 3 or more Micss in a voice call}*/
    DEVICEPP_TX_EC_REF                        = 0xAD000030, /**< @h2xmle_name {Record_Echo_Ref_Mux} @h2xmlk_description {Mux Capture data and Ec Ref}*/
};

/**
    @h2xmlk_key         {STREAMPP_RX}
    @h2xmlk_description {Rx Stream Post/Pre Processing Chain}
    @range       {0xAF000001..0xAF00FFFF}
*/
enum Key_StreamPP_RX {
    STREAMPP_RX_DEFAULT = 0xAF000001, /**< @h2xmle_name {StreamPP_Rx_Default} @h2xmlk_description {Default PP Playback}*/
};

/**
    @h2xmlk_key         {STREAMPP_TX}
    @h2xmlk_description {Tx Stream Post/Pre Processing Chain}
    @range       {0xB0000001..0xB000FFFF}
*/
enum Key_StreamPP_TX {
    STREAMPP_TX_DEFAULT = 0xB0000001, /**< @h2xmle_name {StreamPP_Tx_Default} @h2xmlk_description {Default PP Capture}*/
};


/**
    @h2xmlk_key {VOLUME}
    @h2xmlk_description {Volume}
*/
enum Key_Volume {
    LEVEL_0 = 0, /**< @h2xmle_name {Level_0}*/
    LEVEL_1 = 1, /**< @h2xmle_name {Level_1}*/
    LEVEL_2 = 2, /**< @h2xmle_name {Level_2}*/
    LEVEL_3 = 3, /**< @h2xmle_name {Level_3}*/
    LEVEL_4 = 4, /**< @h2xmle_name {Level_4}*/
    LEVEL_5 = 5, /**< @h2xmle_name {Level_5}*/
    LEVEL_6 = 6, /**< @h2xmle_name {Level_6}*/
    LEVEL_7 = 7, /**< @h2xmle_name {Level_7}*/
    LEVEL_8 = 8, /**< @h2xmle_name {Level_8}*/
    LEVEL_9 = 9, /**< @h2xmle_name {Level_9}*/
    LEVEL_10 = 10, /**< @h2xmle_name {Level_10}*/
    LEVEL_11 = 11, /**< @h2xmle_name {Level_11}*/
    LEVEL_12 = 12, /**< @h2xmle_name {Level_12}*/
    LEVEL_13 = 13, /**< @h2xmle_name {Level_13}*/
    LEVEL_14 = 14, /**< @h2xmle_name {Level_14}*/
    LEVEL_15 = 15, /**< @h2xmle_name {Level_15}*/
};

/**
    @h2xmlk_key {USB_VENDOR_ID}
    @h2xmlk_description {Usb_Vendor_Id}
*/
enum Key_Usb_Vendor_Id {
    USB_PROD_0 = 0, /**< @h2xmle_name {USB_PROD_0}*/
    USB_PROD_1 = 1, /**< @h2xmle_name {USB_PROD_1}*/
    USB_PROD_2 = 2, /**< @h2xmle_name {USB_PROD_2}*/
    USB_PROD_3 = 3, /**< @h2xmle_name {USB_PROD_3}*/
    USB_PROD_4 = 4, /**< @h2xmle_name {USB_PROD_4}*/
    USB_PROD_5 = 5, /**< @h2xmle_name {USB_PROD_5}*/
    USB_PROD_6 = 6, /**< @h2xmle_name {USB_PROD_6}*/
    USB_PROD_7 = 7, /**< @h2xmle_name {USB_PROD_7}*/
};

/**
    @h2xmlk_key {GAIN}
    @h2xmlk_description {Gain}
*/
enum Key_Gain {
    GAIN_0 = 0, /**< @h2xmle_name {Gain_0}*/
    GAIN_1 = 1, /**< @h2xmle_name {Gain_1}*/
    GAIN_2 = 2, /**< @h2xmle_name {Gain_2}*/
    GAIN_3 = 3, /**< @h2xmle_name {Gain_3}*/
    GAIN_4 = 4, /**< @h2xmle_name {Gain_4}*/
    GAIN_5 = 5, /**< @h2xmle_name {Gain_5}*/
    GAIN_6 = 6, /**< @h2xmle_name {Gain_6}*/
    GAIN_7 = 7, /**< @h2xmle_name {Gain_7}*/
    GAIN_8 = 8, /**< @h2xmle_name {Gain_8}*/
    GAIN_9 = 9, /**< @h2xmle_name {Gain_9}*/
    GAIN_10 = 10, /**< @h2xmle_name {Gain_10}*/
    GAIN_11 = 11, /**< @h2xmle_name {Gain_11}*/
    GAIN_12 = 12, /**< @h2xmle_name {Gain_12}*/
    GAIN_13 = 13, /**< @h2xmle_name {Gain_13}*/
    GAIN_14 = 14, /**< @h2xmle_name {Gain_14}*/
    GAIN_15 = 15, /**< @h2xmle_name {Gain_15}*/
};

/**
    @h2xmlk_key {SAMPLINGRATE}
    @h2xmlk_sampleRate
    @h2xmlk_description {Sampling Rate}
*/
enum Key_SamplingRate {
    SAMPLINGRATE_8K   = 8000,   /**< @h2xmle_sampleRate{8000} @h2xmle_name {SR_8K}*/
    SAMPLINGRATE_11K  = 11025,  /**< @h2xmle_sampleRate{11025} @h2xmle_name {SR_11K}*/
    SAMPLINGRATE_16K  = 16000,  /**< @h2xmle_sampleRate{16000} @h2xmle_name {SR_16K}*/
    SAMPLINGRATE_22K  = 22050,  /**< @h2xmle_sampleRate{22050} @h2xmle_name {SR_22K}*/
    SAMPLINGRATE_24K  = 24000,  /**< @h2xmle_sampleRate{24000} @h2xmle_name {SR_24K}*/
    SAMPLINGRATE_32K  = 32000,  /**< @h2xmle_sampleRate{32000} @h2xmle_name {SR_32K}*/
    SAMPLINGRATE_44K  = 44100,  /**< @h2xmle_sampleRate{44100} @h2xmle_name {SR_44.1K}*/
    SAMPLINGRATE_48K  = 48000,  /**< @h2xmle_sampleRate{48000} @h2xmle_name {SR_48K}*/
    SAMPLINGRATE_64K  = 64000,  /**< @h2xmle_sampleRate{64000} @h2xmle_name {SR_64K}*/
    SAMPLINGRATE_88K  = 88200,  /**< @h2xmle_sampleRate{88200} @h2xmle_name {SR_88K}*/
    SAMPLINGRATE_96K  = 96000,  /**< @h2xmle_sampleRate{96000} @h2xmle_name {SR_96K}*/
    SAMPLINGRATE_176K = 176400, /**< @h2xmle_sampleRate{176400} @h2xmle_name {SR_176K}*/
    SAMPLINGRATE_192K = 192000, /**< @h2xmle_sampleRate{192000} @h2xmle_name {SR_192K}*/
    SAMPLINGRATE_352K = 352800, /**< @h2xmle_sampleRate{352800} @h2xmle_name {SR_352.8K}*/
    SAMPLINGRATE_384K = 384000, /**< @h2xmle_sampleRate{384000} @h2xmle_name {SR_384K}*/
};
/**
    @h2xmlk_key {BITWIDTH}
    @h2xmlk_description {Bit Width}
*/
enum Key_BitWidth {
    BITWIDTH_16 = 16, /**< @h2xmle_name {BW_16}*/
    BITWIDTH_24 = 24, /**< @h2xmle_name {BW_24}*/
    BITWIDTH_32 = 32, /**< @h2xmle_name {BW_32}*/
};
/**
    @h2xmlk_key {PAUSE}
    @h2xmlk_description {Pause}
*/
enum Key_Pause {
    OFF = 0, /**< @h2xmle_name {Off}*/
    ON = 1,  /**< @h2xmle_name {On}*/
};
/**
    @h2xmlk_key {MUTE}
    @h2xmlk_description {Mute}
*/
enum Key_Mute {
    MUTE_OFF = 0, /**< @h2xmle_name {Off}*/
    MUTE_ON = 1,  /**< @h2xmle_name {On}*/
};
/**
    @h2xmlk_key {LOGGING}
    @h2xmlk_description {DataLogging}
*/
enum Key_Logging {
    LOGGING_OFF = 0, /**< @h2xmle_name {Off}*/
    LOGGING_ON = 1, /**< @h2xmle_name {On}*/
};
/**
    @h2xmlk_key {STREAM_CHANNELS}
    @h2xmlk_description {StreamChannels}
*/
enum Key_StreamChannels {
    STREAM_CHANNELS_1 = 1,   /**< @h2xmle_name {STREAM_CHS_1}*/
    STREAM_CHANNELS_2 = 2,   /**< @h2xmle_name {STREAM_CHS_2}*/
};
/**
    @h2xmlk_key {CHANNELS}
    @h2xmlk_description {Channels}
*/
enum Key_Channels {
    CHANNELS_1 = 1,   /**< @h2xmle_name {CHS_1}*/
    CHANNELS_2 = 2,   /**< @h2xmle_name {CHS_2}*/
    CHANNELS_3 = 3,   /**< @h2xmle_name {CHS_3}*/
    CHANNELS_4 = 4,   /**< @h2xmle_name {CHS_4}*/
    CHANNELS_5 = 5,   /**< @h2xmle_name {CHS_5}*/
    CHANNELS_5_1 = 6, /**< @h2xmle_name {CHS_6}*/
    CHANNELS_6 = 6,   /**< @h2xmle_name {CHS_6}*/
    CHANNELS_7 = 7,   /**< @h2xmle_name {CHS_7}*/
    CHANNELS_8 = 8,   /**< @h2xmle_name {CHS_8}*/
    CHANNELS_9 = 9,   /**< @h2xmle_name {CHS_9}*/
    CHANNELS_10 = 10, /**< @h2xmle_name {CHS_10}*/
    CHANNELS_11 = 11, /**< @h2xmle_name {CHS_11}*/
    CHANNELS_12 = 12, /**< @h2xmle_name {CHS_12}*/
    CHANNELS_13 = 13, /**< @h2xmle_name {CHS_13}*/
    CHANNELS_14 = 14, /**< @h2xmle_name {CHS_14}*/
    CHANNELS_15 = 15, /**< @h2xmle_name {CHS_15}*/
    CHANNELS_16 = 16, /**< @h2xmle_name {CHS_16}*/
};

/**
    @h2xmlk_key {ECNS}
    @h2xmlk_description {ECNS}
*/
enum Key_ECNS {
    ECNS_OFF = 0, /**< @h2xmle_name {ECNS_Off}*/
    ECNS_ON  = 1, /**< @h2xmle_name {ECNS_On}*/
    EC_ON    = 2, /**< @h2xmle_name {EC_On}*/
    NS_ON    = 3, /**< @h2xmle_name {NS_On}*/
};

/**
    @h2xmlk_key {TAG_KEY_DTMF_SWITCH}
    @h2xmlk_description {DTMF_SWITCH}
*/

enum Key_MODULE_ENABLE {
    TAG_VALUE_MODULE_DISABLE = 0,
    TAG_VALUE_MODULE_ENABLE = 1,
};

/**
    @h2xmlk_key {TAG_KEY_DTMF_GEN_TONE}
    @h2xmlk_description {DTMF_GEN_TONE}
*/
enum Key_DTMF_GEN {
    DTMF_GEN_1 = 1,
    DTMF_GEN_2 = 2,
    DTMF_GEN_3 = 3,
    DTMF_GEN_4 = 4,
    DTMF_GEN_5 = 5,
    DTMF_GEN_6 = 6,
    DTMF_GEN_7 = 7,
    DTMF_GEN_8 = 8,
    DTMF_GEN_9 = 9,
    DTMF_GEN_10 = 10,
    DTMF_GEN_11 = 11,
    DTMF_GEN_12 = 12,
    DTMF_GEN_13 = 13,
    DTMF_GEN_14 = 14,
    DTMF_GEN_15 = 15,
    DTMF_GEN_16 = 16,
};

/**
@h2xmlk_key {MEDIA_FMT_ID}
@h2xmlk_description {MediaFmtID}
@range       {0xAE000001..0xAE00FFFF}

*/
enum Key_MediaFmtID {
    MEDIA_FMT_PCM       = 0xAE000001,  /**< @h2xmle_name {PCM} @h2xmlk_description {PCM Media format}*/
    MEDIA_FMT_AAC       = 0xAE000002,  /**< @h2xmle_name {AAC} @h2xmlk_description {AAC Media format}*/
    MEDIA_FMT_ALAC      = 0xAE000003,  /**< @h2xmle_name {ALAC} @h2xmlk_description {ALAC Media format}*/
    MEDIA_FMT_APE       = 0xAE000004,  /**< @h2xmle_name {APE} @h2xmlk_description {APE Media format}*/
    MEDIA_FMT_WMAPRO    = 0xAE000005,  /**< @h2xmle_name {WMAPRO} @h2xmlk_description {WMAPRO Media format}*/
    MEDIA_FMT_WMASTD    = 0xAE000006,  /**< @h2xmle_name {WMASTD} @h2xmlk_description {WMASTD Media format}*/
    MEDIA_FMT_FLAC      = 0xAE000007,  /**< @h2xmle_name {FLAC} @h2xmlk_description {FLAC Media format}*/
    MEDIA_FMT_VORBIS    = 0xAE000008,  /**< @h2xmle_name {VORBIS} @h2xmlk_description {VORBIS Media format}*/
    MEDIA_FMT_MP3       = 0xAE000009,  /**< @h2xmle_name {MP3} @h2xmlk_description {MP3 Media format}*/
    MEDIA_FMT_AMRWBPLUS = 0xAE00000A,  /**< @h2xmle_name {AMRWBPLUS} @h2xmlk_description {AMRWBPLUS Media format}*/
    MEDIA_FMT_AMRWB     = 0xAE00000B,  /**< @h2xmle_name {AMRWB} @h2xmlk_description {AMRWB Media format}*/
    MEDIA_FMT_AMRNB     = 0xAE00000C,  /**< @h2xmle_name {AMRNB} @h2xmlk_description {AMRNB Media format}*/
    MEDIA_FMT_EVRC      = 0xAE00000D,  /**< @h2xmle_name {EVRC} @h2xmlk_description {EVRC Media format}*/
    MEDIA_FMT_G711      = 0xAE00000E,  /**< @h2xmle_name {G711} @h2xmlk_description {G711 Media format}*/
    MEDIA_FMT_QCELP     = 0xAE00000F,  /**< @h2xmle_name {QCELP} @h2xmlk_description {QCELP Media format}*/
    MEDIA_FMT_OPUS      = 0xAE000010,  /**< @h2xmle_name {OPUS} @h2xmlk_description {OPUS Media format}*/
};

/**
    @h2xmlk_key {VSID}
    @h2xmlk_description {VSID}
    @range       {0xB3000001..0xB300FFFF}
*/
enum Key_VSID {
    VSID_DEFAULT    = 0xB3000001, /*VSID default*/
    VSID_VOICE1     = 0xB3000002, /*VSID 0x11C05000*/
    VSID_VOICE2     = 0xB3000003, /*VSID 0x11DC5000*/
    VSID_VOICE1_LB  = 0xB3000004, /*VSID 0x12006000*/
    VSID_VOICE2_LB  = 0xB3000005, /*VSID 0x121C6000*/
};

/**
    @h2xmlk_key {EQUALIZER_SWITCH}
    @h2xmlk_description {Equalizer_Switch}
*/
enum Key_EQUALIZER {
    EQUALIZER_OFF = 0, /**< @h2xmle_name {Off}*/
    EQUALIZER_ON  = 1, /**< @h2xmle_name {On}*/
};

/**
    @h2xmlk_key {ASPHERE_SWITCH}
    @h2xmlk_description {Asphere_Switch}
*/
enum Key_ASPHERE {
    ASPHERE_OFF = 0, /**< @h2xmle_name {Off}*/
    ASPHERE_ON = 1, /**< @h2xmle_name {On}*/
};

/**
    @h2xmlk_key {BT_PROFILE}
    @h2xmlk_description {BTProfile}
    @range       {0xB4000001..0xB400FFFF}
*/
enum Key_BtProfile {
    SCO  = 0xB4000001, /**< @h2xmle_name {SCO}*/
    A2DP = 0xB4000002, /**< @h2xmle_name {A2DP}*/
    BLE  = 0xB4000003, /**< @h2xmle_name {BLE}*/
};

/**
    @h2xmlk_key {PROXY_TX_TYPE}
    @h2xmlk_description {ProxyTxType}
    @range       {0xD1000001..0xD100FFFF}
*/
enum Key_ProxyTxType {
    PROXY_TX_DEFAULT  = 0xD1000001, /**< @h2xmle_name {PROXY_TX_DEFAULT}*/
    PROXY_TX_WFD      = 0xD1000002, /**< @h2xmle_name {PROXY_TX_WFD}*/
    PROXY_TX_VOICE_RX = 0xD1000003, /**< @h2xmle_name {PROXY_TX_VOICE_RX}*/
};

/**
    @h2xmlk_key {STREAM}
    @h2xmlk_description {Stream}
    @range       {0xD2000001..0xD200FFFF}
*/
enum Key_Stream {
    NT_DECODE = 0xD2000001, /**< @h2xmle_name {NT_DECODE}*/
    NT_ENCODE = 0xD2000002, /**< @h2xmle_name {NT_ENCODE}*/
};

/**
    @h2xmlk_key {BT_FORMAT}
    @h2xmlk_description {BTFormat}
    @range       {0xB5000001..0xB500FFFF}
*/
enum Key_BtFormat {
    GENERIC       = 0xB5000001, /**< @h2xmle_name {GENERIC}*/
    LDAC          = 0xB5000002, /**< @h2xmle_name {LDAC}*/
    APTX_ADAPTIVE = 0xB5000003, /**< @h2xmle_name {APTX_ADAPTIVE}*/
    SWB           = 0xB5000004, /**< @h2xmle_name {SWB}*/
    LC3           = 0xB5000005, /**< @h2xmle_name {LC3}*/
    AAC_ABR       = 0xB5000006, /**< @h2xmle_name {AAC_ABR}*/
    APTX_AD_QLEA  = 0xB5000007, /**< @h2xmle_name {APTX_AD_QLEA}*/
    APTX_AD_R4    = 0xB5000008, /**< @h2xmle_name {APTX_AD_R4}*/
};

/**
    @h2xmlk_key {VIRTUALIZER_SWITCH}
    @h2xmlk_description {Virtualizer_Switch}
*/
enum Key_VIRTUALIZER {
    VIRTUALIZER_OFF = 0, /**< @h2xmle_name {Off}*/
    VIRTUALIZER_ON  = 1, /**< @h2xmle_name {On}*/
};

/**
    @h2xmlk_key {REVERB_SWITCH}
    @h2xmlk_description {Reverb_Switch}
*/
enum Key_REVERB {
    REVERB_OFF = 0, /**< @h2xmle_name {Off}*/
    REVERB_ON  = 1, /**< @h2xmle_name {On}*/
};

/**
    @h2xmlk_key {PBE_SWITCH}
    @h2xmlk_description {PBE_Switch}
*/
enum Key_PBE {
    PBE_OFF = 0, /**< @h2xmle_name {Off}*/
    PBE_ON  = 1, /**< @h2xmle_name {On}*/
};

/**
    @h2xmlk_key {BASS_BOOST_SWITCH}
    @h2xmlk_description {Bass_Boost_Switch}
*/
enum Key_BASS_BOOST {
    BASS_BOOST_OFF = 0, /**< @h2xmle_name {Off}*/
    BASS_BOOST_ON  = 1, /**< @h2xmle_name {On}*/
};

/**
    @h2xmlk_key {ICL}
    @h2xmlk_description {Icl}
*/
enum Key_ICL {
    ICL_OFF = 0, /**< @h2xmle_name {Off}*/
    ICL_ON = 1, /**< @h2xmle_name {On}*/
};

/**
    @h2xmlk_key {SW_SIDETONE}
    @h2xmlk_description {SW_SIDETONE}
    @range       {0xBA000001..0xD100FFFF}
*/
enum Key_SW_SIDETONE {
    SW_SIDETONE_ON = 0xBA000001, /**< @h2xmle_name {SW_Sidetone}*/
};

/**
    @h2xmlk_key {TAG_KEY_SLOW_TALK}
    @h2xmlk_description {Stream_SlowTalk}
*/
enum Key_Slow_Talk {
    TAG_VALUE_SLOW_TALK_OFF = 0, /**< @h2xmle_name {Off}*/
    TAG_VALUE_SLOW_TALK_ON  = 1, /**< @h2xmle_name {On}*/
};

/**
    @h2xmlk_key {STREAM_CONFIG}
    @h2xmlk_description {StreamConfig}
    @range       {0xBC000001..0xBC00FFFF}
*/
enum Key_StreamConfig {
    STREAM_CFG_VUI_SVA           = 0xBC000001, /**< @h2xmle_name {SVA}*/
    STREAM_CFG_VUI_HW            = 0xBC000002, /**< @h2xmle_name {Hotword}*/
    STREAM_CFG_VUI_SVA5          = 0xBC000003, /**< @h2xmle_name {SVA5_PDK}*/
    STREAM_CFG_VUI_CUSTOM        = 0xBC000004, /**< @h2xmle_name {CUSTOM}*/
    STREAM_CFG_VUI_GMM           = 0xBC000005, /**< @h2xmle_name {SVA_GMM_mmap_based}*/
    STREAM_CFG_VUI_PDK           = 0xBC000006, /**< @h2xmle_name {SVA_PDK6}*/
    STREAM_CFG_ACD_QC            = 0xBC000007, /**< @h2xmle_name {ACD_QC}*/
};

/**
    @h2xmlk_key {TAG_KEY_MUX_DEMUX_CONFIG}
    @h2xmlk_description {Stream_MuxDemux}
*/
enum Key_Mux_Demux_Config {
    TAG_VALUE_MUX_DEMUX_CONFIG_UPLINK                 = 1, /**< @h2xmle_name {UL_Mono}*/
    TAG_VALUE_MUX_DEMUX_CONFIG_DOWNLINK               = 2, /**< @h2xmle_name {DL_Mono}*/
    TAG_VALUE_MUX_DEMUX_CONFIG_UPLINK_DOWNLINK_MONO   = 3, /**< @h2xmle_name {UL_DL_Mono}*/
    TAG_VALUE_MUX_DEMUX_CONFIG_UPLINK_DOWNLINK_STEREO = 4, /**< @h2xmle_name {UL_DL_Stereo}*/
};

/**
    @h2xmlk_key {SPK_PRO_DEV_MAP}
    @h2xmlk_description {SpkrProtDevMap}
*/
enum Key_SpkrProtDevMap {
    SP_DISABLED = 0, /**< @h2xmle_name {Disable_SP}*/
    RIGHT_MONO  = 1, /**< @h2xmle_name {Right_Mono}*/
    LEFT_MONO   = 2,  /**< @h2xmle_name {Left_Mono}*/
    LEFT_RIGHT  = 3, /**< @h2xmle_name {Left_Right}*/
    RIGHT_MONO_WITH_DISABLED_2SP = 4, /**< @h2xmle_name {Left_Mono_with_disable_2sp}*/
    LEFT_RIGHT_WITH_DISABLED_2SP = 5, /**< @h2xmle_name {Left_Right_with_disable_2sp}*/
};

/**
    @h2xmlk_key {SPK_PRO_VI_MAP}
    @h2xmlk_description {SpkrProtVIMap}
*/
enum Key_SpkrProtVIState {
    RIGHT_SPKR   = 0, /**< @h2xmle_name {RightSpeaker}*/
    LEFT_SPKR    = 1, /**< @h2xmle_name {LeftSpeaker}*/
    STEREO_SPKR  = 2, /**< @h2xmle_name {StereoSpeaker}*/
    RIGHT_SPKR_WITH_DISABLED_2SP = 3, /**< @h2xmle_name {RightSpeaker_with_disable_2sp}*/
    STEREO_SPKR_WITH_DISABLED_2SP = 4, /**< @h2xmle_name {StereoSpeaker_with_disable_2sp}*/
};

/**
    @h2xmlk_key {HAPTICS_PRO_DEV_MAP}
    @h2xmlk_description {hapticsDevMap}
*/
enum Key_hapticsDevMap {
    HAPTICS_DISABLE     = 0, /**< @h2xmle_name {Disable_Haptics}*/
    HAPTICS_RIGHT_MONO  = 1, /**< @h2xmle_name {Haptics_right_mono}*/
    HAPTICS_LEFT_MONO   = 2, /**< @h2xmle_name {Haptics_left_mono}*/
    HAPTICS_LEFT_RIGHT  = 3, /**< @h2xmle_name {Haptics_left_right}*/
};

/**
    @h2xmlk_key {HAPTICS_PRO_VI_MAP}
    @h2xmlk_description {hapticsVIMap}
*/
enum Key_hapticsVIMap {
    HAPTICS_VI_DISABLE      = 0, /**< @h2xmle_name {Disable_Haptics_vi}*/
    HAPTICS_VI_RIGHT_MONO   = 1, /**< @h2xmle_name {Haptics_vi_right_mono}*/
    HAPTICS_VI_LEFT_MONO    = 2, /**< @h2xmle_name {Haptics_vi_left_mono}*/
    HAPTICS_VI_LEFT_RIGHT   = 3, /**< @h2xmle_name {Haptics_vi_left_right}*/
};

/**
    @h2xmlk_key {SPK_PRO_CPS_MAP}
    @h2xmlk_description {SpkrProtCPSMap}
*/
enum Key_SpkrProtCPSState {
    R_SPKR   = 0, /**< @h2xmle_name {RightSpeaker}*/
    L_SPKR   = 1, /**< @h2xmle_name {LeftSpeaker}*/
    ST_SPKR  = 2, /**< @h2xmle_name {StereoSpeaker}*/
};


/**
    @h2xmlk_key {RAS_SWITCH}
    @h2xmlk_description {RAS_Switch}
*/
enum Key_RAS {
    RAS_OFF = 0, /**< @h2xmle_name {Off}*/
    RAS_ON  = 1, /**< @h2xmle_name {On}*/
};

/**
    @h2xmlk_key {TAG_KEY_SLOT_MASK}
    @h2xmlk_description {SlotMask}
*/
enum Key_Slot_Mask {
    SLOT_MASK_1_16 = 0x00000001,   /**< @h2xmle_name {MASK_1_16}*/
    SLOT_MASK_2_16 = 0x00000002,   /**< @h2xmle_name {MASK_2_16}*/
    SLOT_MASK_3_16 = 0x00000003,   /**< @h2xmle_name {MASK_3_16}*/
    SLOT_MASK_4_16 = 0x00000004,   /**< @h2xmle_name {MASK_4_16}*/
    SLOT_MASK_5_16 = 0x00000005,   /**< @h2xmle_name {MASK_5_16}*/
    SLOT_MASK_6_16 = 0x00000006,   /**< @h2xmle_name {MASK_6_16}*/
    SLOT_MASK_7_16 = 0x00000007,   /**< @h2xmle_name {MASK_7_16}*/
    SLOT_MASK_1_24 = 0x40000001,   /**< @h2xmle_name {MASK_1_24}*/
    SLOT_MASK_2_24 = 0x40000002,   /**< @h2xmle_name {MASK_2_24}*/
    SLOT_MASK_3_24 = 0x40000003,   /**< @h2xmle_name {MASK_3_24}*/
    SLOT_MASK_4_24 = 0x40000004,   /**< @h2xmle_name {MASK_4_24}*/
    SLOT_MASK_5_24 = 0x40000005,   /**< @h2xmle_name {MASK_5_24}*/
    SLOT_MASK_6_24 = 0x40000006,   /**< @h2xmle_name {MASK_6_24}*/
    SLOT_MASK_7_24 = 0x40000007,   /**< @h2xmle_name {MASK_7_24}*/
    SLOT_MASK_1_32 = 0x80000001,   /**< @h2xmle_name {MASK_1_32}*/
    SLOT_MASK_2_32 = 0x80000002,   /**< @h2xmle_name {MASK_2_32}*/
    SLOT_MASK_3_32 = 0x80000003,   /**< @h2xmle_name {MASK_3_32}*/
    SLOT_MASK_4_32 = 0x80000004,   /**< @h2xmle_name {MASK_4_32}*/
    SLOT_MASK_5_32 = 0x80000005,   /**< @h2xmle_name {MASK_5_32}*/
    SLOT_MASK_6_32 = 0x80000006,   /**< @h2xmle_name {MASK_6_32}*/
    SLOT_MASK_7_32 = 0x80000007,   /**< @h2xmle_name {MASK_7_32}*/
};

/**
    @h2xmlk_key {TAG_KEY_DUTY_CYCLE}
    @h2xmlk_description {DutyCycle}
*/
enum Key_Duty_Cycle {
    DISABLE = 0,   /**< @h2xmle_name {disable}*/
    ENABLE = 1,   /**< @h2xmle_name {enable}*/
};

/**
    @h2xmlk_key {TAG_KEY_ORIENTATION}
    @h2xmlk_description {Device Orientation}
*/
enum Key_Orientation {
    ORIENTATION_0   = 0, /**< @h2xmle_name {Orientation_0}*/
    ORIENTATION_90  = 1, /**< @h2xmle_name {Orientation_90}*/
    ORIENTATION_180 = 2, /**< @h2xmle_name {Orientation_180}*/
    ORIENTATION_270 = 3, /**< @h2xmle_name {Orientation_270}*/
};

/**
    @h2xmlk_key {TAG_KEY_ULTRASOUND_GAIN}
    @h2xmlk_description {UltrasoundGain}
*/
enum Key_Ultrasound_Gain {
    GAIN_MUTE = 0,   /**< @h2xmle_name {Mute}*/
    GAIN_LOW  = 1,   /**< @h2xmle_name {Low}*/
    GAIN_HIGH = 2,   /**< @h2xmle_name {High}*/
};


/**
    @h2xmlk_key         {PROXY_RX_TYPE}
    @h2xmlk_description {Key for proxy record type}
    @range              {0xE7010001..0xE701FFFF}
*/
enum Key_Values_ProxyRxType {
    PROXY_RX_DEFAULT    = 0xE7010001,   /**< @h2xmle_name {PROXY_RX_DEFAULT} @h2xmle_description {Key used to identify unique default device RX subgraph proxy record usecase}*/
    PROXY_RX_WFD        = 0xE7010002,   /**< @h2xmle_name {PROXY_RX_WFD} @h2xmle_description {Key used to identify unique WiFi display device RX subgraph proxy record usecase.}*/
};

/**
    @h2xmlk_gkeys
    @h2xmlk_description {Graph Keys}
*/
enum Graph_Keys {
    gk_StreamRX        = STREAMRX,
    gk_DeviceRX        = DEVICERX,
    gk_DeviceTX        = DEVICETX,
    gk_DevicePP_RX     = DEVICEPP_RX,
    gk_DevicePP_TX     = DEVICEPP_TX,
    gk_Instance        = INSTANCE,
    gk_StreamPP_RX     = STREAMPP_RX,
    gk_StreamPP_TX     = STREAMPP_TX,
    gk_StreamTX        = STREAMTX,
    gk_VSID            = VSID,
    gk_BtProfile       = BT_PROFILE,
    gk_BtFormat        = BT_FORMAT,
    gk_SWSidetone      = SW_SIDETONE,
    gk_StreamConfig    = STREAM_CONFIG,
    gk_ProxyTxType     = PROXY_TX_TYPE,
    gk_Stream          = STREAM,
    gk_DeviceTX_EXT    = DEVICETX_EXT,
    gk_ProxyRxType     = PROXY_RX_TYPE,
};
/**
    @h2xmlk_ckeys
    @h2xmlk_description {Calibration Keys}
*/
enum Cal_Keys {
    ck_volume    = VOLUME,
    ck_channels  = CHANNELS,
    ck_spkrprot  = SPK_PRO_DEV_MAP,
    ck_ras       = RAS_SWITCH,
    ck_spvistate = SPK_PRO_VI_MAP,
    ck_spcpsstate = SPK_PRO_CPS_MAP,
    ck_gain      = GAIN,
    ck_stream_channels = STREAM_CHANNELS,
    ck_hapticsdev = HAPTICS_PRO_DEV_MAP,
    ck_hapticsvi = HAPTICS_PRO_VI_MAP,
    ck_usb_vendor_id = USB_VENDOR_ID,
};


#define DEVICE_HW_ENDPOINT_RX        0xC0000004
/**
    @h2xmlk_modTag {"device_hw_ep_rx",DEVICE_HW_ENDPOINT_RX}
    @h2xmlk_description {Hw EP Rx}
*/
enum HW_ENDPOINT_RX_Keys {
    tk1_hweprx = CHANNELS,
};
#define DEVICE_HW_ENDPOINT_TX        0xC0000005
/**
    @h2xmlk_modTag {"device_hw_ep_tx",DEVICE_HW_ENDPOINT_TX}
    @h2xmlk_description {Hw EP Tx}
*/
enum HW_ENDPOINT_TX_Keys {
    tk1_hweptx = CHANNELS,
};
#define TAG_PAUSE       0xC0000006
/**
    @h2xmlk_modTag {"stream_pause", TAG_PAUSE}
    @h2xmlk_description {Stream Pause}
*/
enum TAG_PAUSE_Keys {
    tk1_Pause = PAUSE,
};
#define TAG_MUTE        0xC0000007
/**
    @h2xmlk_modTag {"stream_mute", TAG_MUTE}
    @h2xmlk_description {Stream Mute}
*/
enum TAG_MUTE_Keys {
    tk1_Mute = MUTE,
};

#define TAG_ECNS  0xC000000A
/**
    @h2xmlk_modTag {"device_ecns", TAG_ECNS}
    @h2xmlk_description {Ecns On/Off}
*/
enum TAG_ECNS_Keys {
    tk1_ECNS = ECNS,
};

#define TAG_STREAM_MFC  0xC000000B
/**
    @h2xmlk_modTag {"stream_mfc", TAG_STREAM_MFC}
    @h2xmlk_description {Stream MFC}
*/
enum TAG_STREAM_MFC_Keys {
    tk1_StreamSamplingRate = SAMPLINGRATE,
    tk2_StreamBitWidth     = BITWIDTH,
    tk3_StreamChannels     = CHANNELS,
};

#define TAG_STREAM_VOLUME  0xC000000D
/**
    @h2xmlk_modTag {"stream_volume", TAG_STREAM_VOLUME}
    @h2xmlk_description {Stream Volume}
*/
enum TAG_STREAM_VOLUME_Keys {
    tk1_Volume = VOLUME,
};

#define TAG_DEVICE_PP_MFC  0xC0000011
/**
    @h2xmlk_modTag {"device_pp_mfc", TAG_DEVICE_PP_MFC}
    @h2xmlk_description {Device PP MFC}
*/
enum TAG_DEVICE_PP_MFC_Keys {
    tk1_device_pp_mfc = SAMPLINGRATE,
    tk2_device_pp_mfc = BITWIDTH,
    tk3_device_pp_mfc = CHANNELS,
};

#define TAG_STREAM_PLACEHOLDER_DECODER  0xC0000012
/**
@h2xmlk_modTag {"stream_placeholder_decoder", TAG_STREAM_PLACEHOLDER_DECODER}
@h2xmlk_description {Placeholder Decoder}
*/
enum TAG_STREAM_PLACEHOLDER_DECODER_Keys {
    tk1_MediaFmtID = MEDIA_FMT_ID,
};

#define TAG_STREAM_EQUALIZER  0xC0000014
/**
@h2xmlk_modTag {"stream_equalizer", TAG_STREAM_EQUALIZER}
@h2xmlk_description {Stream Equalizer}
*/
enum TAG_STREAM_EQUALIZER_Keys {
    tk1_EQUALIZER = EQUALIZER_SWITCH,
};


#define TAG_STREAM_VIRTUALIZER  0xC0000015
/**
@h2xmlk_modTag {"stream_virtualizer", TAG_STREAM_VIRTUALIZER}
@h2xmlk_description {Stream Virtualizer}
*/
enum TAG_STREAM_VIRTUALIZER_Keys {
    tk1_VIRTUALIZER = VIRTUALIZER_SWITCH,
};

#define TAG_STREAM_REVERB  0xC0000016
/**
@h2xmlk_modTag {"stream_reverb", TAG_STREAM_REVERB}
@h2xmlk_description {Stream Reverb}
*/
enum TAG_STREAM_REVERB_Keys {
    tk1_REVERB = REVERB_SWITCH,
};

#define TAG_STREAM_PBE  0xC0000017
/**
@h2xmlk_modTag {"stream_pbe", TAG_STREAM_PBE}
@h2xmlk_description {Stream PBE}
*/
enum TAG_STREAM_PBE_Keys {
    tk1_PBE = PBE_SWITCH,
};

#define TAG_STREAM_BASS_BOOST  0xC0000018
/**
@h2xmlk_modTag {"stream_bass_bost", TAG_STREAM_BASS_BOOST}
@h2xmlk_description {Stream Bass Boost}
*/
enum TAG_STREAM_BASS_BOOST_Keys {
    tk1_BASS_BOOST = BASS_BOOST_SWITCH,
};

#define PER_STREAM_PER_DEVICE_MFC  0xC0000019

/**
    @h2xmlk_modTag {"pspd_mfc", PER_STREAM_PER_DEVICE_MFC}
    @h2xmlk_description {Per Stream Per Device MFC}
*/

enum TAG_PER_STREAM_PER_DEVICE_MFC_Keys {
    tk1_pspd_mfc = SAMPLINGRATE,
    tk2_pspd_mfc = BITWIDTH,
    tk3_pspd_mfc = CHANNELS,
};

#define TAG_STREAM_SLOW_TALK  0xC0000025
/**
    @h2xmlk_modTag {"stream_slowtalk", TAG_STREAM_SLOW_TALK}
    @h2xmlk_description {Slow Talk}
*/
enum TAG_STREAM_SLOW_TALK_keys {
    tk1_slowtalk = TAG_KEY_SLOW_TALK,
};

#define TAG_MODULE_CHANNELS  0xC0000026
/**
    @h2xmlk_modTag {"module_channels", TAG_MODULE_CHANNELS}
    @h2xmlk_description {Module Channels}
*/
enum TAG_MODULE_CHANNELS_keys {
    tk1_module_channels = CHANNELS,
};

#define TAG_STREAM_MUX_DEMUX  0xC0000027
/**
    @h2xmlk_modTag {"stream_muxdemux", TAG_STREAM_MUX_DEMUX}
    @h2xmlk_description {Mux Demux}
*/
enum TAG_STREAM_MUX_DEMUX_keys {
    tk1_muxdemux = TAG_KEY_MUX_DEMUX_CONFIG,
};

#define TAG_DEVICE_PP_MBDRC  0xC000002B
/**
    @h2xmlk_modTag {"device_pp_mbdrc", TAG_DEVICE_PP_MBDRC}
    @h2xmlk_description {Device PP Mbdrc}
*/
enum TAG_DEVICE_PP_MBDRC_Keys {
    tk1_device_pp_mbdrc = GAIN,
};

#define TAG_STREAM_PLACEHOLDER_ENCODER  0xC000002D
/**
@h2xmlk_modTag {"stream_placeholder_encoder", TAG_STREAM_PLACEHOLDER_ENCODER}
@h2xmlk_description {Placeholder Encoder}
*/
enum TAG_STREAM_PLACEHOLDER_ENCODER_Keys {
    tk1_EncMediaFmtID = MEDIA_FMT_ID,
};

#define TAG_DEVICE_AL  0xC0000033
/**
@h2xmlk_modTag {"device_al", TAG_DEVICE_AL}
@h2xmlk_description {Device AL}
*/
enum TAG_DEVICE_AL_Keys {
    tk1_ICL = ICL,
};

#define TAG_ASPHERE  0xC0000034
/**
@h2xmlk_modTag {"audio_sphere", TAG_ASPHERE}
@h2xmlk_description {Sphere}
*/
enum TAG_ASPHERE_Keys {
    tk1_ASPHERE = ASPHERE_SWITCH,
};

#define TAG_DEV_MUTE  0xC0000035
/**
    @h2xmlk_modTag {"device_mute", TAG_DEV_MUTE}
    @h2xmlk_description {Device Mute}
*/
enum TAG_DEV_MUTE_Keys {
    tk1_Dev_Mute = MUTE,
};

#define TAG_DATA_LOGGING  0xC0000036
/**
    @h2xmlk_modTag {"data_logging", TAG_DATA_LOGGING}
    @h2xmlk_description {Data Logging}
*/
enum TAG_DATA_LOGGING_Keys {
    tk1_data_logging = LOGGING,
};

#define DTMF_GENERATOR 0xC0000037

/**
@h2xmlk_modTag {"dtmf_generator", DTMF_GENERATOR}
@h2xmlk_description {DTMF GENERATOR}
*/
enum DTMF_GENERATOR_Keys {
    tk1_DTMF_GENERATOR = TAG_KEY_DTMF_GEN_TONE,
};

#define DTMF_DETECTOR 0xC0000038

/**
@h2xmlk_modTag {"dtmf_detector", DTMF_DETECTOR}
@h2xmlk_description {DTMF DETECTOR}
*/
enum DTMF_DETECTOR_Keys {
    tk1_DTMF_DETECTOR = TAG_KEY_DTMF_SWITCH,
};

#define TAG_MFC_SPEAKER_SWAP 0xC0000039
/**
    @h2xmlk_modTag {"mfc_speaker_swap", TAG_MFC_SPEAKER_SWAP}
    @h2xmlk_description {MFC Speaker Swap}
*/
enum TAG_MFC_SPEAKER_SWAP_Keys {
    tk1_mfc_speaker_swap = SAMPLINGRATE,
    tk2_mfc_speaker_swap = BITWIDTH,
    tk3_mfc_speaker_swap = CHANNELS,
};

#define TAG_DEVICE_MUX 0xC0000040
/**
    @h2xmlk_modTag {"device_mux", TAG_DEVICE_MUX}
    @h2xmlk_description {Channel slot mask}
*/
enum TAG_DEVICE_MUX_Keys {
    tk1_slot_mask = TAG_KEY_SLOT_MASK,
};

#define TAG_DUTY_CYCLE 0xC000003C
/**
    @h2xmlk_modTag {"duty_cycle", TAG_DUTY_CYCLE}
    @h2xmlk_description {UPD duty cycle set}
*/
enum TAG_DUTY_CYCLE_Keys {
    tk1_duty_cycle = TAG_KEY_DUTY_CYCLE,
};

#define TAG_DEVPP_MUTE  0xC0000041
/**
    @h2xmlk_modTag {"devicepp_mute", TAG_DEVPP_MUTE}
    @h2xmlk_description {DevicePP Mute}
*/
enum TAG_DEVPP_MUTE_Keys {
    tk1_DevPP_Mute = MUTE,
};

#define TAG_DEVICEPP_EC_MFC 0xC000003B
/**
    @h2xmlk_modTag {"devicepp_ec_mfc", TAG_DEVICEPP_EC_MFC}
    @h2xmlk_description {Devicepp EC MFC}
*/
enum TAG_DEVICEPP_EC_MFC_Keys {
    tk1_devicepp_ec_mfc = SAMPLINGRATE,
};

#define TAG_ORIENTATION 0xC0000042
/**
    @h2xmlk_modTag {"orientation", TAG_ORIENTATION}
    @h2xmlk_description {orientation}
*/
enum TAG_ORIENTATION_Keys {
    tk1_orientation = TAG_KEY_ORIENTATION,
};

#define TAG_MFC_SIDETONE 0xC000004A
/**
    @h2xmlk_modTag {"mfc_sidetone", TAG_MFC_SIDETONE}
    @h2xmlk_description {SIDETONE MFC}
*/
enum TAG_MFC_SIDETONE_Keys {
    tk1_mfc_sidetone = SAMPLINGRATE,
    tk2_mfc_sidetone = BITWIDTH,
    tk3_mfc_sidetone = CHANNELS,
};

#define TAG_ULTRASOUND_GAIN 0xC000004C
/**
    @h2xmlk_modTag {"ultrasound_gain", TAG_ULTRASOUND_GAIN}
    @h2xmlk_description {Ultrasound Signal gain for different configuration}
*/
enum TAG_ULTRASOUND_GAIN_Keys {
    tk1_ultrasound_gain = TAG_KEY_ULTRASOUND_GAIN,
};

/**
    @h2xmlk_modTagList
    @description{ Unique Tag Definitions that can be used as tags.
                         Range: 0xC000 0000 to 0xC000 FFFF
                         Bits:[16-31]: 0xC000 (Fixed)
                         Bits:[ 0-15]: Reserved for Tag definitions
                         Total: 65535
                       }
    @range      { 0xC0000000..0xC000FFFF }
*/
enum TAGS_DEFINITIONS {
    SHMEM_ENDPOINT              = 0xC0000001, /**< @h2xmle_name {"sh_ep"} */
    STREAM_INPUT_MEDIA_FORMAT   = 0xC0000002, /**< @h2xmle_name {"stream_input_media_format" } */
    STREAM_OUTPUT_MEDIA_FORMAT  = 0xC0000003, /**< @h2xmle_name {"stream_output_media_format" } */
    DEVICE_SVA                  = 0xC0000008, /**< @h2xmle_name {"device_sva"} */
    DEVICE_ADAM                 = 0xC0000009, /**< @h2xmle_name {"device_adam"} */
    DEVICE_MFC                  = 0xC000000C, /**< @h2xmle_name {"device_mfc"} */
    STREAM_PCM_DECODER          = 0xC000000E, /**< @h2xmle_name {"stream_pcm_decoder"} */
    STREAM_PCM_ENCODER          = 0xC000000F, /**< @h2xmle_name {"stream_pcm_encoder"} */
    STREAM_PCM_CONVERTER        = 0xC0000010, /**< @h2xmle_name {"stream_pcm_converter"} */
    STREAM_SPR                  = 0xC0000013, /**< @h2xmle_name {"stream_spr"} */
    BT_PLACEHOLDER_ENCODER      = 0xC0000020, /**< @h2xmle_name {"bt_encoder"} */
    COP_PACKETIZER_V0           = 0xC0000021, /**< @h2xmle_name {"cop_packetizer_v0"} */
    RAT_RENDER                  = 0xC0000022, /**< @h2xmle_name {"rate_adapter_module"} */
    BT_PCM_CONVERTER            = 0xC0000023, /**< @h2xmle_name {"bt_pcm_converter"} */
    BT_PLACEHOLDER_DECODER      = 0xC0000024, /**< @h2xmle_name {"bt_decoder"} */
    MODULE_VI                   = 0xC0000028, /**< @h2xmle_name {"module_vi"} */
    MODULE_SP                   = 0xC0000029, /**< @h2xmle_name {"module_sp"} */
    MODULE_GAPLESS              = 0xC000002A, /**< @h2xmle_name {"module_gapless"} */
    WR_SHMEM_ENDPOINT           = 0xC000002C, /**< @h2xmle_name {"wr_sh_ep"} */
    RD_SHMEM_ENDPOINT           = 0xC000002E, /**< @h2xmle_name {"rd_sh_ep"} */
    COP_PACKETIZER_V2           = 0xC000002F, /**< @h2xmle_name {"cop_packetizer_v2"} */
    COP_DEPACKETIZER_V2         = 0xC0000030, /**< @h2xmle_name {"cop_depacketizer_v2"} */
    CONTEXT_DETECTION_ENGINE    = 0xC0000031, /**< @h2xmle_name {"context_detection_engine"} */
    ULTRASOUND_DETECTION_MODULE = 0xC0000032, /**< @h2xmle_name {"ultrasound_detection_module"} */
    DEVICE_POP_SUPPRESSOR       = 0xC000003A, /**< @h2xmle_name {"device_pop_suppressor"} */
    DEVICE_PP_MSIIR             = 0xC000003D, /**< @h2xmle_name {"device_pp_msiir"} */
    MODULE_HAPTICS_VI           = 0xC000003E, /**< @h2xmle_name {"module_haptics_vi"} */
    MODULE_HAPTICS_GEN          = 0xC000003F, /**< @h2xmle_name {"module_haptics_gen"} */
    TAG_MODULE_MSPP             = 0xC0000043, /**< @h2xmle_name {"module_mspp"} */
    TAG_MODULE_CPS              = 0xC0000044, /**< @h2xmle_name {"module_cps"} */
    MODULE_CONGESTION_BUFFER    = 0xC0000045, /**< @h2xmle_name {"congestion_buffer_module"} */
    MODULE_JITTER_BUFFER        = 0xC0000046, /**< @h2xmle_name {"jitter_buffer_module"} */
    MODULE_VI2                  = 0xC0000047, /**< @h2xmle_name {"module_vi2"} */
    MODULE_SP2                  = 0xC0000048, /**< @h2xmle_name {"module_sp2"} */
    TAG_MODULE_CPS2             = 0xC0000049, /**< @h2xmle_name {"module_cps2"} */
    TAG_MODULE_TSM              = 0xC000004B, /**< @h2xmle_name {"module_tsm"} */
};
typedef enum TAGS_DEFINITIONS TAGS_DEFINITIONS;

#endif
