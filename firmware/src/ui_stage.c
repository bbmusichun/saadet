/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* HOME, the instrument (docs/design/README.md "HOME"): one sound, played. Under the header the four tone knobs; then
 * the sound itself, its name large with where it comes from under it (the factory, a user slot; a favourite's star);
 * the scope of what plays, with the chord and its notes over it while keys are held, or what a knob shapes while one
 * turns; at the foot the performance as chips, lit while they act: the scale, the chord keys, the arp, the inserts.
 * Browsing (PRESETS) shows the sound about to load in the theme's colour until it loads. */
#define ST_SOUND_Y Y_GRAPH                              /* the sound: rows 84 .. 135 */
#define ST_SOUND_H 52
#define ST_PANEL_Y (ST_SOUND_Y + ST_SOUND_H + 4)         /* the scope: rows 140 .. 207 */
#define ST_PANEL_H 68
#define ST_STATUS_Y Y_FOOT                              /* the chips: rows 212 .. 239 */
#define ST_STATUS_H H_FOOT
#define ST_KNOB_MS 1500u

static struct {
    uint8_t out;
    uint32_t knob_ms;                                   /* last HOME knob turn, for the response picture */
} stage;

static void stage_knob_touch(void) { stage.knob_ms = fm1_ms | 1u; }   /* (ui_input.c: a knob on Stage) */
static int stage_knob_active(void) { return stage.knob_ms && fm1_ms - (stage.knob_ms & ~1u) < ST_KNOB_MS; }

/* ------------------------------------------------------- the knobs --- */
/* a knob's label: the panel's, plain words for CZ-1's (its panel says R1, L1 ..) */
static const char *stage_label(uint32_t k, const param_desc_t *d)
{
    static const char *const CZ_L[4] = {"WAVE", "DCW", "DETUNE", "VIBRATO"};
    return stage_pg.scope == SC_CZ1 ? CZ_L[k & 3u] : d->label;
}
/* a knob's value as Stage shows it: the Prophet's 0..127 panel values in % (the mockup's CUTOFF 62 %) */
static void stage_value(const page_t *sp, const param_desc_t *d, int32_t v, char *val, const char **unit)
{
    if (sp->scope == SC_P5 && d->fmt == F_INT && d->min == 0 && d->max == 127) {
        fmt_int(val, (v * 100 + 63) / 127);
        *unit = "%";
        return;
    }
    param_format(d, v, val, unit);
}
/* The four cards stay visible: labels and values use the established Stage typography. */
static void stage_columns(void)
{
    const page_t *sp = stage_page();
    uint32_t c;
    for (c = 0; c < 4u; c++) {
        int16_t *vp;
        const char *unit;
        char val[12];
        const param_desc_t *d = page_desc(sp, c, &vp);
        int dim = (sp->scope == SC_CZ1 && !cz_ed_active(cz_patch[song.sel % NTRK].raw, sp->id[c])) ||
                  (sp->scope == SC_P5 && !p5_ed_active(TSEL, sp->id[c]));
        if (!d || !vp || !d->label || d->label[0] == '-') {
            draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);
            continue;
        }
        stage_value(sp, d, *vp, val, &unit);
        draw_column(c, stage_label(c, d), val, unit, dim ? T_DIM : VAL(c), d->fmt == F_ENUM && d->max < 2 ? -1 : RATIO(d, *vp),
                    ICON_NONE);
    }
}

/* ------------------------------------------------------- the panel --- */
/* the output's waveform over the panel's width, smooth: triggered on a rising zero crossing, auto-scaled, each sample
 * the mean of three, 2 px anti-aliased (gfx.c cv_vspan_aa); rows top .. top + h */
static void stage_wave(uint16_t c, int32_t top, int32_t h)
{
    int16_t *snap = scope_snap;
    uint32_t w = scope_w, i, trig = 1;
    int32_t peak = 1500, x, py = 0, cy = (top + h / 2) * 16 - 16;
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak) peak = snap[i];
        else if (-snap[i] > peak) peak = -snap[i];
    }
    for (i = 2; i < SCOPE_N - 240u; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) { trig = i; break; }
    for (x = 0; x < 224; x++) {
        uint32_t k = trig + (uint32_t)x;
        int32_t v = ((int32_t)snap[k - 1u] + 2 * snap[k] + snap[k + 1u]) / 4;
        int32_t y = cy - v * (h / 2 - 4) * 16 / peak;
        cv_vspan_aa(8 + x, x ? py : y, y, c);
        py = y;
    }
}

/* the notes the selected track played (live_last: its last voicing, held or let go), the chord they make, in front of
 * what is on the canvas (cv_over): the root and quality on the first line, notes on the second.
 * Held: the chord in text, the notes in the track's colour; let go: both faded into the panel. 0 none, 1 let go, 2 held */
static int stage_notes(int32_t x1)
{
    uint32_t bits[4], i, n = 0, pcs = 0, root = 0, bass, held = 0, k = song.sel % NTRK;
    uint8_t note[8];
    char b[48];
    const char *q;
    uint16_t c, cn;
    int32_t x;
    fm1_irq_off();                                      /* one coherent ISR snapshot, nothing drawn meanwhile */
    for (i = 0; i < 4u; i++) {
        bits[i] = live_last[k][i];
        held |= live_held[k][i];
    }
    fm1_irq_on();
    for (i = 0; i < 128u; i++)
        if ((bits[i >> 5] >> (i & 31u)) & 1u) {
            pcs |= 1u << (i % 12u);
            if (n < sizeof note) note[n] = (uint8_t)i;
            n++;
        }
    if (!n) return 0;
    c = held ? T_TEXT : ux_mix(T_PANEL, T_THEME, 75);
    cn = held ? T_THEME : ux_mix(T_PANEL, T_THEME, 55);
    bass = note[0] % 12u;
    q = micro_active(TSEL) ? 0 : chord_of(pcs, bass, &root);
    if (q) {
        x = cv_text(10, 1, TY_VL, N_NOTE[root], c);
        x = cv_text(x + 1, 15, TY_TX, q, c);
        if (root != bass) {
            x = cv_text(x + 3, 1, TY_VL, "/", c);
            cv_text(x, 1, TY_VL, N_NOTE[bass], c);
        }
        notes_fit(b, note, n, TY_LB, x1 - 10);
        cv_text(10, 39, TY_LB, b, cn);
    } else {
        const aafont_t *f = TY_VL;
        if (notes_fit(b, note, n, f, x1 - 10) != n) {
            f = TY_LB;
            notes_fit(b, note, n, f, x1 - 10);
        }
        cv_text(10, f == TY_VL ? 1 : 15, f, b, c);
    }
    return (int)(1u + (held != 0u));
}
/* HOME's notes alone (tests/ui_test.c): 1 = there were notes */
static int graph_notes(void) { return stage_notes(220) != 0; }

/* After a knob turns, the panel shows what it shapes behind the notes: a filter's response (cutoff,
 * resonance; the Prophet's envelope amount too), an envelope (its attack .. release); 0 = nothing to show */
enum { SP_NONE, SP_FILTER, SP_ENV };
static uint32_t stage_picture(int32_t *v)
{
    const page_t *sp = stage_page();
    uint32_t hot = ui.hot_col & 3u, id = sp->id[hot], k;
    if (!stage_knob_active())
        return SP_NONE;
    if (sp->scope == SC_P5) {
        const uint8_t *r = p5_patch_of(TSEL)->raw;
        if (id == P5_CUTOFF || id == P5_RESONANCE || id == P5_ENV_FILTER) {
            v[0] = r[P5_CUTOFF];
            v[1] = r[P5_RESONANCE];
            return SP_FILTER;
        }
        if (id == P5_RELEASE_AMP) {
            v[0] = r[P5_ATTACK_AMP]; v[1] = r[P5_DECAY_AMP]; v[2] = r[P5_SUSTAIN_AMP]; v[3] = r[P5_RELEASE_AMP];
            return SP_ENV;
        }
        return SP_NONE;
    }
    if (sp->scope != SC_ENGINE)
        return SP_NONE;
    if (id >= P_ATK && id <= P_REL) {
        for (k = 0; k < 4u; k++)
            v[k] = TSEL->p[P_ATK + k];
        return SP_ENV;
    }
    {   /* an engine's filter: its F_CUTOFF knob and the RES beside it */
        int32_t cut = -1, res = 0;
        for (k = 0; k < 4u; k++) {
            const param_desc_t *d = track_desc(TSEL, sp->id[k]);
            if (d->fmt == F_CUTOFF) cut = (int32_t)k;
            else if (str_eq(d->label, "RES")) res = TSEL->p[sp->id[k]];
        }
        if (cut < 0 || (hot != (uint32_t)cut && !str_eq(track_desc(TSEL, id)->label, "RES")))
            return SP_NONE;
        v[0] = TSEL->p[sp->id[cut]];
        v[1] = res;
        return SP_FILTER;
    }
}
/* a low-pass response: flat, the resonance's peak at the cutoff (a dotted line), then the slope; cut, res 0..127 */
static void stage_filter(int32_t cut, int32_t res, uint16_t c)
{
    int32_t x, fc = 10 + clamp(cut, 0, 127) * 200 / 127, peak = clamp(res, 0, 127) * 14 / 127;
    int32_t top = 3, bot = ST_PANEL_H - 4, y0 = 22 * 16, py = 0;
    for (x = top; x < bot; x += 5)
        cv_rect(fc, x, 1, 2, ux_mix(T_PANEL, c, 60));
    for (x = 0; x < 220; x++) {
        int32_t d = 8 + x - fc, y = y0 - peak * 16 * 64 / (64 + d * d) + (d > 0 ? d * 10 : 0);
        y = clamp(y, top * 16, bot * 16);
        cv_vspan_aa(8 + x, x ? py : y, y, c);
        py = y;
    }
}

static void stage_panel(void)
{
    uint32_t sel = song.sel % NTRK, pic, i;
    int32_t pv[4];
    int held = 0;
    for (uint32_t band = 0; band < ST_PANEL_H; band += CV_MAX / 240u) {
        cv_begin_band(240, ST_PANEL_H, T_BG, band);
        cv_rrect(BOX_X, 0, BOX_W, ST_PANEL_H, 8, T_PANEL, T_BG);
        cv_bg = T_PANEL;
        fm1_irq_off();
        for (i = 0; i < 4u; i++) held |= live_held[sel][i] != 0u;
        fm1_irq_on();
        pic = stage_picture(pv);
        /* behind: the waveform (dimmed while notes are held), or what the knob turning shapes */
        if (pic == SP_FILTER)
            stage_filter(pv[0], pv[1], ux_mix(T_PANEL, T_THEME, 55));
        else if (pic == SP_ENV)
            graph_adsr_v(pv[0], pv[1], pv[2], pv[3], 6, ST_PANEL_H - 8, ux_mix(T_PANEL, T_THEME, 55));
        else
            stage_wave(held ? ux_mix(T_PANEL, T_THEME, 35) : T_THEME, 4, ST_PANEL_H - 8);
        cv_over = 1;                                    /* in front: blended over it */
        stage_notes(pic ? 168 : 226);
        if (pic) {                                      /* the value turning, at the top right */
            int16_t *vp;
            const char *unit;
            char val[12];
            const page_t *sp = stage_page();
            const param_desc_t *d = page_desc(sp, ui.hot_col & 3u, &vp);
            if (d && vp) {
                stage_value(sp, d, *vp, val, &unit);
                str_cpy(val + str_len(val), unit, 4);
                cv_text_r(228, 6, TY_TX, val, T_THEME, T_PANEL);
            }
        }
        cv_over = 0;
        cv_blit_band(0, ST_PANEL_Y);
    }
}

/* ------------------------------------------------------- the sound --- */
static const char *browser_cat_name(uint32_t c);        /* ui_browser.c */
static int browser_shown_fav(void);
static void menu_words(char *d, const char *s, uint32_t n);   /* ui_menu.c */
/* where the sound shown comes from: "Factory 034", "User U012", "Native P007" (an engine's own slot); its category */
static void stage_source(uint32_t src, uint32_t k, char *b, uint32_t n)
{
    char tag[8], nm[16];
    if (src == USER_GENERAL || src == USER_NATIVE_P5 || src == USER_NATIVE_FM || src == USER_NATIVE_CZ) {
        entry_label(src, k, tag, nm);
        str_cpy(b, src == USER_GENERAL ? "User " : "Native ", n);
        str_cpy(b + str_len(b), tag, n - str_len(b));
    } else {
        str_cpy(b, "Factory ", n);
        tag[0] = (char)('0' + (k + 1u) / 100u % 10u);
        tag[1] = (char)('0' + (k + 1u) / 10u % 10u);
        tag[2] = (char)('0' + (k + 1u) % 10u);
        tag[3] = 0;
        str_cpy(b + str_len(b), tag, n - str_len(b));
    }
    str_cpy(b + str_len(b), " \xB7 ", n - str_len(b));
    str_cpy(b + str_len(b), browser_cat_name(entry_cat(src, k)), n - str_len(b));
}
static void stage_sound(void)
{
    static uint32_t drawn;
    uint32_t k, src = browse_shown(&k), sig;
    int pending = browse_pending(), fav = browser_shown_fav();
    char name[32], from[40], tag[8];
    const aafont_t *f = TY_VL;
    if (pending) {
        entry_label(src, k, tag, name);
        if (src == ENGI_PROPHET) str_cpy(name, eng_preset(ENGINES[ENGI_PROPHET], k)->name, sizeof name);
        if (!name[0]) str_cpy(name, tag, sizeof name);
    } else {
        sound_name(TSEL, name);
    }
    stage_source(src, k, from, sizeof from);
    sig = str_hash(str_hash((uint32_t)pending * 3u + (uint32_t)fav * 5u + ux.gen * 977u + ux.pal * 31u, name), from);
    if (!ui.force && sig == drawn)
        return;
    drawn = sig;
    if (text_w(f, name) > 220) f = TY_TX;
    cv_begin(240, ST_SOUND_H, T_BG);
    cv_free_text(10, ty_y(18, f), f, name, pending ? T_THEME : T_TEXT, T_BG, 220);
    {
        int32_t x = cv_free_text(10, ty_y(40, TY_LB), TY_LB, from, T_MID, T_BG, 196);
        if (fav) cv_icon_mid(x + 8, 40, 12, ICON_X_STAR, T_THEME, T_BG);
    }
    cv_blit(0, ST_SOUND_Y);
}

/* ------------------------------------------------------- the chips --- */
/* the performance at a glance: what each chip names acts now (lit) or waits (dim). The chord, arp and insert chips
 * keep their words; the key's chip takes the rest of the row (a long scale name ellipsised) */
static void stage_status(void)
{
    static const char *const ARP[] = {"Arp", "Up", "Down", "Up/Dn", "Rand", "Order", "Rept"};
    static uint32_t drawn;
    char w[4][16], b[16];
    uint32_t on[4], i, nfx = 0, sig;
    int32_t x = BOX_X, cw[4], rest = BOX_W - 3 * 4;
    for (i = 0; i < FX_SLOTS; i++)
        nfx += TSEL->p[P_FXTYPE + i * FX_PARAMS] != FX_OFF && TSEL->p[P_FXMIX + i * FX_PARAMS] != 0;
    str_cpy(w[0], N_NOTE[TSEL->p[P_ROOT] % 12], sizeof w[0]);
    str_cpy(w[0] + str_len(w[0]), " ", 2);
    menu_words(b, N_SCALE[clamp(TSEL->p[P_SCALE], 0, SCALE_TOTAL - 1u)], sizeof b);
    str_cpy(w[0] + str_len(w[0]), b, sizeof w[0] - str_len(w[0]));
    on[0] = TSEL->p[P_SCALE] != 0 || TSEL->p[P_TRANS] != 0;
    str_cpy(w[1], "Chord", sizeof w[1]);
    on[1] = TSEL->p[P_CHRD] != 0;
    str_cpy(w[2], ARP[clamp(TSEL->p[P_AMODE], 0, 6)], sizeof w[2]);
    on[2] = TSEL->p[P_AMODE] != 0;
    str_cpy(w[3], "FX", sizeof w[3]);
    if (nfx) { w[3][2] = ' '; w[3][3] = (char)('0' + nfx); w[3][4] = 0; }
    on[3] = nfx != 0;
    sig = ux.gen * 977u + ux.pal * 31u;
    for (i = 0; i < 4u; i++)
        sig = str_hash(sig + on[i] * 7u, w[i]);
    if (!ui.force && sig == drawn)
        return;
    drawn = sig;
    for (i = 1; i < 4u; i++)
        rest -= cw[i] = text_w(TY_LB, w[i]) + 16;
    cw[0] = text_w(TY_LB, w[0]) + 16 < rest ? text_w(TY_LB, w[0]) + 16 : rest;
    x += (rest - cw[0]) / 2;                            /* (the row centred) */
    cv_begin(240, ST_STATUS_H, T_BG);
    for (i = 0; i < 4u; i++) {
        uint16_t fill = on[i] ? ux_mix(T_BG, T_THEME, 22) : T_BG, line = on[i] ? T_THEME : T_LINE;
        cv_rrect(x, 2, cw[i], 24, 7, line, T_BG);
        cv_rrect(x + 1, 3, cw[i] - 2, 22, 6, fill, line);
        if (text_w(TY_LB, w[i]) <= cw[i] - 16)
            cv_text_c(x + cw[i] / 2, ty_y(14, TY_LB), TY_LB, w[i], on[i] ? T_THEME : T_DIM, fill);
        else
            cv_free_text(x + 8, ty_y(14, TY_LB), TY_LB, w[i], on[i] ? T_THEME : T_DIM, fill, cw[i] - 16);
        x += cw[i] + 4;
    }
    cv_blit(0, ST_STATUS_Y);
}

static void stage_frame(void)
{
    lcd_fill(0, Y_LABEL, 4, CARD_H, T_BG);
    lcd_fill(236, Y_LABEL, 4, CARD_H, T_BG);
    lcd_fill(0, Y_LABEL + CARD_H, 240, ST_SOUND_Y - Y_LABEL - CARD_H, T_BG);
    lcd_fill(0, ST_SOUND_Y + ST_SOUND_H, 240, ST_PANEL_Y - ST_SOUND_Y - ST_SOUND_H, T_BG);
    lcd_fill(0, ST_PANEL_Y + ST_PANEL_H, 240, ST_STATUS_Y - ST_PANEL_Y - ST_PANEL_H, T_BG);
}
/* The scope is drawn last, as in the approved UI, while the LCD sends it. */
static void stage_draw(void)
{
    stage_columns();
    if (!ui.force && (ui.frame & 1u)) return;
    stage_sound();
    stage_status();
    stage_panel();
}

static int32_t st_mid_x(int32_t cx, const aafont_t *f, const char *s, int32_t maxw)
{
    int32_t w = text_w(f, s);
    return cx - (w < maxw ? w : maxw) / 2;
}
static void stage_knobs(void) { stage.out=(uint8_t)stage_knob_active(); if(stage.out)stage_columns(); }
