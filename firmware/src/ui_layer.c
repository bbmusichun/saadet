/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Melodee quick layers: while a page button is held, the keys and KNOB 1..4
 * are its shortcuts and its map shows over the page. One table (LAYERS) drives the gesture, the keys, the knobs,
 * the LEDs and the overlay:
 *   GLO  SET   black keys 1..4 (F#3 G#3 A#3 C#4) T1..T4 MUTE (latched; lit = sounding), F3..B3 SOLO T1..T4 while
 *              held (HOLD cells, a corner triangle), C4 UNMUTE ALL, F4 TAP tempo; KNOB 1..4 T1..T4 LEVEL;
 *              GLO + PLAY: from the top without stopping
 *   SCL  SET   any key: its note name is ROOT; KNOB 1..4 ROOT SCL CHRD VOIC (LY_SCL: the SCL page's first two,
 *              the CHORD page's two; QNT TRN stay on SCL); the LEDs show the root lit and the scale's notes blinking
 * The gesture: let go before HOLD (the menu: 0.3 .. 0.6 s) with nothing else touched: a tap, the button's page.
 * Held past HOLD alone: the map (a peek), letting go does nothing. A key, a knob or a button meanwhile: a combo,
 * the map at once, no tap. Keys pressed with the button down are the layer's (seq.c keyboard_block): silent, no
 * MIDI, never recorded; keys held before stay notes. Only one layer at a time: a second layer button is ignored.
 * PLAY and REC work in every layer (GLO + PLAY: RESTART); SAVE, HOME, SEQ and the other page buttons are swallowed. In SET
 * layers OCT± do not shift the octave: OCT- (on release, not with OCT+) puts back what the layer changed since it
 * opened. A HOLD key's effect lasts until the key is let go, the map with it. No layer in the menu, a dialog,
 * NAME or the UPDATE MODE countdown. A tap opens the button's page without a shortcut hint. */
enum { LK_HOLD, LK_SET };
typedef struct {
    uint8_t btn, kind, fam;            /* the button, HOLD / SET, the family whose first page KNOB 1..4 edit */
    const char *head;                  /* (FAM_HOME: the layer's own knobs) the header over the map */
    khint_t foot[3];                   /* the footer's key hints (a third with no word: none) */
} layer_t;
static const layer_t LAYERS[LAYER_N] = {
    {0, 0, 0, "", {{0, 0}, {0, 0}, {0, 0}}},
    {B_GLO, LK_SET, FAM_GLO, "[GLO] SET", {{KC_PLAY, "RESTART"}, {KC_OCTDN, "UNDO"}, {KC_GLO, "DONE"}}},
    {B_SCL, LK_SET, FAM_SCL, "[SCL] SET", {{KC_KEYS, "ROOT"}, {KC_OCTDN, "UNDO"}, {KC_SCL, "DONE"}}},
};
/* SCL's knobs: the key and its chord (cur_page() while the layer edits or draws them: page_over) */
static const page_t LY_SCL = {"SCL", FAM_SCL, SC_TRACK, GR_SCALE, {P_ROOT, P_SCALE, P_CHRD, P_VOIC}};
#define LY_OPEN 2u                     /* ui.ly_t0: the map opened (no tap any more) */
#define LY_COMBO 4u                    /* .. by a combo */
#define LY_DEAD 8u                     /* .. pressed where there is no layer: does nothing */

static struct {
    uint8_t l, trk, oct;               /* SET: layer, snapshot track, OCT- / OCT+ pressed (bits) */
    int16_t v[9];                      /* the values when it opened: GLO mutes, levels, BPM; SCL ROOT..TRN, CHRD VOIC */
    uint32_t solo;                     /* GLO: the keys held that solo */
    uint32_t tap[4];                   /* GLO TAP: the last taps (fm1_ms) */
    uint8_t ntap, dt_l, dtap, rp_dirty;
    uint32_t dt_ms;
    struct { uint8_t home, page, act, seq, fam[FAM_COUNT]; } nv;
} lys;

static const uint32_t LY_DTAP_MS=300u;



static int layer_allowed(void) { return !ui.menu && !ui.confirm && !ui.uboot && !name_on() && !pop.on && !smap.on; }
static uint32_t ly_bit(uint32_t l) { return 1u << panel.btn[LAYERS[l].btn]; }
static uint32_t ly_down(uint32_t l) { return l && ((fm1_in.buttons & ly_bit(l)) != 0u || ui.lock == l); }
static uint32_t layer_bits(void)
{
    uint32_t l, m = 0;
    for (l = LAYER_GLO; l < LAYER_N; l++)
        m |= ly_bit(l);
    return m;
}
static uint32_t layer_btn(void) { return LAYERS[ui.layer % LAYER_N].btn; }
static const char *layer_head(void)
{
    static const char *const locked[LAYER_N] = {"", "[GLO] LOCK", "[SCL] LOCK"};
    return ui.lock ? locked[ui.layer % LAYER_N] : LAYERS[ui.layer % LAYER_N].head;
}
static uint32_t layer_open(void) { return ui.ly && (ui.ly_t0 & LY_OPEN) && ly_down(ui.ly) && layer_allowed() ? ui.ly : 0u; }
static int layer_set_open(void) { return layer_open() && LAYERS[layer_open()].kind==LK_SET; }
static uint32_t layer_held(void) { return ui.ly && ly_down(ui.ly) && layer_allowed(); }

/* each pass, before the keys: what the ISR (seq.c keyboard_block) gives to a layer. Armed: its button; none
 * armed: any layer button not already down (one pressed meanwhile is armed below; one held over from before
 * is ignored, its keys stay notes); armed but dead (pressed under a dialog, a menu or NAME that has closed
 * since): nothing, its keys stay notes */
static void layer_masks(void)
{
    uint32_t m = !layer_allowed() || (ui.ly && (ui.ly_t0 & LY_DEAD)) ? 0u
               : ui.ly ? ly_bit(ui.ly) : layer_bits() & ~fm1_in.buttons;
    kb_mask = m;

    kb_lock = m && ui.lock ? 1u : 0u;

}

/* a layer button pressed (its edge) while none is armed: it is now, a dead one where no layer opens */
static void layer_arm(uint32_t pressed, uint32_t now)
{
    uint32_t l;
    if (ui.ly)
        return;
    for (l = LAYER_GLO; l < LAYER_N; l++)
        if (pressed & ly_bit(l) & layer_bits()) {
            lys.dtap=lys.dt_l==l && fm1_ms-lys.dt_ms<=LY_DTAP_MS;
            lys.dt_l=0;
            ui.ly = (uint8_t)l;
            ui.ly_t0 = (now & ~15u) | 1u | (layer_allowed() ? 0u : LY_DEAD);
            return;
        }
}

/* the layer opened: SET: what OCT- will put back */
static void layer_opened(uint32_t l)
{
    lys.l=(uint8_t)l;lys.trk=0;
    if(l==LAYER_GLO){lys.v[0]=TSEL->p[P_LEVEL];lys.v[1]=TSEL->p[P_PAN];lys.v[2]=TSEL->p[P_SPRD];lys.v[3]=song.g[G_BPM];}
    else {for(uint32_t k=0;k<4u;k++)lys.v[k]=TSEL->p[P_ROOT+k];lys.v[4]=TSEL->p[P_CHRD];lys.v[5]=TSEL->p[P_VOIC];}
}


static void layer_let_go(uint32_t quiet)
{
    (void)quiet;

    if (lys.rp_dirty) { lys.rp_dirty=0; settings_save(); }
    ui.ly_t0=0; ui.ly=0; ui.lock=0; kb_lock=0;
}
static void layer_lock_input(uint32_t pressed)
{
    uint32_t keep = 1u << panel.btn[B_PLAY] | 1u << panel.btn[B_REC] | 1u << panel.btn[B_OCTDN] | 1u << panel.btn[B_OCTUP];
    if (lys.dt_l && (pressed & ~ly_bit(lys.dt_l)))
        lys.dt_l = 0;
    if (!ui.lock)
        return;
    if (pressed & ly_bit(ui.lock))
        ui.lock = 0;
    else if (pressed & ~keep)
        layer_let_go(1);
}

/* the armed button: 0, or the layer whose button was tapped (let go) */
static uint32_t layer_gesture(uint32_t now, uint32_t combo)
{
    uint32_t *t0 = &ui.ly_t0, l = ui.ly;
    if (!l)
        return 0;
    if (!layer_allowed()) {
        *t0 |= LY_DEAD;
        ui.lock = 0;                                    /* (the menu, a dialog, NAME: the lock closes) */
    }
    if (ly_down(l)) {
        if (!(*t0 & (LY_OPEN | LY_DEAD)) &&
            (combo || now - (*t0 & ~15u) >= (uint32_t)HOLD_MS[settings_hold % 4u] * 1000u * FM1_TICKS_PER_US)) {
            *t0 |= LY_OPEN | (combo ? LY_COMBO : 0u);
            layer_opened(l);
        }
        return 0;
    }
    l = *t0 & (LY_OPEN | LY_DEAD) || combo ? 0u : l;   /* (a knob turned as it was let go: a combo, no tap) */
    if (l && lys.dtap) {                                /* #83: a double tap: locked open (no tap) */
        lys.dtap = 0;
        ui.home=lys.nv.home; ui.page=lys.nv.page; ui.act=lys.nv.act;
        memcpy(ui.fam_last,lys.nv.fam,sizeof ui.fam_last);
         ui.msg_t=0; ui.lock=(uint8_t)l;
        ui.ly_t0=(now&~15u)|1u|LY_OPEN; ui.force=1; layer_opened(l);
        return 0;
    }
    layer_let_go(*t0 & LY_OPEN || combo);
    return l;
}

/* the map shows while the button is held open, and after it while its keys are still held */
static void layer_show(void)
{
    uint32_t show = layer_allowed() ? (layer_open() ? layer_open() : kb_layer ? ui.layer : 0u) : 0u;
    if (show != ui.layer) {
        ui.layer = (uint8_t)show;
        ui.force = 1;
    }
}

/* a tap: the button's page; keep the previous navigation state for a double tap */
static void layer_tap(uint32_t l)
{
    lys.dt_l=(uint8_t)l; lys.dt_ms=fm1_ms;
    lys.nv.home=ui.home; lys.nv.page=ui.page; lys.nv.act=ui.act;
    memcpy(lys.nv.fam,ui.fam_last,sizeof ui.fam_last);
    page_tap(LAYERS[l].btn);
}

/* GLO TAP: from the third tap the tempo of the taps (the last 4); 2 s without one starts over. INT clock only */
static void glo_tap(void)
{
    uint32_t n = lys.ntap, i;
    if (song.g[G_CLOCK]) {
        ui_message("TAP: CLK IS EXT");
        return;
    }
    if (n && fm1_ms - lys.tap[n - 1u] > 2000u)
        n = 0;
    if (n == 4u) {
        for (i = 0; i < 3u; i++)
            lys.tap[i] = lys.tap[i + 1u];
        n = 3;
    }
    lys.tap[n++] = fm1_ms;
    lys.ntap = (uint8_t)n;
    if (n >= 3u && lys.tap[n - 1u] != lys.tap[0]) {
        song.g[G_BPM] = (int16_t)clamp((int32_t)(60000u * (n - 1u) / (lys.tap[n - 1u] - lys.tap[0])),
                                       GP[G_BPM].min, GP[G_BPM].max);
        ui.bpm_t = 40;
    }
}

/* a key pressed in layer l (k: 0 = F3 .. 26 = G5) */
static void layer_key(uint32_t l,uint32_t k)
{
    if(l==LAYER_GLO){if(!key_black(k) && key_place(k)==7u)glo_tap();}
    else if(l==LAYER_SCL){TSEL->p[P_ROOT]=(int16_t)((k+5u)%12u);scale_share(TSEL);}
}
/* each pass: the keys the layer got now; then GLO's solo: the tracks of its keys still held (perform.c) */
static void layer_keys(uint32_t keys)
{for(uint32_t k=0;k<27u;k++)if(keys&(1u<<k))layer_key(layer_open()?layer_open():ui.layer,k);}

/* KNOB 1..4 in the open layer: its own (GLO levels), else the four of its page */
static void layer_knob(uint32_t k,int32_t step)
{
    uint32_t l=layer_open();
    if(l){uint8_t h=ui.home,pg=ui.page;ui.home=0;ui.page=(uint8_t)page_first(LAYERS[l].fam);page_over=l==LAYER_SCL?&LY_SCL:0;edit_param(k,step);page_over=0;ui.home=h;ui.page=pg;}
}

/* PLAY with GLO held open: RESTART (from the top, playing on); stopped: PLAY. 1 = done here */
static int layer_play(void) { return 0; }

/* OCT- / OCT+ in a SET layer: no octave; OCT- let go puts back what the layer changed. Returns the taps left */
static uint32_t layer_oct(uint32_t pressed, uint32_t oct)
{
    uint32_t dn = panel.btn[B_OCTDN], up = panel.btn[B_OCTUP], k, mine;
    if (layer_set_open())
        lys.oct |= (uint8_t)(((pressed >> dn) & 1u) | ((pressed >> up) & 1u) << 1);
    mine = oct & lys.oct;
    if (mine & 1u) {                                    /* OCT-: back to when it opened */
        if(lys.l==LAYER_GLO){
            TSEL->p[P_LEVEL]=lys.v[0];TSEL->p[P_PAN]=lys.v[1];TSEL->p[P_SPRD]=lys.v[2];song.g[G_BPM]=lys.v[3];ui_message("OUTPUT PUT BACK");
        } else {
            for (k = 0; k < 4u; k++)
                trk[lys.trk % NTRK].p[P_ROOT + k] = lys.v[k];
            trk[lys.trk % NTRK].p[P_CHRD] = lys.v[4];
            trk[lys.trk % NTRK].p[P_VOIC] = lys.v[5];
            scale_share(&trk[lys.trk % NTRK]);
            ui_message("SCALE PUT BACK");
        }
    }
    lys.oct &= (uint8_t)(((fm1_in.buttons >> dn) & 1u) | ((fm1_in.buttons >> up) & 1u) << 1);
    return oct & ~mine;
}

/* --------------------------------------------------------- the LEDs --- */
/* lit = in effect now (held, the value, a track sounding), slow blink = can be pressed, dark = nothing there */

static uint32_t layer_leds(void)
{
    uint32_t k, m = 0, blink = ((fm1_ms / 250u) & 1u) == 0u, l = ui.layer;
    uint32_t mask = scale_mask(TSEL), root = (uint32_t)TSEL->p[P_ROOT] % 12u;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), b = (uint32_t)key_black(k), on = 0, e;
        if (l == LAYER_GLO) {
            on=!b && p==7u && !song.g[G_CLOCK] && blink;
        } else if (l == LAYER_SCL) {                    /* the root lit, the scale's notes blink */
            e = (k + 5u + 12u - root) % 12u;
            on = e == 0u || (((mask >> e) & 1u) && blink);
        }
        m |= on << k;
    }
    return m;
}

/* ------------------------------------------------------ the overlay --- */
/* One template: the header names the button and the kind ("[GLO] SET"); the cards are KNOB 1..4; the panel the
 * map of the keys as cells (the key's note name, an icon, a name); the footer the keycaps. A cell: RAISE
 * (can be pressed), the selection's fill (the value now, SET), the accent (held, HOLD), DIM (cannot now: pressing
 * it says why), KEY (a muted track, as the MUTE badge); HOLD cells in a SET layer: a corner triangle */
#define LY_ROW 24                                        /* (large type) the SCL cells: 22 px, 24 apart */
enum { LS_OFF, LS_SEL, LS_HELD, LS_WAIT, LS_DIM, LS_MUTE };

static uint16_t lc_fill(uint32_t st, uint16_t *ink)
{
    *ink = st == LS_DIM ? T_DIM : T_THEME;
    if (st == LS_MUTE) {
        *ink = T_INK;
        return T_KEY;
    }
    if (st == LS_HELD) {
        *ink = T_BG;
        return T_ACCENT;
    }
    if (st == LS_SEL || st == LS_WAIT) {
        *ink = T_INK;
        return T_THEME;
    }
    return T_RAISE;
}
static void layer_scl(void)                             /* KNOB 2's scales as a list (their names in full), four
                                                         * around the one now selected (filled) */
{
    uint32_t i, sc = (uint32_t)TSEL->p[P_SCALE], max = (uint32_t)TP[P_SCALE].max, first;
    first = sc > 1u ? sc - 1u : 0u;
    if (max >= 3u && first > max - 3u)
        first = max - 3u;
    for (i = first; i < first + 4u && i <= max; i++) {
        int32_t y = 3 + 30 * (int32_t)(i - first);
        uint16_t ink, fill = lc_fill(i == sc ? LS_SEL : LS_OFF, &ink);
        const char *t = SCALE_TITLE[i];
        const aafont_t *f = ty_fit(TY_TX, t, 200);
        if (i == sc)
            cv_rrect(8, y, 224, 28, 8, fill, T_SURF);
        cv_free_text(18, ty_y(y + 14, f), f, t, i == sc ? ink : T_SEC, i == sc ? fill : T_SURF, 200);
    }
}
/* GLO held (docs/design: mock/r6_pages GLO): the keys as a keyboard: the black keys MUTE 1..4 (muted: red), the white
 * keys SOLO 1..4 (held: the track's colour), C unmutes all, the high F taps the tempo (an outside clock: dim) */


static void layer_cards(uint32_t l)
{
    uint8_t h=ui.home,pg=ui.page;ui.home=0;ui.page=(uint8_t)page_first(LAYERS[l].fam);
    page_over=l==LAYER_SCL?&LY_SCL:0;draw_columns();page_over=0;ui.home=h;ui.page=pg;
}

static void draw_layer(void)
{
    uint32_t l=ui.layer%LAYER_N;
    layer_cards(l);
    if(l==LAYER_GLO){
        if(ui.force){
            lcd_fill(0,Y_GRAPH,240,240-Y_GRAPH,T_BG);
            draw_text_box(8,110,224,TY_TX,"OUTPUT",T_THEME,1);
            draw_text_box(8,142,224,TY_LB,"Level / Pan / Spread / Tempo",T_TEXT,1);
            draw_text_box(8,176,224,TY_LB,"White key C4: tap tempo",T_SEC,1);
        }
        return;
    }
    uint32_t sig=l*7919u+ux.gen*977u+TSEL->p[P_ROOT]*131u+TSEL->p[P_SCALE];
    if(ui.force || sig!=ui.layer_sig){
        ui.layer_sig=sig;
        for(uint32_t band=0;band<H_GRAPH;band+=CV_MAX/240u){
            cv_begin_band(240,H_GRAPH,T_BG,band);cv_rrect(BOX_X,0,BOX_W,H_GRAPH,6,T_SURF,T_BG);cv_bg=T_SURF;
            layer_scl();cv_blit_band(0,Y_GRAPH);
        }
    }
}
