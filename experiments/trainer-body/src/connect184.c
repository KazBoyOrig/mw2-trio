/* Own experimental code; MIT. Inactive build184 TWO-player LAN candidate.
 * No online gate, Steam APIs, auth/identity edits, DNS, packet rewrite or hooks.
 * This helper only queues the native coopConnect payload. Engine owns reliable
 * transmission/retry/ACK. Caller registers/dispatches its main-thread command.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "bootstrap.h"
#include "pure_sha256.h"
#include "connect184.h"
#include "net184.h"
#include "peer_transport184.h"

#define R8(va) (*(volatile uint8_t *)(uintptr_t)(va))
#define R32(va) (*(volatile uint32_t *)(uintptr_t)(va))
#define FN(type, va) ((type)(uintptr_t)(va))
typedef void (__cdecl *init_fn)(void *, void *, uint32_t);
typedef void (__cdecl *string_fn)(void *, const char *);
typedef void (__cdecl *byte_fn)(void *, uint32_t);
typedef void (__cdecl *data_fn)(void *, const void *, uint32_t);
typedef unsigned char (__cdecl *queue_fn)(const void *, void *);
typedef void (__cdecl *bool_fn)(const char *, int);
typedef void (__cdecl *setup_fn)(uint32_t, void *, uint32_t, uint32_t, uint32_t,
                                 uint32_t, uint32_t, uint32_t, void *, uint32_t,
                                 void *, uint32_t, int32_t);
typedef struct connect_guard184 { uintptr_t va; uint32_t bytes; const char *sha256; } connect_guard184;
#include "connect184-guards.inc"
_Static_assert(sizeof(void *) == 4, "x86 build required");
static int checked_image, failed, sent;
static DWORD owner_tid;

static int image_span(uintptr_t p, SIZE_T bytes)
{
    MEMORY_BASIC_INFORMATION i;
    if (VirtualQuery((void *)p, &i, sizeof(i)) != sizeof(i)
        || i.State != MEM_COMMIT || i.Type != MEM_IMAGE
        || i.AllocationBase != (void *)0x400000u
        || (i.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return 0;
    uintptr_t start = (uintptr_t)i.BaseAddress;
    DWORD protect = i.Protect & 0xffu;
    return (protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE)
        && p >= start && p - start < i.RegionSize && bytes <= i.RegionSize - (p - start);
}

static int enabled(void)
{
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (!n || n >= MAX_PATH) return 0;
    while (n && path[n - 1] != L'\\') --n;
    if (!n || n + 32 >= MAX_PATH) return 0;
    lstrcpyW(path + n, L"mw2-trio-party-test.enable");
    HANDLE h = CreateFileW(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL,
                          OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    BY_HANDLE_FILE_INFORMATION i;
    int ok = GetFileInformationByHandle(h, &i)
        && !(i.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
    CloseHandle(h);
    return ok;
}

static int parse(const char *text, uint8_t out[4])
{
    if (!text) return 0;
    unsigned pos = 0;
    for (unsigned i = 0; i < 4; ++i) {
        unsigned value = 0, digits = 0;
        while (pos < 15 && text[pos] >= '0' && text[pos] <= '9') {
            value = value * 10u + (unsigned)(text[pos++] - '0');
            if (++digits > 3 || value > 255) return 0;
        }
        if (!digits || (i < 3 ? text[pos++] != '.' : text[pos] != 0)) return 0;
        out[i] = (uint8_t)value;
    }
    return 1;
}
static int permitted(const uint8_t out[4],unsigned port)
{
    if(net184_peer_requested()){
        peer_scope184_config c;
        return net184_peer_ready()&&peer_transport184_get_config(&c)==1
            &&c.role==PEER_SCOPE184_CLIENT&&c.peer.port==port&&!memcmp(c.peer.ipv4,out,4);
    }
    return out[0] == 127 || out[0] == 10
        || (out[0] == 172 && out[1] >= 16 && out[1] <= 31)
        || (out[0] == 192 && out[1] == 168)
        || (out[0] == 100 && out[1] >= 64 && out[1] <= 127);
}

int connect184_send(DWORD verified_main_thread_id, const char *numeric_ipv4,
                    unsigned port, const char *display_name, unsigned difficulty)
{
    uint8_t ip[4];
    if (!verified_main_thread_id || GetCurrentThreadId() != verified_main_thread_id
        || !parse(numeric_ipv4, ip) || !port || port > 65535 || !permitted(ip,port)|| !display_name
        || difficulty > 3) return -1;
    if (net184_loopback_requested() && (!net184_loopback_ready()
        || ip[0] != 127 || ip[1] || ip[2] || ip[3] != (MW2TRIO_QA_IPV4_HOST & 255u))) return -1;
    unsigned name_len = 0;
    while (name_len < 32 && display_name[name_len]) {
        if ((unsigned char)display_name[name_len] < 32
            || (unsigned char)display_name[name_len] > 126) return -1;
        ++name_len;
    }
    if (!name_len || name_len >= 32) return -1;
    if (!owner_tid) owner_tid = verified_main_thread_id;
    if (owner_tid != verified_main_thread_id || failed || sent) return -1;
    if (!enabled()) return -2;
    if (!checked_image) {
        bootstrap_report r;
        checked_image = 1;
        if ((uintptr_t)GetModuleHandleW(NULL) != 0x400000u
            || bootstrap_guard_current(&r, sizeof(r)) != 1
            || r.flags != MW2TRIO_GUARD_ALL) { failed = 1; return -3; }
    }
    for (unsigned i = 0; i < sizeof(connect_guards184) / sizeof(connect_guards184[0]); ++i) {
        char digest[65];
        const connect_guard184 *g = &connect_guards184[i];
        if (!image_span(g->va, g->bytes)) { failed = 1; return -3; }
        pure_sha256_data((void *)g->va, g->bytes, digest);
        if (strcmp(digest, g->sha256)) { failed = 1; return -3; }
    }
    /* Own only the empty client party initialized by the preceding join command. */
    if (R32(0x934450) != 0x1bde8b8u || R32(0x925bc0) > 2
        || R32(0x738c6c) != 1 || R8(0x88897c) || R8(0x1a7f0d9)
        || R8(0x934448) || !R8(0x934449) || !R8(0x1bde8c0)
        || R8(0x1bde920) || R8(0x1bde958) || R32(0x1bde918) != 0x322u
        || R32(0x1bde914) != 2 || R32(0x1bde910) != 0
        || R8(0x1beb4a1) || R8(0x1a6b0c1)
        || R32(0x88f560) || R32(0x891b9c)) return -4;
    for (unsigned i = 0; i < 4; ++i)
        if (R32(0x19f6248u + i * 0x780u)) return -4;
    uint32_t uid[2] = {R32(0x19fbf88), R32(0x19fbf8c)};
    if (!(uid[0] | uid[1])) return -5;
    uint32_t address[5] = {4, 0, 0, 0, 0};
    memcpy(&address[1], ip, sizeof(ip));
    address[2] = ((port & 0xffu) << 8) | (port >> 8);
    uint32_t msg[10];
    uint8_t data[1024];
    FN(init_fn, 0x481880)(msg, data, sizeof(data));
    FN(string_fn, 0x41c6c0)(msg, "coopConnect");
    FN(byte_fn, 0x4e6aa0)(msg, 0x1d);
    FN(data_fn, 0x41d5f0)(msg, uid, sizeof(uid));
    FN(string_fn, 0x41c6c0)(msg, display_name);
    FN(byte_fn, 0x4e6aa0)(msg, difficulty);
    if (msg[0] || msg[5] != 23u + name_len) { failed = 1; return -6; }
    R8(0x1a6b0c1) = 1;
    R32(0x891b78) = 0;
    FN(setup_fn, 0x4d7db0)(0, (void *)0x88f550u, address[0], address[1], address[2],
                          address[3], address[4], 0, (void *)0x88fb78u, 0x1000,
                          (void *)0x890b78u, 0x1000, -1);
    unsigned char queued = FN(queue_fn, 0x4f1cc0)(address, msg);
    if (!queued) { failed = 1; return -6; }
    R8(0x88897c) = 1;
    FN(bool_fn, 0x433930)("solo_play", 0);
    sent = 1;
    return 1;
}
