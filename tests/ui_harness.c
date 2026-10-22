/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The UI on the host: the whole instrument (synth_harness.c) with its screen kept, the text on it linted and the input
 * played as a script (press, ui_hold, turn, key_down / key_up, frames). Frames save as PNG at the panel's own
 * 240 x 240 pixels when UI_PNG names a directory (ui_png).
 * The lint keeps a model of the text on the screen: the text boxes a canvas drew land where the canvas is blitted and
 * replace whatever text that canvas covered; a fill clears its area. On the screen no text may be cut by its canvas,
 * leave the screen or overlap other text (a box under a question, flag 16, is covered on purpose). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void lint_begin(void);
static void lint_text(int32_t x0, int32_t y0, int32_t x1, int32_t y1, const char *s, uint32_t flags);
static void lint_blit(uint32_t x, uint32_t y);
static void lint_whole(uint32_t x, uint32_t y, uint32_t r0, uint32_t fill);
#define GFX_HOOK_BEGIN() lint_begin()
#define GFX_HOOK_TEXT(x0, y0, x1, y1, s, flags) lint_text(x0, y0, x1, y1, s, flags)
#define GFX_HOOK_BLIT(x, y, r0) ((void)(r0), lint_blit(x, y))
#define GFX_HOOK_WHOLE(x, y, r0, fill) lint_whole(x, y, r0, fill)
#define GFX_HOOK_PIXELS(n) ((void)0)
#include "synth_harness.c"

/* ------------------------------------------------------------- lint --- */
#define LINT_N 128u
typedef struct { int16_t x0, y0, x1, y1; uint32_t flags; char s[40]; } lint_box_t;
static lint_box_t lint_pend[LINT_N], lint_scr[LINT_N];
static uint32_t lint_np, lint_ns;

static void lint_begin(void) { lint_np = 0; }
static void lint_text(int32_t x0, int32_t y0, int32_t x1, int32_t y1, const char *s, uint32_t flags)
{
    lint_box_t *b;
    if (lint_np >= LINT_N) return;
    b = &lint_pend[lint_np++];
    b->x0 = (int16_t)x0; b->y0 = (int16_t)y0; b->x1 = (int16_t)x1; b->y1 = (int16_t)y1; b->flags = flags;
    snprintf(b->s, sizeof b->s, "%s", s);
}
static void lint_clear(int32_t x, int32_t y, int32_t w, int32_t h)   /* the text an area drawn over held: gone */
{
    uint32_t i, k = 0;
    for (i = 0; i < lint_ns; i++) {
        const lint_box_t *b = &lint_scr[i];
        if (b->x1 > x && b->x0 < x + w && b->y1 > y && b->y0 < y + h) continue;
        lint_scr[k++] = *b;
    }
    lint_ns = k;
}
static void lint_blit(uint32_t x, uint32_t y)              /* the canvas (cv_w x cv_h) lands at (x, y) */
{
    uint32_t i;
    lint_clear((int32_t)x, (int32_t)y, (int32_t)cv_w, (int32_t)cv_h);
    for (i = 0; i < lint_np && lint_ns < LINT_N; i++) {
        lint_box_t b = lint_pend[i];
        b.x0 = (int16_t)(b.x0 + (int32_t)x); b.x1 = (int16_t)(b.x1 + (int32_t)x);
        b.y0 = (int16_t)(b.y0 + (int32_t)y); b.y1 = (int16_t)(b.y1 + (int32_t)y);
        lint_scr[lint_ns++] = b;
    }
    lint_np = 0;
}
static void lint_whole(uint32_t x, uint32_t y, uint32_t r0, uint32_t fill)   /* gfx_fill: (w | h << 16, c | 1 << 16) */
{
    if (fill >> 16) lint_clear((int32_t)x, (int32_t)y, (int32_t)(r0 & 0xFFFFu), (int32_t)(r0 >> 16));
}
static uint32_t lint_n(void) { return lint_ns; }
static int lint_has(const char *s)
{
    for (uint32_t i = 0; i < lint_ns; i++)
        if (!strcmp(lint_scr[i].s, s)) return 1;
    return 0;
}
/* the problems of the text on the screen now, printed under `where`: 0 = none */
static uint32_t lint_check(const char *where)
{
    uint32_t i, j, bad = 0;
    for (i = 0; i < lint_ns; i++) {
        const lint_box_t *a = &lint_scr[i];
        if (a->x0 < 0 || a->y0 < 0 || a->x1 > 240 || a->y1 > 240) {
            printf("  %s: \"%s\" leaves the screen (%d,%d)-(%d,%d)\n", where, a->s, a->x0, a->y0, a->x1, a->y1);
            bad++;
        }
        if (a->flags & 2u) { printf("  %s: \"%s\" is cut by its canvas\n", where, a->s); bad++; }
        for (j = i + 1u; j < lint_ns; j++) {
            const lint_box_t *b = &lint_scr[j];
            if ((a->flags | b->flags) & 16u) continue;
            if (a->x1 > b->x0 && a->x0 < b->x1 && a->y1 > b->y0 && a->y0 < b->y1) {
                printf("  %s: \"%s\" overlaps \"%s\"\n", where, a->s, b->s);
                bad++;
            }
        }
    }
    return bad;
}

/* ----------------------------------------------------------- screen --- */
static uint16_t ui_px(uint32_t x, uint32_t y) { uint16_t v = host_screen[y * 240u + x]; return (uint16_t)(v >> 8 | v << 8); }
static uint32_t ui_count(uint16_t c)                       /* the screen's pixels of colour c */
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < 240u * 240u; i++) n += ui_px(i % 240u, i / 240u) == c;
    return n;
}
static uint32_t png_crc(uint32_t crc, const uint8_t *p, uint32_t n)
{
    static uint32_t t[256];
    if (!t[1])
        for (uint32_t i = 0; i < 256u; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1u ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
    crc = ~crc;
    while (n--) crc = t[(crc ^ *p++) & 255u] ^ (crc >> 8);
    return ~crc;
}
static void png_chunk(FILE *f, const char *type, const uint8_t *d, uint32_t n)
{
    uint8_t h[8] = {(uint8_t)(n >> 24), (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n,
                    (uint8_t)type[0], (uint8_t)type[1], (uint8_t)type[2], (uint8_t)type[3]};
    uint32_t c = png_crc(png_crc(0, h + 4, 4), d, n);
    uint8_t t[4] = {(uint8_t)(c >> 24), (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c};
    fwrite(h, 1, 8, f); fwrite(d, 1, n, f); fwrite(t, 1, 4, f);
}
/* the screen as <$UI_PNG>/<name>.png (RGB, the zlib stream in stored blocks): nothing without UI_PNG */
static void ui_png(const char *name)
{
    enum { ROW = 1 + 240 * 3, RAW = ROW * 240, BLK = 65535 };
    static uint8_t raw[RAW], z[2 + RAW + 5 * (RAW / BLK + 1) + 4];
    const char *dir = getenv("UI_PNG");
    char path[512];
    uint32_t i, n = 0, a = 1, b = 0;
    FILE *f;
    if (!dir) return;
    for (uint32_t y = 0; y < 240u; y++) {
        raw[y * ROW] = 0;
        for (uint32_t x = 0; x < 240u; x++) {
            uint16_t c = ui_px(x, y);
            uint8_t *p = &raw[y * ROW + 1 + x * 3];
            p[0] = (uint8_t)((c >> 11) * 255u / 31u); p[1] = (uint8_t)(((c >> 5) & 63u) * 255u / 63u);
            p[2] = (uint8_t)((c & 31u) * 255u / 31u);
        }
    }
    z[n++] = 0x78; z[n++] = 0x01;
    for (i = 0; i < RAW; i += BLK) {
        uint32_t len = RAW - i < BLK ? RAW - i : BLK;
        z[n++] = i + len >= RAW;
        z[n++] = (uint8_t)len; z[n++] = (uint8_t)(len >> 8); z[n++] = (uint8_t)~len; z[n++] = (uint8_t)(~len >> 8);
        memcpy(z + n, raw + i, len); n += len;
    }
    for (i = 0; i < RAW; i++) { a = (a + raw[i]) % 65521u; b = (b + a) % 65521u; }
    z[n++] = (uint8_t)(b >> 8); z[n++] = (uint8_t)b; z[n++] = (uint8_t)(a >> 8); z[n++] = (uint8_t)a;
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    f = fopen(path, "wb");
    if (!f) { printf("  ui_png: cannot write %s\n", path); return; }
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    static const uint8_t ihdr[13] = {0, 0, 0, 240, 0, 0, 0, 240, 8, 2, 0, 0, 0};
    fwrite(sig, 1, 8, f);
    png_chunk(f, "IHDR", ihdr, 13);
    png_chunk(f, "IDAT", z, n);
    png_chunk(f, "IEND", 0, 0);
    fclose(f);
}

/* ------------------------------------------------------------ input --- */
/* button `label` (panel.c B_*) held for ms, then let go */
static void ui_hold(uint32_t label, uint32_t ms)
{
    fm1_in.buttons |= 1u << panel.btn[label]; host_pressed |= 1u << panel.btn[label];
    frames(ms);
    fm1_in.buttons &= ~(1u << panel.btn[label]); host_released |= 1u << panel.btn[label];
    frame();
}
/* one frame's draw cost: the pixels it sent to the panel */
static uint32_t ui_frame_cost(void)
{
    host_px_sent = 0;
    frame();
    return host_px_sent;
}
