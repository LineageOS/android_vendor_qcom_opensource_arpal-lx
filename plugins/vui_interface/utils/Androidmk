LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libvui_utils
LOCAL_MODULE_OWNER := qti
LOCAL_VENDOR_MODULE := true

LOCAL_CPPFLAGS += -fexceptions -frtti

LOCAL_SRC_FILES := \
    src/VUIInterfaceUtils.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/inc

LOCAL_HEADER_LIBRARIES := \
    liblisten_headers

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    liblx-osal

include $(BUILD_SHARED_LIBRARY)
