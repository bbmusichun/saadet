/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The UI on the host (ui_harness.c): the harness itself on the screens there are so far, the blank one and the update
 * countdown: the screen model, the lint (a cut, an overlap, a fill), the input reaching the instrument and the draw
 * cost of an idle frame. UI_PNG=dir saves the screens. */
#include "ui_harness.c"

int main(void)
{
    int bad = 0;
    power_on();
    frames(200);
    bad |= check("blank screen", ui_count(T_BG) == 240u * 240u && lint_n() == 0);
    ui_png("blank");
    bad |= check("an idle frame sends nothing", ui_frame_cost() == 0u);

    ui.uboot = 3; ui.force = 1;                    /* (main.c: OCT- and OCT+ held) */
    frame();
    bad |= check("countdown drawn", ui_count(T_BG) < 240u * 240u);
    bad |= check("countdown text", lint_n() == 3u && lint_has("Update mode in") && lint_has("3") &&
                                   lint_has("Let go to cancel"));
    bad |= check("countdown lint", lint_check("countdown") == 0u);
    ui_png("uboot");
    ui.uboot = 0; ui.force = 1;
    frame();
    bad |= check("countdown cleared", ui_count(T_BG) == 240u * 240u && lint_n() == 0u);

    /* startup maintenance (ui_boot.c): progress, a failure, the wipe question with the two buttons beside the keys */
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

    /* the lint finds what it is for: two texts overlapping, a text cut by its canvas; a fill clears them */
    cv_begin(120, 24, T_BG);
    cv_text(0, 0, TY_RW, "OVERLAP A", T_TEXT);
    cv_text(20, 4, TY_RW, "OVERLAP B", T_TEXT);
    cv_blit(10, 100);
    bad |= check("lint: an overlap", lint_n() == 2u && lint_check("probe") == 1u);
    gfx_fill(0, 0, 240, 240, T_BG);
    bad |= check("lint: a fill clears", lint_n() == 0u);
    cv_begin(40, 24, T_BG);
    cv_text(0, 0, TY_RW, "A LONG LINE", T_TEXT);
    cv_blit(0, 0);
    bad |= check("lint: a cut", lint_check("probe") == 1u);
    cv_begin(100, 24, T_BG);
    cv_blit(0, 0);
    bad |= check("lint: a canvas covers what it lands on", lint_n() == 0u);

    /* the input reaches the instrument: a key plays; buttons and knobs are taken (no job yet), the screen stays */
    gfx_fill(0, 0, 240, 240, T_BG);
    key_down(12);
    frame();
    bad |= check("a key plays", gates() > 0u);
    key_up(12);
    frames(500);
    press(B_HOME);
    ui_hold(B_EDIT, 600);
    turn(EN_K1, 3);
    turn(EN_PRESET, -2);
    bad |= check("buttons and knobs leave the screen blank", ui_count(T_BG) == 240u * 240u);

    printf(bad ? "UI HARNESS FAILED\n" : "UI harness passed\n");
    return bad;
}
