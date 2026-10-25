/* Own helper process only: this does not launch or attach to a game. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include "bootstrap.h"
#include "winmm32_exports.h"

typedef DWORD (WINAPI *timer_fn)(void);
typedef int (__cdecl *guard_current_fn)(bootstrap_report *, uint32_t);
typedef int (__cdecl *guard_file_fn)(const wchar_t *, bootstrap_report *, uint32_t);
typedef unsigned (__cdecl *export_count_fn)(void);
static int failures;
static void check(int ok, const char *description) { printf("%s %s\n", ok ? "PASS" : "FAIL", description); failures += !ok; }

int wmain(int argc, wchar_t **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: helper.exe <absolute-proxy-path> [supported-game-exe] [synthetic-wrong-hash-file]\n"); return 2; }
    HMODULE proxy = LoadLibraryW(argv[1]);
    check(proxy != NULL, "x86 helper LoadLibrary(proxy)");
    if (!proxy) { printf("WIN32_ERROR %lu\n", GetLastError()); return 1; }
    unsigned resolved = 0;
    for (unsigned i = 0; i < WINMM_EXPORT_COUNT; ++i) {
        FARPROC ordinal = GetProcAddress(proxy, (LPCSTR)(uintptr_t)winmm_exports[i].ordinal);
        FARPROC named = winmm_exports[i].name ? GetProcAddress(proxy, winmm_exports[i].name) : ordinal;
        resolved += ordinal != NULL && ordinal == named;
    }
    check(resolved == WINMM_EXPORT_COUNT, "every system export resolves by original ordinal/name");
    printf("EXPORTS %u/%u\n", resolved, WINMM_EXPORT_COUNT);
    export_count_fn count = (export_count_fn)GetProcAddress(proxy, "Mw2TrioProxyExportCount");
    check(count && count() == WINMM_EXPORT_COUNT, "diagnostic export count matches generated table");
    timer_fn timer = (timer_fn)GetProcAddress(proxy, "timeGetTime");
    check(timer != NULL, "forwarded timeGetTime export exists");
    if (timer) {
        DWORD before = GetTickCount(), value = timer(), after = GetTickCount();
        check((int32_t)(value - before) >= -100 && (int32_t)(after - value) >= -100,
              "forwarded timeGetTime agrees with GetTickCount within 100ms");
        printf("TIMER before=%lu forwarded=%lu after=%lu\n", before, value, after);
    }
    guard_current_fn current = (guard_current_fn)GetProcAddress(proxy, "Mw2TrioGuardCurrent");
    guard_file_fn file = (guard_file_fn)GetProcAddress(proxy, "Mw2TrioGuardFileW");
    check(current && file, "read-only guard diagnostics exported");
    bootstrap_report report;
    if (current) {
        int status = current(&report, sizeof(report));
        check(status == 0 && report.flags != MW2TRIO_GUARD_ALL, "guard refuses unsupported helper executable");
        printf("HELPER_GUARD result=%d flags=0x%02lx sha256=%s\n", status, (unsigned long)report.flags, report.sha256);
        check(current(&report, sizeof(report) - 1) == -1, "guard refuses mismatched report ABI size");
    }
    if (file) {
        check(file(L"__mw2trio_missing_file__.exe", &report, sizeof(report)) == -2,
              "guard refuses missing input file");
        if (argc >= 3) {
            int status = file(argv[2], &report, sizeof(report));
            check(status == 1 && report.flags == MW2TRIO_GUARD_ALL, "read-only guard accepts exact build184 file (not launched)");
            printf("TARGET_GUARD result=%d flags=0x%02lx sha256=%s\n", status, (unsigned long)report.flags, report.sha256);
        }
        if (argc >= 4) {
            int status = file(argv[3], &report, sizeof(report));
            check(status == 0 && report.flags == 15u,
                  "guard rejects matching PE/timestamp/build marker with wrong SHA256");
            printf("WRONG_HASH_GUARD result=%d flags=0x%02lx sha256=%s\n", status, (unsigned long)report.flags, report.sha256);
        }
    }
    check(FreeLibrary(proxy), "helper releases proxy DLL cleanly");
    printf("RESULT: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
