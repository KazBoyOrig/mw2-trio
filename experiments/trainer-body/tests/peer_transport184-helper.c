/* Own MIT mock fixture. No real files, interface enumeration or socket calls. */
#include "peer_transport184.h"
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"failure line %u\n",(unsigned)__LINE__);exit(1);}}while(0)
static const char fixture[]="version=1\nrole=host\nsession_nonce=00112233445566778899aabbccddeeff\nlocal_ip=26.1.2.3\nlocal_port=28960\npeer_ip=26.4.5.6\npeer_port=28961\n";
static int have_enable,have_qa,have_config,config_type,interface_case,grow,large_path;
static DWORD last_error,thread_id,enable_attrs,config_attrs,qa_attrs,stamp_index,stamp_time;
static unsigned opens,closes,allocations,frees,adapter_calls;
static ULONG adapter_required;
static char config_data[256];static size_t config_bytes,position;
static uint8_t bound_ip[4];static unsigned bound_port;static int bound_family,bound_error;

static DWORD WINAPI fake_module(HMODULE m,LPWSTR out,DWORD size)
{(void)m;if(large_path)return size;const wchar_t *p=L"C:\\fixture\\iw4sp.exe";CHECK(wcslen(p)+1<size);wcscpy(out,p);return (DWORD)wcslen(p);}
static DWORD WINAPI fake_thread(void){return thread_id;}
static DWORD WINAPI fake_error(void){return last_error;}
static void WINAPI fake_set_error(DWORD error){last_error=error;}
static HANDLE WINAPI fake_open(LPCWSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,DWORD creation,DWORD flags,HANDLE template_file)
{
    (void)access;(void)security;(void)template_file;
    CHECK(share==FILE_SHARE_READ&&creation==OPEN_EXISTING&&flags==FILE_FLAG_OPEN_REPARSE_POINT);
    uintptr_t id=0;
    if(wcsstr(path,L"mw2-trio-peer-test.enable")&&have_enable)id=3;
    else if(wcsstr(path,L"mw2-trio-loopback-qa.enable")&&have_qa)id=4;
    else if(wcsstr(path,L"mw2-trio-peer.cfg")&&have_config){id=5;position=0;}
    if(!id){last_error=ERROR_FILE_NOT_FOUND;return INVALID_HANDLE_VALUE;}
    ++opens;return (HANDLE)id;
}
static DWORD WINAPI fake_type(HANDLE f){return (uintptr_t)f==5?(DWORD)config_type:FILE_TYPE_DISK;}
static BOOL WINAPI fake_info(HANDLE f,LPBY_HANDLE_FILE_INFORMATION info)
{
    memset(info,0,sizeof(*info));uintptr_t id=(uintptr_t)f;
    CHECK(id>=3&&id<=5);info->dwFileAttributes=id==3?enable_attrs:(id==4?qa_attrs:config_attrs);
    info->dwVolumeSerialNumber=77;info->nFileIndexLow=id==5?stamp_index:(DWORD)id;
    info->ftLastWriteTime.dwLowDateTime=id==5?stamp_time:0;
    if(id==5)info->nFileSizeLow=(DWORD)config_bytes;return TRUE;
}
static BOOL WINAPI fake_close(HANDLE f){CHECK((uintptr_t)f>=3&&(uintptr_t)f<=5);++closes;return TRUE;}
static BOOL WINAPI fake_read(HANDLE f,LPVOID data,DWORD bytes,LPDWORD read,LPOVERLAPPED overlapped)
{
    (void)overlapped;CHECK((uintptr_t)f==5);size_t remaining=config_bytes-position;
    size_t take=bytes<remaining?bytes:remaining;memcpy(data,config_data+position,take);position+=take;*read=(DWORD)take;
    if(!take&&grow&&bytes){*(char *)data='x';*read=1;}return TRUE;
}
static HANDLE WINAPI fake_heap(void){return (HANDLE)(uintptr_t)77;}
static LPVOID WINAPI fake_alloc(HANDLE h,DWORD flags,SIZE_T bytes)
{CHECK((uintptr_t)h==77&&!flags&&bytes<=1024u*1024u);++allocations;return calloc(1,bytes);}
static BOOL WINAPI fake_free(HANDLE h,DWORD flags,LPVOID p)
{CHECK((uintptr_t)h==77&&!flags&&p);++frees;free(p);return TRUE;}
static ULONG WINAPI fake_adapters(ULONG family,ULONG flags,PVOID reserved,PIP_ADAPTER_ADDRESSES a,PULONG bytes)
{
    CHECK(family==AF_INET&&!reserved&&flags==(GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER|GAA_FLAG_INCLUDE_PREFIX));
    ++adapter_calls;
    if(interface_case==1)return ERROR_NO_DATA;
    if(interface_case==2){*bytes=1024u*1024u+1;return ERROR_BUFFER_OVERFLOW;}
    if(interface_case==3)return ERROR_BUFFER_OVERFLOW;
    if(*bytes<adapter_required){*bytes=adapter_required;return ERROR_BUFFER_OVERFLOW;}
    size_t offset=(sizeof(*a)+15u)&~15u;
    IP_ADAPTER_UNICAST_ADDRESS *u=(void *)((uint8_t *)a+offset);
    struct sockaddr_in *s=(void *)((uint8_t *)u+((sizeof(*u)+15u)&~15u));
    CHECK((uintptr_t)(s+1)<=(uintptr_t)a+*bytes);
    a->Length=sizeof(*a);a->OperStatus=interface_case==4?IfOperStatusDown:IfOperStatusUp;a->FirstUnicastAddress=u;
    u->Length=sizeof(*u);u->DadState=interface_case==5?IpDadStateTentative:IpDadStatePreferred;
    u->ValidLifetime=interface_case==6?0:100;u->OnLinkPrefixLength=24;
    s->sin_family=AF_INET;const uint8_t ip[4]={26,1,2,3};memcpy(&s->sin_addr,ip,4);
    u->Address.lpSockaddr=(struct sockaddr *)s;u->Address.iSockaddrLength=sizeof(*s);
    if(interface_case==7)((uint8_t *)&s->sin_addr)[3]=4;
    if(interface_case==8)u->OnLinkPrefixLength=0;
    if(interface_case==9)u->OnLinkPrefixLength=33;
    if(interface_case==10)a->Next=a;
    if(interface_case==11)u->Next=u;
    if(interface_case==12)a->Next=(void *)((uint8_t *)a+*bytes);
    if(interface_case==13)u->Address.lpSockaddr=(void *)((uint8_t *)a+*bytes);
    if(interface_case==14)s->sin_family=AF_INET6;
    if(interface_case==15)u->OnLinkPrefixLength=25;
    return NO_ERROR;
}
static int WSAAPI fake_sockname(SOCKET s,struct sockaddr *address,int *bytes)
{
    CHECK(s&&s!=INVALID_SOCKET&&*bytes==(int)sizeof(struct sockaddr_in));
    if(bound_error){last_error=WSAENOTSOCK;return SOCKET_ERROR;}
    struct sockaddr_in *a=(struct sockaddr_in *)address;memset(a,0,sizeof(*a));
    a->sin_family=(ADDRESS_FAMILY)bound_family;a->sin_port=htons((u_short)bound_port);memcpy(&a->sin_addr,bound_ip,4);
    *bytes=sizeof(*a);return 0;
}
static int WSAAPI fake_wsa_error(void){return (int)last_error;}

#define GetModuleFileNameW fake_module
#define GetCurrentThreadId fake_thread
#define GetLastError fake_error
#define SetLastError fake_set_error
#define CreateFileW fake_open
#define GetFileType fake_type
#define GetFileInformationByHandle fake_info
#define CloseHandle fake_close
#define ReadFile fake_read
#define GetProcessHeap fake_heap
#define HeapAlloc fake_alloc
#define HeapFree fake_free
#define GetAdaptersAddresses fake_adapters
#define getsockname fake_sockname
#define WSAGetLastError fake_wsa_error
#include "peer_transport184.c"

static void reset_fixture(void)
{
    pt_attempted=pt_state=pt_result=pt_error=pt_observations=pt_interfaces=pt_sockets=0;
    pt_owner=0;pt_bound_socket=NULL;memset(&pt_scope,0,sizeof(pt_scope));memset(&pt_stamp,0,sizeof(pt_stamp));
    memset(pt_enable,0,sizeof(pt_enable));memset(pt_config,0,sizeof(pt_config));memset(pt_qa,0,sizeof(pt_qa));
    have_enable=have_config=1;have_qa=0;config_type=FILE_TYPE_DISK;interface_case=grow=large_path=0;
    last_error=0;thread_id=42;enable_attrs=config_attrs=qa_attrs=FILE_ATTRIBUTE_NORMAL;stamp_index=8;stamp_time=9;
    opens=closes=allocations=frees=adapter_calls=0;adapter_required=1024;
    memcpy(config_data,fixture,sizeof(fixture));config_bytes=strlen(fixture);position=0;
    const uint8_t ip[4]={26,1,2,3};memcpy(bound_ip,ip,4);bound_port=28960;bound_family=AF_INET;bound_error=0;
}
static struct sockaddr_in endpoint(const uint8_t ip[4],unsigned port)
{struct sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons((u_short)port);memcpy(&a.sin_addr,ip,4);return a;}
static void balanced(void){CHECK(opens==closes&&allocations==frees);}
int main(void)
{
    reset_fixture();CHECK(peer_transport184_requested()==1);CHECK(peer_transport184_initialize(42)==1);
    CHECK(peer_transport184_ready());balanced();CHECK(peer_transport184_observe()==1);
    peer_scope184_config config;CHECK(peer_transport184_get_config(&config)==1&&config.role==PEER_SCOPE184_HOST);
    uint32_t low;CHECK(peer_transport184_identity_low(&low)==1&&low==0x166f079cu);
    CHECK(peer_transport184_get_config(NULL)<0&&peer_transport184_identity_low(NULL)<0);
    CHECK(peer_transport184_socket_matches(42)==1&&peer_transport184_socket_matches(42)==1);
    struct sockaddr_in a=endpoint(config.local.ipv4,config.local.port),b=endpoint(config.peer.ipv4,config.peer.port);
    CHECK(peer_transport184_tuple_matches(PEER_SCOPE184_SEND,(void *)&a,sizeof(a),(void *)&b,sizeof(b)));
    CHECK(peer_transport184_tuple_matches(PEER_SCOPE184_RECEIVE,(void *)&b,sizeof(b),(void *)&a,sizeof(a)));
    CHECK(!peer_transport184_tuple_matches(PEER_SCOPE184_SEND,(void *)&b,sizeof(b),(void *)&a,sizeof(a)));
    b.sin_port=htons(28962);CHECK(!peer_transport184_tuple_matches(PEER_SCOPE184_SEND,(void *)&a,sizeof(a),(void *)&b,sizeof(b)));
    CHECK(!peer_transport184_tuple_matches(PEER_SCOPE184_SEND,NULL,sizeof(a),(void *)&b,sizeof(b)));
    CHECK(!peer_transport184_tuple_matches(PEER_SCOPE184_SEND,(void *)&a,1,(void *)&b,sizeof(b)));
    peer_transport184_report report;peer_transport184_get_report(&report);
    CHECK(report.version==1&&report.bytes==sizeof(report)&&report.ready&&report.role==1&&report.observations==1);
    CHECK(peer_transport184_initialize(42)==PEER_TRANSPORT184_ONCE&&!peer_transport184_ready());balanced();
    reset_fixture();have_enable=0;CHECK(!peer_transport184_requested()&&peer_transport184_initialize(42)==0);
    have_enable=1;CHECK(peer_transport184_initialize(42)==PEER_TRANSPORT184_ONCE);
    reset_fixture();have_qa=1;CHECK(peer_transport184_requested()==PEER_TRANSPORT184_CONFLICT);
    CHECK(peer_transport184_initialize(42)==PEER_TRANSPORT184_CONFLICT);balanced();
    reset_fixture();enable_attrs=FILE_ATTRIBUTE_REPARSE_POINT;CHECK(peer_transport184_requested()==PEER_TRANSPORT184_FILE);
    reset_fixture();config_attrs=FILE_ATTRIBUTE_DIRECTORY;CHECK(peer_transport184_initialize(42)==PEER_TRANSPORT184_FILE);balanced();
    reset_fixture();config_type=FILE_TYPE_PIPE;CHECK(peer_transport184_initialize(42)==PEER_TRANSPORT184_FILE);balanced();
    reset_fixture();have_config=0;CHECK(peer_transport184_initialize(42)==PEER_TRANSPORT184_FILE);balanced();
    reset_fixture();config_bytes=0;CHECK(peer_transport184_initialize(42)==PEER_TRANSPORT184_FILE);balanced();
    reset_fixture();grow=1;CHECK(peer_transport184_initialize(42)==PEER_TRANSPORT184_FILE);balanced();
    reset_fixture();config_data[0]='x';CHECK(peer_transport184_initialize(42)==PEER_TRANSPORT184_CONFIG_INVALID);balanced();
    reset_fixture();large_path=1;CHECK(peer_transport184_requested()==PEER_TRANSPORT184_FILE);
    CHECK(peer_transport184_initialize(42)==PEER_TRANSPORT184_FILE);
    for(int scenario=1;scenario<=14;++scenario){
        reset_fixture();interface_case=scenario;CHECK(peer_transport184_initialize(42)==PEER_TRANSPORT184_INTERFACE);
        CHECK(!peer_transport184_ready());balanced();if(scenario==3)CHECK(adapter_calls==3);
    }
    reset_fixture();adapter_required=17000;CHECK(peer_transport184_initialize(42)==1&&adapter_calls==2);balanced();
    reset_fixture();interface_case=15;
    snprintf(config_data,sizeof(config_data),"version=1\nrole=host\nsession_nonce=00112233445566778899aabbccddeeff\nlocal_ip=26.1.2.3\nlocal_port=28960\npeer_ip=26.1.2.127\npeer_port=28961\n");
    config_bytes=strlen(config_data);CHECK(peer_transport184_initialize(42)==PEER_TRANSPORT184_INTERFACE);balanced();
    for(unsigned scenario=0;scenario<8;++scenario){
        reset_fixture();CHECK(peer_transport184_initialize(42)==1);
        switch(scenario){case 0:have_enable=0;break;case 1:have_qa=1;break;case 2:config_data[0]^=1;break;
        case 3:stamp_index++;break;case 4:stamp_time++;break;case 5:interface_case=7;break;
        case 6:thread_id=43;break;case 7:pt_scope.seal[0]^=1;break;}
        CHECK(peer_transport184_observe()<0&&!peer_transport184_ready());
        CHECK(peer_transport184_observe()<0&&peer_transport184_get_config(&config)<0);balanced();
    }
    for(unsigned scenario=0;scenario<5;++scenario){
        reset_fixture();CHECK(peer_transport184_initialize(42)==1);
        switch(scenario){case 0:bound_port++;break;case 1:bound_ip[3]++;break;case 2:bound_family=AF_INET6;break;
        case 3:bound_error=1;break;case 4:CHECK(peer_transport184_socket_matches(42)==1);break;}
        CHECK(peer_transport184_socket_matches(scenario==4?43:42)<0&&!peer_transport184_ready());balanced();
    }
    reset_fixture();CHECK(peer_transport184_initialize(42)==1);CHECK(peer_transport184_socket_matches(INVALID_SOCKET)<0);
    reset_fixture();CHECK(peer_transport184_initialize(42)==1);CHECK(peer_transport184_socket_matches(0)<0);
    reset_fixture();CHECK(peer_transport184_initialize(43)==PEER_TRANSPORT184_THREAD&&!pt_attempted);
    reset_fixture();CHECK(peer_transport184_initialize(42)==1);pt_scope.config.nonce[0]^=1;
    a=endpoint((const uint8_t[]){26,1,2,3},28960);b=endpoint((const uint8_t[]){26,4,5,6},28961);
    CHECK(!peer_transport184_tuple_matches(PEER_SCOPE184_SEND,(void *)&a,sizeof(a),(void *)&b,sizeof(b)));
    CHECK(!peer_transport184_ready());balanced();
    printf("peer_transport184: %u mock checks passed; no real files/interfaces/sockets/game/provider calls\n",checks);return 0;
}
