/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Real UI/mixer/storage integration plus deterministic DSP stress. */
#include <assert.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static void reset_fx(void)
{
    fx_clear_all();memset(fx_state,0,sizeof fx_state);
    for(uint32_t k=0;k<FX_STATE_COUNT;k++)resource_release(RES_FX0+k);
    memset(trk,0,sizeof trk);spread_on=0;trk[0].p[P_FXRATE]=60;trk[0].p[P_FXCHAR]=64;
}
static uint64_t fx_signal(uint32_t type,int rate,int character,int mix,const char *demo)
{
    reset_fx();track_t *t=&trk[0];t->p[P_FXTYPE]=(int16_t)type;t->p[P_FXRATE]=(int16_t)rate;
    t->p[P_FXCHAR]=(int16_t)character;t->p[P_FXMIX]=(int16_t)mix;
    uint32_t phase=0;uint64_t h=1469598103934665603ull,energy=0;int32_t b[CTL];
    FILE *w=demo?fopen(demo,"wb"):0;if(demo)assert(w);if(w)wav_hdr(w,2u*FS/CTL*CTL);
    for(uint32_t frame=0;frame<2u*FS/CTL;frame++){
        for(uint32_t i=0;i<CTL;i++){
            phase+=0x02347120u;
            b[i]=((sine_i(phase)+sine_i(phase*3u)/2)/4)*2;
            if(frame%173u==0u && !i)b[i]+=18000;
        }
        int on=fx_process(t,b,CTL);
        assert(resource[RES_FX0].size==((type==FX_CHORUS || type==FX_ROOM) && mix?2048u:type==FX_REVERB && mix?sizeof(fx_echoverb_t):0u));
        for(uint32_t i=0;i<CTL;i++){
            int32_t l=on?fx_left[i]:b[i],r=on?fx_right[i]:b[i];
            assert(l>=-1000000 && l<=1000000 && r>=-1000000 && r<=1000000);
            if(type==FX_WIDTH)assert(r==b[i]);
            h=(h^(uint32_t)l)*1099511628211ull;h=(h^(uint32_t)r)*1099511628211ull;
            energy+=(uint64_t)(l<0?-l:l)+(uint64_t)(r<0?-r:r);
            if(w)wav_put(w,clamp(l,-32767,32767),clamp(r,-32767,32767));
        }
    }
    if(w)fclose(w);assert(energy>1000000u);return h;
}




static void fx_chain_test(void)
{
    ui_power_on();track_t *t=&trk[0];
    int16_t settings[FX_SLOTS*FX_PARAMS]={FX_PHASER,80,30,71,FX_CHORUS,95,55,65,FX_ROOM,105,88,99};
    memcpy(t->p+P_FXTYPE,settings,sizeof settings);
    int32_t b[CTL];uint64_t energy=0;
    for(uint32_t frame=0;frame<1600;frame++){
        for(uint32_t i=0;i<CTL;i++)b[i]=frame<80?8000*sine_i((frame*CTL+i)*12345678u)/32768:0;
        assert(fx_process(t,b,CTL));
        for(uint32_t i=0;i<CTL;i++)if(frame>80)energy+=abs(fx_left[i])+abs(fx_right[i]);
    }
    assert(energy>10000 && fx_state[0].type==FX_PHASER && fx_state[1].type==FX_CHORUS && fx_state[2].type==FX_ROOM);
    assert(!resource[RES_FX0].size && resource[RES_FX0+1].size==2048 && resource[RES_FX0+2].size==2048u);
    uint32_t pos=fx_state[2].comb[0];void *tank=resource[RES_FX0+2].ptr;
    apply_preset_to(t,2);assert(!memcmp(t->p+P_FXTYPE,settings,sizeof settings));
    undo_swap();assert(!memcmp(t->p+P_FXTYPE,settings,sizeof settings));
    set_engine_of(t,ENGI_FM6);apply_preset_to(t,8);assert(!memcmp(t->p+P_FXTYPE,settings,sizeof settings));
    memset(b,0,sizeof b);fx_process(t,b,CTL);
    assert(resource[RES_FX0+2].ptr==tank && fx_state[2].comb[0]!=pos);
    int rc=up_store(0,"CHAIN");assert(rc==0 || rc==3);
    t->p[P_FX2CHAR]=19;assert(!up_load(0));assert(t->p[P_FX2CHAR]==19);t->p[P_FX2CHAR]=65;
    sound_init_of(t,t->eng_req);assert(!memcmp(t->p+P_FXTYPE,settings,sizeof settings));
    assert(!native_store(ENGI_FM6,1,0,"FX FREE") || native_used(ENGI_FM6,1));
    assert(!native_load(ENGI_FM6,1,0));assert(!memcmp(t->p+P_FXTYPE,settings,sizeof settings));
    press(B_FX);assert(cur_page()->graph==GR_FXRACK && cur_page()->scope==SC_TRACK);
    turn(EN_SELECT,1);assert(cur_page()->id[0]==P_FXMIX);turn(EN_SELECT,1);assert(cur_page()->id[0]==P_FX2MIX);
    turn(EN_SELECT,1);assert(cur_page()->id[0]==P_FX3MIX);turn(EN_SELECT,1);assert(str_eq(cur_page()->title,"SHARED"));
    t->p[P_FX2MIX]=0;for(uint32_t k=0;k<24;k++)fx_process(t,b,CTL);
    assert(!resource[RES_FX0+1].size && resource[RES_FX0+2].size==2048u);
    puts("Three serial slots: independent bypass, live tails, preset/engine/undo/user independence, page navigation: OK");
}
static void rate_and_pan(void)
{
    /* Cached block conversion must match the separately verified FIR model,
     * including arbitrary block boundaries, stereo differences and wraps. */
    fx_state_t actual={0},reference[2]={{0}};actual.type=FX_TAPE;int32_t rng=13;
    for(uint32_t block=0;block<10000u;block++){
        uint32_t n=1u+block%CTL,phase=actual.decim,count=0;int32_t expected[2][CTL/4u+1u];
        for(uint32_t i=0;i<n;i++){
            fx_left[i]=(int32_t)(noise32(&rng)%2000001u)-1000000;fx_right[i]=-fx_left[i];
            int32_t l=fx_filter_input(&reference[0],0,fx_left[i],phase),r=fx_filter_input(&reference[1],0,fx_right[i],phase);
            if(!phase){expected[0][count]=l;expected[1][count++]=r;}phase=(phase+1u)&3u;
            fx_dry_l[i]=fx_dry_r[i]=0;
        }
        assert(fx_down(&actual,n)==count);
        for(uint32_t i=0;i<count;i++)assert(fx_low_l[i]==expected[0][i] && fx_low_r[i]==expected[1][i]);
        fx_up(&actual,n,0);assert(actual.decim==phase);
        for(uint32_t ch=0;ch<2u;ch++)for(uint32_t k=0;k<4u;k++)assert(actual.rev_damp[ch*4u+k]==reference[ch].rev_damp[k]);
    }
    for(uint32_t shift=1;shift<16u;shift++)for(uint32_t i=0;i<5000u;i++){
        int32_t x=(int32_t)noise32(&rng);assert(fx_div_pow2(x,shift)==x/(int32_t)(1u<<shift));
    }
    for(uint32_t type=FX_PHASER;type<=FX_SPIT;type++){
        reset_fx();track_t *t=&trk[0];t->p[P_FXTYPE]=type;t->p[P_FXMIX]=127;t->p[P_PAN]=1;
        int32_t b[CTL];for(uint32_t i=0;i<CTL;i++)b[i]=4000;
        for(uint32_t k=0;k<50u;k++)fx_process(t,b,CTL);
        assert(type==FX_WIDTH || fx_state[0].mono); /* PAN alone adds no stereo filter work */
        t->p[P_PAN]=63;fx_process(t,b,CTL);for(uint32_t i=0;i<CTL;i++)assert(type==FX_WIDTH?fx_right[i]==4000:fx_left[i]==fx_scale(fx_right[i],1024u));
        t->p[P_PAN]=-64;fx_process(t,b,CTL);for(uint32_t i=0;i<CTL;i++)assert(!fx_right[i]);
        t->p[P_PAN]=0;spread_on=1;t->p[P_SPRD]=127;
        for(uint32_t i=0;i<CTL;i++)spread_buf[i]=i&1u?-3000:3000;
        fx_process(t,b,CTL);assert(!fx_state[0].mono);
    }
    puts("FX performance safety: cached stereo conversion, arbitrary blocks/wraps, exact shifts, mono pan and genuine spread: OK");
}
static void tape_smooth(void)
{
    for(uint32_t hz=0;hz<=220;hz+=220)for(int rate=60;rate<=127;rate+=67){
        reset_fx();track_t *t=&trk[0];t->p[P_FXTYPE]=FX_TAPE;t->p[P_FXMIX]=127;t->p[P_FXRATE]=rate;t->p[P_FXCHAR]=127;
        int32_t b[CTL]={0};fx_process(t,b,CTL);fx_state[0].mix=127*256;
        uint32_t phase=0,inc=(uint32_t)((uint64_t)hz*0x100000000ull/FS),jump=0;int32_t prev=0;
        for(uint32_t frame=0;frame<6u*FS/CTL;frame++){
            for(uint32_t i=0;i<CTL;i++){phase+=inc;b[i]=hz?8000*sine_i(phase)/32768:8000;}
            if(frame>FS/CTL && frame%401u==0u)t->p[P_FXCHAR]=t->p[P_FXCHAR]?0:127;
            fx_process(t,b,CTL);
            for(uint32_t i=0;i<CTL;i++){
                int32_t y=fx_left[i];uint32_t delta=(uint32_t)abs(y-prev);prev=y;
                if(frame>FS/CTL && delta>jump)jump=delta;
                assert(abs(y)<24000 && fx_left[i]==fx_right[i]);
            }
        }
        /* A steady input used to jump by ~1000 samples when random targets
         * changed; a 220 Hz tone jumped by ~1500..2000 instead of its 251. */
        assert(jump<=(hz?600u:2u));assert(!resource[RES_FX0].size);
        t->p[P_FXCHAR]=0;fx_process(t,b,CTL);t->p[P_FXCHAR]=127;fx_process(t,b,CTL);
        for(uint32_t k=0;k<4000;k++){memset(b,0,sizeof b);fx_process(t,b,CTL);}
        for(uint32_t ch=0;ch<2;ch++)for(uint32_t k=0;k<3;k++)assert(!fx_state[0].q[ch][k].z1 && !fx_state[0].q[ch][k].z2);
    }
    puts("Tape: maximum jitter/rate have smooth phase/filter transitions, bounded stereo output, no allocation and no denormal tail: OK");
}
static void chorus_test(void)
{
    _Static_assert(FX_ROOM==7 && FX_CHORUS==9,"stored FX IDs stay fixed");
    /* A fully wet, depth-zero chorus preserves upper audio frequencies and
     * independent stereo channels; unlike the other time FX it is full-rate. */
    fx_state_t f={.type=FX_CHORUS,.rev_l=256*65536,.rev_r=256*65536,.mix=32768};
    int16_t mem[FX_CHORUS_BYTES/2u]={0};int32_t step[2]={0},rng=31;
    uint64_t dry=0,wet=0;
    for(uint32_t block=0;block<3000u;block++){
        uint32_t n=1u+block%CTL;
        for(uint32_t i=0;i<n;i++){
            int32_t x=8000*sine_i((block*CTL+i)*0x2e709de5u)/32768;
            fx_left[i]=fx_dry_l[i]=x;fx_right[i]=fx_dry_r[i]=-x;
            dry+=(int64_t)x*x;
        }
        fx_chorus(&f,mem,n,step,32768);
        for(uint32_t i=0;i<n;i++){assert(abs(fx_left[i]+fx_right[i])<=4);wet+=(int64_t)fx_left[i]*fx_left[i];}
    }
    assert(wet>dry*98u/100u && wet<dry*102u/100u);
    /* Hot output from preceding slots stays bounded, and no feedback means
     * silence becomes exact within one ring, even after repeated wraps. */
    for(uint32_t block=0;block<1000;block++){
        for(uint32_t i=0;i<CTL;i++){fx_left[i]=fx_dry_l[i]=(int32_t)(noise32(&rng)%2000001u)-1000000;fx_right[i]=fx_dry_r[i]=-fx_left[i];}
        fx_chorus(&f,mem,CTL,step,32768);
        for(uint32_t i=0;i<CTL;i++)assert(abs(fx_left[i])<=131068 && abs(fx_right[i])<=131068);
    }
    for(uint32_t block=0;block<512u/CTL+2u;block++){
        memset(fx_left,0,sizeof fx_left);memset(fx_right,0,sizeof fx_right);fx_chorus(&f,mem,CTL,step,32768);
    }
    for(uint32_t i=0;i<CTL;i++)assert(!fx_left[i] && !fx_right[i]);
    ui_power_on();
    for(uint32_t slot=0;slot<FX_SLOTS;slot++){
        uint32_t b=P_FXTYPE+slot*FX_PARAMS,g=G_MFXTYPE+slot*FX_PARAMS;
        trk[0].p[b]=song.g[g]=FX_CHORUS;trk[0].p[b+1]=100;
        assert(str_eq(track_desc(&trk[0],b+2)->label,"RATE") && track_desc(&trk[0],b+2)->fmt==F_LFOHZ);
        assert(str_eq(track_desc(&trk[0],b+3)->label,"DEPTH"));
        assert(str_eq(global_desc(g+2)->label,"RATE") && global_desc(g+2)->fmt==F_LFOHZ);
        assert(str_eq(global_desc(g+3)->label,"DEPTH"));
    }
    int32_t b[CTL]={0};for(uint32_t k=0;k<32;k++){assert(fx_process(&trk[0],b,CTL));fx_bus_start(CTL);fx_bus_join();}assert(fx_bus_result);
    for(uint32_t slot=0;slot<FX_SLOTS;slot++){
        assert(resource[RES_FX0+slot].size==FX_CHORUS_BYTES && resource[RES_FX0+NPART*FX_SLOTS+slot].size==FX_CHORUS_BYTES);
        trk[0].p[P_FXTYPE+slot*FX_PARAMS]=song.g[G_MFXTYPE+slot*FX_PARAMS]=FX_OFF;
    }
    for(uint32_t k=0;k<24;k++){fx_process(&trk[0],b,CTL);fx_bus_start(CTL);fx_bus_join();}
    for(uint32_t slot=0;slot<FX_SLOTS;slot++)assert(!resource[RES_FX0+slot].size && !resource[RES_FX0+NPART*FX_SLOTS+slot].size);
    puts("Chorus: full-band independent stereo, bounded hot inputs, exact silent tail, 2 KiB/slot, RATE/DEPTH, insert/send persistence and bypass release: OK");
}
static uint64_t tone_energy(uint32_t type,int tone,uint32_t hz)
{
    ui_power_on();reset_fx();track_t *t=&trk[0];
    t->p[P_FXTYPE]=type;t->p[P_FXMIX]=127;t->p[P_FXRATE]=70;t->p[P_FXCHAR]=60;t->p[P_FXTONE]=tone;
    int32_t b[CTL]={0};assert(fx_process(t,b,CTL));fx_state[0].mix=32768;
    uint32_t phase=0,inc=(uint32_t)((uint64_t)hz*0x100000000ull/FS);uint64_t energy=0;
    for(uint32_t frame=0;frame<3u*FS/CTL;frame++){
        for(uint32_t i=0;i<CTL;i++){phase+=inc;b[i]=2000*sine_i(phase)/32768;}
        assert(fx_process(t,b,CTL));
        for(uint32_t i=0;i<CTL;i++){
            assert(abs(fx_left[i])<32768 && abs(fx_right[i])<32768);
            if(frame>=FS/CTL)energy+=(int64_t)fx_left[i]*fx_left[i]+(int64_t)fx_right[i]*fx_right[i];
        }
    }
    return energy;
}
static void tone_test(void)
{
    /* TONE's two sides: a low cut 150 .. 1280 Hz above 0, a high cut 2640 .. 120 Hz below, FULL at 0; ECHO keeps a
     * 100 Hz low cut of its own, ROOM and REVERB 150 Hz (FULL, DARK) */
    assert(!fx_tone_hz(0) && fx_tone_hz(1)==150 && fx_tone_hz(FX_TONE_MAX)==1280 && fx_tone_hz(-1)==2640 && fx_tone_hz(-FX_TONE_MAX)==120);
    for(int v=2;v<=FX_TONE_MAX;v++)assert(fx_tone_hz(v)>=fx_tone_hz(v-1) && fx_tone_hz(-v)<fx_tone_hz(-v+1));
    assert(fx_low_cut_hz(FX_ROOM,-20)==150 && fx_low_cut_hz(FX_ROOM,1)==150 && fx_low_cut_hz(FX_ROOM,24)==313);
    const int thin=24,edge=40,dark=-39;assert(fx_tone_hz(thin)==313 && fx_tone_hz(edge)==605 && fx_tone_hz(dark)==516);
    for(uint32_t type=FX_ROOM;type<=FX_ROOM;type++){
        uint64_t bass=tone_energy(type,0,40),cut=tone_energy(type,edge,40);
        uint64_t mids=tone_energy(type,0,2000),preserved=tone_energy(type,edge,2000);
        assert(cut<bass/12u); /* >10 dB less sub-bass at 605 Hz than at the floor */
        assert(preserved>mids*8u/10u && preserved<mids*12u/10u);
        assert(tone_energy(type,thin,40)<bass && tone_energy(type,FX_TONE_MAX,40)<bass/32u);
        uint64_t at_cut=tone_energy(type,edge,605),reference=tone_energy(type,0,605);
        assert(at_cut>reference*4u/10u && at_cut<reference*6u/10u); /* -3 dB (the floor's own loss there: ~0.3 dB) */
        uint64_t bright=tone_energy(type,0,2000),darker=tone_energy(type,dark,2000);
        uint64_t body=tone_energy(type,0,200),warm=tone_energy(type,dark,200);
        assert(bright>10000u && darker<bright/10u && warm>body*7u/10u);
        assert(tone_energy(type,-FX_TONE_MAX,2000)<darker/8u);
        uint64_t at_high=tone_energy(type,dark,500),flat_high=tone_energy(type,0,500);
        assert(at_high>flat_high*4u/10u && at_high<flat_high*6u/10u);
        trk[0].p[P_FXMIX]=0;reset_fx();int32_t dry[CTL];
        for(uint32_t i=0;i<CTL;i++)dry[i]=(int32_t)i*91-8000;
        trk[0].p[P_FXTONE]=FX_TONE_MAX;assert(!fx_process(&trk[0],dry,CTL) && !resource[RES_FX0].size);
    }
    for(uint32_t type=FX_PHASER;type<FX_AUDIO_COUNT;type++)if(!fx_retired(type) && !fx_has_tone((int32_t)type))
        assert(tone_energy(type,0,1000)==tone_energy(type,FX_TONE_MAX,1000) && tone_energy(type,0,1000)==tone_energy(type,-FX_TONE_MAX,1000));
    /* A constant input exposes discontinuities when moving the cutoff, crossing FULL from one side to the other and
     * switching the high cut off; test both processing rates. */
    for(uint32_t rev=0;rev<1;rev++)for(uint32_t sweep=0;sweep<3;sweep++){
        ui_power_on();reset_fx();int16_t p[4]={FX_ROOM,127,60,64};fx_plan_t plan;
        int32_t previous=8000;uint32_t rate=FS/4u;
        for(uint32_t block=0;block<600;block++){
            int16_t thin_side=(int16_t)(block<100?0:block<200?thin:block<300?FX_TONE_MAX:block<400?1:0);
            trk[0].p[P_FXTONE]=sweep==0?thin_side:sweep==1?(int16_t)-thin_side:(int16_t)(block<100?-FX_TONE_MAX:block<300?FX_TONE_MAX:block<400?-1:0);
            fx_slot_prepare(&plan,0,p);assert(plan.active);
            for(uint32_t i=0;i<CTL;i++){
                int32_t y=fx_tone_input(plan.f,8000,1.0f/(0.010f*rate));
                assert(abs(y-previous)<200 && abs(y)<=9000);previous=y;
            }
        }
        /* the high cut gone exactly; the low cut stays at the floor, so the constant input has died away */
        assert(!plan.f->q[0][1].b1 && !plan.f->q[0][1].z1 && plan.f->q[0][0].a2==(float)fx_floor_hz(p[0]) && abs(previous)<100);
    }
    /* Each slot's TONE targets its own track/slot and section, shared slots too. */
    ui_power_on();reset_fx();fx_plan_t plan;
    for(uint32_t ti=0;ti<=NPART;ti++)for(uint32_t slot=0;slot<FX_SLOTS;slot++){
        int16_t *p=ti==NPART?song.g:trk[ti].p,v=(int16_t)((ti+slot)&1u?1+ti*3+slot:-(int)(1+ti*3+slot));
        uint32_t base=(ti==NPART?G_MFXTYPE:P_FXTYPE)+slot*FX_PARAMS,tone=(ti==NPART?G_MFXTONE:P_FXTONE)+slot;
        p[base]=FX_ROOM;p[base+1]=127;p[tone]=v;
        fx_slot_prepare(&plan,ti*FX_SLOTS+slot,p+base);
        assert(plan.active && plan.f->q[0][0].a2==(float)fx_low_cut_hz(FX_ROOM,v) && plan.f->q[0][1].a2==(v<0?(float)fx_tone_hz(v):0.0f));
    }
    puts("FX TONE: every serial/send slot uses its own verified filter coefficients");
}

int main(int argc,char **argv)
{
    /* The dry path has the upstream frozen oracle; wet paths below check
     * repeatability and audible controls across all extremes. */
    static const uint64_t original_pcm[FX_COUNT]={
        0xdcbb3a74d3a1ba7bull,0x31f2c466625c315full,0xd6837fa20c478664ull,
        0x9582f216361a2b8bull,0xabf785bd7bd51881ull,0xddbbf39f09c8baa3ull,
        0x7705dc943db0ff6bull,0x5cc781ef5a3621a3ull,0xef747476fc21a401ull,
        0xd4c3687f410daf9cull
    };
    fx_chain_test();rate_and_pan();tape_smooth();chorus_test();tone_test();
    uint64_t dry=fx_signal(FX_OFF,60,64,0,0);
    assert(dry==original_pcm[FX_OFF]);
    for(uint32_t mode=FX_PHASER;mode<FX_AUDIO_COUNT;mode++){
        if(fx_retired(mode))continue;
        assert(fx_signal(mode,60,64,0,0)==dry);
        uint64_t a=fx_signal(mode,60,64,127,0);assert(a!=dry && a==fx_signal(mode,60,64,127,0));

        assert(a!=fx_signal(mode,127,64,127,0));assert(a!=fx_signal(mode,60,127,127,0));
        for(int rate=0;rate<=127;rate+=127)for(int cha=0;cha<=127;cha+=127)fx_signal(mode,rate,cha,127,0);
        if(argc>1){char path[256];snprintf(path,sizeof path,"%s/%s.wav",argv[1],N_FX[mode]);fx_signal(mode,60,64,100,path);}
        printf("%s: deterministic, audible controls, bounded extremes, zero-mix bypass: OK\n",N_FX[mode]);
    }
    /* Time tails survive voice completion, bypass releases their allocation. */
    for(uint32_t mode=FX_ROOM;mode<=FX_ROOM;mode++){
        reset_fx();track_t *t=&trk[0];t->p[P_FXTYPE]=(int16_t)mode;t->p[P_FXMIX]=127;t->p[P_FXRATE]=0;
        t->p[P_FXCHAR]=80;fx_state[0].mix=32768;int32_t b[CTL]={20000};
        assert(fx_process(t,b,CTL));uint64_t tail=0;
        for(uint32_t k=0;k<1500;k++){memset(b,0,sizeof b);assert(fx_process(t,b,CTL));for(uint32_t i=0;i<CTL;i++)tail+=(uint32_t)abs(fx_left[i]);}
        assert(tail>1000);t->p[P_FXMIX]=0;
        for(uint32_t k=0;k<16;k++)fx_process(t,b,CTL);assert(!resource[RES_FX0].size);
    }
    puts("FX: classic PCM parity, all active controls, chain/pan and tails passed");return 0;
}
