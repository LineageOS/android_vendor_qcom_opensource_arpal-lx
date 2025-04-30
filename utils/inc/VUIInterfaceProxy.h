#ifndef VOICEUI_INTERFACE_PROXY_H
#define VOICEUI_INTERFACE_PROXY_H

int VUIGetParameters(unsigned int param_id, void **param_payload, unsigned long *payload_size);
int VUISetParameters(unsigned int param_id, void *param_payload, unsigned long payload_size);

#endif
