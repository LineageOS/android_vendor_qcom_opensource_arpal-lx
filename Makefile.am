pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pal.pc
EXTRA_DIST = $(pkgconfig_DATA)


h_sources = ${top_srcdir}/stream/inc/Stream.h \
            ${top_srcdir}/stream/inc/StreamCompress.h \
            ${top_srcdir}/stream/inc/StreamPCM.h \
            ${top_srcdir}/stream/inc/StreamACDB.h \
            ${top_srcdir}/stream/inc/StreamInCall.h \
            ${top_srcdir}/stream/inc/StreamSoundTrigger.h \
            ${top_srcdir}/stream/inc/StreamACD.h \
            ${top_srcdir}/stream/inc/StreamCommon.h \
            ${top_srcdir}/stream/inc/StreamContextProxy.h \
            ${top_srcdir}/stream/inc/StreamCommonProxy.h \
            ${top_srcdir}/stream/inc/StreamUltraSound.h \
            ${top_srcdir}/stream/inc/StreamSensorPCMData.h\
            ${top_srcdir}/stream/inc/StreamHaptics.h \
            ${top_srcdir}/device/inc/Headphone.h \
            ${top_srcdir}/device/inc/USBAudio.h \
            ${top_srcdir}/device/inc/Device.h \
            ${top_srcdir}/device/inc/Speaker.h \
            ${top_srcdir}/device/inc/Bluetooth.h \
            ${top_srcdir}/device/inc/SpeakerMic.h \
            ${top_srcdir}/device/inc/HeadsetMic.h \
            ${top_srcdir}/device/inc/HandsetMic.h \
            ${top_srcdir}/device/inc/HdmiIn.h \
            ${top_srcdir}/device/inc/Handset.h \
            ${top_srcdir}/device/inc/HandsetVaMic.h \
            ${top_srcdir}/device/inc/DisplayPort.h \
            ${top_srcdir}/device/inc/HeadsetVaMic.h \
            ${top_srcdir}/device/inc/RTProxy.h \
            ${top_srcdir}/device/inc/SpeakerProtection.h \
            ${top_srcdir}/device/inc/FMDevice.h \
            ${top_srcdir}/device/inc/ExtEC.h \
            ${top_srcdir}/device/inc/HapticsDev.h \
            ${top_srcdir}/device/inc/UltrasoundDevice.h \
            ${top_srcdir}/device/inc/ECRefDevice.h \
            ${top_srcdir}/device/inc/DummyDev.h \
            ${top_srcdir}/device/inc/HapticsDevProtection.h \
            ${top_srcdir}/session/inc/Session.h \
            ${top_srcdir}/session/inc/PayloadBuilder.h \
            ${top_srcdir}/session/inc/SessionAlsaPcm.h \
            ${top_srcdir}/session/inc/SessionAgm.h \
            ${top_srcdir}/session/inc/SessionAlsaUtils.h \
            ${top_srcdir}/session/inc/SessionAlsaCompress.h \
            ${top_srcdir}/session/inc/SessionAlsaVoice.h \
            ${top_srcdir}/session/inc/SoundTriggerEngine.h \
            ${top_srcdir}/session/inc/SoundTriggerEngineCapi.h \
            ${top_srcdir}/session/inc/SoundTriggerEngineGsl.h \
            ${top_srcdir}/session/inc/ContextDetectionEngine.h \
            ${top_srcdir}/context_manager/inc/ContextManager.h \
            ${top_srcdir}/session/inc/ACDEngine.h \
            ${top_srcdir}/resource_manager/inc/ResourceManager.h \
            ${top_srcdir}/resource_manager/inc/SndCardMonitor.h \
            ${top_srcdir}/utils/inc/SoundTriggerPlatformInfo.h \
            ${top_srcdir}/utils/inc/ACDPlatformInfo.h \
            ${top_srcdir}/utils/inc/VoiceUIPlatformInfo.h \
            ${top_srcdir}/utils/inc/PalRingBuffer.h \
            ${top_srcdir}/utils/inc/SignalHandler.h \
            ${top_srcdir}/utils/inc/AudioHapticsInterface.h \
            ${top_srcdir}/utils/inc/MetadataParser.h

AM_CPPFLAGS := -I $(top_srcdir)/stream/inc
AM_CPPFLAGS += -I $(top_srcdir)/device/inc
AM_CPPFLAGS += -I $(top_srcdir)/session/inc
AM_CPPFLAGS += -I $(top_srcdir)/resource_manager/inc
AM_CPPFLAGS += -I $(top_srcdir)/utils/inc
AM_CPPFLAGS += -I $(top_srcdir)/plugins/codecs
AM_CPPFLAGS += -I $(top_srcdir)/inc
AM_CPPFLAGS += -I $(top_srcdir)/context_manager/inc
AM_CPPFLAGS += -I $(top_srcdir)/
AM_CPPFLAGS += @AGM_CFLAGS@
AM_CPPFLAGS += @SPFHEADERS_CFLAGS@
AM_CPPFLAGS += @AROUTE_CFLAGS@
AM_CPPFLAGS += @MMHEADERS_CFLAGS@
AM_CPPFLAGS += @PALHEADERS_CFLAGS@
AM_CPPFLAGS += -Wno-sign-compare -Wno-unused-result -Wl,-z,defs
AM_CPPFLAGS += -D__unused=__attribute__\(\(__unused__\)\) -DLINUX_ENABLED -DPAL_CUTILS_UNSUPPORTED -DPAL_MEMLOG_UNSUPPORTED -DATRACE_UNSUPPORTED -DCARD_STATE_UNSUPPORTED -DPAL_SIGNAL_HANDLER_UNSUPPORTED -DA2DP_SINK_SUPPORTED
AM_CPPFLAGS += -DSND_AUDIOCODEC_ALAC=0x00000020 -DSND_AUDIOCODEC_APE=0x00000021
AM_CPPFLAGS += -DCONFIG_GSL
AM_CPPFLAGS += -DPAL_SP_TEMP_PATH=\"/etc/acdbdata/audio.cal\"
AM_CPPFLAGS += -DWSA_V883X_ADDR

pal_sources = ${top_srcdir}/Pal.cpp \
              ${top_srcdir}/stream/src/Stream.cpp \
              ${top_srcdir}/stream/src/StreamCompress.cpp \
              ${top_srcdir}/stream/src/StreamPCM.cpp \
              ${top_srcdir}/stream/src/StreamACDB.cpp \
              ${top_srcdir}/stream/src/StreamInCall.cpp \
              ${top_srcdir}/stream/src/StreamNonTunnel.cpp \
              ${top_srcdir}/stream/src/StreamACD.cpp \
              ${top_srcdir}/stream/src/StreamSoundTrigger.cpp \
              ${top_srcdir}/stream/src/StreamCommon.cpp \
              ${top_srcdir}/stream/src/StreamContextProxy.cpp \
              ${top_srcdir}/stream/src/StreamCommonProxy.cpp \
              ${top_srcdir}/stream/src/StreamUltraSound.cpp \
              ${top_srcdir}/stream/src/StreamSensorPCMData.cpp\
              ${top_srcdir}/stream/src/StreamHaptics.cpp \
              ${top_srcdir}/device/src/Headphone.cpp \
              ${top_srcdir}/device/src/USBAudio.cpp \
              ${top_srcdir}/device/src/Device.cpp \
              ${top_srcdir}/device/src/Speaker.cpp \
              ${top_srcdir}/device/src/Bluetooth.cpp \
              ${top_srcdir}/device/src/SpeakerMic.cpp \
              ${top_srcdir}/device/src/HeadsetMic.cpp \
              ${top_srcdir}/device/src/HandsetMic.cpp \
              ${top_srcdir}/device/src/HdmiIn.cpp \
              ${top_srcdir}/device/src/Handset.cpp \
              ${top_srcdir}/device/src/HandsetVaMic.cpp \
              ${top_srcdir}/device/src/DisplayPort.cpp \
              ${top_srcdir}/device/src/HeadsetVaMic.cpp \
              ${top_srcdir}/device/src/RTProxy.cpp \
              ${top_srcdir}/device/src/SpeakerProtection.cpp \
              ${top_srcdir}/device/src/FMDevice.cpp \
              ${top_srcdir}/device/src/ExtEC.cpp \
              ${top_srcdir}/device/src/HapticsDev.cpp \
              ${top_srcdir}/device/src/UltrasoundDevice.cpp \
              ${top_srcdir}/device/src/ECRefDevice.cpp \
              ${top_srcdir}/device/src/DummyDev.cpp \
              ${top_srcdir}/device/src/HapticsDevProtection.cpp \
              ${top_srcdir}/session/src/Session.cpp \
              ${top_srcdir}/session/src/PayloadBuilder.cpp \
              ${top_srcdir}/session/src/SessionAlsaPcm.cpp \
              ${top_srcdir}/session/src/SessionAgm.cpp \
              ${top_srcdir}/session/src/SessionAlsaUtils.cpp \
              ${top_srcdir}/session/src/SessionAlsaCompress.cpp \
              ${top_srcdir}/session/src/SessionAlsaVoice.cpp \
              ${top_srcdir}/session/src/SoundTriggerEngine.cpp \
              ${top_srcdir}/session/src/SoundTriggerEngineCapi.cpp \
              ${top_srcdir}/session/src/SoundTriggerEngineGsl.cpp \
              ${top_srcdir}/session/src/ContextDetectionEngine.cpp \
              ${top_srcdir}/context_manager/src/ContextManager.cpp \
              ${top_srcdir}/session/src/ACDEngine.cpp \
              ${top_srcdir}/resource_manager/src/ResourceManager.cpp \
              ${top_srcdir}/resource_manager/src/SndCardMonitor.cpp \
              ${top_srcdir}/utils/src/SoundTriggerPlatformInfo.cpp \
              ${top_srcdir}/utils/src/ACDPlatformInfo.cpp \
              ${top_srcdir}/utils/src/VoiceUIPlatformInfo.cpp \
              ${top_srcdir}/utils/src/PalRingBuffer.cpp \
              ${top_srcdir}/utils/src/AudioHapticsInterface.cpp \
              ${top_srcdir}/utils/src/MetadataParser.cpp

btbundle_plugin_sources = ${top_srcdir}/plugins/codecs/bt_base.c \
                          ${top_srcdir}/plugins/codecs/bt_bundle.c

library_include_HEADERS = $(h_sources)
library_includedir = $(includedir)/pal

lib_LTLIBRARIES     = libpal.la
libpal_la_SOURCES   = $(pal_sources)
libpal_la_LIBADD    = @GLIB_LIBS@ -ltinyalsa -lar_osal -laudioroute -lspfheaders -lexpat -ltinycompress -lvuiinterface
libpal_la_CPPFLAGS := $(AM_CPPFLAGS)
libpal_la_CPPFLAGS += -std=c++14
libpal_la_LDFLAGS   = -shared -avoid-version
libpal_la_CPPFLAGS += @GLIB_CFLAGS@ -Dstrlcpy=g_strlcpy -Dstrlcat=g_strlcat -include glib.h
libpal_la_CPPFLAGS += -DACD_SM_FILEPATH=\"/etc/models/acd/\"
libpal_la_list      = ./configs/qcm6490/mixer_paths_qcm6490_idp.xml \
                      ./configs/qcm6490/resourcemanager_qcm6490_idp.xml \
                      ./configs/qcm6490/mixer_paths_qcm6490_rb3.xml \
                      ./configs/qcm6490/resourcemanager_qcm6490_rb3.xml \
                      ./configs/qcm6490/mixer_paths_qcm6490_rb3_vision.xml \
                      ./configs/qcm6490/resourcemanager_qcm6490_rb3_vision.xml \
                      ./configs/qcm6490/mixer_paths_qcm6490_rb3_vc.xml \
                      ./configs/qcm6490/resourcemanager_qcm6490_rb3_vc.xml \
                      ./configs/qcm6490/usecaseKvManager.xml

lib_LTLIBRARIES     += lib_bt_bundle.la
lib_bt_bundle_la_SOURCES   = $(btbundle_plugin_sources)
lib_bt_bundle_la_LIBADD    = @GLIB_LIBS@ @SPFHEADERS_LIBS@
lib_bt_bundle_la_CFLAGS    = $(AM_CPPFLAGS)
lib_bt_bundle_la_LDFLAGS   = -shared -avoid-version

if COMPILE_COMPRESS
libpal_la_CPPFLAGS += -DSND_COMPRESS_DEC_HDR
endif

if USE_SYSLOG
libpal_la_CPPFLAGS += -DPAL_USE_SYSLOG
lib_bt_bundle_la_CFLAGS += -DPAL_USE_SYSLOG
endif

# install essential xml files under /etc
root_etcdir      = "/etc"
root_etc_SCRIPTS = $(libpal_la_list)
install-data-hook:
	chmod  go+r $(DESTDIR)$(root_etcdir)/mixer_paths_qcm6490_idp.xml
	chmod  go+r $(DESTDIR)$(root_etcdir)/resourcemanager_qcm6490_idp.xml
	chmod  go+r $(DESTDIR)$(root_etcdir)/mixer_paths_qcm6490_rb3.xml
	chmod  go+r $(DESTDIR)$(root_etcdir)/resourcemanager_qcm6490_rb3.xml
	chmod  go+r $(DESTDIR)$(root_etcdir)/mixer_paths_qcm6490_rb3_vision.xml
	chmod  go+r $(DESTDIR)$(root_etcdir)/resourcemanager_qcm6490_rb3_vision.xml
	chmod  go+r $(DESTDIR)$(root_etcdir)/mixer_paths_qcm6490_rb3_vc.xml
	chmod  go+r $(DESTDIR)$(root_etcdir)/resourcemanager_qcm6490_rb3_vc.xml
	chmod  go+r $(DESTDIR)$(root_etcdir)/usecaseKvManager.xml

adsprpc_sources = adsprpcd/adsprpcd.c

bin_PROGRAMS = audioadsprpcd
audioadsprpcd_SOURCES = $(adsprpc_sources)
audioadsprpcd_LDADD = -ldl
audioadsprpcd_la_CFLAGS = -fPIC
