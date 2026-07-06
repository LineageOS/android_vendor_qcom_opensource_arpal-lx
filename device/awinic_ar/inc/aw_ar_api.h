/*Copyright (c) 2024 AWINIC Technology CO., LTD*/

#ifndef AW_AR_API_H_
#define AW_AR_API_H_

#if defined(__cplusplus)||defined(c_plusplus)
extern "C"{
#endif

#include <tinyalsa/asoundlib.h>


#define AW_AR_VERSION       ("v0.0.0.9")


struct aw_dev_info {
    struct mixer *virt_mixer;
    struct mixer *hw_mixer;
};

int aw_audioreach_monitor_init(struct aw_dev_info *dev_info);
void aw_audioreach_monitor_deinit();
void aw_audioreach_monitor_start();
void aw_audioreach_monitor_stop();
int getCoilTemperatures(float values[2]);
int setLimitV(int speaker, float limitV);
int setLimitX(int speaker, float limitX);
int setEQParams(int speaker, int slot, unsigned char *data);



int aw_audioreach_dsp_read_r0(struct aw_dev_info *dev_info, int *r0, int dev_num);
int aw_audioreach_dsp_read_f0(struct aw_dev_info *dev_info, int *f0, int dev_num);
int aw_audioreach_dsp_read_f0_q(struct aw_dev_info *dev_info, int *f0, int *q, int dev_num);
int aw_audioreach_dsp_set_re(struct aw_dev_info *dev_info, int *cali_re, int dev_num);
int aw_audioreach_dsp_set_offset_and_auth(struct aw_dev_info *dev_info, int dev_num);
int aw_audioreach_dsp_set_offset(struct aw_dev_info *dev_info, int dev_num);


int aw_audioreach_dsp_read_st(struct aw_dev_info *dev_info,
                        int *r0, int *te, int dev_num);
int aw_audioreach_dsp_get_re_range(int *re_min, int *re_max, int dev_num);
int aw_audioreach_dsp_set_algo_en(struct aw_dev_info *dev_info, unsigned int is_enable);


int aw_audioreach_set_re_to_file(int *cali_re, int dev_num);
int aw_audioreach_get_re_from_file(int *cali_re, int dev_num);
int aw_audioreach_dsp_algo_auth(struct aw_dev_info *dev_info);

#if defined(__cplusplus)||defined(c__plusplus)
}
#endif

#endif
