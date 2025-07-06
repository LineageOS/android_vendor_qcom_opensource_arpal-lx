LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE        := libpaleventnotifier
LOCAL_MODULE_OWNER  := qti
LOCAL_VENDOR_MODULE := true

LOCAL_C_INCLUDES    := $(PAL_BASE_PATH)/inc \
                       $(PAL_BASE_PATH)/utils/inc

LOCAL_CLANG             := true
LOCAL_TIDY              := true
LOCAL_CFLAGS            += -v -Wall -Wthread-safety

LOCAL_VINTF_FRAGMENTS := Manifest_IPALEventNotifier.xml

LOCAL_SRC_FILES     :=  \
    Service.cpp \
    PalServerNotify.cpp

LOCAL_STATIC_LIBRARIES := \
    libpalaidltypeconverter \
    libaidlcommonsupport

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libbinder_ndk \
    libbase \
    libcutils \
    libutils \
    libar-pal \
    vendor.qti.hardware.pal-V1-ndk \
    vendor.qti.hardware.paleventnotifier-V3-ndk

LOCAL_HEADER_LIBRARIES := \
    libspf-headers

include $(BUILD_SHARED_LIBRARY)
