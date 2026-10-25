/* Own MIT synthetic memory fixture only. No process, engine or asset files. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../src/bodyassets184_probe.h"
#include "../src/bodyassets184_core.h"
static unsigned checks,failed;
static void check(int ok,const char *label){++checks;if(!ok){++failed;printf("FAIL %s\n",label);}}
typedef struct fixture {
    uint32_t count,addon;unsigned char ids[32],zones[2][0x4c];
    uint16_t buckets[0x9088];unsigned char entries[0x9088][16];
    unsigned char headers[2][8],names[2][128];
    uint32_t registered[513];unsigned calls,bytes,passes,fail_at,change,table_change,flag_change,largest;
} fixture;
static fixture f;
static unsigned hash(const char *n){uint32_t h=4;while(*n)h=h*31u+(unsigned char)*n++;return h%0x9088u;}
static void put32(void *p,uint32_t value){memcpy(p,&value,4);}
static void put16(void *p,uint16_t value){memcpy(p,&value,2);}
static int block(uintptr_t address,void *out,size_t n,uintptr_t base,const void *data,size_t bytes)
{if(address<base||address-base>=bytes||n>bytes-(address-base))return 0;memcpy(out,(const unsigned char *)data+(address-base),n);return 1;}
static int read_fake(void *context,uintptr_t address,void *out,size_t n)
{
    fixture *v=context;++v->calls;v->bytes+=(unsigned)n;if(n>v->largest)v->largest=(unsigned)n;
    if(v->calls==v->fail_at)return 0;
    if(address==0xa3ced4u&&++v->passes==2){
        if(v->change)v->zones[0][0]^=1;if(v->table_change)v->registered[511]^=1;
        if(v->flag_change)v->entries[1][9]^=1;}
    if(block(address,out,n,0xa3ced4u,&v->count,4)||block(address,out,n,0x1446900u,&v->addon,4)
       ||block(address,out,n,0xcfc894u,v->ids,sizeof(v->ids))
       ||block(address,out,n,0xa1c5b8u,v->buckets,sizeof(v->buckets))
       ||block(address,out,n,0xb27de0u,v->entries,sizeof(v->entries))
       ||block(address,out,n,0x10c6510u,v->registered,sizeof(v->registered)))return 1;
    for(unsigned i=0;i<2;++i)if(block(address,out,n,0xc9c288u+(i+1)*0x9cu,v->zones[i],sizeof(v->zones[i]))
        ||block(address,out,n,0x10000000u+i*0x100u,v->headers[i],8)
        ||block(address,out,n,0x10001000u+i*0x100u,v->names[i],128))return 1;
    return 0;
}
static void reset(void)
{
    memset(&f,0,sizeof(f));f.count=2;f.ids[0]=1;f.ids[1]=2;
    strcpy((char *)f.zones[0]+4,"so_killspree_trainer");strcpy((char *)f.zones[1]+4,"trainer");
    put32(f.zones[0]+0x44,4);put32(f.zones[1]+0x44,4);
    const char *names[]={"coop_body_us_army","coop_head_us_army"};
    for(unsigned i=0;i<2;++i){strcpy((char *)f.names[i],names[i]);put32(f.headers[i],0x10001000u+i*0x100u);
        f.headers[i][4]=i?24:90;put32(f.entries[i+1],4);put32(f.entries[i+1]+4,0x10000000u+i*0x100u);
        f.entries[i+1][8]=1;f.entries[i+1][9]=1;f.buckets[hash(names[i])]=(uint16_t)(i+1);
        f.registered[i+1]=0x10000000u+i*0x100u;}
}
static void refuse(const char *label){check(ba184_probe(read_fake,&f,BA184_RESIDENT)==0,label);}
static void refuse_registered(const char *label){check(ba184_probe(read_fake,&f,BA184_REGISTERED)==0,label);}
int main(void)
{
    reset();uint32_t expected=BA184_ALL_PROOF&~(BA184_CAMPAIGN_TRAINER|BA184_PRE_SCRIPT);
    check(ba184_probe(read_fake,&f,BA184_RESIDENT)==expected,"exact resident typed body90/head24 sourceSO4 trainer4 addon0");
    check(f.passes==2&&f.calls<4096&&f.bytes<65536&&f.largest<=4096,"two bounded synthetic snapshots");
    check(!ba184_probe(NULL,&f,BA184_RESIDENT),"missing read capability refuses");
    reset();check(!ba184_probe(read_fake,&f,0)&&!f.calls,"unknown zero phase refuses before reads");
    reset();check(!ba184_probe(read_fake,&f,3)&&!f.calls,"unknown phase refuses before reads");
    reset();f.entries[1][9]=f.entries[2][9]=0;
    check(ba184_probe(read_fake,&f,BA184_RESIDENT)==expected,"unmarked resident exact headers are valid pre-registration candidates");
    reset();check(ba184_probe(read_fake,&f,BA184_REGISTERED)==expected,"exact mark1 and both nonzero-index model registrations");
    reset();f.count=33;refuse("overlarge zone count");reset();f.count=0;refuse("empty zone set");
    reset();f.addon=1;refuse("active addon must be cleared");
    reset();f.ids[0]=0;refuse("zone zero is not resident source");reset();f.ids[0]=33;refuse("zone ID outside pool");
    reset();f.ids[1]=1;refuse("duplicate live zone ID");
    reset();strcpy((char *)f.zones[1]+4,"airport");refuse("trainer source absent");
    reset();strcpy((char *)f.zones[0]+4,"trainer");refuse("resource source absent");
    reset();put32(f.zones[0]+0x44,2);refuse("SO must have stock alloc4 category");
    reset();put32(f.zones[1]+0x44,8);refuse("trainer must have stock alloc4 category");
    reset();memset(f.zones[0]+4,'x',64);refuse("unterminated zone name");
    reset();f.zones[0][4]=0x80;refuse("nonordinary zone name");
    reset();put32(f.entries[1],3);refuse("asset must be exact XModel type");
    reset();f.entries[1][8]=0;refuse("default body forbidden");
    reset();f.entries[2][8]=0;refuse("default head forbidden");
    reset();f.entries[1][8]=2;refuse("body from trainer cannot substitute SO ownership");
    reset();f.entries[2][8]=2;refuse("head must share SO source");
    reset();f.entries[1][9]=0;refuse_registered("resident body without native mark cannot authorize registered phase");
    reset();f.entries[2][9]=0;refuse_registered("resident head without native mark cannot authorize registered phase");
    reset();f.headers[0][4]=84;refuse("NPC body skeleton refused");
    reset();f.headers[1][4]=30;refuse("NPC head skeleton refused");
    reset();f.names[0][0]='C';refuse("name must match case exactly");
    reset();memset(f.names[1],'x',128);refuse("unterminated asset name bounded");
    reset();put32(f.entries[1]+4,0xffffu);refuse("low invalid model pointer");
    reset();put32(f.entries[1]+4,0x7ffeffffu);refuse("model read cannot cross upper address bound");
    reset();f.buckets[hash("coop_body_us_army")]=0x9088;refuse("hash entry index outside pool");
    reset();put32(f.entries[1],3);put16(f.entries[1]+12,1);refuse("cyclic hash chain bounded");
    reset();for(unsigned i=1;i<=65;++i){put32(f.entries[i],3);put16(f.entries[i]+12,(uint16_t)(i+1));}
    refuse("overlong hash chain bounded");
    reset();f.fail_at=1;refuse("failed first read");
    reset();f.fail_at=25;refuse("failed interior read");
    reset();f.change=1;refuse("otherwise-valid metadata changed between full snapshots");
    reset();check(ba184_probe(read_fake,&f,BA184_RESIDENT)==expected,"failure has no persistent poisoned or ready cache");
    const unsigned forbidden[]={2,3,4,5,6,7,0x80,0xff};
    for(unsigned i=0;i<sizeof(forbidden)/sizeof(forbidden[0]);++i){
        reset();f.entries[1][9]=(unsigned char)forbidden[i];refuse("resident phase rejects GC/temp/unknown mask");
        reset();f.entries[2][9]=(unsigned char)forbidden[i];refuse_registered("registered phase rejects GC/temp/unknown mask");}
    reset();memset(f.registered,0,sizeof(f.registered));refuse_registered("empty registration table");
    reset();f.registered[1]=0;refuse_registered("body registration missing");
    reset();f.registered[2]=0;refuse_registered("head registration missing");
    reset();f.registered[0]=f.registered[1];f.registered[1]=0;refuse_registered("reserved slot0 cannot stand in for body registration");
    reset();f.registered[511]=f.registered[1];f.registered[1]=0;
    check(ba184_probe(read_fake,&f,BA184_REGISTERED)==expected,"last native model slot511 is in range");
    reset();f.registered[512]=f.registered[1];f.registered[1]=0;refuse_registered("slot512 is outside native registration bound");
    reset();f.registered[1]^=4;refuse_registered("registration requires exact body header identity");
    reset();f.registered[2]=f.registered[1];refuse_registered("one body pointer cannot stand in for distinct head header");
    reset();f.table_change=1;refuse_registered("full registration table drift between snapshots refused");
    reset();f.flag_change=1;refuse("resident0/1 drift remains a changed snapshot");
    reset();memset(f.registered,0,sizeof(f.registered));
    check(ba184_probe(read_fake,&f,BA184_RESIDENT)==expected,"resident stage does not infer or require later registration table");
    ba184_probe_diagnostic d;reset();check(!ba184_probe_diagnose(read_fake,&f,0,&d)&&d.status==BA184_PROBE_ARGUMENT&&!f.calls,"diagnostic invalid phase is read-free");
    reset();f.fail_at=1;check(!ba184_probe_diagnose(read_fake,&f,1,&d)&&d.status==BA184_PROBE_READ&&d.step==BA184_STEP_COUNT&&d.pass==1,"diagnostic exact failed-read predicate");
    reset();f.count=33;check(!ba184_probe_diagnose(read_fake,&f,1,&d)&&d.status==BA184_PROBE_PARSE&&d.step==BA184_STEP_COUNT,"diagnostic invalid zone count distinct from read failure");
    reset();f.entries[1][9]=4;check(!ba184_probe_diagnose(read_fake,&f,1,&d)&&d.status==BA184_PROBE_PARSE&&d.step==BA184_STEP_FLAGS&&d.asset==1,"diagnostic exact body mark predicate");
    reset();put32(f.entries[1]+4,0xffffu);check(!ba184_probe_diagnose(read_fake,&f,1,&d)&&d.status==BA184_PROBE_RANGE&&d.step==BA184_STEP_HEADER,"diagnostic rejected header bounds");
    reset();f.table_change=1;check(!ba184_probe_diagnose(read_fake,&f,2,&d)&&d.status==BA184_PROBE_DIGEST&&d.pass==2,"diagnostic two valid snapshots changed");
    reset();f.registered[1]=0;check(!ba184_probe_diagnose(read_fake,&f,2,&d)&&d.status==BA184_PROBE_PARSE&&d.step==BA184_STEP_BODY_REGISTERED,"diagnostic exact missing registration predicate");
    reset();check(ba184_probe(read_fake,&f,2)==expected,"legacy API unchanged success");unsigned legacy_calls=f.calls,legacy_bytes=f.bytes;
    reset();check(ba184_probe_diagnose(read_fake,&f,2,&d)==expected&&d.status==BA184_PROBE_OK&&d.pass==2&&f.calls==legacy_calls&&f.bytes==legacy_bytes
        &&d.calls==f.calls&&d.bytes==f.bytes,"diagnostic API adds no reads or bytes to existing successful registered proof");
    printf("bodyassets184 synthetic DB: %u checks, %u failures; no game/native engine\n",checks,failed);
    return failed?1:0;
}
