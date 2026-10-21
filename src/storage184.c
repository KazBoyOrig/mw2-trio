/* Own code; MIT, see LICENSE. Opt-in pre-engine build184 storage experiment.
 * The only writable game targets are four-byte operands in the 20-site manifest.
 * Explicit startup integration installs only within verified pre-engine callbacks.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "bootstrap.h"
#include "storage184.h"

_Static_assert(sizeof(void *) == 4, "Build184 storage module is x86 only");

#define CLIENT_STRIDE 0xAFD4u
#define META_STRIDE 0x180u
#define OLD_CLIENT_BASE 0x0108DC68u
#define OLD_META_BASE 0x01062194u
#define NEW_COUNT 3u
#define CANARY_BYTES 64u
#define CANARY_VALUE 0xA7u
#define MAX_PAGES (STORAGE184_SITE_COUNT * 2u)

enum operand_kind { CLIENT_ADDRESS, META_ADDRESS, CLIENT_BYTES, PLAYER_COUNT, META_BYTES };
typedef struct patch_site {
    uintptr_t va;
    uint8_t length, operand_offset, kind;
    uint32_t field_offset;
    uint8_t original[10];
} patch_site;

/* Source: exact SHA256 target, private metadata-relocations.json + full LLVM
 * disassembly. Sentient aliases 0x1062448/4C/A4 and adjacent dvars are excluded.
 */
static const patch_site sites[STORAGE184_SITE_COUNT] = {
    {0x00435E0A,5,1,META_ADDRESS,0x8C,{0x05,0x20,0x22,0x06,0x01}},
    {0x0045757D,5,1,META_ADDRESS,0,{0x05,0x94,0x21,0x06,0x01}},
    {0x00460065,6,2,META_ADDRESS,0,{0x81,0xC3,0x94,0x21,0x06,0x01}},
    {0x004828C1,6,2,META_ADDRESS,0,{0x8D,0xAB,0x94,0x21,0x06,0x01}},
    {0x004829D6,6,2,META_ADDRESS,0x74,{0xD9,0x83,0x08,0x22,0x06,0x01}},
    {0x004829E0,6,2,META_ADDRESS,0x38,{0xD9,0x83,0xCC,0x21,0x06,0x01}},
    {0x004829EA,6,2,META_ADDRESS,0x08,{0xD9,0x83,0x9C,0x21,0x06,0x01}},
    {0x004CF60D,6,2,META_ADDRESS,0,{0x81,0xC7,0x94,0x21,0x06,0x01}},
    {0x004E0399,6,2,META_ADDRESS,0x164,{0x8B,0x88,0xF8,0x22,0x06,0x01}},
    {0x005BF6B0,6,2,META_ADDRESS,0x164,{0x89,0x86,0xF8,0x22,0x06,0x01}},
    {0x005BF760,6,2,META_ADDRESS,0x164,{0x8B,0x86,0xF8,0x22,0x06,0x01}},
    {0x005BF774,6,2,META_ADDRESS,0x164,{0x89,0xBE,0xF8,0x22,0x06,0x01}},
    {0x005DB9D6,6,2,META_ADDRESS,0x164,{0x8B,0x80,0xF8,0x22,0x06,0x01}},
    {0x005E0BD9,6,2,META_ADDRESS,0,{0x81,0xC3,0x94,0x21,0x06,0x01}},
    {0x005BF6BF,6,2,META_BYTES,0,{0x81,0xFE,0x00,0x03,0x00,0x00}},
    {0x005BF780,6,2,META_BYTES,0,{0x81,0xFE,0x00,0x03,0x00,0x00}},
    {0x004BFBF8,5,1,CLIENT_ADDRESS,0,{0x68,0x68,0xDC,0x08,0x01}},
    {0x004BFC15,10,6,CLIENT_ADDRESS,0,{0xC7,0x05,0x10,0x3C,0x0A,0x01,0x68,0xDC,0x08,0x01}},
    {0x004BFBEC,5,1,CLIENT_BYTES,0,{0x68,0xA8,0x5F,0x01,0x00}},
    {0x004BFBF3,5,1,PLAYER_COUNT,0,{0xBB,0x02,0x00,0x00,0x00}}
};

typedef struct guarded_buffer {
    uint8_t *reservation, *committed, *data;
    SIZE_T reservation_bytes, committed_bytes, data_bytes, page_bytes, prefix_bytes;
} guarded_buffer;
typedef struct page_entry { void *address; DWORD original; int writable; } page_entry;
static guarded_buffer clients, metadata;
static volatile LONG attempted, installed, fatal_state, map_seen;
static DWORD owner_tid;

static int readable(uintptr_t address, void *out, SIZE_T bytes)
{
    SIZE_T done = 0;
    while (done < bytes) {
        MEMORY_BASIC_INFORMATION mi;
        uintptr_t at = address + done;
        if (at < address || VirtualQuery((LPCVOID)at, &mi, sizeof(mi)) != sizeof(mi)) return 0;
        DWORD p = mi.Protect & 0xFFu;
        if (mi.State != MEM_COMMIT || (mi.Protect & PAGE_GUARD)
            || (p != PAGE_READONLY && p != PAGE_READWRITE && p != PAGE_WRITECOPY
                && p != PAGE_EXECUTE_READ && p != PAGE_EXECUTE_READWRITE
                && p != PAGE_EXECUTE_WRITECOPY)) return 0;
        uintptr_t start = (uintptr_t)mi.BaseAddress;
        if (at < start || at - start >= mi.RegionSize) return 0;
        SIZE_T take = mi.RegionSize - (at - start);
        if (take > bytes - done) take = bytes - done;
        memcpy((uint8_t *)out + done, (const void *)at, take);
        done += take;
    }
    return 1;
}

static int read32(uintptr_t va, uint32_t *out) { return readable(va, out, 4); }

static int initial_menu(void)
{
    uint32_t base, count, local, entities;
    uint8_t coop;
    return read32(0x010A3C10u, &base) && !base
        && read32(0x010A3C3Cu, &count) && !count
        && read32(0x010A3C18u, &entities) && !entities
        && read32(0x00738C6Cu, &local) && local == 1
        && readable(0x0088897Cu, &coop, 1) && !coop;
}

static int ordinary_enable(HMODULE module)
{
    wchar_t path[MAX_PATH];
    const wchar_t name[] = L"mw2-trio-storage-test.enable";
    DWORD n = GetModuleFileNameW(module, path, MAX_PATH);
    if (!n || n >= MAX_PATH) return 0;
    while (n && path[n - 1] != L'\\' && path[n - 1] != L'/') --n;
    if (!n || n + sizeof(name) / sizeof(name[0]) > MAX_PATH) return 0;
    memcpy(path + n, name, sizeof(name));
    HANDLE file = CreateFileW(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    BY_HANDLE_FILE_INFORMATION info;
    int ok = GetFileInformationByHandle(file, &info)
        && !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
    CloseHandle(file);
    return ok;
}

static int allocate_guarded(guarded_buffer *g, uint32_t stride, uintptr_t old_base)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    g->page_bytes = si.dwPageSize;
    if (!g->page_bytes || (g->page_bytes & (g->page_bytes - 1))) return 0;
    g->data_bytes = NEW_COUNT * stride;
    /* Keep the original base residue for every alignment up to one page.
     * There is no proof that all engine accesses are alignment-independent.
     */
    g->prefix_bytes = CANARY_BYTES + ((old_base - CANARY_BYTES) & (g->page_bytes - 1));
    g->committed_bytes = (g->data_bytes + g->prefix_bytes + CANARY_BYTES + g->page_bytes - 1)
        & ~(g->page_bytes - 1);
    g->reservation_bytes = g->committed_bytes + g->page_bytes * 2;
    g->reservation = VirtualAlloc(NULL, g->reservation_bytes, MEM_RESERVE, PAGE_NOACCESS);
    if (!g->reservation) return 0;
    g->committed = VirtualAlloc(g->reservation + g->page_bytes, g->committed_bytes,
                                MEM_COMMIT, PAGE_READWRITE);
    if (!g->committed) return 0;
    g->data = g->committed + g->prefix_bytes;
    memset(g->committed, CANARY_VALUE, g->committed_bytes);
    memset(g->data, 0, g->data_bytes);
    return 1;
}

static void release_guarded(guarded_buffer *g)
{
    if (g->reservation) VirtualFree(g->reservation, 0, MEM_RELEASE);
    memset(g, 0, sizeof(*g));
}

static int guards(const guarded_buffer *g)
{
    MEMORY_BASIC_INFORMATION before, after, data;
    if (!g->reservation || !g->committed || !g->data) return 0;
    if (VirtualQuery(g->reservation, &before, sizeof(before)) != sizeof(before)
        || VirtualQuery(g->committed + g->committed_bytes, &after, sizeof(after)) != sizeof(after)
        || VirtualQuery(g->committed, &data, sizeof(data)) != sizeof(data)) return 0;
    /* Uncommitted guard pages are never made accessible by this module. */
    if (before.State != MEM_RESERVE || after.State != MEM_RESERVE
        || before.AllocationBase != g->reservation || after.AllocationBase != g->reservation
        || data.State != MEM_COMMIT || data.Protect != PAGE_READWRITE
        || data.AllocationBase != g->reservation
        || data.RegionSize < g->committed_bytes) return 0;
    for (SIZE_T i = 0; i < g->prefix_bytes; ++i)
        if (g->committed[i] != CANARY_VALUE) return 0;
    for (SIZE_T i = g->prefix_bytes + g->data_bytes; i < g->committed_bytes; ++i)
        if (g->committed[i] != CANARY_VALUE) return 0;
    return 1;
}

static uint32_t replacement(const patch_site *s)
{
    switch (s->kind) {
    case CLIENT_ADDRESS: return (uint32_t)(uintptr_t)clients.data + s->field_offset;
    case META_ADDRESS: return (uint32_t)(uintptr_t)metadata.data + s->field_offset;
    case CLIENT_BYTES: return NEW_COUNT * CLIENT_STRIDE;
    case PLAYER_COUNT: return NEW_COUNT;
    case META_BYTES: return NEW_COUNT * META_STRIDE;
    default: return 0;
    }
}

static void instruction_bytes(unsigned i, int patched, uint8_t out[10])
{
    const patch_site *s = &sites[i];
    memcpy(out, s->original, s->length);
    if (patched) { uint32_t v = replacement(s); memcpy(out + s->operand_offset, &v, 4); }
}

static int check_sites(int patched, storage184_report *r)
{
    uint8_t actual[10], expected[10];
    r->checked_sites = 0;
    for (unsigned i = 0; i < STORAGE184_SITE_COUNT; ++i) {
        instruction_bytes(i, patched, expected);
        if (!readable(sites[i].va, actual, sites[i].length)
            || memcmp(actual, expected, sites[i].length)) { r->failed_site = i; return 0; }
        ++r->checked_sites;
    }
    return 1;
}

static int collect_pages(page_entry pages[MAX_PAGES], unsigned *count)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    if (!si.dwPageSize || (si.dwPageSize & (si.dwPageSize - 1))) return 0;
    *count = 0;
    for (unsigned i = 0; i < STORAGE184_SITE_COUNT; ++i) {
        uintptr_t first = sites[i].va & ~((uintptr_t)si.dwPageSize - 1);
        uintptr_t last = (sites[i].va + sites[i].length - 1) & ~((uintptr_t)si.dwPageSize - 1);
        for (uintptr_t page = first; page <= last; page += si.dwPageSize) {
            unsigned j;
            for (j = 0; j < *count; ++j) if (pages[j].address == (void *)page) break;
            if (j != *count) continue;
            MEMORY_BASIC_INFORMATION mi;
            if (*count >= MAX_PAGES || VirtualQuery((void *)page, &mi, sizeof(mi)) != sizeof(mi)
                || mi.State != MEM_COMMIT || mi.Type != MEM_IMAGE
                || mi.AllocationBase != (void *)0x00400000u || (mi.Protect & PAGE_GUARD)
                || (mi.Protect != PAGE_EXECUTE_READ && mi.Protect != PAGE_EXECUTE_READWRITE
                    && mi.Protect != PAGE_EXECUTE_WRITECOPY)) return 0;
            pages[*count].address = (void *)page;
            pages[*count].original = mi.Protect;
            pages[*count].writable = 0;
            ++*count;
        }
    }
    return 1;
}

static int restore_pages(page_entry pages[MAX_PAGES], unsigned count)
{
    SYSTEM_INFO si;
    int ok = 1;
    GetSystemInfo(&si);
    for (unsigned i = 0; i < count; ++i) {
        DWORD ignored;
        MEMORY_BASIC_INFORMATION mi;
        if (pages[i].writable) {
            if (!VirtualProtect(pages[i].address, si.dwPageSize, pages[i].original, &ignored)) ok = 0;
            else pages[i].writable = 0;
        }
        if (VirtualQuery(pages[i].address, &mi, sizeof(mi)) != sizeof(mi)
            || mi.Protect != pages[i].original) ok = 0;
    }
    return ok;
}

static int make_pages_writable(page_entry pages[MAX_PAGES], unsigned count)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    for (unsigned i = 0; i < count; ++i) {
        DWORD previous;
        if (!VirtualProtect(pages[i].address, si.dwPageSize, PAGE_EXECUTE_READWRITE, &previous)) return 0;
        pages[i].writable = 1;
        if (previous != pages[i].original && previous != PAGE_EXECUTE_READWRITE) return 0;
    }
    return 1;
}

static void write_operands(int patched)
{
    for (unsigned i = 0; i < STORAGE184_SITE_COUNT; ++i) {
        uint8_t bytes[10];
        instruction_bytes(i, patched, bytes);
        memcpy((void *)(sites[i].va + sites[i].operand_offset), bytes + sites[i].operand_offset, 4);
    }
}

/* The caller's quiescence barrier supplies isolation. Multiple code writes are
 * not CPU-atomic as a group. All signatures are checked before the first write;
 * any failed postcondition restores the complete prior operand set.
 */
static int transaction(int install_now, storage184_report *r)
{
    page_entry pages[MAX_PAGES];
    unsigned count;
    int old_patched = !install_now;
    if (!check_sites(old_patched, r)) return STORAGE184_BYTES_DIFFER;
    if (!initial_menu() || !guards(&clients) || !guards(&metadata)) return STORAGE184_NOT_INITIAL_MENU;
    if (!collect_pages(pages, &count)) return STORAGE184_TRANSACTION_FAILED;
    if (!make_pages_writable(pages, count)) {
        r->win32_error = GetLastError();
        r->protections_restored = restore_pages(pages, count);
        return r->protections_restored ? STORAGE184_TRANSACTION_FAILED : STORAGE184_FATAL_RESTORE_FAILED;
    }
    if (!check_sites(old_patched, r) || !initial_menu()) {
        r->protections_restored = restore_pages(pages, count);
        return r->protections_restored ? STORAGE184_TRANSACTION_FAILED : STORAGE184_FATAL_RESTORE_FAILED;
    }
    write_operands(install_now);
    int flushed = FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    int bytes_ok = check_sites(install_now, r);
    r->protections_restored = restore_pages(pages, count);
    if (flushed && bytes_ok && r->protections_restored) return STORAGE184_OK;
    r->win32_error = GetLastError();
    if (!make_pages_writable(pages, count)) {
        r->protections_restored = restore_pages(pages, count);
        return STORAGE184_FATAL_RESTORE_FAILED;
    }
    write_operands(old_patched);
    flushed = FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    bytes_ok = check_sites(old_patched, r);
    r->protections_restored = restore_pages(pages, count);
    return flushed && bytes_ok && r->protections_restored
        ? STORAGE184_TRANSACTION_FAILED : STORAGE184_FATAL_RESTORE_FAILED;
}

static void report_init(storage184_report *r)
{
    memset(r, 0, sizeof(*r));
    r->version = STORAGE184_API_VERSION;
    r->bytes = sizeof(*r);
    r->failed_site = UINT32_MAX;
}

static int options_ok(const storage184_options *o)
{
    return o && o->version == STORAGE184_API_VERSION && o->bytes == sizeof(*o)
        && o->module && o->main_thread_id;
}

static void snapshot(storage184_report *r)
{
    r->installed = installed != 0;
    r->allocated_client_base = (uint32_t)(uintptr_t)clients.data;
    r->allocated_metadata_base = (uint32_t)(uintptr_t)metadata.data;
    r->guards_ok = guards(&clients) && guards(&metadata);
    read32(0x010A3C3Cu, &r->observed_capacity);
    read32(0x010A3C10u, &r->observed_client_base);
    if (installed && (r->observed_capacity || r->observed_client_base)) map_seen = 1;
    read32(0x00738C6Cu, &r->observed_local_count);
    uint8_t coop = 0;
    readable(0x0088897Cu, &coop, 1);
    r->observed_coop_enabled = coop;
    for (unsigned i = 0; i < NEW_COUNT; ++i) {
        if (read32(0x00EA76B8u + i * 0x270u + 0x108u, &r->entity_client[i])
            && clients.data && r->entity_client[i] == (uint32_t)(uintptr_t)clients.data + i * CLIENT_STRIDE)
            r->record_pointer_match_mask |= 1u << i;
    }
}

int __cdecl storage184_install_menu(const storage184_options *o, storage184_report *r)
{
    bootstrap_report identity;
    int result;
    if (!r) return STORAGE184_BAD_ARGUMENT;
    report_init(r);
    if (!options_ok(o)) return r->result = STORAGE184_BAD_ARGUMENT;
    if (GetCurrentThreadId() != o->main_thread_id) return r->result = STORAGE184_WRONG_THREAD;
    if (o->diagnostic_tick != 1) return r->result = STORAGE184_NOT_INITIAL_MENU;
    if (!ordinary_enable(o->module)) return r->result = STORAGE184_DISABLED;
    if (!o->enter_quiescence || !o->leave_quiescence) return r->result = STORAGE184_NO_QUIESCENCE;
    if (InterlockedCompareExchange(&attempted, 1, 0)) return r->result = STORAGE184_ALREADY_ATTEMPTED;
    if ((uintptr_t)GetModuleHandleW(NULL) != 0x00400000u
        || bootstrap_guard_current(&identity, sizeof(identity)) != 1
        || identity.flags != MW2TRIO_GUARD_ALL) return r->result = STORAGE184_WRONG_BUILD;
    if (!initial_menu()) return r->result = STORAGE184_NOT_INITIAL_MENU;
    if (!check_sites(0, r)) return r->result = STORAGE184_BYTES_DIFFER;
    if (!allocate_guarded(&clients, CLIENT_STRIDE, OLD_CLIENT_BASE)
        || !allocate_guarded(&metadata, META_STRIDE, OLD_META_BASE)) {
        r->win32_error = GetLastError();
        release_guarded(&metadata); release_guarded(&clients);
        return r->result = STORAGE184_ALLOCATION_FAILED;
    }
    if (!o->enter_quiescence(o->quiescence_context)) {
        release_guarded(&metadata); release_guarded(&clients);
        return r->result = STORAGE184_NO_QUIESCENCE;
    }
    /* Copy exactly two old records, only while the worker barrier is held. */
    if (!readable(OLD_CLIENT_BASE, clients.data, 2u * CLIENT_STRIDE)
        || !readable(OLD_META_BASE, metadata.data, 2u * META_STRIDE)) result = STORAGE184_NOT_INITIAL_MENU;
    else result = transaction(1, r);
    if (result == STORAGE184_OK) { installed = 1; owner_tid = o->main_thread_id; }
    if (result == STORAGE184_FATAL_RESTORE_FAILED) fatal_state = 1;
    o->leave_quiescence(o->quiescence_context);
    if (result != STORAGE184_OK && !fatal_state) { release_guarded(&metadata); release_guarded(&clients); }
    snapshot(r);
    return r->result = result;
}

int __cdecl storage184_observe(storage184_report *r)
{
    if (!r) return STORAGE184_BAD_ARGUMENT;
    report_init(r);
    if (fatal_state) return r->result = STORAGE184_FATAL_RESTORE_FAILED;
    if (!installed) return r->result = STORAGE184_DISABLED;
    if (GetCurrentThreadId() != owner_tid) return r->result = STORAGE184_WRONG_THREAD;
    snapshot(r);
    return r->result = r->guards_ok ? STORAGE184_OK : STORAGE184_GUARD_CORRUPTION;
}

int __cdecl storage184_rollback_menu(const storage184_options *o, storage184_report *r)
{
    int result;
    if (!r) return STORAGE184_BAD_ARGUMENT;
    report_init(r);
    if (!options_ok(o)) return r->result = STORAGE184_BAD_ARGUMENT;
    if (GetCurrentThreadId() != o->main_thread_id || GetCurrentThreadId() != owner_tid)
        return r->result = STORAGE184_WRONG_THREAD;
    if (fatal_state) return r->result = STORAGE184_FATAL_RESTORE_FAILED;
    if (!installed) return r->result = STORAGE184_DISABLED;
    if (map_seen || !initial_menu()) return r->result = STORAGE184_NOT_INITIAL_MENU;
    if (!o->enter_quiescence || !o->leave_quiescence || !o->enter_quiescence(o->quiescence_context))
        return r->result = STORAGE184_NO_QUIESCENCE;
    result = transaction(0, r);
    if (result == STORAGE184_OK) installed = 0;
    if (result == STORAGE184_FATAL_RESTORE_FAILED) fatal_state = 1;
    o->leave_quiescence(o->quiescence_context);
    if (result == STORAGE184_OK) { release_guarded(&metadata); release_guarded(&clients); }
    snapshot(r);
    return r->result = result;
}
