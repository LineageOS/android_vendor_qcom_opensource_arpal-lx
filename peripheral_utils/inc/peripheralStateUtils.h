#ifndef __PERIPHERAL_STATE_CONTROL__
#define __PERIPHERAL_STATE_CONTROL__

#include <stdint.h>

typedef int32_t (*PeripheralStateCB)(const uint32_t, const uint8_t);

void *registerPeripheralCB(uint32_t peripheral, PeripheralStateCB NotifyEvent);
int32_t getPeripheralState(void *context);
int32_t deregisterPeripheralCB(void *context);

enum {
    STATE_SECURE           = 1,
    STATE_NONSECURE        = 2,
    STATE_PRE_CHANGE       = 4,
    STATE_POST_CHANGE      = 5,
    STATE_RESET_CONNECTION = 255,
};

enum {
    PRPHRL_ERROR = -1,
    PRPHRL_SUCCESS = 0,
};

#endif /* __PERIPHERAL_STATE_CONTROL__ */
