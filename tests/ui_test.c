/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The interface on the host (ui_harness.c): every page of every place with each engine passes the lint (nothing cut,
 * off the screen or overlapping); the buttons open their places and turn their pages; a held button lists what is
 * inside and a key jumps there without playing; the knobs edit, and a change from elsewhere reaches the screen; HOME's
 * macros, the octave, the latch, the LEDs, browsing the sounds and the engines; the system's own screens; an idle
 * frame sends nothing. UI_PNG=dir saves the screens. */
#include "ui_harness.c"

static int bad;
static void lint_all(const char *where)          /* every stop of every place, as the screen shows it now */
{
    char w[48];
    for (uint32_t p = 0; p < PL_N; p++) {
        ui_place_t pl = place_of(p);
        for (uint32_t s = 0; s < (pl.n ? pl.n : 1u); s++) {
            ui_open(p);
            uix.stop[p] = (uint8_t)s;
            frame();
            snprintf(w, sizeof w, "%s %s %u", where, pl.name, (unsigned)s);
            if (lint_check(w)) bad |= check(w, 0);
        }
    }
    ui_open(PL_HOME);
    frame();
}
static void hold_down(uint32_t b) { fm1_in.buttons |= ui_btn_bit(b); host_pressed |= ui_btn_bit(b); frame(); }
static void hold_up(uint32_t b) { fm1_in.buttons &= ~ui_btn_bit(b); host_released |= ui_btn_bit(b); frame(); }

int main(void)
{
    char name[24], want[16];
    const char *u;
    power_on();
    frames(200);
    ui_sound_name(name);
    bad |= check("HOME: the engine and the sound", lint_has("P5") && uix.place == PL_HOME && lint_check("home") == 0u);
    ui_png("home");
    bad |= check("an idle frame sends nothing", ui_frame_cost() == 0u);

    /* every page with every engine (the descriptors' labels and names change with the engine) */
    for (uint32_t n = 0; n < NENG_SHOWN; n++) {
        uint32_t e = eng_vis(n);
        engine_load_of(TSEL, e, e == ENGI_PROPHET ? 1u : 0u);
        frame();
        lint_all(ENG_TAG[eng_tint(e)]);
    }
    power_on();
    memset(uix.stop, 0, sizeof uix.stop);          /* (each place's first page, as at power-on) */
    frames(100);

    /* a place's button opens it, again turns its page; SELECT turns it too */
    press(B_ARP);
    bad |= check("ARP opens", uix.place == PL_ARP && lint_has("ARP") && lint_has("PATTERN") && lint_has("MODE"));
    ui_png("arp");
    press(B_ARP);
    bad |= check("ARP again: its next page", ui_stop(PL_ARP) == 1u && lint_has("FEEL"));
    turn(EN_SELECT, 1);
    bad |= check("SELECT turns the page (round)", ui_stop(PL_ARP) == 0u && lint_has("PATTERN"));
    press(B_FX);
    press(B_ARP);
    bad |= check("a place keeps its page", ui_stop(PL_ARP) == 0u && uix.place == PL_ARP);

    /* a knob edits its column; a change from elsewhere (the editor, MIDI) reaches the screen */
    turn(EN_K1, 1);
    bad |= check("K1 edits the first column", TSEL->p[P_AMODE] == 1 && lint_has("UP") && uix.hot == 0u);
    turn(EN_K1, -1);                               /* (the arp off again: a key then sounds at once) */
    TSEL->p[P_AGATE] = 100;
    param_format(track_desc(TSEL, P_AGATE), 100, want, &u);
    str_cpy(want + str_len(want), u, 4);
    frame();
    bad |= check("a change from elsewhere is shown", lint_has(want));
    host_px_sent = 0;
    turn(EN_K4, 1);
    printf("  a knob step sends %u of %u pixels\n", (unsigned)host_px_sent, 240u * 240u);
    bad |= check("a knob step sends what changed only", host_px_sent > 0u && host_px_sent < 240u * 240u / 2u);

    /* the pictures show the sound: the chord's name, the scale's, a route as a sentence, the tempo; the device's
     * settings edit as the editor's preferences do; SNAP lights the scale on the keys */
    TSEL->p[P_CHRD] = 6; TSEL->p[P_ROOT] = 2; TSEL->p[P_SCALE] = 3; TSEL->p[P_QUANT] = Q_SNAP;
    TSEL->p[P_M2SRC] = 6; TSEL->p[P_M2DST] = 2; TSEL->p[P_M2AMT] = 40;
    press(B_SEQ);
    bad |= check("CHORD: the chord's name", lint_has("Dmaj7") && lint_check("chord") == 0u);
    press(B_SCL);
    bad |= check("SCALE: the scale's name", lint_has("D") && lint_has("Dorian"));
    bad |= check("SNAP: the root mid, the scale dim, the rest dark", ui_led_lv[14u + 9u] == 2u &&
                 ui_led_lv[14u + 11u] == 1u && ui_led_lv[14u + 10u] == 0u);   /* (key 9: D, 10: D#, 11: E) */
    press(B_LFO);
    uix.stop[PL_LFO] = 3;
    frame();
    bad |= check("MATRIX: a route as a sentence", lint_has("MODW \x9b CUT"));
    press(B_GLO);
    uix.stop[PL_GLO] = 0;
    frame();
    bad |= check("GLOBAL: the tempo large", lint_has("120") && lint_has("BPM"));
    uix.stop[PL_GLO] = 5;
    frame();
    turn(EN_K1, 1);
    turn(EN_K2, 1);
    bad |= check("DISPLAY: sleep and lights are the device's", scr_get() == 1u && settings_lights == LIGHTS_HIGH);
    turn(EN_K1, -1);
    turn(EN_K2, -1);
    uix.stop[PL_GLO] = 4;
    frame();
    turn(EN_K1, -1);
    bad |= check("VOICES: the budget is the device's", settings_budget == VOICE_BUDGET_DEFAULT - 1);
    turn(EN_K1, 1);
    TSEL->p[P_CHRD] = 0; TSEL->p[P_QUANT] = Q_OFF; TSEL->p[P_M2SRC] = TSEL->p[P_M2DST] = TSEL->p[P_M2AMT] = 0;
    memset(uix.stop, 0, sizeof uix.stop);

    /* a held button lists what is inside; a key jumps there and plays no note */
    press(B_LFO);
    hold_down(B_LFO);
    frames(600);
    bad |= check("held: the list", uix.reveal && lint_has("LFO") && lint_has("MATRIX") && lint_check("list") == 0u);
    bad |= check("held: its entries on the keys", ui_led_lv[14] == 3u && ui_led_lv[15] == 1u && ui_led_lv[16] == 0u);
    ui_png("list");
    key_down(1);
    frame();
    bad |= check("a key jumps", uix.stop[PL_LFO] == 3u && gates() == 0u);
    key_up(1);
    hold_up(B_LFO);
    bad |= check("letting go stays there", uix.place == PL_LFO && ui_stop(PL_LFO) == 3u && !uix.reveal);
    key_down(1);
    frame();
    bad |= check("then the keys play again", gates() > 0u);
    key_up(1);
    frames(500);

    /* HOME: a macro's value over the screen, then HOME again */
    press(B_HOME);
    turn(EN_K1, 2);
    bad |= check("HOME: a macro over the screen", uix.pop && lint_has("CUTOFF") && lint_check("pop") == 0u);
    ui_png("pop");
    frames(1400);
    bad |= check(".. until the knob rests", !uix.pop && !lint_has("CUTOFF") && lint_has("P5"));

    /* OCT- / OCT+, both at once: 0; REC latches */
    press(B_OCTUP);
    bad |= check("OCT+", song.octave == 1 && lint_has("OCT +1"));
    press(B_OCTDN); press(B_OCTDN);
    bad |= check("OCT- twice", song.octave == -1);
    hold_down(B_OCTDN);
    hold_down(B_OCTUP);
    hold_up(B_OCTUP);
    hold_up(B_OCTDN);
    bad |= check("both: octave 0", song.octave == 0);
    press(B_REC);
    bad |= check("REC latches, lit", TSEL->p[P_AHOLD] == 1 && ui_led_lv[panel.btn[B_REC]] == 3u);
    press(B_REC);
    bad |= check("REC again lets go", TSEL->p[P_AHOLD] == 0 && ui_led_lv[panel.btn[B_REC]] == 0u);
    TSEL->p[P_AMODE] = 1;
    frame();
    bad |= check("LEDs: the place full, a running arp dim", ui_led_lv[panel.btn[B_HOME]] == 3u &&
                                                              ui_led_lv[panel.btn[B_ARP]] == 1u);
    TSEL->p[P_AMODE] = 0;

    /* PRESETS browses, the sound loads at rest; ALGORITHM changes the engine and its colour */
    {
        uint32_t p0 = TSEL->preset;
        turn(EN_PRESET, 3);
        bad |= check("PRESETS: the list", uix.br.on && lint_has("FACTORY") && lint_check("browse") == 0u);
        ui_png("browse");
        frames(400);
        bad |= check(".. loads at rest", TSEL->preset == p0 + 3u);
        frames(1600);
        bad |= check(".. and closes", !uix.br.on && uix.place == PL_HOME);
        turn(EN_ALGO, 1);
        frames(400);
        bad |= check("ALGORITHM: the next engine", TSEL->eng_req == ENGI_FM6 && T_THEME == T_ENG(1));
        frames(1600);
        ui_png("home_fm6");
    }

    /* the system's own screens: the update countdown, startup maintenance */
    ui.uboot = 3; ui.force = 1;
    frame();
    bad |= check("countdown", lint_n() == 3u && lint_has("Update mode in") && lint_check("countdown") == 0u);
    ui_png("uboot");
    ui.uboot = 0; ui.force = 1;
    frame();
    bad |= check("countdown gone: the page again", !lint_has("Update mode in") && lint_has("FM6"));
    static const char *const BOOT[3] = {"boot_progress", "boot_failed", "boot_wipe"};
    for (uint32_t s = 0; s < 3u; s++) {
        boot_work.title = "Updating user presets"; boot_work.presets = 1; boot_work.done = 37; boot_work.total = 128;
        boot_work.state = (uint8_t)(s ? s + 1u : 1u);
        boot_work_draw();
        bad |= check(BOOT[s], lint_check(BOOT[s]) == 0u && lint_n() >= 3u);
        ui_png(BOOT[s]);
    }
    bad |= check("the wipe question names both buttons", lint_has("BACK") && lint_has("WIPE"));
    boot_work.state = 0; ui.force = 1;
    frame();

    /* the lint finds what it is for: an overlap, a cut; a fill clears */
    cv_begin(120, 24, T_BG);
    cv_text(0, 0, TY_RW, "OVERLAP A", T_TEXT);
    cv_text(20, 4, TY_RW, "OVERLAP B", T_TEXT);
    cv_blit(10, 100);
    bad |= check("lint: an overlap", lint_check("probe") >= 1u);
    gfx_fill(0, 0, 240, 240, T_BG);
    bad |= check("lint: a fill clears", lint_n() == 0u);
    cv_begin(40, 24, T_BG);
    cv_text(0, 0, TY_RW, "A LONG LINE", T_TEXT);
    cv_blit(0, 0);
    bad |= check("lint: a cut", lint_check("probe") == 1u);
    (void)name;

    printf(bad ? "UI TEST FAILED\n" : "UI test passed\n");
    return bad;
}
