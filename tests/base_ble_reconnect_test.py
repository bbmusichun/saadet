#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Run the shipping reconnect policy and address bridge with controlled IO."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'build/fm1-base-platform/ble/midi/le_midi.c').read_text()


def routine(signature):
    start = source.rfind('\n', 0, source.index(signature)) + 1
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


code = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef uint8_t u8;
enum {BLE_DEV_OFF,BLE_DEV_SCANNING,BLE_DEV_CONNECTING,BLE_DEV_SEARCHING,BLE_DEV_READY};
enum {BLE_MC_OFF,BLE_MC_SCANNING,BLE_MC_CONNECTING,BLE_MC_SEARCHING,BLE_MC_READY};
#define BLE_READY 2
#define FM1B_BLE_CENTRAL 1
enum {OBJ_BT_LAST=2};
static uint32_t now;
static int radio=BLE_READY,mc_state,mc_handle;
static unsigned creates,saves,scans;
static int store_fail,stored_len=-1;
static uint8_t stored[64];
static char sd_mc_peer_name[21];
static uint8_t mc_tries;
static struct {uint16_t conn_interval,conn_latency,supervision_timeout;
    uint8_t peer_address_type,peer_address[6];} mc_conn;
static int mc_ready(void){return mc_state==BLE_MC_READY&&mc_handle;}
static void mc_create(void){creates++;mc_state=BLE_MC_CONNECTING;}
static void ble_midi_scan(int on){assert(!on);scans++;if(mc_state==BLE_MC_SCANNING)mc_state=BLE_MC_OFF;}
static uint32_t plat_ms(void){return now;}
static int plat_ble_state(void){return radio;}
static int plat_ble_device(void){return mc_state;}
static const char *btdev_name(void){return "Manual pick";}
static int plat_store_load(uint32_t o,void *p,uint32_t n){
    assert(o==OBJ_BT_LAST);if(stored_len<0)return -1;
    if(n>(uint32_t)stored_len)n=(uint32_t)stored_len;
    memcpy(p,stored,n);return n;
}
static int plat_store_save(uint32_t o,const void *p,uint32_t n){
    assert(o==OBJ_BT_LAST&&n<=sizeof stored);saves++;
    if(store_fail)return -1;memcpy(stored,p,n);stored_len=n;return 0;
}
''' + routine('int saadet_ble_peer(') + routine('int saadet_ble_connect_peer(') + r'''
#include "ble_reconnect.h"
static void boot(void){
    memset(&sd_bt_last,0,sizeof sd_bt_last);
    sd_bt_loaded=sd_bt_step=sd_bt_was_ready=sd_bt_display=sd_bt_inflight=sd_bt_save_failed=0;
    sd_bt_run=1;sd_bt_wait=1000;sd_bt_at=sd_bt_save_at=0;
    mc_state=BLE_MC_OFF;mc_handle=0;now=0;radio=BLE_READY;
}
static void connected(unsigned id){
    memset(&mc_conn,0,sizeof mc_conn);mc_conn.peer_address[0]=(uint8_t)id;
    mc_conn.peer_address[5]=42;mc_conn.peer_address_type=1;
    strcpy(sd_mc_peer_name,"Controller");mc_state=BLE_MC_READY;mc_handle=42;
}
int main(void){
    /* No stored device: no unsolicited connections or flash writes. */
    boot();sd_bt_poll(1);now=90000;sd_bt_poll(1);assert(!creates&&!saves);
    /* A failed selection must never replace a working target. */
    mc_state=BLE_MC_SEARCHING;sd_bt_poll(1);assert(!saves);
    connected(7);sd_bt_poll(1);assert(saves==1&&sd_bt_last.address[0]==7);
    sd_bt_poll(1);assert(saves==1); /* no flash wear on repeated frames */
    boot();radio=1;sd_bt_poll(1);now=10000;sd_bt_poll(1);assert(!creates);
    radio=BLE_READY;sd_bt_poll(1);assert(creates==1&&mc_state==BLE_MC_CONNECTING);
    assert(mc_conn.peer_address[0]==7&&mc_conn.peer_address[5]==42&&mc_conn.peer_address_type==1);
    assert(mc_conn.conn_interval==24&&mc_conn.supervision_timeout==400&&!mc_tries);
    assert(!strcmp(sd_bt_name(),"Controller"));
    /* Backoff starts after failure, including setup and transport timeouts. */
    const uint32_t delays[]={2000,4000,8000,16000,30000,30000};
    for(unsigned i=0;i<6;i++){
        now+=31000;sd_bt_poll(1);assert(creates==i+1);
        mc_state=BLE_MC_OFF;mc_handle=0;sd_bt_poll(1);
        now+=delays[i]-1;sd_bt_poll(1);assert(creates==i+1);
        now++;sd_bt_poll(1);assert(creates==i+2);
    }
    /* Ready resets backoff, and unexpected link loss reconnects. */
    connected(7);sd_bt_poll(1);assert(saves==1);
    mc_handle=0;mc_state=BLE_MC_OFF;sd_bt_poll(1);
    unsigned n=creates;now+=1999;sd_bt_poll(1);assert(creates==n);
    now++;sd_bt_poll(1);assert(creates==n+1);
    /* Explicit disconnect and radio off stay quiet; on/retry resumes. */
    sd_bt_pause();mc_state=BLE_MC_OFF;sd_bt_poll(1);now+=60000;sd_bt_poll(1);assert(creates==n+1);
    sd_bt_resume();now+=1000;sd_bt_poll(0);assert(creates==n+1);
    sd_bt_poll(1);assert(creates==n+2);
    /* A new successful device replaces the old one. Failed writes are bounded. */
    connected(9);store_fail=1;sd_bt_poll(1);assert(sd_bt_last.address[0]==7&&saves==2);
    sd_bt_poll(1);now+=29999;sd_bt_poll(1);assert(saves==2);
    store_fail=0;now++;sd_bt_poll(1);assert(saves==3&&sd_bt_last.address[0]==9);
    boot();sd_bt_poll(1);now=1000;sd_bt_poll(1);assert(mc_conn.peer_address[0]==9);
    /* Scanning is stopped before connecting; an in-flight link is untouched. */
    uint8_t addr[6]={11,0,0,0,0,42};char name[21]="Other";
    assert(saadet_ble_connect_peer(addr,0,name)==-1);
    mc_state=BLE_MC_SCANNING;assert(!saadet_ble_connect_peer(addr,0,name)&&scans);
    assert(mc_conn.peer_address[0]==11&&!mc_conn.peer_address_type);
    /* Forget failure keeps retries stopped; success survives a fresh boot. */
    store_fail=1;assert(!sd_bt_forget()&&!sd_bt_run&&sd_bt_last.valid);
    store_fail=0;assert(sd_bt_forget()&&!sd_bt_last.valid);
    boot();sd_bt_poll(1);now=100000;sd_bt_poll(1);n=creates;assert(!sd_bt_last.valid);
    /* Invalid/truncated/version-mismatched records never initiate a link. */
    memset(stored,255,sizeof stored);stored_len=sizeof sd_bt_last;
    boot();sd_bt_poll(1);now=10000;sd_bt_poll(1);assert(creates==n);
    stored_len=4;memcpy(stored,"SBT1",4);boot();sd_bt_poll(1);now=10000;sd_bt_poll(1);assert(creates==n);
    /* Unsigned elapsed arithmetic across the millisecond clock wrap. */
    connected(13);sd_bt_poll(1);boot();now=0xfffffff0u;sd_bt_poll(1);
    now+=999;sd_bt_poll(1);assert(creates==n);now++;sd_bt_poll(1);assert(creates==n+1);
    puts("BLE reconnect: persistence, exact peer/type, boot, backoff, loss, cancellation, forget, write failures and clock wrap passed");
}
'''
with tempfile.TemporaryDirectory() as d:
    path = Path(d)
    (path / 'test.c').write_text(code)
    subprocess.run([os.getenv('CC', 'cc'), '-O1', '-g', '-fsanitize=address,undefined',
                    '-I'+str(ROOT/'platform'), str(path/'test.c'), '-o', str(path/'test')], check=True)
    subprocess.run([str(path/'test')], check=True)
