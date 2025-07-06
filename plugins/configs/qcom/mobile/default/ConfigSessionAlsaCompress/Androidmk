LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libsession_compress_config
LOCAL_MODULE_OWNER := qti
LOCAL_VENDOR_MODULE := true

LOCAL_CPPFLAGS += -fexceptions  -frtti

# add for gcov dump
ifeq ($(AUDIO_FEATURE_ENABLED_GCOV), true)
LOCAL_CFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
LOCAL_CPPFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
LOCAL_LDFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
endif

LOCAL_SRC_FILES := \
    src/ConfigSessionAlsaCompress.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/inc

LOCAL_C_INCLUDES += $(TOP)/system/media/audio_route/include

LOCAL_HEADER_LIBRARIES := \
    libarpal_headers \
    libarpal_internalheaders \
    libarvui_intf_headers \
    liblisten_headers \
    plugin_manager_headers \
    libaudiofeaturestats_headers \
    libspf-headers \
    libacdb_headers \
    libagm_headers \
    libsession_ar_headers \
    libsession_compress_headers \
    libsession_config_utils_headers

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    liblx-osal \
    libexpat \
    libar-pal \
    libsession_ar \
    libsession_compress

LOCAL_STATIC_LIBRARIES := \
    libsession_config_utils

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