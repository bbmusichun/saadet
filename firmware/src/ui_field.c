/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* A field: one value a knob edits, wherever it lives (the selected part's parameters, the song's, a Prophet's panel,
 * the device's settings). It is read, stepped and written the way the web editor does it (editor.c SET, its
 * preferences), so a change from the panel and one from the editor are the same change, and the editor sees it. */
enum { FK_NONE, FK_TRACK, FK_GLOBAL, FK_P5, FK_DEV };
typedef struct { uint8_t kind, id; const char *label; } ui_field_t;   /* label: 0 = the descriptor's own */
/* the device's settings (kept with the settings, not the sound): screen sleep, the LEDs' level, the voice budget */
enum { DEV_SLEEP, DEV_LIGHTS, DEV_BUDGET, DEV_N };
static const char *const N_SLEEP[] = {"NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"};
static const param_desc_t DEV_D[DEV_N] = {
    {"SLEEP", F_ENUM, 0, 4, 0, N_SLEEP, 0},
    {"LIGHTS", F_ENUM, 0, LIGHTS_N - 1, LIGHTS_MID, LIGHTS_NAME, 0},
    {"BUDGET", F_INT, 8, 32, VOICE_BUDGET_DEFAULT, 0, "pts"},
};

/* its descriptor now (the engine's labels, an FX type's names), 0 = nothing to show */
static const param_desc_t *field_desc(const ui_field_t *f)
{
    const param_desc_t *d = 0;
    switch (f->kind) {
    case FK_TRACK: d = track_desc(TSEL, f->id); break;
    case FK_GLOBAL: d = global_desc(f->id); break;
    case FK_P5:
        if (TSEL->eng_req == ENGI_PROPHET && f->id < NELEM(P5_PANEL) && P5_PANEL[f->id].label)
            d = &P5_PANEL[f->id];
        break;
    case FK_DEV: d = f->id < DEV_N ? &DEV_D[f->id] : 0; break;
    }
    return d && d->label && d->label[0] && !str_eq(d->label, "-") ? d : 0;   /* (an engine's unused slot: "-") */
}
static int32_t field_get(const ui_field_t *f)
{
    switch (f->kind) {
    case FK_TRACK: return TSEL->p[f->id];
    case FK_GLOBAL: return f->id == G_A4 ? tuning_a4 : song.g[f->id];
    case FK_P5: return p5_patch_of(TSEL)->raw[f->id];
    case FK_DEV: return f->id == DEV_SLEEP ? (int32_t)scr_get() : f->id == DEV_LIGHTS ? settings_lights : settings_budget;
    }
    return 0;
}
static const char *field_label(const ui_field_t *f, const param_desc_t *d) { return f->label ? f->label : d->label; }
/* steps (+ clockwise) on it: the range, the aliases an enum skips, the shared scale, A4's save. 1 = it changed. A short
 * range or a list moves one step a detent however fast the knob turns */
static int field_step(const ui_field_t *f, int32_t steps)
{
    const param_desc_t *d = field_desc(f);
    int32_t from, v;
    if (!d || d->max <= d->min || !steps)
        return 0;
    if (d->fmt == F_ENUM || d->max - d->min < 24)
        steps = steps > 0 ? 1 : -1;
    from = field_get(f);
    v = clamp(from + steps, d->min, d->max);
    if (d->fmt == F_ENUM)
        v = enum_step(d, from, v);
    if (v == from)
        return 0;
    switch (f->kind) {
    case FK_TRACK:
        TSEL->p[f->id] = (int16_t)v;
        if (scale_shared(f->id))
            scale_share(TSEL);
        break;
    case FK_GLOBAL:
        if (f->id == G_A4) {
            tuning_a4 = (int16_t)v;
            settings_save();
        } else {
            song.g[f->id] = (int16_t)v;
        }
        break;
    case FK_P5:
        p5_edit_value(TSEL, f->id, v);
        break;
    case FK_DEV:                                      /* (as the editor's preferences: saved with the settings) */
        if (f->id == DEV_SLEEP) scr_put((uint32_t)v);
        else if (f->id == DEV_LIGHTS) settings_lights = (uint8_t)v;
        else settings_budget = (int16_t)v;
        settings_save();
        break;
    }
    return 1;
}
/* its value as the screen shows it: the digits or the name (val), a unit apart ("ms", "Hz"; "%" joins the digits) */
static void field_text(const ui_field_t *f, const param_desc_t *d, char *val, char *unit)
{
    const char *u = "";
    param_format(d, field_get(f), val, &u);
    if (u[0] == '%' && !u[1]) {
        str_cpy(val + str_len(val), "%", 2);
        u = "";
    }
    str_cpy(unit, u, 6);
}
