# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Checked adaptations of the pinned base; upstream files keep their notices."""
from pathlib import Path


def replace(s, old, new):
    if old not in s:
        raise ValueError(f"Pinned fm1-base input changed: {old[:90]!r}")
    return s.replace(old, new)


def function(s, name, body):
    start = s.index(name)
    start = s.rfind('\n', 0, start) + 1
    opening = s.index('{', start)
    level = 1
    end = opening + 1
    while level:
        if s[end] == '{': level += 1
        if s[end] == '}': level -= 1
        end += 1
    return s[:start] + body.rstrip() + s[end:]


def patch_usb(s):
    # Native stereo frames throughout both rings; format conversion at USB only.
    s = replace(s, '#define UAC_MAXP (UA_MAXF * 4u)', '#define UAC_MAXP (UA_MAXF * 6u)')
    s = replace(s, 'static uint32_t ua_ring[UA_N] FELUCCA_UAC_RING_ATTR;',
        'typedef struct { int32_t l, r; } sd_pcm_frame;\nstatic sd_pcm_frame ua_ring[UA_N] __attribute__((section(".pool")));')
    s = replace(s, 'static uint32_t ep4tx[UA_MAXF]', 'static uint32_t ep4tx[(UAC_MAXP + 3u) / 4u]')
    s = replace(s, 'uint32_t last;               /* the last frame sent, repeated on an underrun */',
        'sd_pcm_frame last;           /* native frame held on underrun */')
    s = replace(s, '#define UO_MAXP (UO_MAXF * 4u)', '#define UO_MAXP (UO_MAXF * 6u)')
    s = replace(s, 'static uint32_t uo_ring[UO_N];', 'static sd_pcm_frame uo_ring[UO_N] __attribute__((section(".pool")));')
    s = replace(s, 'static uint32_t ep4rx[UO_MAXF + 1u]', 'static uint32_t ep4rx[(UO_MAXP + 3u) / 4u + 1u]')
    # Both widths are selectable per direction; all formats remain 44.1 kHz.
    marker = '#define D_AUDIO(a) D_AC(a) D_MIDI((a) + 1) D_AS((a) + 2) D_ASO((a) + 3)'
    alt = '''#define SD_AS24(s, terminal, ep, attr) \\
    9, 4, (s), 2, 1, 1, 2, 0, 0, \\
    7, 0x24, 1, (terminal), 1, 1, 0, \\
    11, 0x24, 2, 1, 2, 3, 24, 1, 0x44, 0xAC, 0, \\
    9, 5, (ep), (attr), (UA_MAXF*6u)&255u, (UA_MAXF*6u)>>8, 1, 0, 0, \\
    7, 0x25, 1, ((ep)&128u)?1:0, 0, 0, 0,
#if FELUCCA_UAC
#define SD_CAP24(s) SD_AS24(s,2,0x84,0x05)
#else
#define SD_CAP24(s)
#endif
#if FELUCCA_UAC_OUT
#define SD_PLAY24(s) SD_AS24(s,3,0x04,0x09)
#else
#define SD_PLAY24(s)
#endif
#define D_AUDIO(a) D_AC(a) D_MIDI((a) + 1) D_AS((a) + 2) SD_CAP24((a)+2) D_ASO((a) + 3) SD_PLAY24((a)+3)'''
    s = replace(s, marker, alt)
    s = replace(s, '#define AUD_LEN (92 + (63 + FMT_LEN) * FELUCCA_UAC + 74 * FELUCCA_UAC_OUT)',
        '#define AUD_LEN (92 + (106 + FMT_LEN) * FELUCCA_UAC + 117 * FELUCCA_UAC_OUT)')
    s = replace(s, 'wvalue <= 1u && usb.config', 'wvalue <= 2u && usb.config')
    s = replace(s, 'uac.last = 0;', 'uac.last = (sd_pcm_frame){0,0};')
    s = replace(s, 'ua_ring[(r + i) & (UA_N - 1u)] = 0;', 'ua_ring[(r + i) & (UA_N - 1u)] = (sd_pcm_frame){0,0};')
    helper = '''static int32_t sd_pcm_read(const uint8_t *p, uint32_t width)
{
    uint32_t v=p[0]|(uint32_t)p[1]<<8;
    if(width==2u)return (int32_t)(int16_t)v*256;
    v|=(uint32_t)p[2]<<16;
    return (v&0x800000u)?(int32_t)v-16777216:(int32_t)v;
}
static uint32_t sd_pcm_rng=0x913ab267u;
static uint32_t sd_pcm_random(void){uint32_t x=sd_pcm_rng;x^=x<<13;x^=x>>17;x^=x<<5;return sd_pcm_rng=x;}
static void sd_pcm_write(uint8_t *p,uint32_t width,int32_t v)
{
    v=v>8388607?8388607:v<-8388608?-8388608:v;
    if(width==2u){
        if(v)v+=(int32_t)(sd_pcm_random()&255u)-(int32_t)(sd_pcm_random()&255u)+128;
        v>>=8;v=v>32767?32767:v<-32768?-32768:v;
    }
    uint32_t q=(uint32_t)v;p[0]=(uint8_t)q;p[1]=(uint8_t)(q>>8);
    if(width==3u)p[2]=(uint8_t)(q>>16);
}
'''
    s = replace(s, 'static inline int32_t uac_s16(', helper + '\nstatic inline int32_t uac_s16(')
    s = function(s, 'void uac_tap(const int32_t *out', '''static __attribute__((noinline)) void uac_tap(const int32_t *out,uint32_t n)
{
    uint32_t w=ua_w;if(!uac.feed)return;
    if(UA_N-(w-ua_r)<n){uac.overruns++;return;}
    for(uint32_t i=0;i<n;i++)ua_ring[(w+i)&(UA_N-1u)]=(sd_pcm_frame){out[2u*i],out[2u*i+1u]};
    RING_PUBLISH();ua_w=w+n;
}''')
    s = function(s, 'uint32_t uac_packet(uint32_t *d)', '''static uint32_t uac_packet(uint32_t *d)
{
    uint32_t n=44u,width=uac.alt==2u?3u:2u;
    uac.acc+=100u;if(uac.acc>=1000u){uac.acc-=1000u;n++;}
    uint32_t r=ua_r,fill=ua_w-r,k=0;
    if(uac.go){
        if(uac.fill_min>UA_HI){n++;uac.adj_up++;}
        else if(uac.fill_min<UA_LO){n--;uac.adj_down++;}
        k=fill<n?fill:n;if(k<n)uac.underruns++;
    }
    RING_PUBLISH();
    sd_pcm_frame last=uac.go?uac.last:(sd_pcm_frame){0,0};
    uint8_t *p=(uint8_t *)d;
    for(uint32_t i=0;i<n;i++){
        if(i<k)last=ua_ring[(r+i)&(UA_N-1u)];
        sd_pcm_write(p+2u*i*width,width,last.l);
        sd_pcm_write(p+(2u*i+1u)*width,width,last.r);
    }
    uac.last=last;RING_PUBLISH();ua_r=r+k;
    uac.pkts++;uac.frames+=n;return n;
}''')
    s = replace(s, 'n = (sie_rd(S_RXCOUNT1) | (sie_rd(S_RXCOUNT2) << 8)) / 4u;',
        'uint32_t width=uo.alt==2u?3u:2u;\n    n = (sie_rd(S_RXCOUNT1) | (sie_rd(S_RXCOUNT2) << 8)) / (2u*width);')
    s = replace(s, 'uo_ring[(w + i) & (UO_N - 1u)] = ep4rx[i];',
        'uo_ring[(w+i)&(UO_N-1u)]=(sd_pcm_frame){sd_pcm_read((uint8_t *)ep4rx+2u*i*width,width),sd_pcm_read((uint8_t *)ep4rx+(2u*i+1u)*width,width)};')
    s = replace(s, '(float)gain_q12 * (1.0f / 16.0f)', '(float)gain_q12 * (1.0f / 4096.0f)')
    s = replace(s, 'uint32_t k = pos >> 16, a = uo_ring[(r + k) & (UO_N - 1u)], b = uo_ring[(r + k + 1u) & (UO_N - 1u)];',
        'uint32_t k=pos>>16;sd_pcm_frame a=uo_ring[(r+k)&(UO_N-1u)],b=uo_ring[(r+k+1u)&(UO_N-1u)];')
    s = replace(s, '(float)(int16_t)a + ((float)(int16_t)b - (float)(int16_t)a)', '(float)a.l + ((float)b.l - (float)a.l)')
    s = replace(s, '(float)(int16_t)(a >> 16) + ((float)(int16_t)(b >> 16) - (float)(int16_t)(a >> 16))', '(float)a.r + ((float)b.r - (float)a.r)')
    s = replace(s, 'fm1_usb_ep4_send(ep4tx, n * 4u);', 'fm1_usb_ep4_send(ep4tx, n * (uac.alt==2u?6u:4u));')
    # Saadet's standard playback master mute/volume stays available to the OS.
    s=replace(s,'0x01, 53, 0, 3,','0x01, 63, 0, 3,')
    s=replace(s,'9, 0x24, 3, 4, 0x01, 0x03, 0, 3, 0,', '10, 0x24, 6, 5, 3, 1, 3, 0, 0, 0, 9, 0x24, 3, 4, 0x01, 0x03, 0, 5, 0,')
    s=replace(s,'117 * FELUCCA_UAC_OUT)', '127 * FELUCCA_UAC_OUT)')
    s=replace(s,'static void ep0_service(void)', '#if FELUCCA_UAC_OUT\n#include "usb_volume.h"\n#endif\nstatic void ep0_service(void)')
    s=replace(s,'    for (i = 0; i < 8u; i++)\n        s[i] = ep0buf[i];', '#if FELUCCA_UAC_OUT\n    if(sd_volume_pending){\n        int ok=sd_volume_data(ep0buf,sie_rd(S_COUNT0));if(ok)goto ack;goto stall;\n    }\n#endif\n    for (i = 0; i < 8u; i++)\n        s[i] = ep0buf[i];')
    s=replace(s,'    usb.setups++;', '    usb.setups++;\n#if FELUCCA_UAC_OUT\n    if(sd_volume_setup(s))return;\n#endif')
    s=replace(s,'        uac.e0_rx = 0;', '        uac.e0_rx = 0;\n#if FELUCCA_UAC_OUT\n        sd_volume_pending=0;\n#endif')
    s=replace(s,'    uo_ep4_reset();\n    uo_stream(0);', '    uo_ep4_reset();\n    uo_stream(0);\n    ua_host_volume_reset();')
    # These reset routines precede the control implementation.
    s=replace(s,'static void uo_ep4_reset(void)', 'static void ua_host_volume_reset(void);\nstatic void uo_ep4_reset(void)')
    s=replace(s,'        float l = (float)a.l', '        int32_t target=ua_host_mute?0:ua_host_target_q15;\n        ua_host_gain_q15+=(target-ua_host_gain_q15)/128;\n        if(ua_host_gain_q15!=target&&target-ua_host_gain_q15<128&&target-ua_host_gain_q15>-128)ua_host_gain_q15=target;\n        float volume=(float)ua_host_gain_q15*(1.0f/32768.0f);\n        float l = (float)a.l')
    s=replace(s,'(int32_t)(l * g)', '(int32_t)(l * g * volume)')
    s=replace(s,'(int32_t)(rr * g)', '(int32_t)(rr * g * volume)')
    # Raw streams reach the existing bounded native-bank parsers even when too
    # large for the base's app/editor frame collector. Real-time bytes survive.
    s = replace(s, 'static void sysex_byte(uint8_t b)\n{', 'static void sysex_byte(uint8_t b)\n{\n#ifndef FELUCCA_LOADER\n    saadet_sysex_byte(b);\n#endif')
    s=replace(s,'    if (st >= 0x80u && st < 0xF8u) {','    if (st >= 0x80u && st < 0xF8u) {\n#ifndef FELUCCA_LOADER\n        saadet_sysex_byte((uint8_t)st); /* cancel an interrupted native bank */\n#endif')
    return s


def apply(work):
    # BLE-only builds have no sys_timer task. SDK timers naming that service
    # belong to the main-loop timer pump, as unnamed BLE timers already do.
    p=work/'ble/shim/systimer.c';s=p.read_text()
    s=replace(s,'            tm[i].task = task;', '''            tm[i].task = !SHIM_WIFI && task && !strcmp(task, "sys_timer") ? 0 : task;''')
    p.write_text('/* Saadet timer routing Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n'+s)
    p=work/'tools/build.py';s=p.read_text()
    s=replace(s,'("data.bin", "_data_load")):', '("data.bin", "_data_load"), ("audio.bin", "_sd_audio_load")):')
    s=replace(s,'    img = bytearray((OUT / "text.bin").read_bytes())', '    tc("common/bin/objcopy", "-O", "binary", "-j", ".audio_text", elf, OUT / "audio.bin")\n    img = bytearray((OUT / "text.bin").read_bytes())')
    s=replace(s,'    (OUT / (NAME + ".dis")).write_text(dis)', '    (OUT / (NAME + ".dis")).write_text(dis)\n    (OUT / (NAME + ".symbols")).write_text(syms)')
    p.write_text(s)
    p=work/'bsp/chip/fm1_guard.h';s=p.read_text()
    s=replace(s,'re = (uint32_t)(uintptr_t)_rt_end;', 're = (uint32_t)(uintptr_t)_sd_exec_end;')
    s=replace(s,'static void fm1_guard_enable(', 'extern uint32_t _sd_exec_end[];\nstatic void fm1_guard_enable(')
    p.write_text(s)
    p=work/'bsp/chip/usb.c';s=patch_usb(p.read_text());p.write_text('/* Saadet integration modifications Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n'+s)
    p=work/'device/main_fm1.c';s=p.read_text()
    # Use Melodee's complete stock V15 supply sequence and checked result,
    # rather than the base's two-rail-only raises. CPU1 stays held until the
    # DCDC/LDO mode, IO supplies and CPU/SRAM supplies have all verified.
    start=s.index('        for (rails_tries = 1;')
    end=s.index('        flash_ok =',start)
    s=s[:start]+'        rails_tries = 1;\n        sd_power_init(); /* Melodee stock V15 power setup; CPU1 held, IRQs off */\n'+s[end:]
    # Melodee firmware/src/main.c defers the PC limits until CPU1's bounded
    # handshake returns: CPU1 first executes mask-ROM outside our XIP/SRAM
    # windows. Enabling them in cstart faults that bootstrap (DBG bit 10).
    # Keep stack/write/bus guards on throughout, and arm PC limits even when
    # app_init skips CPU1 or the handshake stops it on timeout.
    s=replace(s,'    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);',
        '    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS);')
    s=replace(s,'    app_init();                                     /* the app\'s engine and its saved state (or defaults) */',
        '    app_init();                                     /* the app\'s engine and its saved state (or defaults) */\n'
        '    fm1_guard_enable(FM1_GUARD_PC);                  /* CPU1 has left mask-ROM, or is held after timeout */')
    # Include the already-recorded causes, so an on-device CPU exception can
    # be distinguished from a stack, PC-limit or invalid-bus fault over USB.
    s=replace(s,'        for (k = (int)FM1_CRASH_CTX_N - 1;',
        '        { char cause[]="emu 00000000 dbg 00000000";\n'
        '          hexs(cause+4,fm1_crash.emu); cause[12]=\' \';\n'
        '          hexs(cause+17,fm1_crash.dbg); log_line_send(cause); }\n'
        '        for (k = (int)FM1_CRASH_CTX_N - 1;')
    start=s.index('#if FELUCCA_UAC_OUT\n            uac_play(');end=s.index('            if (full)',start)
    s=s[:start]+'''#if FELUCCA_UAC
            uac_tap(o + 2u * k, REND_FRAMES); /* synth capture precedes computer playback */
#endif
#if FELUCCA_UAC_OUT
            uac_play(o + 2u * k, REND_FRAMES, full ? 4096u : master_pos * master_pos >> 12);
#endif
'''+s[end:]
    s=replace(s,'    us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;', '    us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;\n    saadet_deadline(us,budget);')
    s=replace(s,'            if (full) {', '            for(uint32_t i=0;i<2u*REND_FRAMES;i++){\n                int32_t v=o[2u*k+i];v=v>8388607?8388607:v<-8388608?-8388608:v;\n                o[2u*k+i]=v>>1; /* physical -6 dB ceiling after native USB capture/playback */\n            }\n            if (full) {')
    # The platform's render/tick closure includes P33 and matrix helpers also
    # used during early boot. Initialize their code AND far-call pointers before
    # the first platform helper executes; clear POOL before copying code into it.
    start=s.index('void fm1_cstart(void)')
    clear=s.index('    for (d = _bss_start;',start)
    end=s.index('    fm1_mailbox_clear();',clear)
    init=s[clear:end]+"    for(s=_sd_audio_load,d=_sd_audio_start;d<_sd_audio_end;s++,d++)*d=*s;\n"
    s=s[:clear]+s[end:]
    at=s.index('    fm1_time_init();',start)
    s=s[:at]+init+s[at:]
    s=replace(s,'extern uint32_t _data_start[]', 'extern uint32_t _sd_audio_start[],_sd_audio_end[],_sd_audio_load[];\nextern uint32_t _data_start[]')
    p.write_text('/* Saadet integration modifications Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n'+s)

    p=work/'bsp/chip/fm1_irq.h';s=p.read_text()
    # Match Melodee's CPU0 fp32 policy (firmware/hal/fm1_irq.h,
    # 97357ab). CPU1 already uses it in fm1_core1_main. Inexact arithmetic
    # and quiet filter tails are normal, not reasons to reset the synth.
    s=replace(s,'    FM1_EMU_CON &= ~(1u << 2);',
        '    FM1_EMU_CON &= ~((1u << 2) | (31u << 16)); /* Melodee: div0 and fp32 traps off, stack guard retained */')
    s=replace(s,'void fm1_fault_c(uint32_t *f)\n{','void fm1_fault_c(uint32_t *f)\n{\n    if(sd_fault_frame(f))return;')
    p.write_text('/* Saadet CPU1 recovery modifications Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n'+s)
    p=work/'bsp/chip/fm1_vec.S';s=p.read_text()
    restore=(Path(__file__).resolve().parents[1]/'firmware/hal/fm1_vec.S').read_text()
    restore=restore[restore.index('\t/* fm1_fault_c returns'):]
    s=replace(s,'1:\tgoto 1b',restore)
    p.write_text('/* Saadet CPU1 recovery modifications Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n'+s)

    p=work/'bsp/boards/fm1/fm1_input.h';s=p.read_text()
    s=replace(s,'#define FM1__LIT(c) (fm1_led[c] |', 'static volatile uint8_t sd_led_planes[3][11],sd_led_masks[3];\nstatic uint8_t sd_led_phase;\n#define SD_LED_PLANE(c,p) ((sd_led_masks[p]!=255u && !(sd_led_phase&sd_led_masks[p]))?sd_led_planes[p][c]:0u)\n#define FM1__LIT(c) (fm1_led[c] | SD_LED_PLANE(c,0) | SD_LED_PLANE(c,1) | SD_LED_PLANE(c,2) |')
    s=replace(s,'if (p == 0u)\n        fm1__breath_frame();','if (p == 0u) { sd_led_phase++; fm1__breath_frame(); }')
    p.write_text('/* Saadet LED planes Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n'+s)
    p=work/'device/panel.c';s=p.read_text()
    s=replace(s,'    panel.magic = PANEL_MAGIC;\n    plat_store_save', '    panel.magic = PANEL_MAGIC;\n    saadet_panel_calibrated(panel.btn,panel.enc,panel.dir);\n    plat_store_save')
    p.write_text('/* Saadet calibration bridge Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n'+s)

    p=work/'ble/midi/le_midi.c';s=p.read_text()
    # Keep the two BLE links distinct until Saadet has tracked physical holds.
    s=replace(s,'static void midi_parse(const u8 *b, u16 len)', 'static void midi_parse(const u8 *b, u16 len, u8 source)')
    s=replace(s,'ble_midi_receive(0xFu |', 'ble_midi_receive(source | 0xFu |')
    s=replace(s,'ble_midi_receive((uint32_t)cin_of', 'ble_midi_receive(source | (uint32_t)cin_of')
    s=replace(s,'midi_parse(buffer, buffer_size);','midi_parse(buffer, buffer_size, 0);')
    s=replace(s,'midi_parse(d->blob, d->blob_length);','midi_parse(d->blob, d->blob_length, 0x10);')
    start=s.index('static void client_packet_handler(')
    end=s.index('/* one HCI handler for both roles:',start)
    s=s[:start]+replace(s[start:end],'        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:',
        '        case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE:\n        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:')+s[end:]
    # Cancellation must resume advertising and must not trigger retry-on-0x3E.
    s=function(s,'void ble_midi_disconnect(void)', '''void ble_midi_disconnect(void)
{
    mc_tries = 0;
    if (mc_handle)
        ble_op_disconnect(mc_handle);
    else if (mc_state == BLE_MC_CONNECTING) {
        ble_op_create_connection_cancel();
        mc_state = BLE_MC_OFF;
        bt_ble_adv_enable(1);
    }
}''')
    s=replace(s,'static const char gap_device_name[] = "FM-1";', 'static const char gap_device_name[] = "Saadet";')
    # A fresh link must negotiate its own subscription, MTU and parameters.
    s=replace(s,'    set_ble_work_state(BLE_ST_CONNECT);', '''    CCC_SET(handle, H_CCC, 0);
    send_mtu = 20;
    connection_update_cnt = 0;
    set_ble_work_state(BLE_ST_CONNECT);''')
    s=replace(s,'return con_handle && CCC_GET(con_handle, H_CCC);', 'return con_handle && (CCC_GET(con_handle, H_CCC) & 1u);')
    # ATT name reads may request a buffer larger than the remaining value.
    s=replace(s,'        if (offset >= len || offset + buffer_size > len)\n            break;', '''        if (offset >= len)
            return 0;
        len -= offset;''')
    s=replace(s,'            memcpy(buffer, &gap_device_name[offset], buffer_size);\n            len = buffer_size;', '''            if (len > buffer_size) len = buffer_size;
            memcpy(buffer, &gap_device_name[offset], len);''')
    # Reuse CTL's radio enable/disconnect lifecycle, keeping advertising enabled
    # so the disconnect-complete event makes this peripheral discoverable again.
    s += '''
void ble_midi_drop_host(void)
{
    os_mutex_pend(&mutex, 0);
    if (con_handle) ble_op_disconnect(con_handle);
    os_mutex_post(&mutex);
}
int ble_midi_can_forget(void)
{
    if (con_handle) return 0;
#if BLE_MIDI_CENTRAL
    if (mc_handle || mc_state == BLE_MC_CONNECTING || mc_state == BLE_MC_SEARCHING) return 0;
#endif
    return 1;
}
int ble_midi_forget_all(void)
{
    if (!ble_midi_can_forget()) return 0;
    return ble_list_clear_all(); /* SDK API clears bond keys in RAM and VM */
}
void ble_midi_reconnect(void)
{
    os_mutex_pend(&mutex, 0);
    adv_ctrl_en = 1;
#if BLE_MIDI_CENTRAL
    if (mc_state == BLE_MC_CONNECTING || mc_state == BLE_MC_SEARCHING) {
        if (con_handle) ble_op_disconnect(con_handle);
        os_mutex_post(&mutex);
        return; /* client search owns the advertising pause */
    }
#endif
    if (con_handle)
        ble_op_disconnect(con_handle);
    else {
        bt_ble_adv_enable(0);
        bt_ble_adv_enable(1);
    }
    os_mutex_post(&mutex);
}
'''
    p.write_text('/* Saadet connection adaptations Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n'+s)
    p=work/'device/radio.c';s=p.read_text()
    s=replace(s,'ble_in_q[bi_w % BQ] = pkt & ~0xF0u;', 'ble_in_q[bi_w % BQ] = pkt; /* internal cable tag identifies the BLE link */')
    s=replace(s,'BT ready: advertising as FM-1','BT ready: advertising as Saadet')
    s=replace(s,'static volatile uint8_t ble_state;', 'static volatile uint8_t ble_state;\nstatic uint8_t ble_off;')
    s=replace(s,'static uint8_t ble_off;                           /* switched off by the player (plat_ble_enable) */', '')
    s=replace(s,'        bt_ble_init();\n        ble_state = BLE_READY;', '''        bt_ble_init();
        if (ble_off) {
            extern void ble_midi_enable(int on);
            ble_midi_enable(0); /* an OFF requested during startup still wins */
        }
        ble_state = BLE_READY;''')
    p.write_text('/* Saadet radio naming Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n'+s)

    p=work/'host/plat_host.c';s=p.read_text()
    s=replace(s,'        } else if (!strcmp(cmd, "midi")) {', '''        } else if (!strcmp(cmd, "mididev")) {
            min_q[mi_w % MQ] = mk_pkt(a, b, c) | 0x10u;
            min_port[mi_w++ % MQ] = PORT_BLE;
            run_ms(2);
        } else if (!strcmp(cmd, "midi")) {''')
    s=replace(s,'''        } else if (!strcmp(cmd, "expect_out")) {
            fails += expect_out(a, mk_pkt(b, c, d));''','''        } else if (!strcmp(cmd, "expect_out")) {
            fails += expect_out(a, mk_pkt(b, c, d));
        } else if (!strcmp(cmd, "expect_unsent")) {   /* this message did not go out to PORT since the last "out" */
            uint32_t i, pkt = mk_pkt(b, c, d);
            int p = find_name(PORT_N, NPORTS, a), sent = p < 0;
            for (i = 0; !sent && i < n_out; i++)
                sent = ((out_pkt[i] ^ pkt) & 0xFFFFFF00u) == 0 && (out_to[i] >> p & 1u);
            printf(sent ? "FAIL expect_unsent %s %02X %02X %02X: sent\\n" : "  ok unsent %s %02X %02X %02X\\n", a,
                   (pkt >> 8) & 0xFF, (pkt >> 16) & 0xFF, pkt >> 24);
            fails += sent;''')
    p.write_text('/* Saadet dual BLE ingress and thru fixture Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n'+s)

    p=work/'bsp/chip/fm1_sys.h';s=p.read_text()
    for name in ('fm1_reboot','fm1_enter_uboot'):
        marker=f'static void {name}(void)\n{{'
        s=replace(s,marker,marker+'\n#ifdef APP_CSTART\n    sd_worker_stop();\n#endif')
    s=replace(s,'FM1_INLINE void fm1_core_reset(void)\n{','FM1_INLINE void fm1_core_reset(void)\n{\n#ifdef APP_CSTART\n    sd_worker_stop();\n#endif')
    p.write_text('/* Saadet CPU1 reset lifecycle Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n'+s)
