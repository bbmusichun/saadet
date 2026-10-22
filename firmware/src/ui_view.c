/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* What the screen shows, as a view (ui_vm_t: every string and bar the frame draws), and the drawing of a view. ui.c
 * builds the view every frame and draws it only when it differs from the last one drawn, so a change from anywhere
 * (a knob, the web editor, MIDI) reaches the screen, and an idle frame sends nothing. A view is drawn whole, a band
 * of rows at a time (gfx.c cv_begin_band); the panel gets only the blocks that changed (gfx.c df_diff). */
enum { VM_PAGE, VM_HOME, VM_POP, VM_REVEAL, VM_BROWSE, VM_SAVE };
enum { BAR_NONE, BAR_FILL, BAR_BIPOLAR, BAR_TICK, BAR_STEPS };
#define VM_ROWS 7u                                  /* the held button's list: rows on the screen */
#define UI_COL_X(i) (7 + 59 * (int32_t)(i))         /* the four columns (K1..K4): each value fits 54 px */
typedef struct {
    char label[12], val[16], unit[6];
    int16_t a, b;                                   /* BAR_FILL: 0..b px; BAR_BIPOLAR: a..b around 25; BAR_TICK: at a;
                                                     * BAR_STEPS: step a of b */
    uint8_t bar, on;
} vm_col_t;
typedef struct {
    uint8_t mode, eng, hot;                         /* hot: the column last turned (4: none) */
    uint8_t ndots, dot;
    char head[12], sub[16], toast[28];
    vm_col_t col[4];
    char name[24], src[16], oct[8], state[48];      /* HOME (name: also POP, SAVE) */
    uint8_t macro[4];
    uint16_t sweep;                                 /* POP: the ring, degrees of 270 */
    uint8_t nrows, first, cur;                      /* REVEAL */
    uint32_t keys;
    char row[VM_ROWS][16];
    char brow[5][28], bcount[12];                   /* BROWSE: the sounds around the one chosen; its source's tag */
    uint8_t bsrc;
    uint8_t pic;                                    /* PAGE: its picture (ui_pics.c) and the picture's data */
    int16_t pv[24];
    char ps[4][24];
} ui_vm_t;
static void pic_draw(const ui_vm_t *v);             /* ui_pics.c */

/* -------------------------------------------------------- the parts --- */
static void vm_header(const ui_vm_t *v)
{
    if (v->toast[0]) {                              /* a message stands in for the header a moment */
        cv_text_fit(10, 8, TY_TX, v->toast, T_TEXT, T_BG, 220);
        return;
    }
    if (v->head[0]) {
        int32_t x = cv_text(10, 8, TY_HD, v->head, v->mode == VM_HOME || v->mode == VM_BROWSE ? T_THEME : T_TEXT);
        if (v->sub[0])
            cv_text_fit(x + 8, 8, TY_TX, v->sub, T_MID, T_BG, 150 - x);
    }
    for (uint32_t i = 0; v->ndots > 1u && i < v->ndots; i++)
        cv_disc(227 - 10 * (int32_t)(v->ndots - 1u - i), 16, 6, i == v->dot ? T_TEXT : T_OFF, 0);
}
static void vm_bar(int32_t x, const vm_col_t *c, uint16_t ink)
{
    int32_t i;
    cv_rect(x, 231, 50, 2, T_LINE);
    switch (c->bar) {
    case BAR_FILL: cv_rect(x, 231, c->b, 2, ink); break;
    case BAR_BIPOLAR:
        cv_rect(x + 24, 228, 1, 8, T_FILL);
        cv_rect(x + c->a, 231, c->b - c->a, 2, ink);
        break;
    case BAR_TICK: cv_rect(x + c->a, 231, 6, 2, ink); break;
    case BAR_STEPS:                                 /* a few values: one segment each, the value's lit */
        for (i = 0; i < c->b; i++) {
            int32_t w = (50 - 2 * (c->b - 1)) / c->b;
            cv_rect(x + i * (w + 2), 231, w, 2, i == c->a ? ink : T_LINE);
        }
        break;
    }
}
static void vm_cols(const ui_vm_t *v)
{
    for (uint32_t i = 0; i < 4u; i++) {
        const vm_col_t *c = &v->col[i];
        int32_t x = UI_COL_X(i), w;
        uint16_t ink = i == v->hot ? T_THEME : T_TEXT;
        const aafont_t *f = TY_VL;
        if (!c->on)
            continue;
        cv_text(x, 184, TY_LB, c->label, T_MID);
        if (text_w(f, c->val) > 54)                 /* a long name: the row face, cut if need be */
            f = TY_RW;
        w = cv_text_fit(x, 199 + TY_VL->asc - f->asc, f, c->val, ink, T_BG, 54) - x;
        if (c->unit[0] && w + 2 + text_w(TY_LB, c->unit) <= 54)
            cv_text(x + w + 2, 199 + TY_VL->asc - TY_LB->asc, TY_LB, c->unit, T_MID);
        vm_bar(x, c, i == v->hot ? T_THEME : T_FILL);
    }
}
/* the keyboard (27 keys from F) as a strip at y, h tall: the keys of `keys` lit, key `cur` in the engine's colour */
static void vm_keys(int32_t y, int32_t h, uint32_t keys, uint32_t cur)
{
    static const uint8_t BLACK = 0x2A;             /* F F# G G# A A# B: bit k = semitone k (from F) is black; C..E: 0x0A */
    int32_t white = 0;
    for (uint32_t pass = 0; pass < 2u; pass++) {   /* the white keys, then the black ones over them */
        white = 0;
        for (uint32_t k = 0; k < 27u; k++) {
            uint32_t s = k % 12u, black = s < 7u ? (BLACK >> s) & 1u : (0x0Au >> (s - 7u)) & 1u;
            uint16_t c = k == cur ? T_THEME : (keys >> k) & 1u ? T_MID : black ? T_LINE : T_WELL;
            if (!black) {
                if (!pass) cv_rect(8 + 14 * white, y, 13, h, c);
                white++;
            } else if (pass) {
                int32_t x = 8 + 14 * white - 5;
                cv_rect(x - 2, y, 13, h * 6 / 10 + 2, T_BG);
                cv_rect(x, y, 9, h * 6 / 10, c);
            }
        }
    }
}

/* ---------------------------------------------------------- screens --- */
static void vm_draw_page(const ui_vm_t *v)
{
    vm_header(v);
    pic_draw(v);
    vm_cols(v);
}
static void vm_draw_home(const ui_vm_t *v)
{
    char l1[24];
    const aafont_t *f = text_has(TY_HR, v->name) ? TY_HR : TY_NM;
    int32_t w = text_w(f, v->name), i;
    vm_header(v);
    if (!v->toast[0]) {
        cv_text_r(230, 8, TY_TX, v->oct, T_MID, T_BG);
    }
    if (w <= 220) {
        cv_text(10, 58, f, v->name, T_TEXT);
    } else {                                        /* two lines, broken at the last space that fits */
        int32_t cut = -1;
        for (i = 0; v->name[i]; i++)
            if (v->name[i] == ' ') {
                str_cpy(l1, v->name, (unsigned)i + 1u);
                if (text_w(f, l1) <= 220) cut = i;
            }
        if (cut > 0) {
            str_cpy(l1, v->name, (unsigned)cut + 1u);
            cv_text(10, 46, f, l1, T_TEXT);
            cv_text_fit(10, 46 + f->h - 4, f, v->name + cut + 1, T_TEXT, T_BG, 220);
        } else {
            cv_text_fit(10, 58, f, v->name, T_TEXT, T_BG, 220);
        }
    }
    cv_text_fit(10, 152, TY_TX, v->state, T_MID, T_BG, 220);
    for (i = 0; i < 4; i++) {
        cv_rect(UI_COL_X(i), 222, 50, 4, T_LINE);
        cv_rect(UI_COL_X(i), 222, v->macro[i], 4, T_THEME);
    }
}
static void vm_draw_pop(const ui_vm_t *v)
{
    const aafont_t *f = text_has(TY_PO, v->col[0].val) ? TY_PO : TY_HR;
    cv_arc(120 * 16, 128 * 16, 82 * 16, 6 * 16, 135, 270, T_LINE);
    if (v->sweep)
        cv_arc(120 * 16, 128 * 16, 82 * 16, 6 * 16, 135, v->sweep, T_THEME);
    cv_text(10, 8, TY_HD, v->sub, T_TEXT);
    cv_text_c(120, ty_y(128, f), f, v->col[0].val, T_TEXT, T_BG);
    cv_text_c(120, 212, TY_TX, v->name, T_MID, T_BG);
}
static void vm_draw_reveal(const ui_vm_t *v)
{
    char n[4];
    vm_header(v);
    for (uint32_t i = 0; i < v->nrows; i++) {
        int32_t y = 30 + 24 * (int32_t)i;
        fmt_int(n, (int32_t)(v->first + i + 1u));
        cv_text(10, y + 3, TY_HD, n, T_THEME);
        cv_text_fit(34, y, TY_RW, v->row[i], v->first + i == v->cur ? T_TEXT : T_MID, T_BG, 190);
    }
    vm_keys(206, 28, v->keys, v->cur);
}
static void vm_draw_browse(const ui_vm_t *v)
{
    int32_t x = 10;
    vm_header(v);
    if (!v->toast[0])
        cv_text_r(230, 8, TY_TX, v->bcount, T_MID, T_BG);
    cv_text_fit(10, 36, TY_TX, v->brow[0], T_FAINT, T_BG, 220);
    cv_text_fit(10, 60, TY_RW, v->brow[1], T_MID, T_BG, 220);
    if (v->brow[2][0]) {
        char n[4] = {v->brow[2][0], v->brow[2][1], v->brow[2][2], 0};
        cv_text(10, 106, TY_HD, n, T_THEME);
        cv_text_fit(50, 94, TY_NM, v->brow[2] + 5, T_TEXT, T_BG, 182);
    }
    cv_text_fit(10, 146, TY_RW, v->brow[3], T_MID, T_BG, 220);
    cv_text_fit(10, 174, TY_TX, v->brow[4], T_FAINT, T_BG, 220);
    for (uint32_t e = 0; e < 4u; e++) {             /* the sources ALGORITHM picks, the chosen one lit */
        const aafont_t *f = e == v->bsrc ? TY_HD : TY_TX;
        int32_t w = text_w(f, ENG_TAG[e]);
        cv_text(x, 212, f, ENG_TAG[e], e == v->bsrc ? T_TEXT : T_DIM);
        if (e == v->bsrc)
            cv_rect(x, 231, w, 2, T_THEME);
        x += w + 16;
    }
}
static void vm_draw_save(const ui_vm_t *v)
{
    vm_header(v);
    cv_text_fit(10, 40, TY_NM, v->name, T_TEXT, T_BG, 220);
}
static void vm_draw(const ui_vm_t *v)
{
    uint32_t rows = CV_MAX / 240u, top;
    for (top = 0; top < 240u; top += rows) {
        cv_begin_band(240, 240, T_BG, top);
        switch (v->mode) {
        case VM_HOME: vm_draw_home(v); break;
        case VM_POP: vm_draw_pop(v); break;
        case VM_REVEAL: vm_draw_reveal(v); break;
        case VM_BROWSE: vm_draw_browse(v); break;
        case VM_SAVE: vm_draw_save(v); break;
        default: vm_draw_page(v); break;
        }
        cv_blit_band(0, 0);
    }
}
