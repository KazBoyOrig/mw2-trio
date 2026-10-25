#pragma once
#include <windows.h>
#include <stdint.h>

/* Inactive x86 source-loader experiment. This API has no scheduler and must be
 * called explicitly from a verified wrapper before the FIRST original WinMain.
 * A static-load notification or initialized menu alone is not that guarantee.
 */
#define SCRIPTS184_API_VERSION 1u

enum scripts184_result {
    SCRIPTS184_OK = 1,
    SCRIPTS184_DISABLED = 0,
    SCRIPTS184_BAD_ARGUMENT = -1,
    SCRIPTS184_WRONG_THREAD = -2,
    SCRIPTS184_WRONG_BUILD = -3,
    SCRIPTS184_NO_QUIESCENCE = -4,
    SCRIPTS184_BYTES_DIFFER = -5,
    SCRIPTS184_TRANSACTION_FAILED = -6,
    SCRIPTS184_ALREADY_ATTEMPTED = -7,
    SCRIPTS184_FATAL_RESTORE_FAILED = -8
};

typedef int (__cdecl *scripts184_enter_quiescence)(void *context);
typedef void (__cdecl *scripts184_leave_quiescence)(void *context);

typedef struct scripts184_options {
    uint32_t version, bytes;
    HMODULE module;
    DWORD main_thread_id;
    uint32_t pre_engine_invocation; /* Exactly 1, before any original WinMain. */
    scripts184_enter_quiescence enter_quiescence;
    scripts184_leave_quiescence leave_quiescence;
    void *quiescence_context;
} scripts184_options;

typedef struct scripts184_report {
    uint32_t version, bytes;
    int32_t result;
    uint32_t installed, fatal, checked_signatures, win32_error;
    uint32_t protection_restored;
    uint32_t source_requests, own_sources, patched_sources, failed_requests;
    uint32_t applied_patch_blocks;
} scripts184_report;

/* Requires exact hash/base/signatures, ordinary mw2-trio-scripts-test.enable,
 * same initial thread, initial zero level state and caller-proven pre-engine
 * quiescence. Pins the module before changing the single loader JMP.
 * Caller's quiescence callbacks must NOT call the engine or start map loading.
 * Fatal restore failure requires stopping this OWN isolated process before
 * original WinMain. Never unload or unhook after source loading has begun.
 */
int __cdecl scripts184_install_preengine(const scripts184_options *, scripts184_report *);

/* Counter snapshot only, safe before installation; no game helper calls.
 * Script player count is supplied by the separate GSC probe, not inferred here.
 */
int __cdecl scripts184_observe(scripts184_report *);
