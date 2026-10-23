/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Command 96, shaped as the Prophet's 95: operation, track, payload. 0 GET
 * (the patch message, the program's name), 1 PUT a patch message without
 * F0/F7, 2 field edit (field, value), 3 printable name, 4 send the patch
 * message. Patch messages audition in RAM and wait in SAVE for explicit user
 * storage; single-control messages (41 32) edit the part's tone. */
#define ED_J106 96u
static void j106_accept_patch(uint32_t tr, const j106_patch_t *patch)
{
    track_t *t = &trk[tr];
    panic_req |= (uint8_t)(1u << tr);
    fm1_irq_off();
    j106_patch[tr] = *patch; j106_ready[tr] = 1; t->eng_req = ENGI_J106;
    t->preset = t->user = t->user_native = 0;
    for (uint32_t i = 0; i < 8u; i++) t->p[P_E0 + i] = ENG_J106.edit[i].def;
    fm1_irq_on();
    sync_reload = 1; ui.force = 1;
}
/* the channel a patch message names: GLOBAL > MIDI's OUT CH */
static uint32_t j106_channel(void) { return trk_midi_ch(0) & 15u; }
static void j106_send(uint32_t tr)
{
#if MELODEE_OTA
    uint8_t frame[J106_FRAME];
    uint32_t n = j106_patch_encode(j106_patch_of(&trk[tr % NTRK]), j106_channel(), frame, sizeof frame);
    if (n) ota_wire_send(frame, n);
#else
    (void)tr;
#endif
}
static int ed_j106_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    if (cmd != ED_J106) return 0;
    uint32_t op = n ? a[0] : 127u, tr = n > 1u ? a[1] : 127u, rc = n < 2u || tr >= NTRK || op > 4u;
    j106_patch_t patch;
    uint8_t frame[J106_FRAME];
    uint32_t size = 0;
    if (!rc) {
        if (op == 0u) {
            if (n != 2u) rc = 1;
            else size = j106_patch_encode(j106_patch_of(&trk[tr]), j106_channel(), frame, sizeof frame);
        } else if (op == 1u) {
            if (n != 2u + J106_FRAME - 2u) rc = 1; /* operation, track, the frame less its F0/F7 */
            else {
                frame[0] = 0xF0u; memcpy(frame + 1, a + 2, J106_FRAME - 2u); frame[J106_FRAME - 1u] = 0xF7u;
                if (!j106_patch_decode(&patch, frame, J106_FRAME)) rc = 1;
                else j106_accept_patch(tr, &patch);
            }
        } else if (op == 2u) {
            if (n != 4u || a[2] >= J106_FIELDS || a[3] < J106_PANEL[a[2]].min || a[3] > J106_PANEL[a[2]].max ||
                trk[tr].eng_req != ENGI_J106) rc = 1;
            else { fm1_irq_off(); j106_edit_value(&trk[tr], a[2], a[3]); fm1_irq_on(); ui.force = 1; }
        } else if (op == 3u) {
            if (n > 2u + J106_NAME_LEN || n < 3u || trk[tr].eng_req != ENGI_J106) rc = 1;
            for (uint32_t i = 2u; !rc && i < n; i++) if (a[i] < 32u || a[i] > 126u) rc = 1;
            if (!rc) {
                char name[J106_NAME_LEN + 1u];
                memcpy(name, a + 2, n - 2u); name[n - 2u] = 0;
                j106_patch_name_set(j106_patch_of(&trk[tr]), name);   /* (the audio never reads the name) */
                ui.force = 1;
            }
        } else if (n != 2u) rc = 1;
        else j106_send(tr);
    }
    ed_b(op == 0u || op == 1u ? 1u : op); ed_b(rc); ed_b(tr);
    if (!rc && op == 0u) {
        char name[J106_NAME_LEN + 1u];           /* (the stored name has no terminator) */
        for (uint32_t j = 1u; j + 1u < size; j++) ed_b(frame[j]);
        j106_patch_name(name, j106_patch_of(&trk[tr]));
        ed_str(name, J106_NAME_LEN);
    }
    return 1;
}
/* Roland frames (41 ...) without F0/F7: a patch or manual-mode message (30,
 * 31) auditions and waits in SAVE; a single control (32) edits the selected part's tone
 * when it plays JUNO-106. 0: not one of these. */
static int j106_native_frame(const uint8_t *body, uint32_t n)
{
    if (n < 2u || body[0] != 0x41u || body[1] < 0x30u || body[1] > 0x32u) return 0;
    if (body[1] == 0x32u) {
        uint32_t tr = song.sel % NTRK;
        if (n == 5u && body[2] <= 15u && body[3] < J106_RAW && body[4] <= 127u && trk[tr].eng_req == ENGI_J106) {
            fm1_irq_off(); j106_patch_of(&trk[tr])->raw[body[3]] = body[4]; fm1_irq_on();
            sysex_native_refresh(ENGI_J106);
            ui.force = 1;
        }
        return 1;
    }
    uint8_t frame[J106_FRAME];
    j106_patch_t patch;
    if (n + 2u == J106_FRAME) {
        frame[0] = 0xF0u; memcpy(frame + 1, body, n); frame[J106_FRAME - 1u] = 0xF7u;
        if (j106_patch_decode(&patch, frame, J106_FRAME)) {
            sysex_native_apply(ENGI_J106, (const uint8_t *)&patch);
            sysex_stage(ENGI_J106, (const uint8_t *)&patch, 1, sizeof patch);
            return 1;
        }
    }
    ui_message("INVALID JUNO-106");
    return 1;
}
