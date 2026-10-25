/* Own MIT. This core operates solely through supplied operations. */
#include "bodyassets184_core.h"
#include <string.h>
static const unsigned char original_call[5]={0xe8,0x72,0xd7,0x04,0};
static int fail(ba184_state *s,int reason,int fatal)
{
    if(s){s->result=reason;s->ready_epoch=0;if(fatal)s->fatal=1;}
    return reason;
}
static int validate(ba184_state *s,unsigned phase)
{return s->ops&&s->ops->validate&&s->ops->validate(s->opaque,phase,s->installed)==BA184_ALL_VALIDATION;}
static int publish(ba184_state *s,uint32_t ready)
{
    s->ready_epoch=0;
    if(!s->ops->publish||!s->ops->publish(s->opaque,s->epoch,ready))return fail(s,BA184_PUBLISH,1);
    s->ready_epoch=ready;return BA184_OK;
}
void ba184_initialize(ba184_state *s,const ba184_ops *ops,void *opaque,uint32_t owner)
{if(s){memset(s,0,sizeof(*s));s->ops=ops;s->opaque=opaque;s->owner_tid=owner;}}
int ba184_install(ba184_state *s,uint32_t tid,unsigned enabled,const unsigned char replacement[5])
{
    if(!s||!s->ops||!s->owner_tid)return BA184_BAD_ARGUMENT;
    if(s->fatal)return BA184_FATAL;
    if(!enabled)return BA184_DISABLED;
    if(enabled!=1||!replacement||replacement[0]!=0xe8||!memcmp(replacement,original_call,5))return fail(s,BA184_BAD_ARGUMENT,0);
    if(tid!=s->owner_tid)return fail(s,BA184_WRONG_THREAD,0);
    if(s->attempted||s->installed)return fail(s,BA184_REFUSED,0);
    s->attempted=1;
    uint32_t flags=s->ops->validate?s->ops->validate(s->opaque,BA184_INSTALL,0):0;
    if((flags&(BA184_PRESCRIPT_BARRIER|BA184_OWN_CHANNEL))!=(BA184_PRESCRIPT_BARRIER|BA184_OWN_CHANNEL))
        return fail(s,BA184_MISSING_PRESCRIPT_CONTRACT,0);
    if(flags!=BA184_ALL_VALIDATION)return fail(s,BA184_GUARD,0);
    if(!s->ops->pin||!s->ops->pin(s->opaque))return fail(s,BA184_PIN,0);
    s->pinned=1;
    if(!s->ops->patch||!s->ops->original||!s->ops->proof||!s->ops->publish)return fail(s,BA184_BAD_ARGUMENT,0);
    if(!validate(s,BA184_INSTALL))return fail(s,BA184_GUARD,0);
    memcpy(s->replacement,replacement,5);
    int p=s->ops->patch(s->opaque,original_call,replacement);
    if(p<0)return fail(s,BA184_PATCH,1);
    if(!p)return fail(s,BA184_PATCH,0);
    s->installed=1;
    if(!validate(s,BA184_FRAME)){
        p=s->ops->patch(s->opaque,replacement,original_call);
        if(p!=1)return fail(s,BA184_PATCH,1);
        s->installed=0;return fail(s,BA184_GUARD,0);
    }
    return s->result=BA184_OK;
}
static int observe_proof(ba184_state *s,unsigned stage)
{
    ba184_proof p={0,0,0};++s->proofs;s->proof_stage=stage;
    if(!s->ops->proof(s->opaque,s->epoch,stage,&p)||p.epoch!=s->epoch||p.stage!=stage||p.bits!=BA184_ALL_PROOF)
        return fail(s,BA184_PROOF,1);
    return BA184_OK;
}
int ba184_call(ba184_state *s,uint32_t tid,uintptr_t caller,const char *name,size_t bytes,
              int second,unsigned coop,unsigned valid_so,unsigned so,unsigned fs_empty)
{
    if(!s||!s->installed||s->fatal)return BA184_BAD_ARGUMENT;
    if(tid!=s->owner_tid)return fail(s,BA184_WRONG_THREAD,1);
    if(caller!=BA184_RETURN_VA)return fail(s,BA184_CALLER,1);
    if(s->busy)return fail(s,BA184_REENTRY,1);
    if(!validate(s,BA184_ENTRY))return fail(s,BA184_GUARD,1);
    bodyassets184_input in={1,1,1,1,1,1,fs_empty,coop,valid_so,so};
    int decision=bodyassets184_decide(name,bytes,&in);
    if(decision==BA_INVALID)return fail(s,BA184_BAD_ARGUMENT,1);
    s->busy=1;++s->calls;s->returned=0;s->candidate=0;s->early_epoch=0;s->revoked=0;s->proof_stage=0;
    if(s->epoch==UINT32_MAX)return fail(s,BA184_LIFETIME,1);
    ++s->epoch;
    if(publish(s,0)!=BA184_OK)return s->result;
    const char *argument=name;
    if(decision==BA_SUBSTITUTE_CANDIDATE){argument="so_killspree_trainer";++s->substitutions;s->candidate=1;}
    else{++s->passthrough;if(decision==BA_REFUSED)++s->refused;}
    if(!s->ops->original(s->opaque,argument,second))return fail(s,BA184_LIFETIME,1);
    if(s->fatal)return s->result;
    if(!validate(s,BA184_RETURN))return fail(s,BA184_GUARD,1);
    s->returned=1;s->busy=0;return s->result=BA184_OK;
}
int ba184_observe(ba184_state *s,uint32_t tid)
{
    if(!s)return BA184_BAD_ARGUMENT;
    if(s->fatal)return BA184_FATAL;
    if(!s->installed)return BA184_DISABLED;
    if(tid!=s->owner_tid)return fail(s,BA184_WRONG_THREAD,1);
    if(s->busy)return fail(s,BA184_LIFETIME,1);
    if(!validate(s,BA184_FRAME))return fail(s,BA184_GUARD,1);
    return s->result=BA184_OK;
}
int ba184_ready(ba184_state *s,uint32_t tid,unsigned level_init)
{
    if(!s||!s->installed||s->fatal||s->busy||!s->returned||!s->candidate||!s->epoch||s->revoked)return 0;
    if(tid!=s->owner_tid||level_init>1||!validate(s,BA184_FRAME)){
        s->ready_epoch=0;s->revoked=1;return 0;}
    if(!level_init&&s->early_epoch!=s->epoch)return 0;
    unsigned stage=s->early_epoch==s->epoch?BA184_REGISTERED:BA184_RESIDENT;
    if(observe_proof(s,stage)!=BA184_OK){s->revoked=1;return 0;}
    if(!s->early_epoch)s->early_epoch=s->epoch;
    if(publish(s,s->epoch)!=BA184_OK){s->revoked=1;return 0;}
    return 1;
}
int ba184_source_enabled(ba184_state *s,uint32_t tid)
{
    if(!s||!s->installed||s->fatal||s->busy||!s->returned||!s->candidate||!s->epoch||s->revoked)return 0;
    if(tid!=s->owner_tid||!validate(s,BA184_FRAME)){
        s->ready_epoch=0;s->revoked=1;return 0;}
    return 1;
}
