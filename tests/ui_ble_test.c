/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#include <assert.h>
#define SAADET_FM1_BASE 1
static int radio_available=1,radio_on=1,radio_state=2,radio_link,radio_midi,retries;
static int sd_ble_available(void){return radio_available;}
static int sd_ble_enabled(void){return radio_on;}
static int sd_ble_state(void){return radio_state;}
static int sd_ble_linked(void){return radio_link;}
static int sd_ble_connected(void){return radio_midi;}
static void sd_ble_enable(int on){radio_on=on;radio_state=on?2:0;if(!on)radio_link=radio_midi=0;}
static void sd_ble_reconnect(void){retries++;sd_ble_enable(0);sd_ble_enable(1);}
#include "ui_harness.c"
static void screen_check(const char *status,const char *image){
    frame();if(!lint_has(status)){fprintf(stderr,"Missing %s\n",status);for(unsigned i=0;i<lint_ns;i++)fprintf(stderr,"text %s\n",lint_scr[i].s);}assert(lint_has(status));assert(!lint_check(image));ui_png(image);
}
int main(void){
    power_on();frames(800);ui_open(PL_GLO);
    uint32_t page=0;while(page<NELEM(ST_GLO)&&ST_GLO[page].pic!=PIC_BLE)page++;
    assert(page<NELEM(ST_GLO));uix.stop[PL_GLO]=(uint8_t)page;
    screen_check("ADVERTISING","ble_advertising");
    radio_link=1;screen_check("WAITING FOR MIDI","ble_link_only");
    assert(lint_has("Audio MIDI Setup"));
    radio_midi=1;screen_check("MIDI CONNECTED","ble_midi_ready");
    turn(EN_K4,-1);assert(!retries&&radio_midi);
    turn(EN_K4,1);assert(retries==1&&!radio_link&&!radio_midi&&radio_on);
    screen_check("ADVERTISING","ble_retry");
    turn(EN_K1,-1);screen_check("OFF","ble_off");assert(!radio_on);
    turn(EN_K1,1);screen_check("ADVERTISING","ble_on");
    radio_state=0;screen_check("STOPPED","ble_stopped");
    turn(EN_K4,1);assert(radio_state==2&&retries==2);
    radio_state=1;screen_check("STARTING","ble_starting");
    radio_state=3;screen_check("FAILED","ble_failed");
    radio_available=0;screen_check("UNAVAILABLE","ble_unavailable");
    turn(EN_K4,1);assert(retries==2);
    puts("BLE UI: live state, link without MIDI, radio controls, retry and screen layout passed");
}
