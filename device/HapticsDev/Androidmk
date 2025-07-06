LOCAL_PATH := $(call my-dir)

#-------------------------------------------
#            Build DEVICE_HAPTICS_DEV LIB
#-------------------------------------------

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE        := libdev_haptics
LOCAL_MODULE_OWNER  := qti
LOCAL_VENDOR_MODULE := true

LOCAL_CPPFLAGS += -fexceptions -frtti

# add for gcov dump
ifeq ($(AUDIO_FEATURE_ENABLED_GCOV), true)
LOCAL_CFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
LOCAL_CPPFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
LOCAL_LDFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
endif

LOCAL_SRC_FILES := \
    src/HapticsDev.cpp \
    src/HapticsDevProtection.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/inc

LOCAL_C_INCLUDES += $(TOP)/system/media/audio_route/include
LOCAL_C_INCLUDES += $(TOP)/system/media/audio/include

LOCAL_HEADER_LIBRARIES := \
    libarpal_headers \
    libspf-headers \
    libagm_headers \
    libacdb_headers \
    liblisten_headers \
    libarosal_headers \
    libaudiofeaturestats_headers \
    libarvui_intf_headers \
    libarmemlog_headers \
    libarpal_internalheaders \
    libdev_speaker_headers \
    libsession_ar_headers

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    liblx-osal \
    libar-pal \
    libexpat \
    libaudioroute \
    libutilscallstack \
    libsession_ar

 ifeq ($(TARGET_USES_QTI_TINYCOMPRESS),true)
 LOCAL_SHARED_LIBRARIES += libqti-tinyalsa libqti-tinycompress
 else
 LOCAL_C_INCLUDES       += $(TOP)/external/tinycompress/include
 LOCAL_SHARED_LIBRARIES += libtinyalsa libtinycompress
 endif

ifeq ($(USE_PAL_STATIC_LINKING_MODULES),true)
    include $(BUILD_STATIC_LIBRARY)
else
    include $(BUILD_SHARED_LIBRARY)
endif
