/* Own code; MIT. Exact static-load WinMain boundary for offline storage tests.
   The only attach write is CRT CALL 0x677941. Storage runs before original
   WinMain, outside loader lock; no engine call occurs within its barrier. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include "bootstrap.h"
#include "startup184.h"
#include "storage184.h"
#include "scripts184.h"
#include "cache184.h"
#include "loader_safe.h"

#define CRT_CALL 0x00677941u
#define CRT_RETURN 0x00677946u
#define ORIGINAL_WINMAIN 0x004b1bc0u
#define STARTUP_FATAL_EXIT 0xe1840010u

typedef int (WINAPI *winmain_fn)(HINSTANCE, HINSTANCE, LPSTR, int);
typedef struct startup_state184 {
    HMODULE module;
    DWORD initial_tid;
    uint8_t call_bytes[5];
    int static_attach, hook_live, before_original, wrapper_scope, barrier_held;
    unsigned barrier_enters, barrier_leaves;
    volatile LONG entries, original_returned;
} startup_state184;
static startup_state184 startup;
static wchar_t enable_path[MAX_PATH], scripts_enable_path[MAX_PATH], cache_enable_path[MAX_PATH], control_enable_path[MAX_PATH], log_path[MAX_PATH];

static int image_read(uintptr_t address, void *out, SIZE_T bytes)
{
    MEMORY_BASIC_INFORMATION mi;
    if (!bytes || VirtualQuery((LPCVOID)address, &mi, sizeof(mi)) != sizeof(mi)) return 0;
    DWORD protection = mi.Protect & 0xffu;
    if (mi.State != MEM_COMMIT || mi.Type != MEM_IMAGE || mi.AllocationBase != (void *)0x00400000u
        || (mi.Protect & PAGE_GUARD) || (protection != PAGE_EXECUTE_READ
            && protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY)) return 0;
    uintptr_t start = (uintptr_t)mi.BaseAddress;
    if (address < start || address - start >= mi.RegionSize || bytes > mi.RegionSize - (address - start)) return 0;
    loader_copy(out, (const void *)address, bytes);
    return 1;
}

static int same_image_bytes(uintptr_t address, const uint8_t *expected, SIZE_T bytes)
{
    uint8_t observed[16];
    return bytes <= sizeof(observed) && image_read(address, observed, bytes)
        && loader_equal(observed, expected, bytes);
}

static int ordinary_file(const wchar_t *path)
{
    HANDLE file = CreateFileW(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    BY_HANDLE_FILE_INFORMATION info;
    int ok = GetFileInformationByHandle(file, &info)
        && !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
    CloseHandle(file);
    return ok;
}

static void record(const char *phase, const char *reason, DWORD error, const storage184_report *r)
{
    char line[1024];
    loader_text text = {line, sizeof(line), 0}; line[0] = 0;
    loader_string(&text, "tick="); loader_unsigned(&text, GetTickCount());
    loader_string(&text, " tid="); loader_unsigned(&text, GetCurrentThreadId());
    loader_string(&text, " phase="); loader_string(&text, phase);
    loader_string(&text, " reason="); loader_string(&text, reason);
    loader_string(&text, " error="); loader_unsigned(&text, error);
    loader_string(&text, " static="); loader_unsigned(&text, (uint32_t)startup.static_attach);
    loader_string(&text, " initial_tid="); loader_unsigned(&text, startup.initial_tid);
    loader_string(&text, " entries="); loader_unsigned(&text, (uint32_t)startup.entries);
    loader_string(&text, " before_original="); loader_unsigned(&text, (uint32_t)startup.before_original);
    loader_string(&text, " barrier_enters="); loader_unsigned(&text, startup.barrier_enters);
    loader_string(&text, " barrier_leaves="); loader_unsigned(&text, startup.barrier_leaves);
    if (r) {
        loader_string(&text, " storage_result="); loader_signed(&text, r->result);
        loader_string(&text, " installed="); loader_unsigned(&text, r->installed);
        loader_string(&text, " checked_sites="); loader_unsigned(&text, r->checked_sites);
        loader_string(&text, " failed_site="); loader_unsigned(&text, r->failed_site);
        loader_string(&text, " protections_restored="); loader_unsigned(&text, r->protections_restored);
        loader_string(&text, " guards_ok="); loader_unsigned(&text, r->guards_ok);
        loader_string(&text, " allocated_clients=0x"); loader_hex(&text, r->allocated_client_base, 8);
        loader_string(&text, " allocated_metadata=0x"); loader_hex(&text, r->allocated_metadata_base, 8);
        loader_string(&text, " raw_capacity="); loader_unsigned(&text, r->observed_capacity);
        loader_string(&text, " raw_local_count="); loader_unsigned(&text, r->observed_local_count);
    }
    loader_string(&text, "\r\n");
    HANDLE file = CreateFileW(log_path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) { DWORD written; WriteFile(file, line, (DWORD)text.used, &written, NULL); CloseHandle(file); }
}

static void record_cache(const cache184_report *r)
{
    char line[768]; loader_text text = {line, sizeof(line), 0}; line[0] = 0;
    loader_string(&text, "phase=cache-result result="); loader_signed(&text, r->result);
    loader_string(&text, " installed="); loader_unsigned(&text, r->installed);
    loader_string(&text, " checked_sites="); loader_unsigned(&text, r->checked_sites);
    loader_string(&text, " failed_site="); loader_unsigned(&text, r->failed_site);
    loader_string(&text, " protections_restored="); loader_unsigned(&text, r->protections_restored);
    loader_string(&text, " guards_ok="); loader_unsigned(&text, r->guards_ok);
    loader_string(&text, " cache_base=0x"); loader_hex(&text, r->cache_base, 8);
    loader_string(&text, " cache_bytes="); loader_unsigned(&text, r->cache_bytes);
    loader_string(&text, " allocation_count="); loader_unsigned(&text, r->allocation_count);
    loader_string(&text, " traversal_count="); loader_unsigned(&text, r->traversal_count);
    loader_string(&text, " fatal="); loader_unsigned(&text, r->fatal);
    loader_string(&text, "\r\n");
    HANDLE file = CreateFileW(log_path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) { DWORD written; WriteFile(file, line, (DWORD)text.used, &written, NULL); CloseHandle(file); }
}

static int __cdecl enter_pre_winmain(void *context)
{
    if (context != &startup || !startup.static_attach || !startup.hook_live
        || startup.entries != 1 || GetCurrentThreadId() != startup.initial_tid
        || !startup.before_original || !startup.wrapper_scope || startup.barrier_held
        || !same_image_bytes(CRT_CALL, startup.call_bytes, sizeof(startup.call_bytes))) return 0;
    startup.barrier_held = 1; ++startup.barrier_enters;
    return 1;
}

static void __cdecl leave_pre_winmain(void *context)
{
    if (context == &startup && startup.wrapper_scope && startup.barrier_held
        && GetCurrentThreadId() == startup.initial_tid) {
        startup.barrier_held = 0; ++startup.barrier_leaves;
    }
}

static __declspec(noinline) int WINAPI startup_wrapper(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    LONG entries = InterlockedIncrement(&startup.entries);
    if (startup.static_attach && startup.hook_live && startup.before_original && entries == 1
        && caller == CRT_RETURN && GetCurrentThreadId() == startup.initial_tid
        && (uintptr_t)GetModuleHandleW(NULL) == 0x00400000u
        && (uintptr_t)instance == 0x00400000u
        && same_image_bytes(CRT_CALL, startup.call_bytes, sizeof(startup.call_bytes))) {
        storage184_options options;
        storage184_report report;
        loader_zero(&options, sizeof(options));
        options.version = STORAGE184_API_VERSION; options.bytes = sizeof(options);
        options.module = startup.module; options.main_thread_id = startup.initial_tid;
        options.diagnostic_tick = 1;
        options.enter_quiescence = enter_pre_winmain; options.leave_quiescence = leave_pre_winmain;
        options.quiescence_context = &startup;
        startup.wrapper_scope = 1;
        record("wrapper-enter", "verified-before-original-WinMain", 0, NULL);
        int result = storage184_install_menu(&options, &report);
        startup.wrapper_scope = 0;
        record("storage-finished", result == STORAGE184_OK ? "installed-storage-only" : "refused-or-failed", report.win32_error, &report);
        if (result == STORAGE184_FATAL_RESTORE_FAILED || startup.barrier_held) {
            record("fatal-before-WinMain", "unrestored-storage-or-barrier", report.win32_error, &report);
            ExitProcess(STARTUP_FATAL_EXIT);
        }
        cache184_options cache_options;
        cache184_report cache_report;
        loader_zero(&cache_options, sizeof(cache_options));
        cache_options.version = CACHE184_API_VERSION; cache_options.bytes = sizeof(cache_options);
        cache_options.module = startup.module; cache_options.main_thread_id = startup.initial_tid;
        cache_options.preengine_epoch = CACHE184_PREENGINE_EPOCH;
        cache_options.enter_quiescence = enter_pre_winmain; cache_options.leave_quiescence = leave_pre_winmain;
        cache_options.quiescence_context = &startup;
        startup.wrapper_scope = 1;
        int cache_result = cache184_install_preengine(&cache_options, &cache_report);
        startup.wrapper_scope = 0;
        record("cache-finished", cache_result == CACHE184_OK ? "installed-cache-reserve-three-traverse-two" :
            cache_result == CACHE184_DISABLED ? "disabled" : "refused-or-failed", cache_report.win32_error, NULL);
        record_cache(&cache_report);
        if (cache_result == CACHE184_FATAL_RESTORE_FAILED || startup.barrier_held) {
            record("fatal-before-WinMain", "unrestored-cache-or-barrier", cache_report.win32_error, NULL);
            ExitProcess(0xe1840040u);
        }
        scripts184_options script_options;
        scripts184_report script_report;
        loader_zero(&script_options, sizeof(script_options));
        script_options.version = SCRIPTS184_API_VERSION;
        script_options.bytes = sizeof(script_options);
        script_options.module = startup.module;
        script_options.main_thread_id = startup.initial_tid;
        script_options.pre_engine_invocation = 1;
        script_options.enter_quiescence = enter_pre_winmain;
        script_options.leave_quiescence = leave_pre_winmain;
        script_options.quiescence_context = &startup;
        startup.wrapper_scope = 1;
        int script_result = scripts184_install_preengine(&script_options, &script_report);
        startup.wrapper_scope = 0;
        record("scripts-finished", script_result == SCRIPTS184_OK ? "installed-source-loader" :
            script_result == SCRIPTS184_DISABLED ? "disabled" : "refused-or-failed", script_report.win32_error, NULL);
        if (script_result == SCRIPTS184_FATAL_RESTORE_FAILED || startup.barrier_held) {
            record("fatal-before-WinMain", "unrestored-script-loader-or-barrier", script_report.win32_error, NULL);
            ExitProcess(0xe1840030u);
        }
    } else record("wrapper-refused", "startup-contract-not-satisfied-passthrough", 0, NULL);
    startup.before_original = 0;
    /* First engine entry happens only after storage has left its barrier. */
    int result = ((winmain_fn)ORIGINAL_WINMAIN)(instance, previous, command, show);
    InterlockedExchange(&startup.original_returned, 1); /* retain explicit stdcall return */
    return result;
}

int startup184_attach(HMODULE module, LPVOID reserved, DWORD initial_thread_id)
{
    wchar_t directory[MAX_PATH];
    DWORD n = GetModuleFileNameW(module, directory, MAX_PATH);
    if (!n || n >= MAX_PATH - 40) return 0;
    while (n && directory[n - 1] != L'\\') --n;
    if (!n) return 0;
    directory[n] = 0;
    lstrcpyW(enable_path, directory); lstrcatW(enable_path, L"mw2-trio-storage-test.enable");
    lstrcpyW(scripts_enable_path, directory); lstrcatW(scripts_enable_path, L"mw2-trio-scripts-test.enable");
    lstrcpyW(cache_enable_path, directory); lstrcatW(cache_enable_path, L"mw2-trio-cache-test.enable");
    lstrcpyW(control_enable_path, directory); lstrcatW(control_enable_path, L"mw2-trio-control.enable");
    lstrcpyW(log_path, directory); lstrcatW(log_path, L"startup184.log");
    if (!ordinary_file(enable_path) && !ordinary_file(scripts_enable_path) && !ordinary_file(cache_enable_path)) return 0;
    startup.module = module; startup.initial_tid = initial_thread_id;
    startup.static_attach = reserved != NULL;
    if (!startup.static_attach) { record("attach-refused", "dynamic-load", 0, NULL); return 0; }
    if (!ordinary_file(control_enable_path)) { record("attach-refused", "control-enable-required", 0, NULL); return 0; }
    bootstrap_report identity;
    if (!initial_thread_id || GetCurrentThreadId() != initial_thread_id
        || (uintptr_t)GetModuleHandleW(NULL) != 0x00400000u
        || bootstrap_guard_current(&identity, sizeof(identity)) != 1) {
        record("attach-refused", "unsupported-image-base-or-thread", 0, NULL); return 0;
    }
    static const uint8_t original_call[5] = {0xe8,0x7a,0xa2,0xe3,0xff};
    static const uint8_t arguments[9] = {0x51,0x50,0x6a,0x00,0x68,0x00,0x00,0x40,0x00};
    static const uint8_t main_prefix[10] = {0xe8,0x7b,0x8e,0xff,0xff,0xe8,0x76,0xeb,0xfd,0xff};
    static const uint8_t return16[3] = {0xc2,0x10,0x00};
    if (!same_image_bytes(CRT_CALL, original_call, sizeof(original_call))
        || !same_image_bytes(0x00677938u, arguments, sizeof(arguments))
        || !same_image_bytes(ORIGINAL_WINMAIN, main_prefix, sizeof(main_prefix))
        || !same_image_bytes(0x004b1bfdu, return16, sizeof(return16))
        || !same_image_bytes(0x004b1c3cu, return16, sizeof(return16))) {
        record("attach-refused", "changed-CRT-or-WinMain-signatures", 0, NULL); return 0;
    }
    HMODULE pinned = NULL;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                            (LPCWSTR)(uintptr_t)&startup184_attach, &pinned) || pinned != module) {
        record("attach-refused", "module-pin-failed", GetLastError(), NULL); return 0;
    }
    startup.call_bytes[0] = 0xe8;
    uint32_t relative = (uint32_t)(uintptr_t)&startup_wrapper - (CRT_CALL + 5);
    loader_copy(startup.call_bytes + 1, &relative, 4);
    DWORD old, ignored;
    if (!VirtualProtect((void *)CRT_CALL, sizeof(original_call), PAGE_EXECUTE_READWRITE, &old)) {
        record("attach-refused", "write-protection-failed", GetLastError(), NULL); return 0;
    }
    loader_copy((void *)CRT_CALL, startup.call_bytes, sizeof(startup.call_bytes));
    DWORD error = 0;
    int flushed = FlushInstructionCache(GetCurrentProcess(), (void *)CRT_CALL, sizeof(startup.call_bytes));
    if (!flushed) error = GetLastError();
    int restored = VirtualProtect((void *)CRT_CALL, sizeof(original_call), old, &ignored);
    if (!restored && !error) error = GetLastError();
    if (!flushed || !restored || !same_image_bytes(CRT_CALL, startup.call_bytes, sizeof(startup.call_bytes))) {
        int writable = VirtualProtect((void *)CRT_CALL, sizeof(original_call), PAGE_EXECUTE_READWRITE, &ignored);
        int rollback_ok = 0, protection_ok = 0;
        if (writable) {
            loader_copy((void *)CRT_CALL, original_call, sizeof(original_call));
            rollback_ok = FlushInstructionCache(GetCurrentProcess(), (void *)CRT_CALL, sizeof(original_call))
                && same_image_bytes(CRT_CALL, original_call, sizeof(original_call));
            protection_ok = VirtualProtect((void *)CRT_CALL, sizeof(original_call), old, &ignored);
        }
        record(rollback_ok && protection_ok ? "attach-refused" : "attach-fatal",
               "startup-hook-transaction-failed", error ? error : GetLastError(), NULL);
        return rollback_ok && protection_ok ? 0 : -1;
    }
    startup.hook_live = 1; startup.before_original = 1;
    record("attach-installed", "static-CRT-WinMain-CALL-only", 0, NULL);
    return 1;
}
