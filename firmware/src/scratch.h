/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#ifndef SAADET_MAIN_WORKSPACE_H
#define SAADET_MAIN_WORKSPACE_H
#include <stdint.h>

/* Synchronous main-loop borrows only. No ISR, DMA or CPU1 may keep a pointer. */
static union {
    uint32_t alignment;
    uint8_t st[256];
    uint8_t dec[640];
#ifndef MELODEE_LOADER
    int16_t snap[512];
    uint8_t transfer[256];
#endif
} main_scratch;
#endif
