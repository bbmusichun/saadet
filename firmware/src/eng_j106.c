/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* JUNO-106: a Juno-106-style part written for Saadet from the instrument's
 * published architecture and MIDI implementation. Per voice: one DCO (saw,
 * pulse with PWM, a square one octave below, white noise), a resonant
 * four-pole low-pass, and one ADSR shared by the filter and the VCA. Per part:
 * the triangle LFO with its delay, then, after the voices, the high-pass and
 * the stereo chorus, which the instrument also has once for all its voices.
 * Times, ranges and the chorus are engineering estimates, not calibrated
 * against an instrument. No other implementation's code or data is used.
 * Stored engine ID 21 (20 is the Prophet's user namespace). */
#include "x0x/fastmath.h"
#include "j106_patch.h"
#define J106_POLY 6u                     /* the instrument's six voices */
#define J106_CH_LEN 512u                 /* chorus line, a power of two: 11.6 ms */
#define J106_ENV_END (1.0f / 2048.0f)    /* about -66 dB: a released voice ends */
_Static_assert(FS == 44100 && CTL == 32, "Juno-106 rates assume 44.1 kHz and 32-sample blocks");

typedef struct {
    uint32_t phase;                      /* the DCO's ramp */
    int32_t noise;
    float z[4];                          /* filter stages */
    float env, env0;                     /* ADSR at the block's end and start, 0..1 */
    float gate, gate0;                   /* GATE-mode VCA, 0..1 */
    float g;                             /* filter coefficient at the end of the last block */
    uint8_t stage, sub, fresh;           /* stage: 0 off, 1 attack, 2 decay/sustain, 3 release */
    int32_t last_pcm;
} j106_voice_t;
/* Raw-to-DSP values of the whole part for one block, set before the voices
 * and read-only while they render (on either core). */
typedef struct {
    float lfo0, lfo1;                    /* the LFO after its delay, -1..1, at the block's start and end */
    float vib0, vib1;                    /* DCO pitch ratios from the LFO and the wheel */
    float saw, pulse, sub, noise;        /* mixer gains */
    float pwm, pwm_lfo;                  /* PWM depth 0..1; 1: by the LFO */
    float cut, env_oct, lfo_oct, key;    /* cutoff in octaves above 8 Hz; envelope, LFO amounts; keyboard follow */
    float k, drive, level;
    float atk, dec, sus, rel;            /* envelope coefficients per block */
    uint32_t range;                      /* 0 16', 1 8', 2 4' */
    int32_t tune;                        /* the track's TUNE offset, semitones */
    uint8_t gate;                        /* VCA by gate */
} j106_render_t;
typedef struct {
    j106_voice_t voice[J106_POLY];
    j106_render_t r;
    uint32_t lfo, delay_n;               /* LFO phase; samples since the first key */
    float lfo_out;
    uint8_t keys, chorus, hpf, quiet;    /* keys held last block; the last chorus and HPF modes; post state clear */
    float hp_x, hp_y, boost;             /* high-pass and bass-boost one-poles */
    uint32_t ch_w, ch_lfo;
    float ch_lp[2];
    float line[J106_CH_LEN];
} j106_part_t;
static j106_part_t *j106_part(uint32_t part);
static j106_patch_t j106_patch[NPART] __attribute__((section(".pool")));
static uint8_t j106_ready[NPART];
static j106_patch_t *j106_patch_of(const track_t *t)
{
    uint32_t k = (uint32_t)(t - trk) % NPART;
    if (!j106_ready[k]) { j106_patch_init(&j106_patch[k]); j106_ready[k] = 1; }
    return &j106_patch[k];
}
static inline __attribute__((always_inline)) j106_voice_t *j106_voice(track_t *t, voice_t *v)
{
    j106_part_t *part = j106_part((uint32_t)(t - trk));
    return part ? &part->voice[(uint32_t)(v - t->v) % J106_POLY] : 0;
}
static int32_t j106_value(const track_t *t, uint32_t k, uint32_t e)   /* a slider plus the track's offset P_E0+e */
{
    return clamp((int32_t)(j106_patch_of(t)->raw[k] & 127u) + (e < 8u ? t->p[P_E0 + e] : 0), 0, 127);
}

/* ------------------------------------------------------------ the voice --- */
/* Polynomial band-limited step at a phase wrap: the correction of a step of
 * -2 (a ramp's reset), from the ramp's phase and increment */
static inline __attribute__((always_inline)) float j106_blep(uint32_t ph, uint32_t inc)
{
    if (ph < inc) {
        float x = (float)ph / (float)inc;
        return x + x - x * x - 1.0f;
    }
    if (ph > 0xFFFFFFFFu - inc) {
        float x = -(float)(0xFFFFFFFFu - ph) / (float)inc;
        return x * x + x + x + 1.0f;
    }
    return 0.0f;
}
static inline __attribute__((always_inline)) float j106_ramp(uint32_t ph, uint32_t inc)
{
    return (float)ph * (1.0f / 2147483648.0f) - 1.0f - j106_blep(ph, inc);
}
/* The filter's input stage: a cubic that reaches its limit, 1, at 1.5 */
static inline __attribute__((always_inline)) float j106_sat(float x)
{
    x = fm_clampf(x, -1.5f, 1.5f);
    return x - x * x * x * (4.0f / 27.0f);
}
/* one-pole coefficient G = g / (1 + g), g = tan(pi f / FS), for f = 8 Hz * 2^oct */
static float j106_coeff(float oct)
{
    float f = 8.0f * fm_exp2f(fm_clampf(oct, 0.0f, 11.1f));
    float g = fm_tanf(FM_PI * fm_minf(f, 18000.0f) * (1.0f / FS));
    return g / (1.0f + g);
}

/* One block of the ADSR. The attack aims past full scale so that it arrives
 * in its time with a rounded top; decay and release approach exponentially. */
static void j106_env_tick(j106_voice_t *s, const j106_render_t *r, int gate)
{
    s->env0 = s->env;
    if (!gate && s->stage && s->stage < 3u) s->stage = 3;
    if (s->stage == 1u) {
        s->env += (1.2f - s->env) * r->atk;
        if (s->env >= 1.0f) { s->env = 1.0f; s->stage = 2; }
    } else if (s->stage == 2u) {
        s->env += (r->sus - s->env) * r->dec;
    } else if (s->stage == 3u) {
        s->env -= s->env * r->rel;
        if (s->env < J106_ENV_END) { s->env = 0.0f; s->stage = 0; }
    }
    s->gate0 = s->gate;
    s->gate = fm_clampf(s->gate + (gate ? 0.5f : -0.5f), 0.0f, 1.0f);   /* GATE: about 1.5 ms either way */
}

static void j106_note_on(track_t *t, voice_t *v)
{
    j106_voice_t *s = j106_voice(t, v);
    if (!s) return;
    if (!voice_was) {
        memset(s, 0, sizeof *s);
        s->phase = v->ph[0];                 /* the DCOs run free: a new voice finds them anywhere */
        s->noise = (int32_t)(0x6A1C3u + v->age * 31u + (uint32_t)(v - t->v) * 977u);
        s->sub = (uint8_t)(noise32(&s->noise) & 1u);
        s->fresh = 1;
        /* A circuit's own noise starts a resonant filter even with the
         * mixer shut; a -80 dB seed does it here and is otherwise inaudible. */
        s->z[0] = 1e-4f;
    }
    s->stage = 1;                            /* a retrigger rises from where the envelope is */
}
static int j106_done(track_t *t, voice_t *v)
{
    j106_voice_t *s = j106_voice(t, v);
    if (!s) return 1;
    if (v->gate) return 0;
    return j106_switch(j106_patch_of(t)->raw, J106_VCA_MODE) ? s->gate <= 0.0f && s->gate0 <= 0.0f : s->stage == 0u;
}
static __attribute__((noinline)) int32_t j106_env_source(track_t *t, voice_t *v)
{
    j106_voice_t *s = j106_voice(t, v);
    return s ? (int32_t)(s->env * 32767.0f) : 0;
}

typedef struct {
    float vib0, dvib, w0, dw, g0, dg, a0, da, m0, dm;
    float saw, pulse, sub, noise, k, drive, scale;
    float inc;
} j106_kernel_t;
static __attribute__((section(".dsp_text"))) void j106_samples(j106_voice_t *restrict s, int32_t *restrict out, uint32_t n,
                                                               const j106_kernel_t *restrict c)
{
    float z0 = s->z[0], z1 = s->z[1], z2 = s->z[2], z3 = s->z[3];
    uint32_t phase = s->phase, sub = s->sub;
    int32_t last = 0;
    for (uint32_t i = 0; i < n; i++) {
        float time = (float)i * (1.0f / CTL);
        uint32_t inc = (uint32_t)fm_clampf(c->inc * (c->vib0 + c->dvib * time), 1.0f, 1932735232.0f);
        float w = c->w0 + c->dw * time;
        float blep = j106_blep(phase, inc);
        float saw = (float)phase * (1.0f / 2147483648.0f) - 1.0f - blep;
        /* the pulse is the difference of two ramps: low for the first w of
         * the cycle, a step of 2 as the ramp's; its mean is zero at any width */
        float pulse = saw - j106_ramp(phase + (uint32_t)((1.0f - w) * 4294967296.0f), inc);
        float sq = (sub ? 1.0f : -1.0f) * (1.0f - fm_fabsf(blep));
        float noise = (float)(int32_t)noise32(&s->noise) * (1.0f / 2147483648.0f);
        float x = saw * c->saw + pulse * c->pulse + sq * c->sub + noise * c->noise;
        uint32_t next = phase + inc;
        if (next < phase) sub ^= 1u;         /* the sub-octave divider toggles at each reset */
        phase = next;
        float G = c->g0 + c->dg * time, G2 = G * G, G3 = G2 * G, G4 = G2 * G2;
        float sigma = (1.0f - G) * (G3 * z0 + G2 * z1 + G * z2 + z3);
        float u = j106_sat((x * c->drive - c->k * sigma) / (1.0f + c->k * G4));
        float v;
        v = (u - z0) * G; u = v + z0; z0 = u + v;
        v = (u - z1) * G; u = v + z1; z1 = u + v;
        v = (u - z2) * G; u = v + z2; z2 = u + v;
        v = (u - z3) * G; u = v + z3; z3 = u + v;
        float amp = (c->a0 + c->da * time) * (c->m0 + c->dm * time);
        last = (int32_t)(u * amp * c->scale);
        out[i] += last;
    }
    s->z[0] = fm_flush(z0); s->z[1] = fm_flush(z1); s->z[2] = fm_flush(z2); s->z[3] = fm_flush(z3);
    s->phase = phase; s->sub = (uint8_t)sub; s->last_pcm = last;
}

static void j106_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    j106_voice_t *s = j106_voice(t, v);
    j106_part_t *part = j106_part((uint32_t)(t - trk) % NPART);
    if (!s || !part) return;
    /* A budget victim is already inactive when its final faded block comes:
     * fade the last sample to zero rather than rendering another voice. */
    if (!v->active && !m->amp1) {
        if (n > 1u) {
            int32_t step = s->last_pcm / (int32_t)(n - 1u);
            for (uint32_t i = 0; i < n - 1u; i++) out[i] += s->last_pcm - step * (int32_t)i;
        }
        s->last_pcm = 0;
        return;
    }
    const j106_render_t *r = &part->r;
    j106_env_tick(s, r, v->gate);
    /* pitch: the track's tune offset, the range and the shared vibrato */
    uint32_t base = r->tune ? cents_inc(m->pitch16 + r->tune * 16, 0, m->fine) : m->inc;
    float inc = (float)base * (r->range == 0u ? 0.5f : r->range == 2u ? 2.0f : 1.0f);
    /* pulse width: 0.5 of the cycle down to 0.04, by the slider or the LFO,
     * plus the common SHAPE modulation */
    float shape = (float)(m->shape - (64 << 8)) * (1.0f / (127.0f * 256.0f));
    float pw0 = fm_clampf(r->pwm * (r->pwm_lfo ? (r->lfo0 + 1.0f) * 0.5f : 1.0f) + shape, 0.0f, 1.0f);
    float pw1 = fm_clampf(r->pwm * (r->pwm_lfo ? (r->lfo1 + 1.0f) * 0.5f : 1.0f) + shape, 0.0f, 1.0f);
    /* cutoff: the slider, the envelope, the LFO, the keys and the common
     * modulation (127 << 8 is the slider's whole range) */
    float key = r->key * ((float)m->pitch16 * (1.0f / 16.0f) - 60.0f) * (1.0f / 12.0f);
    float common = (float)m->cutoff * (11.1f / (127.0f * 256.0f));
    float oct1 = r->cut + s->env * r->env_oct + r->lfo1 * r->lfo_oct + key + common;
    float g1 = j106_coeff(oct1);
    if (s->fresh) { s->g = g1; s->fresh = 0; }
    float a0 = (r->gate ? s->gate0 : s->env0) * r->level, a1 = (r->gate ? s->gate : s->env) * r->level;
    float m0 = (float)m->amp0 * (1.0f / 32768.0f), m1 = (float)m->amp1 * (1.0f / 32768.0f);
    const j106_kernel_t c = {
        r->vib0, r->vib1 - r->vib0, 0.5f - 0.46f * pw0, 0.46f * (pw0 - pw1), s->g, g1 - s->g, a0, a1 - a0, m0, m1 - m0,
        r->saw, r->pulse, r->sub, r->noise, r->k, r->drive, 2.0f * VOICE_FS * AUDIO_ONE, inc
    };
    s->g = g1;
    void (*volatile run)(j106_voice_t *, int32_t *, uint32_t, const j106_kernel_t *) = j106_samples;
    run(s, out, n, &c);
}

/* ------------------------------------------------------------- the part --- */
static float j106_per_block(float seconds)    /* a one-pole's coefficient per block for a time constant */
{
    return 1.0f - fm_expf(-(float)CTL / (seconds * FS));
}
static float j106_tri(uint32_t phase)
{
    float x = (float)phase * (1.0f / 2147483648.0f);   /* 0..2 */
    return x < 1.0f ? x * 2.0f - 1.0f : 3.0f - x * 2.0f;
}
/* Once per block, before the voices: the LFO and its delay, and every
 * slider as the voices use it. */
static void j106_block(track_t *t)
{
    j106_part_t *P = j106_part((uint32_t)(t - trk));
    if (!P) return;
    const uint8_t *p = j106_patch_of(t)->raw;
    j106_render_t *r = &P->r;
    uint32_t keys = 0;
    for (uint32_t i = 0; i < NVOICE; i++) keys |= t->v[i].gate;
    /* The LFO delay restarts with the first key after all were up: it waits
     * half the time, then fades in over the other half. */
    if (keys && !P->keys) P->delay_n = 0;
    P->keys = (uint8_t)!!keys;
    float delay = (float)p[J106_LFO_DELAY] * (1.0f / 127.0f);
    delay = delay * delay * 3.0f * FS;
    if (P->delay_n < 0x7FFFFFFFu) P->delay_n += CTL;
    float fade = delay <= 0.0f ? 1.0f : fm_clampf(((float)P->delay_n - delay * 0.5f) / (delay * 0.5f), 0.0f, 1.0f);
    float hz = 0.1f * fm_exp2f((float)(p[J106_LFO_RATE] & 127u) * (7.64f / 127.0f));   /* 0.1 .. 20 Hz */
    P->lfo += (uint32_t)(hz * (4294967296.0f / FS)) * CTL;
    r->lfo0 = P->lfo_out;
    r->lfo1 = P->lfo_out = j106_tri(P->lfo) * fade;
    /* vibrato: the slider (square law, up to half an octave) and the wheel */
    float dco = (float)(p[J106_DCO_LFO] & 127u) * (1.0f / 127.0f);
    float depth = dco * dco * 0.5f + (float)t->mw * (0.1f / 127.0f);
    r->vib0 = fm_exp2f(r->lfo0 * depth);
    r->vib1 = fm_exp2f(r->lfo1 * depth);
    /* one waveform alone peaks near 0.45; all of them drive the filter's
     * input stage into its curve, as a full mixer does */
    r->saw = j106_switch(p, J106_SAW) ? 0.45f : 0.0f;
    r->pulse = j106_switch(p, J106_PULSE) ? 0.45f : 0.0f;
    r->sub = (float)j106_value(t, J106_SUB, 6) * (0.45f / 127.0f);
    r->noise = (float)j106_value(t, J106_NOISE, 7) * (0.35f / 127.0f);
    r->pwm = (float)j106_value(t, J106_DCO_PWM, 5) * (1.0f / 127.0f);
    r->pwm_lfo = j106_switch(p, J106_PWM_MODE) ? 0.0f : 1.0f;
    r->cut = (float)j106_value(t, J106_VCF_FREQ, 0) * (11.1f / 127.0f);
    r->env_oct = (float)j106_value(t, J106_VCF_ENV, 3) * (10.0f / 127.0f) * (j106_switch(p, J106_ENV_POL) ? -1.0f : 1.0f);
    r->lfo_oct = (float)j106_value(t, J106_VCF_LFO, 4) * (4.0f / 127.0f);
    r->key = (float)(p[J106_VCF_KYBD] & 127u) * (1.0f / 127.0f);
    float res = (float)j106_value(t, J106_VCF_RES, 1) * (1.0f / 127.0f);
    r->k = 4.2f * res * fm_sqrtf(res);         /* self-oscillation from about 0.95 */
    r->drive = 1.0f + 0.4f * r->k;              /* keeps some of the passband loss at high resonance */
    r->level = (float)(p[J106_VCA_LEVEL] & 127u) * (1.0f / 64.0f);
    /* 1.5 ms .. 3 s attack, 1.5 ms .. 12 s decay and release (to 1 %) */
    r->atk = j106_per_block(0.0015f * fm_exp2f((float)(p[J106_ENV_A] & 127u) * (10.97f / 127.0f)) / 1.79f);
    r->dec = j106_per_block(0.0015f * fm_exp2f((float)(p[J106_ENV_D] & 127u) * (12.97f / 127.0f)) / 4.6f);
    r->rel = j106_per_block(0.0015f * fm_exp2f((float)(p[J106_ENV_R] & 127u) * (12.97f / 127.0f)) / 4.6f);
    r->sus = (float)(p[J106_ENV_S] & 127u) * (1.0f / 127.0f);
    r->range = j106_range(p);
    r->tune = t->p[P_E2];                       /* (here: a voice may render after the matrix restores the track) */
    r->gate = (uint8_t)j106_switch(p, J106_VCA_MODE);
}

/* After the voices: the high-pass (0 lifts the lows, 1 is flat, 2 and 3 cut
 * progressively), as the instrument filters its mixed voices once. */
static void j106_post(track_t *t, int32_t *out, uint32_t n, uint32_t nr)
{
    j106_part_t *P = j106_part((uint32_t)(t - trk) % NPART);
    if (!P) return;
    if (!nr) {
        if (!P->quiet) {
            P->hp_x = P->hp_y = P->boost = P->ch_lp[0] = P->ch_lp[1] = 0.0f;
            memset(P->line, 0, sizeof P->line);
            P->quiet = 1;
        }
        return;
    }
    P->quiet = 0;
    uint32_t hpf = j106_hpf(j106_patch_of(t)->raw);
    if (hpf != P->hpf) { P->hp_x = P->hp_y = P->boost = 0.0f; P->hpf = (uint8_t)hpf; }
    if (hpf == 1u) return;
    if (hpf == 0u) {                          /* low shelf, about +4 dB below 80 Hz */
        const float a = 1.0f - 0.98866f;      /* 1 - exp(-2 pi 80 / FS) */
        float lp = P->boost;
        for (uint32_t i = 0; i < n; i++) {
            float x = (float)out[i];
            lp += (x - lp) * a;
            out[i] = (int32_t)(x + lp * 0.6f);
        }
        P->boost = lp;
        return;
    }
    /* one-pole high-pass, about 240 Hz (2) or 720 Hz (3) */
    const float k = hpf == 2u ? 0.96639f : 0.90244f;   /* exp(-2 pi f / FS) */
    float x1 = P->hp_x, y1 = P->hp_y;
    for (uint32_t i = 0; i < n; i++) {
        float x = (float)out[i];
        y1 = (y1 + x - x1) * k;
        x1 = x;
        out[i] = (int32_t)y1;
    }
    P->hp_x = x1; P->hp_y = y1;
}

/* The chorus: one delay line, two taps swept in opposite directions by a
 * triangle, each tap mixed with the dry signal for one side. I is slower and
 * II faster; both lie between about 1.7 and 5.3 ms. The taps pass a gentle
 * low-pass, as a bucket-brigade line's filters would. 1: out now holds
 * L + R and left holds L. */
static uint32_t j106_stereo(track_t *t, int32_t *out, int32_t *left, uint32_t n)
{
    j106_part_t *P = j106_part((uint32_t)(t - trk) % NPART);
    uint32_t mode = j106_chorus(j106_patch_of(t)->raw);
    if (!P) return 0;
    if (!P->chorus && mode) {                 /* the line holds nothing older than this */
        memset(P->line, 0, sizeof P->line);
        P->ch_lp[0] = P->ch_lp[1] = 0.0f;
    }
    P->chorus = (uint8_t)mode;
    if (!mode) return 0;
    const float rate = mode == 1u ? 0.5f : 0.83f, center = 3.5e-3f * FS, depth = (mode == 1u ? 1.65e-3f : 1.8e-3f) * FS;
    const float lp = 0.67f;                   /* 1 - exp(-2 pi 7.7 kHz / FS) */
    uint32_t w = P->ch_w, ph = P->ch_lfo, inc = (uint32_t)(rate * (4294967296.0f / FS));
    float l0 = P->ch_lp[0], l1 = P->ch_lp[1];
    for (uint32_t i = 0; i < n; i++) {
        float x = (float)out[i];
        P->line[w] = x;
        float tri = j106_tri(ph);
        float d0 = center + depth * tri, d1 = center - depth * tri;
        float r0 = (float)w - d0 + (float)J106_CH_LEN, r1 = (float)w - d1 + (float)J106_CH_LEN;
        uint32_t i0 = (uint32_t)r0, i1 = (uint32_t)r1;
        float f0 = r0 - (float)i0, f1 = r1 - (float)i1;
        float a = P->line[i0 & (J106_CH_LEN - 1u)], b = P->line[(i0 + 1u) & (J106_CH_LEN - 1u)];
        float c = P->line[i1 & (J106_CH_LEN - 1u)], d = P->line[(i1 + 1u) & (J106_CH_LEN - 1u)];
        l0 += (a + (b - a) * f0 - l0) * lp;
        l1 += (c + (d - c) * f1 - l1) * lp;
        float L = (x + l0) * 0.7071f, R = (x + l1) * 0.7071f;
        left[i] = (int32_t)L;
        out[i] = (int32_t)(L + R);
        w = (w + 1u) & (J106_CH_LEN - 1u);
        ph += inc;
    }
    P->ch_w = w; P->ch_lfo = ph; P->ch_lp[0] = l0; P->ch_lp[1] = l1;
    return 1;
}

/* ------------------------------------------------------------ the sounds --- */
/* Saadet's own starter tones, made for this engine; no factory bank of the
 * instrument is included. Field order as j106_patch.h. */
static const struct { char name[J106_NAME_LEN + 1u]; uint8_t raw[J106_RAW]; } J106_TONES[] = {
    {"WARM PAD",   {40, 60, 0, 50, 0,   58, 20, 30, 6, 40,    64, 70, 80, 90, 75, 40,   26, 16}},
    {"STRINGS",    {50, 40, 4, 70, 0,   70, 0, 10, 0, 60,     64, 55, 70, 100, 70, 0,   90, 8}},
    {"BRASS",      {60, 30, 0, 0, 0,    38, 25, 70, 0, 50,    64, 25, 60, 70, 35, 30,   50, 16}},
    {"SUB BASS",   {64, 0, 0, 0, 0,     30, 40, 45, 0, 30,    70, 0, 55, 30, 20, 100,   41, 24}},
    {"PLUCK BASS", {64, 0, 0, 0, 0,     22, 60, 75, 0, 40,    70, 0, 45, 0, 25, 70,     49, 16}},
    {"POLY SYNTH", {45, 0, 0, 40, 0,    50, 35, 55, 0, 50,    64, 5, 65, 50, 45, 20,    90, 16}},
    {"SQUARE LEAD",{70, 70, 10, 0, 0,   64, 30, 35, 0, 60,    64, 0, 60, 80, 30, 0,     42, 16}},
    {"ORGAN",      {64, 0, 0, 0, 0,     100, 0, 0, 0, 40,     64, 0, 0, 127, 0, 60,     12, 12}},
    {"WIND",       {20, 0, 0, 0, 100,   55, 90, 0, 40, 0,     64, 90, 90, 100, 90, 0,   66, 8}},
    {"RES SWEEP",  {64, 0, 0, 0, 0,     20, 105, 80, 0, 30,   64, 60, 95, 20, 70, 0,    18, 16}},
    {"PWM CHOIR",  {35, 0, 0, 90, 0,    66, 10, 0, 0, 50,     64, 80, 90, 110, 80, 0,   74, 8}},
    {"GLASS KEYS", {64, 0, 0, 30, 0,    70, 75, 40, 0, 100,   64, 0, 70, 0, 70, 0,      92, 1}},
    {"HOLLOW LEAD",{64, 80, 12, 100, 0, 60, 50, 30, 0, 70,    64, 10, 60, 90, 30, 0,    42, 17}},
    {"FALLING PAD",{64, 0, 0, 0, 0,     90, 40, 60, 0, 30,    64, 50, 90, 40, 50, 0,    18, 18}},
    {"SOFT KEYS",  {64, 0, 0, 20, 0,    45, 10, 50, 0, 80,    64, 0, 75, 20, 55, 30,    74, 9}},
    {"ARP BLIP",   {64, 0, 0, 0, 0,     40, 70, 70, 0, 40,    64, 0, 30, 0, 20, 0,      50, 8}},
};
#define J106_TONES_N ((uint32_t)NELEM(J106_TONES))
static void j106_tone(uint32_t k, j106_patch_t *p)    /* preset k: 0 INIT, then the starter tones */
{
    j106_patch_init(p);
    if (!k || k > J106_TONES_N) return;
    memcpy(p->raw, J106_TONES[k - 1u].raw, J106_RAW);
    p->program = (uint8_t)(k - 1u);
    j106_patch_name_set(p, J106_TONES[k - 1u].name);
}
static void j106_preset_make(uint32_t k, preset_t *pr, char *name)
{
    pr->env[2] = 127;
    str_cpy(name, k && k <= J106_TONES_N ? J106_TONES[k - 1u].name : "INIT TONE", 21);
}
static void j106_preset_loaded(track_t *t, uint32_t pi)
{
    j106_tone(pi, j106_patch_of(t));
}
static uint32_t j106_cap(const track_t *t) { (void)t; return J106_POLY; }
/* Provisional: one DCO and one filter per voice, cheaper than a Prophet
 * voice. Not yet measured on hardware. */
static uint32_t j106_units(const track_t *t) { (void)t; return 2u; }
static const engine_t ENG_J106 = {
    .name = "JUNO-106", .page_title = {"FILTER/TUNE", "DCO/MIX"},
    .edit = {{"CUT", F_INT, -63, 63, 0, 0, 0}, {"RES", F_INT, -63, 63, 0, 0, 0},
             {"TUNE", F_SEMI, -24, 24, 0, 0, 0}, {"ENV", F_INT, -63, 63, 0, 0, 0},
             {"LFO", F_INT, -63, 63, 0, 0, 0}, {"PWM", F_INT, -63, 63, 0, 0, 0},
             {"SUB", F_INT, -63, 63, 0, 0, 0}, {"NOISE", F_INT, -63, 63, 0, 0, 0}},
    .preset_make = j106_preset_make, .npresets = J106_TONES_N + 1u, .poly = J106_POLY, .ownenv = 1,
    .note_on = j106_note_on, .render = j106_render, .done = j106_done, .block = j106_block,
    .units = j106_units, .cap = j106_cap, .post = j106_post, .stereo = j106_stereo,
    .knob = {P_E0, P_E1, P_E3, P_REL}
};
