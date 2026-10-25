/* Own MIT opt-in adapter. File/interface/socket-metadata reads only;
 * no game-memory access, provider APIs, socket opens or packet I/O. */
#include "peer_transport184.h"
#include <iphlpapi.h>
#include <stddef.h>
#include <string.h>

#define PT_ADAPTER_INITIAL (15u*1024u)
#define PT_ADAPTER_LIMIT (1024u*1024u)
static volatile LONG pt_attempted,pt_state,pt_result,pt_error,pt_observations,pt_interfaces,pt_sockets;
static DWORD pt_owner;
static peer_scope184_context pt_scope; /* Published once; immutable afterwards. */
static wchar_t pt_enable[MAX_PATH],pt_config[MAX_PATH],pt_qa[MAX_PATH];
static BY_HANDLE_FILE_INFORMATION pt_stamp;
static volatile PVOID pt_bound_socket;

static int pt_fail(int result,DWORD error)
{
    InterlockedExchange(&pt_error,(LONG)error);InterlockedExchange(&pt_result,result);
    InterlockedExchange(&pt_state,-1);return result;
}
static int pt_paths(wchar_t enable[MAX_PATH],wchar_t config[MAX_PATH],wchar_t qa[MAX_PATH])
{
    wchar_t exe[MAX_PATH];DWORD count=GetModuleFileNameW(NULL,exe,MAX_PATH);
    if(!count||count>=MAX_PATH)return 0;
    while(count&&exe[count-1]!=L'\\')--count;
    if(!count||count+40>=MAX_PATH)return 0;
    memcpy(enable,exe,count*sizeof(wchar_t));memcpy(config,exe,count*sizeof(wchar_t));
    memcpy(qa,exe,count*sizeof(wchar_t));
    lstrcpyW(enable+count,PEER_TRANSPORT184_ENABLE);lstrcpyW(config+count,PEER_TRANSPORT184_CONFIG);
    lstrcpyW(qa+count,L"mw2-trio-loopback-qa.enable");return 1;
}
/* Absent=0; ordinary=1; malformed/unreadable=-1. Any QA object is a conflict. */
static int pt_flag(const wchar_t *path,int any_presence)
{
    HANDLE f=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,
                         FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(f==INVALID_HANDLE_VALUE){
        DWORD error=GetLastError();
        return error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND?0:-1;
    }
    BY_HANDLE_FILE_INFORMATION info;
    int ok=GetFileType(f)==FILE_TYPE_DISK&&GetFileInformationByHandle(f,&info);
    if(ok&&!any_presence)ok=!(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
    CloseHandle(f);return ok?1:-1;
}
static int pt_mode(const wchar_t *enable,const wchar_t *qa)
{
    int peer=pt_flag(enable,0);if(peer<=0)return peer<0?PEER_TRANSPORT184_FILE:0;
    int conflict=pt_flag(qa,1);
    if(conflict)return PEER_TRANSPORT184_CONFLICT;return 1;
}
int peer_transport184_requested(void)
{
    wchar_t enable[MAX_PATH],config[MAX_PATH],qa[MAX_PATH];
    if(!pt_paths(enable,config,qa))return PEER_TRANSPORT184_FILE;
    return pt_mode(enable,qa);
}
static int pt_same_stamp(const BY_HANDLE_FILE_INFORMATION *a,const BY_HANDLE_FILE_INFORMATION *b)
{
    return a->dwVolumeSerialNumber==b->dwVolumeSerialNumber
        &&a->nFileIndexHigh==b->nFileIndexHigh&&a->nFileIndexLow==b->nFileIndexLow
        &&a->nFileSizeHigh==b->nFileSizeHigh&&a->nFileSizeLow==b->nFileSizeLow
        &&a->ftLastWriteTime.dwHighDateTime==b->ftLastWriteTime.dwHighDateTime
        &&a->ftLastWriteTime.dwLowDateTime==b->ftLastWriteTime.dwLowDateTime;
}
static int pt_read(uint8_t data[PEER_SCOPE184_MAX_CONFIG],DWORD *bytes,BY_HANDLE_FILE_INFORMATION *stamp)
{
    HANDLE f=CreateFileW(pt_config,GENERIC_READ|FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,
                         OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(f==INVALID_HANDLE_VALUE)return 0;
    BY_HANDLE_FILE_INFORMATION before,after;DWORD read=0,extra=0;uint8_t tail;
    int ok=GetFileType(f)==FILE_TYPE_DISK&&GetFileInformationByHandle(f,&before)
        &&!(before.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))
        &&!before.nFileSizeHigh&&before.nFileSizeLow&&before.nFileSizeLow<=PEER_SCOPE184_MAX_CONFIG;
    if(ok)ok=ReadFile(f,data,before.nFileSizeLow,&read,NULL)&&read==before.nFileSizeLow
        &&ReadFile(f,&tail,1,&extra,NULL)&&extra==0&&GetFileInformationByHandle(f,&after)
        &&pt_same_stamp(&before,&after);
    DWORD error=GetLastError();CloseHandle(f);
    if(ok){*bytes=read;*stamp=after;}else SetLastError(error);return ok;
}
static int pt_span(const void *allocation,size_t bytes,const void *p,size_t n)
{
    uintptr_t base=(uintptr_t)allocation,at=(uintptr_t)p;
    return p&&at>=base&&at-base<=bytes&&n<=bytes-(at-base);
}
static int pt_owned(const peer_scope184_config *config)
{
    InterlockedIncrement(&pt_interfaces);ULONG size=PT_ADAPTER_INITIAL;
    HANDLE heap=GetProcessHeap();if(!heap)return 0;
    for(unsigned attempt=0;attempt<3;++attempt){
        if(size<sizeof(IP_ADAPTER_ADDRESSES)||size>PT_ADAPTER_LIMIT)return 0;
        ULONG allocated=size;
        IP_ADAPTER_ADDRESSES *all=HeapAlloc(heap,0,allocated);if(!all)return 0;
        ULONG result=GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST
            |GAA_FLAG_SKIP_DNS_SERVER|GAA_FLAG_INCLUDE_PREFIX,NULL,all,&size);
        if(result==ERROR_BUFFER_OVERFLOW){HeapFree(heap,0,all);continue;}
        int found=0,malformed=result!=NO_ERROR;unsigned adapter_count=0;
        if(!malformed)for(IP_ADAPTER_ADDRESSES *a=all;a;a=a->Next){
            size_t minimum=offsetof(IP_ADAPTER_ADDRESSES,OperStatus)+sizeof(a->OperStatus);
            if(++adapter_count>128||!pt_span(all,allocated,a,minimum)||a->Length<minimum
                ||!pt_span(all,allocated,a,a->Length)){malformed=1;break;}
            if(a->OperStatus!=IfOperStatusUp)continue;
            unsigned unicast_count=0;
            for(IP_ADAPTER_UNICAST_ADDRESS *u=a->FirstUnicastAddress;u;u=u->Next){
                size_t need=offsetof(IP_ADAPTER_UNICAST_ADDRESS,OnLinkPrefixLength)+sizeof(u->OnLinkPrefixLength);
                if(++unicast_count>256||!pt_span(all,allocated,u,need)||u->Length<need
                    ||!pt_span(all,allocated,u,u->Length)){malformed=1;break;}
                if(u->DadState!=IpDadStatePreferred||!u->ValidLifetime
                    ||u->Address.iSockaddrLength<(int)sizeof(struct sockaddr_in)
                    ||!pt_span(all,allocated,u->Address.lpSockaddr,sizeof(struct sockaddr_in)))continue;
                const struct sockaddr_in *s=(const struct sockaddr_in *)u->Address.lpSockaddr;
                if(s->sin_family!=AF_INET||memcmp(&s->sin_addr,config->local.ipv4,4))continue;
                unsigned prefix=u->OnLinkPrefixLength;
                if(!prefix||prefix>32){malformed=1;break;}
                uint8_t network[4];
                for(unsigned i=0;i<4;++i){
                    unsigned bits=prefix>=8?8:prefix;
                    uint8_t mask=bits?(uint8_t)(0xffu<<(8-bits)):0;
                    network[i]=config->local.ipv4[i]&mask;prefix-=bits;
                }
                prefix=u->OnLinkPrefixLength;
                if(!peer_scope184_prefix_host(config->local.ipv4,network,prefix)){malformed=1;break;}
                /* An on-link peer cannot be this interface's network/broadcast.
                 * Remote-subnet prefix ownership belongs to the other endpoint. */
                uint8_t peer_network[4];unsigned remaining=prefix;
                for(unsigned i=0;i<4;++i){
                    unsigned bits=remaining>=8?8:remaining;
                    uint8_t mask=bits?(uint8_t)(0xffu<<(8-bits)):0;
                    peer_network[i]=config->peer.ipv4[i]&mask;remaining-=bits;
                }
                if(!memcmp(peer_network,network,4)
                    &&!peer_scope184_prefix_host(config->peer.ipv4,network,prefix)){malformed=1;break;}
                found=1;
            }
            if(malformed)break;
        }
        HeapFree(heap,0,all);return found&&!malformed;
    }
    return 0;
}
static int pt_snapshot(peer_scope184_config *out)
{
    if(InterlockedCompareExchange(&pt_state,0,0)!=1)return PEER_TRANSPORT184_NOT_READY;
    peer_scope184_context copy=pt_scope;
    if(!peer_scope184_matches(&copy,PEER_SCOPE184_SEND,&copy.config.local,&copy.config.peer))
        return pt_fail(PEER_TRANSPORT184_CONTEXT,0);
    if(InterlockedCompareExchange(&pt_state,0,0)!=1)return PEER_TRANSPORT184_NOT_READY;
    *out=copy.config;return 1;
}
int peer_transport184_initialize(DWORD owner_tid)
{
    if(!owner_tid||GetCurrentThreadId()!=owner_tid)return PEER_TRANSPORT184_THREAD;
    if(InterlockedCompareExchange(&pt_attempted,1,0))return pt_fail(PEER_TRANSPORT184_ONCE,0);
    pt_owner=owner_tid;
    if(!pt_paths(pt_enable,pt_config,pt_qa))return pt_fail(PEER_TRANSPORT184_FILE,GetLastError());
    int mode=pt_mode(pt_enable,pt_qa);
    if(mode==0){InterlockedExchange(&pt_result,0);InterlockedExchange(&pt_state,-2);return 0;}
    if(mode<0)return pt_fail(mode,GetLastError());
    uint8_t data[PEER_SCOPE184_MAX_CONFIG];DWORD bytes;
    if(!pt_read(data,&bytes,&pt_stamp))return pt_fail(PEER_TRANSPORT184_FILE,GetLastError());
    peer_scope184_context candidate={0};
    if(peer_scope184_latch(&candidate,data,bytes)!=1)return pt_fail(PEER_TRANSPORT184_CONFIG_INVALID,0);
    if(!pt_owned(&candidate.config))return pt_fail(PEER_TRANSPORT184_INTERFACE,0);
    /* Recheck the mutable inputs after interface enumeration before publishing. */
    BY_HANDLE_FILE_INFORMATION stamp;
    if(pt_mode(pt_enable,pt_qa)!=1||!pt_read(data,&bytes,&stamp)||!pt_same_stamp(&pt_stamp,&stamp)
        ||peer_scope184_observe(&candidate,data,bytes)!=1)return pt_fail(PEER_TRANSPORT184_SCOPE_CHANGED,0);
    pt_scope=candidate;InterlockedExchange(&pt_result,1);InterlockedExchange(&pt_state,1);return 1;
}
int peer_transport184_ready(void)
{return InterlockedCompareExchange(&pt_state,0,0)==1;}
int peer_transport184_observe(void)
{
    if(!peer_transport184_ready())return PEER_TRANSPORT184_NOT_READY;
    if(GetCurrentThreadId()!=pt_owner)return pt_fail(PEER_TRANSPORT184_THREAD,0);
    InterlockedIncrement(&pt_observations);uint8_t data[PEER_SCOPE184_MAX_CONFIG];DWORD bytes;
    BY_HANDLE_FILE_INFORMATION stamp;peer_scope184_context copy=pt_scope;
    if(pt_mode(pt_enable,pt_qa)!=1||!pt_read(data,&bytes,&stamp)||!pt_same_stamp(&pt_stamp,&stamp)
        ||peer_scope184_observe(&copy,data,bytes)!=1)return pt_fail(PEER_TRANSPORT184_SCOPE_CHANGED,GetLastError());
    if(!pt_owned(&copy.config))return pt_fail(PEER_TRANSPORT184_INTERFACE,0);
    return 1;
}
int peer_transport184_get_config(peer_scope184_config *out)
{if(!out)return PEER_TRANSPORT184_ARGUMENT;return pt_snapshot(out);}
int peer_transport184_identity_low(uint32_t *out)
{
    if(!out)return PEER_TRANSPORT184_ARGUMENT;
    peer_scope184_config config;int result=pt_snapshot(&config);if(result!=1)return result;
    uint64_t value;if(peer_scope184_derive(&config,0,&value)!=1)return pt_fail(PEER_TRANSPORT184_CONTEXT,0);
    *out=(uint32_t)value;return 1;
}
int peer_transport184_socket_matches(SOCKET s)
{
    peer_scope184_config config;int result=pt_snapshot(&config);if(result!=1)return result;
    InterlockedIncrement(&pt_sockets);
    if(!s||s==INVALID_SOCKET)return pt_fail(PEER_TRANSPORT184_BINDING,0);
    struct sockaddr_in address;int bytes=sizeof(address);memset(&address,0,sizeof(address));
    if(getsockname(s,(struct sockaddr *)&address,&bytes)==SOCKET_ERROR||bytes!=(int)sizeof(address)
        ||address.sin_family!=AF_INET||ntohs(address.sin_port)!=config.local.port
        ||memcmp(&address.sin_addr,config.local.ipv4,4))return pt_fail(PEER_TRANSPORT184_BINDING,(DWORD)WSAGetLastError());
    PVOID observed=InterlockedCompareExchangePointer(&pt_bound_socket,(PVOID)(uintptr_t)s,NULL);
    if(observed&&observed!=(PVOID)(uintptr_t)s)return pt_fail(PEER_TRANSPORT184_BINDING,0);
    return peer_transport184_ready()?1:PEER_TRANSPORT184_NOT_READY;
}
static int pt_endpoint(const struct sockaddr *s,int bytes,peer_scope184_endpoint *out)
{
    if(!s||bytes<(int)sizeof(struct sockaddr_in)||s->sa_family!=AF_INET)return 0;
    const struct sockaddr_in *v=(const struct sockaddr_in *)s;
    memcpy(out->ipv4,&v->sin_addr,4);out->port=ntohs(v->sin_port);return 1;
}
int peer_transport184_tuple_matches(unsigned direction,const struct sockaddr *source,int source_len,
                                   const struct sockaddr *destination,int destination_len)
{
    if(!peer_transport184_ready())return 0;
    peer_scope184_context copy=pt_scope;peer_scope184_endpoint a,b;
    if(!pt_endpoint(source,source_len,&a)||!pt_endpoint(destination,destination_len,&b))return 0;
    int matched=peer_scope184_matches(&copy,direction,&a,&b);
    if(copy.last_result<0){pt_fail(PEER_TRANSPORT184_CONTEXT,0);return 0;}
    if(!matched)return 0;
    return peer_transport184_ready();
}
void peer_transport184_get_report(peer_transport184_report *out)
{
    if(!out)return;memset(out,0,sizeof(*out));out->version=1;out->bytes=sizeof(*out);
    LONG state=InterlockedCompareExchange(&pt_state,0,0);
    out->attempted=(uint32_t)InterlockedCompareExchange(&pt_attempted,0,0);out->ready=state==1;out->failed=state==-1;
    out->result=(int32_t)InterlockedCompareExchange(&pt_result,0,0);
    out->observations=(uint32_t)InterlockedCompareExchange(&pt_observations,0,0);
    out->interface_checks=(uint32_t)InterlockedCompareExchange(&pt_interfaces,0,0);
    out->socket_checks=(uint32_t)InterlockedCompareExchange(&pt_sockets,0,0);
    out->win32_error=(uint32_t)InterlockedCompareExchange(&pt_error,0,0);
    if(state==1)out->role=pt_scope.config.role;
}
