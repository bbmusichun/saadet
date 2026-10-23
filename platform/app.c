/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Platform half of the bridge. The instrument is a separate translation unit,
 * keeping its retained formats/types apart from the base's internal names. */
#include "bridge.h"
void app_init(void){saadet_init();}
static void (*volatile sd_render_fn)(int32_t *,uint32_t,uint32_t)=saadet_render;
void app_render(int32_t *lr,uint32_t n,uint32_t gain){
    sd_render_fn(lr,n,gain);
#ifdef PLAT_HOST
    for(uint32_t i=0;i<2u*n;i++)lr[i]>>=1;
#endif
}
void app_panic(void){saadet_panic();}
void app_ui_init(void){saadet_frame();}
void app_frame(void){saadet_frame();}
void app_input_only(void){saadet_input_only();}
void app_say(const char *a,const char *b){saadet_say(a,b);}
int app_sysex(const uint8_t *p,uint32_t n){return saadet_sysex(p,n);}
int app_expect(const char *s,int *v){return saadet_expect(s,v);}
uint32_t sd_ms(void){return plat_ms();}
uint32_t sd_ticks(void){
#ifdef PLAT_HOST
    return plat_ms()*24000u;
#else
    return fm1_ticks();
#endif
}
uint32_t sd_lock(void){return plat_lock();}
void sd_unlock(uint32_t f){plat_unlock(f);}
void sd_watchdog(void){
#ifndef PLAT_HOST
    fm1_wdt_feed();
#endif
}
void sd_delay(uint32_t ms){
#ifndef PLAT_HOST
    fm1_delay_ms(ms);
#else
    (void)ms;
#endif
}
void sd_reboot(void){
#ifndef PLAT_HOST
    radio_clean_exit();fm1_reboot();
#endif
}
void sd_input(uint32_t *buttons,uint32_t *keys,int32_t *enc){
    static const uint8_t bm[14]={2,3,4,5,6,7,8,9,10,11,12,13,0,1};
    static const uint8_t em[7]={0,1,6,2,3,4,5};
    uint32_t b;*buttons=0;*keys=plat_keys();
#ifdef PLAT_HOST
    b=plat_buttons();
#else
    *buttons=fm1_in.buttons;b=remote.btn;
#endif
    for(uint32_t i=0;i<14u;i++)if(b&(1u<<i))*buttons|=1u<<bm[i];
    #ifdef PLAT_HOST
    for(uint32_t i=0;i<7u;i++)enc[em[i]]=plat_enc((int)i);
#else
    uint32_t f=plat_lock();
    for(uint32_t i=0;i<7u;i++)enc[i]=fm1_enc_take(i);
    for(uint32_t i=0;i<7u;i++){enc[em[i]]+=remote.enc[i];remote.enc[i]=0;}
    plat_unlock(f);
#endif
}
void sd_lights(const uint8_t *bright,const uint8_t *dim,const uint8_t *masks){
#ifndef PLAT_HOST
    for(uint32_t i=0;i<11u;i++)fm1_led[i]=bright[i];
    for(uint32_t i=0;i<33u;i++)sd_led_planes[i/11u][i%11u]=dim[i];
    for(uint32_t i=0;i<3u;i++)sd_led_masks[i]=masks[i];
#else
    (void)bright;(void)dim;(void)masks;
#endif
}
void sd_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t c){lcd_fill(x,y,w,h,c);}
void sd_blit(uint32_t x,uint32_t y,uint32_t w,uint32_t h,const uint16_t *p){lcd_blit(x,y,w,h,p);}
void sd_sync(void){lcd_sync();}
void sd_lcd_power(uint32_t s){
#ifndef PLAT_HOST
    lcd_sync();
    if(s==0){lcd_cmd(0x28);fm1_lcd_backlight(0);}
    else if(s==1)lcd_cmd(0x11);
    else{lcd_cmd(0x29);fm1_lcd_backlight(1);}
#else
    (void)s;
#endif
}
uint32_t sd_master(void){return plat_master();}
int sd_midi_in(uint32_t *p,int *port){
    if(!plat_midi_in(p,port))return 0;
    if(*port==PORT_BLE&&(*p&0x10u))*port=3; /* external device, separate from the phone */
    *p&=~0xF0u;return 1;
}
void sd_midi_out(uint32_t p){plat_midi_send(p,TO_USB|TO_BLE);}
int sd_sysex_send(const uint8_t *p,uint32_t n){
#ifdef PLAT_HOST
    plat_sysex_send(p,n);return 0;
#else
    return ota_wire_send(p,n);
#endif
}
#include "ble_devices.h"
static int sd_data_ok(uint32_t off,uint32_t n){return (off>=0x93000u&&off<=0xE0000u&&n<=0xE0000u-off)||(off>=0xE5000u&&off<=0xFF000u&&n<=0xFF000u-off);}
#ifdef PLAT_HOST
static uint8_t sd_nor[0x100000];
static uint8_t sd_nor_ready;
static void sd_nor_init(void){if(!sd_nor_ready){memset(sd_nor,255,sizeof sd_nor);sd_nor_ready=1;}}
#endif
int sd_flash_ok(void){
#ifdef PLAT_HOST
    sd_nor_init();return 1;
#else
    return flash_ok!=0;
#endif
}
const uint8_t *sd_flash(uint32_t off){
    if(!sd_data_ok(off,1u))return 0;
#ifdef PLAT_HOST
    sd_nor_init();return sd_nor+off;
#else
    return flash_ok?(const uint8_t *)FL_XIP(off):0;
#endif
}
int sd_read(uint32_t off,void *dst,uint32_t n){
    if(!sd_data_ok(off,n))return -1;
#ifdef PLAT_HOST
    sd_nor_init();memcpy(dst,sd_nor+off,n);return 0;
#else
    return flash_ok?st_read(off,dst,n):-1;
#endif
}
int sd_erase(uint32_t off){
    if((off&4095u)||!sd_data_ok(off,4096u))return -1;
#ifdef PLAT_HOST
    sd_nor_init();memset(sd_nor+off,255,4096);return 0;
#else
    uint32_t took;audio_silence();return flash_ok?fl_erase4k(off,&took):-1;
#endif
}
int sd_write(uint32_t off,const void *src,uint32_t n){
    if(!sd_data_ok(off,n))return -1;
#ifdef PLAT_HOST
    sd_nor_init();const uint8_t *p=src;for(uint32_t i=0;i<n;i++)sd_nor[off+i]&=p[i];return 0;
#else
    return flash_ok?fl_write(off,src,n):-1;
#endif
}
uint32_t sd_battery(void){
#ifdef PLAT_HOST
    return 600;
#else
    static uint32_t last,at;
    if((uint32_t)(fm1_ms-at)<1000u)return last;
    uint32_t f=plat_lock();
    if(!fm1_an.con&&!fm1_an.pend){int32_t v=fm1_adc_read(FM1_ADC_BATT);if(v>0)last=(uint32_t)v;at=fm1_ms;}
    plat_unlock(f);return last;
#endif
}

#ifndef PLAT_HOST
void app_cstart(void){sd_worker_stop();}
void app_fault(const void *crash){(void)crash;sd_worker_crash();}
int sd_worker_allowed(void){return fm1_vdc14()==7u&&fm1_sysvdd()==15u&&!bootguard.failed;}
#endif

void sd_panel_set(const uint8_t *buttons,const uint8_t *enc,const int8_t *direction){
#ifndef PLAT_HOST
    for(uint32_t i=0;i<14u;i++)panel.btn[i]=buttons[i];
    for(uint32_t i=0;i<7u;i++){panel.enc[i]=enc[i];panel.dir[i]=direction[i];}
#else
    (void)buttons;(void)enc;(void)direction;
#endif
}

void sd_usb_state(uint32_t *v){
#ifdef PLAT_HOST
    v[0]=1;v[1]=0;v[2]=1;v[3]=0;v[4]=0;
#else
    v[0]=usb.config;v[1]=usb.resets;v[2]=usb.up;v[3]=usb.suspended;v[4]=usb.rx_pkts;
#endif
}
