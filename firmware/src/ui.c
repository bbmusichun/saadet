/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* No interface yet. The screen stays blank while the keys play the sound (seq.c) and MASTER sets the volume
 * (main.c); the panel's buttons and knobs have no job. Only the system's own screens draw: startup maintenance
 * (ui_boot.c), the update countdown below, UPDATE (ota_hw.c) and the crash report (main.c). */
static struct {
    uint8_t force;               /* the screen to be drawn again whole */
    uint8_t uboot;               /* main.c: seconds left before UPDATE MODE (OCT- + OCT+ held), 0 = none */
} ui;

/* gfx.c's type roles: Rubik, or the pixel face a stored preference asks for (favorites.c row 15, byte 30 bit 6) */
static uint32_t ty_pixel(void) { return (favorites.factory[15][30] & 64u) != 0u; }

/* status text (a save, an import refused, USB reconnecting): nowhere to show it yet */
static void ui_say(const char *a, const char *b) { (void)a; (void)b; }
static void ui_message(const char *s) { (void)s; }

#include "screen.c"
#include "ui_boot.c"

static void ui_input(void)
{
    uint32_t pressed = fm1_input_edges(0), notes = fm1_input_note_edges(), k;
    if (boot_work.state) {                       /* maintenance: its own OCT- / OCT+ answers only */
        for (k = 0; k < NE; k++) (void)panel_enc(k);
        panel_moved = 0;
        boot_work_input(pressed);
        return;
    }
    if (scr_input(pressed, notes)) return;
    for (k = 0; k < NE; k++) (void)panel_enc(k);   /* (no knob has a job: its turns go) */
    autosave_poll(pressed || notes || fm1_in.buttons || fm1_in.notes);
}

static void ui_leds(void)                        /* every LED dark: no button has a job */
{
    uint32_t c;
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = fm1_led_dim[0][c] = fm1_led_dim[1][c] = fm1_led_dim[2][c] = 0;
}

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
    gfx_frame(1);
    if (boot_work.state) {                       /* maintenance and its errors: never asleep, never covered */
        scrn.idle = fm1_ms;
        if (ui.force) boot_work_draw();
    } else if (scr_frame() && ui.force) {
        if (ui.uboot) draw_uboot();
        else lcd_fill(0, 0, 240, 240, T_BG);
    }
    ui.force = 0;
    gfx_frame(0);
    scr_shown();
}
