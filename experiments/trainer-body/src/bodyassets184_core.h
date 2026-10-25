/* Own MIT offline-testable argument wrapper core. No addresses are dereferenced
   and no engine, OS, provider, filesystem or socket function is called here. */
#ifndef BODYASSETS184_CORE_H
#define BODYASSETS184_CORE_H
#include <stddef.h>
#include <stdint.h>
#include "bodyassets184_policy.h"
#define BA184_CALL_VA 0x00482cd9u
#define BA184_RETURN_VA 0x00482cdeu
#define BA184_ORIGINAL_VA 0x004d0450u
enum ba184_result { BA184_DISABLED=0, BA184_OK=1, BA184_REFUSED=-1,
    BA184_BAD_ARGUMENT=-2,BA184_WRONG_THREAD=-3,BA184_GUARD=-4,BA184_PIN=-5,
    BA184_PATCH=-6,BA184_FATAL=-7,BA184_CALLER=-8,BA184_REENTRY=-9,
    BA184_LIFETIME=-10,BA184_PROOF=-11,BA184_PUBLISH=-12,
    BA184_MISSING_PRESCRIPT_CONTRACT=-13 };
enum ba184_validation {
    BA184_IMAGE=1u,BA184_LIVE=2u,BA184_FLAG=4u,BA184_FILES=8u,
    BA184_LANGUAGE=16u,BA184_PATH=32u,BA184_OWNER_PHASE=64u,
    BA184_PRESCRIPT_BARRIER=128u,BA184_OWN_CHANNEL=256u
};
#define BA184_ALL_VALIDATION 511u
enum ba184_phase { BA184_INSTALL=1,BA184_ENTRY=2,BA184_RETURN=3,BA184_FRAME=4 };
enum ba184_proof_bits {
    BA184_TYPED_BODY=1u,BA184_TYPED_HEAD=2u,BA184_BODY_90=4u,BA184_HEAD_24=8u,
    BA184_SO_ZONE4=16u,BA184_TRAINER_ZONE4=32u,BA184_SAME_SO_SOURCE=64u,
    BA184_ADDON_ZERO=128u,BA184_CAMPAIGN_TRAINER=256u,
    BA184_DB_STABLE=512u,BA184_PRE_SCRIPT=1024u
};
#define BA184_ALL_PROOF 2047u
enum ba184_proof_stage { BA184_RESIDENT=1,BA184_REGISTERED=2 };
typedef struct ba184_proof {uint32_t bits,epoch,stage;} ba184_proof;
typedef struct ba184_ops {
    uint32_t (*validate)(void *,unsigned phase,unsigned installed);
    int (*pin)(void *);
    /* 1 exact install+protection restored,0 exact unchanged/rolled back,
       negative uncertain. Must check all five expected bytes first. */
    int (*patch)(void *,const unsigned char before[5],const unsigned char after[5]);
    int (*original)(void *,const char *name,int second);
    int (*publish)(void *,uint32_t epoch,uint32_t ready_epoch);
    int (*proof)(void *,uint32_t epoch,unsigned stage,ba184_proof *);
} ba184_ops;
typedef struct ba184_state {
    const ba184_ops *ops;void *opaque;uint32_t owner_tid;
    uint32_t attempted,installed,pinned,fatal,busy,epoch,ready_epoch;
    uint32_t returned,candidate,early_epoch,revoked;
    uint32_t calls,substitutions,passthrough,refused,proofs,proof_stage;
    int32_t result;unsigned char replacement[5];
} ba184_state;
void ba184_initialize(ba184_state *,const ba184_ops *,void *,uint32_t owner_tid);
int ba184_install(ba184_state *,uint32_t tid,unsigned enabled,const unsigned char replacement[5]);
int ba184_call(ba184_state *,uint32_t tid,uintptr_t caller,const char *name,size_t readable_bytes,
              int second,unsigned coop,unsigned specialops_valid,unsigned specialops,unsigned fs_empty);
int ba184_observe(ba184_state *,uint32_t tid);
/* Synchronous read-boundary capability. First promotion requires level_init1;
   later init0 reads require same early-latched generation and fresh proof. */
int ba184_ready(ba184_state *,uint32_t tid,unsigned level_init);
int ba184_source_enabled(ba184_state *,uint32_t tid);
#endif
