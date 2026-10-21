/* Own MIT experiment; adapted concepts/string from stxlvn/mw2_coop (MIT).
 * Explicit pre-engine startup integration; no complete gameplay claim. */
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#define RUNTIME184_API_VERSION 1u
#define RUNTIME184_PATCH_COUNT 5u
enum runtime184_result {
    RUNTIME184_OK=1, RUNTIME184_DISABLED=0, RUNTIME184_BAD_ARGUMENT=-1,
    RUNTIME184_WRONG_THREAD=-2, RUNTIME184_WRONG_BUILD=-3,
    RUNTIME184_NOT_PREENGINE=-4, RUNTIME184_NO_QUIESCENCE=-5,
    RUNTIME184_BYTES_DIFFER=-6, RUNTIME184_ALLOCATION_FAILED=-7,
    RUNTIME184_TRANSACTION_FAILED=-8, RUNTIME184_ALREADY_ATTEMPTED=-9,
    RUNTIME184_FATAL_RESTORE_FAILED=-10
};
typedef int (__cdecl *runtime184_enter_quiescence)(void *);
typedef void (__cdecl *runtime184_leave_quiescence)(void *);
typedef struct runtime184_options {
    uint32_t version, bytes;
    HMODULE module;
    DWORD main_thread_id;
    uint32_t diagnostic_tick; /* exactly first verified pre-WinMain entry: 1 */
    runtime184_enter_quiescence enter_quiescence;
    runtime184_leave_quiescence leave_quiescence;
    void *quiescence_context;
} runtime184_options;
typedef struct runtime184_report {
    uint32_t version, bytes;
    int32_t result;
    uint32_t installed, checked_sites, failed_site, win32_error, protections_restored;
    uint32_t flags_changed, external_found_mask, external_readable_mask; /* bits: start/god/qa/qa_phase */
    uint32_t script_sets, localized_hits, script_error_entries, diagnostic_drops; /* sets counts normal setter callback returns, including uninitialized no-op */
    uint32_t last_error_caller, last_error_message_pointer;
    char state[256], qa_log[256], command[256], cached_error_message[256];
} runtime184_report;

/* Requires ordinary mw2-trio-two-player-test.enable AND control.enable beside
 * the module, full EXE guard, initial/main tid, first pre-WinMain callbacks and
 * all full instruction/table guards. No engine call during install/DllMain.
 * Fatal -10: retain code/allocation; parent must stop before original WinMain.
 * Once installed there is no unloading/rollback after engine initialization.
 * This does not create peers, synchronize dvar values or implement progression. */
int __cdecl runtime184_install_preengine(const runtime184_options *,runtime184_report *);
/* Same owner main thread every frame. Adds visibility flags; reads raw script
 * state/QA/command dvars and bounded error diagnostics. No request is executed.
 * Negative result while installed is fatal to the parent's isolated test. */
int __cdecl runtime184_tick(runtime184_report *);
