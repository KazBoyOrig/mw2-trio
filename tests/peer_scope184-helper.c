/* Own MIT fixtures; no game, provider, sockets, accounts or file operations. */
#include "peer_scope184.h"
#include "pure_sha256.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static unsigned long checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"failure line %u\n",(unsigned)__LINE__);exit(1);}}while(0)
static const char host_text[]=
    "version=1\nrole=host\nsession_nonce=00112233445566778899aabbccddeeff\n"
    "local_ip=26.1.2.3\nlocal_port=28960\npeer_ip=26.4.5.6\npeer_port=28961\n";
static const char client_text[]=
    "version=1\nrole=client\nsession_nonce=00112233445566778899aabbccddeeff\n"
    "local_ip=26.4.5.6\nlocal_port=28961\npeer_ip=26.1.2.3\npeer_port=28960\n";
static uint32_t random_state=0xc7f25a19u;
static uint32_t random32(void)
{random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static peer_scope184_config parsed(const char *text)
{
    peer_scope184_config c;memset(&c,0,sizeof(c));
    CHECK(peer_scope184_parse(text,strlen(text),&c)==PEER_SCOPE184_OK);return c;
}
static void rejected(const void *data,size_t bytes)
{
    peer_scope184_config c,before;memset(&c,0xa5,sizeof(c));before=c;
    CHECK(peer_scope184_parse(data,bytes,&c)<0);CHECK(!memcmp(&c,&before,sizeof(c)));
}
static void replace_bad(const char *old,const char *replacement)
{
    char text[512];const char *at=strstr(host_text,old);CHECK(at!=NULL);
    size_t before=(size_t)(at-host_text),tail=strlen(at+strlen(old));
    CHECK(before+strlen(replacement)+tail<sizeof(text));
    memcpy(text,host_text,before);memcpy(text+before,replacement,strlen(replacement));
    memcpy(text+before+strlen(replacement),at+strlen(old),tail);
    rejected(text,before+strlen(replacement)+tail);
}
static void parser_tests(void)
{
    peer_scope184_config c=parsed(host_text),b=parsed(client_text);
    CHECK(c.version==1&&c.role==PEER_SCOPE184_HOST&&b.role==PEER_SCOPE184_CLIENT);
    CHECK(peer_scope184_pair_compatible(&c,&b));CHECK(peer_scope184_pair_compatible(&b,&c));
    CHECK(c.local.ipv4[0]==26&&c.local.port==28960&&c.peer.port==28961);
    char hex[65];pure_sha256_hex(c.source_fingerprint,hex);
    CHECK(!strcmp(hex,"8f6c6c06e936010ab402b54b2825ec6bfb90a7f93e574750a530d2206fdbaeb1"));
    char crlf[300];size_t n=0;
    for(size_t i=0;i<strlen(host_text);++i){if(host_text[i]=='\n')crlf[n++]='\r';crlf[n++]=host_text[i];}
    CHECK(peer_scope184_parse(crlf,n,&b)==PEER_SCOPE184_OK);
    CHECK(!memcmp(b.nonce,c.nonce,16)&&b.local.port==c.local.port);
    CHECK(memcmp(b.source_fingerprint,c.source_fingerprint,32)!=0);
    rejected(NULL,1);rejected(host_text,0);rejected(host_text,strlen(host_text)-1);
    rejected("",0);char over[257];memset(over,'a',sizeof(over));rejected(over,sizeof(over));
    CHECK(peer_scope184_parse(host_text,strlen(host_text),NULL)==PEER_SCOPE184_BAD_ARGUMENT);
    replace_bad("version=1\n","version=2\n");
    replace_bad("version=1\n","version=1\nversion=1\n");
    replace_bad("role=host\n","role=host\nrole=client\n");
    replace_bad("role=host\n","role=Host\n");replace_bad("role=host\n","role=client host\n");
    replace_bad("role=host\n","");replace_bad("role=host\n","unknown=host\n");
    replace_bad("role=host\n","role =host\n");replace_bad("role=host\n","role=host \n");
    replace_bad("role=host\n","role=host\n\n");replace_bad("role=host\n","role=host\r\n");
    replace_bad("role=host\n","role=host=client\n");replace_bad("role=host\n","role=\n");
    replace_bad("role=host\n","=host\n");replace_bad("role=host\n","role\thost\n");
    replace_bad("00112233445566778899aabbccddeeff","00000000000000000000000000000000");
    replace_bad("00112233445566778899aabbccddeeff","00112233445566778899aabbccddeef");
    replace_bad("00112233445566778899aabbccddeeff","00112233445566778899aabbccddeefff");
    replace_bad("00112233445566778899aabbccddeeff","00112233445566778899aabbccddeefg");
    const char *bad_ip[]={"0.0.0.0","0.1.2.3","127.0.0.2","224.0.0.1","239.255.255.250",
        "255.255.255.255","240.1.2.3","169.254.1.2","26.1.2.0","26.1.2.255",
        "26.1.2","26.1.2.3.4","256.1.2.3","026.1.2.3","26..2.3","localhost",
        "::1","26.1.2.-3","26.1.2.+3","26.1.2.3 ","26.1.2.03"};
    for(unsigned i=0;i<sizeof(bad_ip)/sizeof(bad_ip[0]);++i)replace_bad("26.1.2.3",bad_ip[i]);
    const char *bad_port[]={"0","65536","2896000000000000000000000000","028960","+1","-1","1.0","28960 "};
    for(unsigned i=0;i<sizeof(bad_port)/sizeof(bad_port[0]);++i)replace_bad("28960",bad_port[i]);
    replace_bad("peer_ip=26.4.5.6\npeer_port=28961","peer_ip=26.1.2.3\npeer_port=28960");
    char changed[256];memcpy(changed,host_text,sizeof(host_text));changed[12]=0;rejected(changed,strlen(host_text));
    changed[12]=(char)0x80;rejected(changed,strlen(host_text));
    peer_scope184_config sentinel;memset(&sentinel,0xa5,sizeof(sentinel));
    CHECK(peer_scope184_parse(&sentinel,sizeof(sentinel),&sentinel)==PEER_SCOPE184_BAD_ARGUMENT);
    for(size_t bytes=0;bytes<strlen(host_text);++bytes)rejected(host_text,bytes);
    const char *valid_ip[]={"10.1.2.3","172.16.1.2","172.31.2.3","192.168.1.2","100.64.1.2","100.127.2.3","26.1.2.3","8.8.8.8"};
    for(unsigned i=0;i<sizeof(valid_ip)/sizeof(valid_ip[0]);++i){
        char text[256];snprintf(text,sizeof(text),"version=1\nrole=host\nsession_nonce=00112233445566778899aabbccddeeff\nlocal_ip=%s\nlocal_port=1\npeer_ip=26.4.5.6\npeer_port=65535\n",valid_ip[i]);
        c=parsed(text);CHECK(c.local.port==1&&c.peer.port==65535);
    }
    c=parsed(host_text);b=parsed(client_text);b.role=PEER_SCOPE184_HOST;
    CHECK(!peer_scope184_pair_compatible(&c,&b));b=parsed(client_text);b.nonce[0]^=1;
    CHECK(!peer_scope184_pair_compatible(&c,&b));b=parsed(client_text);b.peer.port++;
    CHECK(!peer_scope184_pair_compatible(&c,&b));CHECK(!peer_scope184_pair_compatible(NULL,&b));
}
static void alias_tests(void)
{
    peer_scope184_config c=parsed(host_text),b=parsed(client_text);uint64_t a=0,d=0;
    CHECK(peer_scope184_derive(&c,0x12345678u,&a)==1&&a==UINT64_C(0x12345678166f079c));
    CHECK(peer_scope184_derive(&b,0x12345678u,&d)==1&&d==a+1);
    CHECK(peer_scope184_derive(&c,0,NULL)<0);CHECK(peer_scope184_derive(NULL,0,&a)<0);
    uint64_t sentinel=UINT64_MAX;c.role=0;
    CHECK(peer_scope184_derive(&c,0,&sentinel)<0&&sentinel==UINT64_MAX);
    c=parsed(host_text);CHECK(peer_scope184_derive(&c,0,(uint64_t *)(void *)&c)<0);
    for(unsigned i=0;i<20000;++i){
        c=parsed(host_text);for(unsigned j=0;j<16;++j)c.nonce[j]=(uint8_t)random32();c.nonce[0]|=1;
        b=c;b.role=PEER_SCOPE184_CLIENT;uint32_t high=random32();
        CHECK(peer_scope184_derive(&c,high,&a)==1);CHECK(peer_scope184_derive(&b,high,&d)==1);
        CHECK((uint32_t)(a>>32)==high&&(uint32_t)(d>>32)==high);
        CHECK((uint32_t)a>=2&&!(a&1)&&((uint32_t)d)==((uint32_t)a|1u)&&a!=d);
        uint64_t repeat;CHECK(peer_scope184_derive(&c,high,&repeat)==1&&repeat==a);
        CHECK(peer_scope184_derive(&b,~high,&repeat)==1&&repeat!=a);
    }
    c=parsed(host_text);CHECK(peer_scope184_derive(&c,0,&a)==1&&(a>>32)==0);
    CHECK(peer_scope184_derive(&c,UINT32_MAX,&a)==1&&(a>>32)==UINT32_MAX);
}
static void tuple_and_context_tests(void)
{
    peer_scope184_context c={0};peer_scope184_config cfg=parsed(host_text);uint64_t alias;
    CHECK(peer_scope184_latch(&c,host_text,strlen(host_text))==1);
    CHECK(peer_scope184_observe(&c,host_text,strlen(host_text))==1);
    CHECK(peer_scope184_context_derive(&c,7,&alias)==1&&(alias>>32)==7);
    CHECK(peer_scope184_matches(&c,PEER_SCOPE184_SEND,&cfg.local,&cfg.peer));
    CHECK(peer_scope184_matches(&c,PEER_SCOPE184_RECEIVE,&cfg.peer,&cfg.local));
    CHECK(!peer_scope184_matches(&c,PEER_SCOPE184_SEND,&cfg.peer,&cfg.local));
    CHECK(!peer_scope184_matches(&c,PEER_SCOPE184_RECEIVE,&cfg.local,&cfg.peer));
    CHECK(!peer_scope184_matches(&c,0,&cfg.local,&cfg.peer));
    CHECK(!peer_scope184_matches(&c,PEER_SCOPE184_SEND,NULL,&cfg.peer));
    CHECK(!peer_scope184_matches(&c,PEER_SCOPE184_SEND,&cfg.local,NULL));
    for(unsigned octet=0;octet<4;++octet)for(unsigned v=0;v<256;++v){
        peer_scope184_endpoint a=cfg.local,b=cfg.peer;a.ipv4[octet]=(uint8_t)v;
        CHECK(peer_scope184_matches(&c,PEER_SCOPE184_SEND,&a,&b)==(v==cfg.local.ipv4[octet]));
        a=cfg.local;b.ipv4[octet]=(uint8_t)v;
        CHECK(peer_scope184_matches(&c,PEER_SCOPE184_SEND,&a,&b)==(v==cfg.peer.ipv4[octet]));
    }
    for(unsigned port=0;port<=65536;++port){
        peer_scope184_endpoint a=cfg.local,b=cfg.peer;a.port=port;
        CHECK(peer_scope184_matches(&c,PEER_SCOPE184_SEND,&a,&b)==(port==cfg.local.port));
        a=cfg.local;b.port=port;
        CHECK(peer_scope184_matches(&c,PEER_SCOPE184_SEND,&a,&b)==(port==cfg.peer.port));
    }
    CHECK(peer_scope184_latch(&c,host_text,strlen(host_text))==PEER_SCOPE184_ALREADY_ATTEMPTED);
    CHECK(!peer_scope184_matches(&c,PEER_SCOPE184_SEND,&cfg.local,&cfg.peer));
    CHECK(peer_scope184_observe(&c,host_text,strlen(host_text))<0);
    peer_scope184_context empty={0};CHECK(peer_scope184_latch(&empty,"bad\n",4)<0);
    CHECK(empty.attempted==1&&peer_scope184_latch(&empty,host_text,strlen(host_text))<0);
    peer_scope184_context unknown={0};CHECK(peer_scope184_observe(&unknown,host_text,strlen(host_text))<0);
    CHECK(peer_scope184_latch(&unknown,host_text,strlen(host_text))<0);
    peer_scope184_context good={0};CHECK(peer_scope184_latch(&good,host_text,strlen(host_text))==1);
    for(size_t i=0;i<sizeof(good.config);++i){
        peer_scope184_context modified=good;((uint8_t *)(void *)&modified.config)[i]^=1;
        CHECK(peer_scope184_observe(&modified,host_text,strlen(host_text))==PEER_SCOPE184_CONTEXT_CHANGED);
        CHECK(!peer_scope184_matches(&modified,PEER_SCOPE184_SEND,&cfg.local,&cfg.peer));
        CHECK(peer_scope184_context_derive(&modified,0,&alias)<0);
    }
    for(unsigned i=0;i<32;++i){
        peer_scope184_context modified=good;modified.seal[i]^=1;
        CHECK(peer_scope184_observe(&modified,host_text,strlen(host_text))==PEER_SCOPE184_CONTEXT_CHANGED);
    }
    peer_scope184_context modified=good;
    CHECK(peer_scope184_observe(&modified,NULL,strlen(host_text))==PEER_SCOPE184_CONFIG_CHANGED);
    modified=good;CHECK(peer_scope184_context_derive(&modified,0,(uint64_t *)(void *)&modified)<0);
    CHECK(peer_scope184_latch(NULL,host_text,strlen(host_text))<0);
    CHECK(peer_scope184_observe(NULL,host_text,strlen(host_text))<0);
    CHECK(peer_scope184_context_derive(NULL,0,&alias)<0);
}
static void mutation_tests(void)
{
    const size_t bytes=strlen(host_text);char text[256];peer_scope184_config valid=parsed(host_text);
    peer_scope184_context good={0};CHECK(peer_scope184_latch(&good,host_text,bytes)==1);
    for(size_t i=0;i<bytes;++i)for(unsigned value=0;value<256;++value){
        if(value==(unsigned char)host_text[i])continue;
        memcpy(text,host_text,bytes);text[i]=(char)value;
        peer_scope184_context changed=good;
        CHECK(peer_scope184_observe(&changed,text,bytes)==PEER_SCOPE184_CONFIG_CHANGED);
        CHECK(!peer_scope184_matches(&changed,PEER_SCOPE184_SEND,&valid.local,&valid.peer));
        CHECK(peer_scope184_observe(&changed,host_text,bytes)<0);
        peer_scope184_config result,sentinel;memset(&result,0xa5,sizeof(result));sentinel=result;
        int parsed_result=peer_scope184_parse(text,bytes,&result);
        if(parsed_result==1){
            CHECK(peer_scope184_endpoint_eligible(&result.local)&&peer_scope184_endpoint_eligible(&result.peer));
            uint64_t alias;CHECK(peer_scope184_derive(&result,0xa5a5a5a5u,&alias)==1&&(alias>>32)==0xa5a5a5a5u);
            CHECK(memcmp(result.source_fingerprint,valid.source_fingerprint,32)!=0);
        }else CHECK(!memcmp(&result,&sentinel,sizeof(result)));
    }
    for(unsigned i=0;i<30000;++i){
        size_t n=(size_t)(random32()%257u);for(size_t j=0;j<n;++j)text[j]=(char)random32();
        peer_scope184_config c,sentinel;memset(&c,0xa5,sizeof(c));sentinel=c;
        int result=peer_scope184_parse(text,n,&c);
        CHECK(result<0);CHECK(!memcmp(&c,&sentinel,sizeof(c)));
    }
}
static void prefix_tests(void)
{
    const uint8_t network[4]={192,168,1,0};uint8_t ip[4]={192,168,1,126};
    CHECK(peer_scope184_prefix_host(ip,network,25));ip[3]=127;
    CHECK(!peer_scope184_prefix_host(ip,network,25));ip[3]=0;
    CHECK(!peer_scope184_prefix_host(ip,network,25));ip[3]=128;
    CHECK(!peer_scope184_prefix_host(ip,network,25));
    const uint8_t pair[4]={26,1,2,2};ip[0]=26;ip[1]=1;ip[2]=2;ip[3]=2;
    CHECK(peer_scope184_prefix_host(ip,pair,31));ip[3]=3;CHECK(peer_scope184_prefix_host(ip,pair,31));
    CHECK(!peer_scope184_prefix_host(ip,pair,32));ip[3]=2;CHECK(peer_scope184_prefix_host(ip,pair,32));
    CHECK(!peer_scope184_prefix_host(ip,pair,0));CHECK(!peer_scope184_prefix_host(ip,pair,33));
    CHECK(!peer_scope184_prefix_host(NULL,pair,24));CHECK(!peer_scope184_prefix_host(ip,NULL,24));
    CHECK(!peer_scope184_prefix_host(ip,pair,24)); /* Noncanonical network base. */
}
int main(void)
{
    parser_tests();alias_tests();tuple_and_context_tests();mutation_tests();prefix_tests();
    printf("peer_scope184: %lu pure checks passed; no game/provider/network calls\n",checks);return 0;
}
