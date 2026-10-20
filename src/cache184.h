#pragma once
#include <windows.h>
#include <stdint.h>

/* Dormant cache-only experiment, enabled explicitly by the startup integration.
   Three cache records do not authorize slot2, connections, UID/roster/wire/UI.
   Parent must call observe on every owner-thread frame and stop on fatal/errors.
   Never unload while installed/fatal; this module pins its code before writes. */
#define CACHE184_API_VERSION 1u
#define CACHE184_OPERAND_COUNT 31u
#define CACHE184_SITE_COUNT 32u /* 31 address/end operands + constructor reset CALL */
#define CACHE184_PREENGINE_EPOCH 1u
#define CACHE184_ALLOCATION_COUNT 3u
#define CACHE184_TRAVERSAL_COUNT 2u /* FUTURE expansion needs shared roster API */

enum cache184_result {
    CACHE184_OK=1, CACHE184_DISABLED=0, CACHE184_BAD_ARGUMENT=-1,
    CACHE184_WRONG_THREAD=-2, CACHE184_WRONG_BUILD=-3, CACHE184_NOT_PREENGINE=-4,
    CACHE184_NO_QUIESCENCE=-5, CACHE184_BYTES_DIFFER=-6, CACHE184_ALLOCATION_FAILED=-7,
    CACHE184_TRANSACTION_FAILED=-8, CACHE184_ALREADY_ATTEMPTED=-9,
    CACHE184_FATAL_RESTORE_FAILED=-10, CACHE184_GUARD_CORRUPTION=-11,
    CACHE184_FATAL_RESET_CONTRACT=-12
};
enum cache184_reset_phase {
    CACHE184_RESET_NONE=0, CACHE184_RESET_ENTERED=1, CACHE184_RESET_ORIGINAL_DONE=2,
    CACHE184_RESET_CACHE_ZEROED=3, CACHE184_RESET_COMPLETE=4, CACHE184_RESET_REFUSED=5
};
enum cache184_reset_failure {
    CACHE184_RESET_FAILURE_NONE=0, CACHE184_RESET_FAILURE_HOST=1,
    CACHE184_RESET_FAILURE_THREAD=2, CACHE184_RESET_FAILURE_CALLER=3,
    CACHE184_RESET_FAILURE_ARGS=4, CACHE184_RESET_FAILURE_GUARD_BEFORE=5,
    CACHE184_RESET_FAILURE_RETURN=6, CACHE184_RESET_FAILURE_GUARD_AFTER=7
};
/* markers: 1 trusted+installed, 2 owner tid, 4 caller, 8 exact args,
   16 guard-before, 32 original return preserved, 64 guard-after. */
typedef int (__cdecl *cache184_enter_quiescence)(void *);
typedef void (__cdecl *cache184_leave_quiescence)(void *);
typedef struct cache184_options {
    uint32_t version, bytes;
    HMODULE module;
    DWORD main_thread_id;
    uint32_t preengine_epoch; /* 1, attested first wrapper before original WinMain */
    cache184_enter_quiescence enter_quiescence;
    cache184_leave_quiescence leave_quiescence;
    void *quiescence_context;
} cache184_options;
typedef struct cache184_report {
    uint32_t version, bytes;
    int32_t result;
    uint32_t installed, fatal, failed_site, win32_error, checked_sites;
    uint32_t protections_restored, guards_ok, cache_base, cache_bytes;
    uint32_t allocation_count, traversal_count;
    uint32_t reset_calls, reset_phase, reset_markers, reset_failure;
    uint32_t reset_tid, reset_caller, reset_destination, reset_value, reset_bytes;
    uint32_t runtime_observed;
} cache184_report;

/* Requires regular mw2-trio-cache-test.enable, exact full EXE/file+live signatures,
   initial level globals/local1/coop0, and verified pre-WinMain quiescence callbacks.
   No engine calls or thread waits/suspends occur. Ordinary refusal writes nothing.
   Fatal(-10) retains allocation/code: parent must stop before original WinMain. */
/* Constructor reset accepts only exact original caller/dst/zero/0x7C on owner
   tid, preserves cdecl return pointer and clears all three new records separately.
   Runtime mismatch/corruption logs cache184-fatal.log and stops only the exact
   previously guarded host with exit0xE1840040; it never clears arbitrary memory. */
int __cdecl cache184_install_preengine(const cache184_options *, cache184_report *);
int __cdecl cache184_observe(cache184_report *);
/* Only before ANY constructor reset/observer frame and with the same verified
   preengine barrier. Never roll back after pointers can escape to the engine/UI. */
int __cdecl cache184_rollback_preengine(const cache184_options *, cache184_report *);
