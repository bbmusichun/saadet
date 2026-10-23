#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Run the shipping shim timer and peripheral helpers with controlled IO."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/fm1-base-platform'


def routine(source, signature):
    start = source.index(signature)
    start = source.rfind('\n', 0, start) + 1
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def run(code, flags=()):
    with tempfile.TemporaryDirectory() as d:
        path = Path(d)
        (path / 'test.c').write_text(code)
        subprocess.run([os.getenv('CC', 'cc'), '-O1', '-g', '-fsanitize=address,undefined',
                        '-I', str(BASE / 'ble/shim'), *flags, str(path / 'test.c'),
                        '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True)


timers = (BASE / 'ble/shim/systimer.c').read_text()
code = r'''
#include <assert.h>
#include <stdio.h>
#include "shim.h"
static uint32_t now;
static unsigned fired, posted;
volatile unsigned long jiffies;
uint32_t shim_ms(void){return now;}
uint32_t shim_irq_save(void){return 0;}
void shim_irq_restore(uint32_t f){(void)f;}
void shim_log(const char *s){(void)s;assert(0);}
int shim_cur_index(void){return 2;}
int os_taskq_post_type(const char *name,int type,int n,int *args){
    (void)type;(void)n;(void)args;assert(name);posted++;return OS_NO_ERR;
}
int shim_task_post_idx(int i,int type,int n,int *args){
    (void)type;(void)n;(void)args;assert(i==2);posted++;return OS_NO_ERR;
}
static void count(void *p){fired+=(unsigned)(uintptr_t)p;}
''' + timers + r'''
int main(void){
    /* The named default timer service must work in a BLE-only build. */
    u16 id=sys_timer_add_to_task("sys_timer",(void *)1,count,10);
    assert(id);now=9;shim_timers();assert(!fired&&!posted);
    now=10;shim_timers_post();
#if SHIM_WIFI
    assert(posted==1&&fired==0); /* WiFi keeps its real task, and pending suppresses repeats. */
    now=30;shim_timers();assert(posted==1);
    fire((void *)(uintptr_t)id);assert(fired==1);
#else
    assert(!fired&&!posted); /* no callback from a task's idle turn */
    shim_timers();assert(fired==1&&!posted);
    now=20;shim_timers();assert(fired==2);
#endif
    sys_timer_del(id);
    fired=posted=0;
    id=sys_timeout_add_to_task("sys_timer",(void *)3,count,5);
    now+=5;shim_timers();
#if SHIM_WIFI
    assert(posted==1&&!fired);fire((void *)(uintptr_t)id);
#endif
    assert(fired==3);now+=100;shim_timers();assert(fired==3);
    /* Explicit Bluetooth owners still get queued, never run in the caller. */
    fired=posted=0;
    id=sys_timer_add_to_task("btstack",(void *)1,count,1);
    now++;shim_timers();assert(!fired&&posted==1);
    now++;shim_timers();assert(posted==1);
    sys_timer_del(id);
    /* Cancellation before due, and wraparound of the clock. */
    now=0xfffffff0u;id=sys_timeout_add_to_task("sys_timer",(void *)1,count,32);
    now=0;shim_timers();assert(!fired);
    sys_timeout_del(id);now=16;shim_timers();assert(!fired);
    puts("BLE timers: default service, task ownership, pending, cancellation and clock wrap passed");
}
'''
run(code)
run(code, ['-DSHIM_WIFI=1'])

radio = (BASE / 'device/radio.c').read_text()
run(r'''
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#define BLE_OFF 0
#define BLE_STARTING 1
#define BLE_READY 2
static unsigned ble_events,ble_state,ble_off,advertising,starts;
uint32_t shim_heap_peak;
struct bt_event_s { uint8_t event; uint8_t args[7]; uint32_t value; };
static void shim_log(const char *s){(void)s;}
static char *put_dec(char *s,uint32_t v){return s+snprintf(s,12,"%u",v);}
static void bt_ble_init(void){starts++;advertising=1;}
void ble_midi_enable(int on){advertising=(unsigned)on;}
''' + routine(radio, 'void shim_bt_event(') + r'''
int main(void){
    struct bt_event_s e={3,{0},0};
    ble_state=BLE_STARTING;ble_off=1;
    shim_bt_event(3,&e,sizeof e);
    assert(starts==1&&ble_state==BLE_READY&&!advertising);
    ble_state=BLE_STARTING;ble_off=0;
    shim_bt_event(3,&e,sizeof e);
    assert(starts==2&&ble_state==BLE_READY&&advertising);
    shim_bt_event(3,&e,sizeof e);assert(starts==2);
    ble_state=BLE_STARTING;shim_bt_event(2,&e,sizeof e);assert(starts==2);
    puts("BLE startup: off during initialization remains off, normal startup advertises");
}
''')


midi = (BASE / 'ble/midi/le_midi.c').read_text()
handler = midi[midi.index('static void server_profile_start'):]
disconnect = handler[handler.index('    case HCI_EVENT_DISCONNECTION_COMPLETE:'):
                     handler.index('    case ATT_EVENT_MTU_EXCHANGE_COMPLETE:')]
code = r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;
typedef uint16_t hci_con_handle_t;
#define H_NAME 3
#define H_MIDI 6
#define H_CCC 7
#define BLE_MIDI_CENTRAL 0
#define BLE_ST_CONNECT 1
#define BLE_ST_DISCONN 2
#define HCI_EVENT_DISCONNECTION_COMPLETE 1
#define ATT_RAM_BUFSIZE 128
#define ATT_LOCAL_MTU_SIZE 64
#define CCC_GET(h,a) ccc
#define CCC_SET(h,a,v) (ccc=(v))
static uint8_t ccc,send_mtu,connection_update_cnt,adv_ctrl_en,mutex,att_ram_buffer[128];
static uint16_t con_handle;
static const char gap_device_name[]="Saadet";
static unsigned disconnects,advertise,init_handle;
static uint8_t ble_work_state;
static void os_mutex_pend(uint8_t *m,int n){(void)m;(void)n;}
static void os_mutex_post(uint8_t *m){(void)m;}
static void ble_op_disconnect(uint16_t h){assert(h==con_handle&&h);disconnects++;}
static void bt_ble_adv_enable(uint8_t on){advertise=(advertise<<1)|on;}
static void ble_op_att_send_init(uint16_t h,void *p,int n,int mtu){(void)p;(void)n;(void)mtu;init_handle=h;}
static void set_ble_work_state(int s){ble_work_state=(uint8_t)s;}
static uint16_t little_endian_read_16(const uint8_t *p,unsigned i){return p[i]|(uint16_t)p[i+1]<<8;}
''' + '\n'.join(routine(midi, x) for x in (
    'static int periph_ready(void)', 'static uint16_t att_read_callback(',
    'static void server_profile_start(', 'void ble_midi_reconnect(void)')) + (
    '\nstatic void disconnect_event(uint8_t *packet){switch(packet[0]){\n' + disconnect + '}}\n') + r'''
int main(void){
    uint8_t b[64];memset(b,0x55,sizeof b);
    assert(att_read_callback(0,H_NAME,0,0,0)==6);
    assert(att_read_callback(0,H_NAME,0,b,sizeof b)==6&&!memcmp(b,"Saadet",6)&&b[6]==0x55);
    assert(att_read_callback(0,H_NAME,2,b,sizeof b)==4&&!memcmp(b,"adet",4));
    assert(att_read_callback(0,H_NAME,6,b,sizeof b)==0);
    assert(att_read_callback(0,H_NAME,7,b,sizeof b)==0);
    assert(att_read_callback(0,H_NAME,0,b,2)==2&&!memcmp(b,"Sa",2));
    con_handle=42;ccc=1;send_mtu=64;connection_update_cnt=4;
    server_profile_start(con_handle);
    assert(init_handle==42&&!ccc&&send_mtu==20&&!connection_update_cnt&&!periph_ready());
    ccc=2;assert(!periph_ready());ccc=1;assert(periph_ready());
    ble_midi_reconnect();assert(disconnects==1&&adv_ctrl_en==1&&!advertise);
    /* A stale event cannot drop the new link; a matching one advertises. */
    uint8_t event[6]={HCI_EVENT_DISCONNECTION_COMPLETE,0,0,41,0,0x13};
    disconnect_event(event);assert(con_handle==42&&!advertise);
    event[3]=42;disconnect_event(event);
    assert(!con_handle&&advertise==1&&!init_handle&&ble_work_state==BLE_ST_DISCONN);
    con_handle=0;advertise=0;ble_midi_reconnect();assert(disconnects==1&&advertise==1&&adv_ctrl_en==1);
    puts("BLE peripheral: ATT name discovery, fresh-link subscription/MTU and reconnect passed");
}
'''
run(code)
