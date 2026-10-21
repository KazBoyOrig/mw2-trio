/* Original isolated classifier fixtures; MIT. No game or network calls. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "service184.h"
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"failure line%u\n",(unsigned)__LINE__); return 1; } } while (0)
static unsigned state=0x184515u;
static unsigned random_value(void)
{
    state^=state<<13;state^=state>>17;state^=state<<5;return state;
}
static void fixture(unsigned char *p)
{
    memset(p,0,39);p[0]=5;p[1]=3;p[2]=20;p[3]=23;
    for(unsigned i=5;i<15;++i)p[i]=(unsigned char)(i*13u);
    for(unsigned i=0;i<6;++i)p[15+i*4]= (unsigned char)i;
}
static const unsigned char peer[4]={192,0,2,1}; /* Documentation-only fixture. */
static const service184_endpoint endpoint={2,{192,0,2,1},1500};
static service184_kind classify(const unsigned char *p,size_t n)
{
    return service184_classify_blocked(p,n,42,42,4,peer,&endpoint);
}
int main(void)
{
    unsigned char p[64],copy[64];fixture(p);memcpy(copy,p,39);
    CHECK(classify(p,39)==SERVICE184_EXTERNAL_IP_TYPE1);
    CHECK(!memcmp(p,copy,39));
    for(size_t n=0;n<64;++n) CHECK(classify(p,n)==(n==39?SERVICE184_EXTERNAL_IP_TYPE1:SERVICE184_UNKNOWN));
    CHECK(service184_classify_blocked(NULL,39,42,42,4,peer,&endpoint)==SERVICE184_UNKNOWN);
    CHECK(service184_classify_blocked(p,39,42,42,4,NULL,&endpoint)==SERVICE184_UNKNOWN);
    CHECK(service184_classify_blocked(p,39,42,42,4,peer,NULL)==SERVICE184_UNKNOWN);
    static const uint32_t invalid_sockets[]={0,UINT32_MAX,43};
    for(size_t i=0;i<sizeof(invalid_sockets)/sizeof(invalid_sockets[0]);++i)
        CHECK(service184_classify_blocked(p,39,42,invalid_sockets[i],4,peer,&endpoint)==SERVICE184_UNKNOWN);
    CHECK(service184_classify_blocked(p,39,41,42,4,peer,&endpoint)==SERVICE184_UNKNOWN);
    for(uint32_t type=0;type<8;++type)
        CHECK(service184_classify_blocked(p,39,42,42,type,peer,&endpoint)==(type==4?SERVICE184_EXTERNAL_IP_TYPE1:SERVICE184_UNKNOWN));
    service184_endpoint changed=endpoint;
    changed.family=6;CHECK(service184_classify_blocked(p,39,42,42,4,peer,&changed)==SERVICE184_UNKNOWN);
    changed=endpoint;changed.port_host_order=1501;
    CHECK(service184_classify_blocked(p,39,42,42,4,peer,&changed)==SERVICE184_UNKNOWN);
    for(unsigned i=0;i<4;++i) {
        changed=endpoint;changed.ipv4[i]^=1;
        CHECK(service184_classify_blocked(p,39,42,42,4,peer,&changed)==SERVICE184_UNKNOWN);
    }
    /* Every field byte mutation: UID/sequence are opaque and ignored; index
     * accepts only0..23. All other changes must make this exact class unknown. */
    for(unsigned at=0;at<39;++at) for(unsigned value=0;value<256;++value) {
        fixture(p);unsigned original=p[at];p[at]=(unsigned char)value;
        int valid=(at>=5&&at<15)||(at==3&&value<24)||value==original;
        CHECK(classify(p,39)==(valid?SERVICE184_EXTERNAL_IP_TYPE1:SERVICE184_UNKNOWN));
    }
    fixture(p);p[2]=23;CHECK(classify(p,39)==SERVICE184_UNKNOWN); /* Unsupported type2. */
    memset(p,255,4);p[4]='R';p[5]=0;CHECK(classify(p,39)==SERVICE184_UNKNOWN);
    SYSTEM_INFO info;GetSystemInfo(&info);size_t page=info.dwPageSize;
    unsigned char *pages=(unsigned char *)VirtualAlloc(NULL,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    CHECK(pages!=NULL);DWORD old;
    CHECK(VirtualProtect(pages+page,page,PAGE_NOACCESS,&old)!=0);
    for(unsigned pass=0;pass<40000;++pass) {
        size_t n=random_value()%65u;unsigned char *edge=pages+page-n;
        for(size_t i=0;i<n;++i)edge[i]=(unsigned char)random_value();
        if(n==39 && (pass&1))fixture(edge);
        service184_kind kind=classify(edge,n);
        CHECK(kind==SERVICE184_UNKNOWN||kind==SERVICE184_EXTERNAL_IP_TYPE1);
        CHECK(service184_kind_name(kind)!=NULL);
    }
    CHECK(VirtualFree(pages,0,MEM_RELEASE)!=0);
    printf("service184: %u checks passed; exhaustive39-byte field mutations;40000 guard-page inputs; no network/game calls\n",checks);
    return 0;
}
