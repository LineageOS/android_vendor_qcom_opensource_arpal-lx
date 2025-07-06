LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := customva_plugin
LOCAL_MODULE_OWNER := qti
LOCAL_VENDOR_MODULE := true

LOCAL_CPPFLAGS += -fexceptions -frtti

LOCAL_SRC_FILES := \
    src/CustomVA.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/inc \
    $(LOCAL_PATH)/../utils/inc

LOCAL_HEADER_LIBRARIES := \
    libarpal_headers \
    libarvui_intf_headers \
    libspf-headers \
    liblisten_headers \
    libarosal_headers

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    liblx-osal \
    libvui_utils

include $(BUILD_SHARED_LIBRARY)
