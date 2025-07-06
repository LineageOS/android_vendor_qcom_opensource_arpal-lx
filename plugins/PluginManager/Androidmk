LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libplugin_manager
LOCAL_MODULE_OWNER := qti
LOCAL_VENDOR_MODULE := true

LOCAL_CPPFLAGS += -fexceptions -frtti

# add for gcov dump
ifeq ($(AUDIO_FEATURE_ENABLED_GCOV), true)
LOCAL_CFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
LOCAL_CPPFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
LOCAL_LDFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
endif

ifeq ($(USE_PAL_STATIC_LINKING_MODULES),true)
    LOCAL_CFLAGS += -DUSE_STATIC_LINKING_MODULES
    LOCAL_SRC_FILES := \
        src/PluginManagerStatic.cpp
else
    LOCAL_SRC_FILES := \
        src/PluginManager.cpp
endif

LOCAL_C_INCLUDES += $(TOP)/system/media/audio_route/include
LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    liblx-osal \
    libexpat

LOCAL_HEADER_LIBRARIES := \
    libarpal_headers \
    libarpal_internalheaders \
    libarvui_intf_headers \
    liblisten_headers \

#used for static compilation
ifeq ($(USE_PAL_STATIC_LINKING_MODULES),true)

    LOCAL_C_INCLUDES += $(TOP)/system/media/audio_route/include
    LOCAL_C_INCLUDES += $(TOP)/system/media/audio/include
    LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/pal/sounddose/inc
    LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/pal/plugins/configs/ConfigSessionAlsaPcm/inc
    LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/pal/plugins/configs/ConfigSessionAlsaCompress/inc
    LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/pal/plugins/configs/ConfigSessionAlsaVoice/inc

    LOCAL_HEADER_LIBRARIES += \
        libarpal_headers \
        libspf-headers \
        libvui_dmgr_headers \
        libarvui_intf_headers \
        libaudiofeaturestats_headers \
        liblisten_headers \
        libacdb_headers \
        libagm_headers \
        libaudioroute \
        libagm_headers \
        libarpal_internalheaders \
        libarmemlog_headers \
        libstream_acd_headers \
        libstream_dummy_headers \
        libstream_common_headers \
        libstream_commonproxy_headers \
        libstream_compress_headers \
        libstream_contextproxy_headers \
        libstream_haptics_headers \
        libstream_incall_headers \
        libstream_nontunnel_headers \
        libstream_pcm_headers \
        libstream_sensorpcmdata_headers \
        libstream_sensorrenderer_headers \
        libstream_soundtrigger_headers \
        libstream_ultrasound_headers \
        libstream_asr_headers \
        libstream_calltranslation_headers \
        libsession_ar_headers \
        libsession_agm_headers \
        libsession_compress_headers \
        libsession_voice_headers \
        libsession_pcm_headers \
        libdev_bt_headers \
        libdev_display_headers \
        libdev_dummy_headers \
        libdev_ecref_headers \
        libdev_extec_headers \
        libdev_fm_headers \
        libdev_handset_headers \
        libdev_handsetmic_headers \
        libdev_handsetva_headers \
        libdev_haptics_headers \
        libdev_headphone_headers \
        libdev_headsetmic_headers \
        libdev_headsetva_headers \
        libdev_proxy_headers \
        libdev_speaker_headers \
        libdev_speakermic_headers \
        libdev_ultrasound_headers \
        libdev_usb_headers \

    ifeq ($(TARGET_USES_QTI_TINYCOMPRESS),true)
    LOCAL_SHARED_LIBRARIES += libqti-tinyalsa libqti-tinycompress
    else
    LOCAL_C_INCLUDES       += $(TOP)/external/tinycompress/include
    LOCAL_SHARED_LIBRARIES += libtinyalsa libtinycompress
    endif
endif #end of static compilation

include $(BUILD_STATIC_LIBRARY)
