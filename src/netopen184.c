/* Own inactive build184 LAN opener; MIT. No active hook or EXE patch.
 * Exact native socket allocation/port fallback; existing game send/receive.
 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bootstrap.h"
#include "pure_sha256.h"
#include "netopen184.h"
#include "net184.h"
#include "peer_transport184.h"

typedef void (__cdecl *native_void)(void);
typedef int (__cdecl *native_register)(void);
typedef struct open_guard184 { uintptr_t va; uint32_t length; const char *sha256; } open_guard184;
#include "netopen184-guards.inc"
_Static_assert(sizeof(void *) == 4, "x86 required");
static netopen184_report report = {1,0,0,0,0,0,0,0};
static DWORD owner_tid;
static int image_checked, image_valid;
static int opened_for_qa;
static int opened_for_peer;

static int read_memory(uintptr_t p, void *out, SIZE_T bytes, int code)
{
    MEMORY_BASIC_INFORMATION m;
    if (!bytes || VirtualQuery((void *)p, &m, sizeof(m)) != sizeof(m)
        || m.State != MEM_COMMIT || (m.Protect & (PAGE_GUARD|PAGE_NOACCESS))) return 0;
    DWORD protection = m.Protect & 0xffu;
    if (protection != PAGE_READONLY && protection != PAGE_READWRITE
        && protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ
        && protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY) return 0;
    if (code && (m.Type != MEM_IMAGE || m.AllocationBase != (void *)0x400000u
        || (protection != PAGE_EXECUTE_READ && protection != PAGE_EXECUTE_READWRITE))) return 0;
    uintptr_t begin = (uintptr_t)m.BaseAddress;
    if (p < begin || p-begin >= m.RegionSize || bytes > m.RegionSize-(p-begin)) return 0;
    if (out) memcpy(out, (void *)p, bytes);
    return 1;
}

static int read32(uintptr_t p, uint32_t *out) { return read_memory(p,out,4,0); }

static int enabled(void)
{
    wchar_t path[MAX_PATH];
    DWORD n=GetModuleFileNameW(NULL,path,MAX_PATH);
    if(!n || n>=MAX_PATH)return 0;
    while(n && path[n-1]!=L'\\')--n;
    if(!n || n+32>=MAX_PATH)return 0;
    lstrcpyW(path+n,L"mw2-trio-party-test.enable");
    HANDLE h=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,
                         OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(h==INVALID_HANDLE_VALUE)return 0;
    BY_HANDLE_FILE_INFORMATION i;
    int ok=GetFileInformationByHandle(h,&i)
        && !(i.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
    CloseHandle(h);return ok;
}

static int code_ok(void)
{
    for(unsigned i=0;i<sizeof(open_guards184)/sizeof(open_guards184[0]);++i){
        const open_guard184 *g=&open_guards184[i];char digest[65];
        if(!read_memory(g->va,NULL,g->length,1))return 0;
        pure_sha256_data((void *)g->va,g->length,digest);
        if(strcmp(digest,g->sha256))return 0;
    }
    return 1;
}

static int zero32(uintptr_t p) { uint32_t v;return read32(p,&v)&&v==0; }
static int zero8(uintptr_t p) { uint8_t v;return read_memory(p,&v,1,0)&&v==0; }
static int dvar_false(uintptr_t global)
{
    uint32_t p;uint8_t type,value;
    return read32(global,&p)&&p&&!(p&3u)&&read_memory(p+12,&type,1,0)
        && type==0&&read_memory(p+16,&value,1,0)&&value==0;
}

static int safe_bind_address(void)
{
    if(net184_peer_requested()){
        peer_scope184_config c;uint32_t ip_dvar,port_dvar,flags,pending,latched_port;uint8_t type;
        if(!net184_peer_ready()||peer_transport184_get_config(&c)!=1
            ||!read32(0x19ff940u,&ip_dvar)||!ip_dvar
            ||!read_memory(ip_dvar+12,&type,1,0)||type!=7
            ||!read32(ip_dvar+8,&flags)||!(flags&2u)
            ||!read32(ip_dvar+32,&pending)||!pending)return 0;
        char wanted[16],latched[16];
        int count=snprintf(wanted,sizeof(wanted),"%u.%u.%u.%u",c.local.ipv4[0],c.local.ipv4[1],c.local.ipv4[2],c.local.ipv4[3]);
        if(count<1||count>15)return 0;
        unsigned n;
        for(n=0;n<sizeof(latched);++n){
            if(!read_memory(pending+n,latched+n,1,0))return 0;
            if(!latched[n])break;
        }
        if(n==sizeof(latched)||strcmp(latched,wanted)
            ||!read32(0x1a00b4cu,&port_dvar)||!port_dvar
            ||!read_memory(port_dvar+12,&type,1,0)||type!=5
            ||!read32(port_dvar+8,&flags)||!(flags&2u)
            ||!read32(port_dvar+32,&latched_port)||latched_port!=c.local.port)return 0;
        /* Guarded OpenIP re-registers both dvars before the first socket call.
         * Native reregister633B90 applies the full +0x20 pending value through
         * its source0 setter whenever flags bit2 is present. No raw stores or
         * custom setter are needed. Actual bound tuple is checked afterwards. */
        return 1;
    }
    uint32_t p,text;
    if(!read32(0x19ff940u,&p))return 0;
    if(!p)return 1; /* OpenIP registers the verified default "localhost". */
    if(!read32(p+16,&text)||!text)return 0;
    char name[16];
    for(unsigned i=0;i<sizeof(name);++i){
        if(!read_memory(text+i,name+i,1,0))return 0;
        if(!name[i]){
            return !strcmp(name,"localhost")||!strcmp(name,"0.0.0.0")||!strcmp(name,"127.0.0.1")
                ||(net184_loopback_ready()&&!strcmp(name,"127.0.0.2"));
        }
    }
    return 0;
}

static int finish(netopen184_report *out, uint32_t result)
{
    report.last_result=result;
    uint32_t socket=0,configured=0,p=0,port=0;
    if(image_valid){
        read32(0x1a00b50u,&socket);read32(0x1a00b64u,&configured);
        if(read32(0x1a00b4cu,&p)&&p)read32(p+16,&port);
    }
    report.socket_present=socket&&socket!=0xffffffffu;
    report.configured_state=configured;report.bound_port=port;
    *out=report;
    return result==1?1:-(int)result;
}

int netopen184_open(DWORD main_thread_id, netopen184_report *out, uint32_t bytes)
{
    if(!main_thread_id||GetCurrentThreadId()!=main_thread_id||!out||bytes!=sizeof(*out))return -2;
    if(!owner_tid)owner_tid=main_thread_id;
    if(owner_tid!=main_thread_id)return -2;
    if(!enabled())return finish(out,3);
    int qa=net184_loopback_requested();
    if(qa&&!net184_loopback_ready())return finish(out,6);
    int peer=net184_peer_requested();
    if(peer&&(!net184_peer_ready()||qa))return finish(out,6);
    if(!image_checked){
        bootstrap_report r;image_checked=1;
        image_valid=(uintptr_t)GetModuleHandleW(NULL)==0x400000u
            &&bootstrap_guard_current(&r,sizeof(r))==1&&r.flags==MW2TRIO_GUARD_ALL;
    }
    if(!image_valid||!code_ok())return finish(out,4);
    if(report.attempted)return finish(out,report.opened?1:9);
    uint32_t session=0,connection=0,locals=0;
    if(!read32(0x934450u,&session)||session!=0x1bde8b8u
        ||!read32(0x925bc0u,&connection)||connection>2
        ||!read32(0x738c6cu,&locals)||locals!=1
        ||!zero8(0x88897cu)||!zero8(0x934449u)||!zero8(0x1bde8c0u)
        ||!zero8(0x1beb4a1u)||!zero32(0x1a00b50u)
        ||!zero32(0x19ff94cu)||!zero32(0x19ff948u)||!zero32(0x19ff964u)
        ||!zero32(0x734a4cu)||!zero32(0x1a00b60u))return finish(out,5);
    /* This helper only registers ordinary native net_* configuration dvars. */
    ((native_register)(uintptr_t)0x6390b0u)();
    if(!dvar_false(0x1a00b5cu)||!dvar_false(0x19ff944u)||!safe_bind_address())return finish(out,6);
    report.attempted=1;
    WSADATA data;
    report.startup_result=(uint32_t)WSAStartup(MAKEWORD(1,1),&data);
    if(report.startup_result)return finish(out,7);
    if(data.wVersion!=MAKEWORD(1,1)){
        WSACleanup();report.startup_result=WSAVERNOTSUPPORTED;return finish(out,7);
    }
    /* Native OpenIP preserves ESI/EDI and takes no stack or register arguments.
     * It supplies EDI itself to its register-ABI socket helper0x6389A0.
     * Do not call that lower helper as an ordinary cdecl function.
     */
    ((native_void)(uintptr_t)0x48c5d0u)();
    uint32_t socket=0;
    if(!read32(0x1a00b50u,&socket)||!socket||socket==0xffffffffu
        ||!zero32(0x1a00b60u))return finish(out,8);
    if(qa){
        struct sockaddr_in bound;int size=sizeof(bound);
        memset(&bound,0,sizeof(bound));
        if(getsockname((SOCKET)socket,(struct sockaddr *)&bound,&size)==SOCKET_ERROR
            ||size!=(int)sizeof(bound)||bound.sin_family!=AF_INET
            ||bound.sin_addr.s_addr!=htonl(MW2TRIO_QA_IPV4_HOST))return finish(out,8);
    }
    if(peer&&peer_transport184_socket_matches((SOCKET)socket)!=1)return finish(out,8);
    opened_for_qa=qa;
    opened_for_peer=peer;
    report.opened=1;
    return finish(out,1);
}

int netopen184_qa_ready(void)
{
    if(!image_valid||!opened_for_qa||!report.opened||!net184_loopback_ready())return 0;
    uint32_t socket=0;struct sockaddr_in bound;int size=sizeof(bound);
    memset(&bound,0,sizeof(bound));
    return read32(0x1a00b50u,&socket)&&socket&&socket!=0xffffffffu
        &&getsockname((SOCKET)socket,(struct sockaddr *)&bound,&size)!=SOCKET_ERROR
        &&size==(int)sizeof(bound)&&bound.sin_family==AF_INET
        &&bound.sin_addr.s_addr==htonl(MW2TRIO_QA_IPV4_HOST);
}
int netopen184_peer_ready(void)
{
    uint32_t socket=0;
    return image_valid&&opened_for_peer&&report.opened&&net184_peer_ready()
        &&read32(0x1a00b50u,&socket)&&socket&&socket!=0xffffffffu
        &&peer_transport184_socket_matches((SOCKET)socket)==1;
}
int netopen184_peer_observe(void)
{return !opened_for_peer?0:netopen184_peer_ready()?1:-1;}
