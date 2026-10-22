/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Single-synth UI fixture shared by retained DSP/control tests. */
#include "synth_ui_harness.c"
static void btn_down(uint32_t label) { fm1_in.buttons |= 1u << panel.btn[label]; host_pressed |= 1u << panel.btn[label]; }
static void btn_up(uint32_t label) { fm1_in.buttons &= ~(1u << panel.btn[label]); }
static void key_down(uint32_t k) { fm1_in.notes |= 1u << k; host_notes |= 1u << k; keyboard_block(); }
static void key_up(uint32_t k) { fm1_in.notes &= ~(1u << k); keyboard_block(); }
static void frames(uint32_t ms) { uint32_t i; for (i = 0; i < ms / 16u; i++) frame(); }
static uint32_t gates(void)                        /* voices of every track with their key down */
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NTRK; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].gate != 0;
    return n;
}
static void go_title(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, title); i++)
        ;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
}

static int check(const char *what,int ok){printf("%s: %s\n",what,ok?"ok":"FAIL");return !ok;}
