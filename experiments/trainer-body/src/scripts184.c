/* MW2 build184 source-buffer loader. MIT, see LICENSE.
 * Patching grammar and loader design adapted from stxlvn/mw2_coop scripts.c,
 * pin 8fcb6bcf9980894a631906059558a4797f4c9752 (MIT).
 * New x86 ABI guards, bounded parser/paths, transaction and diagnostics are own
 * code. No game script assets are embedded. This file is inactive by default.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "bootstrap.h"
#include "scripts184.h"
#include "runtime184.h"
#include "bodyassets184.h"

_Static_assert(sizeof(void *) == 4, "build184 source loader requires x86");

#define SOURCE_LIMIT (16u * 1024u * 1024u)
#define NAME_LIMIT 192u
#define PATH_CAP 1024u
#define SOURCE_VA ((uintptr_t)0x00610020u)
#define RAWFILE_TYPE 0x24

typedef void *(__cdecl *db_find_fn)(int, const char *);
typedef int (__cdecl *db_default_fn)(int, const char *);
typedef int (__cdecl *raw_length_fn)(void *);
typedef char *(__cdecl *hunk_fn)(int);
typedef void (__cdecl *raw_buffer_fn)(void *, char *, int);

static wchar_t content_dir[MAX_PATH], log_path[MAX_PATH];
static volatile LONG attempted, installed, fatal_state;
static volatile LONG source_requests, own_sources, patched_sources;
static volatile LONG failed_requests, applied_blocks;
static SRWLOCK log_lock = SRWLOCK_INIT;

/* Own original compiler probe, never a game asset. Body source cannot be
 * delivered without the installed semantic reader and native loader scope.
 * A console-created readiness Dvar therefore cannot enable missing-runtime
 * configurations. Runtime asset residency is checked later, before precache.
 */
static const char baseline_probe184[] =
    "// Own code, MIT. Loader/compiler probe only; no networking or gameplay changes.\n"
    "players_ready( players )\n{\n"
    "\tSetDvar( \"mw2trio_script_players\", players.size );\n"
    "\tSetDvar( \"mw2trio_script_probe\", \"v1 map=\" + GetDvar( \"mapname\" ) + \" players=\" + players.size );\n"
    "}\n";

static const char *guarded_own_source(const wchar_t *name, const char *text,
                                    size_t *bytes, int channel, int loader)
{
    if (!name || !text || !bytes) return NULL;
    if (_wcsicmp(name,L"maps\\_mw2trio_probe.gsc") == 0 && (channel != 1 || loader != 1)) {
        *bytes = sizeof(baseline_probe184) - 1;
        return baseline_probe184;
    }
    return text;
}

typedef struct signature {
    uintptr_t va;
    uint8_t size, bytes[16];
} signature;

static const signature signatures[] = {
    {0x00610020u,14,{0x53,0x56,0x8b,0x74,0x24,0x10,0x56,0x6a,0x24,0xe8,0xe2,0x6e,0xee,0xff}},
    {0x004f6f10u,11,{0x51,0x53,0x8b,0x1d,0x6c,0x11,0x69,0x00,0x55,0x56,0x57}},
    {0x004c32b0u,15,{0x53,0x8b,0x5c,0x24,0x0c,0x56,0x57,0x8b,0x7c,0x24,0x10,0x8b,0xc7,0x8b,0xcb}},
    {0x004e3080u,11,{0x8b,0x44,0x24,0x04,0x8b,0x40,0x08,0x83,0xc0,0x01,0xc3}},
    {0x0040eed0u,15,{0xa1,0x8c,0x75,0x95,0x01,0x8b,0x54,0x24,0x04,0x8b,0x0d,0x20,0x04,0x96,0x01}},
    {0x004e55f0u,12,{0x83,0xec,0x3c,0x53,0x55,0x56,0x57,0x68,0x00,0x00,0x02,0x00}}
};

static void record(const char *format, ...)
{
    if (!log_path[0]) return;
    char line[1200];
    int used = snprintf(line, sizeof(line), "tick=%lu tid=%lu ",
        (unsigned long)GetTickCount(), (unsigned long)GetCurrentThreadId());
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(line + used, sizeof(line) - (size_t)used - 3, format, ap);
    va_end(ap);
    if (n < 0) return;
    used += n;
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

static int readable(uintptr_t at, void *out, SIZE_T size)
{
    MEMORY_BASIC_INFORMATION mi;
    if (!size || VirtualQuery((void *)at, &mi, sizeof(mi)) != sizeof(mi)) return 0;
    DWORD p = mi.Protect & 0xffu;
    if (mi.State != MEM_COMMIT || (mi.Protect & PAGE_GUARD)
        || (p != PAGE_READONLY && p != PAGE_READWRITE && p != PAGE_WRITECOPY
            && p != PAGE_EXECUTE_READ && p != PAGE_EXECUTE_READWRITE && p != PAGE_EXECUTE_WRITECOPY)) return 0;
    uintptr_t base = (uintptr_t)mi.BaseAddress;
    if (at < base || at - base >= mi.RegionSize || size > mi.RegionSize - (at - base)) return 0;
    memcpy(out, (const void *)at, size);
    return 1;
}

static int initial_zero_level(void)
{
    uint32_t base, count, entities;
    return readable(0x010a3c10u, &base, 4) && !base
        && readable(0x010a3c3cu, &count, 4) && !count
        && readable(0x010a3c18u, &entities, 4) && !entities;
}

static int check_signatures(scripts184_report *r)
{
    uint8_t bytes[16];
    r->checked_signatures = 0;
    for (unsigned i = 0; i < sizeof(signatures) / sizeof(signatures[0]); ++i) {
        if (!readable(signatures[i].va, bytes, signatures[i].size)
            || memcmp(bytes, signatures[i].bytes, signatures[i].size)) return 0;
        ++r->checked_signatures;
    }
    return 1;
}

static int ordinary_enable(HMODULE module)
{
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(module, path, MAX_PATH);
    if (!n || n >= MAX_PATH - 40) return 0;
    while (n && path[n - 1] != L'\\') --n;
    if (!n) return 0;
    path[n] = 0;
    lstrcpyW(content_dir, path); lstrcatW(content_dir, L"mw2trio");
    lstrcpyW(log_path, path); lstrcatW(log_path, L"scripts184.log");
    lstrcatW(path, L"mw2-trio-scripts-test.enable");
    HANDLE file = CreateFileW(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    BY_HANDLE_FILE_INFORMATION info;
    int ok = GetFileInformationByHandle(file, &info)
        && GetFileType(file) == FILE_TYPE_DISK
        && !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
    CloseHandle(file);
    return ok;
}

static int device_component(const wchar_t *part, size_t bytes)
{
    wchar_t name[5] = {0};
    size_t n = 0;
    while (n < bytes && n < 4 && part[n] != L'.') {
        wchar_t ch = part[n];
        name[n++] = ch >= L'a' && ch <= L'z' ? ch - (L'a' - L'A') : ch;
    }
    if (n == 3 && (wmemcmp(name,L"CON",3) == 0 || wmemcmp(name,L"PRN",3) == 0
        || wmemcmp(name,L"AUX",3) == 0 || wmemcmp(name,L"NUL",3) == 0)) return 1;
    return n == 4 && (wmemcmp(name,L"COM",3) == 0 || wmemcmp(name,L"LPT",3) == 0)
        && name[3] >= L'1' && name[3] <= L'9'
        && (bytes == 4 || part[4] == L'.');
}

static int safe_name(const char *name, wchar_t out[NAME_LIMIT + 1])
{
    if (!name) return 0;
    size_t n = 0, component = 0;
    while (name[n]) {
        unsigned char ch = (unsigned char)name[n];
        if (n == NAME_LIMIT) return 0;
        if (ch == '/' || ch == '\\') {
            size_t len = n - component;
            if (!len || out[n-1] == L'.' || device_component(out + component, len)) return 0;
            out[n] = L'\\'; component = n + 1;
        } else {
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
                || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.')) return 0;
            out[n] = (wchar_t)ch;
        }
        ++n;
    }
    size_t len = n - component;
    if (!len || out[n-1] == L'.' || device_component(out + component, len)) return 0;
    out[n] = 0;
    return 1;
}

/* 1 regular directory, 0 missing, -1 unsafe or inaccessible. */
static int directory_status(const wchar_t *path)
{
    DWORD a = GetFileAttributesW(path);
    if (a == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? 0 : -1;
    }
    return (a & FILE_ATTRIBUTE_DIRECTORY) && !(a & FILE_ATTRIBUTE_REPARSE_POINT) ? 1 : -1;
}

/* Returns 1/read, 0/no mod file, -1/refused or read failure. All parent
 * components below the fixed content directory are checked against reparse
 * traversal, and the final file is opened without following a reparse point.
 */
static int read_mod_file(const wchar_t *sub, const wchar_t *name, const wchar_t *suffix,
                         char **text, size_t *size)
{
    wchar_t path[PATH_CAP];
    int n = _snwprintf(path, PATH_CAP, L"\\\\?\\%ls", content_dir);
    if (n <= 0 || n >= (int)PATH_CAP) return -1;
    int status = directory_status(path);
    if (status != 1) return status;
    size_t used = (size_t)n;
    size_t sub_len = wcslen(sub), name_len = wcslen(name), suffix_len = wcslen(suffix);
    if (used + sub_len + name_len + suffix_len + 3 >= PATH_CAP) return -1;
    path[used++] = L'\\'; memcpy(path + used, sub, (sub_len + 1) * sizeof(wchar_t)); used += sub_len;
    status = directory_status(path);
    if (status != 1) return status;
    path[used++] = L'\\';
    for (size_t i = 0; i < name_len; ++i) {
        path[used++] = name[i];
        if (name[i] == L'\\') {
            path[used-1] = 0; status = directory_status(path); path[used-1] = L'\\';
            if (status != 1) return status;
        }
    }
    memcpy(path + used, suffix, (suffix_len + 1) * sizeof(wchar_t));
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? 0 : -1;
    }
    BY_HANDLE_FILE_INFORMATION info;
    LARGE_INTEGER length;
    if (!GetFileInformationByHandle(file, &info) || GetFileType(file) != FILE_TYPE_DISK
        || (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
        || !GetFileSizeEx(file, &length) || length.QuadPart <= 0 || length.QuadPart > SOURCE_LIMIT) {
        CloseHandle(file); return -1;
    }
    char *buf = malloc((size_t)length.QuadPart + 1);
    if (!buf) { CloseHandle(file); return -1; }
    DWORD got = 0;
    int ok = ReadFile(file, buf, (DWORD)length.QuadPart, &got, NULL) && got == (DWORD)length.QuadPart;
    CloseHandle(file);
    if (!ok || memchr(buf, 0, got)) { free(buf); return -1; }
    size_t w = 0;
    for (DWORD i = 0; i < got; ++i) if (buf[i] != '\r') buf[w++] = buf[i];
    buf[w] = 0;
    *text = buf; *size = w;
    return 1;
}

static uint32_t fnv1a(const char *text, size_t size)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ (uint8_t)text[i]) * 16777619u;
    return hash;
}

typedef struct block { char kind[128]; const char *body; size_t size; } block;

static const char *marker(const char *p)
{
    for (; *p; ++p) if ((p[0] == '@' && strncmp(p,"@@@ ",4) == 0)) return p;
    return NULL;
}

/* Bounded normalized patch input; markers must start a line. */
static int next_block(const char *patch, const char **cursor, block *b)
{
    const char *m = marker(*cursor);
    while (m && m != patch && m[-1] != '\n') m = marker(m + 1);
    if (!m) return 0;
    const char *eol = strchr(m, '\n');
    size_t kind_size = eol ? (size_t)(eol - (m + 4)) : strlen(m + 4);
    if (!kind_size || kind_size >= sizeof(b->kind)) return -1;
    memcpy(b->kind, m + 4, kind_size); b->kind[kind_size] = 0;
    if (!eol) { b->body = m + 4 + kind_size; b->size = 0; *cursor = b->body; return 1; }
    b->body = eol + 1;
    const char *end = marker(b->body);
    while (end && end != patch && end[-1] != '\n') end = marker(end + 1);
    if (!end) end = b->body + strlen(b->body);
    b->size = (size_t)(end - b->body);
    if (b->size && b->body[b->size - 1] == '\n') --b->size;
    *cursor = end;
    return 1;
}

static char *block_string(const block *b)
{
    char *s = malloc(b->size + 1);
    if (s) { memcpy(s, b->body, b->size); s[b->size] = 0; }
    return s;
}

static int ident(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
        || (ch >= '0' && ch <= '9') || ch == '_';
}

static int function_span(const char *source, const char *name, const char **start, const char **end)
{
    size_t n = strlen(name);
    if (!n) return 0;
    for (const char *p = strstr(source,name); p; p = strstr(p + 1,name)) {
        if ((p != source && p[-1] != '\n') || ident(p[n])) continue;
        const char *q = p + n;
        while (*q == ' ' || *q == '\t') ++q;
        if (*q != '(') continue;
        int depth = 0;
        for (; *q; ++q) {
            if (q[0] == '/' && q[1] == '/') {
                while (*q && *q != '\n') ++q;
                if (!*q) return 0;
            } else if (q[0] == '/' && q[1] == '*') {
                q = strstr(q + 2,"*/"); if (!q) return 0; ++q;
            } else if (*q == '"') {
                for (++q; *q && *q != '"'; ++q) if (*q == '\\' && q[1]) ++q;
                if (!*q) return 0;
            } else if (*q == '{') {
                ++depth;
            } else if (*q == '}' && depth > 0 && --depth == 0) {
                *start = p; *end = q + 1; return 1;
            }
        }
        return 0;
    }
    return 0;
}

static int replace_region(char **source, size_t *size, size_t offset, size_t remove,
                           const char *insert, size_t insert_size)
{
    if (offset > *size || remove > *size - offset || insert_size > SOURCE_LIMIT
        || *size - remove > SOURCE_LIMIT - insert_size) return 0;
    size_t length = *size - remove + insert_size;
    char *out = malloc(length + 1);
    if (!out) return 0;
    memcpy(out, *source, offset); memcpy(out + offset, insert, insert_size);
    memcpy(out + offset + insert_size, *source + offset + remove, *size - offset - remove + 1);
    free(*source); *source = out; *size = length;
    return 1;
}

static int substitute(char **source, size_t *size, const char *func, const char *from, const char *to)
{
    const char *start, *end;
    size_t from_size = strlen(from), to_size = strlen(to);
    if (!from_size || !function_span(*source,func,&start,&end)) return 0;
    size_t begin = (size_t)(start - *source), finish = (size_t)(end - *source), changed = 0;
    int leading = ident(from[0]), trailing = ident(from[from_size - 1]);
    for (size_t at = begin; at < finish;) {
        if (finish - at >= from_size && memcmp(*source + at,from,from_size) == 0
            && !(leading && at && ident((*source)[at-1]))
            && !(trailing && ident((*source)[at+from_size]))) {
            if (!replace_region(source,size,at,from_size,to,to_size)) return 0;
            finish = finish - from_size + to_size; at += to_size; ++changed;
        } else ++at;
    }
    return changed != 0;
}

/* All operations must parse and match; caller discards the work buffer on any
 * failure. No partially modified text is ever returned to the engine compiler.
 */
static int apply_patch(char **source, size_t *size, const char *patch, unsigned *applied)
{
    const char *cursor = patch;
    block op, value, end;
    *applied = 0;
    int status;
    while ((status = next_block(patch,&cursor,&op)) == 1) {
        int replace = strcmp(op.kind,"replace") == 0;
        int append = strcmp(op.kind,"append") == 0;
        int subst = strncmp(op.kind,"subst ",6) == 0 && op.kind[6];
        if ((!replace && !append && !subst) || !op.size) return 0;
        char *from = block_string(&op), *to = NULL;
        if (!from) return 0;
        if (!append) {
            if (next_block(patch,&cursor,&value) != 1
                || strcmp(value.kind,replace ? "with" : "to") != 0) { free(from); return 0; }
            to = block_string(&value);
            if (!to) { free(from); return 0; }
        }
        if (next_block(patch,&cursor,&end) != 1 || strcmp(end.kind,"end") != 0) {
            free(from); free(to); return 0;
        }
        int ok = 0;
        if (replace) {
            const char *at = strstr(*source,from);
            if (at && !strstr(at + 1,from)) ok = replace_region(source,size,(size_t)(at - *source),strlen(from),to,strlen(to));
        } else if (append) {
            size_t n = strlen(from);
            char *text = malloc(n + 3);
            if (text) {
                text[0] = '\n'; memcpy(text + 1,from,n); text[n+1] = '\n'; text[n+2] = 0;
                ok = replace_region(source,size,*size,0,text,n+2); free(text);
            }
        } else ok = substitute(source,size,op.kind + 6,from,to);
        free(from); free(to);
        if (!ok) return 0;
        ++*applied;
    }
    return status == 0 && *applied != 0;
}

static char *to_hunk(const char *text, size_t size)
{
    if (size > SOURCE_LIMIT) return NULL;
    char *out = ((hunk_fn)(uintptr_t)0x0040eed0u)((int)size + 1);
    if (out) { memcpy(out,text,size); out[size] = 0; }
    return out;
}

static char *zone_source(const char *ext, size_t *size)
{
    void *raw = ((db_find_fn)(uintptr_t)0x004f6f10u)(RAWFILE_TYPE,ext);
    if (((db_default_fn)(uintptr_t)0x004c32b0u)(RAWFILE_TYPE,ext)) return NULL;
    if (!raw) return NULL;
    int bytes = ((raw_length_fn)(uintptr_t)0x004e3080u)(raw);
    if (bytes <= 0 || (unsigned)bytes > SOURCE_LIMIT) return NULL;
    char *buf = ((hunk_fn)(uintptr_t)0x0040eed0u)(bytes);
    if (!buf) return NULL;
    ((raw_buffer_fn)(uintptr_t)0x004e55f0u)(raw,buf,bytes);
    const char *nul = memchr(buf,0,(size_t)bytes);
    if (!nul) return NULL;
    *size = (size_t)(nul - buf);
    return buf;
}

static char *__cdecl __attribute__((force_align_arg_pointer,noinline))
add_source_buffer(const char *filename, const char *ext, const char *code_pos, int archive)
{
    (void)filename; (void)code_pos; (void)archive;
    InterlockedIncrement(&source_requests);
    wchar_t safe[NAME_LIMIT + 1];
    if (!safe_name(ext,safe)) {
        InterlockedIncrement(&failed_requests); record("refused unsafe/overlong script name"); return NULL;
    }
    char *own = NULL;
    size_t size = 0;
    int own_status = read_mod_file(L"scripts",safe,L"",&own,&size);
    if (own_status == -1) {
        InterlockedIncrement(&failed_requests); record("source=%s refused own-file path/read",ext); return NULL;
    }
    if (own_status == 1) {
        const char *selected = own;
        if (_wcsicmp(safe,L"maps\\_mw2trio_probe.gsc") == 0) {
            selected = guarded_own_source(safe,own,&size,
                runtime184_body_channel_ready(),bodyassets184_source_enabled());
            if (selected != own) record("source=%s body-source=baseline missing-channel-or-loader",ext);
        }
        char *result = selected ? to_hunk(selected,size) : NULL;
        if (result) InterlockedIncrement(&own_sources); else InterlockedIncrement(&failed_requests);
        record("source=%s own bytes=%u fnv1a=%08lx accepted=%d",ext,(unsigned)size,
            selected ? (unsigned long)fnv1a(selected,size) : 0ul,result != NULL);
        free(own); return result;
    }
    char *zone = zone_source(ext,&size);
    if (!zone) { record("source=%s native zone/default unavailable",ext); return NULL; }
    char *patch = NULL;
    size_t patch_size = 0;
    int patch_status = read_mod_file(L"patches",safe,L".patch",&patch,&patch_size);
    if (patch_status == -1) {
        InterlockedIncrement(&failed_requests); record("source=%s refused patch path/read",ext); return NULL;
    }
    if (patch_status == 0) {
        record("source=%s native bytes=%u fnv1a=%08lx",ext,(unsigned)size,(unsigned long)fnv1a(zone,size));
        return zone;
    }
    char *work = malloc(size + 1);
    if (!work) { free(patch); InterlockedIncrement(&failed_requests); return NULL; }
    size_t normalized = 0;
    for (size_t i = 0; i < size; ++i) if (zone[i] != '\r') work[normalized++] = zone[i];
    work[normalized] = 0;
    unsigned applied;
    int ok = apply_patch(&work,&normalized,patch,&applied);
    free(patch);
    char *result = ok ? to_hunk(work,normalized) : NULL;
    if (result) {
        InterlockedIncrement(&patched_sources);
        InterlockedExchangeAdd(&applied_blocks,(LONG)applied);
    } else InterlockedIncrement(&failed_requests);
    record("source=%s patch accepted=%d blocks=%u bytes=%u fnv1a=%08lx%s",ext,result != NULL,applied,
        (unsigned)normalized,(unsigned long)fnv1a(work,normalized),result ? "" : " REFUSED_PARTIAL_OR_INVALID_PATCH");
    free(work); return result;
}

static void init_report(scripts184_report *r)
{
    memset(r,0,sizeof(*r)); r->version = SCRIPTS184_API_VERSION; r->bytes = sizeof(*r);
}

int __cdecl scripts184_observe(scripts184_report *r)
{
    if (!r) return SCRIPTS184_BAD_ARGUMENT;
    init_report(r);
    r->installed = (uint32_t)InterlockedCompareExchange(&installed,0,0);
    r->fatal = (uint32_t)InterlockedCompareExchange(&fatal_state,0,0);
    r->source_requests = (uint32_t)InterlockedCompareExchange(&source_requests,0,0);
    r->own_sources = (uint32_t)InterlockedCompareExchange(&own_sources,0,0);
    r->patched_sources = (uint32_t)InterlockedCompareExchange(&patched_sources,0,0);
    r->failed_requests = (uint32_t)InterlockedCompareExchange(&failed_requests,0,0);
    r->applied_patch_blocks = (uint32_t)InterlockedCompareExchange(&applied_blocks,0,0);
    return r->result = r->fatal ? SCRIPTS184_FATAL_RESTORE_FAILED : r->installed ? SCRIPTS184_OK : SCRIPTS184_DISABLED;
}

int __cdecl scripts184_install_preengine(const scripts184_options *o, scripts184_report *r)
{
    if (!r) return SCRIPTS184_BAD_ARGUMENT;
    init_report(r);
    if (!o || o->version != SCRIPTS184_API_VERSION || o->bytes != sizeof(*o)
        || !o->module || !o->main_thread_id || o->pre_engine_invocation != 1) return r->result = SCRIPTS184_BAD_ARGUMENT;
    HMODULE actual = NULL;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)(uintptr_t)&scripts184_install_preengine,&actual) || actual != o->module)
        return r->result = SCRIPTS184_BAD_ARGUMENT;
    if (GetCurrentThreadId() != o->main_thread_id) return r->result = SCRIPTS184_WRONG_THREAD;
    if (!ordinary_enable(o->module)) return r->result = SCRIPTS184_DISABLED;
    if (!o->enter_quiescence || !o->leave_quiescence) return r->result = SCRIPTS184_NO_QUIESCENCE;
    if (InterlockedCompareExchange(&attempted,1,0)) return r->result = SCRIPTS184_ALREADY_ATTEMPTED;
    bootstrap_report identity;
    if ((uintptr_t)GetModuleHandleW(NULL) != 0x00400000u
        || bootstrap_guard_current(&identity,sizeof(identity)) != 1
        || identity.flags != MW2TRIO_GUARD_ALL || !initial_zero_level()) return r->result = SCRIPTS184_WRONG_BUILD;
    if (!check_signatures(r)) return r->result = SCRIPTS184_BYTES_DIFFER;
    if (!o->enter_quiescence(o->quiescence_context)) return r->result = SCRIPTS184_NO_QUIESCENCE;
    int result = SCRIPTS184_BYTES_DIFFER;
    if (check_signatures(r) && initial_zero_level()) {
        HMODULE reference = NULL;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCWSTR)(uintptr_t)&scripts184_install_preengine,&reference) || reference != o->module) {
            r->win32_error = GetLastError(); result = SCRIPTS184_BAD_ARGUMENT;
        } else {
            uint8_t jump[5] = {0xe9};
            uint32_t delta = (uint32_t)(uintptr_t)add_source_buffer - (uint32_t)(SOURCE_VA + 5);
            memcpy(jump + 1,&delta,4);
            DWORD old, unused;
            if (!VirtualProtect((void *)SOURCE_VA,5,PAGE_EXECUTE_READWRITE,&old)) {
                r->win32_error = GetLastError(); result = SCRIPTS184_TRANSACTION_FAILED;
            } else {
                memcpy((void *)SOURCE_VA,jump,5);
                int flushed = FlushInstructionCache(GetCurrentProcess(),(void *)SOURCE_VA,5);
                int restored = VirtualProtect((void *)SOURCE_VA,5,old,&unused);
                MEMORY_BASIC_INFORMATION mi;
                restored = restored && VirtualQuery((void *)SOURCE_VA,&mi,sizeof(mi)) == sizeof(mi) && mi.Protect == old;
                if (flushed && restored && memcmp((void *)SOURCE_VA,jump,5) == 0) {
                    installed = 1; r->installed = 1; r->protection_restored = 1; result = SCRIPTS184_OK;
                } else {
                    r->win32_error = GetLastError(); result = SCRIPTS184_FATAL_RESTORE_FAILED;
                    if (VirtualProtect((void *)SOURCE_VA,5,PAGE_EXECUTE_READWRITE,&unused)) {
                        memcpy((void *)SOURCE_VA,signatures[0].bytes,5);
                        flushed = FlushInstructionCache(GetCurrentProcess(),(void *)SOURCE_VA,5);
                        restored = VirtualProtect((void *)SOURCE_VA,5,old,&unused);
                        restored = restored && VirtualQuery((void *)SOURCE_VA,&mi,sizeof(mi)) == sizeof(mi) && mi.Protect == old;
                        r->protection_restored = restored;
                        if (flushed && restored && memcmp((void *)SOURCE_VA,signatures[0].bytes,5) == 0)
                            result = SCRIPTS184_TRANSACTION_FAILED;
                    }
                    if (result == SCRIPTS184_FATAL_RESTORE_FAILED) { fatal_state = 1; r->fatal = 1; }
                }
            }
        }
    }
    o->leave_quiescence(o->quiescence_context);
    record("install result=%d signatures=%u protection_restored=%u fatal=%u; no cooperative player claim",
        result,r->checked_signatures,r->protection_restored,r->fatal);
    return r->result = result;
}
