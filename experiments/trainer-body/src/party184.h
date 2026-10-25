/* Own experimental code; MIT. Explicit guarded main-thread commands. */
#pragma once
#include <stdint.h>
#include <windows.h>

enum party184_result {
    PARTY184_NONE = 0, PARTY184_REGISTERED = 1, PARTY184_HOST_CREATED = 2,
    PARTY184_JOIN_INITIALIZED = 3, PARTY184_LAUNCH_REQUESTED = 4,
    PARTY184_CONNECT_ENQUEUED = 5,
    PARTY184_SOCKET_OPENED = 6,
    PARTY184_QA_ALIAS_APPLIED = 7,
    PARTY184_DISABLED = 10, PARTY184_BAD_THREAD = 11, PARTY184_BAD_IMAGE = 12,
    PARTY184_BAD_CODE = 13, PARTY184_NOT_FRESH = 14, PARTY184_BAD_UID = 15,
    PARTY184_NAME_COLLISION = 16, PARTY184_NATIVE_FAILURE = 17
};
typedef struct party184_report {
    uint32_t version, registered, last_result, guard_checks;
    uint32_t host_attempts, join_attempts, launch_attempts, rejected;
} party184_report;

/* Call only on the existing verified engine main-thread tick, after engine init.
 * Pass the thread ID established by the startup/control module, not a worker ID.
 * The DLL and its static command nodes must remain loaded until process exit.
 * Returns 1 when registered, 0 while disabled/not initialized, -1 on guard failure.
 */
int party184_tick(DWORD main_thread_id);
int party184_get_report(party184_report *out, uint32_t bytes);
