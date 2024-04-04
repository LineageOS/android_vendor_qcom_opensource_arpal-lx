LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libvui_intf
LOCAL_MODULE_OWNER := qti
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES := \
   src/VoiceUIInterface.cpp \
   src/SVAInterface.cpp \
   src/SoundTriggerUtils.cpp \
   src/SVAExtension.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/inc

LOCAL_HEADER_LIBRARIES := \
    libarpal_headers \
    libarvui_intf_headers \
    libspf-headers \
    liblisten_headers \
    libarosal_headers

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    liblx-osal

include $(BUILD_SHARED_LIBRARY)
