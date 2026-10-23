/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Retained Saadet overload policy; decisions stay on CPU0 between halves. */
static uint32_t shed_count,shed_part[NPART];
static uint8_t sd_overload_history,sd_shed_pending;
static void shed_voice(void)
{
    uint32_t p, i;
    voice_t *best = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && !v->gate && v->stage != 4u && (!best || v->env < best->env))
                best = v;
        }
    if (best) {
        voice_kill(best);
        shed_count++;
        shed_part[((uintptr_t)best - (uintptr_t)trk) / sizeof(track_t)]++;
        return;
    }
    for (p = 0; p < NPART; p++) {
        const track_t *t = &trk[p];
        uint32_t mode = trk_vmode(t), low = mode == V_POLY ? lowest_held(t) : 0u;
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && v->gate && v->stage != 4u &&
                ((mode == V_POLY && i != low) || (mode != V_POLY && i > 0u))) {
                if (!best || v->age < best->age)
                    best = v;
            }
        }
    }
    if (best) {
        voice_kill(best);
        shed_count++;
        shed_part[((uintptr_t)best - (uintptr_t)trk) / sizeof(track_t)]++;
    }
}

void saadet_deadline(uint32_t us,uint32_t budget){
    if(us>sd_render_max_us)sd_render_max_us=us;
    sd_overload_history=(uint8_t)((sd_overload_history<<1)|(us>budget*85u/100u));
    if((sd_overload_history&3u)==3u)sd_shed_pending=1;
    song.cpu_q8=(song.cpu_q8*31u+(budget?us*256u/budget:0u))/32u;
}
