/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The interface. Every button is one place (ui_pages.c) and the screen names it; the four knobs edit the four values at
 * the foot of the screen; pressing the place's button again or turning SELECT turns its page; holding the button lists
 * what is inside on the screen and on the keys (a key jumps there, the keys play no notes while it is held). On HOME the
 * four knobs are the sound's macros and a turn shows its value over the whole screen. PRESETS browses the sounds and
 * ALGORITHM the engines (the sound loads when they rest). REC latches the notes held, OCT- / OCT+ shift the octave
 * (both: back to 0; both for 5 s: UPDATE MODE, main.c). Startup maintenance (ui_boot.c), the update countdown below,
 * UPDATE (ota_hw.c) and the crash report (main.c) are the system's own screens. */
static struct {
    uint8_t force;               /* the screen to be drawn again whole */
    uint8_t uboot;               /* main.c: seconds left before UPDATE MODE (OCT- + OCT+ held), 0 = none */
} ui;

static void ui_message(const char *s);
static void ui_say(const char *a, const char *b);

#include "screen.c"
#include "ui_boot.c"
#include "ui_field.c"
#include "ui_pages.c"
#include "ui_view.c"
#include "ui_pics.c"
static void up_name(uint32_t k, char *b);                  /* upreset.c */
static void up_slot_label(char *b, uint32_t k);

#define POP_MS 1200u             /* HOME: a macro's value over the screen, after the knob rests */
#define TOAST_MS 1500u           /* a message in the header */
#define LOAD_MS 300u             /* PRESETS / ALGORITHM at rest: the sound loads */
#define BROWSE_MS 1500u          /* .. and the list closes */
static struct {
    uint8_t place, stop[PL_N];   /* the place on the screen; each place's stop, kept */
    uint8_t held;                /* the place whose button is down (PL_N: none) */
    uint8_t held_new, jumped, reveal;   /* it opened with this press; a key jumped; the list is shown */
    uint32_t held_t;
    uint8_t hot, pop;            /* the column last turned (4: none); HOME: its value over the screen */
    uint32_t hot_t, knob_t[4];
    struct { uint8_t on, pending, eng; uint16_t idx; uint32_t t; } br;   /* browsing the sounds */
    char toast[28];
    uint32_t toast_t;
} uix = {.held = PL_N, .hot = 4u};

static void fmt3(char *b, int32_t v)             /* "041" */
{
    b[0] = (char)('0' + v / 100 % 10); b[1] = (char)('0' + v / 10 % 10); b[2] = (char)('0' + v % 10); b[3] = 0;
}
static void ui_message(const char *s)            /* a message in the header for a moment */
{
    str_cpy(uix.toast, s, sizeof uix.toast);
    uix.toast_t = fm1_ms;
}
static void ui_say(const char *a, const char *b)
{
    char m[28];
    str_cpy(m, a, sizeof m);
    str_cpy(m + str_len(m), b, sizeof m - str_len(m));
    ui_message(m);
}

/* ----------------------------------------------------------- places --- */
static uint32_t ui_stop(uint32_t p)
{
    ui_place_t pl = place_of(p);
    return pl.n ? uix.stop[p] % pl.n : 0u;
}
static void ui_open(uint32_t p)
{
    uix.place = (uint8_t)p;
    uix.hot = 4u;
    uix.pop = 0;
}
static void ui_turn_page(uint32_t p, int32_t d)  /* the place's next (d > 0) or previous stop, round */
{
    ui_place_t pl = place_of(p);
    if (pl.n)
        uix.stop[p] = (uint8_t)(((int32_t)ui_stop(p) + d % pl.n + pl.n) % pl.n);
    uix.hot = 4u;
}
/* the field knob k edits now: a HOME macro, the stop's, or none */
static const ui_field_t *ui_knob_field(uint32_t k)
{
    static const ui_field_t none = F0;
    ui_place_t pl = place_of(uix.place);
    if (uix.place == PL_HOME)
        return &HOME_F[eng_tint(TSEL->eng_req)][k];
    return pl.n ? &pl.stop[ui_stop(uix.place)].f[k] : &none;
}

/* ----------------------------------------------------------- browse --- */
static void browse_load(void)
{
    if (!uix.br.pending)
        return;
    uix.br.pending = 0;
    if (uix.br.eng != TSEL->eng_req)
        engine_load_of(TSEL, uix.br.eng, uix.br.idx);
    else
        apply_preset(uix.br.idx);
}
static void browse_turn(int engine, int32_t s)
{
    uint32_t n;
    if (!uix.br.on) {
        uix.br.on = 1;
        uix.br.eng = (uint8_t)(eng_ok(TSEL->eng_req) ? TSEL->eng_req : ENGI_PROPHET);
        uix.br.idx = user_of(TSEL) != USER_NONE ? 0u : TSEL->preset;
    }
    if (engine) {
        uix.br.eng = (uint8_t)eng_step(uix.br.eng, s);
        uix.br.idx = uix.br.eng == ENGI_PROPHET ? 1u : 0u;   /* (set_engine_of: Sequential's first program) */
    } else {
        n = ENGINES[uix.br.eng]->npresets;
        if (n)
            uix.br.idx = (uint16_t)(((int32_t)uix.br.idx + s % (int32_t)n + (int32_t)n) % (int32_t)n);
    }
    uix.br.pending = 1;
    uix.br.t = fm1_ms;
    uix.pop = 0;
}

/* ------------------------------------------------------------ input --- */
static uint32_t ui_btn_bit(uint32_t b) { return 1u << panel.btn[b]; }
static void ui_octave(uint32_t b)
{
    char m[16] = "OCTAVE ";
    uint32_t both = ui_btn_bit(B_OCTDN) | ui_btn_bit(B_OCTUP);
    if ((fm1_in.buttons & both) == both)
        song.octave = 0;
    else
        song.octave = (int8_t)clamp(song.octave + (b == B_OCTUP ? 1 : -1), -3, 3);
    if (song.octave > 0)
        m[7] = '+';
    fmt_int(m + str_len(m), song.octave);
    if (uix.place != PL_HOME || uix.pop || uix.br.on)
        ui_message(m);
}
static void ui_button_down(uint32_t b)
{
    uint32_t p = place_of_button(b);
    if (uix.br.on) {                                   /* a button closes the browsing (its sound loads now) */
        browse_load();
        uix.br.on = 0;
    }
    if (b == B_OCTDN || b == B_OCTUP) {
        ui_octave(b);
        return;
    }
    if (b == B_REC) {                                  /* latch the notes held, or let them go */
        TSEL->p[P_AHOLD] = (int16_t)!TSEL->p[P_AHOLD];
        return;
    }
    if (p >= PL_N)
        return;
    uix.held_new = uix.place != p;
    if (uix.held_new)
        ui_open(p);
    uix.pop = 0;
    uix.held = (uint8_t)p;
    uix.held_t = fm1_ms;
    uix.jumped = uix.reveal = 0;
}
static void ui_button_up(uint32_t b)
{
    uint32_t p = place_of_button(b);
    if (p >= PL_N || p != uix.held)
        return;
    if (!uix.held_new && !uix.jumped && !uix.reveal)   /* a tap on the open place: its next page */
        ui_turn_page(p, 1);
    uix.held = PL_N;
    uix.reveal = 0;
}
static void ui_keys(uint32_t notes)                    /* a key while a place's button is held: jump there */
{
    uint8_t s[27];
    uint32_t k, n;
    ui_place_t pl;
    if (uix.held >= PL_N || !notes)
        return;
    pl = place_of(uix.held);
    n = place_entries(&pl, s, 27u);
    for (k = 0; k < 27u && !((notes >> k) & 1u); k++)
        ;
    if (k < n) {
        uix.stop[uix.held] = s[k];
        ui_open(uix.held);
        uix.jumped = 1;
    }
}
static void ui_knob(uint32_t k, int32_t s)
{
    const ui_field_t *f = ui_knob_field(k);
    const param_desc_t *d = field_desc(f);
    uint32_t dt = fm1_ms - uix.knob_t[k];
    int32_t range;
    if (!d)
        return;
    range = d->max - d->min;
    uix.knob_t[k] = fm1_ms;
    if (dt < 60u && range >= 40)                       /* turned fast: bigger steps */
        s *= range >= 120 ? 4 : 2;
    field_step(f, s);
    uix.hot = (uint8_t)k;
    uix.hot_t = fm1_ms;
    uix.pop = uix.place == PL_HOME;
}
static void ui_timers(void)
{
    uint32_t now = fm1_ms;
    if (uix.pop && now - uix.hot_t >= POP_MS)
        uix.pop = 0;
    if (uix.br.pending && now - uix.br.t >= LOAD_MS)
        browse_load();
    if (uix.br.on && !uix.br.pending && now - uix.br.t >= BROWSE_MS)
        uix.br.on = 0;
    if (uix.held < PL_N && !uix.reveal && now - uix.held_t >= HOLD_MS[settings_hold % 4u]) {
        ui_place_t pl = place_of(uix.held);
        uix.reveal = pl.n > 1u;
    }
    if (uix.toast[0] && now - uix.toast_t >= TOAST_MS)
        uix.toast[0] = 0;
}

static void ui_input(void)
{
    uint32_t released, pressed = fm1_input_edges(&released), notes = fm1_input_note_edges(), k;
    int32_t s;
    if (boot_work.state) {                       /* maintenance: its own OCT- / OCT+ answers only */
        for (k = 0; k < NE; k++) (void)panel_enc(k);
        panel_moved = 0;
        boot_work_input(pressed);
        return;
    }
    kb_mask = 0;                                 /* a place's button held: the keys choose, they play no notes */
    for (k = 0; k < PL_N; k++)
        kb_mask |= ui_btn_bit(button_of_place(k));
    if (scr_input(pressed, notes)) return;
    for (k = 0; k < NB; k++) {
        if (pressed & ui_btn_bit(k)) ui_button_down(k);
        if (released & ui_btn_bit(k)) ui_button_up(k);
    }
    ui_keys(notes);
    if ((s = panel_enc(EN_SELECT)) && uix.place != PL_HOME)
        ui_turn_page(uix.place, s);
    if ((s = panel_enc(EN_PRESET)))
        browse_turn(0, s);
    if ((s = panel_enc(EN_ALGO)))
        browse_turn(1, s);
    for (k = 0; k < 4u; k++)
        if ((s = panel_enc(EN_K1 + k)))
            ui_knob(k, s);
    ui_timers();
    autosave_poll(pressed || notes || fm1_in.buttons || fm1_in.notes);
}

/* ------------------------------------------------------------- view --- */
static void ui_sound_name(char *b)               /* the sound's name: its user slot's, or its factory preset's */
{
    uint32_t u = user_of(TSEL);
    const engine_t *e = ENGINES[eng_idx(TSEL->eng_req)];
    if (u != USER_NONE)
        up_name(u, b);
    else if (e->npresets && TSEL->preset < e->npresets)
        str_cpy(b, eng_preset(e, TSEL->preset)->name, 22);
    else
        str_cpy(b, "INIT", 22);
}
static void vm_col_of(vm_col_t *c, const ui_field_t *f)
{
    const param_desc_t *d = field_desc(f);
    int32_t v, r;
    if (!d)
        return;
    c->on = 1;
    str_cpy(c->label, field_label(f, d), sizeof c->label);
    field_text(f, d, c->val, c->unit);
    v = field_get(f);
    r = d->max - d->min;
    if (r <= 0) {
        c->bar = BAR_NONE;
    } else if (r <= 3) {
        c->bar = BAR_STEPS; c->a = (int16_t)(v - d->min); c->b = (int16_t)(r + 1);
    } else if (d->fmt == F_ENUM) {
        c->bar = BAR_TICK; c->a = (int16_t)((v - d->min) * 44 / r);
    } else if (d->min < 0 && d->max > 0) {
        int32_t m = d->max > -d->min ? d->max : -d->min, x = 25 + v * 25 / m;
        c->bar = BAR_BIPOLAR; c->a = (int16_t)(x < 25 ? x : 25); c->b = (int16_t)(x < 25 ? 25 : x);
    } else {
        c->bar = BAR_FILL; c->b = (int16_t)((v - d->min) * 50 / r);
    }
}
static void vm_state(char *b, uint32_t n)        /* what changes the notes now: arp, scale, chords, latch */
{
    const track_t *t = TSEL;
    b[0] = 0;
#define ADD(s) do { if (b[0]) str_cpy(b + str_len(b), " \xb7 ", n - str_len(b)); str_cpy(b + str_len(b), (s), n - str_len(b)); } while (0)
    if (t->p[P_AMODE]) {
        char a[24] = "ARP ";
        str_cpy(a + 4, N_AMODE[clamp(t->p[P_AMODE], 0, (int32_t)NELEM(N_AMODE) - 1)], 8);
        str_cpy(a + str_len(a), " ", 2);
        str_cpy(a + str_len(a), N_DIV[clamp(t->p[P_ARATE], 0, (int32_t)NELEM(N_DIV) - 1)], 8);
        ADD(a);
    }
    if (t->p[P_QUANT]) {
        char a[24];
        str_cpy(a, N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)NELEM(N_SCALE) - 1)], 10);
        str_cpy(a + str_len(a), " ", 2);
        str_cpy(a + str_len(a), N_NOTE[t->p[P_ROOT] % 12], 4);
        ADD(a);
    }
    if (t->p[P_CHRD]) {
        char a[16] = "CHORD ";
        str_cpy(a + 6, N_CHRD[clamp(t->p[P_CHRD], 0, (int32_t)NELEM(N_CHRD) - 1)], 8);
        ADD(a);
    }
    if (t->p[P_AHOLD])
        ADD("LATCH");
#undef ADD
}
static void vm_build(ui_vm_t *v)
{
    uint32_t i, e = eng_tint(TSEL->eng_req);
    ui_place_t pl = place_of(uix.place);
    memset(v, 0, sizeof *v);
    v->eng = (uint8_t)e;
    v->hot = uix.hot;
    str_cpy(v->toast, uix.toast, sizeof v->toast);
    if (uix.br.on) {                                   /* browsing: the sounds around the one chosen */
        const engine_t *en = ENGINES[uix.br.eng];
        uint32_t n = en->npresets;
        v->mode = VM_BROWSE;
        v->bsrc = (uint8_t)eng_tint(uix.br.eng);
        str_cpy(v->head, ENG_TAG[v->bsrc], sizeof v->head);
        str_cpy(v->sub, "FACTORY", sizeof v->sub);
        for (i = 0; i < 5u; i++) {
            int32_t j = (int32_t)uix.br.idx + (int32_t)i - 2;
            char *r = v->brow[i];
            if (j < 0 || j >= (int32_t)n) continue;
            fmt3(r, j);
            r[3] = r[4] = ' ';
            str_cpy(r + 5, eng_preset(en, (uint32_t)j)->name, sizeof v->brow[i] - 5u);
        }
        fmt3(v->bcount, (int32_t)uix.br.idx);
        str_cpy(v->bcount + str_len(v->bcount), " / ", 4);
        fmt_int(v->bcount + str_len(v->bcount), n ? (int32_t)n - 1 : 0);
        return;
    }
    if (uix.held < PL_N && uix.reveal) {               /* the held button's list, on the keys too */
        ui_place_t hp = place_of(uix.held);
        uint8_t s[27];
        uint32_t n = place_entries(&hp, s, 27u), cur = 0;
        for (i = 0; i < n; i++)
            if (s[i] <= ui_stop(uix.held)) cur = i;
        v->mode = VM_REVEAL;
        str_cpy(v->head, hp.name, sizeof v->head);
        if (uix.held == PL_EDIT) str_cpy(v->sub, ENG_TAG[e], sizeof v->sub);
        v->cur = (uint8_t)cur;
        v->first = (uint8_t)(cur > 3u && n > VM_ROWS ? (cur - 3u < n - VM_ROWS ? cur - 3u : n - VM_ROWS) : 0u);
        v->nrows = (uint8_t)(n - v->first < VM_ROWS ? n - v->first : VM_ROWS);
        for (i = 0; i < v->nrows; i++)
            str_cpy(v->row[i], entry_name(&hp, s[v->first + i]), sizeof v->row[i]);
        v->keys = n >= 32u ? 0xFFFFFFFFu : (1u << n) - 1u;
        return;
    }
    if (uix.place == PL_HOME) {
        if (uix.pop && uix.hot < 4u) {                 /* a macro turned: its name and value over the screen */
            const ui_field_t *f = &HOME_F[e][uix.hot];
            const param_desc_t *d = field_desc(f);
            v->mode = VM_POP;
            if (d) {
                vm_col_of(&v->col[0], f);
                if (v->col[0].unit[0]) {
                    str_cpy(v->col[0].val + str_len(v->col[0].val), " ", 2);
                    str_cpy(v->col[0].val + str_len(v->col[0].val), v->col[0].unit, 6);
                }
                str_cpy(v->sub, field_label(f, d), sizeof v->sub);
                v->sweep = (uint16_t)(d->max > d->min ? (field_get(f) - d->min) * 270 / (d->max - d->min) : 0);
            }
            ui_sound_name(v->name);
            return;
        }
        v->mode = VM_HOME;
        str_cpy(v->head, ENG_TAG[e], sizeof v->head);
        if (user_of(TSEL) != USER_NONE) {
            str_cpy(v->sub, "USER ", sizeof v->sub);
            up_slot_label(v->sub + 5, user_of(TSEL));
        } else {
            str_cpy(v->sub, "FACTORY ", sizeof v->sub);
            fmt3(v->sub + 8, TSEL->preset);
        }
        if (song.octave) {
            str_cpy(v->oct, song.octave > 0 ? "OCT +" : "OCT ", sizeof v->oct);
            fmt_int(v->oct + str_len(v->oct), song.octave);
        }
        ui_sound_name(v->name);
        vm_state(v->state, sizeof v->state);
        for (i = 0; i < 4u; i++) {
            const ui_field_t *f = &HOME_F[e][i];
            const param_desc_t *d = field_desc(f);
            if (d && d->max > d->min)
                v->macro[i] = (uint8_t)((field_get(f) - d->min) * 50 / (d->max - d->min));
        }
        return;
    }
    str_cpy(v->head, pl.name, sizeof v->head);
    if (uix.place == PL_SAVE) {
        v->mode = VM_SAVE;
        ui_sound_name(v->name);
        return;
    }
    v->mode = VM_PAGE;
    if (pl.n) {
        uint32_t s = ui_stop(uix.place);
        str_cpy(v->sub, stop_title(&pl, s), sizeof v->sub);
        stop_dots(&pl, s, &v->ndots, &v->dot);
        for (i = 0; i < 4u; i++)
            vm_col_of(&v->col[i], &pl.stop[s].f[i]);
        pic_build(v, &pl.stop[s]);
    }
}

/* ------------------------------------------------------------- LEDs --- */
/* each LED's level now: 0 off, 1 dim, 2 mid, 3 full (ids: buttons 0..13, keys 14..40) */
#define UI_LEDS 41u
static uint8_t ui_led_lv[UI_LEDS];
static void ui_led_levels(void)
{
    const track_t *t = TSEL;
    uint32_t k, fx = 0;
    memset(ui_led_lv, 0, sizeof ui_led_lv);
    for (k = 0; k < FX_SLOTS; k++)
        fx |= t->p[P_FXTYPE + k * FX_PARAMS] != FX_OFF;
    ui_led_lv[panel.btn[B_ARP]] = t->p[P_AMODE] ? (t->arp_note ? 3u : 1u) : 0u;   /* (the arp: its notes flash) */
    ui_led_lv[panel.btn[B_SCL]] = t->p[P_QUANT] ? 1u : 0u;
    ui_led_lv[panel.btn[B_SEQ]] = t->p[P_CHRD] ? 1u : 0u;
    ui_led_lv[panel.btn[B_FX]] = fx ? 1u : 0u;
    ui_led_lv[panel.btn[B_REC]] = t->p[P_AHOLD] ? 3u : 0u;
    ui_led_lv[panel.btn[button_of_place(uix.place)]] = 3u;   /* the place on the screen: full */
    if (uix.held < PL_N) {                                    /* the held button's entries on the keys */
        ui_place_t pl = place_of(uix.held);
        uint8_t s[27];
        uint32_t n = place_entries(&pl, s, 27u), cur = 0;
        for (k = 0; k < n; k++)
            if (s[k] <= ui_stop(uix.held)) cur = k;
        for (k = 0; k < n; k++)
            ui_led_lv[14u + k] = k == cur ? 3u : 1u;
    } else {
        uint32_t snap = t->p[P_QUANT] == Q_SNAP && t->p[P_SCALE] < (int32_t)SCALE_LEGACY && settings_lights != LIGHTS_OFF;
        for (k = 0; k < 27u; k++) {                           /* the keys down; SNAP: the scale dim, its root mid */
            uint32_t pc = (uint32_t)(53 + (int32_t)k - t->p[P_ROOT] + 120) % 12u;   /* (key 0: F, MIDI 53) */
            if ((fm1_in.notes >> k) & 1u) ui_led_lv[14u + k] = 3u;
            else if (snap && !pc) ui_led_lv[14u + k] = 2u;
            else if (snap && (SCALE_MASK[t->p[P_SCALE]] >> pc) & 1u) ui_led_lv[14u + k] = 1u;
        }
    }
}
static void ui_leds(void)
{
    static uint8_t col[UI_LEDS], row[UI_LEDS], mapped;
    uint32_t c, r, id;
    if (!mapped) {                                            /* (hal/fm1_input.h: LED rows 1..4 of the matrix) */
        for (id = 0; id < UI_LEDS; id++) row[id] = 0;
        for (c = 0; c < FM1_NCOL; c++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][c] >= 0 && (uint32_t)FM1_KEYMAP[r][c] < UI_LEDS && !row[FM1_KEYMAP[r][c]]) {
                    col[FM1_KEYMAP[r][c]] = (uint8_t)c;
                    row[FM1_KEYMAP[r][c]] = (uint8_t)r;
                }
        mapped = 1;
    }
    ui_led_levels();
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = fm1_led_dim[0][c] = fm1_led_dim[1][c] = fm1_led_dim[2][c] = 0;
    fm1_led_dim_mask[0] = settings_lights == LIGHTS_OFF ? 0xFFu : 7u;   /* keys dim: 1/8 */
    fm1_led_dim_mask[1] = 3u;                                           /* buttons dim: 1/4 */
    fm1_led_dim_mask[2] = 1u;                                           /* mid: 1/2 */
    for (id = 0; id < UI_LEDS; id++) {
        uint8_t bit = (uint8_t)(1u << row[id]);
        if (!row[id] || !ui_led_lv[id]) continue;
        if (ui_led_lv[id] == 3u) fm1_led[col[id]] |= bit;
        else if (ui_led_lv[id] == 2u) fm1_led_dim[2][col[id]] |= bit;
        else fm1_led_dim[id < 14u ? 1u : 0u][col[id]] |= bit;
    }
}

/* ------------------------------------------------------------- draw --- */
static void draw_uboot(void)                     /* OCT- + OCT+ held: the seconds left, letting go cancels */
{
    char d[2] = {(char)('0' + ui.uboot % 10u), 0};
    lcd_fill(0, 0, 240, 240, T_BG);
    draw_text_box(0, 50, 240, TY_TX, "Update mode in", T_TEXT, 1);
    draw_text_box(0, 80, 240, TY_HR, d, T_THEME, 1);
    draw_text_box(0, 170, 240, TY_LB, "Let go to cancel", T_MID, 1);
}

static void ui_draw(void)
{
    static ui_vm_t vm, last;
    gfx_frame(1);
    if (boot_work.state) {                       /* maintenance and its errors: never asleep, never covered */
        scrn.idle = fm1_ms;
        if (ui.force) boot_work_draw();
    } else if (scr_frame()) {
        if (ui.uboot) {
            if (ui.force) draw_uboot();
        } else {
            if (theme_engine(eng_tint(TSEL->eng_req)))
                ui.force = 1;
            vm_build(&vm);
            if (ui.force || memcmp(&vm, &last, sizeof vm)) {
                vm_draw(&vm);
                last = vm;
            }
        }
    }
    ui.force = 0;
    gfx_frame(0);
    scr_shown();
}
