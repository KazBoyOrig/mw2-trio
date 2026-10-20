/* Own process: pure SHA vectors and read-only file digest; never launches files. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../src/pure_sha256.h"
static int failures;
static void check(int ok, const char *name)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name); failures += !ok;
}
int wmain(int argc, wchar_t **argv)
{
    char hex[65];
    pure_sha256_data(NULL, 0, hex);
    check(!strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"), "SHA256 empty");
    pure_sha256_data("abc", 3, hex);
    check(!strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), "SHA256 abc");
    static const char long_text[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    pure_sha256_data(long_text, sizeof(long_text) - 1, hex);
    check(!strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"), "SHA256 56-byte padding boundary");
    pure_sha256_context context;
    unsigned char buffer[4096], digest[32];
    memset(buffer, 'a', 1000);
    pure_sha256_init(&context);
    for (unsigned i = 0; i < 1000; ++i) pure_sha256_update(&context, buffer, 1000);
    pure_sha256_final(&context, digest); pure_sha256_hex(digest, hex);
    check(!strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"), "SHA256 million a in 1000 chunks");
    if (argc >= 2) {
        HANDLE file = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        check(file != INVALID_HANDLE_VALUE, "read-only file open");
        if (file != INVALID_HANDLE_VALUE) {
            DWORD got; int ok = 1;
            static const DWORD sizes[] = {1,7,13,29,64,3,127,4096,2};
            unsigned chunk = 0;
            pure_sha256_init(&context);
            for (;;) {
                DWORD request = argc >= 3 ? sizes[chunk++ % (sizeof(sizes) / sizeof(sizes[0]))] : sizeof(buffer);
                if (!ReadFile(file, buffer, request, &got, NULL)) { ok = 0; break; }
                if (!got) break;
                pure_sha256_update(&context, buffer, got);
            }
            CloseHandle(file);
            pure_sha256_final(&context, digest); pure_sha256_hex(digest, hex);
            check(ok, "read-only complete file hash");
            printf("SHA256_FILE=%s\n", hex);
        }
    }
    printf("SHA_RESULT=%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
