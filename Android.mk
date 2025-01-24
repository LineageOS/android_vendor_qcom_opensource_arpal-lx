ifneq ($(AUDIO_USE_STUB_HAL), true)

LOCAL_PATH := $(call my-dir)
PAL_BASE_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libarpal_internalheaders
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)/stream/inc
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)/session/inc
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)/resource_manager/inc
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)/device/inc
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)/utils/inc
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)/context_manager/inc
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)/plugins/codecs
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)/plugins/PluginManager/inc
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)/sounddose/inc/

LOCAL_VENDOR_MODULE := true

include $(BUILD_HEADER_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE        := libar-pal
LOCAL_MODULE_OWNER  := qti
LOCAL_MODULE_TAGS   := optional
LOCAL_VENDOR_MODULE := true

LOCAL_CFLAGS        := -D_ANDROID_
LOCAL_CFLAGS        += -Wno-macro-redefined
LOCAL_CFLAGS        += -Wall -Werror -Wno-unused-variable -Wno-unused-parameter
LOCAL_CFLAGS        += -DCONFIG_GSL
LOCAL_CFLAGS        += -D_GNU_SOURCE
LOCAL_CFLAGS        += -DADSP_SLEEP_MONITOR
ifeq ($(call is-board-platform-in-list,kalama pineapple sun canoe), true)
LOCAL_CFLAGS        += -DSOC_PERIPHERAL_PROT
endif
ifeq ($(AUDIO_FEATURE_STATS_ENABLED), true)
LOCAL_CFLAGS += -DAUDIO_FEATURE_STATS_ENABLED
endif
LOCAL_CPPFLAGS      += -fexceptions -frtti -Wno-ignored-attributes

LOCAL_C_INCLUDES := \
    $(TOP)/system/media/audio_route/include \
    $(TOP)/system/media/audio/include

ifneq ($(TARGET_KERNEL_VERSION), 3.18)
ifneq ($(TARGET_KERNEL_VERSION), 4.14)
ifneq ($(TARGET_KERNEL_VERSION), 4.19)
ifneq ($(TARGET_KERNEL_VERSION), 4.4)
ifneq ($(TARGET_KERNEL_VERSION), 4.9)
ifneq ($(TARGET_KERNEL_VERSION), 5.4)
LOCAL_HEADER_LIBRARIES += generated_kernel_headers
endif
endif
endif
endif
endif
endif

LOCAL_C_INCLUDES              += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_C_INCLUDES              += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/techpack/audio/include

LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_EXPORT_C_INCLUDE_DIRS   := $(LOCAL_PATH)/inc \

# add for gcov dump
ifeq ($(AUDIO_FEATURE_ENABLED_GCOV), true)
LOCAL_CFLAGS += -DAUDIO_FEATURE_ENABLED_GCOV -g --coverage -fprofile-arcs -ftest-coverage
LOCAL_CPPFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
LOCAL_LDFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
endif

LOCAL_SRC_FILES := \
    Pal.cpp \
    stream/src/Stream.cpp \
    device/src/Device.cpp \
    session/src/Session.cpp \
    context_manager/src/ContextManager.cpp \
    resource_manager/src/ResourceManager.cpp \
    resource_manager/src/SndCardMonitor.cpp \
    utils/src/SoundTriggerPlatformInfo.cpp \
    utils/src/ACDPlatformInfo.cpp \
    utils/src/ASRPlatformInfo.cpp \
    utils/src/VoiceUIPlatformInfo.cpp \
    utils/src/SignalHandler.cpp \
    utils/src/AudioHapticsInterface.cpp \
    utils/src/MetadataParser.cpp \
    utils/src/MemLogBuilder.cpp \
    utils/src/PerfLock.cpp

LOCAL_HEADER_LIBRARIES := \
    libarpal_headers \
    libspf-headers \
    libcapiv2_headers \
    libagm_headers \
    libacdb_headers \
    liblisten_headers \
    libarosal_headers \
    libvui_dmgr_headers \
    libarvui_intf_headers \
    libarmemlog_headers \
    libarpal_internalheaders

ifeq ($(AUDIO_FEATURE_STATS_ENABLED), true)
LOCAL_HEADER_LIBRARIES += libaudiofeaturestats_headers
endif

LOCAL_SHARED_LIBRARIES := \
    libar-gsl\
    liblog\
    libexpat\
    liblx-osal\
    libaudioroute\
    libcutils \
    libutilscallstack \
    libagmclient \
    libarmemlog \
    libhidlbase

LOCAL_STATIC_LIBRARIES := libplugin_manager

#used for static compilation
ifeq ($(USE_PAL_STATIC_LINKING_MODULES),true)

    LOCAL_STATIC_LIBRARIES += \
        libstream_acd \
        libstream_dummy \
        libstream_common \
        libstream_commonproxy \
        libstream_calltranslation \
        libstream_compress \
        libstream_contextproxy \
        libstream_haptics \
        libstream_incall \
        libstream_nontunnel \
        libstream_pcm \
        libstream_sensorpcmdata \
        libstream_sensorrenderer \
        libstream_soundtrigger \
        libstream_ultrasound \
        libstream_asr \
        libsession_ar \
        libsession_compress \
        libsession_agm \
        libsession_pcm \
        libsession_voice \
        libdev_handset \
        libdev_handset_mic \
        libdev_handset_va \
        libdev_speaker \
        libdev_speaker_mic \
        libdev_headphone \
        libdev_headset_mic \
        libdev_headset_va \
        libdev_bt \
        libdev_fm \
        libdev_usb \
        libdev_ultrasound \
        libdev_proxy \
        libdev_display \
        libdev_dummy \
        libdev_haptics \
        libdev_ext_ec \
        libdev_ec_ref \
        libpal_sounddose \
        libsession_pcm_config \
        libsession_compress_config \
        libsession_voice_config \
        libsession_config_utils
endif #end of static compilation

ifeq ($(call is-board-platform-in-list,kalama pineapple sun canoe), true)
LOCAL_SHARED_LIBRARIES += libPeripheralStateUtils
LOCAL_HEADER_LIBRARIES += peripheralstate_headers \
    vendor_common_inc\
    mink_headers
endif

# Use flag based selection to use QTI vs open source tinycompress project

ifeq ($(TARGET_USES_QTI_TINYCOMPRESS),true)
LOCAL_SHARED_LIBRARIES += libqti-tinyalsa libqti-tinycompress
else
LOCAL_C_INCLUDES       += $(TOP)/external/tinycompress/include
LOCAL_SHARED_LIBRARIES += libtinyalsa libtinycompress
endif

ifeq ($(TARGET_DISABLE_PAL_ST),true)
LOCAL_CFLAGS        += -DSOUND_TRIGGER_FEATURES_DISABLED
else
LOCAL_SRC_FILES     += utils/src/STUtils.cpp
endif

ifeq ($(TARGET_DISABLE_PAL_BT),true)
LOCAL_CFLAGS        += -DBLUETOOTH_FEATURES_DISABLED
else
LOCAL_SRC_FILES     += utils/src/BTUtils.cpp
endif

include $(BUILD_SHARED_LIBRARY)

#-------------------------------------------
#            Build CHARGER_LISTENER LIB
#-------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := libaudiochargerlistener
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= utils/src/ChargerListener.cpp

LOCAL_CFLAGS += -Wall -Werror -Wno-unused-function -Wno-unused-variable

LOCAL_SHARED_LIBRARIES += libcutils liblog

LOCAL_C_INCLUDES := $(LOCAL_PATH)/utils/inc

include $(BUILD_SHARED_LIBRARY)

#-------------------------------------------
#            Build libpal_sounddose
#-------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := libpal_sounddose
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true

# add for gcov dump
ifeq ($(AUDIO_FEATURE_ENABLED_GCOV), true)
LOCAL_CFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
LOCAL_CPPFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
LOCAL_LDFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
endif

LOCAL_SRC_FILES:= sounddose/src/SoundDoseUtility.cpp

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/sounddose/inc

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/sounddose/inc \
    $(TOP)/system/media/audio_route/include \
    $(TOP)/system/media/audio/include

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog \
    libaudioroute \
    libsession_ar \
    libsession_agm \
    libexpat \
    libar-pal

LOCAL_HEADER_LIBRARIES := \
    libarpal_headers \
    libspf-headers \
    libagm_headers \
    libacdb_headers \
    liblisten_headers \
    libarosal_headers \
    libarvui_intf_headers \
    libarmemlog_headers \
    libarpal_internalheaders \
    libagm_headers \
    libsession_agm_headers \
    libsession_ar_headers

ifeq ($(AUDIO_FEATURE_STATS_ENABLED), true)
LOCAL_HEADER_LIBRARIES += libaudiofeaturestats_headers
endif

# Use flag based selection to use QTI vs open source tinycompress project
ifeq ($(TARGET_USES_QTI_TINYCOMPRESS),true)
LOCAL_SHARED_LIBRARIES += libqti-tinyalsa
else
LOCAL_SHARED_LIBRARIES += libtinyalsa
endif

ifeq ($(USE_PAL_STATIC_LINKING_MODULES),true)
    include $(BUILD_STATIC_LIBRARY)
else
    include $(BUILD_SHARED_LIBRARY)
endif

include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wno-tautological-compare
LOCAL_CFLAGS += -Wno-macro-redefined
LOCAL_CFLAGS += -D_ANDROID_

LOCAL_SRC_FILES  := test/PalUsecaseTest.c \
                    test/PalTest_main.c

LOCAL_MODULE               := PalTest
LOCAL_MODULE_OWNER         := qti
LOCAL_MODULE_TAGS          := optional

LOCAL_HEADER_LIBRARIES := \
    libarpal_headers

LOCAL_SHARED_LIBRARIES := \
                          libpalclient \
                          libbinder_ndk
LOCAL_VENDOR_MODULE := true

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

include $(PAL_BASE_PATH)/plugins/Android.mk
include $(PAL_BASE_PATH)/ipc/aidl/Android.mk
include $(PAL_BASE_PATH)/stream/Android.mk
include $(PAL_BASE_PATH)/session/Android.mk
include $(PAL_BASE_PATH)/device/Android.mk

endif #AUDIO_USE_STUB_HAL