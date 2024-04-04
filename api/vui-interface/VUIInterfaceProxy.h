/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "VoiceUIInterface.h"

typedef int32_t (*get_vui_intf_f)(struct vui_intf_t *intf_handle,
    vui_intf_param_t *model);

typedef int32_t (*release_vui_intf_f)(struct vui_intf_t *intf_handle);

#ifndef VUI_UNSUPPORTED
int32_t GetVUIInterface(struct vui_intf_t *intf, vui_intf_param_t *model);

int32_t ReleaseVUIInterface(struct vui_intf_t *intf);

int32_t VUISetParameters(uint32_t param_id, void *param_payload, size_t payload_size);

int32_t VUIGetParameters(uint32_t param_id, void **param_payload, size_t *payload_size);
#else
static inline int32_t GetVUIInterface(struct vui_intf_t *intf, vui_intf_param_t *model){return 0;}

static inline int32_t ReleaseVUIInterface(struct vui_intf_t *intf){return 0;}

static inline int32_t VUISetParameters(uint32_t param_id, void *param_payload, size_t payload_sizei){return 0;}

static inline int32_t VUIGetParameters(uint32_t param_id, void **param_payload, size_t *payload_size){return 0;}
#endif
