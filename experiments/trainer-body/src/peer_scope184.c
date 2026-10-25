/* Own MIT candidate. Pure SHA/config/tuple support, never opens a socket. */
#include "peer_scope184.h"
#include "pure_sha256.h"
#include <string.h>

static const char alias_domain[]="mw2-trio/peer-scope184/alias/v1";
static const char config_domain[]="mw2-trio/peer-scope184/config/v1";
static const char seal_domain[]="mw2-trio/peer-scope184/seal/v1";
enum { PS_READY=1, PS_FAILED=2 };
static uint32_t be32(const uint8_t p[4])
{return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static void hash32(pure_sha256_context *h,uint32_t value)
{
    uint8_t b[4]={(uint8_t)(value>>24),(uint8_t)(value>>16),(uint8_t)(value>>8),(uint8_t)value};
    pure_sha256_update(h,b,4);
}
static void fingerprint(const void *data,size_t bytes,uint8_t out[32])
{
    pure_sha256_context h;pure_sha256_init(&h);
    pure_sha256_update(&h,config_domain,sizeof(config_domain));hash32(&h,(uint32_t)bytes);
    pure_sha256_update(&h,data,bytes);pure_sha256_final(&h,out);
}
static int equal_endpoint(const peer_scope184_endpoint *a,const peer_scope184_endpoint *b)
{return a&&b&&a->port==b->port&&!memcmp(a->ipv4,b->ipv4,4);}
int peer_scope184_endpoint_eligible(const peer_scope184_endpoint *e)
{
    return e&&e->port>=1&&e->port<=65535&&e->ipv4[0]!=0&&e->ipv4[0]!=127
        &&e->ipv4[0]<224&&!(e->ipv4[0]==169&&e->ipv4[1]==254)
        &&e->ipv4[3]!=0&&e->ipv4[3]!=255;
}
int peer_scope184_prefix_host(const uint8_t ip[4],const uint8_t network[4],unsigned prefix)
{
    if(!ip||!network||!prefix||prefix>32)return 0;
    uint32_t mask=UINT32_MAX<<(32-prefix),address=be32(ip),base=be32(network);
    if((base&mask)!=base||(address&mask)!=base)return 0;
    return prefix>=31||(address!=base&&address!=(base|~mask));
}
static int config_valid(const peer_scope184_config *c)
{
    if(!c||c->version!=1||(c->role!=PEER_SCOPE184_HOST&&c->role!=PEER_SCOPE184_CLIENT)
        ||!peer_scope184_endpoint_eligible(&c->local)||!peer_scope184_endpoint_eligible(&c->peer)
        ||equal_endpoint(&c->local,&c->peer)||!c->source_bytes
        ||c->source_bytes>PEER_SCOPE184_MAX_CONFIG)return 0;
    unsigned nonzero=0;for(unsigned i=0;i<16;++i)nonzero|=c->nonce[i];
    return nonzero!=0;
}
static int decimal(const uint8_t *p,size_t n,uint32_t max,uint32_t *out)
{
    if(!n||(n>1&&p[0]=='0'))return 0;
    uint32_t value=0;
    for(size_t i=0;i<n;++i){
        if(p[i]<'0'||p[i]>'9')return 0;
        uint32_t digit=p[i]-'0';if(value>(max-digit)/10u)return 0;
        value=value*10u+digit;
    }
    *out=value;return 1;
}
static int ipv4(const uint8_t *p,size_t n,uint8_t out[4])
{
    size_t begin=0;unsigned octet=0;
    for(size_t i=0;i<=n;++i)if(i==n||p[i]=='.'){
        uint32_t value;if(octet>=4||!decimal(p+begin,i-begin,255,&value))return 0;
        out[octet++]=(uint8_t)value;begin=i+1;
    }
    return octet==4;
}
static int hex(uint8_t c)
{
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}
static int key(const uint8_t *p,size_t n)
{
    static const char *const keys[]={"version","role","session_nonce","local_ip","local_port","peer_ip","peer_port"};
    for(unsigned i=0;i<7;++i)if(n==strlen(keys[i])&&!memcmp(p,keys[i],n))return (int)i;
    return -1;
}
static int disjoint(const void *a,size_t an,const void *b,size_t bn)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    if(x>UINTPTR_MAX-an||y>UINTPTR_MAX-bn)return 0;
    return x+an<=y||y+bn<=x;
}
int peer_scope184_parse(const void *input,size_t bytes,peer_scope184_config *out)
{
    if(!input||!out||!bytes||bytes>PEER_SCOPE184_MAX_CONFIG
        ||!disjoint(input,bytes,out,sizeof(*out)))return PEER_SCOPE184_BAD_ARGUMENT;
    const uint8_t *p=input;peer_scope184_config c;memset(&c,0,sizeof(c));
    unsigned seen=0,line_ending=0;size_t begin=0;
    while(begin<bytes){
        size_t end=begin;while(end<bytes&&p[end]!='\n')++end;
        if(end==bytes)return PEER_SCOPE184_BAD_CONFIG;
        size_t limit=end;unsigned ending=1;
        if(limit>begin&&p[limit-1]=='\r'){--limit;ending=2;}
        if(line_ending&&line_ending!=ending)return PEER_SCOPE184_BAD_CONFIG;
        line_ending=ending;
        size_t equals=limit;
        for(size_t i=begin;i<limit;++i){
            if(p[i]<33||p[i]>126)return PEER_SCOPE184_BAD_CONFIG;
            if(p[i]=='='){if(equals!=limit)return PEER_SCOPE184_BAD_CONFIG;equals=i;}
        }
        if(equals==begin||equals>=limit-1)return PEER_SCOPE184_BAD_CONFIG;
        int k=key(p+begin,equals-begin);
        if(k<0||(seen&(1u<<(unsigned)k)))return PEER_SCOPE184_BAD_CONFIG;
        seen|=1u<<(unsigned)k;const uint8_t *v=p+equals+1;size_t n=limit-equals-1;
        int valid=1;
        switch(k){
        case 0:valid=n==1&&v[0]=='1';c.version=1;break;
        case 1:
            if(n==4&&!memcmp(v,"host",4))c.role=PEER_SCOPE184_HOST;
            else if(n==6&&!memcmp(v,"client",6))c.role=PEER_SCOPE184_CLIENT;
            else valid=0;break;
        case 2:
            valid=n==32;
            if(valid)for(unsigned i=0;i<16;++i){
                int a=hex(v[2*i]),b=hex(v[2*i+1]);
                if(a<0||b<0){valid=0;break;}c.nonce[i]=(uint8_t)((a<<4)|b);
            }
            break;
        case 3:valid=ipv4(v,n,c.local.ipv4);break;
        case 4:valid=decimal(v,n,65535,&c.local.port);break;
        case 5:valid=ipv4(v,n,c.peer.ipv4);break;
        case 6:valid=decimal(v,n,65535,&c.peer.port);break;
        }
        if(!valid)return PEER_SCOPE184_BAD_CONFIG;
        begin=end+1;
    }
    c.source_bytes=(uint32_t)bytes;
    if(seen!=127||!config_valid(&c))return PEER_SCOPE184_BAD_CONFIG;
    fingerprint(input,bytes,c.source_fingerprint);*out=c;return PEER_SCOPE184_OK;
}
int peer_scope184_pair_compatible(const peer_scope184_config *a,const peer_scope184_config *b)
{
    return config_valid(a)&&config_valid(b)&&a->role!=b->role
        &&!memcmp(a->nonce,b->nonce,16)&&equal_endpoint(&a->local,&b->peer)
        &&equal_endpoint(&a->peer,&b->local);
}
int peer_scope184_derive(const peer_scope184_config *c,uint32_t high,uint64_t *out)
{
    if(!out||!config_valid(c)||!disjoint(c,sizeof(*c),out,sizeof(*out)))return PEER_SCOPE184_BAD_ARGUMENT;
    pure_sha256_context h;uint8_t digest[32];pure_sha256_init(&h);
    pure_sha256_update(&h,alias_domain,sizeof(alias_domain));pure_sha256_update(&h,c->nonce,16);
    pure_sha256_final(&h,digest);uint32_t low=be32(digest)&0x7ffffffeu;
    if(!low)low=2;if(c->role==PEER_SCOPE184_CLIENT)low|=1;
    *out=(uint64_t)high<<32|low;return PEER_SCOPE184_OK;
}
static void seal(const peer_scope184_config *c,uint8_t out[32])
{
    pure_sha256_context h;pure_sha256_init(&h);pure_sha256_update(&h,seal_domain,sizeof(seal_domain));
    hash32(&h,c->version);hash32(&h,c->role);pure_sha256_update(&h,c->nonce,16);
    pure_sha256_update(&h,c->local.ipv4,4);hash32(&h,c->local.port);
    pure_sha256_update(&h,c->peer.ipv4,4);hash32(&h,c->peer.port);hash32(&h,c->source_bytes);
    pure_sha256_update(&h,c->source_fingerprint,32);pure_sha256_final(&h,out);
}
static int poison(peer_scope184_context *c,int result)
{if(c){c->state=PS_FAILED;c->last_result=result;}return result;}
static int ready(peer_scope184_context *c)
{
    if(!c)return PEER_SCOPE184_BAD_ARGUMENT;
    if(c->attempted!=1||c->state!=PS_READY||c->last_result!=PEER_SCOPE184_OK)
        return poison(c,PEER_SCOPE184_NOT_READY);
    uint8_t digest[32];
    if(!config_valid(&c->config))return poison(c,PEER_SCOPE184_CONTEXT_CHANGED);
    seal(&c->config,digest);
    if(memcmp(digest,c->seal,32))return poison(c,PEER_SCOPE184_CONTEXT_CHANGED);
    return PEER_SCOPE184_OK;
}
int peer_scope184_latch(peer_scope184_context *c,const void *data,size_t bytes)
{
    if(!c)return PEER_SCOPE184_BAD_ARGUMENT;
    if(c->attempted||c->state)return poison(c,PEER_SCOPE184_ALREADY_ATTEMPTED);
    c->attempted=1;
    if(!data||!bytes||bytes>PEER_SCOPE184_MAX_CONFIG||!disjoint(c,sizeof(*c),data,bytes))
        return poison(c,PEER_SCOPE184_BAD_ARGUMENT);
    peer_scope184_config parsed;int result=peer_scope184_parse(data,bytes,&parsed);
    if(result!=PEER_SCOPE184_OK)return poison(c,result);
    c->config=parsed;seal(&c->config,c->seal);c->state=PS_READY;c->last_result=PEER_SCOPE184_OK;
    return PEER_SCOPE184_OK;
}
int peer_scope184_observe(peer_scope184_context *c,const void *data,size_t bytes)
{
    int result=ready(c);if(result!=PEER_SCOPE184_OK)return result;
    if(!data||bytes!=c->config.source_bytes||bytes>PEER_SCOPE184_MAX_CONFIG
        ||!disjoint(c,sizeof(*c),data,bytes))return poison(c,PEER_SCOPE184_CONFIG_CHANGED);
    uint8_t digest[32];fingerprint(data,bytes,digest);
    if(memcmp(digest,c->config.source_fingerprint,32))return poison(c,PEER_SCOPE184_CONFIG_CHANGED);
    return PEER_SCOPE184_OK;
}
int peer_scope184_matches(peer_scope184_context *c,unsigned direction,
                         const peer_scope184_endpoint *source,const peer_scope184_endpoint *destination)
{
    if(ready(c)!=PEER_SCOPE184_OK)return 0;
    if(direction==PEER_SCOPE184_SEND)
        return equal_endpoint(source,&c->config.local)&&equal_endpoint(destination,&c->config.peer);
    if(direction==PEER_SCOPE184_RECEIVE)
        return equal_endpoint(source,&c->config.peer)&&equal_endpoint(destination,&c->config.local);
    return 0;
}
int peer_scope184_context_derive(peer_scope184_context *c,uint32_t high,uint64_t *out)
{
    int result=ready(c);if(result!=PEER_SCOPE184_OK)return result;
    if(!out||!disjoint(c,sizeof(*c),out,sizeof(*out)))return PEER_SCOPE184_BAD_ARGUMENT;
    return peer_scope184_derive(&c->config,high,out);
}
