/* Own experimental code; MIT. Explicit main-thread command integration.
 * Existing TWO-slot build184 storage/protocol only. No EXE instruction patches.
 * Private LAN entry calls local session/channel helpers rather than replacing
 * the stock online gate or calling the Steam lobby-creation/address helpers.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bootstrap.h"
#include "pure_sha256.h"
#include "party184.h"
#include "connect184.h"
#include "netopen184.h"
#include "net184.h"
#include "qa_identity184.h"
#include "peer_transport184.h"

#define U8(va) (*(volatile uint8_t *)(uintptr_t)(va))
#define U16(va) (*(volatile uint16_t *)(uintptr_t)(va))
#define U32(va) (*(volatile uint32_t *)(uintptr_t)(va))
#define NATIVE(type, va) ((type)(uintptr_t)(va))

typedef void (__cdecl *noarg_fn)(void);
typedef uint32_t (__cdecl *clock_fn)(void);
typedef void (__cdecl *create_fn)(void *, uint32_t, uint32_t, uint32_t);
typedef unsigned char (__cdecl *join_fn)(void *, uint32_t, uint32_t, const void *,
                                        uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (__cdecl *setup_fn)(uint32_t, void *, uint32_t, uint32_t, uint32_t,
                                 uint32_t, uint32_t, uint32_t, void *, uint32_t,
                                 void *, uint32_t, int32_t);
typedef void (__cdecl *member_fn)(uint32_t, const char *, uint32_t, uint32_t,
                                  uint32_t, uint32_t, uint32_t, const void *,
                                  uint32_t, uint32_t, uint32_t, uint32_t,
                                  uint32_t, uint32_t);
typedef void (__cdecl *bool_fn)(const char *, int);
typedef void (__cdecl *bandwidth_fn)(uint32_t, uint32_t);
typedef struct cmd184 {
    struct cmd184 *next;
    const char *name, *autocomplete_dir, *autocomplete_ext;
    noarg_fn function;
    uint32_t flags;
} cmd184;
_Static_assert(sizeof(void *) == 4, "x86 build required");
_Static_assert(sizeof(cmd184) == 24, "build184 command node layout");
typedef cmd184 *(__cdecl *find_fn)(const char *);
typedef void (__cdecl *add_fn)(const char *, noarg_fn, cmd184 *, uint32_t);
typedef struct guard184 { uintptr_t va; uint32_t bytes; const char *sha256; } guard184;
#include "party184-guards.inc"

static DWORD initial_tid;
static int image_checked, fatal, busy;
static wchar_t enable_path[MAX_PATH], log_path[MAX_PATH];
static party184_report report = {1, 0, 0, 0, 0, 0, 0, 0};
static cmd184 nodes[7];
static uint32_t native_frames;
static qa_identity184_report qa_identity;
static int last_connect_result;
static int last_open_result;
static netopen184_report opened_socket;

static int regular_file(const wchar_t *path)
{
    HANDLE h = CreateFileW(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL,
                          OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    BY_HANDLE_FILE_INFORMATION i;
    int ok = GetFileInformationByHandle(h, &i)
        && !(i.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
    CloseHandle(h);
    return ok;
}

static int paths(void)
{
    if (enable_path[0]) return 1;
    wchar_t exe[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (!n || n >= MAX_PATH) return 0;
    while (n && exe[n - 1] != L'\\') --n;
    if (!n || n + 40 >= MAX_PATH) return 0;
    memcpy(enable_path, exe, n * sizeof(wchar_t));
    memcpy(log_path, exe, n * sizeof(wchar_t));
    lstrcpyW(enable_path + n, L"mw2-trio-party-test.enable");
    lstrcpyW(log_path + n, L"party184.log");
    return 1;
}

static void result(uint32_t code)
{
    report.last_result = code;
    char line[512];
    int count = snprintf(line, sizeof(line),
        "party184 result=%lu registered=%lu host=%lu join=%lu launch=%lu rejected=%lu connect_result=%d "
        "open_result=%d socket_present=%lu bound_port=%lu startup_result=%lu "
        "qa_result=%ld qa_active=%lu qa_failed_precondition=%lu qa_failed_site=%lu\r\n",
        (unsigned long)code, (unsigned long)report.registered,
        (unsigned long)report.host_attempts, (unsigned long)report.join_attempts,
        (unsigned long)report.launch_attempts, (unsigned long)report.rejected, last_connect_result,
        last_open_result,(unsigned long)opened_socket.socket_present,(unsigned long)opened_socket.bound_port,
        (unsigned long)opened_socket.startup_result,(long)qa_identity.result,
        (unsigned long)qa_identity.active,(unsigned long)qa_identity.failed_precondition,
        (unsigned long)qa_identity.failed_site);
    if (count <= 0 || (size_t)count >= sizeof(line)) return;
    HANDLE h = CreateFileW(log_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(h, line, (DWORD)count, &written, NULL);
        CloseHandle(h);
    }
}

static int reject(uint32_t reason)
{
    ++report.rejected;
    result(reason);
    return 0;
}

static int image_span(uintptr_t address, SIZE_T bytes, int executable)
{
    MEMORY_BASIC_INFORMATION i;
    if (!bytes || VirtualQuery((void *)address, &i, sizeof(i)) != sizeof(i)
        || i.State != MEM_COMMIT || i.Type != MEM_IMAGE
        || i.AllocationBase != (void *)0x400000u
        || (i.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return 0;
    DWORD p = i.Protect & 0xffu;
    if (executable && p != PAGE_EXECUTE_READ && p != PAGE_EXECUTE_READWRITE) return 0;
    uintptr_t start = (uintptr_t)i.BaseAddress;
    return address >= start && address - start < i.RegionSize
        && bytes <= i.RegionSize - (address - start);
}

static int live_guards(void)
{
    ++report.guard_checks;
    for (unsigned i = 0; i < sizeof(guards184) / sizeof(guards184[0]); ++i) {
        char digest[65];
        if (!image_span(guards184[i].va, guards184[i].bytes, 1)) return 0;
        pure_sha256_data((void *)guards184[i].va, guards184[i].bytes, digest);
        if (strcmp(digest, guards184[i].sha256)) return 0;
    }
    return 1;
}

static int invoke_guard(void)
{
    if (GetCurrentThreadId() != initial_tid) return 0;
    if (fatal || !report.registered || busy) return reject(PARTY184_NATIVE_FAILURE);
    if (!regular_file(enable_path)) return reject(PARTY184_DISABLED);
    if (!live_guards()) { fatal = 1; return reject(PARTY184_BAD_CODE); }
    return 1;
}

static int qa_party_scope(void)
{
    if(net184_loopback_requested()&&!netopen184_qa_ready()) {
        reject(PARTY184_NOT_FRESH);return 0;
    }
    if(net184_peer_requested()&&!netopen184_peer_ready()){
        reject(PARTY184_NOT_FRESH);return 0;
    }
    return 1;
}
static int peer_role(unsigned role)
{
    if(!net184_peer_requested())return 1;
    peer_scope184_config c;
    return net184_peer_ready()&&peer_transport184_get_config(&c)==1&&c.role==role&&qa_identity.active;
}

/* Initial isolated-menu ownership only; no reset/leave of another session.
 * The six deferred-session outer occupancy bytes must all be clear so that
 * LobbyJoin's cleanup helper cannot enter its lifecycle/wait path.
 */
static int fresh(void)
{
    if (!image_span(0x1bde8b8u, 0x100, 0)
        || U32(0x934450) != 0x1bde8b8u || U32(0x925bc0) > 2u
        || U32(0x738c6c) != 1u
        || U8(0x88897c) || U8(0x1a7f0d9) || U8(0x934448)
        || U8(0x934449) || U8(0x1bde8c0) || U8(0x1bde920)
        || U8(0x1bde958) || U8(0x1beb4a1) || U8(0x93444b)
        || U8(0x1a6b0c1) || U32(0x88f560) || U32(0x891b9c)) return 0;
    for (unsigned i = 0; i < 6; ++i)
        if (U8(0x1bddf88u + i * 0x108u)) return 0;
    return 1;
}

static void __cdecl host_command(void)
{
    if (!invoke_guard()||!qa_party_scope()||!peer_role(PEER_SCOPE184_HOST)) return;
    ++report.host_attempts;
    if (!fresh()) { reject(PARTY184_NOT_FRESH); return; }
    /* Uses the native cached UID. The separate explicit loopback QA adapter
     * may alias the client cache; provider/account/auth APIs are untouched. */
    uint32_t uid_lo = U32(0x19fbf88), uid_hi = U32(0x19fbf8c);
    if (!(uid_lo | uid_hi)) { reject(PARTY184_BAD_UID); return; }
    busy = 1;
    void *session = (void *)0x1bde8b8u;
    /* Address by value: type2, 127.0.0.1, port0, remaining words0. */
    const uint32_t a0 = 2u, a1 = 0x0100007fu;
    U32(0x891b78) = 0;
    U32(0x88c328) = NATIVE(clock_fn, 0x4af1a0)();
    U32(0x88f560) = 0;
    U32(0x891b9c) = 0;
    U32(0x88f560) = 2;
    U16(0x88f568) = 0;
    U32(0x1a7f0f8) = 1;
    U8(0x1a7f0d9) = 1;
    NATIVE(setup_fn, 0x4d7db0)(0, (void *)0x88f550u, a0, a1, 0, 0, 0, 0,
                              (void *)0x88fb78u, 0x1000,
                              (void *)0x890b78u, 0x1000, -1);
    U8(0x88897c) = 1;
    NATIVE(bool_fn, 0x433930)("solo_play", 0);
    NATIVE(create_fn, 0x437d40)(session, 0x323, 0, 2);
    U8(0x934448) = 1;
    U32(0x93444c) = NATIVE(clock_fn, 0x44e1b0)();
    NATIVE(noarg_fn, 0x45dde0)();
    NATIVE(noarg_fn, 0x419810)();
    NATIVE(member_fn, 0x4442c0)(0, "LAN Host", uid_lo, uid_hi, 1, 0, 3,
                               (void *)0x1a50be0u, a0, a1, 0, 0, 0, 1);
    NATIVE(bandwidth_fn, 0x48f280)(0, U32(0x7349e8));
    NATIVE(bandwidth_fn, 0x448bd0)(0, U32(0x7349ec));
    busy = 0;
    if (!U8(0x934449) || !U8(0x1bde8c0) || !U8(0x1bde920)
        || U32(0x1bde928) != uid_lo || U32(0x1bde92c) != uid_hi) {
        fatal = 1; reject(PARTY184_NATIVE_FAILURE); return;
    }
    result(PARTY184_HOST_CREATED);
}

static void __cdecl join_command(void)
{
    if (!invoke_guard()||!qa_party_scope()||!peer_role(PEER_SCOPE184_CLIENT)) return;
    ++report.join_attempts;
    if (!fresh()) { reject(PARTY184_NOT_FRESH); return; }
    uint32_t host_info[15] = {0};
    busy = 1;
    U8(0x934448) = 0;
    unsigned char ok = NATIVE(join_fn, 0x492e80)((void *)0x1bde8b8u,
                                               0, 0x322, host_info, 0, 0, 2, 0);
    if (ok) {
        U32(0x93444c) = NATIVE(clock_fn, 0x44e1b0)();
        NATIVE(noarg_fn, 0x45dde0)();
        NATIVE(noarg_fn, 0x419810)();
    }
    busy = 0;
    if (!ok) { fatal = 1; reject(PARTY184_NATIVE_FAILURE); return; }
    result(PARTY184_JOIN_INITIALIZED);
}

static void __cdecl launch_command(void)
{
    if (!invoke_guard()||!qa_party_scope()) return;
    ++report.launch_attempts;
    if (!U8(0x1a7f0d9) || !U8(0x934448) || !U8(0x934449)
        || !U8(0x1bde8c0)) { reject(PARTY184_NOT_FRESH); return; }
    NATIVE(noarg_fn, 0x57aa20)();
    result(PARTY184_LAUNCH_REQUESTED);
}

static int bounded_text(uintptr_t address,char *out,size_t capacity)
{
    if(!address||!capacity)return 0;
    for(size_t n=0;n<capacity;++n){
        MEMORY_BASIC_INFORMATION m;
        if(address>UINT32_MAX-n||VirtualQuery((void *)(address+n),&m,sizeof(m))!=sizeof(m)
            ||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return 0;
        unsigned char c=*(const unsigned char *)(address+n);
        if(!c){out[n]=0;return n>0;}
        if(c<32||c>126)return 0;
        out[n]=(char)c;
    }
    return 0;
}

static void __cdecl connect_command(void)
{
    if(!invoke_guard()||!qa_party_scope())return;
    uint32_t depth=U32(0x144c950);
    if(depth>=8||U32(0x144c994u+depth*4)!=3){reject(PARTY184_NATIVE_FAILURE);return;}
    uintptr_t argv=U32(0x144c9b4u+depth*4);
    MEMORY_BASIC_INFORMATION m;
    if(!argv||VirtualQuery((void *)argv,&m,sizeof(m))!=sizeof(m)||m.State!=MEM_COMMIT
        ||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))||argv<(uintptr_t)m.BaseAddress
        ||argv-(uintptr_t)m.BaseAddress>m.RegionSize||12>m.RegionSize-(argv-(uintptr_t)m.BaseAddress)){
        reject(PARTY184_NATIVE_FAILURE);return;
    }
    char ip[16],port_text[6];
    if(!bounded_text(((uint32_t *)argv)[1],ip,sizeof(ip))
        ||!bounded_text(((uint32_t *)argv)[2],port_text,sizeof(port_text))){reject(PARTY184_NATIVE_FAILURE);return;}
    unsigned port=0;
    for(unsigned i=0;port_text[i];++i){if(port_text[i]<'0'||port_text[i]>'9'){reject(PARTY184_NATIVE_FAILURE);return;}
        port=port*10u+(unsigned)(port_text[i]-'0');}
    if(!port||port>65535){reject(PARTY184_NATIVE_FAILURE);return;}
    busy=1;
    last_connect_result=connect184_send(initial_tid,ip,port,"LAN Client",1);
    busy=0;
    if(last_connect_result!=1){
        if(last_connect_result==-3||last_connect_result==-6)fatal=1;
        reject(PARTY184_NATIVE_FAILURE);return;
    }
    result(PARTY184_CONNECT_ENQUEUED);
}

static void __cdecl open_command(void)
{
    if(!invoke_guard())return;
    if(net184_peer_requested()&&!qa_identity.active){reject(PARTY184_NOT_FRESH);return;}
    busy=1;
    last_open_result=netopen184_open(initial_tid,&opened_socket,sizeof(opened_socket));
    busy=0;
    if(last_open_result!=1){
        if(opened_socket.attempted)fatal=1;
        reject(PARTY184_NATIVE_FAILURE);return;
    }
    result(PARTY184_SOCKET_OPENED);
}

static void __cdecl qa_alias_command(void)
{
    if(!invoke_guard())return;
    if(!fresh()||!net184_loopback_ready()){reject(PARTY184_NOT_FRESH);return;}
    HMODULE own=NULL;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)(uintptr_t)&qa_alias_command,&own)){reject(PARTY184_NATIVE_FAILURE);return;}
    qa_identity184_options options={QA_IDENTITY184_VERSION,sizeof(options),own,initial_tid,
        native_frames,1,1,1};
    busy=1;
    int applied=qa_identity184_apply_client(&options,&qa_identity);
    busy=0;
    if(applied!=QA_IDENTITY184_OK){
        if(qa_identity.attempted)fatal=1;
        reject(PARTY184_NATIVE_FAILURE);return;
    }
    result(PARTY184_QA_ALIAS_APPLIED);
}
static int __cdecl peer_identity_scope(uint32_t *low)
{return net184_peer_ready()&&peer_transport184_identity_low(low)==1;}
static void __cdecl peer_identity_command(void)
{
    if(!invoke_guard())return;
    peer_scope184_config config;uint32_t low=0;
    if(!fresh()||!net184_peer_ready()||peer_transport184_get_config(&config)!=1
        ||!peer_identity_scope(&low)){reject(PARTY184_NOT_FRESH);return;}
    HMODULE own=NULL;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)(uintptr_t)&peer_identity_command,&own)){reject(PARTY184_NATIVE_FAILURE);return;}
    qa_identity184_peer_options options={QA_IDENTITY184_VERSION,sizeof(options),own,initial_tid,
        native_frames,config.role,low,peer_identity_scope};
    busy=1;int applied=qa_identity184_apply_peer(&options,&qa_identity);busy=0;
    if(applied!=QA_IDENTITY184_OK){if(qa_identity.attempted)fatal=1;reject(PARTY184_NATIVE_FAILURE);return;}
    result(PARTY184_QA_ALIAS_APPLIED);
}

int party184_tick(DWORD main_thread_id)
{
    if (!main_thread_id || GetCurrentThreadId() != main_thread_id) return -1;
    if(qa_identity184_observe(&qa_identity)<0){fatal=1;result(PARTY184_NATIVE_FAILURE);return -1;}
    if(netopen184_peer_observe()<0){fatal=1;result(PARTY184_NATIVE_FAILURE);return -1;}
    ++native_frames;
    if (!initial_tid) initial_tid = main_thread_id;
    if (initial_tid != main_thread_id || !paths() || fatal) return -1;
    if (!regular_file(enable_path)) return 0;
    if (report.registered) return 1;
    if (!image_checked) {
        bootstrap_report image;
        image_checked = 1;
        if ((uintptr_t)GetModuleHandleW(NULL) != 0x400000u
            || bootstrap_guard_current(&image, sizeof(image)) != 1
            || image.flags != MW2TRIO_GUARD_ALL) {
            fatal = 1; result(PARTY184_BAD_IMAGE); return -1;
        }
    }
    if (!live_guards()) { fatal = 1; result(PARTY184_BAD_CODE); return -1; }
    find_fn find = NATIVE(find_fn, 0x4d0310);
    cmd184 *listen = find("listen"), *connect = find("connect"), *launch = find("coopLaunch");
    if (!listen || !connect || !launch) return 0;
    if (listen->function != NATIVE(noarg_fn, 0x57a860)
        || connect->function != NATIVE(noarg_fn, 0x57a5b0)
        || launch->function != NATIVE(noarg_fn, 0x57aa20)) {
        fatal = 1; result(PARTY184_BAD_CODE); return -1;
    }
    const char *names[7] = {"mw2trio_lan_host", "mw2trio_party_join", "mw2trio_launch", "mw2trio_lan_connect","mw2trio_lan_open","mw2trio_qa_alias","mw2trio_peer_identity"};
    noarg_fn handlers[7] = {host_command, join_command, launch_command,connect_command,open_command,qa_alias_command,peer_identity_command};
    for (unsigned i = 0; i < 7; ++i) if (find(names[i])) {
        fatal = 1; result(PARTY184_NAME_COLLISION); return -1;
    }
    for (unsigned i = 0; i < 7; ++i) {
        NATIVE(add_fn, 0x428090)(names[i], handlers[i], &nodes[i], 0);
        if (find(names[i]) != &nodes[i] || nodes[i].function != handlers[i]) {
            fatal = 1; result(PARTY184_NATIVE_FAILURE); return -1;
        }
    }
    report.registered = 1;
    result(PARTY184_REGISTERED);
    return 1;
}

int party184_get_report(party184_report *out, uint32_t bytes)
{
    if (!out || bytes != sizeof(*out) || GetCurrentThreadId() != initial_tid) return 0;
    *out = report;
    return 1;
}
