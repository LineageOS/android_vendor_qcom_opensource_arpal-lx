/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef MEMLOG_BUILDER_H
#define MEMLOG_BUILDER_H


// Todo need to define MEM_LOGGER macro
#ifndef PAL_MEMLOG_UNSUPPORTED
#include "mem_logger.h"
#include "pal_state_queue.h"
#include "kpi_queue.h"
#else
struct pal_state_queue {};
enum pal_state_queue_state {
    PAL_STATE_OPENED,
    PAL_STATE_STARTED,
    PAL_STATE_PAUSED,
    PAL_STATE_SUSPENDED,
    PAL_STATE_STOPPED,
    PAL_STATE_CLOSED,
};
#endif
#include "ResourceManager.h"
#include "Stream.h"
#include <inttypes.h>
#ifndef PAL_MEMLOG_UNSUPPORTED
int palStateQueueBuilder(pal_state_queue &que, Stream *s, pal_state_queue_state state, int32_t error);
int palStateEnqueue(Stream *s, pal_state_queue_state state, int32_t error);
void kpiEnqueue(const char name[], bool isEnter);
#else
static inline int palStateQueueBuilder(pal_state_queue &que, Stream *s, pal_state_queue_state state, int32_t error)
{return 0;}
static inline int palStateEnqueue(Stream *s, pal_state_queue_state state, int32_t error)
{return 0;}
static inline void kpiEnqueue(const char name[], bool isEnter)
{return;}
#endif
#endif
