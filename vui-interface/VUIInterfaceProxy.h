#ifndef VOICEUI_INTERFACE_PROXY_H
#define VOICEUI_INTERFACE_PROXY_H

#include "VoiceUIInterface.h"

int32_t GetVUIInterface(struct vui_intf_t *intf, vui_intf_param_t *model);
int32_t ReleaseVUIInterface(struct vui_intf_t *intf);
int32_t VUIGetParameters(uint32_t param_id, void **param_payload, size_t *payload_size);
int32_t VUISetParameters(uint32_t param_id, void *param_payload, size_t payload_size);

#endif /* VOICEUI_INTERFACE_PROXY_H */
