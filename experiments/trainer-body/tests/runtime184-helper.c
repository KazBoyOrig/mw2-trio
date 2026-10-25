/* Own-process tests only: no native engine function or game code is executed. */
#include "../src/runtime184.c"
#include <stdio.h>

static unsigned checks,failures,guard_calls,enters,leaves;
static unsigned body_proofs,format_calls,lookup_calls,proof_value;
static const char *seen_name,*seen_fallback;
static rt_dvar own_coop;
int __cdecl bodyassets184_ready_now(void){++body_proofs;return 0;}
static int __cdecl own_proof(void){++body_proofs;SetLastError(99);return (int)proof_value;}
static const char *__cdecl own_format(const char *name,const char *fallback)
{++format_calls;seen_name=name;seen_fallback=fallback;SetLastError(0x184a);return "own native formatter";}
static void *__cdecl own_lookup(const char *name)
{++lookup_calls;seen_name=name;SetLastError(0x184b);return &own_coop;}
static __attribute__((used)) volatile LONG helper_original_calls;
static __attribute__((used)) uint32_t helper_regs[7],helper_flags_before,helper_flags_after;
int __cdecl bootstrap_guard_current(bootstrap_report *r,uint32_t n)
{++guard_calls;if(r&&n==sizeof(*r))memset(r,0,sizeof(*r));return 0;}
static void check(int ok,const char *name){++checks;failures+=!ok;printf("%s %s\n",ok?"PASS":"FAIL",name);}
static int __cdecl enter(void *p){(void)p;++enters;return 1;}
static void __cdecl leave(void *p){(void)p;++leaves;}
static void __attribute__((naked,noinline)) own_original(void)
{__asm__ __volatile__("pushfl\n\tincl _helper_original_calls\n\tpopfl\n\tret\n\t");}
static void __attribute__((naked,noinline)) probe_registers(void)
{
    __asm__ __volatile__(
      "pushfl\n\tpushal\n\tmovl $0x12345678,%eax\n\tmovl $0x23456789,%ecx\n\tmovl $0x3456789a,%edx\n\t"
      "movl $0x456789ab,%ebx\n\tmovl $0x56789abc,%ebp\n\tmovl $0x6789abcd,%esi\n\tmovl $0x789abcde,%edi\n\t"
      "stc\n\tpushfl\n\tpopl _helper_flags_before\n\tcall _rt_error_entry\n\t"
      "movl %eax,_helper_regs\n\tmovl %ecx,_helper_regs+4\n\tmovl %edx,_helper_regs+8\n\tmovl %ebx,_helper_regs+12\n\t"
      "movl %ebp,_helper_regs+16\n\tmovl %esi,_helper_regs+20\n\tmovl %edi,_helper_regs+24\n\t"
      "pushfl\n\tpopl _helper_flags_after\n\tpopal\n\tpopfl\n\tret\n\t");
}
static int helper_flag(wchar_t *out,const wchar_t *name)
{
    DWORD n=GetModuleFileNameW(NULL,out,MAX_PATH);if(!n||n>=MAX_PATH)return 0;
    while(n&&out[n-1]!=L'\\')--n;out[n]=0;if(n+40>=MAX_PATH)return 0;lstrcatW(out,name);
    HANDLE f=CreateFileW(out,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f==INVALID_HANDLE_VALUE)return 0;CloseHandle(f);return 1;
}
int main(void)
{
    rt_owner=GetCurrentThreadId();
    rt_dvar *d=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!d)return 2;
    d->name=(uint32_t)(uintptr_t)"start";d->type=7;d->flags=0x100;
    check(sizeof(rt_dvar)==32&&offsetof(rt_dvar,flags)==8&&offsetof(rt_dvar,current)==16,"x86 prefix offsets and union width");
    check((COOP_READ_MASK&0x80)!=0,"visibility bit is accepted by exact native filter mask");
    check(rt_flag_pointer(d,"start")&&d->flags==0x180,"external flag preserved while visibility bit added");
    check(rt_changes==1,"first visibility transition counted once");
    check(rt_flag_pointer(d,"start")&&rt_changes==1&&d->flags==0x180,"repeated flag application is idempotent");
    d->flags=0x104;check(rt_flag_pointer(d,"start")&&d->flags==0x104,"already readable flag combination is untouched");
    d->flags=0x100;check(!rt_flag_pointer(d,"different")&&d->flags==0x100,"unexpected native name refuses before write");
    d->type=9;check(!rt_flag_pointer(d,"start")&&d->flags==0x100,"unknown native dvar type refuses before write");d->type=7;
    check(!rt_flag_pointer((void *)((uintptr_t)d+1),"start")&&d->flags==0x100,"unaligned native flag address refuses");
    check(rt_flag_pointer(d,"START")&&d->flags==0x180,"native name matching accepts ASCII case without changing name");
    d->flags=0x100;DWORD old;VirtualProtect(d,4096,PAGE_READONLY,&old);
    check(!rt_flag_pointer(d,"start")&&d->flags==0x100,"read-only storage never receives an Interlocked store");
    MEMORY_BASIC_INFORMATION m;VirtualQuery(d,&m,sizeof(m));check(m.Protect==PAGE_READONLY,"refusal preserves original page protection");
    VirtualProtect(d,4096,old,&old);
    check(!rt_flag_pointer(NULL,"start"),"NULL lookup never receives a write");
    rt_owner=GetCurrentThreadId()+1;check(!rt_flag_pointer(d,"start")&&d->flags==0x100,"wrong owner thread refuses writes");rt_owner=GetCurrentThreadId();
    check(sizeof(rt_localize)==8&&!strcmp(rt_en.name,rt_key)&&!strcmp(rt_ru.name,rt_key),"own localize entries have stable x86 name/value layout");
    check(!strcmp(rt_en.value,"You are stunned. Wait for the other player")&&(unsigned char)rt_ru.value[0]==0xc2&&(unsigned char)rt_ru.value[1]==0xfb,"own English and CP1251 Russian literals retained");
    rt_build_plan();uint32_t target;
    memcpy(&target,rt_plan[0].after+1,4);target+=(uint32_t)FILTER_CALL+5u;
    check(target==(uint32_t)(uintptr_t)rt_filter_find&&rt_plan[0].n==5,"filter CALL rel32 encodes own typed lookup shim");
    for(unsigned i=5;i<7;++i){memcpy(&target,rt_plan[i].after+1,4);target+=(uint32_t)rt_plan[i].va+5u;
        check(target==(uint32_t)(uintptr_t)rt_script_value&&rt_plan[i].n==5,"both actual value CALLs target the controlled cdecl formatter");}
    check(RUNTIME184_PATCH_COUNT==7&&rt_plan[5].va==FORMAT_READABLE_CALL&&rt_plan[6].va==FORMAT_FALLBACK_CALL,"seven-site transaction includes readable and fallback paths");
    unsigned calls_before=body_proofs;SetLastError(0x184c);
    proof_value=0;check(!strcmp(rt_value_dispatch(rt_body_key,"unsafe",own_format,own_proof,FORMAT_READABLE_CALL+5u),"0")&&body_proofs==calls_before+1&&!format_calls,"owned key false ignores supplied default and never queries native value");
    check(GetLastError()==0x184c,"owned proof dispatch preserves incoming Win32 error");
    proof_value=1;check(!strcmp(rt_value_dispatch(rt_body_key,"0",own_format,own_proof,FORMAT_READABLE_CALL+5u),"1")&&!format_calls,"owned key true comes only from direct fresh proof");
    check(!strcmp(rt_value_dispatch(rt_body_key,"0",own_format,own_proof,FORMAT_FALLBACK_CALL+5u),"1"),"native fallback CALL return address also permits fresh proof");
    calls_before=body_proofs;
    const uintptr_t bad_callers[]={0,FORMAT_READABLE_CALL,FORMAT_READABLE_CALL+4u,FORMAT_READABLE_CALL+6u,FORMAT_FALLBACK_CALL+4u,FORMAT_FALLBACK_CALL+6u};
    for(unsigned i=0;i<sizeof(bad_callers)/sizeof(bad_callers[0]);++i)
        check(!strcmp(rt_value_dispatch(rt_body_key,"1",own_format,own_proof,bad_callers[i]),"0")&&body_proofs==calls_before,"foreign or adjacent caller refuses before native proof");
    check(!strcmp(rt_script_value(rt_body_key,"1"),"0")&&body_proofs==calls_before,"actual wrapper called directly by own helper refuses foreign return address");
    proof_value=2;check(!strcmp(rt_value_dispatch(rt_body_key,"1",own_format,own_proof,FORMAT_READABLE_CALL+5u),"0"),"noncanonical positive predicate is refused");
    check(!strcmp(rt_value_dispatch(rt_body_key,"1",own_format,NULL,FORMAT_READABLE_CALL+5u),"0")&&!format_calls,"absent predicate refuses user supplied true default");
    calls_before=body_proofs;const char ordinary[]="ordinary",fallback[]="same pointer";
    check(!strcmp(rt_value_dispatch(ordinary,fallback,own_format,own_proof,0),"own native formatter")
        &&format_calls==1&&seen_name==ordinary&&seen_fallback==fallback&&body_proofs==calls_before,"other keys forward exact original pointers without body proof");
    check(GetLastError()==0x184a,"ordinary formatter retains its own Win32 error result");
    const char different[]="MW2TRIO_BODY_ASSETS_READY";
    rt_value_dispatch(different,NULL,own_format,own_proof,0);
    check(format_calls==2&&seen_name==different&&seen_fallback==NULL&&body_proofs==calls_before,"owned key is exact case-sensitive and ordinary NULL default is preserved");
    check(!runtime184_body_channel_ready()&&!rt_body_live_ready()&&body_proofs==calls_before,"inactive channel refuses before any native proof");
    rt_installed=1;rt_owner=GetCurrentThreadId()+1;
    check(!runtime184_body_channel_ready()&&!rt_body_live_ready()&&body_proofs==calls_before,"foreign-thread channel refuses before any native proof");
    rt_owner=GetCurrentThreadId();rt_installed=0;
    rt_installed=1;calls_before=body_proofs;
    check(!runtime184_body_channel_ready()&&rt_fatal&&!rt_body_live_ready()&&body_proofs==calls_before,"forced installed state in unsupported helper fails guards before native proof");
    rt_installed=0;rt_fatal=0;
    SetLastError(0x184c);check(!rt_filter_lookup(rt_body_key,own_lookup)&&!lookup_calls&&GetLastError()==0x184c,"inactive owned lookup never queries attacker-created key");
    rt_installed=1;own_coop.name=(uint32_t)(uintptr_t)"coop";own_coop.type=0;own_coop.flags=0x80;own_coop.current[0]=1;
    check(rt_filter_lookup(rt_body_key,own_lookup)==&own_coop&&lookup_calls==1&&!strcmp(seen_name,"coop"),"visibility filter returns only validated real-coop shape");
    own_coop.current[0]=0;check(!rt_filter_lookup(rt_body_key,own_lookup),"false coop cannot authorize owned visibility");own_coop.current[0]=1;
    own_coop.type=7;check(!rt_filter_lookup(rt_body_key,own_lookup),"wrong coop type cannot authorize owned visibility");own_coop.type=0;
    own_coop.name=(uint32_t)(uintptr_t)rt_body_key;check(!rt_filter_lookup(rt_body_key,own_lookup),"user-named lookalike cannot authorize owned visibility");own_coop.name=(uint32_t)(uintptr_t)"coop";
    own_coop.flags=0;check(!rt_filter_lookup(rt_body_key,own_lookup),"unreadable coop flags refuse without mutation");own_coop.flags=0x80;
    calls_before=lookup_calls;rt_owner=GetCurrentThreadId()+1;check(!rt_filter_lookup(rt_body_key,own_lookup)&&lookup_calls==calls_before,"foreign-thread owned lookup never calls native lookup");
    rt_owner=GetCurrentThreadId();rt_installed=0;
    memcpy(&target,rt_plan[3].after+1,4);target+=(uint32_t)LOCALIZE_CALL+5u;
    check(target==(uint32_t)(uintptr_t)rt_localized_find&&rt_plan[3].n==5,"localization changes only its existing CALL");
    check(rt_plan[4].n==7&&rt_plan[4].after[0]==0xe9&&rt_plan[4].after[5]==0x90&&rt_plan[4].after[6]==0x90,"core error replacement covers exactly complete seven-byte CMP");
    memcpy(&target,rt_plan[1].after,4);check(target==(uint32_t)(uintptr_t)rt_set&&rt_plan[1].va==0x731508,"setter plan targets pointer field only");
    runtime184_report report;check(runtime184_tick(&report)==RUNTIME184_DISABLED&&!report.installed,"inactive public tick calls no engine");
    check(runtime184_install_preengine(NULL,&report)==RUNTIME184_BAD_ARGUMENT,"NULL installer options refuse");
    runtime184_options o={RUNTIME184_API_VERSION,sizeof(o),GetModuleHandleW(NULL),GetCurrentThreadId(),1,enter,leave,NULL};
    --o.bytes;check(runtime184_install_preengine(&o,&report)==RUNTIME184_BAD_ARGUMENT,"mismatched options ABI refuses");++o.bytes;
    ++o.main_thread_id;check(runtime184_install_preengine(&o,&report)==RUNTIME184_WRONG_THREAD,"installer enforces owner thread");--o.main_thread_id;
    o.diagnostic_tick=2;check(runtime184_install_preengine(&o,&report)==RUNTIME184_NOT_PREENGINE,"later entry refuses before flags/engine");o.diagnostic_tick=1;
    check(runtime184_install_preengine(&o,&report)==RUNTIME184_DISABLED&&!rt_attempted,"missing opt-in makes no attempt or native call");
    wchar_t first[MAX_PATH],second[MAX_PATH];
    if(!helper_flag(first,L"mw2-trio-two-player-test.enable")){VirtualFree(d,0,MEM_RELEASE);return 3;}
    if(!helper_flag(second,L"mw2-trio-control.enable")){DeleteFileW(first);VirtualFree(d,0,MEM_RELEASE);return 3;}
    o.enter_quiescence=NULL;check(runtime184_install_preengine(&o,&report)==RUNTIME184_NO_QUIESCENCE,"explicit opt-in still requires verified barrier");o.enter_quiescence=enter;
    check(runtime184_install_preengine(&o,&report)==RUNTIME184_WRONG_BUILD&&!rt_installed&&!rt_error_page,"opt-in unsupported helper refuses before native signatures/allocation");
    check(!enters&&!leaves&&guard_calls<=1,"unsupported process never enters engine barrier or hooks");
    check(runtime184_install_preengine(&o,&report)==RUNTIME184_ALREADY_ATTEMPTED,"unsupported guarded attempt cannot retry later");
    rt_log[0]=0;rt_error_original=(void *)(uintptr_t)&own_original;
    __attribute__((aligned(16))) unsigned char before[512],after[512];
    SetLastError(0x184c0deu);
    __asm__ __volatile__("fxsave %0":"=m"(before));
    __asm__ __volatile__("call _rt_error_entry":::"eax","ecx","edx","memory","cc");
    __asm__ __volatile__("fxsave %0":"=m"(after));
    DWORD preserved=GetLastError();
    check(helper_original_calls==1&&rt_errors==1,"real wrapper logs then reaches own original stub without native engine call");
    check(!memcmp(before,after,2)&&!memcmp(before+24,after+24,8)&&!memcmp(before+160,after+160,128),"error wrapper preserves x87 control, MXCSR and every x86 XMM register");
    check(preserved==0x184c0deu,"error diagnostic preserves Win32 last-error state");
    probe_registers();const uint32_t expected[7]={0x12345678,0x23456789,0x3456789a,0x456789ab,0x56789abc,0x6789abcd,0x789abcde};
    check(!memcmp(helper_regs,expected,sizeof(expected))&&helper_original_calls==2,"error wrapper preserves all seven non-stack general registers");
    check(helper_flags_before==helper_flags_after,"error wrapper preserves complete incoming flags through own original stub");
    check(DeleteFileW(first)&&DeleteFileW(second),"helper deletes only its two newly created own opt-in files");
    VirtualFree(d,0,MEM_RELEASE);
    printf("RESULT checks=%u failures=%u; own data/unsupported helper only; no game or native engine executed\n",checks,failures);
    return failures?1:0;
}
