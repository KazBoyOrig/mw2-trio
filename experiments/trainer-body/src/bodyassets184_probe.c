#include "bodyassets184_probe.h"
#include "bodyassets184_core.h"
#include "pure_sha256.h"
#include <string.h>
typedef struct reader {ba184_read_fn read;void *context;unsigned calls,bytes;pure_sha256_context digest;ba184_probe_diagnostic *d;} reader;
static void step(reader *r,unsigned value){r->d->step=value;}
static int bad(reader *r,unsigned status){r->d->status=status;return 0;}
static int take(reader *r,uintptr_t a,void *out,size_t n)
{
    if(!n||n>4096||a<0x10000u||a>0x7ffeffffu||n-1>0x7ffeffffu-a)return bad(r,BA184_PROBE_RANGE);
    if(++r->calls>2048||n>32768-r->bytes)return bad(r,BA184_PROBE_BUDGET);
    r->bytes+=(unsigned)n;if(!r->read(r->context,a,out,n))return bad(r,BA184_PROBE_READ);
    uint32_t address=(uint32_t)a,bytes=(uint32_t)n;pure_sha256_update(&r->digest,&address,4);
    pure_sha256_update(&r->digest,&bytes,4);pure_sha256_update(&r->digest,out,n);return 1;
}
static uint32_t u32(const unsigned char *p){uint32_t v;memcpy(&v,p,4);return v;}
static uint16_t u16(const unsigned char *p){uint16_t v;memcpy(&v,p,2);return v;}
static int text(reader *r,uintptr_t p,char out[128])
{
    for(unsigned i=0;i<128;++i){unsigned char c;if(!take(r,p+i,&c,1))return 0;out[i]=(char)c;
        if(!c)return 1;if(c<32||c>126)return bad(r,BA184_PROBE_PARSE);}return bad(r,BA184_PROBE_PARSE);
}
static unsigned hash(const char *name){uint32_t h=4;for(;*name;++name)h=h*31u+(unsigned char)*name;return h%0x9088u;}
static int model(reader *r,const char *name,unsigned bones,unsigned source,unsigned stage,uint32_t *pointer)
{
    step(r,BA184_STEP_HASH);uint16_t index;if(!take(r,0xa1c5b8u+2*hash(name),&index,2))return 0;
    uint16_t seen[64];unsigned count=0;
    while(index){step(r,BA184_STEP_CHAIN);if(index>=0x9088u||count==64)return 0;
        for(unsigned i=0;i<count;++i)if(seen[i]==index)return 0;seen[count++]=index;
        step(r,BA184_STEP_ENTRY);
        unsigned char entry[16];if(!take(r,0xb27de0u+index*16u,entry,16))return 0;
        if(u32(entry)==4){unsigned char header[8];char actual[128];
            step(r,BA184_STEP_HEADER);if(!take(r,u32(entry+4),header,8))return 0;
            step(r,BA184_STEP_NAME);if(!text(r,u32(header),actual))return 0;
            if(!strcmp(actual,name)){
                unsigned flag=entry[9];*pointer=u32(entry+4);
                step(r,BA184_STEP_SOURCE);if(entry[8]!=source)return 0;
                step(r,BA184_STEP_BONES);if(header[4]!=bones)return 0;
                step(r,BA184_STEP_FLAGS);return stage==BA184_RESIDENT?flag<=1:flag==1;}}
        index=u16(entry+12);
    }return 0;
}
static uint32_t once(reader *r,unsigned stage)
{
    step(r,BA184_STEP_COUNT);uint32_t count,addon;if(!take(r,0xa3ced4u,&count,4)||count>32)return 0;
    step(r,BA184_STEP_ADDON);if(!take(r,0x1446900u,&addon,4)||addon)return 0;
    step(r,BA184_STEP_IDS);
    unsigned char ids[32];if(count&&!take(r,0xcfc894u,ids,count))return 0;
    unsigned source=0,map=0;
    for(unsigned i=0;i<count;++i){step(r,BA184_STEP_ZONE_ID);unsigned id=ids[i];if(!id||id>32)return 0;
        for(unsigned j=0;j<i;++j)if(ids[j]==id)return 0;
        step(r,BA184_STEP_ZONE_RECORD);unsigned char z[0x4c];if(!take(r,0xc9c288u+id*0x9cu,z,sizeof(z)))return 0;
        step(r,BA184_STEP_ZONE_NAME);
        unsigned n=0;while(n<64&&z[4+n]){if(z[4+n]<32||z[4+n]>126)return 0;++n;}
        if(!n||n==64)return 0;
        if(!strcmp((char *)z+4,"so_killspree_trainer")){step(r,BA184_STEP_ZONE_ALLOC);if(source||u32(z+0x44)!=4)return 0;source=id;}
        if(!strcmp((char *)z+4,"trainer")){step(r,BA184_STEP_ZONE_ALLOC);if(map||u32(z+0x44)!=4)return 0;map=id;}
    }
    uint32_t body=0,head=0;
    step(r,BA184_STEP_ZONE_SET);if(!source||!map||source==map)return 0;
    r->d->asset=1;if(!model(r,"coop_body_us_army",90,source,stage,&body))return 0;
    r->d->asset=2;if(!model(r,"coop_head_us_army",24,source,stage,&head))return 0;
    r->d->asset=0;step(r,BA184_STEP_ALIAS);if(body==head)return 0;
    if(stage==BA184_REGISTERED){
        /* Exact native G_ModelIndex: start1; cmp0x200/jl; table stride4.
           Read slot0 too for stable full-table comparison, but never accept it. */
        uint32_t registered[512];unsigned found_body=0,found_head=0;
        step(r,BA184_STEP_TABLE);if(!take(r,0x10c6510u,registered,sizeof(registered)))return 0;
        for(unsigned i=1;i<512;++i){found_body|=registered[i]==body;found_head|=registered[i]==head;}
        step(r,BA184_STEP_BODY_REGISTERED);if(!found_body)return 0;
        step(r,BA184_STEP_HEAD_REGISTERED);if(!found_head)return 0;
    }
    return BA184_TYPED_BODY|BA184_TYPED_HEAD|BA184_BODY_90|BA184_HEAD_24|BA184_SO_ZONE4
        |BA184_TRAINER_ZONE4|BA184_SAME_SO_SOURCE|BA184_ADDON_ZERO|BA184_DB_STABLE;
}
uint32_t ba184_probe(ba184_read_fn callback,void *context,unsigned stage)
{return ba184_probe_diagnose(callback,context,stage,NULL);}
uint32_t ba184_probe_diagnose(ba184_read_fn callback,void *context,unsigned stage,ba184_probe_diagnostic *out)
{
    ba184_probe_diagnostic local={0};ba184_probe_diagnostic *d=out?out:&local;memset(d,0,sizeof(*d));
    if(!callback||(stage!=BA184_RESIDENT&&stage!=BA184_REGISTERED)){d->status=BA184_PROBE_ARGUMENT;return 0;}
    d->pass=1;unsigned char a[32],b[32];reader r={callback,context,0,0,{0},d};
    pure_sha256_init(&r.digest);uint32_t first=once(&r,stage);pure_sha256_final(&r.digest,a);
    d->calls=r.calls;d->bytes=r.bytes;if(!first){if(!d->status)d->status=BA184_PROBE_PARSE;return 0;}
    d->pass=2;r.calls=r.bytes=0;pure_sha256_init(&r.digest);uint32_t second=once(&r,stage);pure_sha256_final(&r.digest,b);
    d->calls+=r.calls;d->bytes+=r.bytes;
    if(!second){if(!d->status)d->status=BA184_PROBE_PARSE;return 0;}
    if(first!=second||memcmp(a,b,32)){d->status=BA184_PROBE_DIGEST;return 0;}
    d->status=BA184_PROBE_OK;return first;
}
