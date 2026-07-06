/*Copyright (c) 2024 AWINIC Technology CO., LTD*/

#ifndef AW_AR_CALI_H_
#define AW_AR_CALI_H_


enum {
    AW_1000_US = 1000,
    AW_2000_US = 2000,
    AW_3000_US = 3000,
    AW_4000_US = 4000,
    AW_5000_US = 5000,
    AW_10000_US = 10000,
    AW_32000_US = 32000,
    AW_70000_US = 70000,
    AW_100000_US = 100000,
};

enum {
    CALI_OPS_HMUTE = 0X0001,
    CALI_OPS_NOISE = 0X0002,
};

enum {
    CALI_TYPE_RE = 0,
    CALI_TYPE_F0,
};

enum {
    MSG_CALI_DISABLE_DATA = 0,
    MSG_CALI_RE_ENABLE_DATA,
    MSG_CALI_F0_ENABLE_DATA,
};


void aw_audioreach_set_noise_en(bool is_noise_en);
int aw_audioreach_set_cali_re_time(unsigned int cali_time_ms);
void aw_audioreach_set_single_cali_en(bool is_signle_cali);




#endif
