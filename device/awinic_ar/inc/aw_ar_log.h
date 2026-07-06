/*Copyright (c) 2024 AWINIC Technology CO., LTD*/


#ifndef __AW_AR_LOG_H__
#define __AW_AR_LOG_H__

#define AW_OK (0)
#define AW_FAIL (-1)


#ifdef AW_BACK_END_NAME
#include <cutils/log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>

#define AWPRINTFE(format, ...) printf("[Awinic] [ERR] %s: " format "\n", __func__, ##__VA_ARGS__)
#define AWPRINTFI(format, ...) printf("[Awinic] [INF] %s: " format "\n", __func__, ##__VA_ARGS__)
#define AWPRINTFD(format, ...) printf("[Awinic] [DBG] %s: " format "\n", __func__, ##__VA_ARGS__)
#define AWLOGE(format, ...) ALOGE("[Awinic] [ERR] %s: " format "\n", __func__, ##__VA_ARGS__)
#define AWLOGI(format, ...) ALOGI("[Awinic] [INF] %s: " format "\n", __func__, ##__VA_ARGS__)
#define AWLOGD(format, ...) ALOGD("[Awinic] [DGB] %s: " format "\n", __func__, ##__VA_ARGS__)

#else

#include "PalCommon.h"
#define AWLOGE(format, ...)                                         \
    if (pal_log_lvl & PAL_LOG_ERR) {                                \
        ALOGE("[Awinic] [ERR] %s: " format "\n", __func__, ##__VA_ARGS__);     \
    }

#define AWLOGI(format, ...)                                         \
    if(pal_log_lvl & PAL_LOG_INFO) {                                \
        ALOGI("[Awinic] [INF] %s: " format "\n", __func__, ##__VA_ARGS__);     \
    }

#define AWLOGD(format, ...)                                           \
    if (pal_log_lvl & PAL_LOG_DBG) {                                  \
        ALOGD("[Awinic] [DGB] %s: " format "\n", __func__, ##__VA_ARGS__);       \
    }

#endif


#endif
