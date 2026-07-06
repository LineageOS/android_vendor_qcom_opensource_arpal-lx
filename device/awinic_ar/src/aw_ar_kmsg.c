/*Copyright (c) 2024 AWINIC Technology CO., LTD*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <tinyalsa/asoundlib.h>

#include "aw_ar_dev.h"
#include "aw_ar_kmsg.h"
#include "aw_ar_log.h"



#define AW882XX_MONITOR_WORK_CTL    ("aw_dev_%u_hal_mon_work")
#define AW882XX_MONITOR_TIME_CTL    ("aw882xx_hal_monitor_time")
#define AW882XX_MONITOR_EN_CTL      ("aw_dev_%d_monitor")
#define AW882XX_VOLTAGE_OFFSET_CTL  ("aw_dev_%d_offset")
#define AW882XX_IV_OUTPUT_CTL       ("aw_dev_%d_iv_output")

#define AW87XXX_MONITOR_WORK_CTL    ("aw87xxx_vmax_get_%d")
#define AW87XXX_MONITOR_TIME_CTL    ("aw87xxx_hal_monitor_time")
#define AW87XXX_MONITOR_EN_CTL      ("aw87xxx_monitor_switch_%d")

enum {
    AW_MONITOR_WORK_CTL = 0,
    AW_MONITOR_TIME_CTL,
    AW_MONITOR_EN_CTL,
    AW_VOLTAGE_OFFSET_CTL,
    AW_IV_OUTPUT_CTL,
    AW_CTL_MAX,
};

static const char *g_ctl_list[AW_CTL_MAX][CHIP_TYPE_MAX] ={
    {AW882XX_MONITOR_WORK_CTL, AW87XXX_MONITOR_WORK_CTL},
    {AW882XX_MONITOR_TIME_CTL, AW87XXX_MONITOR_TIME_CTL},
    {AW882XX_MONITOR_EN_CTL, AW87XXX_MONITOR_EN_CTL},
    {AW882XX_VOLTAGE_OFFSET_CTL, NULL},
    {AW882XX_IV_OUTPUT_CTL, NULL},
};


static int aw_ar_ctl_get_name_from_list(unsigned int chip_type,
                                        unsigned int ctl_index, const char **name)
{
    if (chip_type >= CHIP_TYPE_MAX) {
        AWLOGE("Invalid chip_type:%d", chip_type);
        return AW_FAIL;
    }

    if (ctl_index >= AW_CTL_MAX) {
        AWLOGE("Invalid ctl_index:%d", ctl_index);
        return AW_FAIL;
    }

    *name = g_ctl_list[ctl_index][chip_type];

    return AW_OK;
}


int aw_ar_kmsg_ctl_monitor_work(struct mixer *hw_mixer,
                unsigned int chip_type, int dev_index, unsigned int *vmax)
{
    int ret = 0;
    struct mixer_ctl *ctl = NULL;
    char kctl_name[AW_CTL_NAME_BUF_MAX] = { 0 };
    const char *ctl_base_name = NULL;

    if (hw_mixer == NULL) {
        AWLOGE("hw_mixer is NULL");
        return AW_FAIL;
    }

    ret = aw_ar_ctl_get_name_from_list(chip_type,
                AW_MONITOR_WORK_CTL, &ctl_base_name);
    if (ret < 0) {
        AWLOGE("get monitor work control name failed");
        return ret;
    }

    snprintf(kctl_name, AW_CTL_NAME_BUF_MAX, ctl_base_name, dev_index);

    ctl = mixer_get_ctl_by_name(hw_mixer, kctl_name);
    if (!ctl) {
        AWLOGE("Invalid mixer control: %s", kctl_name);
        return AW_FAIL;
    }

    *vmax = mixer_ctl_get_value(ctl, 0);

    return AW_OK;
}

int aw_ar_kmsg_ctl_get_monitor_time(struct mixer *hw_mixer,
                    unsigned int chip_type, unsigned int *monitor_time)
{
    int ret = 0;
    struct mixer_ctl *ctl = NULL;
    const char *ctl_name = NULL;

    if (hw_mixer == NULL) {
        AWLOGE("hw_mixer is NULL");
        return AW_FAIL;
    }

    ret = aw_ar_ctl_get_name_from_list(chip_type,
                AW_MONITOR_TIME_CTL, &ctl_name);
    if (ret < 0) {
        AWLOGE("get monitor time control name failed");
        return ret;
    }

    ctl = mixer_get_ctl_by_name(hw_mixer, ctl_name);
    if (!ctl) {
        //AWLOGE("Invalid mixer control: %s", AW_AR_MON_TIME_CTL);
        return AW_FAIL;
    }

    *monitor_time = mixer_ctl_get_value(ctl, 0);

    AWLOGD("monitor time:%d", *monitor_time);
    return AW_OK;
}

int aw_ar_kmsg_ctl_get_monitor_en(struct mixer *hw_mixer,
             unsigned int chip_type, int dev_index, unsigned int *monitor_en)
{
    int ret = 0;
    struct mixer_ctl *ctl = NULL;
    char kctl_name[AW_CTL_NAME_BUF_MAX] = { 0 };
    const char *ctl_base_name = NULL;

    if (hw_mixer == NULL) {
        AWLOGE("hw_mixer is NULL");
        return AW_FAIL;
    }

    ret = aw_ar_ctl_get_name_from_list(chip_type,
                AW_MONITOR_EN_CTL, &ctl_base_name);
    if (ret < 0) {
        AWLOGE("get monitor enable control name failed");
        return ret;
    }

    snprintf(kctl_name, AW_CTL_NAME_BUF_MAX, ctl_base_name, dev_index);

    ctl = mixer_get_ctl_by_name(hw_mixer, kctl_name);
    if (!ctl) {
        //AWLOGE("dev_index[%d]:Invalid mixer control: %s", dev_index, kctl_name);
        return AW_FAIL;
    }

    *monitor_en = mixer_ctl_get_value(ctl, 0);

    AWLOGD("dev_index[%d]:monitor enable:%d", dev_index, *monitor_en);

    return AW_OK;
}

int aw_ar_kmsg_ctl_set_monitor_en(struct mixer *hw_mixer,
              unsigned int chip_type, int dev_index, unsigned int monitor_en)
{
    int ret = 0;
    struct mixer_ctl *ctl = NULL;
    char kctl_name[AW_CTL_NAME_BUF_MAX] = { 0 };
    const char *ctl_base_name = NULL;

    if (hw_mixer == NULL) {
        AWLOGE("hw_mixer is NULL");
        return AW_FAIL;
    }

    ret = aw_ar_ctl_get_name_from_list(chip_type,
                AW_MONITOR_EN_CTL, &ctl_base_name);
    if (ret < 0) {
        AWLOGE("get monitor enable control name failed");
        return ret;
    }

    snprintf(kctl_name, AW_CTL_NAME_BUF_MAX, ctl_base_name, dev_index);

    ctl = mixer_get_ctl_by_name(hw_mixer, kctl_name);
    if (!ctl) {
        //AWLOGE("dev_index[%d]:Invalid mixer control: %s", dev_index, kctl_name);
        return AW_FAIL;
    }

    mixer_ctl_set_value(ctl, 0, monitor_en);

    AWLOGD("dev_index[%d]:set monitor enable:%d", dev_index, monitor_en);

    return AW_OK;
}

int aw_ar_kmsg_ctl_get_dev_num(struct mixer *hw_mixer,
                           unsigned int chip_type, int *dev_num)
{
    int ret = 0;
    uint32_t monitor_en = 0;
    int i = 0;

    for (i = 0; i< AW_DEV_CH_MAX; i++) {
        ret = aw_ar_kmsg_ctl_get_monitor_en(hw_mixer, chip_type, i, &monitor_en);
        if (ret < 0) {
            break;
        }
    }

    if (i == 0) {
        AWLOGE("get dev_num failed");
        return AW_FAIL;
    }

    *dev_num = i;

    AWLOGD("get dev_num:%d", *dev_num);

    return AW_OK;
}

int aw_ar_kmsg_ctl_get_chip_type(struct mixer *hw_mixer,
                                            unsigned int *chip_type)
{
    int ret = 0;
    uint32_t monitor_time = 0;

    ret = aw_ar_kmsg_ctl_get_monitor_time(hw_mixer, AW882XX_CHIP_TYPE, &monitor_time);
    if (ret < 0) {
        ret = aw_ar_kmsg_ctl_get_monitor_time(hw_mixer, AW87XXX_CHIP_TYPE, &monitor_time);
        if (ret < 0) {
            AWLOGE("get chip type failed");
            return AW_FAIL;
        }
        *chip_type = AW87XXX_CHIP_TYPE;
        return AW_OK;
    }

    *chip_type = AW882XX_CHIP_TYPE;
    return AW_OK;
}

int aw_ar_kmsg_ctl_get_voltage_offset(struct mixer *hw_mixer,
             unsigned int chip_type, int dev_index, int *offset)
{
    int ret = 0;
    struct mixer_ctl *ctl = NULL;
    char kctl_name[AW_CTL_NAME_BUF_MAX] = { 0 };
    const char *ctl_base_name = NULL;

    if (hw_mixer == NULL) {
        AWLOGE("hw_mixer is NULL");
        return AW_FAIL;
    }

    ret = aw_ar_ctl_get_name_from_list(chip_type,
                AW_VOLTAGE_OFFSET_CTL, &ctl_base_name);
    if (ret < 0) {
        AWLOGE("get monitor enable control name failed");
        return ret;
    }

    snprintf(kctl_name, AW_CTL_NAME_BUF_MAX, ctl_base_name, dev_index);

    ctl = mixer_get_ctl_by_name(hw_mixer, kctl_name);
    if (!ctl) {
        AWLOGE("dev_index[%d]:Invalid mixer control: %s", dev_index, kctl_name);
        return AW_FAIL;
    }

    *offset = mixer_ctl_get_value(ctl, 0);

    AWLOGD("dev_index[%d]:offset: %d", dev_index, *offset);

    return AW_OK;
}

int aw_ar_kmsg_ctl_set_iv_output(struct mixer *hw_mixer,
	unsigned int chip_type, int dev_index, unsigned int iv_output_en)
{
     int ret = 0;
     struct mixer_ctl *ctl = NULL;
     char kctl_name[AW_CTL_NAME_BUF_MAX] = { 0 };
     const char *ctl_base_name = NULL;

     if (hw_mixer == NULL) {
     AWLOGE("hw_mixer is NULL");
     return AW_FAIL;
     }

     ret = aw_ar_ctl_get_name_from_list(chip_type,
	  AW_IV_OUTPUT_CTL, &ctl_base_name);
     if (ret < 0) {
     AWLOGE("get monitor enable control name failed");
     return ret;
     }

     snprintf(kctl_name, AW_CTL_NAME_BUF_MAX, ctl_base_name, dev_index);

     ctl = mixer_get_ctl_by_name(hw_mixer, kctl_name);
     if (!ctl) {
	     AWLOGE("dev_index[%d]:Invalid mixer control: %s", dev_index, kctl_name);
	     return AW_FAIL;
     }

     mixer_ctl_set_value(ctl, 0, iv_output_en);

     AWLOGD("dev_index[%d]:set monitor enable:%d", dev_index, iv_output_en);

     return AW_OK;
}

int aw_ar_kmsg_ctl_get_auth_data(struct mixer *hw_mixer, struct algo_auth_data *data)
{
    int ret = 0;
    struct mixer_ctl *ctl = NULL;
	const char *ctl_name = "aw_algo_auth";

    if (hw_mixer == NULL) {
        AWLOGE("hw_mixer is NULL");
        return AW_FAIL;
    }

    ctl = mixer_get_ctl_by_name(hw_mixer, ctl_name);
    if (!ctl) {
        AWLOGE("Invalid mixer control: %s", ctl_name);
        return AW_FAIL;
    }

	ret = mixer_ctl_get_array(ctl, data, sizeof(struct algo_auth_data));
    if (ret < 0) {
        AWLOGE("%s:Failed to get data failed\n", ctl_name);
        return AW_FAIL;
    }

	return ret;
}


int aw_ar_kmsg_ctl_set_auth_data(struct mixer *hw_mixer, struct algo_auth_data *data)
{
    int ret = 0;
    struct mixer_ctl *ctl = NULL;
	const char *ctl_name = "aw_algo_auth";

    if (hw_mixer == NULL) {
        AWLOGE("hw_mixer is NULL");
        return AW_FAIL;
    }

    ctl = mixer_get_ctl_by_name(hw_mixer, ctl_name);
    if (!ctl) {
        AWLOGE("Invalid mixer control: %s", ctl_name);
        return AW_FAIL;
    }

	ret = mixer_ctl_set_array(ctl, data, sizeof(struct algo_auth_data));
    if (ret < 0) {
        AWLOGE("%s: Failed to set data\n", ctl_name);
        return AW_FAIL;
    }

	return ret;

}



/********************************************************
*
*   Node operation
*   node path: /dev/aw882xx_smartpa
*
********************************************************/
static const char *ch_name[AW_DEV_CH_MAX] = {"pri_l", "pri_r", "sec_l", "sec_r",
                        "tert_l", "tert_r", "quat_l", "quat_r"};
static const char *cali_str[CALI_STR_MAX] = {
        "none", "dev_sel", "get_ver", "get_dev_num",
         "get_re_range"
};

static int aw_ar_kmsg_node_open_dev(int *fd)
{
    *fd = open("/dev/aw882xx_smartpa", O_RDWR);
    if (*fd < 0) {
        AWLOGE("open /dev/aw882xx_smartpa failed");
        return AW_FAIL;
    }

    return AW_OK;
}

static void aw_ar_kmsg_node_colse_dev(int fd)
{
    close(fd);
}

static int aw_ar_kmsg_node_write_data(const char *data_buf, int data_size)
{
    int ret = 0;
    int fd = 0;

    ret = aw_ar_kmsg_node_open_dev(&fd);
    if (ret < 0)
        return ret;

    ret = write(fd, data_buf, data_size);
    if (ret <= 0) {
        AWLOGE("write data to dev node failed");
    }

    aw_ar_kmsg_node_colse_dev(fd);
    return ret;
};

static int aw_ar_kmsg_node_read_data(char *data_buf, int data_size)
{
    int ret = 0;
    int fd = 0;

    ret = aw_ar_kmsg_node_open_dev(&fd);
    if (ret < 0)
        return ret;

    ret = read(fd, data_buf, data_size);
    if (ret < 0) {
        AWLOGE("read data from dev node failed");
    }

    aw_ar_kmsg_node_colse_dev(fd);

    return ret;
};

/*write cmd*/
static int aw_ar_kmsg_node_write_cmd(unsigned int cmd)
{
    int ret;

    if ((cmd >= CALI_STR_MAX) || (cmd == CALI_STR_DEV_SEL)) {
        AWLOGE("cmd:%d unsupported", cmd);
        return AW_FAIL;
    }

    ret = aw_ar_kmsg_node_write_data(cali_str[cmd],
            strlen(cali_str[cmd]) + 1);
    if (ret < 0) {
        AWLOGE("write cmd %s faild", cali_str[cmd]);
        return ret;
    }

    return AW_OK;
}


#define READ_RE_RANGE_MAX (256)
#define STR_DATA_BUF_MAX (32)


int aw_ar_kmsg_node_read_re_range(int dev_index,
                                int *re_min, int *re_max)
{
    int ret = 0;
    char re_range_buf[READ_RE_RANGE_MAX] = { 0 };
    int re_min_buf[AW_DEV_CH_MAX] = { 0 };
    int re_max_buf[AW_DEV_CH_MAX] = { 0 };
    char str_data[STR_DATA_BUF_MAX] = { 0 };
    int dev_num = 0;
    int i = 0;
    int len = 0;

    ret = aw_ar_kmsg_node_write_cmd(CALI_STR_SHOW_RE_RANGE);
    if (ret < 0)
        return ret;

    ret = aw_ar_kmsg_node_read_data(re_range_buf, sizeof(re_range_buf));
    if (ret < 0) {
        AWLOGE("read re range buf failed");
        return ret;
    }


    for (i = 0; i < AW_DEV_CH_MAX; i++) {
        memset(str_data, 0, sizeof(str_data));
        snprintf(str_data, sizeof(str_data), "%s:re_min:%s re_max:%s ", ch_name[i], "%d", "%d");

        ret = sscanf(re_range_buf + len, str_data, &re_min_buf[i], &re_max_buf[i]);
        if (ret <= 0) {
            if (i == 0) {
                AWLOGE("unsupported str: %s", re_range_buf);
                return -1;
            }
            dev_num = i;
            break;
        }
        len += snprintf(str_data, sizeof(str_data), "%s:re_min:%d re_max:%d ",
            ch_name[i], re_min_buf[i], re_max_buf[i]);
    }


    if (dev_index >= dev_num) {
        AWLOGE("dev_index[%d] is greater than dev_num[%d]", dev_index, dev_num);
        return AW_FAIL;
    }

    *re_min = re_min_buf[dev_index];
    *re_max = re_max_buf[dev_index];

    return AW_OK;
}
