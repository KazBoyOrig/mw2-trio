/* Original isolated parser test; MIT. No game, identity or socket calls. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "reliable184.h"
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"failure line%u\n",(unsigned)__LINE__); return 1; } } while (0)
static size_t frame(unsigned char *p, unsigned sequence, const char *text)
{
    memset(p,255,4); p[4]='R'; p[5]=0; p[6]=(unsigned char)sequence;
    size_t n=strlen(text)+1; memcpy(p+7,text,n); return 7+n;
}
static unsigned random_state=0x184184u;
static unsigned next_random(void)
{
    random_state^=random_state<<13; random_state^=random_state>>17;
    random_state^=random_state<<5; return random_state;
}
int main(void)
{
    unsigned char p[128], original[128];
    reliable184_token t=reliable184_decode(NULL,0);
    CHECK(t.outer==RELIABLE184_NOT_RECOGNIZED);
    static const struct { const char *text; reliable184_reason reason; } rejects[]={
        {"coopReject COOP_WRONGSTATE",RELIABLE184_WRONGSTATE},
        {"coopReject COOP_FULL",RELIABLE184_FULL},
        {"coopReject COOP_WRONGPROTOCOL",RELIABLE184_WRONGPROTOCOL},
        {"coopReject secret-not-for-output",RELIABLE184_REASON_UNKNOWN}
    };
    for(size_t r=0;r<sizeof(rejects)/sizeof(rejects[0]);++r) {
        size_t n=frame(p,255,rejects[r].text); memcpy(original,p,n);
        t=reliable184_decode(p,n);
        CHECK(t.outer==RELIABLE184_R && t.first_command==RELIABLE184_REJECT
              && t.rejection==rejects[r].reason);
        CHECK(!memcmp(original,p,n));
        for(size_t cut=0;cut<n;++cut) {
            t=reliable184_decode(p,cut);
            CHECK(t.first_command==RELIABLE184_COMMAND_UNKNOWN && t.rejection==RELIABLE184_REASON_NONE);
        }
    }
    size_t n=frame(p,0,"coopConnect");
    /* A forged rejection string after the command's NUL is binary payload.
     * It must never override the first recognized command. */
    memcpy(p+n,"coopReject COOP_WRONGSTATE",26); n+=26;
    t=reliable184_decode(p,n);
    CHECK(t.first_command==RELIABLE184_CONNECT && t.rejection==RELIABLE184_REASON_NONE);
    n=frame(p,0,"unknown"); memcpy(p+n,"coopReject COOP_FULL",20); n+=20;
    t=reliable184_decode(p,n); CHECK(t.first_command==RELIABLE184_COMMAND_UNKNOWN);
    memset(p,'x',sizeof(p)); memset(p,255,4);p[4]='R';p[5]=0;p[6]=0;p[80]=0;
    t=reliable184_decode(p,sizeof(p));CHECK(t.first_command==RELIABLE184_COMMAND_UNKNOWN);
    static const char *acks[]={"RA 0","RA 9","RA 10","RA 255"};
    for(size_t i=0;i<sizeof(acks)/sizeof(acks[0]);++i) {
        memset(p,255,4);n=strlen(acks[i])+1;memcpy(p+4,acks[i],n);n+=4;
        CHECK(reliable184_decode(p,n).outer==RELIABLE184_RA);
        for(size_t cut=0;cut<n;++cut) CHECK(reliable184_decode(p,cut).outer!=RELIABLE184_RA);
    }
    static const char *bad[]={"RA -1","RA 256","RA 00","RA 01","RA 2x","RA ","RA 1234"};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        memset(p,255,4);n=strlen(bad[i])+1;memcpy(p+4,bad[i],n);n+=4;
        CHECK(reliable184_decode(p,n).outer==RELIABLE184_NOT_RECOGNIZED);
    }
    /* Place every bounded input immediately before PAGE_NOACCESS. */
    SYSTEM_INFO info;GetSystemInfo(&info);size_t page=info.dwPageSize;
    unsigned char *pages=(unsigned char *)VirtualAlloc(NULL,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    CHECK(pages!=NULL);DWORD old;
    CHECK(VirtualProtect(pages+page,page,PAGE_NOACCESS,&old)!=0);
    for(unsigned pass=0;pass<40000;++pass) {
        n=next_random()%129u;unsigned char *edge=pages+page-n;
        for(size_t i=0;i<n;++i)edge[i]=(unsigned char)next_random();
        if(n>=7 && (pass&1)) {memset(edge,255,4);edge[4]='R';edge[5]=0;}
        t=reliable184_decode(edge,n);
        CHECK((unsigned)t.outer<=RELIABLE184_RA);
        CHECK((unsigned)t.first_command<=RELIABLE184_BANDWIDTH);
        CHECK((unsigned)t.rejection<=RELIABLE184_WRONGPROTOCOL);
        CHECK(reliable184_outer_name(t.outer)!=NULL && reliable184_command_name(t.first_command)!=NULL
              && reliable184_reason_name(t.rejection)!=NULL);
    }
    CHECK(VirtualFree(pages,0,MEM_RELEASE)!=0);
    printf("reliable184: %u checks passed; 40000 bounded guard-page mutations; no network/game calls\n",checks);
    return 0;
}
