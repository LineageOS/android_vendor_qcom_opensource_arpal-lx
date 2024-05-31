/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: VoiceUIInterface"

#include <dlfcn.h>
#include "VUIInterfaceProxy.h"
#include "SVAInterface.h"
#include "SVAExtension.h"

typedef struct vui_intf_plugin {
    void *handle;
    release_vui_intf_f release_intf;
    struct vui_intf_t *intf;
} vui_intf_plugin_t;

std::map<st_module_type_t, vui_intf_plugin_t *> vui_intf_plugin_map;

int32_t GetVUIInterface(struct vui_intf_t *intf, vui_intf_param_t *model) {

    int32_t status = 0;
    get_vui_intf_f get_intf = nullptr;
    sound_model_config_t *config = nullptr;
    vui_intf_plugin_t *plugin = nullptr;

    if (!intf || !model || !model->data)
        return -EINVAL;

    config = (sound_model_config_t *)model->data;
    switch (config->module_type) {
        case ST_MODULE_TYPE_GMM:
        case ST_MODULE_TYPE_PDK:
        case ST_MODULE_TYPE_MMA:
            intf->interface = SVAInterface::GetInstance(model);
            break;
        case ST_MODULE_TYPE_HW:
        case ST_MODULE_TYPE_CUSTOM_1:
        case ST_MODULE_TYPE_CUSTOM_2:
            plugin = (vui_intf_plugin_t *)calloc(1, sizeof(vui_intf_plugin_t));
            if (!plugin) {
                status = -ENOMEM;
                break;
            }

            plugin->handle = dlopen(config->intf_plugin_lib.c_str(), RTLD_NOW);
            if (!plugin->handle) {
                status = -ENOMEM;
                free(plugin);
                break;
            }

            get_intf = (get_vui_intf_f)dlsym(plugin->handle, "get_vui_interface");
            if (!get_intf) {
                status = -ENOMEM;
                dlclose(plugin->handle);
                free(plugin);
                break;
            }

            plugin->release_intf = (release_vui_intf_f)dlsym(plugin->handle, "release_vui_interface");
            if (!plugin->release_intf) {
                status = -ENOMEM;
                dlclose(plugin->handle);
                free(plugin);
                break;
            }

            status = get_intf(intf, model);
            if (status != 0) {
                dlclose(plugin->handle);
                free(plugin);
                break;
            }
            plugin->intf = intf;
            vui_intf_plugin_map[config->module_type] = plugin;
            break;
        default:
            status = -EINVAL;
            break;
    }

    return status;
}

int32_t ReleaseVUIInterface(struct vui_intf_t *intf) {
    int32_t status = 0;
    st_module_type_t type;
    vui_intf_param_t param {};
    vui_intf_plugin_t *plugin = nullptr;

    if (!intf || !intf->interface)
        return -EINVAL;

    for (auto iter = vui_intf_plugin_map.begin();
         iter != vui_intf_plugin_map.end(); ) {
        plugin = iter->second;
        if (plugin->intf == intf) {
            status = plugin->release_intf(intf);
            dlclose(plugin->handle);
            free(plugin);
            vui_intf_plugin_map.erase(iter);
            break;
        } else {
            iter++;
        }
    }

    return status;
}

/* ============== Global Set and Get param APIs to enable SVA ======================== */
int32_t VUISetParameters(uint32_t param_id, void *param_payload, size_t payload_size) {

    int32_t status = 0;
    std::shared_ptr<SVAExtension> ext = SVAExtension::GetInstance();

    if (!param_payload || !ext) {
        return -EINVAL;
    }

    status = ext->SetParameters(param_id, param_payload, payload_size);

    return status;
}

int32_t VUIGetParameters(uint32_t param_id, void **param_payload, size_t *payload_size) {

    int32_t status = 0;
    std::shared_ptr<SVAExtension> ext = SVAExtension::GetInstance();

    if (!param_payload || !*param_payload || !payload_size || !ext) {
        return -EINVAL;
    }

    status = ext->GetParameters(param_id, param_payload, payload_size);

    return status;
}
