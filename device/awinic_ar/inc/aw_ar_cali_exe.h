/*Copyright (c) 2022 AWINIC Technology CO., LTD*/

#ifndef AW_AR_CALI_EXE_H_
#define AW_AR_CALI_EXE_H_

#if defined(__cplusplus)||defined(c_plusplus)
extern "C"{
#endif

#include "aw_ar_dev.h"

enum aw_ar_exe_cmd_type {
    AW_FAST_START_CALI,
    AW_GET_SPKR_ST,
    AW_SET_RE,
    AW_RE_CALI,
    AW_F0_CALI,
    AW_F0_Q_CALI,
    AW_CALI_ALL,
    AW_GET_RE_RANGE,
    AW_SET_OFFSET,
    AW_CMD_MAX,
};


struct aw_dev {
    const char *name;
    uint32_t dev_index;
};


struct aw882xx {
    int dev_num;
    /* This value is the same as dev_num when calibrated together*/
    int num_or_index;
    uint32_t cmd;
    int set_re[AW_DEV_CH_MAX];
    struct aw_dev *aw_dev;
    struct aw_dev_info dev_info;
};

#if defined(__cplusplus)||defined(c__plusplus)
}
#endif

#endif
