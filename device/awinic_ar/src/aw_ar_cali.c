#include <stdio.h>
#include <string.h>

#include "aw_ar_log.h"
#include "aw_ar_dev.h"
#include "aw_ar_api.h"
#include "aw_ar_kmsg.h"
#include "aw_ar_dsp.h"
#include "aw_ar_cali.h"




#define AW_INT_DEC_DIGIT            (10)
#define AW_RE_BUF_MAX             (AW_DEV_CH_MAX * AW_INT_DEC_DIGIT + 1)



#define AW_CALI_READ_RE_TIMES       (8)
#define AW_CALI_READ_F0_Q_TIMES     (5)
#define AW_CALI_RE_DEFAULT_TIMER    (3000)
#define AW_CALI_F0_TIME             (5000 * 1000)



#ifndef AWINIC_CALI_FILE
#define AWINIC_CALI_FILE    "/mnt/vendor/persist/factory/audio/aw_cali.bin"
#endif
static bool g_is_single_cali = false;
static unsigned int g_cali_re_time_ms = AW_CALI_RE_DEFAULT_TIMER;
static unsigned int g_noise_flag = CALI_OPS_NOISE;
static uint32_t g_monitor_en[AW_DEV_CH_MAX] = { 0 };


void aw_audioreach_set_single_cali_en(bool is_signle_cali)
{
    g_is_single_cali = is_signle_cali;
}

int aw_audioreach_set_cali_re_time(unsigned int cali_time_ms)
{
    int ret = 0;

    if (cali_time_ms < 1000) {
        AWLOGE("time:%d is too short, no set", cali_time_ms);
        return AW_FAIL;
    }

    g_cali_re_time_ms = cali_time_ms;

    AWLOGD("set time:%d", g_cali_re_time_ms);
    return AW_OK;
}

void aw_audioreach_set_noise_en(bool is_noise_en)
{
    if (is_noise_en) {
        g_noise_flag = CALI_OPS_NOISE;
    } else {
        g_noise_flag = 0;
    }
}

static int aw_ar_cali_dev_set_cali_status(struct aw_dev_info *dev_info,
                                                        int dev_index, int status)
{
    int ret = 0;
    uint32_t monitor_en = 0;

    if (status) {
        /*set monitor stop*/
            ret = aw_ar_kmsg_ctl_get_monitor_en(dev_info->hw_mixer,
                    AW882XX_CHIP_TYPE, dev_index, &g_monitor_en[dev_index]);
            if (ret < 0) {
                AWLOGE("get monitor status failed");
                return ret;
            }

            ret = aw_ar_kmsg_ctl_set_monitor_en(dev_info->hw_mixer,
                    AW882XX_CHIP_TYPE, dev_index, monitor_en);
            if (ret < 0) {
                AWLOGE("set monitor disable failed");
                return ret;
            }
    } else {
        /*restore monitor status*/
        ret = aw_ar_kmsg_ctl_set_monitor_en(dev_info->hw_mixer,
                AW882XX_CHIP_TYPE, dev_index, g_monitor_en[dev_index]);
        if (ret < 0) {
            AWLOGE("restore monitor disable failed");
            return ret;
        }
    }

    return AW_OK;
}

static int aw_ar_cali_dev_get_re(struct aw_dev_info *dev_info,
                                            int *cali_re, int dev_index)
{
    int ret =0;
    int i = 0;
    int re = 0;
    int sum = 0;

    for (i = 0; i < AW_CALI_READ_RE_TIMES; i++) {
        ret = aw_ar_dsp_read_r0(dev_info->virt_mixer, dev_index, &re);
        if (ret < 0) {
            AWLOGE("dev[%d]:get re failed!", dev_index);
            return ret;;
        }
        sum += re;
        usleep(AW_32000_US);
    }
    *cali_re = sum / AW_CALI_READ_RE_TIMES;

    AWLOGD("dev[%d]cali re is %d", dev_index, *cali_re);

    return AW_OK;
}

static int aw_ar_cali_devs_get_re(struct aw_dev_info *dev_info,
                                            int *cali_re, int dev_num)
{
    int i = 0;
    int ret = 0;

    for (i = 0; i < dev_num; i++) {
        ret = aw_ar_cali_dev_get_re(dev_info, &cali_re[i], i);
        if (ret < 0)
            return ret;
    }

    return AW_OK;
}

static int aw_ar_cali_dev_get_f0(struct aw_dev_info *dev_info,
                                            int *cali_f0, int dev_index)
{
    int ret = 0;
    int i = 0;
    int f0 = 0;
    int sum = 0;

    for (i = 0; i < AW_CALI_READ_F0_Q_TIMES; i++) {
        ret = aw_ar_dsp_read_f0(dev_info->virt_mixer, dev_index, &f0);
        if (ret < 0) {
            AWLOGE("dev[%d]:get f0 failed!", dev_index);
            return ret;
        }
        sum += f0;
        usleep(AW_70000_US);
    }

    *cali_f0 = sum / AW_CALI_READ_F0_Q_TIMES;

    AWLOGD("dev[%d]cali f0 is %d", dev_index, *cali_f0);

    return AW_OK;
}

static int aw_ar_cali_devs_get_f0(struct aw_dev_info *dev_info,
                                            int *cali_f0, int dev_num)
{
    int i = 0;
    int ret = 0;

    for (i = 0; i < dev_num; i++) {
        ret = aw_ar_cali_dev_get_f0(dev_info, &cali_f0[i], i);
        if (ret < 0)
            return ret;
    }

    return AW_OK;
}


static int aw_ar_cali_dev_get_f0_q(struct aw_dev_info *dev_info,
                                int *cali_f0, int *cali_q, int dev_index)
{
    int ret = 0;
    int i = 0;
    int f0 = 0;
    int q = 0;
    int sum_f0 = 0;
    int sum_q = 0;

    for (i = 0; i < AW_CALI_READ_F0_Q_TIMES; i++) {
        ret = aw_ar_dsp_read_f0_q(dev_info->virt_mixer, dev_index, &f0, &q);
        if (ret) {
            AWLOGE("get f0 failed!");
            return ret;
        }
        sum_f0 += f0;
        sum_q += q;
        usleep(AW_70000_US);
    }
    *cali_f0 = sum_f0 / AW_CALI_READ_F0_Q_TIMES;
    *cali_q = sum_q / AW_CALI_READ_F0_Q_TIMES;

    AWLOGD("dev[%d]cali f0 is %d, q is %d", dev_index, *cali_f0, *cali_q);

    return AW_OK;
}

static int aw_ar_cali_devs_get_f0_q(struct aw_dev_info *dev_info,
                                int *cali_f0, int *cali_q, int dev_num)
{
    int i = 0;
    int ret = 0;

    for (i = 0; i < dev_num; i++) {
        ret = aw_ar_cali_dev_get_f0_q(dev_info, &cali_f0[i], &cali_q[i], i);
        if (ret < 0)
            return ret;
    }

    return AW_OK;
}


static int aw_ar_cali_dev_cali_mode_en(struct aw_dev_info *dev_info,
                        int dev_index, int type, bool is_enable, unsigned int flag)
{
    int ret;
    struct mixer *mixer = dev_info->virt_mixer;

    /* open cali mode */
    if (is_enable) {
        ret = aw_ar_kmsg_ctl_set_iv_output(dev_info->hw_mixer, AW882XX_CHIP_TYPE, dev_index, true);
        if (ret < 0)
            return ret;

        ret = aw_ar_cali_dev_set_cali_status(dev_info, dev_index, true);
        if (ret < 0)
            return ret;

        if (type == CALI_TYPE_RE) {
            if (flag & CALI_OPS_HMUTE) {
                ret = aw_ar_dsp_hmute_en(mixer, dev_index, true);
                if (ret < 0)
                    return ret;
            }

            ret = aw_ar_dsp_cali_en(mixer, dev_index, MSG_CALI_RE_ENABLE_DATA);
            if (ret < 0)
                return ret;
        } else {
            if (flag & CALI_OPS_NOISE) {
                ret = aw_ar_dsp_noise_en(mixer, dev_index, true);
                if (ret < 0)
                    return ret;
            }
            ret = aw_ar_dsp_cali_en(mixer, dev_index, MSG_CALI_F0_ENABLE_DATA);
            if (ret < 0)
                return ret;
        }
    } else {
        aw_ar_dsp_cali_en(mixer, dev_index, MSG_CALI_DISABLE_DATA);

        if (type == CALI_TYPE_RE) {
            /*close mute*/
            if (flag & CALI_OPS_HMUTE)
                aw_ar_dsp_hmute_en(mixer, dev_index, false);
        } else {
            /*close noise*/
            if (flag & CALI_OPS_NOISE)
                aw_ar_dsp_noise_en(mixer, dev_index, false);
        }

        /*close cali mode*/
        aw_ar_cali_dev_set_cali_status(dev_info, dev_index, false);
        aw_ar_kmsg_ctl_set_iv_output(dev_info->hw_mixer, AW882XX_CHIP_TYPE, dev_index, false);
    }

    return AW_OK;
}

static int aw_ar_cali_devs_cali_mode_en(struct aw_dev_info *dev_info,
                        int dev_num, int type, bool is_enable, unsigned int flag)
{
    int ret = 0;
    int i = 0;

    for (i = 0; i < dev_num; i++) {
        ret = aw_ar_cali_dev_cali_mode_en(dev_info, i, type, is_enable, flag);
        if (ret < 0)
            return ret;
    }

    return AW_OK;
}

static int aw_ar_cali_dev_cali_re(struct aw_dev_info *dev_info,
                            int *cali_re, int dev_index, unsigned int flag)
{
    int ret = 0;

    ret = aw_ar_cali_dev_cali_mode_en(dev_info,
                    dev_index, CALI_TYPE_RE, true, flag);
    if (ret < 0)
        goto exit;

    /*wait time*/
    usleep(g_cali_re_time_ms * 1000);


    /*dev get echo cali re*/
    ret = aw_ar_cali_dev_get_re(dev_info, cali_re, dev_index);

exit:
    aw_ar_cali_dev_cali_mode_en(dev_info,
                dev_index, CALI_TYPE_RE, false, flag);
    return ret;
}

static int aw_ar_cali_devs_cali_re(struct aw_dev_info *dev_info,
                            int *cali_re, int dev_num, unsigned int flag)
{
    int ret = 0;

    ret = aw_ar_cali_devs_cali_mode_en(dev_info,
                        dev_num, CALI_TYPE_RE, true, flag);
    if (ret < 0)
        goto exit;

    /*wait time*/
    usleep(g_cali_re_time_ms * 1000);

    ret = aw_ar_cali_devs_get_re(dev_info, cali_re, dev_num);

exit:
    aw_ar_cali_devs_cali_mode_en(dev_info,
                        dev_num, CALI_TYPE_RE, false, flag);
    return ret;
}

static int aw_ar_cali_dev_cali_f0(struct aw_dev_info *dev_info,
                            int *cali_f0, int dev_index, unsigned int flag)
{
    int ret = 0;

    ret = aw_ar_cali_dev_cali_mode_en(dev_info,
                    dev_index, CALI_TYPE_F0, true, flag);
    if (ret < 0)
        goto exit;

    /*wait time*/
    usleep(AW_CALI_F0_TIME);

    ret= aw_ar_cali_dev_get_f0(dev_info, cali_f0, dev_index);

exit:
    aw_ar_cali_dev_cali_mode_en(dev_info,
                        dev_index, CALI_TYPE_F0, false, flag);
    return ret;
}

static int aw_ar_cali_devs_cali_f0(struct aw_dev_info *dev_info,
                            int *cali_f0, int dev_num, unsigned int flag)
{
    int ret = 0;

    ret = aw_ar_cali_devs_cali_mode_en(dev_info,
                    dev_num, CALI_TYPE_F0, true, flag);
    if (ret < 0)
        goto exit;

    /*wait time*/
    usleep(AW_CALI_F0_TIME);

    ret= aw_ar_cali_devs_get_f0(dev_info, cali_f0, dev_num);

exit:
    aw_ar_cali_devs_cali_mode_en(dev_info,
                    dev_num, CALI_TYPE_F0, false, flag);
    return ret;
}

static int aw_ar_cali_dev_cali_f0_q(struct aw_dev_info *dev_info,
                int *cali_f0, int *cali_q,int dev_index, unsigned int flag)
{
    int ret = 0;

    ret = aw_ar_cali_dev_cali_mode_en(dev_info,
                    dev_index, CALI_TYPE_F0, true, flag);
    if (ret < 0)
        goto exit;

    /*wait time*/
    usleep(AW_CALI_F0_TIME);

    ret= aw_ar_cali_dev_get_f0_q(dev_info, cali_f0, cali_q, dev_index);

exit:
    aw_ar_cali_dev_cali_mode_en(dev_info,
                        dev_index, CALI_TYPE_F0, false, flag);
    return ret;
}



static int aw_ar_cali_devs_cali_f0_q(struct aw_dev_info *dev_info,
                            int *cali_f0, int *cali_q, int dev_num, unsigned int flag)
{
    int ret = 0;

    ret = aw_ar_cali_devs_cali_mode_en(dev_info,
                    dev_num, CALI_TYPE_F0, true, flag);
    if (ret < 0)
        goto exit;

    /*wait time*/
    usleep(AW_CALI_F0_TIME);

    ret= aw_ar_cali_devs_get_f0_q(dev_info, cali_f0, cali_q, dev_num);

exit:
    aw_ar_cali_devs_cali_mode_en(dev_info,
                    dev_num, CALI_TYPE_F0, false, flag);
    return ret;
}

static int aw_ar_cali_devs_set_re(struct aw_dev_info *dev_info,
                            int *cali_re, int dev_num)
{
    int ret = 0;
    int i = 0;
    struct aw_set_re_array re_array[AW_DEV_CH_MAX];


    for (i = 0; i < dev_num; i++) {
        re_array[i].channel = i;
        re_array[i].set_re = cali_re[i];
    }

    ret = aw_ar_dsp_write_cali_re_array(dev_info->virt_mixer,
                        dev_num, re_array);
    if (ret < 0) {
        AWLOGE("set re failed");
        return AW_FAIL;
    }

    return AW_OK;
}


static int aw_ar_cali_devs_set_offset(struct aw_dev_info *dev_info,int dev_num)
{
    int ret = 0;
    int index = 0;
    int offset = 0;

    /* add offset set to dsp, 0xffff do not need to set offset */
    for (index = 0; index < dev_num; index++) {
        ret = aw_ar_kmsg_ctl_get_voltage_offset(dev_info->hw_mixer, AW882XX_CHIP_TYPE,
                    index, &offset);

        if (ret < 0) {
            AWLOGE("get dev%d offset failed", index);
            return AW_FAIL;
        }
        AWLOGI("get dev%d offset %d", index, offset);

        if ((offset != 0xffff) && !ret) {
            aw_ar_dsp_write_voltage_offset(dev_info->virt_mixer, index, offset);
        } else {
            AWLOGI("ignore, Chip type do not need to set offset");
        }
    }

    return AW_OK;
}

static int aw_ar_cali_devs_set_auth(struct aw_dev_info *dev_info)
{
    int ret = 0;

	ret = aw_audioreach_dsp_algo_auth(dev_info);
	if (ret < 0) {
        AWLOGE("write algo auth failed");
        return AW_FAIL;
    }

    return AW_OK;
}

static int aw_ar_cali_dev_read_st(struct aw_dev_info *dev_info,
                                    int *r0, int32_t *te, int dev_index)
{
    int ret = 0;

    ret = aw_ar_dsp_read_st(dev_info->virt_mixer, dev_index, r0, te);
    if (ret < 0) {
        AWLOGE("dev[%d]:read st failed!", dev_index);
        return AW_FAIL;
    }

    return AW_OK;
}

static int aw_ar_cali_devs_read_st(struct aw_dev_info *dev_info,
                                    int *r0, int32_t *te, int dev_num)
{
    int ret = 0;
    int i = 0;

    for (i = 0; i < dev_num; i++) {
        ret = aw_ar_cali_dev_read_st(dev_info, &r0[i], &te[i], i);
        if (ret < 0) {
            return AW_FAIL;
        }
    }

    return AW_OK;
}

static int aw_ar_cali_dev_get_re_range(int *re_min, int *re_max, int dev_index)
{
    return aw_ar_kmsg_node_read_re_range(dev_index, re_min, re_max);
}

static int aw_ar_cali_devs_get_re_range(int *re_min, int *re_max, int dev_num)
{
    int i = 0;
    int ret = 0;

    for (i = 0; i < dev_num; i++) {
        ret = aw_ar_cali_dev_get_re_range(&re_min[i], &re_max[i], i);
        if (ret < 0)
            return ret;
    }

    return AW_OK;
}

#if 0
static int aw_ar_cali_file_init()
{
    int ret = 0;
    char blank_data[AW_RE_BUF_MAX] = { 0 };
    char re_buf[AW_RE_BUF_MAX] = { 0 };
    int i = 0;
    FILE *fd = NULL;
    int re_buf_len = 0;


    if (access(AWINIC_CALI_FILE, 0) == 0) {
        fd = fopen(AWINIC_CALI_FILE, "r");
        if (fd == NULL) {
            AWLOGE("can not open: %s", AWINIC_CALI_FILE);
            return AW_FAIL;
        }

        /*get file data*/
        re_buf_len = fread(re_buf, 1, AW_RE_BUF_MAX, fd);
        if (re_buf_len < 0) {
            AWLOGE("read file failed");
            fclose(fd);
            return AW_FAIL;
        }

        fclose(fd);
     }


    for (i = 0; i < AW_RE_BUF_MAX; i++) {
        blank_data[i] = ' ';
    }

    fd = fopen(AWINIC_CALI_FILE, "w+");
    if (fd == NULL) {
        AWLOGE("can not open: %s", AWINIC_CALI_FILE);
        return AW_FAIL;
    }

    /*fill the blank space to file*/
    ret = fwrite(blank_data, 1, AW_RE_BUF_MAX, fd);
    if (ret <= 0) {
        AWLOGE("write blank_data failed");
        fclose(fd);
        return AW_FAIL;
    }

    fseek(fd, 0, SEEK_SET);

    /*recover file data*/
    if (re_buf_len != 0) {
        ret = fwrite(re_buf, 1, re_buf_len, fd);
        if (ret <= 0) {
            AWLOGE("recover re_buf failed");
            fclose(fd);
            return AW_FAIL;
        }
    }

    fclose(fd);

    return AW_OK;
}

static int aw_ar_cali_dev_set_re_to_file(int *cali_re, int dev_index)
{
    FILE *fd = NULL;
    int ret = AW_FAIL;
    char re_buf[AW_RE_BUF_MAX] = { 0 };
    int len = 0;
    int offset = 0;

    ret = aw_ar_cali_file_init();
    if (ret < 0)
        return ret;

    //write re
    len += snprintf(re_buf + len, AW_RE_BUF_MAX -len, "%10d", *cali_re);
    AWLOGI("channel:[%d], set re:%d", dev_index, *cali_re);

    fd = fopen(AWINIC_CALI_FILE, "r+");
    if (fd == NULL) {
        AWLOGE("can not open: %s", AWINIC_CALI_FILE);
        return AW_FAIL;
    }

    offset = AW_INT_DEC_DIGIT * dev_index;

    fseek(fd, offset, SEEK_SET);

    ret = fwrite(re_buf, len, 1, fd);
    if (ret <= 0) {
        AWLOGE("set re to file failed");
        fclose(fd);
        return AW_FAIL;
    }

    fclose(fd);
    AWLOGD("set re to file success");

    return AW_OK;
}

static int aw_ar_cali_devs_set_re_to_file(int *cali_re, int dev_num)
{
    int ret = 0;
    int i = 0;

    for(i = 0; i < dev_num; i++) {
        ret = aw_ar_cali_dev_set_re_to_file(&cali_re[i], i);
        if (ret < 0) {
            AWLOGE("dev[%d]set re failed", i);
            return AW_FAIL;
        }
    }

    return AW_OK;
}
#endif

static int aw_ar_cali_dev_set_re_to_file(int *cali_re, int dev_index)
{
    int ret = AW_FAIL;
    char re_buf[AW_RE_BUF_MAX] = { 0 };
    int len = 0;
    int fd = 0;
    int offset = 0;

    //write re
    len += snprintf(re_buf + len, AW_RE_BUF_MAX -len, "%10d", *cali_re);
    AWLOGI("channel:[%d], set re:%d", dev_index, *cali_re);


    fd = open (AWINIC_CALI_FILE, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
        AWLOGE("can not open:%s", AWINIC_CALI_FILE);
        return AW_FAIL;
    }

    offset = AW_INT_DEC_DIGIT * dev_index;

    lseek(fd, offset, SEEK_SET);


    ret = write(fd, re_buf, len);
    if (ret <= 0) {
        AWLOGE("set re to file failed");
        close(fd);
        return AW_FAIL;
    }

    close(fd);

    return AW_OK;
}

static int aw_ar_cali_devs_set_re_to_file(int *cali_re, int dev_num)
{
    int ret = 0;
    int i = 0;

    for(i = 0; i < dev_num; i++) {
        ret = aw_ar_cali_dev_set_re_to_file(&cali_re[i], i);
        if (ret < 0) {
            AWLOGE("dev[%d]set re failed", i);
            return AW_FAIL;
        }
    }

    return AW_OK;
}

#if 0
static int aw_ar_cali_dev_get_re_from_file(int *cali_re, int dev_index)
{
    FILE *fd = NULL;
    int ret = AW_FAIL;
    char re_buf[AW_RE_BUF_MAX] = { 0 };
    int len = 0;

    fd = fopen(AWINIC_CALI_FILE, "r");
    if (fd == NULL) {
        AWLOGE("can not open: %s", AWINIC_CALI_FILE);
        return AW_FAIL;
    }

    ret = fread(re_buf, 1, AW_RE_BUF_MAX, fd);
    if (ret <= 0) {
        AWLOGE("get re from file failed");
        fclose(fd);
        return AW_FAIL;
    }

    fclose(fd);

    len = sscanf(re_buf + AW_INT_DEC_DIGIT * dev_index, "%10d", cali_re);
    if (len < 0) {
        AWLOGE("parse re failed,channel is %d", dev_index);
        return AW_FAIL;
    }

    AWLOGI("read re value is %d, channel is %d", *cali_re, dev_index);

    return AW_OK;
}

static int aw_ar_cali_devs_get_re_from_file(int *cali_re, int dev_num)
{
    int ret = 0;
    int i = 0;

    for ( i = 0; i < dev_num; i++) {
        ret = aw_ar_cali_dev_get_re_from_file(&cali_re[i], i);
        if (ret < 0) {
            AWLOGE("dev[%d]get re failed", i);
            return AW_FAIL;
        }
    }

    return AW_OK;
}
#endif
static int aw_ar_cali_dev_get_re_from_file(int *cali_re, int dev_index)
{
    int ret = AW_FAIL;
    char re_buf[AW_RE_BUF_MAX] = { 0 };
    int len = 0;
    int fd = 0;
    int offset = 0;

    fd = open(AWINIC_CALI_FILE, O_RDONLY);
    if (fd < 0) {
        AWLOGE("can not open: %s", AWINIC_CALI_FILE);
        return AW_FAIL;
    }

    offset = AW_INT_DEC_DIGIT * dev_index;

    lseek(fd, offset, SEEK_SET);

    ret = read(fd, re_buf, AW_RE_BUF_MAX);
    if (ret <= 0) {
        AWLOGE("get re from file failed");
        close(fd);
        return AW_FAIL;
    }

    close(fd);

    len = sscanf(re_buf, "%10d", cali_re);
    if (len < 0) {
        AWLOGE("parse re failed,channel is %d", dev_index);
        return AW_FAIL;
    }

    AWLOGI("read re value is %d, channel is %d", *cali_re, dev_index);

    return AW_OK;
}

static int aw_ar_cali_devs_get_re_from_file(int *cali_re, int dev_num)
{
    int ret = 0;
    int i = 0;

    for ( i = 0; i < dev_num; i++) {
        ret = aw_ar_cali_dev_get_re_from_file(&cali_re[i], i);
        if (ret < 0) {
            AWLOGE("dev[%d]get re failed", i);
            return AW_FAIL;
        }
    }

    return AW_OK;
}

int aw_audioreach_dsp_read_r0(struct aw_dev_info *dev_info, int *r0, int dev_num)
{
	int ret= 0;

	if ((dev_info == NULL) || (r0 == NULL)) {
		AWLOGE("pointer is NULL");
		return AW_FAIL;
	}

	if (dev_num > AW_DEV_CH_MAX)
		return AW_FAIL;

	if (g_is_single_cali) {
		ret = aw_ar_cali_dev_cali_re(dev_info, r0, dev_num, CALI_OPS_HMUTE);
	} else {
		ret = aw_ar_cali_devs_cali_re(dev_info, r0, dev_num, CALI_OPS_HMUTE);
	}

	return ret;
}

int aw_audioreach_dsp_read_f0(struct aw_dev_info *dev_info, int *f0, int dev_num)
{
    int ret= 0;

    if ((dev_info == NULL) || (f0 == NULL)) {
        AWLOGE("pointer is NULL");
        return AW_FAIL;
    }

    if (dev_num > AW_DEV_CH_MAX)
        return AW_FAIL;

    if (g_is_single_cali) {
        ret = aw_ar_cali_dev_cali_f0(dev_info, f0, dev_num, g_noise_flag);
    } else {
        ret = aw_ar_cali_devs_cali_f0(dev_info, f0, dev_num, g_noise_flag);
    }

    return ret;
}

int aw_audioreach_dsp_read_f0_q(struct aw_dev_info *dev_info, int *f0, int *q, int dev_num)
{
    int ret= 0;

    if ((dev_info == NULL) || (f0 == NULL)) {
        AWLOGE("pointer is NULL");
        return AW_FAIL;
    }

    if (dev_num > AW_DEV_CH_MAX)
        return AW_FAIL;

    if (g_is_single_cali) {
        ret = aw_ar_cali_dev_cali_f0_q(dev_info, f0, q, dev_num, g_noise_flag);
    } else {
        ret = aw_ar_cali_devs_cali_f0_q(dev_info, f0, q, dev_num, g_noise_flag);
    }

    return ret;
}

int aw_audioreach_dsp_set_re(struct aw_dev_info *dev_info, int *cali_re, int dev_num)
{
	int ret = 0;

	if ((dev_info == NULL) || (cali_re == NULL)) {
		AWLOGE("pointer is NULL");
		return AW_FAIL;
	}

	if (dev_num > AW_DEV_CH_MAX)
		return AW_FAIL;

    ret = aw_ar_cali_devs_set_re(dev_info, cali_re, dev_num);

	return ret;
}

int aw_audioreach_dsp_set_offset_and_auth(struct aw_dev_info *dev_info, int dev_num)
{
	int ret = 0;

	if (dev_info == NULL) {
		AWLOGE("pointer is NULL");
		return AW_FAIL;
	}

	if (dev_num > AW_DEV_CH_MAX)
		return AW_FAIL;

    ret = aw_ar_cali_devs_set_offset(dev_info, dev_num);
	if (ret < 0) {
		AWLOGE("set offset for devs failed");
		return AW_FAIL;
	}
    ret = aw_ar_cali_devs_set_auth(dev_info);
	if (ret < 0) {
		AWLOGE("set auth for devs failed");
		return AW_FAIL;
	}

	return ret;
}

int aw_audioreach_dsp_set_offset(struct aw_dev_info *dev_info, int dev_num)
{
	int ret = 0;

	if (dev_info == NULL) {
		AWLOGE("pointer is NULL");
		return AW_FAIL;
	}

	if (dev_num > AW_DEV_CH_MAX)
		return AW_FAIL;

    ret = aw_ar_cali_devs_set_offset(dev_info, dev_num);

	return ret;
}

int aw_audioreach_dsp_read_st(struct aw_dev_info *dev_info,
                        int *r0, int *te, int dev_num)
{
	int ret = 0;

	if ((dev_info == NULL) || (r0 == NULL) || (te == NULL)) {
		AWLOGE("pointer is NULL");
		return AW_FAIL;
	}

	if (dev_num > AW_DEV_CH_MAX)
		return AW_FAIL;

	if (g_is_single_cali) {
		ret = aw_ar_cali_dev_read_st(dev_info, r0, te, dev_num);
	} else {
		ret = aw_ar_cali_devs_read_st(dev_info, r0, te, dev_num);
	}

	return ret;
}

int aw_audioreach_dsp_get_re_range(int *re_min, int *re_max, int dev_num)
{
	int ret = 0;

	if ((re_min == NULL) || (re_max == NULL)) {
		AWLOGE("pointer is NULL");
		return AW_FAIL;
	}

	if (dev_num > AW_DEV_CH_MAX)
		return AW_FAIL;

	if (g_is_single_cali) {
		ret = aw_ar_cali_dev_get_re_range(re_min, re_max, dev_num);
	} else {
		ret = aw_ar_cali_devs_get_re_range(re_min, re_max, dev_num);
	}

	return ret;
}

int aw_audioreach_set_re_to_file(int *cali_re, int dev_num)
{
    int ret = 0;

    if (g_is_single_cali) {
        ret = aw_ar_cali_dev_set_re_to_file(cali_re, dev_num);
    } else {
        ret = aw_ar_cali_devs_set_re_to_file(cali_re, dev_num);
    }

    return ret;
}

int aw_audioreach_get_re_from_file(int *cali_re, int dev_num)
{
    int ret = 0;

    if (g_is_single_cali) {
        ret = aw_ar_cali_dev_get_re_from_file(cali_re, dev_num);
    } else {
        ret = aw_ar_cali_devs_get_re_from_file(cali_re, dev_num);
    }

    return ret;
}

int aw_audioreach_dsp_set_algo_en(struct aw_dev_info *dev_info, unsigned int is_enable)
{
    int ret = 0;

    ret = aw_ar_dsp_algo_en(dev_info->virt_mixer, is_enable);
    if (ret < 0) {
        AWLOGE("set rx %s failed", ((is_enable? "enable" : "disable")));
        return ret;
    }

    return AW_OK;
}

int aw_audioreach_dsp_algo_auth(struct aw_dev_info *dev_info)
{
	struct algo_auth_data auth_data;
	int ret = 0;

	memset(&auth_data, 0, sizeof(struct algo_auth_data));

	/*get data from dsp*/
	ret = aw_ar_dsp_read_algo_auth_data(dev_info->virt_mixer, (char *)&auth_data, sizeof(struct algo_auth_data));
	if (ret < 0) {
		return ret;
	}

	/*write data to PA*/
	ret = aw_ar_kmsg_ctl_set_auth_data(dev_info->hw_mixer, &auth_data);
	if (ret < 0) {
		return ret;
	}

	/*get verity data form PA*/
	ret = aw_ar_kmsg_ctl_get_auth_data(dev_info->hw_mixer, &auth_data);
	if (ret < 0) {
		return ret;
	}

	/*write data to dsp*/
	ret = aw_ar_dsp_write_algo_auth_data(dev_info->virt_mixer, (char *)&auth_data, sizeof(struct algo_auth_data));
	if (ret < 0) {
		return ret;
	}

	return ret;
}



