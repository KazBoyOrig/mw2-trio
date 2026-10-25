/* Own code; MIT. Bounded stock co-op packet diagnostics, no protocol changes. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bootstrap.h"
#include "pure_sha256.h"
#include "net184.h"
#include "reliable184.h"
#include "service184.h"
#include "backend184.h"
#include "peer_transport184.h"

typedef int (WSAAPI *send_fn)(SOCKET, const char *, int, int, const struct sockaddr *, int);
typedef int (WSAAPI *recv_fn)(SOCKET, char *, int, int, struct sockaddr *, int *);
static send_fn original_send;
static recv_fn original_recv;
static wchar_t flag_path[MAX_PATH], log_path[MAX_PATH], qa_path[MAX_PATH], control_path[MAX_PATH];
static volatile LONG budget = 256;
static volatile LONG bandwidth_budget = 4;
static int state;
static int loopback_qa;
static int direct_peer;
static int peer_install_requested;
static volatile LONG qa_boundary_failed;
static IMAGE_THUNK_DATA32 *installed_slots[2];
static volatile LONG io_send_calls,io_send_oob,io_send_errors,io_recv_calls,io_received,io_recv_oob;
static volatile LONG io_send_error,io_recv_error;
static volatile LONG qa_refused_external_ip;
static volatile LONG qa_refused_backend;
static int image_span(uintptr_t address,SIZE_T bytes,int executable);

static service184_kind external_ip_request(SOCKET s,const char *buf,int len,
    const struct sockaddr *address,int address_len)
{
    if(len<0||!address||address_len<(int)sizeof(struct sockaddr_in)||address->sa_family!=AF_INET
        ||!image_span(0x1a00b50u,4,0)||!image_span(0x1be7490u,8,0))return SERVICE184_UNKNOWN;
    uint32_t socket,kind;unsigned char ip[4];
    memcpy(&socket,(void *)0x1a00b50u,4);memcpy(&kind,(void *)0x1be7490u,4);
    memcpy(ip,(void *)0x1be7494u,4);
    const struct sockaddr_in *a=(const struct sockaddr_in *)address;
    service184_endpoint destination={(uint32_t)a->sin_family,{0},ntohs(a->sin_port)};
    memcpy(destination.ipv4,&a->sin_addr,4);
    return service184_classify_blocked(buf,(size_t)len,(uint32_t)s,socket,kind,ip,&destination);
}

static const char *command_word(const char *buf,int len)
{
    if(!buf||len<5||memcmp(buf,"\xff\xff\xff\xff",4))return NULL;
    static const char *const words[]={"coopConnect","coopAccept","coopAcceptAck","coopLaunch",
        "coopLoaded","coopRestart","coopRestartAck","hb","hba","disLobby","bw","R","RA"};
    for(unsigned i=0;i<sizeof(words)/sizeof(words[0]);++i){
        size_t n=strlen(words[i]);
        if((size_t)len>=n+5&&!memcmp(buf+4,words[i],n)&&(unsigned char)buf[4+n]<=32)return words[i];
    }
    return NULL;
}

static backend184_kind backend_request(uintptr_t caller,SOCKET s,const char *buf,int len,
    const struct sockaddr *address,int address_len)
{
    if(caller!=BACKEND184_RETURN_VA||len<=0||!buf||!address
        ||address_len<(int)sizeof(struct sockaddr_in)||address->sa_family!=AF_INET
        ||!image_span(0x19ff94cu,4,0)||!image_span(0x1be8f38u,10,0)
        ||!image_span(BACKEND184_SENDER_VA,BACKEND184_SENDER_BYTES,1))return BACKEND184_UNKNOWN;
    backend184_context context;memset(&context,0,sizeof(context));
    context.native_return_address=caller;context.packet_bytes=(size_t)len;context.sending_socket=(uint32_t)s;
    memcpy(&context.native_backend_socket,(void *)0x19ff94cu,4);
    memcpy(&context.native_address_type,(void *)0x1be8f38u,4);
    memcpy(context.native_ipv4,(void *)0x1be8f3cu,4);
    uint16_t wire_port;memcpy(&wire_port,(void *)0x1be8f40u,2);
    context.native_port_host_order=ntohs(wire_port);
    context.known_coop_token=command_word(buf,len)!=NULL;
    char digest[65];pure_sha256_data((void *)BACKEND184_SENDER_VA,BACKEND184_SENDER_BYTES,digest);
    context.function_sha256_verified=!strcmp(digest,BACKEND184_SENDER_SHA256);
    const struct sockaddr_in *a=(const struct sockaddr_in *)address;
    service184_endpoint destination={(uint32_t)a->sin_family,{0},ntohs(a->sin_port)};
    memcpy(destination.ipv4,&a->sin_addr,4);
    return backend184_classify_blocked(&context,&destination);
}

static int regular_file(const wchar_t *path)
{
    HANDLE h = CreateFileW(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    BY_HANDLE_FILE_INFORMATION i;
    int ok = GetFileInformationByHandle(h, &i)
        && !(i.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
    CloseHandle(h);
    return ok;
}

static void append(const char *line)
{
    HANDLE h = CreateFileW(log_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(h, line, (DWORD)strlen(line), &written, NULL);
        CloseHandle(h);
    }
}

static void qa_failure(LONG reason,const struct sockaddr *address,int address_len)
{
    if(InterlockedCompareExchange(&qa_boundary_failed,reason,0))return;
    unsigned char ip[4]={0};unsigned port=0;
    if(address&&address_len>=(int)sizeof(struct sockaddr_in)&&address->sa_family==AF_INET){
        const struct sockaddr_in *a=(const struct sockaddr_in *)address;
        memcpy(ip,&a->sin_addr,4);port=ntohs(a->sin_port);
    }
    char line[160];
    snprintf(line,sizeof(line),"QA boundary failure reason=%ld address_bytes=%d peer=%u.%u.%u.%u:%u\r\n",
        (long)reason,address_len,ip[0],ip[1],ip[2],ip[3],port);
    append(line);
}

static void refuse_qa_send(service184_kind kind,const struct sockaddr *address,int address_len)
{
    if(kind==SERVICE184_EXTERNAL_IP_TYPE1){
        LONG count=InterlockedIncrement(&qa_refused_external_ip);
        if(count==1)append("QA external-IP request refused before socket callee; no payload forwarded\r\n");
    }else qa_failure(1,address,address_len);
}

static void packet(const char *direction, const char *buf, int len, int result,
                   const struct sockaddr *address, int address_len)
{
    /* Record the command token only, never raw payloads, IDs or service packets. */
    const char *word = command_word(buf,len);
    if(word&&!strcmp(word,"bw")&&InterlockedDecrement(&bandwidth_budget)<0)return;
    if (!word || InterlockedDecrement(&budget) < 0) return;
    unsigned char ip[4] = {0}; unsigned port = 0;
    if (address && address_len >= (int)sizeof(struct sockaddr_in) && address->sa_family == AF_INET) {
        const struct sockaddr_in *a = (const struct sockaddr_in *)address;
        memcpy(ip, &a->sin_addr, sizeof(ip));
        const unsigned char *p = (const unsigned char *)&a->sin_port;
        port = (unsigned)p[0] * 256u + p[1];
    }
    char line[256];
    reliable184_token token=reliable184_decode(buf,(size_t)len);
    snprintf(line, sizeof(line), "tick=%lu tid=%lu %s command=%s bytes=%d result=%d peer=%u.%u.%u.%u:%u inner=%s reason=%s\r\n",
             (unsigned long)GetTickCount(), (unsigned long)GetCurrentThreadId(), direction,
             word, len, result, ip[0], ip[1], ip[2], ip[3], port,
             reliable184_command_name(token.first_command),reliable184_reason_name(token.rejection));
    append(line);
}

static __declspec(noinline) int WSAAPI traced_send(SOCKET s, const char *buf, int len, int flags,
                             const struct sockaddr *address, int address_len)
{
    uintptr_t native_caller=(uintptr_t)__builtin_return_address(0);
    if(direct_peer){
        /* Known service provenance is denied before any peer tuple allowance,
         * including a service endpoint numerically equal to the chosen peer. */
        if(backend_request(native_caller,s,buf,len,address,address_len)==BACKEND184_IWNET_3005){
            InterlockedIncrement(&qa_refused_backend);
            WSASetLastError(WSAEACCES);return SOCKET_ERROR;
        }
        if(external_ip_request(s,buf,len,address,address_len)==SERVICE184_EXTERNAL_IP_TYPE1){
            InterlockedIncrement(&qa_refused_external_ip);
            WSASetLastError(WSAEACCES);return SOCKET_ERROR;
        }
        uint32_t native_socket=0;struct sockaddr_in local;int local_bytes=sizeof(local);
        memset(&local,0,sizeof(local));
        if(InterlockedCompareExchange(&qa_boundary_failed,0,0)||!peer_transport184_ready()
            ||!image_span(0x1a00b50u,4,0)
            ||(memcpy(&native_socket,(void *)0x1a00b50u,4),s!=(SOCKET)native_socket)
            ||peer_transport184_socket_matches(s)!=1
            ||getsockname(s,(struct sockaddr *)&local,&local_bytes)==SOCKET_ERROR
            ||peer_transport184_tuple_matches(PEER_SCOPE184_SEND,(struct sockaddr *)&local,local_bytes,address,address_len)!=1){
            qa_failure(6,address,address_len);WSASetLastError(WSAEACCES);return SOCKET_ERROR;
        }
    }
    /* QA alias traffic is confined even if the native engine changes its peer.
     * Latch until process exit; deleting the opt-in cannot relax this boundary. */
    if (loopback_qa && (!address || address_len < (int)sizeof(struct sockaddr_in)
        || address->sa_family != AF_INET
        || ((const struct sockaddr_in *)address)->sin_addr.s_addr != htonl(MW2TRIO_QA_IPV4_HOST))) {
        /* Positive classification only adjusts abort policy. Every outside
         * attempt still returns access denied before original_send. */
        if(backend_request(native_caller,s,buf,len,address,address_len)==BACKEND184_IWNET_3005){
            LONG count=InterlockedIncrement(&qa_refused_backend);
            if(count==1)append("QA native LSP backend send refused before socket callee; no payload forwarded\r\n");
        }else {
            refuse_qa_send(external_ip_request(s,buf,len,address,address_len),address,address_len);
            if(InterlockedCompareExchange(&qa_boundary_failed,0,0)){
                char line[128];snprintf(line,sizeof(line),"QA refused unknown outbound bytes=%d caller=0x%08lx\r\n",
                    len,(unsigned long)native_caller);append(line);
            }
        }
        WSASetLastError(WSAEACCES); return SOCKET_ERROR;
    }
    InterlockedIncrement(&io_send_calls);
    if(buf&&len>=4&&!memcmp(buf,"\xff\xff\xff\xff",4))InterlockedIncrement(&io_send_oob);
    int result = original_send(s, buf, len, flags, address, address_len);
    int error = WSAGetLastError();
    if(result==SOCKET_ERROR){InterlockedIncrement(&io_send_errors);InterlockedExchange(&io_send_error,error);}
    packet("send", buf, len, result, address, address_len);
    WSASetLastError(error);
    return result;
}

static int WSAAPI traced_recv(SOCKET s, char *buf, int len, int flags,
                             struct sockaddr *address, int *address_len)
{
    struct sockaddr_in local;int local_bytes=sizeof(local);
    if(direct_peer){
        uint32_t native_socket=0,backend_socket=0;
        /* The separately identified native service socket never receives a
         * packet through this mode. A refused send is never reported success. */
        if(image_span(0x19ff94cu,4,0))memcpy(&backend_socket,(void *)0x19ff94cu,4);
        if(backend_socket&&s==(SOCKET)backend_socket){WSASetLastError(WSAEWOULDBLOCK);return SOCKET_ERROR;}
        memset(&local,0,sizeof(local));
        if(InterlockedCompareExchange(&qa_boundary_failed,0,0)||!peer_transport184_ready()
            ||!address||!address_len||*address_len<(int)sizeof(struct sockaddr_in)
            ||!image_span(0x1a00b50u,4,0)
            ||(memcpy(&native_socket,(void *)0x1a00b50u,4),s!=(SOCKET)native_socket)
            ||peer_transport184_socket_matches(s)!=1
            ||getsockname(s,(struct sockaddr *)&local,&local_bytes)==SOCKET_ERROR){
            qa_failure(7,NULL,0);WSASetLastError(WSAEACCES);return SOCKET_ERROR;
        }
    }
    if (loopback_qa && (!address || !address_len || *address_len < (int)sizeof(struct sockaddr_in))) {
        qa_failure(2,address,address_len?*address_len:0);
        WSASetLastError(WSAEACCES); return SOCKET_ERROR;
    }
    InterlockedIncrement(&io_recv_calls);
    int result = original_recv(s, buf, len, flags, address, address_len);
    int error = WSAGetLastError();
    if(result==SOCKET_ERROR)InterlockedExchange(&io_recv_error,error);
    if(result>=0&&direct_peer
        &&peer_transport184_tuple_matches(PEER_SCOPE184_RECEIVE,address,*address_len,(struct sockaddr *)&local,local_bytes)!=1){
        qa_failure(8,address,*address_len);WSASetLastError(WSAEACCES);return SOCKET_ERROR;
    }
    if (result >= 0 && loopback_qa) {
        if (*address_len < (int)sizeof(struct sockaddr_in)
            || address->sa_family != AF_INET
            || ((const struct sockaddr_in *)address)->sin_addr.s_addr != htonl(MW2TRIO_QA_IPV4_HOST)) {
            qa_failure(3,address,*address_len);
            WSASetLastError(WSAEACCES); return SOCKET_ERROR;
        }
    }
    if (result > 0) {
        InterlockedIncrement(&io_received);
        if(result>=4&&!memcmp(buf,"\xff\xff\xff\xff",4))InterlockedIncrement(&io_recv_oob);
        packet("recv", buf, result, result, address, address_len ? *address_len : 0);
    }
    WSASetLastError(error);
    return result;
}

int net184_loopback_ready(void)
{
    return state == 1 && loopback_qa && !InterlockedCompareExchange(&qa_boundary_failed, 0, 0)
        && installed_slots[0] && installed_slots[1]
        && installed_slots[0]->u1.Function == (uintptr_t)traced_send
        && installed_slots[1]->u1.Function == (uintptr_t)traced_recv;
}

int net184_loopback_requested(void)
{
    return loopback_qa || (qa_path[0] && regular_file(qa_path));
}
int net184_peer_requested(void)
{return direct_peer||peer_transport184_requested()!=0;}
int net184_peer_ready(void)
{
    return state==1&&direct_peer&&!InterlockedCompareExchange(&qa_boundary_failed,0,0)
        &&peer_transport184_ready()&&installed_slots[0]&&installed_slots[1]
        &&installed_slots[0]->u1.Function==(uintptr_t)traced_send
        &&installed_slots[1]->u1.Function==(uintptr_t)traced_recv;
}

void net184_observe(net184_report *out)
{
    if(!out)return;
    out->installed=state==1;
#define SNAP(field,variable) out->field=(uint32_t)InterlockedCompareExchange(&variable,0,0)
    SNAP(send_calls,io_send_calls);SNAP(send_oob,io_send_oob);SNAP(send_errors,io_send_errors);
    SNAP(recv_calls,io_recv_calls);SNAP(received_packets,io_received);SNAP(received_oob,io_recv_oob);
    SNAP(last_send_error,io_send_error);SNAP(last_recv_error,io_recv_error);
    SNAP(qa_refused_external_ip,qa_refused_external_ip);
    SNAP(qa_refused_backend,qa_refused_backend);
#undef SNAP
}

static int image_span(uintptr_t address, SIZE_T bytes, int executable)
{
    MEMORY_BASIC_INFORMATION i;
    if (!bytes || VirtualQuery((void *)address, &i, sizeof(i)) != sizeof(i)
        || i.State != MEM_COMMIT || i.Type != MEM_IMAGE || i.AllocationBase != (void *)0x400000u
        || (i.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return 0;
    DWORD p = i.Protect & 0xffu;
    if (executable && p != PAGE_EXECUTE_READ && p != PAGE_EXECUTE_READWRITE) return 0;
    uintptr_t start = (uintptr_t)i.BaseAddress;
    return address >= start && address - start < i.RegionSize && bytes <= i.RegionSize - (address - start);
}

int net184_tick(void)
{
    if(direct_peer){
        uint8_t online=1,lobby=1;
        if(InterlockedCompareExchange(&qa_boundary_failed,0,0)||!net184_peer_ready()
            ||peer_transport184_observe()!=1||!regular_file(flag_path)||!regular_file(control_path)
            ||!image_span(0x19fbfe4u,1,0)||!image_span(0x1beb4a1u,1,0))return -1;
        memcpy(&online,(void *)0x19fbfe4u,1);memcpy(&lobby,(void *)0x1beb4a1u,1);
        if(online||lobby){qa_failure(9,NULL,0);return -1;}
    }
    if(loopback_qa){
        if(InterlockedCompareExchange(&qa_boundary_failed,0,0))return -1;
        if(!regular_file(qa_path)||!regular_file(flag_path)||!regular_file(control_path)){
            qa_failure(4,NULL,0);return -1;
        }
        if(!net184_loopback_ready()){qa_failure(5,NULL,0);return -1;}
    }
    if (state) return state < 0 ? (peer_install_requested?-1:0) : 1;
    if (!flag_path[0]) {
        wchar_t exe[MAX_PATH];
        DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
        if (!n || n >= MAX_PATH) { state = -1; return 0; }
        while (n && exe[n - 1] != L'\\') --n;
        if (!n || n + 36 >= MAX_PATH) { state = -1; return 0; }
        memcpy(flag_path, exe, n * sizeof(wchar_t));
        memcpy(log_path, exe, n * sizeof(wchar_t));
        memcpy(qa_path, exe, n * sizeof(wchar_t));
        memcpy(control_path, exe, n * sizeof(wchar_t));
        lstrcpyW(flag_path + n, L"mw2-trio-network-test.enable");
        lstrcpyW(log_path + n, L"network184.log");
        lstrcpyW(qa_path + n, L"mw2-trio-loopback-qa.enable");
        lstrcpyW(control_path + n, L"mw2-trio-control.enable");
    }
    if (!regular_file(flag_path)) return peer_transport184_requested()!=0?-1:0;
    int peer=peer_transport184_requested();
    peer_install_requested=peer!=0;
    state = -1;
    bootstrap_report report;
    if ((uintptr_t)GetModuleHandleW(NULL) != 0x400000u
        || bootstrap_guard_current(&report, sizeof(report)) != 1) return peer_install_requested?-1:0;
    if(peer<0||(peer&&regular_file(qa_path)))return -1;
    if(peer&&peer_transport184_initialize(GetCurrentThreadId())!=1)return -1;
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)0x400000u;
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(0x400000u + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_THUNK_DATA32 *slots[2] = {NULL, NULL};
    if (!dir.Size || dir.Size > 65536u || !image_span(0x400000u + dir.VirtualAddress, dir.Size, 0)) return peer_install_requested?-1:0;
    const IMAGE_IMPORT_DESCRIPTOR *d = (const IMAGE_IMPORT_DESCRIPTOR *)(0x400000u + dir.VirtualAddress);
    unsigned descriptors = dir.Size / sizeof(*d);
    for (unsigned i = 0; i < descriptors && d[i].Name; ++i) {
        uintptr_t name = 0x400000u + d[i].Name;
        if (!image_span(name, 12, 0) || lstrcmpiA((const char *)name, "WSOCK32.dll")) continue;
        uintptr_t names = 0x400000u + d[i].OriginalFirstThunk, iat = 0x400000u + d[i].FirstThunk;
        for (unsigned j = 0; j < 256; ++j) {
            if (!image_span(names + j * 4u, 4, 0) || !image_span(iat + j * 4u, 4, 0)) return peer_install_requested?-1:0;
            uint32_t ordinal = *(const uint32_t *)(names + j * 4u);
            if (!ordinal) break;
            if (ordinal == (IMAGE_ORDINAL_FLAG32 | 20u)) slots[0] = (IMAGE_THUNK_DATA32 *)(iat + j * 4u);
            if (ordinal == (IMAGE_ORDINAL_FLAG32 | 17u)) slots[1] = (IMAGE_THUNK_DATA32 *)(iat + j * 4u);
        }
    }
    HMODULE sockets = GetModuleHandleW(L"WSOCK32.dll");
    void *real[2] = {(void *)GetProcAddress(sockets, (LPCSTR)20), (void *)GetProcAddress(sockets, (LPCSTR)17)};
    void *wrappers[2] = {(void *)traced_send, (void *)traced_recv};
    if (!slots[0] || !slots[1] || !real[0] || !real[1]
        || slots[0]->u1.Function != (uintptr_t)real[0] || slots[1]->u1.Function != (uintptr_t)real[1]) {
        append("install refused: stock socket import contract differs\r\n"); return peer_install_requested?-1:0;
    }
    uintptr_t first = (uintptr_t)slots[0], last = (uintptr_t)slots[1];
    if (first > last) { uintptr_t tmp = first; first = last; last = tmp; }
    if (last - first > 1024u || !image_span(first, last - first + 4u, 0)) return peer_install_requested?-1:0;
    DWORD old, ignored;
    if (!VirtualProtect((void *)first, last - first + 4u, PAGE_READWRITE, &old)) return peer_install_requested?-1:0;
    original_send = (send_fn)real[0]; original_recv = (recv_fn)real[1];
    unsigned changed = 0;
    for (; changed < 2; ++changed)
        if (InterlockedCompareExchangePointer((void *volatile *)&slots[changed]->u1.Function,
                                               wrappers[changed], real[changed]) != real[changed]) break;
    if (changed != 2) {
        while (changed) {
            --changed;
            if (InterlockedCompareExchangePointer((void *volatile *)&slots[changed]->u1.Function,
                                                   real[changed], wrappers[changed]) != wrappers[changed]) return -1;
        }
        if (!VirtualProtect((void *)first, last - first + 4u, old, &ignored)) return -1;
        append("install refused: socket import changed during transaction\r\n"); return peer_install_requested?-1:0;
    }
    if (!VirtualProtect((void *)first, last - first + 4u, old, &ignored)) return -1;
    installed_slots[0] = slots[0]; installed_slots[1] = slots[1];
    loopback_qa = regular_file(qa_path);
    direct_peer=peer!=0;
    state = 1;
    append("install ok: two stock socket imports forwarded unchanged; token-only budget=256\r\n");
    if (loopback_qa) append("QA boundary latched: IPv4 127.0.0.2 only until process exit\r\n");
    if(direct_peer)append("Private exact-peer UDP boundary latched; provider APIs unchanged\r\n");
    return 1;
}
