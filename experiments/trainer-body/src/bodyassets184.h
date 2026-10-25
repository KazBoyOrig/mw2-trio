/* Own MIT private default-OFF resource prototype. */
#pragma once
#include <windows.h>
#include <stdint.h>
typedef struct bodyassets184_report {
    uint32_t version,requested,attempted,installed,fatal,validation_bits;
    uint32_t failed_guard,missing_contract,epoch,ready_epoch;
    int32_t result;
    uint32_t proof_mask,proof_failure,probe_status,probe_step,probe_pass;
} bodyassets184_report;
enum bodyassets184_proof_failure { BA184_DETAIL_OK=0,BA184_DETAIL_LIVE=1,
    BA184_DETAIL_PATH=2,BA184_DETAIL_FS=3,BA184_DETAIL_DB_BEFORE=4,
    BA184_DETAIL_SO_DVAR=5,BA184_DETAIL_SO_VALUE=6,BA184_DETAIL_COOP_READ=7,
    BA184_DETAIL_COOP_VALUE=8,BA184_DETAIL_MAP_DVAR=9,BA184_DETAIL_MAP_TEXT=10,
    BA184_DETAIL_MAP_NAME=11,BA184_DETAIL_PROBE=12,BA184_DETAIL_DB_AFTER=13,
    BA184_DETAIL_INIT_READ=14,BA184_DETAIL_INIT_VALUE=15,BA184_DETAIL_INIT_STAGE=16,
    BA184_DETAIL_DB_DRIFT=17,BA184_DETAIL_INIT_AFTER=18,BA184_DETAIL_INIT_DRIFT=19,
    BA184_DETAIL_QUERY_CONTEXT=20,BA184_DETAIL_WORKER_WAIT=21 };
/* Completed owner mainframe only; no load or game command is issued here.
   Default OFF. This private adapter has not been exercised in the game. */
int __cdecl bodyassets184_tick(DWORD initial_thread_id);
void __cdecl bodyassets184_observe(bodyassets184_report *);
int __cdecl bodyassets184_ready_now(void);
/* Own source selection only: substituted loader returned in the current
   generation, owner thread, installed channel, live guards and held seals.
   Does not inspect typed assets or promote the early readiness latch. */
int __cdecl bodyassets184_source_enabled(void);
