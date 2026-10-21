/* Own MIT experiment. Separate explicit loopback-QA and exact-peer APIs;
 * neither is authentication or a shipping identity flow. */
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#define QA_IDENTITY184_VERSION 1u
enum qa_identity184_result {
    QA_IDENTITY184_OK=1, QA_IDENTITY184_DISABLED=0,
    QA_IDENTITY184_BAD_ARGUMENT=-1, QA_IDENTITY184_WRONG_THREAD=-2,
    QA_IDENTITY184_WRONG_BUILD=-3, QA_IDENTITY184_BYTES_DIFFER=-4,
    QA_IDENTITY184_NOT_FRESH=-5, QA_IDENTITY184_BAD_UID=-6,
    QA_IDENTITY184_ALREADY_ATTEMPTED=-7, QA_IDENTITY184_PIN_FAILED=-8,
    QA_IDENTITY184_UID_CHANGED=-9, QA_IDENTITY184_SCOPE_LOST=-10
};
typedef struct qa_identity184_options {
    uint32_t version,bytes;
    HMODULE module;
    DWORD main_thread_id;
    uint32_t native_frame_ordinal,client_role,delta,loopback_enforced;
} qa_identity184_options;
typedef struct qa_identity184_report {
    uint32_t version,bytes;
    int32_t result;
    uint32_t attempted,active,fatal,checked_sites,failed_site,failed_precondition;
    uint32_t observations,original_hash,expected_hash,win32_error;
} qa_identity184_report;
/* Explicit owner main-thread CLIENT command only, after native menu readiness,
 * before any private party/member/channel/queue/socket. role/delta/enforced=1.
 * Parent must already guarantee QA transport: only127.0.0.2 destinations and
 * actual socket bound127.0.0.2 before any party action. This module opens no
 * socket and cannot prove those FUTURE adapter actions from its option alone.
 * Requires ordinary loopback-QA/party/control flags next to its containing DLL,
 * exact executable, unmodified identity/validation code and strict fresh state.
 * Changes only cached local slot0 by an aligned expected-value8B CAS, once.
 * No provider/interface/account/auth/identity-validation patch or service call.
 * Negative attempted result requires parent to stop its own isolated process.
 */
int __cdecl qa_identity184_apply_client(const qa_identity184_options *,qa_identity184_report *);
/* Explicit private direct-peer mode. A separate main-thread command supplies
 * its containing-module scope callback, which must verify the latched transport
 * and return its role-derived low DWORD. Native high DWORD is preserved. This
 * is a private participant cache, never a provider/account/authentication API.
 * Callback ownership, flags, exact image/code, empty engine state, expected
 * atomic comparison and lifetime observation remain mandatory.
 */
typedef int (__cdecl *qa_identity184_peer_scope)(uint32_t *low);
typedef struct qa_identity184_peer_options {
    uint32_t version,bytes;
    HMODULE module;
    DWORD main_thread_id;
    uint32_t native_frame_ordinal,role,derived_low;
    qa_identity184_peer_scope scope;
} qa_identity184_peer_options;
int __cdecl qa_identity184_apply_peer(const qa_identity184_peer_options *,qa_identity184_report *);
/* EVERY owner main-thread callback BEFORE opt-out/throttling. Active changes,
 * wrong thread, changed code or lost flags/online/lobby scope are fatal. Parent
 * must stop its own test on a negative result. Never restore/reapply live.
 * Cached UID also affects native local/remote, headset, progress and heartbeat
 * logic; it is not merely a packet label. No saves/progression QA under alias.
 */
int __cdecl qa_identity184_observe(qa_identity184_report *);
