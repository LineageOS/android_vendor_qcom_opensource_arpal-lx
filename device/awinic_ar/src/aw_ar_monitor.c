/*Copyright (c) 2024 AWINIC Technology CO., LTD*/

#include "aw_ar_log.h"
#include "aw_ar_dev.h"
#include "aw_ar_monitor.h"
#include "aw_ar_dsp.h"
#include "aw_ar_api.h"
#include "aw_ar_kmsg.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VMAX_NONE   (0xFFFFFFFF)
#define POW_2_20 1048576

static struct aw_ar_monitor g_monitor;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

int getCoilTemperatures(float values[2])
{
    int ret = AW_FAIL;
    struct aw_dev_info dev_info = {0};
    int dev_num = 0;
    int te[2] = {0};
    ret = aw_ar_dsp_mix_init(&dev_info);
    if (ret != AW_OK) {
        AWLOGE("aw_ar_dsp_mix_init failed");
        return AW_FAIL;
    }
    ret = aw_ar_kmsg_ctl_get_dev_num(dev_info.hw_mixer,
                        0, &dev_num);
    if (ret < 0) {
        AWLOGE("get dev_num failed");
        return AW_FAIL;
    }
    for (int i = 0; i < dev_num; i++)
    {
        if(i == 2)
            break;
        ret = aw_ar_dsp_read_te(dev_info.virt_mixer, i, &te[i]);
            if (ret < 0) {
            AWLOGE("dev[%d]:get te failed!", i);
            return AW_FAIL;
        }
        values[i] = te[i] / 1.0f;
        AWLOGD("dev[%d]: coil temperature = %.1f℃", i, values[i]);
    }
    return AW_OK;
}

int setLimitV(int speaker, float limitV)
{
    int ret = AW_FAIL;
    struct aw_dev_info dev_info = {0};
    int32_t vmax = 0x0;
    ret = aw_ar_dsp_mix_init(&dev_info);
    if (ret != AW_OK) {
        AWLOGE("aw_ar_dsp_mix_init failed");
        return AW_FAIL;
    }
    vmax = (int32_t)(limitV * (float)POW_2_20);
    ret = aw_ar_dsp_write_vmax(dev_info.virt_mixer, speaker, vmax);
    if (ret < 0) {
        AWLOGE("set vmax to dsp failed");
        return AW_FAIL;
    }
    return AW_OK;
}
int setLimitX(int speaker, float limitX)
{
    int ret = AW_FAIL;
    struct aw_dev_info dev_info = {0};
    int32_t xmax = 0x0;
    ret = aw_ar_dsp_mix_init(&dev_info);
    if (ret != AW_OK) {
        AWLOGE("aw_ar_dsp_mix_init failed");
        return AW_FAIL;
    }
    xmax = (int32_t)(limitX * (float)POW_2_20);
    ret = aw_ar_dsp_write_xmax(dev_info.virt_mixer, speaker, xmax);
    if (ret < 0) {
        AWLOGE("set xmax to dsp failed");
        return AW_FAIL;
    }
    return AW_OK;
}

int setEQParams(int speaker, int slot, unsigned char *data)
{
    int ret = AW_FAIL;
    struct aw_dev_info dev_info = {0};
    ret = aw_ar_dsp_mix_init(&dev_info);
    if (ret != AW_OK) {
        AWLOGE("aw_ar_dsp_mix_init failed");
        return AW_FAIL;
    }
    ret = aw_ar_dsp_write_eq(dev_info.virt_mixer, speaker, slot);
    if (ret < 0) {
        AWLOGE("set setEQParams to dsp failed");
        return AW_FAIL;
    }
    return AW_OK;
}

static int aw_ar_monitor_work_func(struct aw_ar_monitor *monitor)
{
    int ret = AW_FAIL;
    uint32_t vmax = 0x0;
    int i = 0;
    uint32_t monitor_en = 0;

    for (i = 0; i < monitor->dev_num; i++) {
        ret = aw_ar_kmsg_ctl_get_monitor_en(monitor->hw_mixer,
                                monitor->chip_type, i, &monitor_en);
        if (ret < 0) {
            AWLOGE("get monitor enable control failed");
            return ret;
        }

        if (!monitor_en) {
            AWLOGD("dev[%d] monitor not enable", i);
            continue;
        }

        ret = aw_ar_kmsg_ctl_monitor_work(monitor->hw_mixer, monitor->chip_type, i, &vmax);
        if (ret < 0) {
            AWLOGE("get vmax failed");
            return AW_FAIL;
        }

        if ((vmax == monitor->pre_vmax[i]) || (vmax == VMAX_NONE)) {
            AWLOGD("dev[%d], vmax[0x%x] is not modified or the value is none",
                    i, vmax);
            continue;
        }

        ret = aw_ar_dsp_write_vmax(monitor->virt_mixer, i, vmax);
        if (ret < 0) {
            AWLOGE("set vmax to dsp failed");
            return AW_FAIL;
        }

        monitor->pre_vmax[i] = vmax;
        AWLOGI("set vmax to dsp success");
    }

    return AW_OK;
}

static void *aw_ar_monitor_thread(void *args)
{
    struct aw_ar_monitor *monitor = (struct aw_ar_monitor *)args;

    while (1) {
        pthread_mutex_lock(&g_mutex);

        if ((monitor->state != AW_MONITER_RUN) &&
            (monitor->state != AW_MONITER_DESTROY)) {
            /* wait start */
            monitor->state = AW_MONITER_WAIT;
            pthread_cond_wait(&g_cond, &g_mutex);
        }

        if (monitor->state == AW_MONITER_DESTROY) {
            pthread_mutex_unlock(&g_mutex);
            break;
        }

        aw_ar_monitor_work_func(monitor);
        pthread_mutex_unlock(&g_mutex);

         /*usleep*/
        usleep(monitor->time_ms * 1000);
    }

    return NULL;
}

void aw_audioreach_monitor_stop()
{
    struct aw_ar_monitor *monitor = &g_monitor;

    AWLOGI("enter");

    pthread_mutex_lock(&g_mutex);
    if ((monitor->state != AW_MONITER_INIT) ||
        (monitor->state != AW_MONITER_DESTROY)) {
        monitor->state = AW_MONITER_STOP;
    }
    pthread_mutex_unlock(&g_mutex);
}


void aw_audioreach_monitor_start()
{
    struct aw_ar_monitor *monitor = &g_monitor;
    int ret = 0;

    AWLOGI("enter");

    pthread_mutex_lock(&g_mutex);
    if ((monitor->state != AW_MONITER_INIT) ||
        (monitor->state != AW_MONITER_DESTROY)) {
        if (monitor->time_ms == 0) {
            AWLOGI("unsupport monitor time[%d]", monitor->time_ms);
            pthread_mutex_unlock(&g_mutex);
            return;
        }

        memset(monitor->pre_vmax, 0, sizeof(monitor->pre_vmax));
        monitor->state = AW_MONITER_RUN;
        pthread_cond_signal(&g_cond);
    }
    pthread_mutex_unlock(&g_mutex);
}

int aw_audioreach_monitor_init(struct aw_dev_info *dev_info)
{
    int ret = 0;
    struct aw_ar_monitor *monitor = &g_monitor;

    AWLOGI("enter, version:%s", AW_AR_VERSION);

    pthread_mutex_lock(&g_mutex);
    if (monitor->state == AW_MONITER_INIT) {
        monitor->hw_mixer = dev_info->hw_mixer;
        monitor->virt_mixer = dev_info->virt_mixer;

        ret = aw_ar_kmsg_ctl_get_chip_type(monitor->hw_mixer,
                                        &monitor->chip_type);
        if (ret < 0) {
            AWLOGE("get chip_type failed");
            goto error;
        }

        ret = aw_ar_kmsg_ctl_get_dev_num(monitor->hw_mixer,
                        monitor->chip_type, &monitor->dev_num);
        if (ret < 0) {
            AWLOGE("get dev_num failed");
            goto error;
        }

        ret = aw_ar_kmsg_ctl_get_monitor_time(monitor->hw_mixer,
                        monitor->chip_type, &monitor->time_ms);
        if (ret < 0) {
            AWLOGE("get monitor time failed");
            goto error;
        }

        /*Create a thread*/
        ret = pthread_create(&monitor->thread_id, NULL,
                            aw_ar_monitor_thread, monitor);
        if (ret < 0) {
            AWLOGE("pthread create failed");
            goto error;
        }

        monitor->state = AW_MONITER_CREATE;
    }

    pthread_mutex_unlock(&g_mutex);
    return AW_OK;

error:
    pthread_mutex_unlock(&g_mutex);
    return ret;
}

void aw_audioreach_monitor_deinit()
{
    struct aw_ar_monitor *monitor = &g_monitor;

    AWLOGI("enter");

    pthread_mutex_lock(&g_mutex);
    monitor->state = AW_MONITER_DESTROY;
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_mutex);

    pthread_join(monitor->thread_id, NULL);

    memset(&g_monitor, 0, sizeof(g_monitor));
}


