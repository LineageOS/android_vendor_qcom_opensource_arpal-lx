/*Copyright (c) 2024 AWINIC Technology CO., LTD*/


#ifndef __AW_AR_DEV_H__
#define __AW_AR_DEV_H__

extern const char *aw_back_end_name;
extern int back_end_name_flag;

enum {
    AW_DEV_CH_PRI_L = 0,
    AW_DEV_CH_PRI_R = 1,
    AW_DEV_CH_SEC_L = 2,
    AW_DEV_CH_SEC_R = 3,
    AW_DEV_CH_TERT_L = 4,
    AW_DEV_CH_TERT_R = 5,
    AW_DEV_CH_QUAT_L = 6,
    AW_DEV_CH_QUAT_R = 7,
    AW_DEV_CH_MAX,
};


#endif
