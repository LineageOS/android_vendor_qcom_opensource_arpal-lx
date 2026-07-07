/*Copyright (c) 2024 AWINIC Technology CO., LTD*/

#include "aw_ar_log.h"
#include "aw_ar_dev.h"
#include "aw_ar_dsp.h"
#include "aw_ar_api.h"

#include <tinyalsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <iostream>
#include <memory>
#include <string>


#ifdef AW_BACK_END_NAME
#define MODULE_SP            (0xC0000029)
extern "C" {
#include <snd-card-def.h>
}
#else
#include "ResourceManager.h"
#endif


#define AW_DSP_RE_TO_SHOW_RE(re)    ((int32_t)(((int64_t)(re) * (1000)) >> (12)))
#define AW_SHOW_RE_TO_DSP_RE(re)    ((int32_t)(((int64_t)(re) << 12) / (1000)))


#define AW_ALGO_EN               (1)
#define AW_ALGO_ID         		(0x10013d01)
#define AW_ALGO_VERSION_MAX     (80)
#define PCM_ID_MIN              (100)
#define PCM_ID_MAX              (200)
#define AW_PCM_NAME_LIST_MAX    (100)

#define AW_DSP_MSG_HDR_VER (1)

enum {
    AW_DSP_MSG_TYPE_DATA = 0,
    AW_DSP_MSG_TYPE_CMD = 1,
};


#define AW_MSG_ID_ENABLE_CALI       (0x00000001)
#define AW_MSG_ID_ENABLE_HMUTE      (0x00000002)
#define AW_MSG_ID_F0_Q              (0x00000003)
#define AW_MSG_ID_REAL_DATA         (0x00000005)
#define AW_MSG_ID_DIRECT_CUR_FLAG   (0x00000006)
#define AW_MSG_ID_SPK_STATUS        (0x00000007)
#define AW_MSG_ID_VERSION           (0x00000008)
#define AW_MSG_ID_AUDIO_MIX         (0x0000000B)
#define AW_MSG_ID_VERSION_NEW       (0x00000012)
#define AW_MSG_ID_SWITCH_SCENE      (0x00000013)
#define AW_MSG_ID_VOLTAGE_OFFSET    (0x0000001F)


/*dsp params id*/
#define AW_MSG_ID_RX_SET_ENABLE     (0x10013D11)
#define AW_MSG_ID_PARAMS            (0x10013D12)
#define AW_MSG_ID_TX_SET_ENABLE     (0x10013D13)
#define AW_MSG_ID_VMAX_L            (0X10013D17)
#define AW_MSG_ID_VMAX_R            (0X10013D18)
#define AW_MSG_ID_CALI_CFG_L        (0X10013D19)
#define AW_MSG_ID_CALI_CFG_R        (0x10013d1A)
#define AW_MSG_ID_RE_L              (0x10013d1B)
#define AW_MSG_ID_RE_R              (0X10013D1C)
#define AW_MSG_ID_NOISE_L           (0X10013D1D)
#define AW_MSG_ID_NOISE_R           (0X10013D1E)
#define AW_MSG_ID_F0_L              (0X10013D1F)
#define AW_MSG_ID_F0_R              (0X10013D20)
#define AW_MSG_ID_REAL_DATA_L       (0X10013D21)
#define AW_MSG_ID_REAL_DATA_R       (0X10013D22)
#define AW_MSG_ID_SPIN              (0x10013D2E)
#define AW_MSG_ID_RE_ARRAY          (0x10013D32)
#define AW_MSG_ID_XMAX_L            (0x10013D67)
#define AW_MSG_ID_XMAX_R            (0x10013D68)
#define AW_MSG_ID_TEEQ_SLOT_L       (0x10013D69)
#define AW_MSG_ID_TEEQ_SLOT_R       (0x10013D6A)

#define AFE_MSG_ID_MSG_0             (0X10013D2A)
#define AFE_MSG_ID_MSG_1             (0X10013D2B)
#define AFE_MSG_ID_MSG_2             (0X10013D36)
#define AFE_MSG_ID_MSG_3             (0X10013D33)
#define AW_MSG_ID_PARAMS_DEFAULT     (0x10013D37)
#define AW_MSG_ID_ALGO_AUTH          (0x10013D46)
#define AW_MSG_ID_ALGO_ID            (0x10013D53)
#define AW_MSG_ID_VOLUME_L			 (0x10013D54)
#define AW_MSG_ID_RAMP_EN_L			 (0x10013D56)
#define AW_MSG_ID_RAMP_PARAMS_L		 (0x10013D62)
#define AW_MSG_ID_RUN_STATE_AVG		 (0x10013d58)


enum {
    MSG_PARAM_ID_0 = 0,
    MSG_PARAM_ID_1,
    MSG_PARAM_ID_2,
    MSG_PARAM_ID_3,
    MSG_PARAM_ID_MAX,
};

static uint32_t afe_param_msg_id[MSG_PARAM_ID_MAX] = {
    AFE_MSG_ID_MSG_0,
    AFE_MSG_ID_MSG_1,
    AFE_MSG_ID_MSG_2,
    AFE_MSG_ID_MSG_3,
};

static pthread_mutex_t g_aw_dsp_msg_lock;
static uint32_t g_lock_status = false;
static int g_priv_pcm_indx = 0;


static int aw_ar_dsp_platform_init(struct aw_ar_info *ar_info, bool get_ver_en);

#ifdef AW_BACK_END_NAME
#else
int aw_ar_dsp_mix_init(struct aw_dev_info *dev_info)
{
    int ret = 0;
    std::shared_ptr<ResourceManager> rm = NULL;
    struct aw_dev_info ar_dev_info;

    rm = ResourceManager::getInstance();

    ret = rm->getVirtualAudioMixer(&ar_dev_info.virt_mixer);
    if (ret) {
        AWLOGE("Awinic virt mixer error %d", ret);
        return 1;
    }

    ret = rm->getHwAudioMixer(&ar_dev_info.hw_mixer);
    if (ret) {
        AWLOGE("Awinic hw mixer error %d", ret);
        return 1;
    }

    memcpy(dev_info, &ar_dev_info, sizeof(struct aw_dev_info));

    return 0;
}
#endif
static int aw_ar_dsp_mutex_init()
{
    int ret = 0;

    if (!g_lock_status) {
        ret = pthread_mutex_init(&g_aw_dsp_msg_lock, NULL);
        if (ret) {
            AWLOGE("mutex init failed");
            return ret;
        }
        g_lock_status = true;
    }

    return AW_OK;
}

static int aw_ar_dsp_get_msg_num(int dev_ch, int *msg_num)
{
    switch (dev_ch) {
    case AW_DEV_CH_PRI_L:
        *msg_num = MSG_PARAM_ID_0;
        break;
    case AW_DEV_CH_PRI_R:
        *msg_num = MSG_PARAM_ID_0;
        break;
    case AW_DEV_CH_SEC_L:
        *msg_num = MSG_PARAM_ID_1;
        break;
    case AW_DEV_CH_SEC_R:
        *msg_num = MSG_PARAM_ID_1;
        break;
   case AW_DEV_CH_TERT_L:
        *msg_num = MSG_PARAM_ID_2;
        break;
   case AW_DEV_CH_TERT_R:
        *msg_num = MSG_PARAM_ID_2;
        break;
   case AW_DEV_CH_QUAT_L:
        *msg_num = MSG_PARAM_ID_3;
        break;
   case AW_DEV_CH_QUAT_R:
        *msg_num = MSG_PARAM_ID_3;
        break;
    default:
        AWLOGE("can not find msg num, channel %d ", dev_ch);
        return AW_FAIL;
    }

    AWLOGD("msg num[%d]", *msg_num);
    return AW_OK;
}

static int aw_ar_dsp_get_ctl_by_pcm_info(struct mixer *mixer,
        char *pcm_dev_name, const char *ctl_cmd, struct mixer_ctl **ctl)
{
    char *mixer_name = NULL;
    uint32_t ctl_len = 0;

    if ((mixer == NULL) || (pcm_dev_name == NULL) ||
        (ctl_cmd == NULL) || (ctl == NULL)) {
        AWLOGE("pointer is nulll");
        return AW_FAIL;
    }

    ctl_len = strlen(pcm_dev_name) + strlen(ctl_cmd) + 2;/*control_str: "pcm_dev_name ctl_cmd"*/

    mixer_name = (char *)calloc(1, ctl_len);
    if (mixer_name == NULL) {
        AWLOGE("calloc failed");
        return AW_FAIL;
    }

    snprintf(mixer_name, ctl_len, "%s %s", pcm_dev_name, ctl_cmd);
    AWLOGD("mixer_name:%s", mixer_name);

    *ctl = mixer_get_ctl_by_name(mixer, mixer_name);
    if (*ctl == NULL) {
        AWLOGE("get ctl failed");
        free(mixer_name);
        mixer_name = NULL;
        return AW_FAIL;
    }

    free(mixer_name);
    mixer_name = NULL;

    return AW_OK;
}

static int aw_ar_dsp_set_dsp_info(struct aw_ar_info *ar_info,
            uint32_t param_id, char *data_ptr, unsigned int data_size)
{
    int ret;
    uint8_t *payload = NULL;
    uint32_t payload_size = 0, pad_bytes = 0;
    aw_apm_module_param_data_t *header = NULL;
    struct mixer_ctl *ctl = NULL;

    if ((ar_info == NULL) || (data_ptr == NULL) || (data_size == 0)) {
        AWLOGE("pointer is nulll");
        return AW_FAIL;
    }

    ret = aw_ar_dsp_get_ctl_by_pcm_info(ar_info->mixer,
                ar_info->pcm_device_name,
                AW_CTL_SET_PARAM, &ctl);
    if (ret < 0)
        return ret;

    payload_size = sizeof(aw_apm_module_param_data_t) + AW_ALIGN_4BYTE(data_size);
    pad_bytes = AW_PADDING_ALIGN_8BYTE(payload_size);

    payload = (uint8_t *)calloc(1, payload_size + pad_bytes);
    if (payload == NULL) {
        AWLOGE("calloc failed");
        return AW_FAIL;
    }

    header = (struct aw_apm_module_param_data_t *)payload;
    header->module_instance_id = ar_info->miid;
    header->param_id = param_id;
    header->param_size = payload_size - sizeof(aw_apm_module_param_data_t);
    header->error_code = 0;
    memcpy(payload + sizeof(aw_apm_module_param_data_t), data_ptr, data_size);

    ret = mixer_ctl_set_array(ctl, payload, payload_size + pad_bytes);
    if (ret < 0) {
        //AWLOGE("mixer_ctl_set_array failed");
        free(payload);
        payload = NULL;
        return AW_FAIL;
    }

    free(payload);
    payload = NULL;
    return AW_OK;
}

static int aw_ar_dsp_get_dsp_info(struct aw_ar_info *ar_info,
            uint32_t param_id, char *data_ptr, unsigned int data_size)
{
    int ret;
    uint8_t *payload = NULL;
    uint32_t payload_size = 0;
    aw_apm_module_param_data_t *header = NULL;
    struct mixer_ctl *ctl = NULL;

    if ((ar_info == NULL) || (data_ptr == NULL) || (data_size == 0)) {
        AWLOGE("pointer is nulll or data_size is 0");
        return AW_FAIL;
    }

    ret = aw_ar_dsp_get_ctl_by_pcm_info(ar_info->mixer,
                 ar_info->pcm_device_name,
                 AW_CTL_GET_PARAM, &ctl);
    if (ret < 0)
        return ret;

    payload_size = AW_ALIGN_8BYTE(data_size + sizeof(aw_apm_module_param_data_t));
    payload = (uint8_t *)calloc(1, payload_size);
    if (payload == NULL) {
        AWLOGE("calloc failed");
        return AW_FAIL;
    }

    header = (struct aw_apm_module_param_data_t *)payload;
    header->module_instance_id = ar_info->miid;
    header->param_id = param_id;
    header->param_size = payload_size - sizeof(aw_apm_module_param_data_t);
    header->error_code = 0;

    ret = mixer_ctl_set_array(ctl, payload, payload_size);
    if (ret < 0) {
        AWLOGE("mixer_ctl_set_array failed");
        free(payload);
        payload = NULL;
        return AW_FAIL;
    }

    ret = mixer_ctl_get_array(ctl, payload, payload_size);
    if (ret < 0) {
        AWLOGE("mixer_ctl_set_array failed");
        free(payload);
        payload = NULL;
        return AW_FAIL;
    }

    memcpy(data_ptr, payload + sizeof(aw_apm_module_param_data_t), data_size);
    free(payload);
    payload = NULL;
    return AW_OK;
}

static int aw_ar_dsp_write_dev_data_to_dsp(struct mixer *virt_mixer,
            uint32_t param_id, char *data_ptr, unsigned int data_size)
{
    int ret = 0;
    uint8_t *payload = NULL;
    uint32_t payload_size = 0, pad_bytes = 0;
    aw_apm_module_param_data_t *header = NULL;
    struct mixer_ctl *ctl = NULL;
    struct aw_ar_info ar_info;

    if ((virt_mixer == NULL) || (data_ptr == NULL) || (data_size == 0)) {
        AWLOGE("pointer is nulll");
        return AW_FAIL;
    }
    memset(&ar_info, 0, sizeof(struct aw_ar_info));

    ar_info.mixer = virt_mixer;
    ret = aw_ar_dsp_platform_init(&ar_info, false);
    if (ret < 0) {
        AWLOGE("aw_ar_platform_init failed");
        return ret;
    }

    ret = aw_ar_dsp_get_ctl_by_pcm_info(ar_info.mixer,
                ar_info.p_intf_name,
                AW_CTL_SET_PARAM, &ctl);
    if (ret < 0)
        return ret;

    payload_size = sizeof(aw_apm_module_param_data_t) + AW_ALIGN_4BYTE(data_size);
    pad_bytes = AW_PADDING_ALIGN_8BYTE(payload_size);

    payload = (uint8_t *)calloc(1, payload_size + pad_bytes);
    if (payload == NULL) {
        AWLOGE("calloc failed");
        return AW_FAIL;
    }

    header = (struct aw_apm_module_param_data_t *)payload;
    header->module_instance_id = ar_info.miid;
    header->param_id = param_id;
    header->param_size = payload_size - sizeof(aw_apm_module_param_data_t);
    header->error_code = 0;
    memcpy(payload + sizeof(aw_apm_module_param_data_t), data_ptr, data_size);

    ret = aw_ar_dsp_mutex_init();
    if (ret)
        return ret;

    pthread_mutex_lock(&g_aw_dsp_msg_lock);
    ret = mixer_ctl_set_array(ctl, payload, payload_size + pad_bytes);
    if (ret < 0) {
        AWLOGE("mixer_ctl_set_array failed");
        pthread_mutex_unlock(&g_aw_dsp_msg_lock);
        free(payload);
        payload = NULL;
        return AW_FAIL;
    }

    pthread_mutex_unlock(&g_aw_dsp_msg_lock);
    free(payload);
    payload = NULL;
    return AW_OK;
}

static int aw_ar_dsp_write_data_to_dsp(struct mixer *virt_mixer,
            uint32_t param_id, char *data_ptr, unsigned int size)
{
    int ret = 0;
    struct aw_ar_info ar_info;

    if ((virt_mixer == NULL) || (data_ptr == NULL) || (size == 0)) {
        AWLOGE("pointer is nulll or size is 0");
        return AW_FAIL;
    }

    memset(&ar_info, 0, sizeof(struct aw_ar_info));

    ar_info.mixer = virt_mixer;
    ret = aw_ar_dsp_platform_init(&ar_info, true);
    if (ret < 0) {
        AWLOGE("aw_ar_platform_init failed");
        return ret;
    }

    ret = aw_ar_dsp_mutex_init();
    if (ret)
        return ret;

    pthread_mutex_lock(&g_aw_dsp_msg_lock);
    ret = aw_ar_dsp_set_dsp_info(&ar_info, param_id, data_ptr, size);
    pthread_mutex_unlock(&g_aw_dsp_msg_lock);

    return ret;
}

static int aw_ar_dsp_read_data_from_dsp(struct mixer *virt_mixer,
				uint32_t param_id, char *data_ptr, unsigned int size)
{
    int ret = 0;
    struct aw_ar_info ar_info;

    if ((virt_mixer == NULL) || (data_ptr == NULL) || (size == 0)) {
        AWLOGE("pointer is nulll or size is 0");
        return AW_FAIL;
    }

    memset(&ar_info, 0, sizeof(struct aw_ar_info));

    ar_info.mixer = virt_mixer;
    ret = aw_ar_dsp_platform_init(&ar_info, true);
    if (ret < 0) {
        AWLOGE("aw_ar_platform_init failed");
        return ret;
    }

    ret = aw_ar_dsp_mutex_init();
    if (ret)
        return ret;

    pthread_mutex_lock(&g_aw_dsp_msg_lock);
    ret = aw_ar_dsp_get_dsp_info(&ar_info, param_id, data_ptr, size);
    pthread_mutex_unlock(&g_aw_dsp_msg_lock);

    return ret;
}

static int aw_ar_dsp_write_msg_to_dsp(struct mixer *virt_mixer, uint32_t msg_num,
                uint32_t msg_id, char *data_ptr, unsigned int size)
{
    uint32_t *dsp_msg = NULL;
    int ret = 0;
    int msg_len = (int)(sizeof(aw_msg_hdr_t) + size);
    struct aw_ar_info ar_info;

    if ((virt_mixer == NULL) || (data_ptr == NULL) || (size == 0)) {
        AWLOGE("pointer is nulll or size is 0");
        return AW_FAIL;
    }

    memset(&ar_info, 0, sizeof(struct aw_ar_info));

    ar_info.mixer = virt_mixer;

    ret = aw_ar_dsp_platform_init(&ar_info, true);
    if (ret < 0) {
        AWLOGE("aw_ar_platform_init failed");
        return ret;
    }

    ret = aw_ar_dsp_mutex_init();
    if (ret)
        return ret;

    pthread_mutex_lock(&g_aw_dsp_msg_lock);
    dsp_msg = (uint32_t *) calloc(1, msg_len);
    if (!dsp_msg) {
        AWLOGE("msg_id:0x%x calloc dsp_msg error", msg_id);
        ret = AW_FAIL;
        goto w_mem_err;
    }

    dsp_msg[0] = AW_DSP_MSG_TYPE_DATA;
    dsp_msg[1] = msg_id;
    dsp_msg[2] = AW_DSP_MSG_HDR_VER;

    memcpy(dsp_msg + (sizeof(aw_msg_hdr_t) / sizeof(uint32_t)),
            data_ptr, size);

    ret = aw_ar_dsp_set_dsp_info(&ar_info, afe_param_msg_id[msg_num],
        (char *)(dsp_msg), msg_len);
    if (ret < 0) {
        AWLOGE("msg_id:0x%x, write data to dsp failed", msg_id);
        free(dsp_msg);
        dsp_msg = NULL;
        goto w_mem_err;
    }
    pthread_mutex_unlock(&g_aw_dsp_msg_lock);

    AWLOGD("msg_id:0x%x, write data[%d] to dsp success", msg_id, msg_len);
    free(dsp_msg);
    dsp_msg = NULL;

    return AW_OK;

w_mem_err:
    pthread_mutex_unlock(&g_aw_dsp_msg_lock);
    return ret;
}

static int aw_ar_dsp_read_msg_from_dsp(struct mixer *virt_mixer, uint32_t msg_num,
                uint32_t msg_id, char *data_ptr, unsigned int size)
{
    aw_msg_hdr_t cmd_msg;
    int ret;
    struct aw_ar_info ar_info;

    if ((virt_mixer == NULL) || (data_ptr == NULL) || (size == 0)) {
        AWLOGE("pointer is nulll or size is 0");
        return AW_FAIL;
    }

    memset(&ar_info, 0, sizeof(struct aw_ar_info));

    ar_info.mixer = virt_mixer;

    ret = aw_ar_dsp_platform_init(&ar_info, true);
    if (ret < 0) {
        AWLOGE("aw_ar_platform_init failed");
        return ret;
    }

    ret = aw_ar_dsp_mutex_init();
    if (ret)
        return ret;

    pthread_mutex_lock(&g_aw_dsp_msg_lock);

    cmd_msg.type = AW_DSP_MSG_TYPE_CMD;
    cmd_msg.opcode_id = msg_id;
    cmd_msg.version = AW_DSP_MSG_HDR_VER;

    ret = aw_ar_dsp_set_dsp_info(&ar_info, afe_param_msg_id[msg_num],
        (char *)(&cmd_msg), sizeof(aw_msg_hdr_t));
    if (ret < 0) {
        AWLOGE("msg_id:0x%x, write cmd to dsp failed", msg_id);
        goto dsp_msg_failed;
    }

    ret = aw_ar_dsp_get_dsp_info(&ar_info, afe_param_msg_id[msg_num],
            data_ptr,  (int)(size));
    if (ret < 0) {
        AWLOGE("msg_id:0x%x, read data from dsp failed", msg_id);
        goto dsp_msg_failed;
    }

    AWLOGD("msg_id:0x%x, read data[%d] from dsp success", msg_id, size);
    pthread_mutex_unlock(&g_aw_dsp_msg_lock);
    return AW_OK;

dsp_msg_failed:
    pthread_mutex_unlock(&g_aw_dsp_msg_lock);
    return ret;
}

static int aw_ar_dsp_read_tranfer_test(struct aw_ar_info *ar_info)
{
    int ret = AW_FAIL;
    int algo_test =  0;

    if (ar_info == NULL) {
        AWLOGE("pointer is null");
        return AW_FAIL;
     }

    ret = aw_ar_dsp_mutex_init();
    if (ret)
        return ret;

    pthread_mutex_lock(&g_aw_dsp_msg_lock);

    ret = aw_ar_dsp_get_dsp_info(ar_info, AW_MSG_ID_ALGO_ID, (char *)&algo_test, sizeof(int));
    if (ret < 0) {
        ret = aw_ar_dsp_get_dsp_info(ar_info, AW_MSG_ID_RX_SET_ENABLE, (char *)&algo_test, sizeof(int));
        if (ret < 0) {
            ret= AW_FAIL;
            AWLOGD("msg_id:0x%x, read enable data from dsp failed", AW_MSG_ID_RX_SET_ENABLE);
        } else {
            if(algo_test == AW_ALGO_EN) {
                ret = AW_OK;
                AWLOGD("msg_id:0x%x, read enable data from dsp done", AW_MSG_ID_RX_SET_ENABLE);
            } else {
                ret= AW_FAIL;
                AWLOGD("msg_id:0x%x, read enable data from dsp mismatch", AW_MSG_ID_RX_SET_ENABLE);
            }
        }
    } else {
        if(algo_test == AW_ALGO_ID) {
            ret = AW_OK;
            AWLOGD("msg_id:0x%x, read algo_id data from dsp done", AW_MSG_ID_ALGO_ID);
        } else {
            ret= AW_FAIL;
            AWLOGD("msg_id:0x%x, read algo_id data from dsp mismatch", AW_MSG_ID_ALGO_ID);
        }
	}

    pthread_mutex_unlock(&g_aw_dsp_msg_lock);
    return ret;

}

static int aw_ar_dsp_set_stream_metadata_type(
                    struct aw_ar_info *ar_info, const char *val)
{
    struct mixer_ctl *ctl = NULL;
    int ret;

    ret = aw_ar_dsp_get_ctl_by_pcm_info(ar_info->mixer,
                ar_info->pcm_device_name,
                AW_CTL_CONTROL, &ctl);
    if (ret < 0)
        return ret;

    ret = mixer_ctl_set_enum_by_string(ctl, val);
    if (ret < 0) {
        //AWLOGE("set data:%s failed", val);
        return AW_FAIL;
    }

    return AW_OK;
}

static int aw_ar_dsp_get_miid(struct aw_ar_info *ar_info, uint32_t tag_id, const char *backend_name)
{
    int ret = 0;
    struct mixer_ctl *ctl = NULL;
    char payload[AW_DATA_BUF_MAX] = { 0 };
    struct aw_gsl_tag_module_info *tag_info = NULL;
    struct aw_gsl_tag_module_info_entry *tag_entry = NULL;
    unsigned int i = 0;
    int offset = 0;
    struct aw_gsl_module_id_info_entry *mod_info_entry = NULL;

    ret = aw_ar_dsp_set_stream_metadata_type(ar_info, backend_name);
    if (ret < 0) {
        //AWLOGE("set pcm_device_name:%s failed", backend_name);
        return AW_FAIL;
    }

    ret = aw_ar_dsp_get_ctl_by_pcm_info(ar_info->mixer,
                ar_info->pcm_device_name,
                AW_CTL_GET_TAGGED_INFO, &ctl);
    if (ret < 0)
        return ret;

    ret = mixer_ctl_get_array(ctl, &payload, AW_DATA_BUF_MAX);
    if (ret < 0) {
        AWLOGE("Failed to mixer_ctl_get_array");
        return AW_FAIL;
    }

    tag_info = (struct aw_gsl_tag_module_info *)payload;
    ret = AW_FAIL;
    tag_entry = (struct aw_gsl_tag_module_info_entry *)(&tag_info->tag_module_entry[0]);
    offset = 0;
    for (i = 0; i < tag_info->num_tags; i++) {
        tag_entry += offset / sizeof(struct aw_gsl_tag_module_info_entry);

        offset = sizeof(struct aw_gsl_tag_module_info_entry) +
                    (tag_entry->num_modules * sizeof(struct aw_gsl_module_id_info_entry));
        if (tag_entry->tag_id == tag_id) {
            if (tag_entry->num_modules) {
                mod_info_entry = &tag_entry->module_entry[0];
                ar_info->miid = mod_info_entry->module_iid;
                ret = AW_OK;
                break;
            }
        }
    }

    return ret;
}

static void aw_dsp_get_check_sum(int pos, void *data, int size, int *check_sum)
{
	int i = 0;
	int sum_data = 0;

	for (i = pos; i < size / sizeof(uint8_t); i++)
		sum_data += *((uint8_t *)data + sizeof(uint8_t) * i);

	*check_sum = sum_data;
}

static int aw_write_msg_to_dsp_v_1_0_0_0(struct mixer *virt_mixer,
		uint32_t index, uint32_t params_id, void *data, unsigned int len, int num)
{
	int ret = 0;
	int32_t *dsp_msg = NULL;
	aw_msg_hdr_v1_t *hdr = NULL;

	dsp_msg = (int32_t *)calloc(1, sizeof(aw_msg_hdr_v1_t) + len);
	if (!dsp_msg) {
		AWLOGE("calloc dsp_msg error");
		ret = -ENOMEM;
		goto kalloc_error;
	}
	hdr = (aw_msg_hdr_v1_t *)dsp_msg;
	hdr->version = AW_DSP_MSG_VER;
	hdr->type = DSP_MSG_TYPE_WRITE_DATA;
	hdr->params_id = params_id;
	hdr->channel = index;
	hdr->num = num;
	hdr->data_size = len / num;

	memcpy(((char *)dsp_msg) + sizeof(aw_msg_hdr_v1_t), data, len);

	aw_dsp_get_check_sum(sizeof(int32_t), dsp_msg, sizeof(aw_msg_hdr_v1_t) + len, &hdr->checksum);

	ret = aw_ar_dsp_write_data_to_dsp(virt_mixer,
				AW_MSG_ID_PARAMS_DEFAULT, (char *)dsp_msg, sizeof(aw_msg_hdr_v1_t) + len);
	if (ret < 0) {
		AWLOGE("write data failed");
		goto write_msg_error;
	}

write_msg_error:
	free(dsp_msg);
	dsp_msg = NULL;
kalloc_error:
	return ret;
}

static int aw_read_msg_from_dsp_v_1_0_0_0(struct mixer *virt_mixer,
		uint32_t index, uint32_t params_id, char *data_ptr, unsigned int len, int num)
{
	int ret = 0;
	int sum_data = 0;
	int32_t *dsp_msg = NULL;
	aw_msg_hdr_v1_t write_hdr;
	aw_msg_hdr_v1_t *read_hdr = NULL;

	memset(&write_hdr, 0, sizeof(aw_msg_hdr_v1_t));
	write_hdr.version = AW_DSP_MSG_VER;
	write_hdr.type = DSP_MSG_TYPE_WRITE_CMD;
	write_hdr.params_id = params_id;
	write_hdr.channel = index;
	write_hdr.num = num;
	write_hdr.data_size = len / num;

	aw_dsp_get_check_sum(sizeof(int32_t), &write_hdr, sizeof(aw_msg_hdr_v1_t), &write_hdr.checksum);

	ret = aw_ar_dsp_write_data_to_dsp(virt_mixer,
				AW_MSG_ID_PARAMS_DEFAULT, (char *)&write_hdr, sizeof(aw_msg_hdr_v1_t));

	if (ret < 0) {
		AWLOGE("write data to dsp failed");
		goto write_hdr_error;
	}

	dsp_msg = (int32_t *)calloc(1, sizeof(aw_msg_hdr_v1_t) + len);
	if (!dsp_msg) {
		ret = -ENOMEM;
		goto kalloc_msg_error;
	}

    ret = aw_ar_dsp_read_data_from_dsp(virt_mixer,
                    AW_MSG_ID_PARAMS_DEFAULT, (char *)dsp_msg, sizeof(aw_msg_hdr_v1_t) + len);
	if (ret < 0) {
		AWLOGE("read data from dsp failed");
		goto read_msg_error;
	}

	read_hdr = (aw_msg_hdr_v1_t *)dsp_msg;
	if (read_hdr->type != DSP_MSG_TYPE_READ_DATA) {
		AWLOGE("read_hdr type = %d not read data!", read_hdr->type);
		ret = -EINVAL;
		goto read_msg_error;
	}

	aw_dsp_get_check_sum(sizeof(int32_t), dsp_msg, sizeof(aw_msg_hdr_v1_t) + len, &sum_data);

	if (sum_data != read_hdr->checksum) {
		AWLOGE("aw_dsp_msg check sum error!");
		AWLOGE("read_hdr->checksum=%d sum_data=%d", read_hdr->checksum, sum_data);
		ret = -EINVAL;
		goto read_msg_error;
	}

	memcpy(data_ptr, ((char *)dsp_msg) + sizeof(aw_msg_hdr_v1_t), len);

read_msg_error:
	free(dsp_msg);
	dsp_msg = NULL;
write_hdr_error:
kalloc_msg_error:
	return ret;
}

static int aw_ar_dsp_write_msg(struct mixer *virt_mixer, uint32_t msg_num,
                uint32_t msg_id, char *data_ptr, unsigned int size, int dev_index)
{
	int ret = 0;

	if (dev_index < AW_DEV_CH_TERT_L) {
		ret = aw_ar_dsp_write_msg_to_dsp(virt_mixer, msg_num, msg_id, data_ptr, size);
	} else {
        ret = aw_write_msg_to_dsp_v_1_0_0_0(virt_mixer, dev_index, msg_id,
                    data_ptr, size, AW_DSP_CHANNEL_DEFAULT_NUM);
	}

	return ret;
}

static int aw_ar_dsp_read_msg(struct mixer *virt_mixer, uint32_t msg_num,
                uint32_t msg_id, char *data_ptr, unsigned int size, int dev_index)
{
 	int ret = 0;

	if (dev_index < AW_DEV_CH_TERT_L) {
		ret = aw_ar_dsp_read_msg_from_dsp(virt_mixer, msg_num, msg_id, data_ptr, size);
	} else {
        ret = aw_read_msg_from_dsp_v_1_0_0_0(virt_mixer, dev_index, msg_id,
                    data_ptr, size, AW_DSP_CHANNEL_DEFAULT_NUM);
	}

    return ret;
}

#ifdef AW_BACK_END_NAME
static int aw_ar_dsp_get_pcm_list_from_card_defs(char *buf, unsigned int buf_size)
{
    void *card = NULL;
    void **nodes = NULL;
    char *name = NULL;
    unsigned int offset = 0;
    int node_types[2] = { SND_NODE_TYPE_PCM, SND_NODE_TYPE_COMPR };
    int num = 0, val = 0, i = 0, t = 0;
    int ret = AW_FAIL;

    card = snd_card_def_get_card(AW_VIRT_CARD);
    if (card == NULL) {
        AWLOGE("no card-defs entry for card %d", AW_VIRT_CARD);
        return AW_FAIL;
    }

    for (t = 0; t < 2; t++) {
        num = snd_card_def_get_num_node(card, node_types[t]);
        if (num <= 0)
            continue;

        nodes = (void **)calloc(num, sizeof(void *));
        if (nodes == NULL)
            goto done;

        if (snd_card_def_get_nodes_for_type(card, node_types[t], nodes, num)) {
            free(nodes);
            nodes = NULL;
            continue;
        }

        for (i = 0; i < num; i++) {
            val = 0;
            if (snd_card_def_get_int(nodes[i], "playback", &val) || (val == 0))
                continue;
            if (snd_card_def_get_str(nodes[i], "name", &name) || (name == NULL))
                continue;
            if (offset + strlen(name) + 2 > buf_size)
                break;
            offset += snprintf(buf + offset, buf_size - offset, "%s%s",
                               (offset != 0) ? "," : "", name);
        }
        free(nodes);
        nodes = NULL;
    }

    if (offset != 0) {
        AWLOGI("playback pcm list from card-defs: %s", buf);
        ret = AW_OK;
    }

done:
    snd_card_def_put_card(card);
    return ret;
}

static int aw_ar_dsp_platform_init(struct aw_ar_info *ar_info, bool tranfer_test)
{
    int ret = 0;
    char *pcm_name_list[AW_PCM_NAME_LIST_MAX] = { 0 };
    char pcm_name_buf[4096] = { 0 };
    char *pcm_name_buf_p = pcm_name_buf;
    int i = 0;
    int pcm_num = 0;

    memset(ar_info->p_intf_name, 0, sizeof(ar_info->p_intf_name));
    /* AW_BACK_END_NAME define on Android.mk */
    if(back_end_name_flag != 0){
        strncpy(ar_info->p_intf_name, aw_back_end_name, AW_NAME_BUF_MAX);
        AWLOGD("AW_BACK_END_NAME:%s",aw_back_end_name);
    }else{
        strncpy(ar_info->p_intf_name, AW_BACK_END_NAME, AW_NAME_BUF_MAX);
        AWLOGD("AW_BACK_END_NAME:%s",AW_BACK_END_NAME);
    }

    if (aw_ar_dsp_get_pcm_list_from_card_defs(pcm_name_buf,
                                              sizeof(pcm_name_buf)) != AW_OK) {
        AWLOGE("no pcm name list available");
        return AW_FAIL;
    }


    while((pcm_name_list[pcm_num] = strtok(pcm_name_buf_p, ",")) != NULL) {
        pcm_num++;
        pcm_name_buf_p = NULL;
    }

	/*priv-pcm_idx first test*/
	if (g_priv_pcm_indx != 0) {
		memset(ar_info->pcm_device_name, 0, AW_NAME_BUF_MAX);
		strncpy(ar_info->pcm_device_name,
			pcm_name_list[g_priv_pcm_indx], strlen(pcm_name_list[g_priv_pcm_indx]));

		ret = aw_ar_dsp_get_miid(ar_info, MODULE_SP, ar_info->p_intf_name);
		if (ret < 0) {
			g_priv_pcm_indx = 0;
			goto loop_pcm;
	   }

	   if (tranfer_test) {
		   ret = aw_ar_dsp_read_tranfer_test(ar_info);
		   if (ret != AW_OK) {
			   g_priv_pcm_indx = 0;
			   goto loop_pcm;
		   }
	   }
	   AWLOGD("pcm_id[%d], pcm device name[%s] miid[%u]", g_priv_pcm_indx,
					ar_info->pcm_device_name, ar_info->miid);
	   return ret;
	}

loop_pcm:
    for (i = 0; i < pcm_num; i++) {
        memset(ar_info->pcm_device_name, 0, AW_NAME_BUF_MAX);
        strncpy(ar_info->pcm_device_name, pcm_name_list[i], strlen(pcm_name_list[i]));

        ret = aw_ar_dsp_get_miid(ar_info, MODULE_SP, ar_info->p_intf_name);
        if (ret < 0) {
            continue;
        }

        /*The value is set to false only when set re*/
        if (tranfer_test) {
            ret = aw_ar_dsp_read_tranfer_test(ar_info);
            if (ret != AW_OK) {
                continue;
            }

            if (ret == AW_OK) {
				g_priv_pcm_indx = i;
                break;
            }
        } else {
            break;
        }
    }

    return ret;
}

#else
static int aw_ar_dsp_platform_init(struct aw_ar_info *ar_info, bool tranfer_test)
{
    int ret = AW_OK;
    int i = 0;
    std::string backEndName;
    std::shared_ptr<ResourceManager> rm = NULL;
    int device_id = PAL_DEVICE_OUT_SPEAKER;
    char *pcm_device_name = NULL;

    if (ar_info->mixer == NULL) {
        AWLOGE("mixer is NULL");
        return AW_FAIL;
    }

    rm = ResourceManager::getInstance();

    ret = rm->getBackendName(device_id, backEndName);
    if (ret < 0) {
        AWLOGE("get BackendName error");
        goto exit;
    }

    memset(ar_info->p_intf_name, 0, sizeof(ar_info->p_intf_name));
    strcpy(ar_info->p_intf_name, backEndName.c_str());

	/*priv-pcm_idx first test*/
	if (g_priv_pcm_indx != 0) {
		pcm_device_name = rm->getDeviceNameFromID(g_priv_pcm_indx);
		if (!pcm_device_name) {
			g_priv_pcm_indx = 0;
			goto loop_pcm;
		}

		memset(ar_info->pcm_device_name, 0, AW_NAME_BUF_MAX);
		strncpy(ar_info->pcm_device_name, pcm_device_name, strlen(pcm_device_name));

		ret = aw_ar_dsp_get_miid(ar_info, MODULE_SP, ar_info->p_intf_name);
		if (ret < 0) {
			g_priv_pcm_indx = 0;
			goto loop_pcm;
		}

		if (tranfer_test) {
			ret = aw_ar_dsp_read_tranfer_test(ar_info);
			if (ret != AW_OK) {
				g_priv_pcm_indx = 0;
				goto loop_pcm;
			}
		}
		AWLOGD("pcm_id[%d], pcm device name[%s] miid[%u]", g_priv_pcm_indx,
					ar_info->pcm_device_name, ar_info->miid);
		return ret;
	}

loop_pcm:
    /*get pcm id*/
    for (i = PCM_ID_MIN; i < PCM_ID_MAX; i++) {
        pcm_device_name = rm->getDeviceNameFromID(i);
        if (!pcm_device_name) {
            continue;
        }

        memset(ar_info->pcm_device_name, 0, AW_NAME_BUF_MAX);
        strncpy(ar_info->pcm_device_name, pcm_device_name, strlen(pcm_device_name));

        ret = aw_ar_dsp_get_miid(ar_info, MODULE_SP, backEndName.c_str());
        if (ret < 0) {
            continue;
        }

        /*The value is set to false only when set re*/
        if (tranfer_test) {
            ret = aw_ar_dsp_read_tranfer_test(ar_info);
            if (ret != AW_OK) {
                continue;
            }

            if (ret == AW_OK) {
				g_priv_pcm_indx = i;
                break;
            }
        } else {
            break;
        }
    }

    if (i == PCM_ID_MAX) {
        AWLOGE("get pcm id failed");
        ret = AW_FAIL;
    } else {
        AWLOGD("pcm_id[%d], pcm device name[%s] miid[%u]", i,
                    ar_info->pcm_device_name, ar_info->miid);
    }

exit:
    return ret;
}
#endif

int aw_ar_dsp_write_voltage_offset(struct mixer *virt_mixer,
                                    int dev_index, int offset)
{
    int32_t ret = 0;

    ret = aw_write_msg_to_dsp_v_1_0_0_0(virt_mixer, dev_index, AW_MSG_ID_VOLTAGE_OFFSET,
                (char *)(&offset), sizeof(offset), AW_DSP_CHANNEL_DEFAULT_NUM);
    if (ret) {
        AWLOGE("write offset failed");
        return ret;
    }

    AWLOGD("dev[%d]write offset:0x%x", dev_index, offset);
    return AW_OK;
}

int aw_ar_dsp_write_vmax(struct mixer *virt_mixer,
                                    int dev_index, uint32_t vmax)

{
    int32_t ret = 0;
    int msg_num = 0;
    uint32_t msg_id = 0;

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret)
        return ret;

    if (dev_index == AW_DEV_CH_PRI_L ||
            dev_index == AW_DEV_CH_SEC_L ||
            dev_index == AW_DEV_CH_TERT_L ||
            dev_index == AW_DEV_CH_QUAT_L ) {
        msg_id = AW_MSG_ID_VMAX_L;
    } else if (dev_index == AW_DEV_CH_PRI_R ||
            dev_index == AW_DEV_CH_SEC_R ||
            dev_index == AW_DEV_CH_TERT_R ||
            dev_index == AW_DEV_CH_QUAT_R ) {
        msg_id = AW_MSG_ID_VMAX_R;
    } else {
        AWLOGE("unsupport dev channel");
        return AW_FAIL;
    }

    ret = aw_ar_dsp_write_msg(virt_mixer, msg_num,
            msg_id, (char *)(&vmax), sizeof(uint32_t), dev_index);
    if (ret) {
        AWLOGE("write vmax failed");
        return ret;
    }

    AWLOGD("dev[%d]write vmax:0x%x", dev_index, vmax);
    return AW_OK;
}

int aw_ar_dsp_write_xmax(struct mixer *virt_mixer,
                                    int dev_index, uint32_t xmax)

{
    int32_t ret = 0;
    int msg_num = 0;
    uint32_t msg_id = 0;

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret)
        return ret;

    if (dev_index == AW_DEV_CH_PRI_L ||
            dev_index == AW_DEV_CH_SEC_L ||
            dev_index == AW_DEV_CH_TERT_L ||
            dev_index == AW_DEV_CH_QUAT_L ) {
        msg_id = AW_MSG_ID_XMAX_L;
    } else if (dev_index == AW_DEV_CH_PRI_R ||
            dev_index == AW_DEV_CH_SEC_R ||
            dev_index == AW_DEV_CH_TERT_R ||
            dev_index == AW_DEV_CH_QUAT_R ) {
        msg_id = AW_MSG_ID_XMAX_R;
    } else {
        AWLOGE("unsupport dev channel");
        return AW_FAIL;
    }

    ret = aw_ar_dsp_write_msg(virt_mixer, msg_num,
            msg_id, (char *)(&xmax), sizeof(uint32_t), dev_index);
    if (ret) {
        AWLOGE("write xmax failed");
        return ret;
    }

    AWLOGD("dev[%d]write xmax:0x%x", dev_index, xmax);
    return AW_OK;
}

int aw_ar_dsp_write_eq(struct mixer *virt_mixer,
                                    int dev_index, uint32_t slot)

{
    int32_t ret = 0;
    int msg_num = 0;
    uint32_t msg_id = 0;

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret)
        return ret;

    if (dev_index == AW_DEV_CH_PRI_L ||
            dev_index == AW_DEV_CH_SEC_L ||
            dev_index == AW_DEV_CH_TERT_L ||
            dev_index == AW_DEV_CH_QUAT_L ) {
        msg_id = AW_MSG_ID_TEEQ_SLOT_L;
    } else if (dev_index == AW_DEV_CH_PRI_R ||
            dev_index == AW_DEV_CH_SEC_R ||
            dev_index == AW_DEV_CH_TERT_R ||
            dev_index == AW_DEV_CH_QUAT_R ) {
        msg_id = AW_MSG_ID_TEEQ_SLOT_R;
    } else {
        AWLOGE("unsupport dev channel");
        return AW_FAIL;
    }

    ret = aw_ar_dsp_write_msg(virt_mixer, msg_num,
            msg_id, (char *)(&slot), sizeof(uint32_t), dev_index);
    if (ret) {
        AWLOGE("write eq slot failed");
        return ret;
    }

    AWLOGD("dev[%d]write eq slot:0x%x", dev_index, slot);
    return AW_OK;
}


int aw_ar_dsp_read_vmax(struct mixer *virt_mixer,
                                int dev_index, uint32_t *vmax)
{
    int32_t ret = 0;
    int msg_num = 0;
    uint32_t msg_id = 0;

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret)
        return ret;

    if (dev_index == AW_DEV_CH_PRI_L ||
            dev_index == AW_DEV_CH_SEC_L ||
            dev_index == AW_DEV_CH_TERT_L ||
            dev_index == AW_DEV_CH_QUAT_L ) {
        msg_id = AW_MSG_ID_VMAX_L;
    } else if (dev_index == AW_DEV_CH_PRI_R ||
            dev_index == AW_DEV_CH_SEC_R ||
            dev_index == AW_DEV_CH_TERT_R ||
            dev_index == AW_DEV_CH_QUAT_R ) {
        msg_id = AW_MSG_ID_VMAX_R;
    } else {
        AWLOGE("unsupport dev channel");
        return AW_FAIL;
    }

    ret = aw_ar_dsp_read_msg(virt_mixer, msg_num,
                    msg_id, (char *)vmax, sizeof(uint32_t), dev_index);
    if (ret) {
        AWLOGE("read vmax failed");
        return ret;
    }
    AWLOGD("dev[%d]read vmax:0x%x", dev_index, *vmax);

    return AW_OK;
}


int aw_ar_dsp_read_te(struct mixer *virt_mixer,int dev_index,
                                int32_t *te)
{
    int ret = 0;
    int msg_num = 0;
    int32_t data[8] = { 0 }; /*[re:r0:Te:r0_te]*/

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret)
        return ret;

    ret = aw_ar_dsp_read_msg(virt_mixer, msg_num,
                    AW_MSG_ID_SPK_STATUS, (char *)data,
                    sizeof(int32_t) * 8, dev_index);
    if (ret) {
        AWLOGE("read Te failed");
        return ret;
    }

    if ((dev_index % 2) == 0)
        *te = data[2];
    else
        *te = data[6];

    AWLOGD("dev[%d]read Te %d", dev_index, *te);
    return AW_OK;
}

int aw_ar_dsp_read_st(struct mixer *virt_mixer,int dev_index,
                            int32_t *r0, int32_t *te)
{
    int ret = 0;
    int msg_num = 0;
    int32_t data[8] = { 0 }; /*[re:r0:Te:r0_te]*/

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret)
        return ret;

    ret = aw_ar_dsp_read_msg(virt_mixer, msg_num,
                    AW_MSG_ID_SPK_STATUS, (char *)data,
                    sizeof(int32_t) * 8, dev_index);
    if (ret) {
        AWLOGE("read Te failed");
        return ret;
    }

    if ((dev_index % 2) == 0) {
        *r0 = AW_DSP_RE_TO_SHOW_RE(data[0]);
        *te = data[2];
    } else {
        *r0 = AW_DSP_RE_TO_SHOW_RE(data[4]);
        *te = data[6];
    }

    AWLOGD("dev[%d]read Re %d , Te %d", dev_index, *r0, *te);
    return AW_OK;
}

int aw_ar_dsp_read_r0(struct mixer *virt_mixer,
                            int dev_index, int32_t *r0)
{
    uint32_t msg_id = 0;
    int ret = 0;
    int msg_num = 0;
    int32_t data[6] = { 0 };

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret < 0) {
        return ret;
    }

    if (dev_index == AW_DEV_CH_PRI_L ||
            dev_index == AW_DEV_CH_SEC_L ||
            dev_index == AW_DEV_CH_TERT_L ||
            dev_index == AW_DEV_CH_QUAT_L) {
        msg_id = AW_MSG_ID_REAL_DATA_L;
    } else if (dev_index == AW_DEV_CH_PRI_R ||
                dev_index == AW_DEV_CH_SEC_R ||
                dev_index == AW_DEV_CH_TERT_R ||
                dev_index == AW_DEV_CH_QUAT_R) {
        msg_id = AW_MSG_ID_REAL_DATA_R;
    } else {
        AWLOGE("unsupport dev channel");
        return AW_FAIL;
    }

    ret = aw_ar_dsp_read_msg(virt_mixer, msg_num,
                    msg_id, (char *)data, sizeof(int32_t) * 6, dev_index);
    if (ret) {
        AWLOGE("read real re failed");
        return ret;
    }

    *r0 = AW_DSP_RE_TO_SHOW_RE(data[0]);
    AWLOGD("dev[%d]read r0 %d", dev_index, *r0);

    return AW_OK;
}

int aw_ar_dsp_read_f0_q(struct mixer *virt_mixer,
                            int dev_index, int32_t *f0, int32_t *q)
{
    int ret = 0;
    int msg_num = 0;
    int32_t data[4] = { 0 };

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret < 0) {
        return ret;
    }

    ret = aw_ar_dsp_read_msg(virt_mixer, msg_num,
                    AW_MSG_ID_F0_Q, (char *)data, sizeof(int32_t) * 4, dev_index);
    if (ret) {
        AWLOGE("read f0 & q failed");
        return ret;
    }

    if ((dev_index % 2) == 0) {
        *f0 = data[0];
        *q  = data[1];
    } else {
        *f0 = data[2];
        *q  = data[3];
    }
    AWLOGD("dev[%d]read f0 is %d, q is %d", dev_index, *f0, *q);

    return AW_OK;
}

int aw_ar_dsp_read_f0(struct mixer *virt_mixer,
                                int dev_index, int32_t *f0)
{
    int ret = 0;
    uint32_t msg_id = 0;
    int msg_num = 0;

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret < 0) {
        return ret;
    }

    if (dev_index == AW_DEV_CH_PRI_L ||
            dev_index == AW_DEV_CH_SEC_L ||
            dev_index == AW_DEV_CH_TERT_L ||
            dev_index == AW_DEV_CH_QUAT_L) {
        msg_id = AW_MSG_ID_F0_L;
    } else if (dev_index == AW_DEV_CH_PRI_R ||
                dev_index == AW_DEV_CH_SEC_R ||
                dev_index == AW_DEV_CH_TERT_R ||
                dev_index == AW_DEV_CH_QUAT_R) {
        msg_id = AW_MSG_ID_F0_R;
    } else {
        AWLOGE("unsupport dev channel");
        return AW_FAIL;
    }

    ret = aw_ar_dsp_read_msg(virt_mixer, msg_num,
                        msg_id, (char *)f0, sizeof(int32_t), dev_index);
    if (ret) {
        AWLOGE("read f0 failed");
        return ret;
    }

    AWLOGD("dev[%d]read f0 is %d", dev_index, *f0);

    return AW_OK;
}

int aw_ar_dsp_cali_en(struct mixer *virt_mixer,
                            int dev_index, int32_t cali_en_mode)
{
    int ret = 0;
    int msg_num = 0;

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret < 0) {
        return ret;
    }

    ret = aw_ar_dsp_write_msg(virt_mixer, msg_num,
                    AW_MSG_ID_ENABLE_CALI, (char *)&cali_en_mode,
                    sizeof(int32_t), dev_index);
    if (ret) {
        AWLOGE("write cali en failed");
        return ret;
    }

    AWLOGD("dev[%d]write cali_en[%d]", dev_index, cali_en_mode);

    return AW_OK;
}

int aw_ar_dsp_hmute_en(struct mixer *virt_mixer,
                        int dev_index, bool is_hmute)
{
    int ret = 0;
    int msg_num = 0;
    int32_t hmute = is_hmute;

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret < 0) {
        return ret;
    }

    ret = aw_ar_dsp_write_msg(virt_mixer, msg_num,
                AW_MSG_ID_ENABLE_HMUTE, (char *)&hmute, sizeof(int32_t), dev_index);
    if (ret) {
        AWLOGE("write hmue failed ");
        return ret;
    }

    AWLOGD("dev[%d]write hmute[%d]", dev_index, is_hmute);

    return AW_OK;
}

int aw_ar_dsp_read_cali_re(struct mixer *virt_mixer,
                        int dev_index, int32_t *cali_re)
{
    int ret = 0;
    uint32_t msg_id = 0;
    int msg_num = 0;
    int32_t read_re = 0;

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret < 0) {
        return ret;
    }

    if (dev_index == AW_DEV_CH_PRI_L ||
            dev_index == AW_DEV_CH_SEC_L ||
            dev_index == AW_DEV_CH_TERT_L ||
            dev_index == AW_DEV_CH_QUAT_L) {
        msg_id = AW_MSG_ID_RE_L;
    } else if (dev_index == AW_DEV_CH_PRI_R ||
                dev_index == AW_DEV_CH_SEC_R ||
                dev_index == AW_DEV_CH_TERT_R ||
                dev_index == AW_DEV_CH_QUAT_R) {
        msg_id = AW_MSG_ID_RE_R;
    } else {
        AWLOGE("unsupport dev channel");
        return AW_FAIL;
    }

    ret = aw_ar_dsp_read_msg(virt_mixer, msg_num,
                            msg_id, (char *)&read_re, sizeof(int32_t), dev_index);
    if (ret) {
        AWLOGE("read cali re failed ");
        return ret;
    }

    *cali_re = AW_DSP_RE_TO_SHOW_RE(read_re);
    AWLOGD("dev[%d]read cali re:%d", dev_index, *cali_re);


    return AW_OK;
}

int aw_ar_dsp_write_cali_re(struct mixer *virt_mixer,
                        int dev_index, int32_t cali_re)
{
    int ret = 0;
    uint32_t msg_id = 0;
    int msg_num = 0;
    int32_t local_re = AW_SHOW_RE_TO_DSP_RE(cali_re);

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret < 0) {
        return ret;
    }

    if (dev_index == AW_DEV_CH_PRI_L ||
            dev_index == AW_DEV_CH_SEC_L ||
            dev_index == AW_DEV_CH_TERT_L ||
            dev_index == AW_DEV_CH_QUAT_L) {
        msg_id = AW_MSG_ID_RE_L;
    } else if (dev_index == AW_DEV_CH_PRI_R ||
                dev_index == AW_DEV_CH_SEC_R ||
                dev_index == AW_DEV_CH_TERT_R ||
                dev_index == AW_DEV_CH_QUAT_R) {
        msg_id = AW_MSG_ID_RE_R;
    } else {
        AWLOGE("unsupport dev channel");
        return AW_FAIL;
    }

    ret = aw_ar_dsp_write_msg(virt_mixer, msg_num,
                        msg_id, (char *)&local_re, sizeof(int32_t), dev_index);
    if (ret) {
        AWLOGE("write cali re failed");
        return ret;
    }

    AWLOGD("write cali re done");

    return AW_OK;
}

int aw_ar_dsp_write_cali_re_array(struct mixer *virt_mixer,
                        int dev_num, struct aw_set_re_array *re_array)
{
    int ret = 0;
    int i = 0;

    if (dev_num > AW_DEV_CH_MAX) {
        AWLOGE("unsupport dev_num:%d", dev_num);
        return AW_FAIL;
    }

    for (i = 0; i < dev_num; i++) {
        AWLOGD("dev[%d]set_re:%d", re_array[i].channel, re_array[i].set_re);
        re_array[i].set_re = AW_SHOW_RE_TO_DSP_RE( re_array[i].set_re);
    }

    ret = aw_ar_dsp_write_dev_data_to_dsp(virt_mixer,
            AW_MSG_ID_RE_ARRAY, (char *)re_array,
            dev_num * sizeof(struct aw_set_re_array));
    if (ret) {
        AWLOGE("set re failed");
        return ret;
    }

    return AW_OK;
}


int aw_ar_dsp_noise_en(struct mixer *virt_mixer,
                        int dev_index, bool is_noise)
{
    int ret = 0;
    int32_t noise = is_noise;
    uint32_t msg_id = 0;
    int msg_num = 0;

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret < 0) {
        return ret;
    }

    if (dev_index == AW_DEV_CH_PRI_L ||
            dev_index == AW_DEV_CH_SEC_L ||
            dev_index == AW_DEV_CH_TERT_L ||
            dev_index == AW_DEV_CH_QUAT_L) {
        msg_id = AW_MSG_ID_NOISE_L;
    } else if (dev_index == AW_DEV_CH_PRI_R ||
                dev_index == AW_DEV_CH_SEC_R ||
                dev_index == AW_DEV_CH_TERT_R ||
                dev_index == AW_DEV_CH_QUAT_R) {
        msg_id = AW_MSG_ID_NOISE_R;
    } else {
        AWLOGE("unsupport dev channel");
        return AW_FAIL;
    }

    ret = aw_ar_dsp_write_msg(virt_mixer, msg_num,
                msg_id, (char *)&noise, sizeof(int32_t), dev_index);
    if (ret) {
        AWLOGE("write noise failed ");
        return ret;
    }

    AWLOGD("dev[%d]write noise[%d]", dev_index, is_noise);
    return AW_OK;
}

int aw_ar_dsp_read_spin(struct mixer *virt_mixer, int *spin_mode)
{
    int ret;
    int32_t spin = 0;

    ret = aw_ar_dsp_read_data_from_dsp(virt_mixer,
                    AW_MSG_ID_SPIN, (char *)spin_mode, sizeof(int32_t));
    if (ret) {
        *spin_mode = 0;
        AWLOGE("read spin failed ");
        return ret;
    }

    return AW_OK;
}

int aw_ar_dsp_write_spin(struct mixer *virt_mixer, int spin_mode)
{
    int ret = 0;

    if (spin_mode >= AW_SPIN_MAX) {
        AWLOGE("spin [%d] unsupported", spin_mode);
        return AW_FAIL;
    }

    ret = aw_ar_dsp_write_data_to_dsp(virt_mixer,
                    AW_MSG_ID_SPIN, (char *)&spin_mode, sizeof(int32_t));
    if (ret) {
        AWLOGE("write spin failed");
        return ret;
    }

    AWLOGD("write spin done");

    return AW_OK;
}

int aw_ar_dsp_set_mixer_en(struct mixer *virt_mixer, int dev_index,
                                uint32_t mixer_en)
{
    int ret = 0;
    int msg_num = 0;

    ret = aw_ar_dsp_get_msg_num(dev_index, &msg_num);
    if (ret < 0) {
        AWLOGE("get msg_num failed");
        return ret;
    }

    ret = aw_ar_dsp_write_msg(virt_mixer, msg_num,
                    AW_MSG_ID_AUDIO_MIX, (char *)&mixer_en, sizeof(uint32_t), dev_index);
    if (ret) {
        AWLOGE("write mixer_en failed");
        return ret;
    }

    AWLOGD("dev[%d]write mixer_en[%d]", dev_index, mixer_en);
    return AW_OK;
}

int aw_ar_dsp_algo_en(struct mixer *virt_mixer, uint32_t is_enable)
{
    int ret = 0;

    ret = aw_ar_dsp_write_data_to_dsp(virt_mixer, AW_MSG_ID_RX_SET_ENABLE,
                    (char *)&is_enable, sizeof(is_enable));
    if (ret < 0) {
        AWLOGE("set rx %s failed", ((is_enable? "enable" : "disable")));
        return ret;
    }

    AWLOGD("set rx %s", ((is_enable? "enable" : "disable")));
    return AW_OK;
}


int aw_ar_dsp_read_algo_auth_data(struct mixer *virt_mixer,
		char *data, unsigned int data_len)
{
	int ret = 0;

	ret = aw_ar_dsp_read_data_from_dsp(virt_mixer, AW_MSG_ID_ALGO_AUTH, data, data_len);
	if (ret < 0) {
		AWLOGE("read algo auth failed");
		return ret;
	}

	AWLOGD("read algo auth data done");
	return ret;
}

int aw_ar_dsp_write_algo_auth_data(struct mixer *virt_mixer,
		char *data, unsigned int data_len)
{
	int ret = 0;

	ret = aw_ar_dsp_write_data_to_dsp(virt_mixer, AW_MSG_ID_ALGO_AUTH, data, data_len);
	if (ret < 0) {
		AWLOGE("write algo auth failed ");
		return ret;
	}

	AWLOGD("write algo auth done");
	return ret;
}

int aw_ar_dsp_write_volume(struct mixer *virt_mixer,
                                    int dev_index, char *data, unsigned int data_len)
{
    int32_t ret = 0;

    ret = aw_write_msg_to_dsp_v_1_0_0_0(virt_mixer, dev_index, AW_MSG_ID_VOLUME_L,
                data, data_len, AW_DSP_CHANNEL_DEFAULT_NUM);
    if (ret) {
        AWLOGE("write volume failed");
        return ret;
    }

    AWLOGD("dev[%d]write volume done", dev_index);
    return AW_OK;
}

int aw_ar_dsp_write_ramp_enable(struct mixer *virt_mixer,
                                    int dev_index, int enable)
{
    int32_t ret = 0;

    ret = aw_write_msg_to_dsp_v_1_0_0_0(virt_mixer, dev_index, AW_MSG_ID_RAMP_EN_L,
                (char *)(&enable), sizeof(enable), AW_DSP_CHANNEL_DEFAULT_NUM);
    if (ret) {
        AWLOGE("write ramp enable failed");
        return ret;
    }

    AWLOGD("dev[%d]write ramp enable:0x%x", dev_index, enable);
    return AW_OK;
}

int aw_ar_dsp_write_ramp_params(struct mixer *virt_mixer,
                                    int dev_index, char *data, unsigned int data_len)
{
    int32_t ret = 0;

    ret = aw_write_msg_to_dsp_v_1_0_0_0(virt_mixer, dev_index, AW_MSG_ID_RAMP_PARAMS_L,
                data, data_len, AW_DSP_CHANNEL_DEFAULT_NUM);
    if (ret) {
        AWLOGE("write ramp params failed");
        return ret;
    }

    AWLOGD("dev[%d]write ramp params done", dev_index);
    return AW_OK;
}

int aw_ar_dsp_read_run_state_avg(struct mixer *virt_mixer,
                                        int dev_index, char *data, unsigned int data_len)
{
	int ret = 0;

    ret = aw_read_msg_from_dsp_v_1_0_0_0(virt_mixer, dev_index, AW_MSG_ID_RUN_STATE_AVG,
                data, data_len, AW_DSP_CHANNEL_DEFAULT_NUM);
	if (ret < 0) {
		AWLOGE("read run state avg failed");
		return ret;
	}

	AWLOGD("read run state avg done");
	return ret;
}