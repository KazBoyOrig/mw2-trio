#pragma once
#include <windows.h>
#include <stdint.h>

/* Dormant, offline storage experiment; no scheduler, hook, or automatic entry.
 * Compile separately until reviewed. No network/player-connection support.
 * The caller must pin the containing DLL for the process lifetime if installed.
 */
#define STORAGE184_API_VERSION 1u
#define STORAGE184_SITE_COUNT 20u

enum storage184_result {
    STORAGE184_OK = 1,
    STORAGE184_DISABLED = 0,
    STORAGE184_BAD_ARGUMENT = -1,
    STORAGE184_WRONG_THREAD = -2,
    STORAGE184_WRONG_BUILD = -3,
    STORAGE184_NOT_INITIAL_MENU = -4,
    STORAGE184_NO_QUIESCENCE = -5,
    STORAGE184_BYTES_DIFFER = -6,
    STORAGE184_ALLOCATION_FAILED = -7,
    STORAGE184_TRANSACTION_FAILED = -8,
    STORAGE184_ALREADY_ATTEMPTED = -9,
    STORAGE184_FATAL_RESTORE_FAILED = -10,
    STORAGE184_GUARD_CORRUPTION = -11
};

/* A successful enter must stop engine workers which can access these tables,
 * and prevent new readers until leave. A verified static-load wrapper before
 * the first original WinMain/engine initialization can also establish this
 * condition; static-load reserved != NULL alone cannot. Menu globals alone
 * are not a barrier.
 * The module refuses installation without these callbacks. They must not call
 * the game main frame or begin map loading. No worker suspension is performed.
 */
typedef int (__cdecl *storage184_enter_quiescence)(void *context);
typedef void (__cdecl *storage184_leave_quiescence)(void *context);

typedef struct storage184_options {
    uint32_t version, bytes;
    HMODULE module;
    DWORD main_thread_id;
    uint32_t diagnostic_tick; /* First diagnostic entry only: 1, never a later retry. */
    storage184_enter_quiescence enter_quiescence;
    storage184_leave_quiescence leave_quiescence;
    void *quiescence_context;
} storage184_options;

typedef struct storage184_report {
    uint32_t version, bytes;
    int32_t result;
    uint32_t installed, failed_site, win32_error, checked_sites;
    uint32_t protections_restored, guards_ok, record_pointer_match_mask;
    uint32_t allocated_client_base, allocated_metadata_base;
    uint32_t observed_capacity, observed_client_base, observed_local_count;
    uint32_t observed_coop_enabled, entity_client[3];
} storage184_report;

/* Requires regular mw2-trio-storage-test.enable beside options.module, exact
 * executable hash + all 20 live instruction signatures, first main-thread menu
 * tick, native client-base/count == 0, coop == 0, local player count == 1, and
 * caller-provided worker quiescence. Refusal makes no game code/data writes.
 * Successful installation changes only address/size/count code operands.
 * No claim of complete metadata relocation follows from this experiment.
 */
int __cdecl storage184_install_menu(const storage184_options *, storage184_report *);

/* Call on every main-thread diagnostic tick while installed, including during
 * map startup. Read-only game snapshot and canary checks. A match mask of 7 shows three separate
 * entity.client pointers into allocated storage; it does not show three active
 * players. No game helper or engine calling convention is used here.
 */
int __cdecl storage184_observe(storage184_report *);

/* Explicit rollback is allowed only before map initialization, on the same
 * thread and under the same quiescence contract. Observation of any nonzero
 * level capacity/base latches a permanent no-rollback state. Never roll back after a map
 * has used the relocated records, or unload the module while installed/fatal.
 */
int __cdecl storage184_rollback_menu(const storage184_options *, storage184_report *);
