/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Shared, synchronous progress for boot maintenance. No timer IRQ, audio or
 * normal UI loop is needed: flush the warning before any flash write begins.
 * Errors remain on screen until acknowledged; callers retain their write guards. */
static struct {
    const char *title;
    uint16_t done, total;
    uint8_t state;                             /* 0 idle, 1 working, 2 failed, 3 wipe confirmation */
    uint8_t presets;                           /* 0 other maintenance, 1 migration, 2 wiping */
} boot_work;
static int shared_wipe(void);                  /* confirmed recovery from preset migration failure: shared_presets.c */

#define TL_MAX 4u
static uint32_t text_lines(const char *s, char (*l)[32], uint32_t m, int32_t w, const aafont_t *f)
{
    uint32_t k = 0;
    while (*s && k < m) {
        uint32_t i, cut = 0;
        while (*s == ' ' || *s == '\n')
            s++;
        for (i = 1; s[i - 1] && i < 32u; i++) {
            str_cpy(l[k], s, i + 1u);
            if (text_w(f, l[k]) > w)
                break;
            if (!s[i] || s[i] == ' ' || s[i] == '\n')
                cut = i;
            if (s[i] == '\n')
                break;
        }
        if (!cut) {                                     /* (one long word: as much of it as fits) */
            for (cut = 1; s[cut] && cut < 31u; cut++) {
                str_cpy(l[k], s, cut + 2u);
                if (text_w(f, l[k]) > w)
                    break;
            }
        }
        str_cpy(l[k++], s, cut + 1u);
        s += cut;
    }
    return k;
}

/* (large type) a text centred, wrapped onto the lines it needs (nothing cut); returns the y under it */
static int32_t boot_lines(int32_t y, const aafont_t *f, const char *s, uint16_t c, int32_t step)
{
    char l[4][32];
    uint32_t n = text_lines(s, l, 3u, 228, f), i;
    for (i = 0; i < n; i++, y += step)
        draw_text_box(6, (uint32_t)y, 228, f, l[i], c, 1);
    return y;
}
/* the left (right: 1) button beside the keys as a small keycap at (x, y) with its arrow; returns its right edge */
static int32_t boot_cap(int32_t x, int32_t y, int right)
{
    int32_t i;
    cv_rrect(x, y, 22, 18, 4, T_LIT, T_BG);
    for (i = 0; i < 5; i++)                             /* the arrow: 5 columns, 1 to 9 px tall, pointing out */
        cv_rect(right ? x + 13 - i : x + 8 + i, y + 9 - i, 1, 1 + 2 * i, T_TEXT);
    return x + 22;
}
static void boot_keys(const char *dn, const char *up)   /* the two buttons beside the keys and what they do, at the foot */
{
    cv_begin(240, 26, T_BG);
    if (dn)
        cv_text(boot_cap(8, 4, 0) + 6, ty_y(13, TY_HD), TY_HD, dn, T_TEXT);
    if (up) {
        cv_text(232 - 22 - 6 - text_w(TY_HD, up), ty_y(13, TY_HD), TY_HD, up, T_TEXT);
        boot_cap(232 - 22, 4, 1);
    }
    cv_blit(0, 240 - 30);
}
static void boot_work_draw(void)
{
    char count[24];
    int32_t y;
    lcd_fill(0, 0, 240, 240, T_BG);
    draw_text_box(12, 12, 216, TY_LB, "Startup maintenance", T_MID, 1);
    y = boot_lines(40, TY_TX, boot_work.title, T_TEXT, 26);
    if (boot_work.state == 3u) {
        y = boot_lines(y + 12, TY_TX, "Wipe all user presets?", T_THEME, 26);
        y = boot_lines(y + 8, TY_LB, "Every engine's. No undo.", T_TEXT, 20);
        boot_lines(y + 2, TY_LB, "Projects and settings stay.", T_MID, 20);
        boot_keys("BACK", "WIPE");
    } else if (boot_work.state == 2u) {
        y = boot_lines(y + 12, TY_TX, "Could not finish", T_THEME, 26);
        boot_lines(y + 8, TY_LB, "Restart to retry", T_TEXT, 20);
        boot_keys(boot_work.presets ? "WIPE" : 0, boot_work.presets == 1u ? "KEEP" : "CONTINUE");
    } else {
        uint32_t width = boot_work.total ? 200u * boot_work.done / boot_work.total : 0u;
        fmt_int(count, boot_work.done);
        uint32_t n = str_len(count);
        str_cpy(count + n, " / ", sizeof count - n);
        fmt_int(count + str_len(count), boot_work.total);
        draw_text_box(12, (uint32_t)y + 10, 216, TY_VL, count, T_THEME, 1);
        y += 10 + TY_VL->h + 8;
        lcd_fill(20, y, 200, 8, T_LINE);
        if (width) lcd_fill(20, y, width, 8, T_THEME);
        y = boot_lines(y + 22, TY_TX, boot_work.state ? "Keep power connected" : "Complete", T_TEXT, 26);
        boot_lines(y + 4, TY_LB, boot_work.state ? "Do not switch off." : "Starting up...", T_MID, 20);
    }
    lcd_sync();                                 /* physically visible before erases/programs */
#ifdef FM1_IRQ_TARGET
    fm1_wdt_feed();
#endif
}
static void boot_work_begin(const char *title, uint32_t total, uint32_t presets)
{
    scr_wake_now();
    boot_work.title = title;
    boot_work.done = 0;
    boot_work.total = (uint16_t)total;
    boot_work.state = 1;
    boot_work.presets = (uint8_t)presets;
    boot_work_draw();
}
static void boot_work_progress(uint32_t done)
{
    boot_work.done = (uint16_t)(done > boot_work.total ? boot_work.total : done);
    boot_work_draw();
}
static void boot_work_end(int failed)
{
    boot_work.state = failed ? 2u : 0u;
    boot_work_draw();
    ui.force = 1;
}
/* Input is consumed by this modal screen until the operation has finished.
 * Ignore simultaneous OCT keys: entering a confirmation never confirms it. */
static void boot_work_input(uint32_t pressed)
{
    uint32_t dn = 1u << panel.btn[B_OCTDN], up = 1u << panel.btn[B_OCTUP];
    uint32_t keys = pressed & (dn | up);
    if ((fm1_in.buttons & (dn | up)) == (dn | up)) return;
    if (boot_work.state == 2u) {
        if (keys == dn && boot_work.presets) { boot_work.state = 3; ui.force = 1; }
        else if (keys == up) { boot_work.state = 0; ui.force = 1; }
    } else if (boot_work.state == 3u) {
        if (keys == dn) { boot_work.state = 2; ui.force = 1; }
        else if (keys == up) (void)shared_wipe();
    }
}
#define BOOT_WORK_BEGIN(title, total) boot_work_begin(title, total, 0)
#define BOOT_PRESET_BEGIN(title, total) boot_work_begin(title, total, 1)
#define BOOT_WIPE_BEGIN(title, total) boot_work_begin(title, total, 2)
#define BOOT_WORK_PROGRESS(done) boot_work_progress(done)
#define BOOT_WORK_END(failed) boot_work_end(failed)
