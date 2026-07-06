/*Copyright (c) 2024 AWINIC Technology CO., LTD*/

#ifndef AW_AR_DSP_H_
#define AW_AR_DSP_H_

#include <stdlib.h>
#include <stdint.h>
#include "aw_ar_api.h"

#if defined(__cplusplus)||defined(c_plusplus)
extern "C"{
#endif


#define AW_CTL_GET_PARAM        ("getParam")
#define AW_CTL_SET_PARAM        ("setParam")
#define AW_CTL_CONTROL          ("control")
#define AW_CTL_GET_TAGGED_INFO  ("getTaggedInfo")


#define AW_ALIGN_4BYTE(x)           (((x) + 3) & (~3))
#define AW_ALIGN_8BYTE(x)           (((x) + 7) & (~7))
#define AW_PADDING_ALIGN_8BYTE(x)   ((((x) + 7) & 7) ^ 7)


#define AW_DATA_BUF_MAX     (1024)
#define AW_NAME_BUF_MAX     (256)

#define AW_DSP_MSG_VER              (0x10000000)
#define AW_DSP_CHANNEL_DEFAULT_NUM  (1)

enum {
    AW_SPIN_0 = 0,
    AW_SPIN_90,
    AW_SPIN_180,
    AW_SPIN_270,
    AW_SPIN_MAX,
};

struct aw_set_re_array {
    int32_t channel;
    int32_t set_re;
};


typedef struct aw_hdr {
    int type;
    int opcode_id;
    int version;
    int reseriver[3];
} aw_msg_hdr_t;


struct aw_ar_info {
    struct mixer *mixer; /*virtual mixer*/
    uint32_t miid;
    char pcm_device_name[AW_NAME_BUF_MAX];
    char p_intf_name[AW_NAME_BUF_MAX];
};

enum {
	DSP_MSG_TYPE_WRITE_CMD = 0,
	DSP_MSG_TYPE_WRITE_DATA,
	DSP_MSG_TYPE_READ_DATA,
};

typedef struct aw_msg_hdr_v_1_0_0_0 {
	int32_t checksum;
	int32_t version;
	int32_t type;
	int32_t params_id;
	int32_t channel;
	int32_t num;
	int32_t data_size;
	int32_t reseriver[3];
} aw_msg_hdr_v1_t;

struct aw_gsl_module_id_info_entry {
    uint32_t module_id; /**< module id */
    uint32_t module_iid; /**< globally unique module instance id */
};

/**
 * Structure mapping the tag_id to module info (mid and miid)
 */
struct aw_gsl_tag_module_info_entry {
    uint32_t tag_id; /**< tag id of the module */
    uint32_t num_modules; /**< number of modules matching the tag_id */
    struct aw_gsl_module_id_info_entry module_entry[0]; /**< module list */
};

struct aw_gsl_tag_module_info {
    uint32_t num_tags; /**< number of tags */
    struct aw_gsl_tag_module_info_entry tag_module_entry[0];
    /**< variable payload of type struct gsl_tag_module_info_entry */
};

struct aw_apm_module_param_data_t
{
    uint32_t module_instance_id;
    /**< Valid instance ID of module
    @values  */

    uint32_t param_id;
    /**< Valid ID of the parameter.

    @values See Chapter */

    uint32_t param_size;
    /**< Size of the parameter data based upon the
    module_instance_id/param_id combination.
    @values > 0 bytes, in multiples of
    4 bytes at least */

    uint32_t error_code;
    /**< Error code populated by the entity hosting the	module.
     Applicable only for out-of-band command mode  */
};

int aw_ar_dsp_mix_init(struct aw_dev_info *dev_info);
int aw_ar_dsp_write_vmax(struct mixer *virt_mixer, int dev_index, uint32_t vmax);
int aw_ar_dsp_write_xmax(struct mixer *virt_mixer, int dev_index, uint32_t xmax);
int aw_ar_dsp_write_eq(struct mixer *virt_mixer, int dev_index, uint32_t slot);
int aw_ar_dsp_read_vmax(struct mixer *virt_mixer,
                                int dev_index, uint32_t *vmax);
int aw_ar_dsp_write_voltage_offset(struct mixer *virt_mixer,
                        int dev_index, int offset);
int aw_ar_dsp_read_te(struct mixer *virt_mixer,int dev_index,
                                    int32_t *te);
int aw_ar_dsp_read_st(struct mixer *virt_mixer,int dev_index,
                            int32_t *r0, int32_t *te);
int aw_ar_dsp_read_r0(struct mixer *virt_mixer,
                            int dev_index, int32_t *r0);
int aw_ar_dsp_read_f0_q(struct mixer *virt_mixer,
                            int dev_index, int32_t *f0, int32_t *q);
int aw_ar_dsp_read_f0(struct mixer *virt_mixer,
                                int dev_index, int32_t *f0);
int aw_ar_dsp_cali_en(struct mixer *virt_mixer,
                            int dev_index, int32_t cali_en_mode);
int aw_ar_dsp_hmute_en(struct mixer *virt_mixer,
                        int dev_index, bool is_hmute);
int aw_ar_dsp_read_cali_re(struct mixer *virt_mixer,
                        int dev_index, int *cali_re);
int aw_ar_dsp_write_cali_re(struct mixer *virt_mixer,
                        int dev_index, int cali_re);
int aw_ar_dsp_write_cali_re_array(struct mixer *virt_mixer,
                        int dev_num, struct aw_set_re_array *re_array);
int aw_ar_dsp_noise_en(struct mixer *virt_mixer,
                        int dev_index, bool is_noise);
int aw_ar_dsp_read_spin(struct mixer *virt_mixer, int *spin_mode);
int aw_ar_dsp_write_spin(struct mixer *virt_mixer, int spin_mode);
int aw_ar_dsp_set_mixer_en(struct mixer *virt_mixer, int dev_index,
                                uint32_t mixer_en);
int aw_ar_dsp_algo_en(struct mixer *virt_mixer, uint32_t is_enable);
int aw_ar_dsp_read_algo_auth_data(struct mixer *virt_mixer,
			char *data, unsigned int data_len);
int aw_ar_dsp_write_algo_auth_data(struct mixer *virt_mixer,
			char *data, unsigned int data_len);

int aw_ar_dsp_write_volume(struct mixer *virt_mixer,
            int dev_index, char *data, unsigned int data_len);
int aw_ar_dsp_write_ramp_enable(struct mixer *virt_mixer,
            int dev_index, int enable);
int aw_ar_dsp_write_ramp_params(struct mixer *virt_mixer,
            int dev_index, char *data, unsigned int data_len);
int aw_ar_dsp_read_run_state_avg(struct mixer *virt_mixer,
            int dev_index, char *data, unsigned int data_len);

#if defined(__cplusplus)||defined(c__plusplus)
}
#endif


#endif
