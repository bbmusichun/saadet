/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#pragma once
#include <stdint.h>
/* Native audio is Q23 in int32 containers. Controls, phases, envelopes and
 * coefficient tables retain their own units. Q15 is available to regression
 * fixtures; USB format selection never changes synthesis precision. */
#ifndef AUDIO_FRAC_BITS
#define AUDIO_FRAC_BITS 8
#endif
#if AUDIO_FRAC_BITS != 0 && AUDIO_FRAC_BITS != 8
#error "Audio precision must be Q15 or Q23"
#endif
#define AUDIO_ONE (1 << AUDIO_FRAC_BITS)
#define AUDIO_Q15(x) ((int32_t)(x) * AUDIO_ONE)
#if AUDIO_FRAC_BITS
typedef struct { uint8_t byte[3]; } audio_history_t;
typedef int32_t audio_ring_t;
typedef int64_t audio_dc_t;
#else
typedef int16_t audio_history_t;
typedef int16_t audio_ring_t;
typedef int32_t audio_dc_t;
#endif
/* Packed delay histories retain all 24 bits without doubling their RAM.
 * USB rings stay word aligned for the short interrupt-guarded copies.
 * These run inside per-sample DSP loops: always inline them, or the target
 * compiler emits a call per sample from the mixer, pan and DC kernels. */
#define AUDIO_INLINE static inline __attribute__((always_inline))
AUDIO_INLINE int32_t audio_history_read(const audio_history_t *p)
{
#if AUDIO_FRAC_BITS
    int32_t x=(int32_t)((uint32_t)p->byte[0]|(uint32_t)p->byte[1]<<8|(uint32_t)p->byte[2]<<16);
    return x&0x800000?x-16777216:x;
#else
    return *p;
#endif
}
AUDIO_INLINE void audio_history_write(audio_history_t *p,int32_t x)
{
#if AUDIO_FRAC_BITS
    p->byte[0]=(uint8_t)x;p->byte[1]=(uint8_t)((uint32_t)x>>8);p->byte[2]=(uint8_t)((uint32_t)x>>16);
#else
    *p=(int16_t)x;
#endif
}
AUDIO_INLINE int32_t audio_mul(int32_t sample,int32_t gain,uint32_t shift)
{ return (int32_t)(((int64_t)sample*gain)>>shift); }
/* Split the Q30 multiply: DC states carry eight additional guard bits. */
AUDIO_INLINE audio_dc_t audio_dc_decay(audio_dc_t y,int32_t coefficient)
{
    return (audio_dc_t)((y>>30)*coefficient+
        (((int64_t)(y&1073741823)*coefficient)>>30));
}
AUDIO_INLINE int32_t audio_clamp(int32_t x,int32_t lo,int32_t hi)
{ return x<AUDIO_Q15(lo)?AUDIO_Q15(lo):x>AUDIO_Q15(hi)?AUDIO_Q15(hi):x; }
