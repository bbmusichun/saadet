/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* HOME's key and chord (from Melodee's Stage band): the chord the notes played make, its notes, the keys sounding.
 * The notes are the selected part's as played (seq.c live_last / live_held: the keys, the chord keys and MIDI, after
 * the scale), so the name holds while an arp plays them. Held: bright; let go: the last
 * voicing stays, faded; nothing played yet: the key. A tuning has no chord names: its notes alone. */

/* a note's name: "C#4"; in a tuning its degree from middle C and its period ("D3", "D3+1") */
static void note_name(char *b, uint32_t n)
{
    if (micro_active(TSEL)) {
        int32_t d = (int32_t)n - 60, count = micro_scale(TSEL)->count, cycle = micro_floor(d, count);
        b[0] = 'D';
        fmt_int(b + 1, d - cycle * count + 1);
        if (cycle) {
            uint32_t at = str_len(b);
            b[at] = cycle < 0 ? '-' : '+';
            fmt_int(b + at + 1, cycle < 0 ? -cycle : cycle);
        }
        return;
    }
    str_cpy(b, N_NOTE[n % 12u], 4);
    fmt_int(b + str_len(b), (int32_t)(n / 12u) - 1);
}

static const struct {
    uint16_t iv;                                     /* bit i: i semitones over the root */
    char q[8];
} CHORDS[] = {                                       /* simplest first: a chord off its bass takes the first that fits */
    {0x091, ""}, {0x089, "m"}, {0x049, "dim"}, {0x111, "aug"}, {0x0A1, "sus4"}, {0x085, "sus2"},
    {0x491, "7"}, {0x891, "maj7"}, {0x489, "m7"}, {0x449, "m7b5"}, {0x249, "dim7"}, {0x4A1, "7sus4"},
    {0x291, "6"}, {0x289, "m6"}, {0x095, "add9"}, {0x08D, "madd9"}, {0x0B1, "add11"}, {0x0A9, "madd11"},
    {0x889, "mM7"}, {0x511, "7#5"}, {0x451, "7b5"}, {0x911, "maj7#5"}, {0x849, "dimM7"},
    {0x495, "9"}, {0x895, "maj9"}, {0x48D, "m9"}, {0x4A5, "9sus4"}, {0x295, "6/9"}, {0x28D, "m6/9"},
    {0x493, "7b9"}, {0x499, "7#9"}, {0x4D1, "7#11"}, {0x591, "7b13"}, {0x8D1, "maj7#11"},
    /* 11ths and 13ths, also without the 5th or the 9th */
    {0x4B5, "11"}, {0x4AD, "m11"}, {0x4A9, "m11"}, {0x42D, "m11"},
    {0x695, "13"}, {0x691, "13"}, {0x615, "13"}, {0x611, "13"},
    {0x6AD, "m13"}, {0x68D, "m13"}, {0x689, "m13"}, {0x60D, "m13"},
    {0xA95, "maj13"}, {0xA91, "maj13"}, {0xA15, "maj13"},
    /* 7ths and 9ths without the 5th */
    {0x411, "7"}, {0x811, "maj7"}, {0x409, "m7"}, {0x415, "9"}, {0x815, "maj9"}, {0x40D, "m9"},
    {0x413, "7b9"}, {0x419, "7#9"}, {0x851, "maj7#11"},
};
#define NCHORDS (sizeof CHORDS / sizeof CHORDS[0])

/* the chord of pitch classes pcs, *root its root: on the bass when it makes one, else the simplest on another root
 * (shown "/bass"); 0 = none */
static const char *chord_of(uint32_t pcs, uint32_t bass, uint32_t *root)
{
    uint32_t i, k, best = NCHORDS;
    for (i = 0; i < 12u; i++) {
        uint32_t r = (bass + i) % 12u, iv = ((pcs >> r) | (pcs << (12u - r))) & 0xFFFu;
        if (!((pcs >> r) & 1u))
            continue;
        for (k = 0; k < best; k++)
            if (CHORDS[k].iv == iv) {
                best = k;
                *root = r;
                break;
            }
        if (!i && best < NCHORDS)
            break;                                   /* on the bass: no slash */
    }
    return best < NCHORDS ? CHORDS[best].q : 0;
}

/* the lowest of n notes (note[] holds the first 8) that fit in w px of font f, "C4 E4 G4", " .." when some are left
 * out; returns how many are shown */
static uint32_t notes_fit(char *b, const uint8_t *note, uint32_t n, const aafont_t *f, int32_t w)
{
    uint32_t m, i;
    for (m = n < 8u ? n : 8u; m > 1u; m--) {
        b[0] = 0;
        for (i = 0; i < m; i++) {
            if (i)
                str_cpy(b + str_len(b), " ", 2);
            note_name(b + str_len(b), note[i]);
        }
        if (m < n)
            str_cpy(b + str_len(b), " ..", 4);
        if (text_w(f, b) <= w)
            return m;
    }
    note_name(b, note[0]);
    return 1;
}

/* the key: "D Dorian" (a natural minor: "A minor") */
static void key_name(char *b, uint32_t n)
{
    const char *t = SCALE_TITLE[clamp(TSEL->p[P_SCALE], 0, (int32_t)SCALE_TOTAL - 1)];
    if (!memcmp(t, "Natural ", 8))
        t += 8;
    str_cpy(b, N_NOTE[TSEL->p[P_ROOT] % 12], 4);
    str_cpy(b + str_len(b), " ", 2);
    str_cpy(b + str_len(b), t, n - str_len(b));
}
