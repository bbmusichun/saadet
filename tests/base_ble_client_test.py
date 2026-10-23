#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Exercise the target MIDI client's acknowledged handshake, with SDK IO controlled."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/fm1-base-platform'
source = (BASE / 'ble/midi/le_midi.c').read_text()


def routine(signature):
    start = source.index(signature)
    start = source.rfind('\n', 0, start) + 1
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


handler = routine('static void client_packet_handler(')
start = handler.index('    case HCI_EVENT_ENCRYPTION_CHANGE:')
end = handler.index('    case ATT_EVENT_MTU_EXCHANGE_COMPLETE:', start)
encryption_case = handler[start:end]

code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;
enum {BLE_MC_OFF,BLE_MC_SCANNING,BLE_MC_CONNECTING,BLE_MC_SEARCHING,BLE_MC_READY};
#define HCI_EVENT_PACKET 4
#define GATT_EVENT_QUERY_COMPLETE 0xa0
#define BLE_MIDI_CENTRAL 1
#define SM_EVENT_JUST_WORKS_REQUEST 0xd0
#define SM_EVENT_PASSKEY_DISPLAY_NUMBER 0xd1
#define SM_EVENT_PASSKEY_DISPLAY_CANCEL 0xd2
#define SM_EVENT_PASSKEY_INPUT_NUMBER 0xd3
#define SM_EVENT_PAIR_PROCESS 0xdf
#define log_info(...) ((void)0)
#define HCI_EVENT_ENCRYPTION_CHANGE 8
#define HCI_EVENT_LE_META 0x3e
#define GAP_EVENT_ADVERTISING_REPORT 0xe2
#define HCI_SUBEVENT_LE_CONNECTION_COMPLETE 1
#define HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE 10
typedef void (*callback_t)(u8,u16,u8 *,u16);
typedef struct {u16 uuid16;u8 uuid128[16];} service_t;
typedef struct {u16 value_handle,end_handle,uuid16;u8 uuid128[16];} characteristic_t;
typedef struct {service_t services;characteristic_t characteristic;} search_result_t;
typedef struct {u16 handle,uuid16;u8 uuid128[16];} charact_descriptor_t;
typedef struct {u8 type,size;u16 con_handle;u8 addr_type,address[6],data[4];} sm_just_event_t;
static u8 mc_state,mc_tries;
static u16 mc_handle,mc_value;
static u32 now,mc_t0;
static const u8 midi_svc_le[16]={1,2,3},midi_chr_le[16]={4,5,6};
static unsigned reads,writes,pairs,drops,cancels,advertising;
static u8 busy;
static int pairing_ok=1;
static callback_t completion;
static int uuid_is(const u8 *a,const u8 *b){return !memcmp(a,b,16);}
static u16 little_endian_read_16(const u8 *p,unsigned i){return p[i]|(u16)p[i+1]<<8;}
static u32 shim_ms(void){return now;}
static u8 hci_event_packet_get_type(const u8 *p){return p[0];}
static u8 hci_event_le_meta_get_subevent_code(const u8 *p){return p[2];}
static u8 hci_subevent_le_connection_complete_get_role(const u8 *p){return p[6];}
static u16 sm_event_just_works_request_get_handle(const u8 *p){return little_endian_read_16(p,2);}
static void sm_just_works_confirm(u16 h){(void)h;}
static void cbk_packet_handler(u8 type,u16 channel,u8 *p,u16 n){(void)type;(void)channel;(void)p;(void)n;}
static void ble_op_disconnect(u16 h){assert(h==mc_handle&&h);drops++;}
static void ble_op_create_connection_cancel(void){cancels++;}
static void bt_ble_adv_enable(u8 on){advertising=on;}
static int sm_api_request_pairing(u16 h){assert(h==mc_handle&&h);pairs++;return pairing_ok;}
static u8 gatt_client_read_value_of_characteristic_using_value_handle(callback_t cb,u16 h,u16 value){
    assert(h==mc_handle&&value==11);reads++;completion=cb;return busy;
}
static u8 gatt_client_write_value_of_characteristic(callback_t cb,u16 h,u16 value,u16 n,u8 *data){
    assert(h==mc_handle&&value==15&&n==2&&data[0]==1&&!data[1]);writes++;completion=cb;return busy;
}
#include "ble_client.h"
''' + '''
static void client_packet_handler(u8 type,u16 channel,u8 *packet,u16 size){
    (void)channel;
    if(type==HCI_EVENT_PACKET)switch(packet[0]){
''' + encryption_case + '}}\n' + routine('static void both_packet_handler(') + '\n'.join(routine(s) for s in (
    'static void cbk_sm_packet_handler(',
    'void ble_midi_poll(void)', 'void ble_midi_disconnect(void)',
    'void user_client_report_search_result(search_result_t *r)',
    'void user_client_report_descriptor_result(charact_descriptor_t *d)')) + r'''
static void result(u8 status,u16 h){
    u8 p[]={GATT_EVENT_QUERY_COMPLETE,3,h&255,h>>8,status};
    assert(completion);completion(HCI_EVENT_PACKET,0,p,sizeof p);
}
static void discover(void){
    sd_mc_reset();mc_handle=0x50;mc_value=0;mc_state=BLE_MC_SEARCHING;
    sd_mc_phase=SD_MC_DISCOVER;mc_t0=now;advertising=0;busy=0;
    search_result_t r={0};
    memcpy(r.services.uuid128,midi_svc_le,16);
    memcpy(r.characteristic.uuid128,midi_chr_le,16);
    r.characteristic.value_handle=11;r.characteristic.end_handle=19;
    /* A matching characteristic under a different service is not MIDI. */
    r.services.uuid16=0x1800;user_client_report_search_result(&r);assert(!mc_value);
    r.services.uuid16=0;user_client_report_search_result(&r);assert(mc_value==11);
    /* Use the actual CCC descriptor, not the adjacent attribute. */
    charact_descriptor_t d={20,0x2902,{0}};
    user_client_report_descriptor_result(&d);assert(!sd_mc_ccc);
    d.handle=15;d.uuid16=0x2901;user_client_report_descriptor_result(&d);assert(!sd_mc_ccc);
    d.uuid16=0x2902;user_client_report_descriptor_result(&d);assert(sd_mc_ccc==15);
    unsigned n=reads;user_client_report_search_result((void *)-1);
    assert(sd_mc_phase==SD_MC_READ&&reads==n&&mc_state==BLE_MC_SEARCHING);
}
static void encrypted_n(u8 status,u8 on,u16 h,u16 size){
    u8 p[]={8,4,status,h&255,h>>8,on};both_packet_handler(HCI_EVENT_PACKET,0,p,size);
}
static void encrypted(u8 status,u8 on,u16 h){encrypted_n(status,on,h,6);}
static void bonded(u16 h){
    sm_just_event_t e={0};e.type=SM_EVENT_PAIR_PROCESS;e.con_handle=h;e.data[0]=0x10;
    cbk_sm_packet_handler(HCI_EVENT_PACKET,0,(u8 *)&e,sizeof e);
}
static void passkey(u16 handle,u32 pin,u16 size,int display){
    u8 e[16]={0};e[0]=display?SM_EVENT_PASSKEY_DISPLAY_NUMBER:SM_EVENT_PASSKEY_DISPLAY_CANCEL;
    e[1]=14;e[2]=handle&255;e[3]=handle>>8;
    for(unsigned i=0;i<4;i++)e[11+i]=(u8)(pin>>(8*i));
    cbk_sm_packet_handler(HCI_EVENT_PACKET,0,e,size);
}
static void settle(void){
    assert(sd_mc_phase==SD_MC_SECURITY);unsigned r=reads;
    ble_midi_poll();assert(reads==r);
    now+=199;ble_midi_poll();assert(reads==r&&sd_mc_phase==SD_MC_SECURITY);
    now++;ble_midi_poll();assert(reads==r+1&&sd_mc_phase==SD_MC_READING);
}
int main(void){
    discover();ble_midi_poll();assert(reads==1&&!writes&&mc_state!=BLE_MC_READY);
    /* Neither an unrelated link nor a malformed event completes the read. */
    result(0,0x51);assert(sd_mc_phase==SD_MC_READING);
    u8 short_event[]={GATT_EVENT_QUERY_COMPLETE};sd_mc_att(HCI_EVENT_PACKET,0,short_event,1);
    assert(sd_mc_phase==SD_MC_READING);
    result(0,mc_handle);assert(sd_mc_phase==SD_MC_SUBSCRIBE&&!writes&&!advertising);
    ble_midi_poll();assert(writes==1&&mc_state!=BLE_MC_READY);
    result(0,mc_handle);assert(mc_state==BLE_MC_READY&&advertising&&!pairs);

    /* iOS-style authentication errors on either read or subscription trigger
     * one pairing attempt, then repeat the handshake after encryption. */
    const u8 auth[]={5,12,15};
    for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<3;i++){
        discover();ble_midi_poll();
        if(stage){result(0,mc_handle);ble_midi_poll();}
        unsigned p=pairs;result(auth[i],mc_handle);
        assert(sd_mc_phase==SD_MC_PAIR&&mc_state!=BLE_MC_READY&&!advertising);
        ble_midi_poll();assert(pairs==p+1&&sd_mc_phase==SD_MC_PAIRING);
        now+=20000;ble_midi_poll();assert(sd_mc_phase==SD_MC_PAIRING);
        encrypted(0,1,0x51);assert(sd_mc_phase==SD_MC_PAIRING);
        /* The SDK HCI dispatcher reports 5 for this complete 6-byte event.
         * Exercise the actual shipping event branch and both-role router. */
        encrypted_n(0,1,mc_handle,i==0?5:6);assert(sd_mc_phase==SD_MC_READ);
        ble_midi_poll();result(0,mc_handle);ble_midi_poll();result(0,mc_handle);
        assert(mc_state==BLE_MC_READY&&advertising&&pairs==p+1);
    }
    /* Bond encryption may finish before an old unauthenticated ATT request
     * completes, or before the main loop sends its explicit pairing request. */
    discover();ble_midi_poll();unsigned old_pairs=pairs;
    encrypted_n(0,1,mc_handle,4);assert(!sd_mc_encrypted&&sd_mc_phase==SD_MC_READING);
    encrypted_n(0,1,mc_handle,5);assert(sd_mc_encrypted&&sd_mc_phase==SD_MC_READING);
    result(5,mc_handle);assert(sd_mc_phase==SD_MC_SECURITY&&pairs==old_pairs);
    settle();result(0,mc_handle);ble_midi_poll();result(0,mc_handle);
    assert(mc_state==BLE_MC_READY&&pairs==old_pairs);
    discover();ble_midi_poll();result(5,mc_handle);old_pairs=pairs;
    encrypted_n(0,1,mc_handle,5);assert(sd_mc_phase==SD_MC_READ);
    ble_midi_poll();result(0,mc_handle);ble_midi_poll();result(0,mc_handle);
    assert(mc_state==BLE_MC_READY&&pairs==old_pairs);
    /* Captured iPhone ordering: encryption succeeds, the retried read still
     * returns auth-required, then the SDK reports that bond keys were saved. */
    discover();ble_midi_poll();result(5,mc_handle);ble_midi_poll();
    old_pairs=pairs;encrypted_n(0,1,mc_handle,5);ble_midi_poll();unsigned old_drops=drops;
    result(5,mc_handle);assert(sd_mc_phase==SD_MC_SECURITY&&drops==old_drops);
    bonded(0x51);assert(sd_mc_phase==SD_MC_SECURITY);
    bonded(mc_handle);assert(sd_mc_phase==SD_MC_READ);
    ble_midi_poll();result(0,mc_handle);ble_midi_poll();result(0,mc_handle);
    assert(mc_state==BLE_MC_READY&&pairs==old_pairs&&drops==old_drops);
    /* Peers with no bond-complete event retry after settling; persistent
     * read errors reach CCC after the budget without asking to pair again. */
    discover();ble_midi_poll();encrypted_n(0,1,mc_handle,5);
    old_pairs=pairs;old_drops=drops;
    for(unsigned i=0;i<5;i++){
        result(5,mc_handle);assert(sd_mc_phase==SD_MC_SECURITY&&drops==old_drops);
        settle();
    }
    result(5,mc_handle);
    assert(sd_mc_phase==SD_MC_SUBSCRIBE&&drops==old_drops&&mc_state!=BLE_MC_READY);
    ble_midi_poll();assert(sd_mc_phase==SD_MC_SUBSCRIBING&&mc_state!=BLE_MC_READY);
    result(0,mc_handle);assert(mc_state==BLE_MC_READY&&drops==old_drops&&pairs==old_pairs);
    /* The captured persistent read rejection may permit CCC access; it must
     * never hide a rejected CCC or cause additional pairing prompts. */
    for(unsigned i=0;i<3;i++){
        discover();ble_midi_poll();encrypted_n(0,1,mc_handle,5);
        sd_mc_security_retry=5;old_drops=drops;old_pairs=pairs;
        result(auth[i],mc_handle);assert(sd_mc_phase==SD_MC_SUBSCRIBE);
        ble_midi_poll();result(auth[i],mc_handle);
        assert(drops==old_drops+1&&pairs==old_pairs&&mc_state!=BLE_MC_READY);
    }
    /* A non-readable/unsupported initial read still permits subscription.
     * If that subscription needs pairing, the normal security path remains. */
    const u8 no_read[]={2,6};
    for(unsigned i=0;i<2;i++){
        discover();ble_midi_poll();result(no_read[i],mc_handle);
        assert(sd_mc_phase==SD_MC_SUBSCRIBE&&mc_state!=BLE_MC_READY);
        ble_midi_poll();result(5,mc_handle);assert(sd_mc_phase==SD_MC_PAIR);
        ble_midi_poll();encrypted_n(0,1,mc_handle,5);ble_midi_poll();
        result(no_read[i],mc_handle);ble_midi_poll();result(0,mc_handle);
        assert(mc_state==BLE_MC_READY);
    }
    /* An ordinary ATT error does not ask the user to pair. */
    discover();ble_midi_poll();unsigned d=drops,p=pairs;result(3,mc_handle);
    assert(drops==d+1&&pairs==p&&mc_state!=BLE_MC_READY);
    discover();ble_midi_poll();result(5,mc_handle);pairing_ok=0;d=drops;
    ble_midi_poll();assert(drops==d+1&&sd_mc_phase==SD_MC_IDLE);pairing_ok=1;
    discover();ble_midi_poll();result(5,mc_handle);ble_midi_poll();d=drops;
    encrypted(6,0,mc_handle);assert(drops==d+1);

    /* Real SM callback: generated passkeys, zero/leading zeros, wrong link,
     * malformed events, cancellation, encryption, and a rejected ATT while
     * the peer is asking its user for the displayed code. */
    discover();assert(ble_midi_pairing_code()==-1);
    ble_midi_poll();result(5,mc_handle);ble_midi_poll();
    passkey(0x51,123,16,1);assert(ble_midi_pairing_code()==-1);
    passkey(mc_handle,123,14,1);assert(ble_midi_pairing_code()==-1);
    passkey(mc_handle,1000000,16,1);assert(ble_midi_pairing_code()==-1);
    passkey(mc_handle,123,16,1);assert(ble_midi_pairing_code()==123);
    passkey(mc_handle,1000000,16,1);assert(ble_midi_pairing_code()==123);
    assert(sd_mc_timeout()==30000u&&mc_state!=BLE_MC_READY);
    now+=20000;ble_midi_poll();assert(ble_midi_pairing_code()==123);
    passkey(0x51,0,16,0);assert(ble_midi_pairing_code()==123);
    passkey(mc_handle,0,16,0);assert(ble_midi_pairing_code()==-1);
    passkey(mc_handle,0,16,1);assert(ble_midi_pairing_code()==0);
    encrypted_n(0,1,mc_handle,5);assert(ble_midi_pairing_code()==-1);
    ble_midi_poll();result(0,mc_handle);ble_midi_poll();result(0,mc_handle);
    assert(mc_state==BLE_MC_READY);
    passkey(mc_handle,123,16,1);assert(ble_midi_pairing_code()==-1);
    discover();ble_midi_poll();old_pairs=pairs;passkey(mc_handle,999999,16,1);
    result(5,mc_handle);assert(sd_mc_phase==SD_MC_PAIRING);
    ble_midi_poll();assert(pairs==old_pairs&&ble_midi_pairing_code()==999999);
    ble_midi_disconnect();assert(ble_midi_pairing_code()==-1);
    passkey(mc_handle,123,16,1);assert(ble_midi_pairing_code()==-1);
    discover();ble_midi_poll();passkey(mc_handle,123,16,1);d=drops;
    now+=30001;ble_midi_poll();assert(drops==d+1&&ble_midi_pairing_code()==-1);

    /* Queue rejection and no response both remain unconnected and time out. */
    discover();busy=0x93;ble_midi_poll();assert(sd_mc_phase==SD_MC_READ&&mc_state!=BLE_MC_READY);
    busy=0;ble_midi_poll();assert(sd_mc_phase==SD_MC_READING);
    d=drops;now+=5001;ble_midi_poll();assert(drops==d+1&&mc_state!=BLE_MC_READY);
    discover();ble_midi_poll();result(0,mc_handle);ble_midi_poll();
    d=drops;now+=5001;ble_midi_poll();assert(drops==d+1&&mc_state!=BLE_MC_READY);
    discover();ble_midi_poll();result(5,mc_handle);ble_midi_poll();
    d=drops;now+=30001;ble_midi_poll();assert(drops==d+1);
    /* Cancellation suppresses queued operations, and late acknowledgements. */
    discover();ble_midi_poll();result(0,mc_handle);d=drops;
    ble_midi_disconnect();assert(drops==d+1&&sd_mc_phase==SD_MC_IDLE);
    unsigned w=writes;ble_midi_poll();result(0,mc_handle);
    assert(writes==w&&mc_state!=BLE_MC_READY);
    discover();ble_midi_poll();encrypted(0,1,mc_handle);result(5,mc_handle);
    ble_midi_disconnect();unsigned r=reads;bonded(mc_handle);now+=201;ble_midi_poll();
    assert(reads==r&&sd_mc_phase==SD_MC_IDLE&&mc_state!=BLE_MC_READY);
    /* A missing descriptor is a failure, not an invented value+1 write. */
    discover();sd_mc_phase=SD_MC_DISCOVER;sd_mc_ccc=0;d=drops;
    user_client_report_search_result((void *)-1);assert(drops==d+1);
    puts("BLE client: acknowledged read/subscription, security fallback, errors, cancellation and timeouts passed");
}
'''

with tempfile.TemporaryDirectory() as d:
    path = Path(d)
    # The target build checks the actual SDK declarations; controlled IO here
    # makes every asynchronous result independently testable under sanitizers.
    (path / 'btstack/le').mkdir(parents=True)
    (path / 'btstack/le/gatt.h').write_text('')
    (path / 'test.c').write_text(code)
    subprocess.run([os.getenv('CC', 'cc'), '-O1', '-g', '-fsanitize=address,undefined',
                    '-I', str(path), '-I', str(ROOT / 'platform'),
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
