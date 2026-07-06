/*Copyright (c) 2024 AWINIC Technology CO., LTD*/

#ifndef AW_AR_MONITOR_H_
#define AW_AR_MONITOR_H_


#include "aw_ar_dev.h"
#include <pthread.h>


enum aw_monitor_stat {
    AW_MONITER_INIT = 0,        /*wait init*/
    AW_MONITER_CREATE,          /*finsh init*/
    AW_MONITER_DESTROY,         /*deinit*/
    AW_MONITER_RUN,
    AW_MONITER_STOP,
    AW_MONITER_WAIT,
};

struct aw_ar_monitor {
    pthread_t thread_id;

    struct mixer *hw_mixer;
    struct mixer *virt_mixer;

    int dev_num;
    int state;

    unsigned int chip_type;

    unsigned int time_ms;
    unsigned int pre_vmax[AW_DEV_CH_MAX];
};


#endif
