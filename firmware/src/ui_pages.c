/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The places the buttons open and what is in them. A place is a row of stops; a stop is one page of one object (an
 * FX slot, the matrix) with the four fields K1..K4 edit. Pressing the place's button again or turning SELECT moves
 * along the row; the header names the object (or, in a place of one object, the page) and its dots count the
 * object's pages. Holding the button lists the objects (or the pages) on the keys. */
enum { PL_HOME, PL_FX, PL_SCL, PL_ENV, PL_LFO, PL_EDIT, PL_GLO, PL_SAVE, PL_ARP, PL_SEQ, PL_PLAY, PL_N };
/* the picture between the header and the values (ui_pics.c) */
enum { PIC_NONE, PIC_ENV, PIC_LFO, PIC_MATRIX, PIC_ARP, PIC_SCALE, PIC_CHORD, PIC_VOICES, PIC_FX, PIC_READOUT };
typedef struct { const char *obj, *page; uint8_t pic; ui_field_t f[4]; } ui_stop_t;   /* obj "": the place is one object */
typedef struct { const char *name; const ui_stop_t *stop; uint8_t n; } ui_place_t;
#define FT(id, l) {FK_TRACK, (uint8_t)(id), l}
#define FG(id, l) {FK_GLOBAL, (uint8_t)(id), l}
#define FP(id, l) {FK_P5, (uint8_t)(id), l}
#define FD(id, l) {FK_DEV, (uint8_t)(id), l}
#define F0 {FK_NONE, 0, 0}

static const ui_stop_t ST_FX[] = {
#define FX_SLOT(k, n) \
    {n, "SOUND", PIC_FX, {FT(P_FXTYPE + (k) * FX_PARAMS, "TYPE"), FT(P_FXMIX + (k) * FX_PARAMS, "MIX"), \
                          FT(P_FXRATE + (k) * FX_PARAMS, 0), FT(P_FXCHAR + (k) * FX_PARAMS, 0)}}, \
    {n, "TONE", PIC_FX, {FT(P_FXTONE + (k), "TONE"), F0, F0, F0}}
    FX_SLOT(0, "SLOT 1"), FX_SLOT(1, "SLOT 2"), FX_SLOT(2, "SLOT 3"),
#undef FX_SLOT
};
static const ui_stop_t ST_SCL[] = {
    {"", "SCALE", PIC_SCALE, {FT(P_SCALE, "SCALE"), FT(P_ROOT, "ROOT"), FT(P_QUANT, "KEYS"), FT(P_TRANS, "SHIFT")}},
    {"", "MPC", PIC_SCALE, {FT(P_MPCDEG, "DEGREE"), F0, F0, F0}},
};
static const ui_stop_t ST_ENV_MOD[] = {             /* engines with envelopes of their own: the common ENV modulates */
    {"MOD ENV", "SHAPE", PIC_ENV, {FT(P_ATK, "ATTACK"), FT(P_DEC, "DECAY"), FT(P_SUS, "SUSTAIN"), FT(P_REL, "RELEASE")}},
    {"MOD ENV", "AMOUNTS", PIC_ENV, {FT(P_ED_FLT, "FILTER"), FT(P_ED_PIT, "PITCH"), FT(P_ED_SHP, "SHAPE"), F0}},
};
static const ui_stop_t ST_ENV_AMP[] = {             /* (SID: the common ENV is the amp) */
    {"AMP ENV", "SHAPE", PIC_ENV, {FT(P_ATK, "ATTACK"), FT(P_DEC, "DECAY"), FT(P_SUS, "SUSTAIN"), FT(P_REL, "RELEASE")}},
    {"AMP ENV", "AMOUNTS", PIC_ENV, {FT(P_ED_FLT, "FILTER"), FT(P_ED_PIT, "PITCH"), FT(P_ED_SHP, "SHAPE"), F0}},
};
static const ui_stop_t ST_LFO[] = {
    {"LFO", "WAVE", PIC_LFO, {FT(P_LRATE, "RATE"), FT(P_LWAVE, "WAVE"), FT(P_LFADE, "FADE IN"), FT(P_LTRIG, "RESTART")}},
    {"LFO", "SYNC", PIC_LFO, {FT(P_LSYNC, "SYNC"), FT(P_LPHASE, "PHASE"), FT(P_LPOL, "RANGE"), F0}},
    {"LFO", "DEPTH", PIC_LFO, {FT(P_LD_PIT, "PITCH"), FT(P_LD_FLT, "FILTER"), FT(P_LD_SHP, "SHAPE"), FT(P_LD_AMP, "AMP")}},
    {"MATRIX", "AMOUNTS", PIC_MATRIX, {FT(P_M1AMT, "ROUTE 1"), FT(P_M2AMT, "ROUTE 2"), FT(P_M3AMT, "ROUTE 3"),
                                       FT(P_M4AMT, "ROUTE 4")}},
#define ROUTE(k, n) {"MATRIX", n, PIC_MATRIX, {FT(P_M1SRC + (k) * 3, "SOURCE"), FT(P_M1DST + (k) * 3, "DEST"), \
                                               FT(P_M1AMT + (k) * 3, "AMOUNT"), F0}}
    ROUTE(0, "ROUTE 1"), ROUTE(1, "ROUTE 2"), ROUTE(2, "ROUTE 3"), ROUTE(3, "ROUTE 4"),
#undef ROUTE
};
static const ui_stop_t ST_EDIT[] = {                /* the engine's eight macros (its native panel: phase 3) */
    {"", "MACROS 1", PIC_NONE, {FT(P_E0, 0), FT(P_E1, 0), FT(P_E2, 0), FT(P_E3, 0)}},
    {"", "MACROS 2", PIC_NONE, {FT(P_E4, 0), FT(P_E5, 0), FT(P_E6, 0), FT(P_E7, 0)}},
};
static const ui_stop_t ST_GLO[] = {
    {"", "TEMPO", PIC_READOUT, {FG(G_BPM, "TEMPO"), FG(G_CLOCK, "CLOCK"), FG(G_SWING, "SWING"), F0}},
    {"", "OUTPUT", PIC_READOUT, {FT(P_LEVEL, "LEVEL"), FT(P_PAN, "PAN"), FT(P_SPRD, "SPREAD"), F0}},
    {"", "TUNING", PIC_READOUT, {FG(G_A4, "A4"), FG(G_TUNE, "FINE"), F0, F0}},
    {"", "MIDI", PIC_READOUT, {FG(G_ROUTE, "IN CH"), FG(G_DRUMCH, "OUT CH"), F0, F0}},
    {"", "VOICES", PIC_VOICES, {FD(DEV_BUDGET, "BUDGET"), F0, F0, F0}},
    {"", "DISPLAY", PIC_READOUT, {FD(DEV_SLEEP, "SLEEP"), FD(DEV_LIGHTS, "LIGHTS"), F0, F0}},
};
static const ui_stop_t ST_ARP[] = {
    {"", "PATTERN", PIC_ARP, {FT(P_AMODE, "MODE"), FT(P_ARATE, "RATE"), FT(P_AOCT, "OCTAVES"), FT(P_AGATE, "GATE")}},
    {"", "FEEL", PIC_ARP, {FT(P_ASWING, "SWING"), FT(P_APROB, "CHANCE"), FT(P_AORDER, "ORDER"), FG(G_BPM, "TEMPO")}},
};
static const ui_stop_t ST_SEQ[] = {
    {"", "CHORD", PIC_CHORD, {FT(P_CHRD, "CHORD"), FT(P_VOIC, "VOICING"), FT(P_STRUM, "STRUM"), FT(P_STROKE, "STROKE")}},
};
static const ui_stop_t ST_PLAY[] = {
    {"", "VOICE", PIC_VOICES, {FT(P_VOICE, "MODE"), FT(P_GLIDE, "GLIDE"), FT(P_GLMODE, "GLIDE BY"), FT(P_PRIO, "PRIORITY")}},
    {"", "UNISON", PIC_VOICES, {FT(P_DETUNE, "DETUNE"), FT(P_ALLOC, "ASSIGN"), F0, F0}},
};

/* the engine's place in the colours and the tags: 0 PROPHET, 1 FM6, 2 CZ-1, 3 SID */
static uint32_t eng_tint(uint32_t e) { return e == ENGI_FM6 ? 1u : e == ENGI_CZ ? 2u : e == 3u ? 3u : 0u; }
static const char *const ENG_TAG[4] = {"P5", "FM6", "CZ-1", "SID"};

static ui_place_t place_of(uint32_t p)
{
    static const char *const NAME[PL_N] = {"HOME", "FX", "SCALE", "ENV", "LFO", "EDIT", "GLOBAL", "SAVE", "ARP",
                                           "CHORD", "PLAY"};
    ui_place_t r = {NAME[p % PL_N], 0, 0};
#define SET(t) (r.stop = (t), r.n = (uint8_t)NELEM(t))
    switch (p) {
    case PL_FX: SET(ST_FX); break;
    case PL_SCL: SET(ST_SCL); break;
    case PL_ENV:
        if (ENGINES[eng_idx(TSEL->eng_req)]->ownenv) SET(ST_ENV_MOD); else SET(ST_ENV_AMP);
        break;
    case PL_LFO: SET(ST_LFO); break;
    case PL_EDIT: SET(ST_EDIT); break;
    case PL_GLO: SET(ST_GLO); break;
    case PL_ARP: SET(ST_ARP); break;
    case PL_SEQ: SET(ST_SEQ); break;
    case PL_PLAY: SET(ST_PLAY); break;
    }
#undef SET
    return r;
}

/* HOME's four macros, the engine's (a turn shows the name and value over the whole screen) */
static const ui_field_t HOME_F[4][4] = {
    {FP(P5_CUTOFF, "CUTOFF"), FP(P5_RESONANCE, "RESONANCE"), FP(P5_ENV_FILTER, "ENV AMOUNT"), FP(P5_RELEASE_AMP, "RELEASE")},
    {FT(P_E2, "MOD LEVEL"), FT(P_E3, "MOD RATIO"), FT(P_E4, "MOD EG"), FT(P_E6, "DETUNE")},
    {FT(P_LD_PIT, "VIBRATO"), FT(P_LRATE, "LFO RATE"), FT(P_GLIDE, "GLIDE"), FT(P_LEVEL, "LEVEL")},
    {FT(P_E4, "CUTOFF"), FT(P_E5, "RESONANCE"), FT(P_E3, "PULSE WIDTH"), FT(P_E1, "WAVE")},
};

/* the buttons' places (panel.c B_*); REC is no place (it latches) */
static uint32_t place_of_button(uint32_t b)
{
    switch (b) {
    case B_FX: return PL_FX;
    case B_SCL: return PL_SCL;
    case B_ENV: return PL_ENV;
    case B_LFO: return PL_LFO;
    case B_EDIT: return PL_EDIT;
    case B_GLO: return PL_GLO;
    case B_HOME: return PL_HOME;
    case B_SAVE: return PL_SAVE;
    case B_ARP: return PL_ARP;
    case B_SEQ: return PL_SEQ;
    case B_PLAY: return PL_PLAY;
    }
    return PL_N;
}
static uint32_t button_of_place(uint32_t p)
{
    static const uint8_t B[PL_N] = {B_HOME, B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_SAVE, B_ARP, B_SEQ, B_PLAY};
    return B[p % PL_N];
}

/* the entries the held button lists (and the keys light): the objects, or the pages of a place of one object.
 * Entry k's first stop into s[k]; returns the count */
static int obj_starts(const ui_place_t *pl, uint32_t i) { return !i || !str_eq(pl->stop[i].obj, pl->stop[i - 1u].obj); }
static uint32_t place_objects(const ui_place_t *pl)
{
    uint32_t i, n = 0;
    for (i = 0; i < pl->n; i++)
        n += (uint32_t)obj_starts(pl, i);
    return n;
}
static uint32_t place_entries(const ui_place_t *pl, uint8_t *s, uint32_t max)
{
    uint32_t i, n = 0, many = place_objects(pl) > 1u;
    for (i = 0; i < pl->n && n < max; i++)
        if (!many || obj_starts(pl, i))
            s[n++] = (uint8_t)i;
    return n;
}
static const char *entry_name(const ui_place_t *pl, uint32_t stop)
{
    const ui_stop_t *st = &pl->stop[stop];
    return place_objects(pl) > 1u ? st->obj : st->page;
}
/* the header's second word (the object, or the page of a place of one object), the stop's dots: its object's pages */
static const char *stop_title(const ui_place_t *pl, uint32_t s) { return pl->stop[s].obj[0] ? pl->stop[s].obj : pl->stop[s].page; }
static void stop_dots(const ui_place_t *pl, uint32_t s, uint8_t *n, uint8_t *at)
{
    uint32_t a = s, b = s;
    while (a && !obj_starts(pl, a)) a--;
    while (b + 1u < pl->n && !obj_starts(pl, b + 1u)) b++;
    *n = (uint8_t)(b - a + 1u);
    *at = (uint8_t)(s - a);
}
