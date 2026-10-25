#pragma once
#include <stdint.h>
#include <windows.h>

#define MW2TRIO_REPORT_VERSION 1u
#define MW2TRIO_GUARD_ALL 31u
/* flags: 1 valid PE, 2 x86 PE32, 4 timestamp, 8 build marker, 16 SHA256 */
typedef struct bootstrap_report {
    uint32_t version;
    uint32_t flags;
    uint32_t pe_timestamp;
    uint16_t machine;
    uint16_t optional_magic;
    char sha256[65];
    char build_marker[4];
} bootstrap_report;

int __cdecl bootstrap_guard_current(bootstrap_report *report, uint32_t report_bytes);
int __cdecl bootstrap_guard_file(const wchar_t *path, bootstrap_report *report, uint32_t report_bytes);
unsigned __cdecl bootstrap_export_count(void);
