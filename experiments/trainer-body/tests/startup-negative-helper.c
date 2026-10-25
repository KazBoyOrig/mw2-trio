/* Own x86 EXE. Its genuine static winmm import exercises reserved!=NULL. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include "../src/winmm32_exports.h"
typedef unsigned (__cdecl *count_fn)(void);
static int failures;
static void check(int ok, const char *name)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name); failures += !ok;
}
int wmain(int argc, wchar_t **argv)
{
    if (argc != 3 && argc != 4) return 2;
    HMODULE proxy = GetModuleHandleW(L"winmm.dll");
    count_fn count = proxy ? (count_fn)GetProcAddress(proxy, "Mw2TrioProxyExportCount") : NULL;
    check(count && count() == WINMM_EXPORT_COUNT, "static import loaded own x86 proxy");
    DWORD before = GetTickCount(), value = timeGetTime(), after = GetTickCount();
    check((LONG)(value - before) >= -100 && (LONG)(after - value) >= -100, "static timeGetTime forwarding");
    HANDLE file = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    char text[4097]; DWORD got = 0;
    int read_ok = file != INVALID_HANDLE_VALUE && ReadFile(file, text, sizeof(text) - 1, &got, NULL);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    text[got] = 0;
    const char *refusal = argc == 4 ? "phase=attach-refused reason=control-enable-required"
                                   : "phase=attach-refused reason=unsupported-image-base-or-thread";
    check(read_ok && strstr(text, refusal)
          && strstr(text, " static=1 "), "static startup attach refuses unsupported own image");
    check(!strstr(text, "phase=attach-installed") && !strstr(text, "phase=wrapper-enter"), "no native startup install/wrapper path");
    check(GetFileAttributesW(argv[2]) != INVALID_FILE_ATTRIBUTES, "own mailbox remains untouched");
    printf("STATIC_NEGATIVE_RESULT=%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
