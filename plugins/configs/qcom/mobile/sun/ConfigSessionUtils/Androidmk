LOCAL_PATH := $(call my-dir)

# Define the header library module
include $(CLEAR_VARS)

LOCAL_MODULE := libsession_config_utils_headers
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/inc
LOCAL_MODULE_OWNER := qti
LOCAL_VENDOR_MODULE := true

include $(BUILD_HEADER_LIBRARY)

# Define the existing static library module

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libsession_config_utils
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
    src/ConfigSessionUtils.cpp

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
    libsession_pcm_headers \
    libsession_config_utils_headers

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    liblx-osal \
    libexpat \
    libar-pal \
    libsession_pcm

ifeq ($(TARGET_USES_QTI_TINYCOMPRESS),true)
LOCAL_SHARED_LIBRARIES += libqti-tinyalsa
else
LOCAL_SHARED_LIBRARIES += libtinyalsa
endif

include $(BUILD_STATIC_LIBRARY)