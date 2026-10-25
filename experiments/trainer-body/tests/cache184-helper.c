/* Own x86 process only. Includes the unchanged production module so private
   transaction/reset internals can be tested on owned pages/buffers. No fixture
   instruction is executed. Public negative tests use the actual bootstrap guard. */
#include "../src/cache184.c"
#include <stdio.h>

static int failures,passes,protect_calls,flush_calls,mode,once,original_calls;
static int entered,left;
static void check(int ok,const char *name)
{
    printf("%s %s\n",ok?"PASS":"FAIL",name);if(ok)++passes;else++failures;
}
static int all_byte(const void *data,size_t bytes,unsigned char value)
{
    const unsigned char *p=data;for(size_t i=0;i<bytes;++i)if(p[i]!=value)return 0;return 1;
}
static BOOL WINAPI test_protect(void *address,SIZE_T bytes,DWORD protection,DWORD *old)
{
    ++protect_calls;
    if((mode==2&&protection==PAGE_EXECUTE_READ&&!once)
        ||(mode==3&&protection==PAGE_EXECUTE_READWRITE&&flush_calls)) {
        once=1;SetLastError(ERROR_ACCESS_DENIED);return FALSE;
    }
    return VirtualProtect(address,bytes,protection,old);
}
static BOOL WINAPI test_flush(HANDLE process,const void *address,SIZE_T bytes)
{
    ++flush_calls;
    if((mode==1||mode==3)&&flush_calls==1){SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return FlushInstructionCache(process,address,bytes);
}
static const cache_ops test_ops={test_protect,test_flush};
static void fault_mode(int selected){mode=selected;once=0;protect_calls=0;flush_calls=0;}
static int __cdecl test_enter(void *context){(void)context;++entered;return 1;}
static void __cdecl test_leave(void *context){(void)context;++left;}
static void *__cdecl test_memset(void *dst,int value,size_t bytes)
{
    ++original_calls;return memset(dst,value,bytes);
}
static void *__cdecl test_corrupt_guard(void *dst,int value,size_t bytes)
{
    ++original_calls;memset(dst,value,bytes);cache_allocation.data[-1]=0;return dst;
}
static void reset_failure_state(void){cache_fatal=0;cache_last.reset_failure=0;}
static int pages_rx(void *base,SIZE_T bytes,SIZE_T page)
{
    for(SIZE_T at=0;at<bytes;at+=page){MEMORY_BASIC_INFORMATION mi;if(!VirtualQuery((uint8_t *)base+at,&mi,sizeof(mi))||mi.Protect!=PAGE_EXECUTE_READ)return 0;}return 1;
}
static void fixture_original(uint8_t *fixture,SIZE_T bytes,const uint8_t *original)
{
    DWORD old;VirtualProtect(fixture,bytes,PAGE_READWRITE,&old);memcpy(fixture,original,bytes);
    VirtualProtect(fixture,bytes,PAGE_EXECUTE_READ,&old);FlushInstructionCache(GetCurrentProcess(),fixture,bytes);
}
int main(void)
{
    wchar_t flag[MAX_PATH];DWORD n=GetModuleFileNameW(NULL,flag,MAX_PATH);
    if(!n||n>=MAX_PATH-40)return 2;while(n&&flag[n-1]!=L'\\')--n;
    flag[n]=0;lstrcatW(flag,L"mw2-trio-cache-test.enable");
    if(GetFileAttributesW(flag)!=INVALID_FILE_ATTRIBUTES){puts("FAIL fresh own helper flag required");return 2;}
    cache184_options o={CACHE184_API_VERSION,sizeof(o),GetModuleHandleW(NULL),GetCurrentThreadId(),1,test_enter,test_leave,NULL};
    cache184_report r;
    check(cache184_install_preengine(NULL,&r)==CACHE184_BAD_ARGUMENT,"public refuses NULL options");
    check(cache184_install_preengine(&o,NULL)==CACHE184_BAD_ARGUMENT,"public refuses NULL report");
    --o.bytes;check(cache184_install_preengine(&o,&r)==CACHE184_BAD_ARGUMENT,"public refuses options ABI mismatch");++o.bytes;
    ++o.main_thread_id;check(cache184_install_preengine(&o,&r)==CACHE184_WRONG_THREAD,"public refuses wrong owner thread");--o.main_thread_id;
    o.preengine_epoch=2;check(cache184_install_preengine(&o,&r)==CACHE184_NOT_PREENGINE,"public refuses later startup epoch");o.preengine_epoch=1;
    check(cache184_install_preengine(&o,&r)==CACHE184_DISABLED,"public remains disabled without cache opt-in");
    HANDLE file=CreateFileW(flag,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 2;DWORD written;WriteFile(file,"own helper only\n",16,&written,NULL);CloseHandle(file);
    o.enter_quiescence=NULL;check(cache184_install_preengine(&o,&r)==CACHE184_NO_QUIESCENCE,"public refuses absent quiescence callback");o.enter_quiescence=test_enter;
    bootstrap_report identity;
    check(bootstrap_guard_current(&identity,sizeof(identity))==0&&identity.flags!=MW2TRIO_GUARD_ALL,"real pure-hash guard rejects own helper image");
    check(cache184_install_preengine(&o,&r)==CACHE184_WRONG_BUILD,"opt-in public installer rejects unsupported own image");
    check(!entered&&!left&&!cache_allocation.reservation&&!cache_trusted_host,"unsupported installer does not enter barrier, allocate, or trust host");
    check(cache184_observe(&r)==CACHE184_DISABLED,"inactive observer is harmless");

    check(cache_allocate(&cache_allocation)&&cache_guards(&cache_allocation),"guard pages and complete canary padding allocated");
    if(!cache_allocation.data){DeleteFileW(flag);return 1;}
    check(((uintptr_t)cache_allocation.data&(cache_allocation.page_bytes-1))==(CACHE_OLD_BASE&(cache_allocation.page_bytes-1))
        &&all_byte(cache_allocation.data,CACHE_BYTES,0),"three-record allocation preserves residue and starts zero");
    SIZE_T page=cache_allocation.page_bytes,total=3*page;
    uint8_t *fixture=VirtualAlloc(NULL,total,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    uint8_t *old_image=HeapAlloc(GetProcessHeap(),0,total),*new_image=HeapAlloc(GetProcessHeap(),0,total);
    if(!fixture||!old_image||!new_image){DeleteFileW(flag);return 2;}
    memset(fixture,0xcc,total);uintptr_t addresses[CACHE184_SITE_COUNT];
    for(unsigned i=0;i<CACHE184_SITE_COUNT;++i)addresses[i]=(uintptr_t)(fixture+(i? page+32+i*32:page-8));
    cache_plan plan[CACHE184_SITE_COUNT];cache_make_plan(plan,(uintptr_t)cache_allocation.data,addresses);
    unsigned ends=0;int ends_correct=1;
    for(unsigned i=0;i<CACHE184_SITE_COUNT;++i)if(cache_sites[i].kind==CACHE_END) {
        uint32_t endpoint;memcpy(&endpoint,plan[i].new_bytes+plan[i].offset,4);++ends;
        if(endpoint!=(uint32_t)(uintptr_t)cache_allocation.data+2*CACHE_STRIDE+cache_sites[i].field)ends_correct=0;
    }
    check(ends==4&&ends_correct&&CACHE_ALLOCATION_COUNT==3&&CACHE_TRAVERSAL_COUNT==2&&CACHE_BYTES==0xa8,
          "all four end formulas preserve traversal2 while guarded allocation is3");
    for(unsigned i=0;i<CACHE184_SITE_COUNT;++i)memcpy((void *)plan[i].va,plan[i].old_bytes,plan[i].length);
    memcpy(old_image,fixture,total);memcpy(new_image,old_image,total);
    for(unsigned i=0;i<CACHE184_SITE_COUNT;++i)memcpy(new_image+plan[i].va-(uintptr_t)fixture+plan[i].offset,plan[i].new_bytes+plan[i].offset,4);
    DWORD protection;VirtualProtect(fixture,total,PAGE_EXECUTE_READ,&protection);
    cache_report_init(&r);fault_mode(0);
    check(cache_transaction(plan,1,MEM_PRIVATE,fixture,&test_ops,&r)==CACHE184_OK&&r.checked_sites==32&&r.protections_restored,"real 32-site transaction succeeds, including cross-page instruction");
    check(!memcmp(fixture,new_image,total)&&pages_rx(fixture,total,page)&&cache_guards(&cache_allocation),"only selected operands change; code protections and cache guards preserved");
    cache_report_init(&r);fault_mode(0);
    check(cache_transaction(plan,0,MEM_PRIVATE,fixture,&test_ops,&r)==CACHE184_OK&&!memcmp(fixture,old_image,total)&&pages_rx(fixture,total,page),"explicit transaction rollback restores entire original fixture");
    VirtualProtect(fixture,total,PAGE_READWRITE,&protection);((uint8_t *)plan[0].va)[10]^=1;
    VirtualProtect(fixture,total,PAGE_EXECUTE_READ,&protection);memcpy(new_image,fixture,total);
    cache_report_init(&r);fault_mode(0);
    check(cache_transaction(plan,1,MEM_PRIVATE,fixture,&test_ops,&r)==CACHE184_BYTES_DIFFER&&r.failed_site==0&&!protect_calls&&!memcmp(fixture,new_image,total),"full-instruction mismatch refuses before any protection/write");
    fixture_original(fixture,total,old_image);cache_report_init(&r);fault_mode(0);
    check(cache_transaction(plan,1,MEM_IMAGE,fixture,&test_ops,&r)==CACHE184_TRANSACTION_FAILED&&!protect_calls&&!memcmp(fixture,old_image,total),"wrong allocation type refuses before write");
    cache_report_init(&r);fault_mode(1);
    check(cache_transaction(plan,1,MEM_PRIVATE,fixture,&test_ops,&r)==CACHE184_TRANSACTION_FAILED&&!memcmp(fixture,old_image,total)&&pages_rx(fixture,total,page),"injected flush failure rolls back bytes and protections");
    cache_report_init(&r);fault_mode(2);
    check(cache_transaction(plan,1,MEM_PRIVATE,fixture,&test_ops,&r)==CACHE184_TRANSACTION_FAILED&&!memcmp(fixture,old_image,total)&&pages_rx(fixture,total,page),"injected restore failure recovers through checked rollback");
    cache_report_init(&r);fault_mode(3);
    check(cache_transaction(plan,1,MEM_PRIVATE,fixture,&test_ops,&r)==CACHE184_FATAL_RESTORE_FAILED&&cache_allocation.data&&pages_rx(fixture,total,page),"rollback write-protection failure reports fatal and keeps allocation");
    /* The fixture CALL bytes are never executed. Recover own pages directly. */
    fixture_original(fixture,total,old_image);fault_mode(0);

    unsigned char prefix[16+CACHE_PREFIX_BYTES+16],saved_prefix[sizeof(prefix)],saved_cache[CACHE_BYTES];
    memset(prefix,0x5a,sizeof(prefix));void *dst=prefix+16;int ok;
    cache_installed=1;cache_trusted_host=1;cache_owner_tid=GetCurrentThreadId();cache_owner_pid=GetCurrentProcessId();
    memset(&cache_last,0,sizeof(cache_last));reset_failure_state();
    memset(dst,0x71,CACHE_PREFIX_BYTES);memset(cache_allocation.data,0x72,CACHE_BYTES);
    void *returned=cache_reset_checked(dst,0,CACHE_PREFIX_BYTES,0x12345678,dst,0x12345678,test_memset,&ok);
    check(ok&&returned==dst&&original_calls==1&&cache_last.reset_calls==1&&cache_last.reset_markers==0x7f,"reset preserves cdecl memset return and complete phase markers");
    check(all_byte(dst,CACHE_PREFIX_BYTES,0)&&all_byte(prefix,16,0x5a)&&all_byte(prefix+16+CACHE_PREFIX_BYTES,16,0x5a)
        &&all_byte(cache_allocation.data,CACHE_BYTES,0)&&cache_guards(&cache_allocation),"original 0x7C prefix and all three records zero; neighbors/canaries preserved");
    memset(dst,0x73,CACHE_PREFIX_BYTES);memset(cache_allocation.data,0x74,CACHE_BYTES);
    returned=cache_reset_checked(dst,0,CACHE_PREFIX_BYTES,0x12345678,dst,0x12345678,test_memset,&ok);
    check(ok&&returned==dst&&original_calls==2&&cache_last.reset_calls==2&&all_byte(cache_allocation.data,CACHE_BYTES,0),"second reset clears all three records again");
    memset(dst,0x75,CACHE_PREFIX_BYTES);memset(cache_allocation.data,0x76,CACHE_BYTES);
    memcpy(saved_prefix,prefix,sizeof(prefix));memcpy(saved_cache,cache_allocation.data,CACHE_BYTES);reset_failure_state();
    cache_reset_checked((uint8_t *)dst+4,0,CACHE_PREFIX_BYTES,0x12345678,dst,0x12345678,test_memset,&ok);
    check(!ok&&cache_fatal&&cache_last.reset_failure==CACHE184_RESET_FAILURE_ARGS&&original_calls==2
        &&!memcmp(prefix,saved_prefix,sizeof(prefix))&&!memcmp(cache_allocation.data,saved_cache,CACHE_BYTES),"mismatched destination refuses without clearing any address");
    check(cache184_observe(&r)==CACHE184_FATAL_RESET_CONTRACT&&r.fatal&&r.reset_phase==CACHE184_RESET_REFUSED&&r.reset_destination==(uint32_t)(uintptr_t)((uint8_t *)dst+4),"observer exposes reset fatal/phase/argument marker");
    reset_failure_state();
    cache_reset_checked(dst,0,(size_t)(int32_t)(int8_t)0xb4,0x12345678,dst,0x12345678,test_memset,&ok);
    check(!ok&&original_calls==2&&cache_last.reset_failure==CACHE184_RESET_FAILURE_ARGS&&!memcmp(prefix,saved_prefix,sizeof(prefix)),"signed-imm8 negative-size hazard is rejected, never used");
    reset_failure_state();cache_reset_checked(dst,0,CACHE_PREFIX_BYTES,0x87654321,dst,0x12345678,test_memset,&ok);
    check(!ok&&original_calls==2&&cache_last.reset_failure==CACHE184_RESET_FAILURE_CALLER,"unexpected constructor caller refuses before memset");
    reset_failure_state();++cache_owner_tid;cache_reset_checked(dst,0,CACHE_PREFIX_BYTES,0x12345678,dst,0x12345678,test_memset,&ok);--cache_owner_tid;
    check(!ok&&original_calls==2&&cache_last.reset_failure==CACHE184_RESET_FAILURE_THREAD,"wrong reset thread refuses before memset");
    reset_failure_state();cache_allocation.data[-1]=0;
    cache_reset_checked(dst,0,CACHE_PREFIX_BYTES,0x12345678,dst,0x12345678,test_memset,&ok);
    check(!ok&&original_calls==2&&cache_last.reset_failure==CACHE184_RESET_FAILURE_GUARD_BEFORE,"corrupt pre-reset canary prevents original clear");
    cache_allocation.data[-1]=CACHE_CANARY;reset_failure_state();
    cache_reset_checked(dst,0,CACHE_PREFIX_BYTES,0x12345678,dst,0x12345678,test_corrupt_guard,&ok);
    check(!ok&&original_calls==3&&cache_last.reset_failure==CACHE184_RESET_FAILURE_GUARD_AFTER,"post-reset guard check catches injected corruption");
    cache_allocation.data[-1]=CACHE_CANARY;reset_failure_state();cache_runtime_observed=0;
    check(cache184_rollback_preengine(&o,&r)==CACHE184_NOT_PREENGINE,"rollback forbidden after constructor reset");
    cache_last.reset_calls=0;check(cache184_observe(&r)==CACHE184_OK&&r.runtime_observed&&r.guards_ok&&r.allocation_count==3&&r.traversal_count==2,
        "healthy owner observer reports allocation3/traversal2 and latches runtime phase");
    check(cache184_rollback_preengine(&o,&r)==CACHE184_NOT_PREENGINE,"rollback forbidden after first observer frame");
    ++cache_owner_tid;check(cache184_observe(&r)==CACHE184_WRONG_THREAD,"observer refuses wrong owner thread");--cache_owner_tid;
    cache_installed=0;cache_trusted_host=0;cache_fatal=0;cache_release(&cache_allocation);
    VirtualFree(fixture,0,MEM_RELEASE);HeapFree(GetProcessHeap(),0,old_image);HeapFree(GetProcessHeap(),0,new_image);
    check(DeleteFileW(flag),"helper removes only its own opt-in file");
    printf("RESULT passes=%d failures=%d\n",passes,failures);return failures?1:0;
}
