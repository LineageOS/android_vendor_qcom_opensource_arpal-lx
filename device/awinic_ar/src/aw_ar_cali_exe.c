#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "aw_ar_dev.h"
#include "aw_ar_log.h"
#include "aw_ar_api.h"
#include "aw_ar_cali.h"
#include "aw_ar_cali_exe.h"
#include "aw_ar_kmsg.h"


#define AW_HW_CARD          (0)
#define AW_VIRT_CARD        (100)

const char *aw_back_end_name = NULL;
int back_end_name_flag = 0;

static const char *fops_cmd[] = {
    "cali",
    "get_spkr_st",
    "set_cali_re",
    "cali_re",
    "cali_f0",
    "cali_f0_q",
    "cali_all",
    "get_re_range",
    "set_offset",
};

static const char *g_dev_name_list[] = {
    "aw882xx_smartpa_l",
    "aw882xx_smartpa_r",
    "aw882xx_smartpa_sec_l",
    "aw882xx_smartpa_sec_r",
    "aw882xx_smartpa_tert_l",
    "aw882xx_smartpa_tert_r",
    "aw882xx_smartpa_quat_l",
    "aw882xx_smartpa_quat_r",
    "aw882xx_smartpa",
};

static int aw_ar_cali_exe_init(struct aw882xx *aw882xx)
{
    memset(aw882xx, 0, sizeof(struct aw882xx));

    aw882xx->aw_dev = NULL;
    aw882xx->dev_info.hw_mixer = mixer_open(AW_HW_CARD);
    if (!aw882xx->dev_info.hw_mixer) {
        AWPRINTFE("open card[%d] failed", AW_HW_CARD);
        return AW_FAIL;
    }

    aw882xx->dev_info.virt_mixer = mixer_open(AW_VIRT_CARD);
    if (!aw882xx->dev_info.virt_mixer) {
        AWPRINTFE("open card[%d] failed", AW_VIRT_CARD);
        mixer_close(aw882xx->dev_info.hw_mixer);
        return AW_FAIL;
    }

    aw_audioreach_set_noise_en(false);

    return AW_OK;
}

static void aw_ar_cali_exe_deinit(struct aw882xx *aw882xx)
{
    if (aw882xx->aw_dev != NULL) {
        free(aw882xx->aw_dev);
        aw882xx->aw_dev = NULL;
    }

    if (aw882xx->dev_info.hw_mixer) {
        mixer_close(aw882xx->dev_info.hw_mixer);
        aw882xx->dev_info.hw_mixer = NULL;
    }

    if (aw882xx->dev_info.virt_mixer) {
        mixer_close(aw882xx->dev_info.virt_mixer);
        aw882xx->dev_info.virt_mixer = NULL;
    }
}

static int aw_ar_cali_exe_get_cmd_type(char *cmd_str, uint32_t *cmd)
{
    if (strcmp(cmd_str, *(fops_cmd + 0)) == 0) {
        *cmd = AW_FAST_START_CALI;
    } else if (strcmp(cmd_str, *(fops_cmd + 1)) == 0) {
        *cmd = AW_GET_SPKR_ST;
    } else if (strcmp(cmd_str, *(fops_cmd + 2)) == 0) {
        *cmd = AW_SET_RE;
    } else if (strcmp(cmd_str, *(fops_cmd + 3)) == 0) {
        *cmd = AW_RE_CALI;
    } else if (strcmp(cmd_str, *(fops_cmd + 4)) == 0) {
        *cmd = AW_F0_CALI;
    } else if (strcmp(cmd_str, *(fops_cmd + 5)) == 0) {
        *cmd = AW_F0_Q_CALI;
    } else if (strcmp(cmd_str, *(fops_cmd + 6)) == 0) {
        *cmd = AW_CALI_ALL;
    } else if (strcmp(cmd_str, *(fops_cmd + 7)) == 0) {
        *cmd = AW_GET_RE_RANGE;
    } else if (strcmp(cmd_str, *(fops_cmd + 8)) == 0) {
        *cmd = AW_SET_OFFSET;
    } else {
        return AW_FAIL;
    }

    return AW_OK;
}

static int aw_ar_cali_exe_get_back_end_name_type(const char *cmd_str)
{
    if (strncmp(cmd_str, "MI2", 3) == 0) {
        AWPRINTFE("AW_BACK_END_NAME:%s (MI2)", cmd_str);
        aw_back_end_name = cmd_str;
        return 1;
    }

    if (strncmp(cmd_str, "TDM", 3) == 0) {
        AWPRINTFE("AW_BACK_END_NAME:%s (TDM)", cmd_str);
        aw_back_end_name = cmd_str;
        return 1;
    }
    AWPRINTFE("Unknown backend name: %s", cmd_str);
    return 0;
}

static int aw_ar_cali_exe_create_all_dev(struct aw882xx *aw882xx)
{
    int ret = 0;
    int i = 0;

    ret = aw_ar_kmsg_ctl_get_dev_num(aw882xx->dev_info.hw_mixer,
                            AW882XX_CHIP_TYPE, &aw882xx->dev_num);
    if (ret < 0) {
        AWPRINTFE("get dev num failed");
        return AW_FAIL;
    }

    aw882xx->num_or_index = aw882xx->dev_num;

    aw882xx->aw_dev = (struct aw_dev *)calloc(aw882xx->dev_num, sizeof(struct aw_dev));
    if (aw882xx->aw_dev == NULL) {
        AWPRINTFE("calloc failed!");
        return AW_FAIL;
    }

    if (aw882xx->dev_num == 1) {
        aw882xx->aw_dev[0].name = g_dev_name_list[AW_DEV_CH_MAX];
        aw882xx->aw_dev[0].dev_index = 0;
    } else if (aw882xx->dev_num < AW_DEV_CH_MAX) {
        for (i = 0; i < aw882xx->dev_num; i++) {
            aw882xx->aw_dev[i].name = g_dev_name_list[i];
            aw882xx->aw_dev[i].dev_index = i;
        }
    } else {
        AWPRINTFE(" unsupported dev_num:%u", aw882xx->dev_num);
        return AW_FAIL;
    }

    return AW_OK;
}

static int aw_ar_cali_exe_get_option_params(struct aw882xx *aw882xx,
				int option_argc, char **option_argv)
{
    int i = 0;
    int32_t i_set_re = 0;
    int cali_re_time_ms = 0;
    int ret = 0;

    switch (aw882xx->cmd) {
    case AW_FAST_START_CALI:
    case AW_CALI_ALL:
    case AW_RE_CALI:
        if (option_argc >= 1) {
            cali_re_time_ms = atoi(option_argv[0]);
            ret = aw_audioreach_set_cali_re_time((unsigned int)cali_re_time_ms);
            if (ret < 0) {
                AWPRINTFE("set cali re time failed");
                return AW_FAIL;
            }
        }
        break;
    case AW_F0_CALI:
    case AW_F0_Q_CALI:
        if (option_argc >= 1) {
            aw_audioreach_set_noise_en(true);
        }
        break;
    case AW_SET_RE:
        if (option_argc != aw882xx->dev_num) {
            AWPRINTFE("command params error, please check");
            return AW_FAIL;
        }

        for (i = 0; i < aw882xx->dev_num; i++) {
            aw882xx->set_re[i] = atoi(option_argv[i]);
        }
        break;
    case AW_GET_SPKR_ST:
    case AW_GET_RE_RANGE:
    case AW_SET_OFFSET:
        break;
    }

    return AW_OK;
}

static int aw_ar_cali_exe_get_all_dev_params(struct aw882xx *aw882xx,
				int argc, char **argv , int back_end_name_flag)
{
    int ret = 0;
    if(back_end_name_flag == 0)
    {
        if (argc < 2) {
            AWPRINTFE("cmd error, please check");
            return AW_FAIL;
        }

        ret = aw_ar_cali_exe_create_all_dev(aw882xx);
        if (ret < 0) {
            AWPRINTFE("get device failed");
            return ret;
        }

        if (argc > 2) {
            ret = aw_ar_cali_exe_get_option_params(aw882xx, argc - 2, &argv[2]);
            if (ret < 0)
                return ret;
        }
    }else{
        if (argc < 3) {
            AWPRINTFE("cmd error, please check");
            return AW_FAIL;
        }

        ret = aw_ar_cali_exe_create_all_dev(aw882xx);
        if (ret < 0) {
            AWPRINTFE("get device failed");
            return ret;
        }

        if (argc > 3) {
            ret = aw_ar_cali_exe_get_option_params(aw882xx, argc - 3, &argv[3]);
            if (ret < 0)
                return ret;
        }
    }
    return AW_OK;
}

static int aw_ar_cali_exe_create_single_dev(struct aw882xx *aw882xx,  char *dev_name)
{
    int i = 0;

    aw882xx->dev_num = 1;
    aw882xx->aw_dev = (struct aw_dev *)calloc(1, sizeof(struct aw_dev));
    if (aw882xx->aw_dev == NULL) {
        AWPRINTFE("calloc failed!");
        return AW_FAIL;
    }

    for (i = 0; i < AW_DEV_CH_MAX; i++) {
        if (!strcmp(dev_name, g_dev_name_list[i])) {
            aw882xx->aw_dev[0].name = dev_name;
            aw882xx->aw_dev[0].dev_index = i;
            aw882xx->num_or_index = i;
            break;
        }
    }

    if (!strcmp(dev_name, g_dev_name_list[AW_DEV_CH_MAX])) {
        aw882xx->aw_dev[0].name = dev_name;
        aw882xx->aw_dev[0].dev_index = 0;
        aw882xx->num_or_index = 0;
    }

    return AW_OK;
}


static int aw_ar_cali_exe_get_single_dev_params(struct aw882xx *aw882xx,
        int argc, char **argv, int back_end_name_flag)
{
    int ret = AW_FAIL;
    if(back_end_name_flag == 0)
    {
        if (argc < 3) {
            AWPRINTFE("cmd error, please check");
            return AW_FAIL;
        }

        ret = aw_ar_cali_exe_create_single_dev(aw882xx, argv[1]);
        if (ret < 0)
            return ret;

        if (argc > 3) {
            ret = aw_ar_cali_exe_get_option_params(aw882xx, argc - 3, &argv[3]);
            if (ret < 0)
                return ret;
        }

    }else{
        if (argc < 4) {
            AWPRINTFE("cmd error, please check");
            return AW_FAIL;
        }

        ret = aw_ar_cali_exe_create_single_dev(aw882xx, argv[2]);
        if (ret < 0)
            return ret;

        if (argc > 4) {
            ret = aw_ar_cali_exe_get_option_params(aw882xx, argc - 4, &argv[4]);
            if (ret < 0)
                return ret;
        }
    }
    aw_audioreach_set_single_cali_en(true);

    return AW_OK;
}


static int aw_ar_cali_exe_get_params(struct aw882xx *aw882xx,
                int argc, char **argv)
{
    int ret = AW_FAIL;

    if (aw_ar_cali_exe_get_back_end_name_type(argv[1]))
    {
        back_end_name_flag = 1;
        ret = aw_ar_cali_exe_get_cmd_type(argv[2], &aw882xx->cmd);
        if (ret == AW_OK) {
            return aw_ar_cali_exe_get_all_dev_params(aw882xx, argc, argv, back_end_name_flag);
        }

        ret = aw_ar_cali_exe_get_cmd_type(argv[3], &aw882xx->cmd);
        if (ret == AW_OK) {
            return aw_ar_cali_exe_get_single_dev_params(aw882xx, argc, argv, back_end_name_flag);
        }
    }else{
        back_end_name_flag = 0;
        ret = aw_ar_cali_exe_get_cmd_type(argv[1], &aw882xx->cmd);
        if (ret == AW_OK) {
            return aw_ar_cali_exe_get_all_dev_params(aw882xx, argc, argv, back_end_name_flag);
        }

        ret = aw_ar_cali_exe_get_cmd_type(argv[2], &aw882xx->cmd);
        if (ret == AW_OK) {
            return aw_ar_cali_exe_get_single_dev_params(aw882xx, argc, argv, back_end_name_flag);
        }
    }

    AWPRINTFE("cmd error, please check");

    return AW_FAIL;
}

static int aw_ar_cali_exe_set_cali_re_to_file(struct aw882xx *aw882xx,
                int *re)
{
    int ret = 0;
    int re_min[AW_DEV_CH_MAX] = { 0 };
    int re_max[AW_DEV_CH_MAX] = { 0 };
    int i = 0;

    ret = aw_audioreach_dsp_get_re_range(re_min, re_max, aw882xx->num_or_index);
    if (ret < 0) {
        AWPRINTFE("get re range failed");
        return AW_FAIL;
    }

    for (i = 0; i < aw882xx->dev_num; i++) {
        if (re[i] > re_max[i] || re[i] < re_min[i]) {
            AWPRINTFE("[%s]re[%d] out of range [%d:%d]",
                   aw882xx->aw_dev[i].name, re[i], re_min[i], re_max[i]);
            return AW_FAIL;
        }
    }

    ret = aw_audioreach_set_re_to_file(re, aw882xx->num_or_index);
    if (ret < 0) {
        AWPRINTFE("set re to file failed");
        return AW_FAIL;
    }

    return AW_OK;
}

static int aw_ar_cali_exe_set_cali_re_to_dsp(struct aw882xx *aw882xx, int *re)
{
    int ret = 0;
    int i = 0;

    ret = aw_audioreach_dsp_set_re(&aw882xx->dev_info,
                    re, aw882xx->num_or_index);
    if (ret < 0) {
        AWPRINTFE("set re failed");
        return AW_FAIL;
    }

    for (i = 0; i < aw882xx->dev_num; i++) {
        printf("[%s]:set cali re %d\n", aw882xx->aw_dev[i].name, re[i]);
    }

    return AW_OK;
}

static int aw_ar_cali_exe_set_offset_to_dsp(struct aw882xx *aw882xx)
{
    int ret = 0;
    int i = 0;

    ret = aw_audioreach_dsp_set_offset(&aw882xx->dev_info, aw882xx->num_or_index);
    if (ret < 0) {
        AWPRINTFE("set offset failed");
        return AW_FAIL;
    }

    return AW_OK;
}

static int aw_ar_cali_exe_cali_re(struct aw882xx *aw882xx)
{
    int ret = 0;
    int cali_re[AW_DEV_CH_MAX] = { 0 };
    int i = 0;

    ret = aw_audioreach_dsp_read_r0(&aw882xx->dev_info,
                                cali_re, aw882xx->num_or_index);
    if (ret < 0) {
        AWPRINTFE("cali re failed");
        return AW_FAIL;
    }

    for (i = 0; i < aw882xx->dev_num; i++) {
        printf("[%s]cali_RE = %d\n", aw882xx->aw_dev[i].name, cali_re[i]);
    }

    ret = aw_ar_cali_exe_set_cali_re_to_file(aw882xx, cali_re);
    if (ret < 0) {
        AWPRINTFE("set re to file failed");
        return ret;
    }

    return AW_OK;
}

static int aw_ar_cali_exe_cali_f0(struct aw882xx *aw882xx)
{
    int ret = 0;
    int cali_f0[AW_DEV_CH_MAX] = { 0 };
    int i = 0;

    ret = aw_audioreach_dsp_read_f0(&aw882xx->dev_info,
                                cali_f0, aw882xx->num_or_index);
    if (ret < 0) {
        AWPRINTFE("cali f0 failed");
        return AW_FAIL;
    }

    for (i = 0; i < aw882xx->dev_num; i++) {
        printf("[%s]cali_f0 = %d\n", aw882xx->aw_dev[i].name, cali_f0[i]);
    }

    return AW_OK;
}

static int aw_ar_cali_exe_cali_f0_q(struct aw882xx *aw882xx)
{
    int ret = 0;
    int cali_f0[AW_DEV_CH_MAX] = { 0 };
    int cali_q[AW_DEV_CH_MAX] = { 0 };
    int i = 0;

    ret = aw_audioreach_dsp_read_f0_q(&aw882xx->dev_info,
                            cali_f0, cali_q, aw882xx->num_or_index);
    if (ret < 0) {
        AWPRINTFE("cali f0 q failed");
        return AW_FAIL;
    }

    for (i = 0; i < aw882xx->dev_num; i++) {
        printf("[%s]spkr f0 = %d\n", aw882xx->aw_dev[i].name, cali_f0[i]);
        printf("[%s]spkr q = %d\n", aw882xx->aw_dev[i].name, cali_q[i]);
    }

    return AW_OK;
}

static int aw_ar_cali_exe_cali(struct aw882xx *aw882xx)
{
    int ret = 0;

    aw_audioreach_set_noise_en(true);

    ret = aw_ar_cali_exe_cali_re(aw882xx);
    if (ret < 0)
        return ret;

    ret = aw_ar_cali_exe_cali_f0(aw882xx);
    if (ret < 0)
        return ret;

    return AW_OK;
}

static int aw_ar_cali_exe_cali_all(struct aw882xx *aw882xx)
{
    int ret = 0;

    aw_audioreach_set_noise_en(true);

    ret = aw_ar_cali_exe_cali_re(aw882xx);
    if (ret < 0)
        return ret;

    ret = aw_ar_cali_exe_cali_f0_q(aw882xx);
    if (ret < 0)
        return ret;

    return AW_OK;
}

static int aw_ar_cali_exe_set_cali_re(struct aw882xx *aw882xx)
{
    int ret = 0;
    int i = 0;

    ret = aw_ar_cali_exe_set_cali_re_to_dsp(aw882xx, aw882xx->set_re);

    ret = aw_ar_cali_exe_set_cali_re_to_file(aw882xx, aw882xx->set_re);
    if (ret < 0) {
        AWPRINTFE("set re to file failed");
        return ret;
    }

    return AW_OK;
}

static int aw_ar_cali_exe_set_offset(struct aw882xx *aw882xx)
{
    int ret = 0;
    int i = 0;

    ret = aw_ar_cali_exe_set_offset_to_dsp(aw882xx);
    return AW_OK;
}

static int aw_ar_cali_exe_get_spkr_st(struct aw882xx *aw882xx)
{
    int ret = 0;
    int r0[AW_DEV_CH_MAX] = { 0 };
    int te[AW_DEV_CH_MAX] = { 0 };
    int i = 0;

    ret = aw_audioreach_dsp_read_st(&aw882xx->dev_info,
                            r0, te, aw882xx->num_or_index);
    if (ret < 0) {
        AWPRINTFE("get st failed");
        return ret;
    }

    for (i = 0; i < aw882xx->dev_num; i++) {
        printf("[%s]spkr R0 = %d\n", aw882xx->aw_dev[i].name, r0[i]);
        printf("[%s]spkr Te = %d\n", aw882xx->aw_dev[i].name, te[i]);
    }

    return AW_OK;
}

static int aw_ar_cali_exe_get_re_range(struct aw882xx *aw882xx)
{
    int ret = 0;
    int re_min[AW_DEV_CH_MAX] = { 0 };
    int re_max[AW_DEV_CH_MAX] = { 0 };
    int i = 0;


    ret = aw_audioreach_dsp_get_re_range(re_min, re_max, aw882xx->num_or_index);
    if (ret < 0) {
        AWPRINTFE("get re range failed");
        return AW_FAIL;
    }

    for (i = 0; i < aw882xx->dev_num; i++)
        printf("[%s]:re_min:%d mOhms re_max:%d mOhms\n", aw882xx->aw_dev[i].name,
                re_min[i], re_max[i]);

    return AW_OK;
}

static int aw_ar_cali_exe_command(struct aw882xx *aw882xx)
{
    int ret = 0;

    switch (aw882xx->cmd) {
    case AW_FAST_START_CALI:
        ret = aw_ar_cali_exe_cali(aw882xx);
        break;
    case AW_RE_CALI:
        ret = aw_ar_cali_exe_cali_re(aw882xx);
        break;
    case AW_F0_CALI:
        ret = aw_ar_cali_exe_cali_f0(aw882xx);
        break;
    case AW_F0_Q_CALI:
        ret = aw_ar_cali_exe_cali_f0_q(aw882xx);
        break;
    case AW_CALI_ALL:
        ret = aw_ar_cali_exe_cali_all(aw882xx);
        break;
    case AW_SET_RE:
        ret = aw_ar_cali_exe_set_cali_re(aw882xx);
        break;
    case AW_GET_SPKR_ST:
        ret = aw_ar_cali_exe_get_spkr_st(aw882xx);
        break;
    case AW_GET_RE_RANGE:
        ret = aw_ar_cali_exe_get_re_range(aw882xx);
        break;
    case AW_SET_OFFSET:
        ret = aw_ar_cali_exe_set_offset(aw882xx);
        break;
    default:
        AWPRINTFE("cmd err, please check !");
        ret = AW_FAIL;
    }

exit:
    return ret;

}


int main(int argc , char **argv)
{
    int ret = AW_FAIL;
    struct aw882xx aw882xx;

    if (argc < 2) {
        printf(" Calibration executables version: %s\n", AW_AR_VERSION);
        printf("----------------------------------------------------------------------------------\n");
        printf("./aw882xx_cali [back_end_name] [dev_name] cmd [optional params]\n");
        printf("----------------------------------------------------------------------------------\n");
        printf("./aw882xx_cali [back_end_name] [dev_name] cali [cali_re_time(ms)]\n");
        printf("----------------------------------------------------------------------------------\n");
        printf("./aw882xx_cali [back_end_name] [dev_name] cali_re [cali_re_time(ms)]\n");
        printf("----------------------------------------------------------------------------------\n");
        printf("./aw882xx_cali [back_end_name] [dev_name] cali_f0 [noise]\n");
        printf("----------------------------------------------------------------------------------\n");
        printf("./aw882xx_cali [back_end_name] [dev_name] get_spkr_st\n");
        printf("----------------------------------------------------------------------------------\n");
        printf("./aw882xx_cali [back_end_name] [dev_name] set_cali_re re_value1 [re_value2] ...\n");
        printf("----------------------------------------------------------------------------------\n");
        printf("./aw882xx_cali [back_end_name] [dev_name] cali_f0_q [noise]\n");
        printf("----------------------------------------------------------------------------------\n");
        printf("./aw882xx_cali [back_end_name] [dev_name] cali_all [cali_re_time(ms)]\n");
        printf("----------------------------------------------------------------------------------\n");
        printf("./aw882xx_cali [back_end_name] [dev_name] get_re_range\n");
        printf("----------------------------------------------------------------------------------\n");
        return AW_FAIL;
    }

    ret = aw_ar_cali_exe_init(&aw882xx);
    if (ret < 0) {
        AWPRINTFE("cali exe init failed");
        return ret;
    }

    ret = aw_ar_cali_exe_get_params(&aw882xx, argc, argv);
    if (ret < 0) {
        AWPRINTFE("parse params failed, please check");
        goto exit;
    }

    ret = aw_ar_cali_exe_command(&aw882xx);

exit:
    aw_ar_cali_exe_deinit(&aw882xx);
    return ret;
}


