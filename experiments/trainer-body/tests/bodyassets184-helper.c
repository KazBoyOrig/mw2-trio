/* Own MIT offline fixtures. Only fake-engine callbacks and own allocated inert
   RX bytes; never a game address, provider, socket or native engine call. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "../src/bodyassets184_core.h"
#include "../src/bodyassets184_tx.h"
static unsigned checks,failed;
static void check(int ok,const char *text){++checks;if(!ok){++failed;printf("FAIL %s\n",text);}}
static const unsigned char original[5]={0xe8,0x72,0xd7,4,0};
static const unsigned char replacement[5]={0xe8,0x11,0x22,0x33,0x44};
typedef struct fixture {
    ba184_state state;uint32_t flags,proof_bits,proof_epoch_delta,proof_stage_delta;
    unsigned validate_calls,pins,patches,originals,publishes,proofs,break_phase;
    int pin_ok,patch_result,rollback_result,original_ok,publish_ok,proof_ok,reenter;
    const char *argument;int second;uint32_t external_epoch,external_ready;
    unsigned char site[5];
} fixture;
static uint32_t valid(void *v,unsigned phase,unsigned installed)
{
    fixture *f=v;++f->validate_calls;
    if(f->break_phase==phase)return 0;
    if(memcmp(f->site,installed?replacement:original,5))return 0;
    return f->flags;
}
static int pin(void *v){fixture *f=v;++f->pins;return f->pin_ok;}
static int patch(void *v,const unsigned char before[5],const unsigned char after[5])
{
    fixture *f=v;++f->patches;if(memcmp(f->site,before,5))return 0;
    int r=f->patches>1?f->rollback_result:f->patch_result;if(r==1)memcpy(f->site,after,5);return r;
}
static int call_original(void *v,const char *name,int second)
{
    fixture *f=v;++f->originals;f->argument=name;f->second=second;
    check(f->state.busy&&f->external_ready==0,"generation revoked before fake original");
    if(f->reenter){int r=ba184_call(&f->state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1);
        check(r==BA184_REENTRY,"nested callback refused before second original");}
    return f->original_ok;
}
static int publish(void *v,uint32_t epoch,uint32_t ready)
{fixture *f=v;++f->publishes;if(!f->publish_ok)return 0;f->external_epoch=epoch;f->external_ready=ready;return 1;}
static int proof(void *v,uint32_t epoch,unsigned stage,ba184_proof *p)
{fixture *f=v;++f->proofs;p->bits=f->proof_bits;p->epoch=epoch+f->proof_epoch_delta;
    p->stage=stage+f->proof_stage_delta;return f->proof_ok;}
static const ba184_ops ops={valid,pin,patch,call_original,publish,proof};
static void setup(fixture *f)
{
    memset(f,0,sizeof(*f));f->flags=BA184_ALL_VALIDATION;f->proof_bits=BA184_ALL_PROOF;
    f->pin_ok=f->original_ok=f->publish_ok=f->proof_ok=1;f->patch_result=f->rollback_result=1;
    memcpy(f->site,original,5);ba184_initialize(&f->state,&ops,f,7);
}
static void installed(fixture *f){setup(f);check(ba184_install(&f->state,7,1,replacement)==1,"fake guarded install");}
static void core_tests(void)
{
    fixture f;setup(&f);
    check(ba184_install(&f.state,7,0,replacement)==0&&!f.validate_calls&&!f.pins&&!f.patches,"default off performs no capability callbacks");
    check(ba184_install(&f.state,8,1,replacement)==BA184_WRONG_THREAD&&!f.pins&&!f.patches,"wrong install thread no mutation");
    for(unsigned bit=1;bit<=BA184_OWN_CHANNEL;bit<<=1){setup(&f);f.flags&=~bit;
        check(ba184_install(&f.state,7,1,replacement)<0&&!f.pins&&!f.patches&&!f.originals&&!f.publishes&&!f.proofs,"each missing exact validation bit refuses before mutation");}
    setup(&f);f.flags|=512;check(ba184_install(&f.state,7,1,replacement)==BA184_GUARD,"unexpected validation bits refused");
    setup(&f);f.site[2]^=1;check(ba184_install(&f.state,7,1,replacement)<0&&!f.pins&&!f.patches,"wrong live CALL bytes before pin");
    setup(&f);f.pin_ok=0;check(ba184_install(&f.state,7,1,replacement)==BA184_PIN&&!f.patches,"unretained lifetime never patches");
    setup(&f);f.patch_result=0;check(ba184_install(&f.state,7,1,replacement)==BA184_PATCH&&!f.state.installed&&!f.state.fatal,"unchanged transaction refusal");
    setup(&f);f.patch_result=-1;check(ba184_install(&f.state,7,1,replacement)==BA184_PATCH&&f.state.fatal,"uncertain transaction sticky fatal");
    setup(&f);f.break_phase=BA184_FRAME;check(ba184_install(&f.state,7,1,replacement)==BA184_GUARD&&!f.state.installed&&f.patches==2&&!memcmp(f.site,original,5),"postwrite guard failure rolls back exact original");
    setup(&f);f.break_phase=BA184_FRAME;f.rollback_result=-1;check(ba184_install(&f.state,7,1,replacement)<0&&f.state.fatal,"rollback uncertainty fatal");
    installed(&f);check(ba184_install(&f.state,7,1,replacement)==BA184_REFUSED&&f.patches==1,"no second install");
    installed(&f);char name[8]="trainer",saved[8];memcpy(saved,name,8);
    check(ba184_call(&f.state,7,BA184_RETURN_VA,name,sizeof(name),INT_MIN,1,1,0,1)==1
        &&f.originals==1&&!strcmp(f.argument,"so_killspree_trainer")&&f.second==INT_MIN
        &&!memcmp(name,saved,8)&&!f.state.ready_epoch&&!f.external_ready&&!f.proofs,"target substitutes distinct string preserves arg2/source without early promotion");
    check(ba184_source_enabled(&f.state,7)==1&&!f.proofs&&!f.state.early_epoch&&!f.state.ready_epoch,"source gate permits exact returned candidate without proof promotion");
    check(!ba184_ready(&f.state,7,0)&&!f.proofs,"first late query cannot promote missing initialization proof");
    check(ba184_ready(&f.state,7,1)&&f.state.ready_epoch==1&&f.external_ready==1&&f.proofs==1
        &&f.state.proof_stage==BA184_RESIDENT,"first synchronous initialization query requests resident phase");
    check(ba184_ready(&f.state,7,0)&&f.proofs==2&&f.state.proof_stage==BA184_REGISTERED,"postinitialization query requires registered phase of same early generation");
    check(ba184_observe(&f.state,7)==1&&f.proofs==2,"frame checks guards without implicit proof promotion");
    const char *names[]={"airport","Trainer","so_killspree_trainer","","trainer ","train"};
    for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);++i){installed(&f);
        check(ba184_call(&f.state,7,BA184_RETURN_VA,names[i],strlen(names[i])+1,INT_MAX,1,1,0,1)==1
            &&f.argument==names[i]&&f.second==INT_MAX&&!f.state.ready_epoch&&!f.proofs,"other exact maps pointer and arg2 passthrough, no proof promotion");}
    for(unsigned i=0;i<4;++i){installed(&f);unsigned coop=1,sv=1,so=0,fs=1;
        if(i==0)coop=0;if(i==1)sv=0;if(i==2)so=1;if(i==3)fs=0;
        const char *same="trainer";check(ba184_call(&f.state,7,BA184_RETURN_VA,same,8,19,coop,sv,so,fs)==1
            &&f.argument==same&&f.state.refused==1&&!f.state.ready_epoch,"ineligible mode preserves original name, ready remains revoked");}
    installed(&f);check(ba184_call(&f.state,8,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==BA184_WRONG_THREAD&&!f.originals,"wrong callback thread no original call");
    installed(&f);check(ba184_call(&f.state,7,BA184_RETURN_VA+1,"trainer",8,0,1,1,0,1)==BA184_CALLER&&!f.originals,"wrong return site no original call");
    installed(&f);f.state.busy=1;check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==BA184_REENTRY&&!f.originals,"reentry no original call");
    installed(&f);f.reenter=1;check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)<0&&f.originals==1&&!f.state.ready_epoch,"recursive fake original cannot publish readiness");
    installed(&f);f.original_ok=0;check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==BA184_LIFETIME&&f.state.busy&&!f.state.ready_epoch,"nonreturn/lifetime failure blocks next frame");
    for(unsigned bit=1;bit<=BA184_PRE_SCRIPT;bit<<=1){installed(&f);f.proof_bits&=~bit;
        check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==1&&!ba184_ready(&f.state,7,1)&&!f.state.ready_epoch,"each missing typed/source/zone/phase proof refuses synchronous promotion");}
    installed(&f);f.proof_epoch_delta=1;check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==1&&!ba184_ready(&f.state,7,1),"proof cannot use another load generation");
    installed(&f);f.proof_stage_delta=1;check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==1
        &&!ba184_ready(&f.state,7,1)&&f.state.revoked,"proof cannot return a different phase echo");
    installed(&f);check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==1&&ba184_ready(&f.state,7,1),"registered phase failure fixture has resident proof");
    f.proof_ok=0;check(!ba184_ready(&f.state,7,1)&&f.state.proof_stage==BA184_REGISTERED&&f.state.revoked&&!f.state.ready_epoch,
        "second query must prove registration even while initialization remains1");
    f.proof_ok=1;check(!ba184_ready(&f.state,7,1),"failed registration cannot fall back to resident capability");
    installed(&f);check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==1&&ba184_ready(&f.state,7,1)
        &&ba184_ready(&f.state,7,1)&&f.state.proof_stage==BA184_REGISTERED,"immediate second initialization query advances to registered");
    check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==1&&!f.state.proof_stage
        &&ba184_ready(&f.state,7,1)&&f.state.proof_stage==BA184_RESIDENT,"new substituted generation resets phase before first query");
    installed(&f);f.publish_ok=0;check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==BA184_PUBLISH&&!f.originals,"failed revoke stops before loader");
    installed(&f);f.break_phase=BA184_RETURN;check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==BA184_GUARD&&!f.state.ready_epoch,"file/language/path/code lifetime drift after native return");
    installed(&f);check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==1,"first generation valid");
    const char *next="airport";check(ba184_call(&f.state,7,BA184_RETURN_VA,next,8,27,1,1,0,1)==1&&f.external_epoch==2&&!f.external_ready&&f.argument==next,"next passthrough load revokes preceding proof");
    installed(&f);f.state.epoch=UINT32_MAX;check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==BA184_LIFETIME&&!f.originals,"generation overflow fails closed");
    installed(&f);check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",7,0,1,1,0,1)==BA184_BAD_ARGUMENT&&!f.originals,"unterminated callback input never reaches loader");
    installed(&f);f.state.busy=1;check(ba184_observe(&f.state,7)==BA184_LIFETIME,"next frame notices abandoned callback lifetime");
    installed(&f);f.flags&=~BA184_FLAG;check(ba184_observe(&f.state,7)==BA184_GUARD&&f.state.fatal,"flag revocation invalidates installed scope");
    installed(&f);check(!ba184_source_enabled(&f.state,7)&&!f.proofs,"source gate refuses before any loader generation");
    for(unsigned bit=1;bit<=BA184_OWN_CHANNEL;bit<<=1){installed(&f);
        check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==1,"source gate candidate prepared");
        f.flags&=~bit;check(!ba184_source_enabled(&f.state,7)&&!f.proofs&&!f.state.early_epoch&&!f.state.ready_epoch&&f.state.revoked,"source gate independently rejects each missing guard without promotion");}
    installed(&f);check(ba184_call(&f.state,7,BA184_RETURN_VA,"trainer",8,0,1,1,0,1)==1,"source gate owner fixture");
    check(!ba184_source_enabled(&f.state,8)&&!f.proofs&&!f.state.early_epoch,"wrong-thread source query cannot gain capability");
    installed(&f);check(ba184_call(&f.state,7,BA184_RETURN_VA,"airport",8,0,1,1,0,1)==1&&!ba184_source_enabled(&f.state,7),"passthrough cannot select candidate body source");
}
typedef struct tx_fixture {unsigned char *page;unsigned writes,restores,flushes;unsigned fail_write,fail_restore,fail_flush,drift;} tx_fixture;
static int query(void *v,unsigned char *s,size_t n){tx_fixture *f=v;MEMORY_BASIC_INFORMATION m;
    return s==f->page+19&&n==5&&VirtualQuery(s,&m,sizeof(m))==sizeof(m)&&m.State==MEM_COMMIT&&m.Type==MEM_PRIVATE&&m.Protect==PAGE_EXECUTE_READ;}
static int writable(void *v,unsigned char *s,size_t n){tx_fixture *f=v;DWORD old;++f->writes;
    if(f->writes==f->fail_write)return 0;int ok=VirtualProtect(s,n,PAGE_EXECUTE_READWRITE,&old);
    if(ok&&f->drift&&f->writes==1)s[0]^=1;return ok;}
static int restore(void *v,unsigned char *s,size_t n){tx_fixture *f=v;DWORD old;++f->restores;
    if(f->restores==f->fail_restore)return 0;return VirtualProtect(s,n,PAGE_EXECUTE_READ,&old)!=0;}
static int flush(void *v,unsigned char *s,size_t n){tx_fixture *f=v;++f->flushes;if(f->flushes==f->fail_flush)return 0;return FlushInstructionCache(GetCurrentProcess(),s,n)!=0;}
static const ba184_tx_ops tx_ops={query,writable,restore,flush};
static void tx_tests(void)
{
    for(unsigned scenario=0;scenario<8;++scenario){tx_fixture f={0};
        f.page=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!f.page){check(0,"own fixture allocation");return;}
        memcpy(f.page+19,original,5);DWORD old;VirtualProtect(f.page,4096,PAGE_EXECUTE_READ,&old);
        if(scenario==1)f.fail_write=1;if(scenario==2)f.fail_flush=1;if(scenario==3)f.fail_restore=1;
        if(scenario==4){f.fail_flush=1;f.fail_write=2;}if(scenario==5){f.fail_flush=1;f.fail_restore=2;}
        if(scenario==6)f.drift=1;
        unsigned char before[5];memcpy(before,original,5);if(scenario==7)before[1]^=1;
        int r=ba184_tx(&tx_ops,&f,f.page+19,before,replacement);
        if(scenario==0)check(r==1&&query(&f,f.page+19,5)&&!memcmp(f.page+19,replacement,5),"own RX transaction exact bytes and protection");
        if(scenario==1||scenario==2||scenario==3||scenario==7)check(r==0&&query(&f,f.page+19,5)&&!memcmp(f.page+19,original,5),"unchanged or rollback restores exact bytes and RX");
        if(scenario==4||scenario==5)check(r==-1,"uncertain rollback/protection is explicit fatal result");
        if(scenario==6)check(r==0&&query(&f,f.page+19,5)&&f.flushes==0,"prewrite drift refused with protection restored, no overwrite");
        check(VirtualFree(f.page,0,MEM_RELEASE)!=0,"only own allocated fixture released");
    }
}
int main(void){core_tests();tx_tests();printf("bodyassets184 offline: %u checks, %u failures; no game/native engine\n",checks,failed);return failed?1:0;}
