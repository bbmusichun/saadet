/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The pictures between a page's header and its four values: what the values do, drawn from the sound itself (the
 * envelope's shape, the LFO's wave, the routes, the arp's steps, the scale, the chord, the voices, the FX chain) or,
 * on the GLOBAL pages, the first value large. pic_build puts a picture's data into the view (pv, ps), so the picture is
 * drawn again whenever what it shows changes (an arp step, a voice starting); pic_draw draws it from the view alone.
 * The area: y 30 .. 172, between the header and the column labels. */

/* -------------------------------------------------------------- data --- */
static void pic_arp(ui_vm_t *v, const track_t *t)   /* the 16 steps the pattern plays over the notes held */
{
    static const uint8_t EXAMPLE[3] = {60, 64, 67};   /* (nothing held: a C triad) */
    uint32_t list[64], cnt = t->nheld ? t->nheld : 3u, len = 0, i, j, o, lo = 127, hi = 0;
    for (i = 0; i < cnt; i++)
        list[i] = t->nheld ? t->held[i] : EXAMPLE[i];
    if (!t->p[P_AORDER])                                   /* (as seq.c arp_step: by pitch, or as played) */
        for (i = 1; i < cnt; i++)
            for (j = i; j > 0 && list[j - 1u] > list[j]; j--) {
                uint32_t x = list[j];
                list[j] = list[j - 1u];
                list[j - 1u] = x;
            }
    for (o = 0; o < (uint32_t)clamp(t->p[P_AOCT], 1, 4); o++)
        for (i = 0; i < cnt && len < 64u; i++)
            list[len++] = (uint32_t)clamp((int32_t)list[i] + 12 * (int32_t)o, 0, 127);
    if (t->p[P_AMODE] == 6) {                               /* REPEAT: the last note played */
        list[0] = t->nheld ? t->held[t->nheld - 1u] : EXAMPLE[2];
        len = 1;
    }
    for (i = 0; i < 16u; i++) {
        switch (t->p[P_AMODE]) {
        case 2: j = len - 1u - i % len; break;
        case 3: { uint32_t cyc = len > 1u ? 2u * len - 2u : 1u, k = i % cyc; j = k < len ? k : cyc - k; break; }
        case 4: j = (i * 5u + 3u) % len; break;         /* RND: one random-looking pattern */
        default: j = i % len; break;
        }
        v->pv[i] = (int16_t)list[j];
        if (list[j] < lo) lo = list[j];
        if (list[j] > hi) hi = list[j];
    }
    for (i = 0; i < 16u; i++)
        v->pv[i] = (int16_t)(v->pv[i] - (int32_t)lo);
    v->pv[16] = (int16_t)(hi - lo);
    v->pv[17] = (int16_t)(t->p[P_AMODE] && t->nheld ? (int32_t)(t->arp_idx % 16u) : -1);   /* the playhead */
    v->pv[18] = t->p[P_AGATE];
    v->pv[19] = t->p[P_ASWING];
    v->pv[20] = t->p[P_AMODE];
    fmt_int(v->ps[0], song.g[G_BPM]);
    str_cpy(v->ps[0] + str_len(v->ps[0]), " BPM", 8);
}
static void pic_route(char *b, const track_t *t, uint32_t k)   /* "LFO > PITCH", the engine's name for E1..E8 */
{
    int32_t src = t->p[P_M1SRC + 3u * k], dst = t->p[P_M1DST + 3u * k];
    const char *d = dst >= 12 && dst <= 19 ? track_desc(t, P_E0 + (uint32_t)(dst - 12))->label : 0;
    if (!src || !dst) {
        str_cpy(b, "OFF", 24);
        return;
    }
    str_cpy(b, N_MSRC[clamp(src, 0, (int32_t)NELEM(N_MSRC) - 1)], 24);
    str_cpy(b + str_len(b), " \x9b ", 24 - str_len(b));
    str_cpy(b + str_len(b), d && d[0] && !str_eq(d, "-") ? d : N_MDST[clamp(dst, 0, (int32_t)NELEM(N_MDST) - 1)],
            24 - str_len(b));
}
static void pic_build(ui_vm_t *v, const ui_stop_t *st)
{
    const track_t *t = TSEL;
    uint32_t k;
    v->pic = st->pic;
    switch (st->pic) {
    case PIC_ENV:
        v->pv[0] = t->p[P_ATK]; v->pv[1] = t->p[P_DEC]; v->pv[2] = t->p[P_SUS]; v->pv[3] = t->p[P_REL];
        break;
    case PIC_LFO:
        v->pv[0] = t->p[P_LWAVE]; v->pv[1] = t->p[P_LFADE]; v->pv[2] = t->p[P_LPOL]; v->pv[3] = t->p[P_LPHASE];
        break;
    case PIC_MATRIX:
        for (k = 0; k < 4u; k++) {
            pic_route(v->ps[k], t, k);
            v->pv[k] = t->p[P_M1SRC + 3u * k] && t->p[P_M1DST + 3u * k] ? t->p[P_M1AMT + 3u * k] : 0;
        }
        v->pv[4] = (int16_t)((int32_t)v->dot - 1);     /* the route of its page (AMOUNTS: none) */
        break;
    case PIC_ARP:
        pic_arp(v, t);
        break;
    case PIC_SCALE:
        v->pv[0] = t->p[P_SCALE]; v->pv[1] = t->p[P_ROOT]; v->pv[2] = t->p[P_QUANT]; v->pv[3] = t->p[P_TRANS];
        break;
    case PIC_CHORD: {                                     /* the last chord played, or the one the root would play */
        uint8_t out[CHORD_MAX];
        int32_t r;
        uint16_t mask;
        uint32_t n, i;
        if (!t->p[P_CHRD]) {
            str_cpy(v->ps[0], "OFF", 24);
            break;
        }
        if (chord_last[0].n > 1u) {
            n = chord_last[0].n;
            for (i = 0; i < n; i++) out[i] = chord_last[0].note[i];
            r = chord_last[0].root;
            mask = chord_last[0].mask;
        } else {
            n = chord_make(t, 60u + (uint32_t)t->p[P_ROOT] % 12u, out, &r, &mask);
        }
        chord_name(v->ps[0], (uint32_t)r, mask | 1u);
        for (i = 0; i < n && i < 4u; i++) v->pv[i] = out[i];
        v->pv[4] = (int16_t)n;
        v->pv[5] = (int16_t)(out[0] / 12u * 12u);          /* the strip starts at the C below the lowest note */
        break;
    }
    case PIC_VOICES: {
        const engine_t *e = ENGINES[eng_idx(t->eng_req)];
        uint32_t cap = e->cap ? e->cap(t) : e->poly ? e->poly : NPOLY;
        v->pv[0] = (int16_t)(cap > NVOICE ? NVOICE : cap);
        v->pv[1] = t->p[P_VOICE];
        for (k = 0; k < NVOICE; k++)
            if (t->v[k].active) v->pv[2] = (int16_t)(v->pv[2] | (int16_t)(1u << k));
        break;
    }
    case PIC_FX:
        for (k = 0; k < FX_SLOTS; k++) {
            int32_t type = t->p[P_FXTYPE + k * FX_PARAMS];
            str_cpy(v->ps[k], N_FX[clamp(type, 0, (int32_t)NELEM(N_FX) - 1)], 24);
            v->pv[k] = type ? t->p[P_FXMIX + k * FX_PARAMS] : 0;
            v->pv[4 + k] = (int16_t)type;
        }
        v->pv[3] = (int16_t)(st->f[0].id >= P_FXTONE && st->f[0].id <= P_FX3TONE ? st->f[0].id - P_FXTONE :
                             (st->f[0].id - P_FXTYPE) / FX_PARAMS);
        break;
    }
}

/* ------------------------------------------------------------ drawing --- */
static void pic_env(const ui_vm_t *v)
{
    int32_t a = 6 + v->pv[0] * 50 / 127, d = 6 + v->pv[1] * 50 / 127, r = 6 + v->pv[3] * 50 / 127;
    int32_t x1 = 12 + a, x2 = x1 + d, x3 = 228 - r, ys = 160 - v->pv[2] * 112 / 127;
    if (x3 < x2 + 16) x3 = x2 + 16;
    int32_t xy[10] = {12 * 16, 160 * 16, x1 * 16, 48 * 16, x2 * 16, ys * 16, x3 * 16, ys * 16, 228 * 16, 160 * 16};
    cv_rect(12, 161, 216, 1, T_LINE);
    cv_poly(xy, 5, 32, T_THEME, 3u);
    for (uint32_t i = 1; i < 4u; i++)
        cv_disc(xy[2 * i] / 16, xy[2 * i + 1] / 16, 7, T_THEME, 0);
}
static void pic_lfo(const ui_vm_t *v)
{
    int32_t xy[2 * 109], i, wave = v->pv[0], uni = v->pv[2];
    float fade = (float)v->pv[1] * 216.0f / 127.0f;
    cv_rect(12, uni ? 156 : 100, 216, 1, T_LINE);
    for (i = 0; i < 109; i++) {
        float ph = (float)i / 54.0f + (float)v->pv[3] / 128.0f, f = ph - fm_floorf(ph), w, amp;
        switch (wave) {
        case 0: w = fm_sinf(FM_TWO_PI * ph); break;                       /* SIN */
        case 1: w = 1.0f - 4.0f * fm_fabsf(f - 0.5f); w = -w; break;      /* TRI */
        case 2: w = 2.0f * f - 1.0f; break;                               /* SAW */
        case 3: w = f < 0.5f ? 1.0f : -1.0f; break;                       /* SQR */
        default: {                                                         /* S&H: a held random value a step */
            uint32_t h = (uint32_t)(fm_floorf(ph * 4.0f) + 64.0f) * 2654435761u;
            w = (float)((h >> 24) & 255u) / 127.5f - 1.0f;
            break;
        }
        }
        amp = fade > 0.0f && (float)(2 * i) < fade ? (float)(2 * i) / fade : 1.0f;
        xy[2 * i] = (12 + 2 * i) * 16;
        xy[2 * i + 1] = (int32_t)((uni ? 156.0f - (w + 1.0f) * 0.5f * amp * 104.0f : 100.0f - w * amp * 52.0f) * 16.0f);
    }
    cv_poly(xy, 109, 32, T_THEME, 3u);
}
static void pic_matrix(const ui_vm_t *v)
{
    char n[2] = {0, 0};
    for (int32_t k = 0; k < 4; k++) {
        int32_t y = 36 + 32 * k, cur = v->pv[4] == k, off = str_eq(v->ps[k], "OFF"), amt = v->pv[k];
        uint16_t ink = off ? T_DIM : cur || v->pv[4] < 0 ? T_TEXT : T_MID;
        n[0] = (char)('1' + k);
        cv_text(10, y + 2, TY_HD, n, cur ? T_THEME : T_DIM);
        cv_text_fit(30, y, TY_RW, v->ps[k], ink, T_BG, 200);
        cv_rect(30, y + 24, 200, 2, T_LINE);
        cv_rect(129, y + 21, 1, 8, T_FILL);
        if (amt > 0) cv_rect(130, y + 24, amt * 100 / 64, 2, cur ? T_THEME : ink);
        if (amt < 0) cv_rect(130 + amt * 100 / 64, y + 24, -amt * 100 / 64, 2, cur ? T_THEME : ink);
    }
}
static void pic_arp_draw(const ui_vm_t *v)
{
    int32_t span = v->pv[16] ? v->pv[16] : 1, ph = v->pv[17], w = v->pv[18] * 13 / 128, sw = v->pv[19] * 13 / 250;
    if (w < 2) w = 2;
    if (ph >= 0)
        cv_rect(16 + 13 * ph, 50, 13, 112, T_LINE);
    for (int32_t i = 0; i < 16; i++) {
        int32_t y = v->pv[16] ? 150 - v->pv[i] * 94 / span : 104, x = 17 + 13 * i + ((i & 1) ? sw : 0);
        cv_rect(x, y, w > 12 ? 12 : w, 7, !v->pv[20] ? T_OFF : i == ph ? T_THEME : T_MID);
    }
    for (int32_t i = 0; i <= 16; i += 4)
        cv_rect(16 + 13 * i, 166, 1, 3, T_OFF);
    cv_text_r(230, 32, TY_TX, v->ps[0], T_MID, T_BG);
}
static void pic_scale(const ui_vm_t *v)
{
    const int32_t cx = 120, cy = 92, r = 50;
    int32_t s = clamp(v->pv[0], 0, (int32_t)SCALE_TOTAL - 1), on = v->pv[2] != 0, i, n;
    char root[4];
    cv_arc(cx * 16, cy * 16, r * 16, 16, 0, 360, T_LINE);
    if (s < (int32_t)SCALE_LEGACY) {                       /* twelve semitones, the scale's lit */
        uint32_t mask = SCALE_MASK[s];
        for (i = 0; i < 12; i++) {
            float a = (float)i * (FM_TWO_PI / 12.0f) - FM_PI * 0.5f;
            int32_t x = cx + (int32_t)(fm_cosf(a) * (float)r), y = cy + (int32_t)(fm_sinf(a) * (float)r);
            if (!i) cv_disc(x, y, 11, on ? T_THEME : T_MID, 0);
            else if ((mask >> i) & 1u) cv_disc(x, y, 7, on ? T_TEXT : T_MID, 0);
            else cv_disc(x, y, 5, T_OFF, 0);
        }
    } else {                                                /* a tuning: its degrees where their pitches lie */
        const micro_scale_t *m = &MICRO_SCALE[s - (int32_t)SCALE_LEGACY];
        n = m->count;
        for (i = 0; i < n; i++) {
            float a = (float)m->pitch[i] / (float)m->pitch[n] * FM_TWO_PI - FM_PI * 0.5f;
            int32_t x = cx + (int32_t)(fm_cosf(a) * (float)r), y = cy + (int32_t)(fm_sinf(a) * (float)r);
            cv_disc(x, y, i ? (n > 24 ? 4 : 6) : 11, i ? (on ? T_TEXT : T_MID) : on ? T_THEME : T_MID, 0);
        }
    }
    str_cpy(root, N_NOTE[clamp(v->pv[1], 0, 11)], sizeof root);
    cv_text_c(cx, ty_y(cy, TY_NM), TY_NM, root, on ? T_TEXT : T_MID, T_BG);
    cv_text_c(cx, 150, TY_TX, SCALE_TITLE[s], T_MID, T_BG);
}
static void pic_chord(const ui_vm_t *v)
{
    int32_t base = v->pv[5], white = 0, off = str_eq(v->ps[0], "OFF");
    const aafont_t *f = text_has(TY_HR, v->ps[0]) ? TY_HR : TY_NM;
    cv_text_c(120, 44, f, v->ps[0], off ? T_DIM : T_TEXT, T_BG);
    for (uint32_t pass = 0; pass < 2u; pass++) {          /* two octaves from base (a C): the chord's notes lit */
        white = 0;
        for (int32_t k = 0; k < 24; k++) {
            int32_t s = k % 12, black = s == 1 || s == 3 || s == 6 || s == 8 || s == 10, lit = 0;
            for (int32_t i = 0; i < v->pv[4] && i < 4; i++) lit |= v->pv[i] == base + k;
            if (!black) {
                if (!pass) cv_rect(22 + 14 * white, 116, 13, 44, lit ? T_THEME : T_WELL);
                white++;
            } else if (pass) {
                int32_t x = 22 + 14 * white - 5;
                cv_rect(x - 2, 116, 13, 28, T_BG);
                cv_rect(x, 116, 9, 26, lit ? T_THEME : T_LINE);
            }
        }
    }
}
static void pic_voices(const ui_vm_t *v)
{
    int32_t cap = v->pv[0] > 0 ? v->pv[0] : 1, w = (216 - 3 * (cap - 1)) / cap, x, i;
    char b[24];
    if (w > 22) w = 22;
    x = 120 - (cap * w + 3 * (cap - 1)) / 2;
    for (i = 0; i < cap; i++)
        cv_rect(x + i * (w + 3), 66, w, 44, (v->pv[2] >> i) & 1 ? T_THEME : T_WELL);
    fmt_int(b, cap);
    str_cpy(b + str_len(b), cap == 1 ? " VOICE" : " VOICES", 12);
    cv_text_c(120, 124, TY_TX, b, T_MID, T_BG);
}
static void pic_fx(const ui_vm_t *v)
{
    char n[2] = {0, 0};
    for (int32_t k = 0; k < 3; k++) {
        int32_t y = 36 + 42 * k, cur = v->pv[3] == k, off = !v->pv[4 + k];
        uint16_t ink = off ? T_DIM : cur ? T_TEXT : T_MID;
        char mix[8];
        n[0] = (char)('1' + k);
        cv_text(10, y + 3, TY_HD, n, cur ? T_THEME : T_DIM);
        cv_text_fit(30, y, TY_RW, v->ps[k], ink, T_BG, 140);
        if (!off) {
            fmt_int(mix, (v->pv[k] * 100 + 63) / 127);
            str_cpy(mix + str_len(mix), "%", 2);
            cv_text_r(230, y, TY_RW, mix, ink, T_BG);
        }
        cv_rect(30, y + 26, 200, 2, T_LINE);
        cv_rect(30, y + 26, v->pv[k] * 200 / 127, 2, cur ? T_THEME : T_FILL);
    }
}
static void pic_readout(const ui_vm_t *v)                 /* the first value, large; its unit under it */
{
    const aafont_t *f = text_has(TY_PO, v->col[0].val) && text_w(TY_PO, v->col[0].val) <= 220 ? TY_PO : TY_HR;
    if (!v->col[0].on)
        return;
    cv_text_c(120, ty_y(100, f), f, v->col[0].val, T_TEXT, T_BG);
    if (v->col[0].unit[0])
        cv_text_c(120, 150, TY_TX, v->col[0].unit, T_MID, T_BG);
}
static void pic_draw(const ui_vm_t *v)
{
    switch (v->pic) {
    case PIC_ENV: pic_env(v); break;
    case PIC_LFO: pic_lfo(v); break;
    case PIC_MATRIX: pic_matrix(v); break;
    case PIC_ARP: pic_arp_draw(v); break;
    case PIC_SCALE: pic_scale(v); break;
    case PIC_CHORD: pic_chord(v); break;
    case PIC_VOICES: pic_voices(v); break;
    case PIC_FX: pic_fx(v); break;
    case PIC_READOUT: pic_readout(v); break;
    }
}
