/* Own MIT bounded typed DB reader. Supplied read callback only. */
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int (*ba184_read_fn)(void *,uintptr_t,void *,size_t);
enum ba184_probe_status { BA184_PROBE_NONE=0,BA184_PROBE_OK=1,BA184_PROBE_ARGUMENT=2,
    BA184_PROBE_RANGE=3,BA184_PROBE_BUDGET=4,BA184_PROBE_READ=5,
    BA184_PROBE_PARSE=6,BA184_PROBE_DIGEST=7 };
enum ba184_probe_step { BA184_STEP_COUNT=1,BA184_STEP_ADDON=2,BA184_STEP_IDS=3,
    BA184_STEP_ZONE_ID=4,BA184_STEP_ZONE_RECORD=5,BA184_STEP_ZONE_NAME=6,
    BA184_STEP_ZONE_ALLOC=7,BA184_STEP_ZONE_SET=8,BA184_STEP_HASH=9,
    BA184_STEP_CHAIN=10,BA184_STEP_ENTRY=11,BA184_STEP_HEADER=12,
    BA184_STEP_NAME=13,BA184_STEP_SOURCE=14,BA184_STEP_BONES=15,
    BA184_STEP_FLAGS=16,BA184_STEP_ALIAS=17,BA184_STEP_TABLE=18,
    BA184_STEP_BODY_REGISTERED=19,BA184_STEP_HEAD_REGISTERED=20 };
typedef struct ba184_probe_diagnostic {uint32_t status,step,pass,asset,calls,bytes;} ba184_probe_diagnostic;
/* Two independent bounded passes must have identical observation digests.
   Returns proof bits for exact body90/head24 in activeSO4 plus trainer4/addon0.
   This establishes neither engine synchronization nor script phase by itself. */
/* BA184_RESIDENT allows exact mark0/1 before native registration.
   BA184_REGISTERED requires exact mark1 and both headers in slots1..511. */
uint32_t ba184_probe(ba184_read_fn,void *,unsigned stage);
uint32_t ba184_probe_diagnose(ba184_read_fn,void *,unsigned stage,ba184_probe_diagnostic *);
