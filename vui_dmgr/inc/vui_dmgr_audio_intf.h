#ifndef __VUI_DMGR_AUDIO_INTF__
#define __VUI_DMGR_AUDIO_INTF__

#include <stdint.h>

#include <PalDefs.h>

typedef int32_t (*voiceuiDmgrCallback)(int32_t, void *, size_t);

typedef int (*vui_dmgr_init_t)(voiceuiDmgrCallback);
typedef void (*vui_dmgr_deinit_t)(void);

struct vui_dmgr_uuid_t {
    uint8_t data[16];
};

struct vui_dmgr_usecases_t {
    pal_stream_type_t stream_type;
    vui_dmgr_uuid_t vendor_uuid;
};

struct vui_dmgr_param_restart_usecases_t {
    vui_dmgr_usecases_t usecases[64];
    uint32_t num_usecases;
};

enum {
    VUI_DMGR_PARAM_ID_RESTART_USECASES = 0,
};

#endif /* __VUI_DMGR_AUDIO_INTF__ */
