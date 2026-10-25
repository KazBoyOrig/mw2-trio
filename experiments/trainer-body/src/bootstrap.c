/* Minimal x86 winmm forwarding/bootstrap diagnostic, adapted from mw2_coop (MIT).
 * Native control is opt-in and exact-build guarded; see control184.c.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include "bootstrap.h"
#include "winmm32_exports.h"
#include "control184.h"
#include "pure_sha256.h"
#include "loader_safe.h"
#include "startup184.h"

FARPROC winmm_real[WINMM_EXPORT_COUNT];
static HINSTANCE module_self;
static const char expected_sha256[] = "4dd53fabf9677980aaf081009584ade99d6bf440330046e5c8950e87ef0547d9";

static uint16_t u16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8); }
static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static void diagnostic(const char *event, int result, const bootstrap_report *r)
{
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(module_self, path, MAX_PATH);
    if (!n || n >= MAX_PATH) return;
    while (n && path[n - 1] != L'\\') --n;
    path[n] = 0;
    if (n + 27 >= MAX_PATH) return;
    lstrcatW(path, L"bootstrap-diagnostics.log");
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    char line[320];
    loader_text text = {line, sizeof(line), 0}; line[0] = 0;
    loader_string(&text, event); loader_string(&text, " result="); loader_signed(&text, result);
    loader_string(&text, " flags=0x"); loader_hex(&text, r->flags, 2);
    loader_string(&text, " machine=0x"); loader_hex(&text, r->machine, 4);
    loader_string(&text, " magic=0x"); loader_hex(&text, r->optional_magic, 4);
    loader_string(&text, " timestamp=0x"); loader_hex(&text, r->pe_timestamp, 8);
    loader_string(&text, " marker="); loader_string(&text, r->build_marker);
    loader_string(&text, " sha256="); loader_string(&text, r->sha256);
    loader_string(&text, "\r\n");
    DWORD written; WriteFile(h, line, (DWORD)text.used, &written, NULL);
    CloseHandle(h);
}

int __cdecl bootstrap_guard_file(const wchar_t *path, bootstrap_report *r, uint32_t report_bytes)
{
    if (!path || !r || report_bytes != sizeof(*r)) return -1;
    loader_zero(r, sizeof(*r));
    r->version = MW2TRIO_REPORT_VERSION;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) { diagnostic("guard-file", -2, r); return -2; }
    LARGE_INTEGER length;
    if (!GetFileSizeEx(file, &length) || length.QuadPart < 64 || length.QuadPart > 256 * 1024 * 1024) {
        CloseHandle(file); diagnostic("guard-file", 0, r); return 0;
    }
    DWORD bytes = (DWORD)length.QuadPart;
    uint8_t *data = HeapAlloc(GetProcessHeap(), 0, bytes);
    if (!data) { CloseHandle(file); return -3; }
    DWORD total = 0;
    while (total < bytes) {
        DWORD got = 0;
        if (!ReadFile(file, data + total, bytes - total, &got, NULL) || !got) break;
        total += got;
    }
    CloseHandle(file);
    int result = 0;
    if (total != bytes) { result = -4; goto done; }
    pure_sha256_data(data, bytes, r->sha256);
    if (loader_equal(r->sha256, expected_sha256, sizeof(expected_sha256))) r->flags |= 16;
    if (u16(data) != 0x5a4d) goto done;
    uint32_t pe = u32(data + 0x3c);
    if (pe > bytes - 24 || u32(data + pe) != 0x00004550) goto done;
    uint16_t sections = u16(data + pe + 6), opt_bytes = u16(data + pe + 20);
    uint32_t opt = pe + 24;
    if (opt_bytes < 32 || opt_bytes > bytes - opt) goto done;
    uint32_t table = opt + opt_bytes;
    if ((uint32_t)sections > (bytes - table) / 40) goto done;
    r->flags |= 1;
    r->machine = u16(data + pe + 4);
    r->pe_timestamp = u32(data + pe + 8);
    r->optional_magic = u16(data + opt);
    if (r->machine == 0x14c && r->optional_magic == 0x10b) r->flags |= 2;
    if (r->pe_timestamp == 0x4b7215e1u) r->flags |= 4;
    if (r->optional_magic == 0x10b) {
        uint32_t imagebase = u32(data + opt + 28);
        if (imagebase <= 0x6d2784u) {
            uint32_t marker_rva = 0x6d2784u - imagebase;
            for (unsigned i = 0; i < sections; ++i) {
                const uint8_t *sec = data + table + 40 * i;
                uint32_t va = u32(sec + 12), raw_bytes = u32(sec + 16), raw = u32(sec + 20);
                if (marker_rva < va || marker_rva - va > raw_bytes || raw_bytes - (marker_rva - va) < 4) continue;
                uint32_t rel = marker_rva - va;
                if (raw > bytes || rel > bytes - raw || bytes - raw - rel < 4) continue;
                if (loader_equal(data + raw + rel, "184\0", 4)) {
                    loader_copy(r->build_marker, "184", 4); r->flags |= 8;
                }
            }
        }
    }
    result = r->flags == MW2TRIO_GUARD_ALL;
done:
    HeapFree(GetProcessHeap(), 0, data);
    diagnostic("guard-file", result, r);
    return result;
}

int __cdecl bootstrap_guard_current(bootstrap_report *r, uint32_t report_bytes)
{
    wchar_t path[32768];
    DWORD n = GetModuleFileNameW(NULL, path, 32768);
    if (!n || n >= 32768) return -5;
    return bootstrap_guard_file(path, r, report_bytes);
}

unsigned __cdecl bootstrap_export_count(void) { return WINMM_EXPORT_COUNT; }

static int proxy_init(void)
{
    wchar_t path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (!n || n > MAX_PATH - 12) return 0;
    lstrcatW(path, L"\\winmm.dll");
    HMODULE real = LoadLibraryW(path);
    if (!real || real == module_self) return 0;
    for (unsigned i = 0; i < WINMM_EXPORT_COUNT; ++i) {
        winmm_real[i] = GetProcAddress(real, winmm_exports[i].name ? winmm_exports[i].name : (LPCSTR)(uintptr_t)winmm_exports[i].ordinal);
        if (!winmm_real[i]) return 0;
    }
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        module_self = instance;
        DisableThreadLibraryCalls(instance);
        if (!proxy_init()) return FALSE;
        if (startup184_attach(instance, reserved, GetCurrentThreadId()) < 0) return FALSE;
        control184_schedule(instance, GetCurrentThreadId());
        return TRUE;
    }
    return TRUE;
}
