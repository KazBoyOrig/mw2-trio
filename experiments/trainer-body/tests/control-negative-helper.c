/* Own x86 process only. Exercises the opt-in worker's unsupported-image gate. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

int wmain(int argc, wchar_t **argv)
{
    if (argc != 4) { fprintf(stderr, "usage: helper <proxy> <log> <mailbox>\n"); return 2; }
    HMODULE proxy = LoadLibraryW(argv[1]);
    if (!proxy) { printf("FAIL LoadLibrary error=%lu\n", GetLastError()); return 1; }
    char text[4097];
    int refused = 0, patched = 0;
    DWORD started = GetTickCount();
    while ((DWORD)(GetTickCount() - started) < 3000) {
        HANDLE file = CreateFileW(argv[2], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            DWORD got = 0;
            if (ReadFile(file, text, sizeof(text) - 1, &got, NULL)) {
                text[got] = 0;
                refused = strstr(text, "worker refused unsupported executable/base or absent opt-in; no native writes") != NULL;
                patched = strstr(text, "hook state=") != NULL;
            }
            CloseHandle(file);
        }
        if (refused || patched) break;
        Sleep(10);
    }
    int mailbox_exists = GetFileAttributesW(argv[3]) != INVALID_FILE_ATTRIBUTES;
    int released = FreeLibrary(proxy) != 0;
    printf("%s opt-in worker refuses unsupported own helper image\n", refused ? "PASS" : "FAIL");
    printf("%s no hook-install path reported\n", !patched ? "PASS" : "FAIL");
    printf("%s own test mailbox remains unconsumed\n", mailbox_exists ? "PASS" : "FAIL");
    printf("%s helper releases its DLL reference\n", released ? "PASS" : "FAIL");
    return !(refused && !patched && mailbox_exists && released);
}
