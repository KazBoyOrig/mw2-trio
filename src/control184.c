/* Opt-in build184 control/telemetry. Own code; MIT, see LICENSE.
 * Only native patch: WinMain CALL 0x4B1CFA -> wrapper -> original 0x47A6F0.
 * There are no player-capacity, authentication, network or script patches here.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "bootstrap.h"
#include "control184.h"
#include "storage184.h"
#include "scripts184.h"
#include "cache184.h"

#define EXPECTED_IMAGE_BASE ((uintptr_t)0x00400000u)
#define FRAME_CALL_VA ((uintptr_t)0x004b1cfau)
#define FRAME_TARGET_VA ((uintptr_t)0x0047a6f0u)
#define CBUF_ADDTEXT_VA ((uintptr_t)0x00409b40u)
#define DVAR_FINDVAR_VA ((uintptr_t)0x004bca20u)
#define MAILBOX_LIMIT 4096u

typedef void (__cdecl *frame_fn)(void);
typedef void (__cdecl *cbuf_fn)(int, const char *);
typedef void *(__cdecl *find_dvar_fn)(const char *);
typedef struct dvar_prefix184 {
    uint32_t name, description, flags;
    uint8_t type, reserved[3];
    uint8_t current[16];
} dvar_prefix184;
_Static_assert(sizeof(dvar_prefix184) == 0x20, "x86 dvar prefix layout");

static wchar_t enable_path[MAX_PATH], mailbox_path[MAX_PATH], log_path[MAX_PATH];
static DWORD initial_tid;
static frame_fn volatile original_frame;
static volatile LONG active, wrong_thread_noted;
static SRWLOCK log_lock = SRWLOCK_INIT;
static unsigned long long frame_count, enqueue_count;
static DWORD last_poll, last_telemetry;
static int mailbox_error_noted, opt_out_noted;

static void record(const char *format, ...)
{
    char line[2048];
    int used = snprintf(line, sizeof(line), "tick=%lu tid=%lu ",
                        (unsigned long)GetTickCount(), (unsigned long)GetCurrentThreadId());
    va_list ap;
    va_start(ap, format);
    int count = vsnprintf(line + used, sizeof(line) - (size_t)used - 3, format, ap);
    va_end(ap);
    if (count < 0) return;
    used += count;
    if (used > (int)sizeof(line) - 3) used = (int)sizeof(line) - 3;
    line[used++] = '\r'; line[used++] = '\n';
    AcquireSRWLockExclusive(&log_lock);
    HANDLE file = CreateFileW(log_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(file, line, (DWORD)used, &written, NULL);
        CloseHandle(file);
    }
    ReleaseSRWLockExclusive(&log_lock);
}

static int ordinary_enable_file(void)
{
    DWORD attributes = GetFileAttributesW(enable_path);
    return attributes != INVALID_FILE_ATTRIBUTES
        && !(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
}

static int read_span(uintptr_t address, void *destination, SIZE_T bytes)
{
    MEMORY_BASIC_INFORMATION region;
    if (!bytes || VirtualQuery((LPCVOID)address, &region, sizeof(region)) != sizeof(region)) return 0;
    DWORD protection = region.Protect & 0xff;
    if (region.State != MEM_COMMIT || (region.Protect & PAGE_GUARD)
        || (protection != PAGE_READONLY && protection != PAGE_READWRITE
            && protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ
            && protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY)) return 0;
    uintptr_t start = (uintptr_t)region.BaseAddress;
    if (address < start || address - start >= region.RegionSize
        || bytes > region.RegionSize - (address - start)) return 0;
    memcpy(destination, (const void *)address, bytes);
    return 1;
}

static int read32(uintptr_t address, uint32_t *value) { return read_span(address, value, sizeof(*value)); }

static void printable_string(uintptr_t address, char *out, size_t capacity)
{
    size_t i;
    for (i = 0; i + 1 < capacity; ++i) {
        unsigned char ch;
        if (!read_span(address + i, &ch, 1)) { snprintf(out, capacity, "<unreadable>"); return; }
        if (!ch) break;
        out[i] = ch >= 32 && ch <= 126 ? (char)ch : '.';
    }
    out[i] = 0;
}

static void dvar_text(const char *name, char *out, size_t capacity)
{
    void *pointer = ((find_dvar_fn)DVAR_FINDVAR_VA)(name);
    dvar_prefix184 dvar;
    if (!pointer) { snprintf(out, capacity, "<missing>"); return; }
    if (!read_span((uintptr_t)pointer, &dvar, sizeof(dvar))) { snprintf(out, capacity, "<unreadable>"); return; }
    uint32_t value;
    memcpy(&value, dvar.current, sizeof(value));
    if (dvar.type == 0) snprintf(out, capacity, "%u", dvar.current[0] != 0);
    else if (dvar.type == 5 || dvar.type == 6) snprintf(out, capacity, "%ld", (long)(int32_t)value);
    else if (dvar.type == 7) printable_string((uintptr_t)value, out, capacity);
    else snprintf(out, capacity, "<type%u>", (unsigned)dvar.type);
}

static void telemetry(void)
{
    char map[96], running[32], coop[32], specialops[32];
    dvar_text("mapname", map, sizeof(map));
    dvar_text("sv_running", running, sizeof(running));
    dvar_text("coop", coop, sizeof(coop));
    dvar_text("specialops", specialops, sizeof(specialops));
    uint32_t capacity = 0, client_base = 0, local_participants = 0;
    uint32_t entity_client[2] = {0}, native_index[2] = {0};
    uint8_t coop_enabled = 0;
    unsigned valid = 0;
    /* Verified by native init 0x4BF8B0 and ClientBegin 0x4CF5D0. These are raw
       storage/capacity fields, never a claim about connected or playable players. */
    if (read32(0x010a3c3cu, &capacity)) valid |= 1;
    if (read32(0x010a3c10u, &client_base)) valid |= 2;
    if (read32(0x00738c6cu, &local_participants)) valid |= 4;
    if (read_span(0x0088897cu, &coop_enabled, 1)) valid |= 8;
    if (client_base == 0x0108dc68u && capacity >= 1 && capacity <= 2) {
        for (unsigned i = 0; i < 2; ++i) {
            uintptr_t entity_slot = 0x00ea76b8u + 0x270u * i + 0x108u;
            if (read32(entity_slot, &entity_client[i])) {
                valid |= 16u << i;
                if (entity_client[i] == 0x0108dc68u || entity_client[i] == 0x0108dc68u + 0xafd4u)
                    if (read32((uintptr_t)entity_client[i] + 0x100u, &native_index[i])) valid |= 64u << i;
            }
        }
    }
    record("frame=%llu enqueued_mailboxes=%llu map=%s sv_running=%s coop=%s specialops=%s "
           "raw_valid=0x%x raw_level_maxclients=%lu raw_gclient_base=0x%08lx "
           "raw_local_participants=%lu raw_coop_enabled=%u raw_entity0_client=0x%08lx "
           "raw_entity1_client=0x%08lx raw_client0_index=%ld raw_client1_index=%ld",
           frame_count, enqueue_count, map, running, coop, specialops, valid,
           (unsigned long)capacity, (unsigned long)client_base, (unsigned long)local_participants,
           (unsigned)coop_enabled, (unsigned long)entity_client[0], (unsigned long)entity_client[1],
           (long)(int32_t)native_index[0], (long)(int32_t)native_index[1]);
}

static __declspec(noinline) void poll_mailbox(void)
{
    HANDLE file = CreateFileW(mailbox_path, GENERIC_READ | DELETE, 0, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    BY_HANDLE_FILE_INFORMATION info;
    LARGE_INTEGER length;
    if (!GetFileInformationByHandle(file, &info) || !GetFileSizeEx(file, &length)
        || (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))
        || length.QuadPart <= 0 || length.QuadPart > MAILBOX_LIMIT) {
        CloseHandle(file);
        if (!mailbox_error_noted) record("mailbox rejected: must be a regular file containing 1..%u bytes; left untouched", MAILBOX_LIMIT);
        mailbox_error_noted = 1;
        return;
    }
    char text[MAILBOX_LIMIT + 2];
    DWORD got = 0;
    if (!ReadFile(file, text, (DWORD)length.QuadPart, &got, NULL) || got != (DWORD)length.QuadPart) {
        CloseHandle(file); record("mailbox read incomplete; left untouched"); return;
    }
    int printable = 1;
    for (DWORD i = 0; i < got; ++i) {
        unsigned char ch = (unsigned char)text[i];
        if ((ch < 32 || ch > 126) && ch != '\n' && ch != '\r') { printable = 0; break; }
    }
    /* Delete by the already-open handle after reading, so a concurrently replaced
       pathname can never cause us to delete a different, new mailbox. */
    FILE_DISPOSITION_INFO disposition = {TRUE};
    if (!SetFileInformationByHandle(file, FileDispositionInfo, &disposition, sizeof(disposition))) {
        DWORD error = GetLastError(); CloseHandle(file);
        record("mailbox consume failed error=%lu; no commands enqueued", (unsigned long)error); return;
    }
    CloseHandle(file);
    mailbox_error_noted = 0;
    if (!printable) { record("mailbox consumed/rejected: non-printable content; no commands enqueued"); return; }
    if (text[got - 1] != '\n') text[got++] = '\n';
    text[got] = 0;
    ((cbuf_fn)CBUF_ADDTEXT_VA)(0, text);
    ++enqueue_count;
    record("mailbox enqueued count=%llu bytes=%lu; execution requires native state evidence", enqueue_count, (unsigned long)got);
}

static void storage_record(const char *event, const storage184_report *storage)
{
    record("%s result=%ld installed=%lu guards_ok=%lu record_pointer_match_mask=0x%lx "
           "allocated_client_base=0x%08lx allocated_metadata_base=0x%08lx "
           "raw_capacity=%lu raw_client_base=0x%08lx raw_local_count=%lu raw_coop_enabled=%lu "
           "raw_entity0_client=0x%08lx raw_entity1_client=0x%08lx raw_entity2_client=0x%08lx",
           event, (long)storage->result, (unsigned long)storage->installed, (unsigned long)storage->guards_ok,
           (unsigned long)storage->record_pointer_match_mask, (unsigned long)storage->allocated_client_base,
           (unsigned long)storage->allocated_metadata_base, (unsigned long)storage->observed_capacity,
           (unsigned long)storage->observed_client_base, (unsigned long)storage->observed_local_count,
           (unsigned long)storage->observed_coop_enabled, (unsigned long)storage->entity_client[0],
           (unsigned long)storage->entity_client[1], (unsigned long)storage->entity_client[2]);
}

static void cache_record(const char *event, const cache184_report *r)
{
    record("%s result=%ld installed=%lu guards_ok=%lu fatal=%lu cache_base=0x%08lx cache_bytes=%lu "
           "allocation_count=%lu traversal_count=%lu protections_restored=%lu reset_calls=%lu "
           "reset_phase=%lu reset_markers=0x%lx reset_failure=%lu reset_tid=%lu reset_caller=0x%08lx "
           "reset_destination=0x%08lx reset_value=%lu reset_bytes=%lu",
           event, (long)r->result, (unsigned long)r->installed, (unsigned long)r->guards_ok, (unsigned long)r->fatal,
           (unsigned long)r->cache_base, (unsigned long)r->cache_bytes,
           (unsigned long)r->allocation_count, (unsigned long)r->traversal_count,
           (unsigned long)r->protections_restored, (unsigned long)r->reset_calls,
           (unsigned long)r->reset_phase, (unsigned long)r->reset_markers, (unsigned long)r->reset_failure,
           (unsigned long)r->reset_tid, (unsigned long)r->reset_caller,
           (unsigned long)r->reset_destination, (unsigned long)r->reset_value, (unsigned long)r->reset_bytes);
}

static __declspec(noinline) void control_tick(void)
{
    if (!InterlockedCompareExchange(&active, 0, 0)) return;
    if (GetCurrentThreadId() != initial_tid) {
        if (!InterlockedExchange(&wrong_thread_noted, 1)) record("unexpected frame thread; control disabled on this thread");
        return;
    }
    storage184_report storage;
    int observed = storage184_observe(&storage); /* every tick, including while opt-out pauses logging */
    if (observed == STORAGE184_GUARD_CORRUPTION || observed == STORAGE184_FATAL_RESTORE_FAILED) {
        storage_record("storage-fatal", &storage);
        /* This tick exists only after exact-image/base/signature guard and runs
           in the own test process. Never call the engine again after corruption. */
        ExitProcess(observed == STORAGE184_GUARD_CORRUPTION ? 0xe1840020u : 0xe1840021u);
    }
    cache184_report cache;
    int cache_observed = cache184_observe(&cache); /* every tick, even if mailbox opt-out */
    if (cache_observed < 0) {
        cache_record("cache-fatal", &cache);
        ExitProcess(0xe1840041u);
    }
    ++frame_count;
    DWORD now = GetTickCount();
    if ((DWORD)(now - last_poll) < 100) return;
    last_poll = now;
    if (!ordinary_enable_file()) {
        if (!opt_out_noted) record("opt-in file absent; mailbox processing/telemetry paused; CALL wrapper remains until process exit");
        opt_out_noted = 1;
        return;
    }
    opt_out_noted = 0;
    poll_mailbox();
    if ((DWORD)(now - last_telemetry) >= 1000) {
        last_telemetry = now; telemetry();
        scripts184_report script;
        scripts184_observe(&script);
        if (script.installed) {
            char players[24], probe[160];
            dvar_text("mw2trio_script_players", players, sizeof(players));
            dvar_text("mw2trio_script_probe", probe, sizeof(probe));
            record("scripts installed=%lu requests=%lu own_sources=%lu patched_sources=%lu failed_requests=%lu "
                   "applied_blocks=%lu script_players=%s probe=%s",
                   (unsigned long)script.installed, (unsigned long)script.source_requests,
                   (unsigned long)script.own_sources, (unsigned long)script.patched_sources,
                   (unsigned long)script.failed_requests, (unsigned long)script.applied_patch_blocks, players, probe);
        }
        storage_record("storage", &storage);
        if (cache.installed) cache_record("cache", &cache);
    }
}

static void __cdecl frame_wrapper(void)
{
    original_frame();
    control_tick();
}

static DWORD WINAPI control_worker(void *module_reference)
{
    bootstrap_report report;
    if (!ordinary_enable_file() || (uintptr_t)GetModuleHandleW(NULL) != EXPECTED_IMAGE_BASE
        || bootstrap_guard_current(&report, sizeof(report)) != 1) {
        record("worker refused unsupported executable/base or absent opt-in; no native writes");
        FreeLibraryAndExitThread((HMODULE)module_reference, 0);
    }
    static const uint8_t original_call[5] = {0xe8, 0xf1, 0x89, 0xfc, 0xff};
    static const uint8_t cbuf_prefix[7] = {0x6a, 0x21, 0xe8, 0x39, 0x83, 0x08, 0x00};
    static const uint8_t dvar_prefix[12] = {0x57, 0x8b, 0x7c, 0x24, 0x08, 0xe8, 0x56, 0x6a, 0x17, 0x00, 0x5f, 0xc3};
    static const uint8_t frame_prefix[6] = {0x55, 0x8b, 0xec, 0x56, 0x6a, 0x02};
    uint8_t observed[16];
    if (!read_span(FRAME_CALL_VA, observed, sizeof(original_call)) || memcmp(observed, original_call, sizeof(original_call))
        || !read_span(CBUF_ADDTEXT_VA, observed, sizeof(cbuf_prefix)) || memcmp(observed, cbuf_prefix, sizeof(cbuf_prefix))
        || !read_span(DVAR_FINDVAR_VA, observed, sizeof(dvar_prefix)) || memcmp(observed, dvar_prefix, sizeof(dvar_prefix))
        || !read_span(FRAME_TARGET_VA, observed, sizeof(frame_prefix)) || memcmp(observed, frame_prefix, sizeof(frame_prefix))) {
        record("worker refused changed native signatures; no native writes");
        FreeLibraryAndExitThread((HMODULE)module_reference, 0);
    }
    HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, initial_tid);
    if (!thread) { record("OpenThread failed error=%lu; no native writes", (unsigned long)GetLastError()); FreeLibraryAndExitThread((HMODULE)module_reference, 0); }
    CONTEXT context;
    memset(&context, 0, sizeof(context));
    context.ContextFlags = CONTEXT_CONTROL;
    uint8_t patch[5] = {0xe8};
    uint32_t relative = (uint32_t)(uintptr_t)frame_wrapper - (uint32_t)(FRAME_CALL_VA + 5);
    memcpy(patch + 1, &relative, sizeof(relative));
    original_frame = (frame_fn)FRAME_TARGET_VA;
    DWORD prior = SuspendThread(thread), error = 0, old_protection = 0;
    int installed = 0, reason = 0, retain_reference = 0;
    DWORD rollback_error = 0, protection_error = 0, resume_error = 0;
    if (prior == (DWORD)-1) {
        error = GetLastError(); CloseHandle(thread);
        record("SuspendThread failed error=%lu; no native writes", (unsigned long)error);
        FreeLibraryAndExitThread((HMODULE)module_reference, 0);
    }
    /* From here to ResumeThread: no file IO, logging, library loading, waits or
       allocation. Only context/signature checks and the verified five-byte CALL. */
    if (prior != 0) reason = 1;
    else if (!GetThreadContext(thread, &context)) { reason = 2; error = GetLastError(); }
    else if ((uintptr_t)context.Eip >= FRAME_CALL_VA && (uintptr_t)context.Eip < FRAME_CALL_VA + 5) reason = 3;
    else if (memcmp((const void *)FRAME_CALL_VA, original_call, sizeof(original_call))) reason = 4;
    else if (!VirtualProtect((LPVOID)FRAME_CALL_VA, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protection)) { reason = 5; error = GetLastError(); }
    else {
        memcpy((void *)FRAME_CALL_VA, patch, sizeof(patch));
        DWORD unused;
        BOOL flushed = FlushInstructionCache(GetCurrentProcess(), (LPCVOID)FRAME_CALL_VA, sizeof(patch));
        if (!flushed) { error = GetLastError(); reason = 6; }
        else if (!VirtualProtect((LPVOID)FRAME_CALL_VA, sizeof(patch), old_protection, &unused)) {
            error = GetLastError(); reason = 7;
        } else {
            installed = 1; retain_reference = 1; InterlockedExchange(&active, 1);
        }
        if (!installed) {
            /* Reconfirm write access before rollback; never write through a
               failed VirtualProtect. Pin the DLL if any stale CALL can remain. */
            retain_reference = 1;
            if (!VirtualProtect((LPVOID)FRAME_CALL_VA, sizeof(patch), PAGE_EXECUTE_READWRITE, &unused)) {
                rollback_error = GetLastError();
                installed = 1; /* wrapper remains, but active is still zero */
            } else {
                memcpy((void *)FRAME_CALL_VA, original_call, sizeof(original_call));
                if (FlushInstructionCache(GetCurrentProcess(), (LPCVOID)FRAME_CALL_VA, sizeof(original_call)))
                    retain_reference = 0;
                else rollback_error = GetLastError();
                if (!VirtualProtect((LPVOID)FRAME_CALL_VA, sizeof(original_call), old_protection, &unused))
                    protection_error = GetLastError();
            }
        }
    }
    DWORD resumed = ResumeThread(thread);
    if (resumed == (DWORD)-1) { resume_error = GetLastError(); resumed = ResumeThread(thread); }
    CloseHandle(thread);
    /* If both OS resume attempts fail, do not do file IO or unload code while
       the initial thread may remain suspended. The dedicated ref stays pinned. */
    if (resumed == (DWORD)-1) return 0;
    const char *state = retain_reference && reason ? "degraded-passthrough" : installed ? "active" : "unchanged-or-rolled-back";
    record("hook state=%s installed=%d active=%ld reason=%d error=%lu rollback_error=%lu protection_error=%lu "
           "initial_tid=%lu previous_suspend=%lu resume_result=%lu resume_error=%lu retained=%d site=0x004b1cfa target=0x0047a6f0",
           state, installed, (long)InterlockedCompareExchange(&active, 0, 0), reason, (unsigned long)error,
           (unsigned long)rollback_error, (unsigned long)protection_error,
           (unsigned long)initial_tid, (unsigned long)prior, (unsigned long)resumed, (unsigned long)resume_error, retain_reference);
    if (!retain_reference) FreeLibraryAndExitThread((HMODULE)module_reference, 0);
    /* Keep our dedicated module reference for the lifetime of the installed CALL.
       An injected address must never outlive the DLL code it calls. */
    return 0;
}

void control184_schedule(HINSTANCE instance, DWORD initial_thread_id)
{
    wchar_t directory[MAX_PATH];
    DWORD n = GetModuleFileNameW(instance, directory, MAX_PATH);
    if (!n || n >= MAX_PATH - 32) return;
    while (n && directory[n - 1] != L'\\') --n;
    directory[n] = 0;
    lstrcpyW(enable_path, directory); lstrcatW(enable_path, L"mw2-trio-control.enable");
    lstrcpyW(mailbox_path, directory); lstrcatW(mailbox_path, L"control-command.txt");
    lstrcpyW(log_path, directory); lstrcatW(log_path, L"control184.log");
    if (!ordinary_enable_file()) return;
    initial_tid = initial_thread_id;
    HMODULE reference = NULL;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           (LPCWSTR)(uintptr_t)&control184_schedule, &reference)) return;
    HANDLE worker = CreateThread(NULL, 0, control_worker, reference, 0, NULL);
    if (worker) CloseHandle(worker);
    else FreeLibrary(reference); /* dedicated extra ref only; attachment ref remains */
}
