LOCAL_PATH := $(call my-dir)

#-------------------------------------------
#            Build SESSION_AR LIB
#-------------------------------------------

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE        := libsession_ar
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
    src/SessionAR.cpp \
    src/SessionAlsaUtils.cpp \
    src/PayloadBuilder.cpp

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
    libagm_headers

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    liblx-osal \
    libar-pal \
    libexpat \
    libagmclient \
    libaudioroute \
    libar-gsl

 ifeq ($(TARGET_USES_QTI_TINYCOMPRESS),true)
 LOCAL_SHARED_LIBRARIES += libqti-tinyalsa libqti-tinycompress
 else
 LOCAL_C_INCLUDES       += $(TOP)/external/tinycompress/include
 LOCAL_SHARED_LIBRARIES += libtinyalsa libtinycompress
 endif

 ifeq ($(call is-board-platform-in-list,kalama pineapple sun), true)
LOCAL_SHARED_LIBRARIES += libPeripheralStateUtils
LOCAL_HEADER_LIBRARIES += peripheralstate_headers \
    vendor_common_inc\
    mink_headers
endif

ifeq ($(USE_PAL_STATIC_LINKING_MODULES),true)
    include $(BUILD_STATIC_LIBRARY)
else
    include $(BUILD_SHARED_LIBRARY)
endif
