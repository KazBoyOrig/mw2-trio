/* Own MIT tests. Only own data, OS atomics and unsupported public paths.
 * Active observer states below are seeded from an own allocated8B fixture,
 * never a supported engine image or a native game installation. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static void *own_page;
static int claim_image;
static uintptr_t guarded_callback;
static SIZE_T WINAPI fixture_query(LPCVOID at,PMEMORY_BASIC_INFORMATION m,SIZE_T bytes)
{
    SIZE_T got=VirtualQuery(at,m,bytes);
    uintptr_t a=(uintptr_t)at,b=(uintptr_t)own_page;
    if(guarded_callback&&a==guarded_callback&&got==sizeof(*m))m->Protect|=PAGE_GUARD;
    if(claim_image&&got==sizeof(*m)&&a>=b&&a-b<4096){m->Type=MEM_IMAGE;m->AllocationBase=(void *)0x400000u;}
    return got;
}
#define VirtualQuery fixture_query
#include "qa_identity184.c"
#undef VirtualQuery
static unsigned checks,failures,guards;
int __cdecl bootstrap_guard_current(bootstrap_report *r,uint32_t bytes)
{++guards;if(r&&bytes==sizeof(*r))memset(r,0,bytes);return 0;}
static void check(int ok,const char *name)
{++checks;failures+=!ok;printf("%s %s\n",ok?"PASS":"FAIL",name);}
static int create_flag(wchar_t *out,const wchar_t *name)
{
    DWORD n=GetModuleFileNameW(NULL,out,MAX_PATH);if(!n||n>=MAX_PATH)return 0;
    while(n&&out[n-1]!=L'\\')--n;out[n]=0;if(n+40>=MAX_PATH)return 0;lstrcatW(out,name);
    HANDLE f=CreateFileW(out,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f==INVALID_HANDLE_VALUE)return 0;CloseHandle(f);return 1;
}
static uint32_t peer_scope_low=2;
static unsigned peer_scope_calls;
static int peer_scope_accept=1;
static int __cdecl own_peer_scope(uint32_t *low)
{++peer_scope_calls;if(low)*low=peer_scope_low;return peer_scope_accept;}
static void peer_next_fixtures(void)
{
    const uint64_t before=UINT64_C(0x11223344fedcba98),sentinel=UINT64_C(0xa55aa55aa55aa55a);
    const uint32_t accepted[]={2,3,0x7ffffffeu,0x7fffffffu};
    const uint32_t refused[]={0,1,0x80000000u,UINT32_MAX};
    uint64_t after;
    check(sizeof(qa_identity184_peer_options)==32,"exact x86 private-peer options ABI");
    for(unsigned i=0;i<sizeof(accepted)/sizeof(accepted[0]);++i){after=sentinel;
        check(qi_peer_next(before,accepted[i],&after)&&(uint32_t)after==accepted[i]
            &&(after>>32)==(before>>32),"peer even/odd low boundary preserves every high DWORD bit");
    }
    for(unsigned i=0;i<sizeof(refused)/sizeof(refused[0]);++i){after=sentinel;
        check(!qi_peer_next(before,refused[i],&after)&&after==sentinel,
            "peer zero/one/oversized low refuses without output mutation");
    }
    after=sentinel;
    check(!qi_peer_next(0,2,&after)&&after==sentinel,"peer zero original identity refuses without output mutation");
    check(!qi_peer_next(UINT64_C(0x1122334400000000),2,&after)&&after==sentinel,
        "peer zero original low DWORD refuses without output mutation");
    check(!qi_peer_next(UINT64_C(0x00000000fedcba98),2,&after)&&after==sentinel,
        "peer zero original high DWORD refuses without output mutation");
    check(!qi_peer_next(before,2,NULL),"peer NULL derived output refuses");
    check(qi_peer_next(UINT64_C(0xffffffffffffffff),2,&after)&&after==UINT64_C(0xffffffff00000002),
        "peer replaces original maximum low without carry or high DWORD loss");
}
static void peer_public_fixtures(volatile LONGLONG *fixture,uint64_t expected_fixture)
{
    /* Independent unsupported-helper model, after all original QA regressions.
     * Only own statics are reset; no native image has been installed or touched. */
    qi_attempted=0;qi_active=0;qi_fatal=0;qi_owner=0;
    qi_mode=0;qi_peer_low=0;qi_peer_callback=NULL;qi_directory[0]=0;
    peer_scope_calls=0;peer_scope_low=2;peer_scope_accept=1;
    qa_identity184_report r;
    qa_identity184_peer_options o={QA_IDENTITY184_VERSION,sizeof(o),GetModuleHandleW(NULL),
        GetCurrentThreadId(),1,1,2,own_peer_scope};
    check(qa_identity184_apply_peer(NULL,&r)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
        "peer NULL options refuses before callback");
    check(qa_identity184_apply_peer(&o,NULL)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
        "peer NULL report refuses before callback");
    ++o.version;check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
        "peer wrong API version refuses before callback");--o.version;
    --o.bytes;check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
        "peer wrong options ABI refuses before callback");++o.bytes;
    o.role=0;check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
        "peer role zero refuses before callback");
    o.role=3;check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
        "peer role beyond host/client refuses before callback");o.role=1;
    o.derived_low=3;check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
        "peer host odd low refuses before callback");
    o.role=2;o.derived_low=2;check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
        "peer client even low refuses before callback");o.role=1;
    const uint32_t invalid_low[]={0,1,0x80000000u,UINT32_MAX};
    for(unsigned i=0;i<sizeof(invalid_low)/sizeof(invalid_low[0]);++i){o.derived_low=invalid_low[i];
        check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
            "peer public zero/one/oversized low refuses before callback");
    }o.derived_low=2;
    guarded_callback=(uintptr_t)own_peer_scope;
    check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
        "peer guarded RX callback refuses before callback execution");guarded_callback=0;
    o.scope=NULL;check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
        "peer NULL scope refuses before callback");
    o.scope=(qa_identity184_peer_scope)(uintptr_t)own_page;
    check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
        "peer foreign-allocation callback refuses without executing allocated bytes");
    o.scope=(qa_identity184_peer_scope)(uintptr_t)&peer_scope_low;
    check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
        "peer own-module writable-data callback refuses before execution");o.scope=own_peer_scope;
    o.module=NULL;check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_BAD_ARGUMENT&&!peer_scope_calls,
        "peer NULL module refuses callback ownership");o.module=GetModuleHandleW(NULL);
    ++o.main_thread_id;check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_WRONG_THREAD&&!peer_scope_calls,
        "peer foreign owner thread refuses before callback");--o.main_thread_id;
    peer_scope_accept=0;
    check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_SCOPE_LOST&&peer_scope_calls==1&&!qi_attempted,
        "peer refused scope never consumes the one permitted attempt");peer_scope_accept=1;
    peer_scope_low=4;
    check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_SCOPE_LOST&&peer_scope_calls==2&&!qi_attempted,
        "peer mismatched scope low refuses before build checks");peer_scope_low=2;
    o.native_frame_ordinal=0;
    check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_BAD_ARGUMENT&&peer_scope_calls==3&&!qi_attempted,
        "peer pre-readiness common options refuse without an attempt");o.native_frame_ordinal=1;
    check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_DISABLED&&peer_scope_calls==4&&!qi_attempted,
        "peer host even low and own RX callback still require explicit flags");
    o.role=2;o.derived_low=3;peer_scope_low=3;
    check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_DISABLED&&peer_scope_calls==5&&!qi_attempted,
        "peer client odd low and own RX callback still require explicit flags");
    wchar_t paths[4][MAX_PATH];
    const wchar_t *names[]={L"mw2-trio-peer-test.enable",L"mw2-trio-network-test.enable",
        L"mw2-trio-party-test.enable",L"mw2-trio-control.enable"};
    unsigned created=0;
    for(;created<4;++created)if(!create_flag(paths[created],names[created]))break;
    check(created==4,"create only own new private-peer opt-in fixtures");
    if(created!=4){for(unsigned i=0;i<created;++i)DeleteFileW(paths[i]);return;}
    unsigned previous_guards=guards;
    check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_WRONG_BUILD&&peer_scope_calls==6
        &&qi_attempted&&!qi_active&&qi_mode==2&&guards-previous_guards<=1
        &&(uint64_t)*fixture==expected_fixture,
        "peer explicit flags still reject unsupported helper before native engine access");
    check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_ALREADY_ATTEMPTED&&peer_scope_calls==6
        &&(uint64_t)*fixture==expected_fixture,"peer second apply cannot call scope or write");
    qa_identity184_options common={QA_IDENTITY184_VERSION,sizeof(common),GetModuleHandleW(NULL),
        GetCurrentThreadId(),1,1,1,1};
    check(qa_identity184_apply_client(&common,&r)==QA_IDENTITY184_ALREADY_ATTEMPTED
        &&(uint64_t)*fixture==expected_fixture,"peer attempt cannot be retried through QA identity API");
    /* The own active model refuses callback scope before native scope reads. */
    qi_active=1;qi_owner=GetCurrentThreadId();qi_fatal=0;peer_scope_accept=0;
    check(qa_identity184_observe(&r)==QA_IDENTITY184_SCOPE_LOST&&qi_fatal&&peer_scope_calls==7
        &&(uint64_t)*fixture==expected_fixture,"peer active callback refusal is fatal without restoring cache");
    qi_fatal=0;peer_scope_accept=1;peer_scope_low=5;
    check(qa_identity184_observe(&r)==QA_IDENTITY184_SCOPE_LOST&&qi_fatal&&peer_scope_calls==8
        &&(uint64_t)*fixture==expected_fixture,"peer active callback low drift is fatal without reapplying cache");
    qi_active=0;qi_fatal=0;qi_attempted=0;peer_scope_accept=-1;peer_scope_low=o.derived_low;
    check(qa_identity184_apply_peer(&o,&r)==QA_IDENTITY184_SCOPE_LOST&&!qi_attempted,
        "negative scope result is refusal, never truthy permission");
    unsigned removed=0;for(unsigned i=0;i<created;++i)removed+=(unsigned)(DeleteFileW(paths[i])!=0);
    check(removed==created,"remove only own newly created private-peer flags");
}
int main(void)
{
    uint64_t before=UINT64_C(0x0110000100000033),next=0,sentinel=UINT64_C(0xa55aa55aa55aa55a);
    check(sizeof(qa_identity184_options)==32&&QI_UID%8==0,"exact x86 options ABI and native cache8B alignment");
    check(qi_next(before,1,&next)&&next==before+1&&(next>>32)==(before>>32),"delta1 changes low DWORD only");
    check(!qi_next(before,0,&sentinel)&&sentinel==UINT64_C(0xa55aa55aa55aa55a),"zero delta refuses without output mutation");
    check(!qi_next(before,2,&next)&&!qi_next(before,UINT32_MAX,&next),"only literal delta1 allowed");
    check(!qi_next(0,1,&next)&&!qi_next(UINT64_C(0x100000000),1,&next),"zero identity/account component refuses");
    check(!qi_next(1,1,&next),"zero high DWORD refuses");
    check(!qi_next(UINT64_C(0x01100001ffffffff),1,&next),"low DWORD carry refuses");
    check(qi_next(UINT64_C(0x01100001fffffffe),1,&next)&&next==UINT64_C(0x01100001ffffffff),"last non-carry increment retains high DWORD");
    check(!qi_next(before,1,NULL),"NULL derived output refuses");
    qi_snapshot s;memset(&s,0,sizeof(s));s.uid=before;
    for(unsigned i=0;i<QI_FIELDS;++i)s.value[i]=qi_requirements[i].expected;
    uint32_t failed=0;
    check(qi_values_fresh(&s,&failed),"all44 exact empty native fields accept synthetic fresh menu");
    s.value[0]=0;check(qi_values_fresh(&s,&failed),"frontend state0 allowed with all independent fresh-state guards intact");s.value[0]=1;
    for(unsigned i=0;i<QI_FIELDS;++i){uint32_t original=s.value[i];
        s.value[i]=qi_requirements[i].va==0x1a00b64u?2:original+1;failed=0;
        check(!qi_values_fresh(&s,&failed)&&failed==i+1,"each occupied or invalid native field refuses at its own index");
        s.value[i]=original;
    }
    for(unsigned i=0;i<QI_FIELDS;++i)if(qi_requirements[i].va==0x1a00b64u)s.value[i]=1;
    check(qi_values_fresh(&s,&failed),"configured1 allowed only with all actual sockets/proxy/queues/party still empty");
    s.value[19]=1;check(!qi_values_fresh(&s,&failed)&&failed==20,"configured1 never excuses actual IPv4 socket");s.value[19]=0;
    own_page=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!own_page)return 2;
    uint64_t *words=own_page;for(unsigned i=0;i<512;++i)words[i]=sentinel;
    volatile LONGLONG *uid=(void *)(words+2);memcpy((void *)uid,&before,8);qi_next(before,1,&next);
    check(qi_atomic(uid,before,next)&&(uint64_t)*uid==next,"real aligned expected-value8B CAS changes own fixture once");
    check(words[1]==sentinel&&words[3]==sentinel,"neighbor canaries unchanged");
    check(!qi_atomic(uid,before,next+1)&&(uint64_t)*uid==next,"stale CAS comparison leaves entire fixture unchanged");
    check(!qi_atomic((void *)((uintptr_t)uid+1),next,next+1)&&!qi_atomic(NULL,next,next+1),"unaligned/NULL CAS refuses before access");
    check(!qi_span((uintptr_t)uid,8,0,1),"actual private mapping is not accepted as native UID storage");
    claim_image=1;check(qi_span((uintptr_t)uid,8,0,1),"fixture-only exact image metadata exercises RW permission branch");
    DWORD old;VirtualProtect(own_page,4096,PAGE_READONLY,&old);
    check(!qi_span((uintptr_t)uid,8,0,1)&&(uint64_t)*uid==next,"read-only native-shaped fixture refuses before atomic store");
    VirtualProtect(own_page,4096,PAGE_READWRITE,&old);claim_image=0;
    qa_identity184_report report;
    check(qa_identity184_observe(&report)==QA_IDENTITY184_DISABLED&&!report.active,"inactive observer calls no native engine path");
    check(qa_identity184_apply_client(NULL,&report)==QA_IDENTITY184_BAD_ARGUMENT,"NULL apply options refuse");
    qa_identity184_options o={QA_IDENTITY184_VERSION,sizeof(o),GetModuleHandleW(NULL),GetCurrentThreadId(),1,1,1,1};
    --o.bytes;check(qa_identity184_apply_client(&o,&report)==QA_IDENTITY184_BAD_ARGUMENT,"wrong options ABI refuses");++o.bytes;
    o.native_frame_ordinal=0;check(qa_identity184_apply_client(&o,&report)==QA_IDENTITY184_BAD_ARGUMENT,"pre-readiness call refuses");o.native_frame_ordinal=1;
    o.client_role=0;check(qa_identity184_apply_client(&o,&report)==QA_IDENTITY184_BAD_ARGUMENT,"non-client role refuses");o.client_role=1;
    o.delta=2;check(qa_identity184_apply_client(&o,&report)==QA_IDENTITY184_BAD_ARGUMENT,"public delta2 refuses");o.delta=1;
    o.loopback_enforced=0;check(qa_identity184_apply_client(&o,&report)==QA_IDENTITY184_BAD_ARGUMENT,"missing parent loopback contract refuses");o.loopback_enforced=1;
    ++o.main_thread_id;check(qa_identity184_apply_client(&o,&report)==QA_IDENTITY184_WRONG_THREAD,"wrong initial owner thread refuses");--o.main_thread_id;
    check(qa_identity184_apply_client(&o,&report)==QA_IDENTITY184_DISABLED&&!qi_attempted,"missing opt-in does not consume attempt");
    wchar_t paths[3][MAX_PATH];const wchar_t *names[]={L"mw2-trio-loopback-qa.enable",L"mw2-trio-party-test.enable",L"mw2-trio-control.enable"};
    unsigned created=0;
    for(;created<3;++created)if(!create_flag(paths[created],names[created]))break;
    if(created!=3){for(unsigned i=0;i<created;++i)DeleteFileW(paths[i]);VirtualFree(own_page,0,MEM_RELEASE);return 3;}
    check(qa_identity184_apply_client(&o,&report)==QA_IDENTITY184_WRONG_BUILD&&!qi_active,"explicit flags still refuse unsupported helper before native reads/writes");
    check(guards<=1&&qi_attempted&&!qi_active&&(uint64_t)*uid==next,"unsupported attempt never changes own fixture or activates");
    check(qa_identity184_apply_client(&o,&report)==QA_IDENTITY184_ALREADY_ATTEMPTED&&(uint64_t)*uid==next,"second apply cannot write");
    /* Model an active observer around the already committed own8B CAS only. */
    qi_original=before;qi_expected=next;qi_owner=GetCurrentThreadId();qi_active=1;qi_fatal=0;
    qi_report(&report);check(qi_cache_observation((uint64_t)*uid,1,&report)==QA_IDENTITY184_OK,"own expected cache observation succeeds");
    *uid=(LONGLONG)(next+7);check(qi_cache_observation((uint64_t)*uid,1,&report)==QA_IDENTITY184_UID_CHANGED&&qi_fatal
        &&(uint64_t)*uid==next+7&&qi_expected==next,"own cache drift is fatal and is neither restored nor reapplied");
    check(qa_identity184_observe(&report)<0&&(uint64_t)*uid==next+7,"fatal observer remains sticky without native reads or writes");
    qi_fatal=0;qi_report(&report);check(qi_cache_observation(0,0,&report)==QA_IDENTITY184_UID_CHANGED&&qi_fatal,"unreadable cache observation is fatal");
    qi_fatal=0;qi_owner=GetCurrentThreadId()+1;
    check(qa_identity184_observe(&report)==QA_IDENTITY184_WRONG_THREAD&&qi_fatal&&(uint64_t)*uid==next+7,"public wrong-thread observer fails before native reads without restore");
    qi_fatal=0;qi_owner=GetCurrentThreadId();
    check(DeleteFileW(paths[0]),"delete only own newly created QA flag for scope regression");
    check(qa_identity184_observe(&report)==QA_IDENTITY184_SCOPE_LOST&&qi_fatal&&(uint64_t)*uid==next+7,"public removed-flag observer is fatal before native reads without restore");
    check(DeleteFileW(paths[1])&&DeleteFileW(paths[2]),"remove only remaining own newly created flags");
    peer_next_fixtures();
    peer_public_fixtures(uid,next+7);
    check(VirtualFree(own_page,0,MEM_RELEASE),"own allocated region released");
    printf("RESULT checks=%u failures=%u; own data/atomics/unsupported image only, no game or native engine executed\n",checks,failures);
    return failures?1:0;
}
