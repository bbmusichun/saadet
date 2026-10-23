/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Main-loop policy for the outgoing MIDI-device link. The incoming phone/
 * computer link remains host-initiated. Storage uses its own atomic A/B pair. */
#if FM1B_BLE_CENTRAL && !defined(PLAT_HOST)
extern int saadet_ble_peer(uint8_t *address,uint8_t *type,char *name);
extern int saadet_ble_connect_peer(const uint8_t *address,uint8_t type,const char *name);
static struct sd_bt_record {
    uint8_t tag[4],address[6],type,valid;
    char name[21];
} sd_bt_last;
static uint8_t sd_bt_loaded,sd_bt_run=1,sd_bt_step,sd_bt_was_ready,sd_bt_display,sd_bt_inflight;
static uint32_t sd_bt_at,sd_bt_wait=1000u,sd_bt_save_at;
static uint8_t sd_bt_save_failed;

static void sd_bt_resume(void)
{
    sd_bt_run=1;sd_bt_step=0;sd_bt_at=plat_ms();sd_bt_wait=1000u;
}
static void sd_bt_pause(void){sd_bt_run=0;}
static int sd_bt_forget(void)
{
    struct sd_bt_record empty={0};
    sd_bt_pause();
    if(plat_store_save(OBJ_BT_LAST,&empty,sizeof empty))return 0;
    sd_bt_last=empty;sd_bt_display=0;
    return 1;
}
static void sd_bt_poll(int on)
{
    uint32_t now=plat_ms();
    if(!sd_bt_loaded){
        struct sd_bt_record r={0};
        if(plat_store_load(OBJ_BT_LAST,&r,sizeof r)==sizeof r&&
           !memcmp(r.tag,"SBT1",4)&&r.valid==1&&r.type<=1&&r.name[20]==0)
            sd_bt_last=r;
        sd_bt_loaded=1;sd_bt_at=now;
    }
    if(!on||plat_ble_state()!=BLE_READY)return;
    int state=plat_ble_device();
    if(state==BLE_DEV_READY){
        struct sd_bt_record r={.tag={'S','B','T','1'},.valid=1};
        if(saadet_ble_peer(r.address,&r.type,r.name)&&
           memcmp(&r,&sd_bt_last,sizeof r)&&
           (!sd_bt_save_failed||now-sd_bt_save_at>=30000u)){
            sd_bt_save_at=now;
            sd_bt_save_failed=!!plat_store_save(OBJ_BT_LAST,&r,sizeof r);
            if(!sd_bt_save_failed)sd_bt_last=r;
        }
        sd_bt_was_ready=1;sd_bt_step=0;sd_bt_inflight=0;
        return;
    }
    if(state==BLE_DEV_CONNECTING||state==BLE_DEV_SEARCHING){sd_bt_inflight=1;return;}
    if(sd_bt_inflight){sd_bt_inflight=0;sd_bt_at=now;return;}
    if(sd_bt_was_ready){
        sd_bt_was_ready=0;sd_bt_at=now;sd_bt_wait=2000u;
    }
    if(!sd_bt_run||!sd_bt_last.valid||
       (state!=BLE_DEV_OFF&&state!=BLE_DEV_SCANNING)||now-sd_bt_at<sd_bt_wait)return;
    sd_bt_at=now;
    /* Retry indefinitely, with bounded radio activity: 2,4,8,16,30 seconds.
     * Each connection/setup has its own existing timeout. No blocking waits. */
    sd_bt_wait=sd_bt_step<4?(2000u<<sd_bt_step):30000u;
    if(sd_bt_step<4)sd_bt_step++;
    if(!saadet_ble_connect_peer(sd_bt_last.address,sd_bt_last.type,sd_bt_last.name)){
        sd_bt_display=1;sd_bt_inflight=1;
    }
}
static const char *sd_bt_name(void){return sd_bt_display?sd_bt_last.name:btdev_name();}
#else
static void sd_bt_resume(void){}
static void sd_bt_pause(void){}
static int sd_bt_forget(void){return 1;}
static void sd_bt_poll(int on){(void)on;}
static const char *sd_bt_name(void){return btdev_name();}
#endif
