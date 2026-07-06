/*Copyright (c) 2024 AWINIC Technology CO., LTD*/

#ifndef AW_AR_KMSG_H_
#define AW_AR_KMSG_H_



#define AW_CTL_NAME_BUF_MAX (50)

enum {
    CALI_STR_NONE = 0,
    CALI_STR_DEV_SEL,         //switch device
    CALI_STR_VER,
    CALI_STR_DEV_NUM,
    CALI_STR_SHOW_RE_RANGE,
    CALI_STR_MAX,
};

enum aw_chip_type {
    AW882XX_CHIP_TYPE = 0,
    AW87XXX_CHIP_TYPE,
    CHIP_TYPE_MAX,
};


struct algo_auth_data {
	int32_t auth_mode;  /* 0: disable  1 : chip ID  2 : reg crc */
	int32_t reg_crc;
	int32_t random;
	int32_t chip_id;
	int32_t check_result;
};

int aw_ar_kmsg_ctl_get_auth_data(struct mixer *hw_mixer, struct algo_auth_data *data);
int aw_ar_kmsg_ctl_set_auth_data(struct mixer *hw_mixer, struct algo_auth_data *data);

int aw_ar_kmsg_ctl_monitor_work(struct mixer *hw_mixer,
                unsigned int chip_type, int dev_index, unsigned int *vmax);
int aw_ar_kmsg_ctl_get_monitor_time(struct mixer *hw_mixer,
                unsigned int chip_type, unsigned int *monitor_time);
int aw_ar_kmsg_ctl_get_monitor_en(struct mixer *hw_mixer,
                unsigned int chip_type, int dev_index, unsigned int *monitor_en);
int aw_ar_kmsg_ctl_set_monitor_en(struct mixer *hw_mixer,
                unsigned int chip_type, int dev_index, unsigned int monitor_en);
int aw_ar_kmsg_ctl_get_dev_num(struct mixer *hw_mixer,
                           unsigned int chip_type, int *dev_num);
int aw_ar_kmsg_ctl_get_chip_type(struct mixer *hw_mixer,
                                            unsigned int *chip_type);
int aw_ar_kmsg_node_read_re_range(int dev_index,
                                int *re_min, int *re_max);
int aw_ar_kmsg_ctl_get_voltage_offset(struct mixer *hw_mixer,
                        unsigned int chip_type, int dev_index, int *offset);
int aw_ar_kmsg_ctl_set_iv_output(struct mixer *hw_mixer,
	   unsigned int chip_type, int dev_index, unsigned int iv_output_en);


#endif

