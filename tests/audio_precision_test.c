/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Exercise the shipping Q23 path, including signals below one PCM16 LSB.
 * Historical Q15 fixtures run separately as regression coverage. */
#define AUDIO_FRAC_BITS 8
#define MELODEE_USB_AUDIO 1
#define RESOURCE_HOST_WORDS 43008u
#define RESOURCE_HOST_LOW_WORDS 2048u
#define RESOURCE_HOST_DATA_WORDS 12000u
#define RESOURCE_HOST_RETAINED_WORDS 3800u
#include "synth_harness.c"
#include <assert.h>

static void transport(void)
{
    const int32_t edges[]={-8388608,-8388607,-65537,-257,-256,-255,-1,0,1,127,255,256,257,65537,8388606,8388607};
    uint8_t bytes[3];
    for(uint32_t i=0;i<NELEM(edges);i++){
        ua_encode(bytes,3,edges[i]);assert(ua_decode(bytes,3)==edges[i]);
        audio_history_t sample;audio_history_write(&sample,edges[i]);
        assert(audio_history_read(&sample)==edges[i]);
    }
    uint32_t random=0x13579bdf;
    for(uint32_t i=0;i<100000;i++){
        random=random*1664525u+1013904223u;
        int32_t x=(int32_t)(random&0xffffffu)-8388608;
        ua_encode(bytes,3,x);assert(ua_decode(bytes,3)==x);
    }
    /* Quantization must preserve a sub-LSB signal's mean when reduced. */
    int64_t sum=0;
    for(uint32_t i=0;i<200000;i++){
        ua_encode(bytes,2,32);sum+=(int16_t)(bytes[0]|(uint16_t)bytes[1]<<8);
    }
    assert(fabs((double)sum*256/200000-32)<2);
    ua_encode(bytes,2,0);assert(bytes[0]==0 && bytes[1]==0);
    ua_reset();ua.play_alt=ua.cap_alt=2;
    for(uint32_t i=0;i<UA_TARGET;i++){
        ua.play[2*i]=1;ua.play[2*i+1]=-1;
    }
    ua.pw=UA_TARGET;
    int32_t pcm[2*CTL];for(uint32_t i=0;i<CTL;i++){pcm[2*i]=17;pcm[2*i+1]=-19;}
    ua_audio(pcm,CTL,4096);
    assert(pcm[0]==18 && pcm[1]==-20 && ua.cap[0]==17 && ua.cap[1]==-19);
    puts("PCM24: signed edges/random values, all history bits, sub-LSB duplex and unbiased PCM16 reduction passed");
}

static void master_curve(void)
{
    double error=0;
    for(int32_t x=-3*8388608;x<=3*8388608;x+=127){
        double input=(double)x/8388608;
        if(input>2)input=2;if(input<-2)input=-2;
        double reference=tanh(input)*(32767.0*256);
        double e=fabs(softclip(x)-reference);if(e>error)error=e;
    }
    assert(error<3.0);
    printf("Q23 master curve: libm maximum error %.3f native LSBs\n",error);
}

static void engines(void)
{
    for(uint32_t engine=0;engine<NENG_SHOWN;engine++){
        power_on();host_preset(TSEL,ENGINE_ORDER[engine],0);
        TSEL->p[P_LEVEL]=64;TSEL->p[P_SPRD]=64;
        for(uint32_t k=0;k<6;k++)trk_note_on(TSEL,48+k*3,64+k*8);
        uint64_t energy=0;uint32_t fractional=0,peak=0;
        for(uint32_t block=0;block<1500;block++){
            if(block==500)for(uint32_t k=0;k<6;k++)trk_note_off(TSEL,48+k*3);
            int32_t pcm[2*CTL];mix_block(pcm,CTL);
            for(uint32_t i=0;i<2*CTL;i++){
                int32_t x=pcm[i];assert(x>=-8388608 && x<=8388607);
                energy+=abs(x);fractional+=(x&255)!=0;
                if((uint32_t)abs(x)>peak)peak=(uint32_t)abs(x);
            }
        }
        assert(energy && fractional>1000);
        printf("Q23 %-7s: polyphony/spread/release, %u fractional samples, peak %u\n",ENGINES[ENGINE_ORDER[engine]]->name,fractional,peak);
    }
    /* An amplitude below the old output LSB must survive float -> PCM. */
    float source[CTL];int32_t output[CTL];vmod_t modulation={.amp0=32768,.amp1=32768};
    for(uint32_t i=0;i<CTL;i++)source[i]=0.00001f;
    fm6_output(source,output,CTL,&modulation,4096,0);
    assert(output[0]>0 && output[0]<256);
    assert(softclip(32)>0 && softclip(-32)<0);
}

static void effects(void)
{
    for(uint32_t type=1;type<FX_COUNT;type++){
        if(fx_is_midi(type)||fx_retired(type))continue;
        power_on();TSEL->p[P_FXTYPE]=type;TSEL->p[P_FXMIX]=127;
        TSEL->p[P_FXRATE]=80;TSEL->p[P_FXCHAR]=96;
        uint64_t quiet=0;
        for(uint32_t block=0;block<1400;block++){
            int32_t pcm[CTL];for(uint32_t i=0;i<CTL;i++)pcm[i]=(int32_t)(192*sin((block*CTL+i)*0.04));
            assert(fx_process(TSEL,pcm,CTL));
            if(block>100)for(uint32_t i=0;i<CTL;i++)quiet+=abs(fx_left[i])+abs(fx_right[i]);
        }
        assert(quiet>1000);
        for(uint32_t block=0;block<300;block++){
            int32_t pcm[CTL];for(uint32_t i=0;i<CTL;i++)pcm[i]=(int32_t)(AUDIO_Q15(60000)*sin((block*CTL+i)*0.21));
            assert(fx_process(TSEL,pcm,CTL));
            for(uint32_t i=0;i<CTL;i++)assert(abs(fx_left[i])<=AUDIO_Q15(1000000) && abs(fx_right[i])<=AUDIO_Q15(1000000));
        }
        printf("Q23 FX %s: sub-PCM16 signal survives\n",N_FX[type]);
    }

}

/* The limiter's envelope must release on the nightly Q15 schedule: its
 * gain applies at native precision, its timing never changes. */
static void limiter_release(void)
{
    int32_t env15=36000;
    lim_env=AUDIO_Q15(env15);
    for(uint32_t i=0;i<20000u;i++){
        int32_t l=0,r=0;master_out(&l,&r);
        if(env15>18000)env15-=((env15-18000)>>12)+1;
        assert(lim_env==AUDIO_Q15(env15));
    }
    assert(lim_env==LIM_T);
    puts("Q23 limiter: release timing matches the Q15 envelope sample for sample");
}

static void insert_controls(void)
{
    power_on();frame();
    for(uint32_t b=0;b<NB;b++){                      /* no button has a layer: held, it never claims a key */
        fm1_in.buttons|=1u<<panel.btn[b];frames(700);
        key_down(0);frame();assert(gates());key_up(0);frame();assert(!gates());
        fm1_in.buttons&=~(1u<<panel.btn[b]);frame();
    }
    _Static_assert(FX_CHAIN_COUNT==1,"Only the synth insert chain exists");
    puts("FX controls: three inserts; no held button claims the musical keys");
}

static void memory(void)
{
    /* Match the minimum target main arena. Packed histories and split reverb
     * state must accommodate two Echoverb inserts and a chorus. */
    power_on();
    memset(resource,0,sizeof resource);memset(fx_state,0,sizeof fx_state);
    host_preset(TSEL,ENGI_FM6,0);trk_note_on(TSEL,60,100);
    TSEL->p[P_FXTYPE]=FX_REVERB;TSEL->p[P_FXMIX]=100;
    TSEL->p[P_FX2TYPE]=FX_REVERB;TSEL->p[P_FX2MIX]=80;
    TSEL->p[P_FX3TYPE]=FX_CHORUS;TSEL->p[P_FX3MIX]=80;
    for(uint32_t block=0;block<100;block++){int32_t pcm[2*CTL];mix_block(pcm,CTL);}
    for(uint32_t slot=0;slot<FX_SLOTS;slot++)assert(resource[RES_FX0+slot].size && !fx_state[slot].failed);
    assert(resource[RES_FX_HISTORY0].size==((FX_ECHOVERB_HISTORY_BYTES+3u)&~3u));
    TSEL->p[P_FXMIX]=0;
    for(uint32_t block=0;block<20;block++){int32_t pcm[2*CTL];mix_block(pcm,CTL);}
    assert(!resource[RES_FX0].size && !resource[RES_FX_HISTORY0].size);
    printf("Q23 arena: two full-length Echoverbs + chorus + FM6 coexist; %u bytes live\n",resource_used());
}

int main(void)
{
    _Static_assert(sizeof(audio_history_t)==3,"Packed native PCM24 history");
    transport();master_curve();limiter_release();engines();effects();insert_controls();memory();
    puts("NATIVE 24-BIT AUDIO TESTS PASSED");return 0;
}
