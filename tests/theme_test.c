/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The theme and the real canvas/text renderer (src/gfx.c): STAGE with each engine's colour as THEME, the test-only
 * GRAY stays gray in every token and blend, the contrast of what is read on the screen, the alpha blend, the ramp
 * cache, the faces (Archivo: metrics, tabular figures, the extras, the ellipsis, PO's digits only) and the smooth
 * shapes (segment, polyline, arc) across the canvas's bands.
 * Optional: a sheet of the theme and the faces (PPM). */
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#define __attribute__(x)
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define UI_TEST_PALETTE 1
#include "../firmware/src/gfx.c"

#define SHEET_W (240u * UI_PAL_ENTRIES)
#define SHEET_H 240u
static uint16_t sheet[SHEET_W * SHEET_H];
static unsigned channel(uint16_t c, unsigned n)
{ return n == 0 ? c >> 11 : n == 1 ? (c >> 5) & 63 : c & 31; }
static int gray(uint16_t c) { return channel(c, 0) == channel(c, 2) && channel(c, 1) == channel(c, 0) * 2u; }
static double luminance(uint16_t color)
{
    const double weight[] = {0.2126, 0.7152, 0.0722};
    double result = 0;
    for (unsigned c = 0; c < 3; c++) {
        double v = channel(color, c) / (c == 1 ? 63.0 : 31.0);
        result += weight[c] * (v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4));
    }
    return result;
}
static double contrast(uint16_t fg, uint16_t bg)
{
    double a = luminance(fg), b = luminance(bg);
    return a > b ? (a + 0.05) / (b + 0.05) : (b + 0.05) / (a + 0.05);
}
static uint16_t px_at(uint32_t x, uint32_t y) { return swap16(cv_px[(y - cv_top) * cv_w + x]); }

/* a page in the D look, drawn into the band at cv_top: every face, the tokens and the shapes */
static void specimen(void)
{
    static const int32_t curve[] = {160, 1536, 1792, 1536, 2016, 1120, 2336, 1120, 2752, 2080, 3296, 2784};
    cv_text(10, 9, TY_HD, "EDIT", T_TEXT);
    cv_text(52, 9, TY_TX, "FILTER", T_MID);
    cv_disc(217, 17, 6, T_TEXT, 0);
    cv_disc(227, 17, 6, T_OFF, 0);
    cv_poly(curve, 6, 32, T_THEME, 3);
    cv_arc(120 * 16, 112 * 16, 40 * 16, 6 * 16, 135, 153, T_THEME);
    cv_arc(120 * 16, 112 * 16, 40 * 16, 6 * 16, 288, 117, T_LINE);
    cv_text(12, 120, TY_RW, "LFO \x9b PITCH", T_TEXT);
    cv_vdash(160, 120, 168, 2, 3, T_FILL);
    cv_text(8, 184, TY_LB, "CUTOFF", T_MID);
    cv_text(8, 201, TY_VL, "72", T_THEME);
    cv_rect(8, 231, 50, 2, T_LINE);
    cv_rect(8, 231, 28, 2, T_THEME);
    cv_text(68, 184, TY_LB, "RES", T_MID);
    cv_text(68, 201, TY_VL, "38", T_TEXT);
    cv_text(128, 184, TY_LB, "ENV AMT", T_DIM);
    cv_text(128, 201, TY_VL, "54", T_DIM);
    cv_rect(188, 200, 13, 28, T_WELL);
    cv_rect(202, 200, 13, 28, T_LIT);
    cv_rect(216, 200, 13, 28, T_FILL);
    cv_text(10, 40, TY_NM, "U097\x96U128", T_FAINT);
    cv_text(140, 30, TY_HR, "14", T_TEXT);
    cv_text(170, 120, TY_PO, "7", T_TEXT);
}

int main(int argc, char **argv)
{
    static const aag_t pg[1] = {{0, 48, 0, 0, 3, 1}};   /* one glyph 'A': 3 px, alpha 0, 8, 15 */
    static const uint8_t pd[] = {0x08, 0xf0};
    const aafont_t probe = {1, 1, 'A', 'A', 0, 0, pg, pd, 0, 0, 0};
    const aafont_t *const FACES[8] = {TY_LB, TY_TX, TY_HD, TY_RW, TY_VL, TY_NM, TY_HR, TY_PO};
    double worst = 100;
    assert(NPALETTES == 1u && !strcmp(UI_PALETTES[0].name, "STAGE") && !strcmp(UI_PALETTES[UI_GRAY_INDEX].name, "GRAY"));
    for (unsigned pe = 0; pe < UI_PAL_ENTRIES * 4u; pe++) {
        unsigned p = pe / 4u, e = pe % 4u;
        const uint16_t *tok = &ux.bg;
        palette_set(p);
        theme_engine(e + 1u);
        assert(theme_engine(e) && !theme_engine(e));          /* (a change is reported once) */
        assert(T_THEME == T_ENG(e) && T_THEME == UI_PALETTES[p].eng[e]);
        if (p == UI_GRAY_INDEX) {
            for (unsigned i = 0; i < 10u; i++) assert(gray(tok[i]));   /* every token */
            assert(gray(T_THEME));
        }
        /* what is read on the screen; the generator checks the same (tools/gen_ui_palettes.py) */
        struct { uint16_t fg, bg; double min; } c[] = {
            {T_TEXT, T_BG, 15.0}, {T_MID, T_BG, 7.0}, {T_DIM, T_BG, 4.5}, {T_FAINT, T_BG, 2.5}, {T_THEME, T_BG, 4.5},
            {T_TEXT, T_WELL, 12.0}, {T_MID, T_LINE, 5.5}, {T_FILL, T_LINE, 2.0}, {T_LIT, T_WELL, 1.8},
            {T_OFF, T_BG, 1.7}, {T_LINE, T_BG, 1.2}};
        double min = 100;
        for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++) {
            double r = contrast(c[i].fg, c[i].bg);
            if (r < c[i].min) printf("%s E%u: pairing %u contrast %.2f < %.2f\n", UI_PALETTES[p].name, e, i, r, c[i].min);
            assert(r >= c[i].min);
            if (r / c[i].min < min) min = r / c[i].min;
        }
        if (min < worst) worst = min;
        printf("%-6s E%u text %.1f:1, labels %.1f:1, engine %.1f:1\n", UI_PALETTES[p].name, e,
               contrast(T_TEXT, T_BG), contrast(T_MID, T_BG), contrast(T_THEME, T_BG));
        assert(!ux.light);
        /* the blend: transparent, solid and an edge between the two, on two backgrounds */
        for (unsigned b = 0; b < 2; b++) {
            uint16_t under = b ? T_WELL : T_BG, ink = b ? T_TEXT : T_THEME;
            cv_begin(3, 1, under);
            cv_text_on(0, 0, &probe, "A", ink, under);
            assert(swap16(cv_px[0]) == under && swap16(cv_px[2]) == ink);
            for (unsigned k = 0; k < 3; k++) {
                unsigned a = channel(under, k), z = channel(ink, k), m = channel(swap16(cv_px[1]), k);
                assert(m >= (a < z ? a : z) && m <= (a > z ? a : z));   /* no halo */
            }
            if (p == UI_GRAY_INDEX) assert(gray(swap16(cv_px[1])));
        }
        for (uint32_t top = 0; top < 240u; top += CV_MAX / 240u) {   /* the specimen, band by band */
            cv_begin_band(240, 240, T_BG, top);
            specimen();
            for (unsigned y = top; y < top + cv_rows; y++)
                for (unsigned x = 0; x < 240; x++) {
                    uint16_t v = px_at(x, y);
                    if (p == UI_GRAY_INDEX) assert(gray(v));      /* every blended pixel of GRAY */
                    if (!e) sheet[y * SHEET_W + p * 240u + x] = v;
                }
        }
    }
    /* the ramp cache follows the palette (a stale ramp would blend toward the old background) */
    palette_set(0);
    cv_begin(3, 1, T_BG); cv_text(0, 0, &probe, "A", T_TEXT);
    uint16_t stage = swap16(cv_px[1]);
    palette_set(UI_GRAY_INDEX);
    cv_begin(3, 1, T_BG); cv_text(0, 0, &probe, "A", T_TEXT);
    assert(swap16(cv_px[1]) != stage);
    palette_set(0);
    theme_engine(0);
    cv_begin(3, 1, T_BG); cv_text(0, 0, &probe, "A", T_THEME);
    uint16_t p5 = swap16(cv_px[1]);
    theme_engine(1);
    cv_begin(3, 1, T_BG); cv_text(0, 0, &probe, "A", T_THEME);
    assert(swap16(cv_px[1]) != p5);                           /* (and the engine's colour) */
    /* the faces: Archivo LB 13, TX 14, HD 14, RW 18, VL 22, NM 32, HR 46, PO 92 px (tools/gen_aa_font.py --preset stage) */
    assert(AF_LB.h == 15 && AF_LB.asc == 12 && AF_TX.h == 16 && AF_TX.asc == 13 && AF_HD.h == 16 && AF_HD.asc == 13);
    assert(AF_RW.h == 20 && AF_RW.asc == 16 && AF_VL.h == 25 && AF_VL.asc == 20 && AF_NM.h == 36 && AF_NM.asc == 29);
    assert(AF_HR.h == 51 && AF_HR.asc == 41 && AF_PO.h == 101 && AF_PO.asc == 81);
    for (unsigned f = 0; f < 8u; f++) {
        assert(text_w(FACES[f], "0000") == text_w(FACES[f], "1111") && text_w(FACES[f], "1.25") == text_w(FACES[f], "8.75"));
        if (f < 6u)                                            /* (HR PO: the characters a name or a number needs) */
            assert(glyph(FACES[f], (uint8_t)ELLIPSIS) != glyph(FACES[f], '?') &&
                   glyph(FACES[f], (uint8_t)EN_DASH) != glyph(FACES[f], '?') &&
                   glyph(FACES[f], (uint8_t)ANGLE) != glyph(FACES[f], '?'));
    }
    assert(text_w(TY_HR, "abc") != text_w(TY_HR, "ABC") && glyph(TY_HR, 'a') != glyph(TY_HR, 'A'));
    assert(text_has(TY_PO, "+72.5%") && !text_has(TY_PO, "HALF"));
    assert(text_w(TY_HD, "EDIT") > text_w(TY_TX, "EDIT"));    /* (800 is wider than 500) */
    assert(text_w(TY_VL, "RATIO") <= 54 && text_w(TY_LB, "RELEASE") <= 54);   /* (fits a 60 px column, 6 px apart) */
    {
        char b[32];
        assert(!text_fit(b, sizeof b, "KICK", TY_LB, 60) && !strcmp(b, "KICK"));
        assert(text_fit(b, sizeof b, "A VERY LONG PRESET NAME", TY_LB, 60) && text_w(TY_LB, b) <= 60);
        assert(b[strlen(b) - 1] == ELLIPSIS && b[strlen(b) - 2] != ' ');
        assert(text_fit(b, sizeof b, "WWWWWWWW", TY_TX, 1) && b[0] == ELLIPSIS && !b[1]);
    }
    /* the shapes: a 2 px segment covers its two rows whole and nothing else */
    cv_begin(40, 10, T_BG);
    cv_seg(5 * 16, 5 * 16, 35 * 16, 5 * 16, 32, T_TEXT, 3u);
    assert(px_at(20, 4) == T_TEXT && px_at(20, 5) == T_TEXT && px_at(20, 3) == T_BG && px_at(20, 6) == T_BG);
    assert(px_at(3, 5) == T_BG && px_at(37, 5) == T_BG);                      /* (round caps: 1 px past each end) */
    cv_begin(40, 10, T_BG);
    cv_seg(5 * 16, 5 * 16, 35 * 16, 5 * 16, 32, T_TEXT, 0u);
    assert(px_at(4, 5) == T_BG && px_at(5, 5) == T_TEXT && px_at(35, 5) == T_BG);   /* (square ends: at the ends) */
    /* .. a polyline's joint is drawn once (no brighter bead where two segments meet) */
    {
        static const int32_t v[] = {16 * 2 + 8, 16 * 2 + 8, 16 * 20 + 8, 16 * 2 + 8, 16 * 38 + 8, 16 * 2 + 8};
        cv_begin(40, 6, T_BG);
        cv_poly(v, 3, 24, T_THEME, 3u);
        assert(px_at(20, 2) == px_at(10, 2) && px_at(20, 3) == px_at(10, 3));
    }
    /* .. a full ring is symmetric, an arc stops at its ends, and both draw the same across a band's edge */
    {
        uint16_t top_row[240], bot_row[240];
        uint32_t rows = CV_MAX / 240u;
        for (uint32_t top = 0; top < 2u * rows; top += rows) {
            cv_begin_band(240, 2u * rows, T_BG, top);
            cv_arc(120 * 16, (int32_t)rows * 16, 30 * 16, 4 * 16, 0, 360, T_TEXT);
            for (unsigned x = 0; x < 240; x++) {
                if (top) bot_row[x] = px_at(x, rows);
                else top_row[x] = px_at(x, rows - 1u);
            }
        }
        for (unsigned x = 0; x < 240; x++) assert(top_row[x] == bot_row[x]);   /* (the ring's rows mirror at the edge) */
        cv_begin(120, 120, T_BG);
        cv_arc(60 * 16, 60 * 16, 40 * 16, 6 * 16, 0, 360, T_TEXT);
        assert(px_at(100, 60) == px_at(19, 60) && px_at(60, 100) == px_at(60, 19) && px_at(60, 60) == T_BG);
        cv_begin(120, 120, T_BG);
        cv_arc(60 * 16, 60 * 16, 40 * 16, 6 * 16, 0, 90, T_TEXT);             /* 3 o'clock to 6 o'clock */
        assert(px_at(88, 88) == T_TEXT && px_at(32, 88) == T_BG && px_at(88, 32) == T_BG && px_at(32, 32) == T_BG);
    }
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb"); assert(f);
        fprintf(f, "P6\n%u %u\n255\n", (unsigned)SHEET_W, (unsigned)SHEET_H);
        for (unsigned i = 0; i < SHEET_W * SHEET_H; i++) {
            fputc(channel(sheet[i], 0) * 255 / 31, f);
            fputc(channel(sheet[i], 1) * 255 / 63, f);
            fputc(channel(sheet[i], 2) * 255 / 31, f);
        }
        fclose(f);
    }
    printf("Theme: GRAY stays gray, contrast (worst margin x%.2f), blending, ramp cache, faces and shapes passed.\n", worst);
    return 0;
}
