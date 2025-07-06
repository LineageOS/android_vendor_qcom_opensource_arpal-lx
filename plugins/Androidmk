PLUGINS_BASE_PATH := $(call my-dir)
include $(PLUGINS_BASE_PATH)/codecs/Android.mk
include $(PLUGINS_BASE_PATH)/vui_interface/Android.mk
include $(PLUGINS_BASE_PATH)/PluginManager/Android.mk

# Config Plugin; which BA ? use board-specific or default ?
FILE_TO_CHECK := $(PLUGINS_BASE_PATH)/configs/qcom/$(BA_NAME)/$(TARGET_BOARD_PLATFORM)/Android.mk
DEFAULT_CONFIG_PLUGIN := $(PLUGINS_BASE_PATH)/configs/qcom/$(BA_NAME)/default/Android.mk

ifneq ($(wildcard $(FILE_TO_CHECK)),)
# FILE exists, include BA and board specific config plugins.
$(info BA($(BA_NAME)) and board specific($(TARGET_BOARD_PLATFORM)) makefile exists for PAL config plugin.)
include $(FILE_TO_CHECK)
else
# FILE does not exists, print msg, include BA default.
$(warning $(FILE_TO_CHECK) does NOT exist. Using BA level default PAL config plugin.)
include $(DEFAULT_CONFIG_PLUGIN)
endif