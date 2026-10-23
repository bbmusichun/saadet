/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* JUNO-106 panel fields: the sixteen sliders and the two switch bytes, then
 * the switches as fields of their own (j106_patch.h J106_RANGE..). Editing a
 * switch changes only its own bits; the other bytes stay as they are. */
static const char *const J106_RANGE_NAMES[] = {"16'", "8'", "4'"};
static const char *const J106_CHORUS_NAMES[] = {"OFF", "I", "II"};
static const char *const J106_PWM_NAMES[] = {"LFO", "MAN"};
static const char *const J106_POL_NAMES[] = {"NORMAL", "INVERT"};
static const char *const J106_VCA_NAMES[] = {"ENV", "GATE"};
#define J106_K(n, l) [n] = PD(l, F_INT, 0, 127, 0)
static const param_desc_t J106_PANEL[J106_FIELDS] = {
    J106_K(J106_LFO_RATE, "RATE"), J106_K(J106_LFO_DELAY, "DELAY"), J106_K(J106_DCO_LFO, "DCO LFO"),
    J106_K(J106_DCO_PWM, "PWM"), J106_K(J106_NOISE, "NOISE"), J106_K(J106_VCF_FREQ, "FREQ"),
    J106_K(J106_VCF_RES, "RES"), J106_K(J106_VCF_ENV, "ENV"), J106_K(J106_VCF_LFO, "VCF LFO"),
    J106_K(J106_VCF_KYBD, "KYBD"), J106_K(J106_VCA_LEVEL, "LEVEL"), J106_K(J106_ENV_A, "ATTACK"),
    J106_K(J106_ENV_D, "DECAY"), J106_K(J106_ENV_S, "SUSTAIN"), J106_K(J106_ENV_R, "RELEASE"),
    J106_K(J106_SUB, "SUB"), J106_K(J106_SW1, "SW1"), J106_K(J106_SW2, "SW2"),
    [J106_RANGE] = PE("RANGE", J106_RANGE_NAMES, 1), [J106_PULSE] = PE("PULSE", N_ONOFF, 0),
    [J106_SAW] = PE("SAW", N_ONOFF, 1), [J106_CHORUS] = PE("CHORUS", J106_CHORUS_NAMES, 0),
    [J106_PWM_MODE] = PE("PWM BY", J106_PWM_NAMES, 0), [J106_ENV_POL] = PE("ENV POL", J106_POL_NAMES, 0),
    [J106_VCA_MODE] = PE("VCA", J106_VCA_NAMES, 0), [J106_HPF] = PD("HPF", F_INT, 0, 3, 1),
};
#undef J106_K
static int32_t j106_get(const track_t *t, uint32_t id)
{
    const uint8_t *p = j106_patch_of(t)->raw;
    return id < J106_RAW ? p[id] : (int32_t)j106_switch(p, id);
}
static void j106_edit_value(track_t *t, uint32_t id, int32_t value)
{
    if (id >= J106_FIELDS) return;
    uint8_t *p = j106_patch_of(t)->raw;
    value = clamp(value, J106_PANEL[id].min, J106_PANEL[id].max);
    if (id < J106_RAW) p[id] = (uint8_t)value;
    else j106_switch_set(p, id, (uint32_t)value);
}
/* Conservative signal-path hints, as on the Prophet's panel: dim a control
 * whose waveform, destination or mode is off. It stays editable. */
static int j106_ed_active(const track_t *t, uint32_t id)
{
    const uint8_t *p = j106_patch_of(t)->raw;
    /* A matrix route can supply modulation of its own: no hints then. */
    for (uint32_t k = 0; k < 4u; k++) if (t->p[P_M1SRC + 3u * k] && t->p[P_M1DST + 3u * k] && t->p[P_M1AMT + 3u * k]) return 1;
    int pulse = (p[J106_SW1] >> 3) & 1u, pwm = pulse && p[J106_DCO_PWM];
    int lfo = p[J106_DCO_LFO] || p[J106_VCF_LFO] || t->mw || (pwm && !(p[J106_SW2] & 1u));
    switch (id) {
    case J106_DCO_PWM: return pulse;
    case J106_PWM_MODE: return pwm;
    case J106_LFO_RATE: case J106_LFO_DELAY: return lfo;
    case J106_ENV_POL: return p[J106_VCF_ENV] != 0;
    case J106_ENV_A: case J106_ENV_D: case J106_ENV_S: case J106_ENV_R:
        return !(p[J106_SW2] & 4u) || p[J106_VCF_ENV];
    default: return 1;
    }
}
