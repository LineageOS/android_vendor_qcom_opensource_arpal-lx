LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libstream_haptics
LOCAL_MODULE_OWNER := qti
LOCAL_VENDOR_MODULE := true

LOCAL_CPPFLAGS += -fexceptions -frtti

# add for gcov dump
ifeq ($(AUDIO_FEATURE_ENABLED_GCOV), true)
LOCAL_CFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
LOCAL_CPPFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
LOCAL_LDFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
endif

LOCAL_SRC_FILES := \
    src/StreamHaptics.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/inc

LOCAL_C_INCLUDES += $(TOP)/system/media/audio_route/include

LOCAL_HEADER_LIBRARIES := \
    libarpal_headers \
    libstream_pcm_headers \
    libspf-headers \
    libvui_dmgr_headers \
    libarvui_intf_headers \
    libaudiofeaturestats_headers \
    liblisten_headers \
    libacdb_headers \
    libaudioroute \
    libarpal_internalheaders \
    libarmemlog_headers

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    liblx-osal \
    libar-pal \
    libexpat \
    libstream_pcm

ifeq ($(USE_PAL_STATIC_LINKING_MODULES),true)
    include $(BUILD_STATIC_LIBRARY)
else
    include $(BUILD_SHARED_LIBRARY)
endif
