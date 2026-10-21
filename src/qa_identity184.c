/* Own MIT implementation. QA cached-identity offset concept from pinned MIT
 * stxlvn/mw2_coop@8fcb6bcf9980894a631906059558a4797f4c9752, game.c/control.c.
 * No upstream unrestricted delta/raw-ID logging, native engine call or hook.
 */
#include "qa_identity184.h"
#include "bootstrap.h"
#include <stddef.h>
#include <string.h>
_Static_assert(sizeof(void *)==4,"exact x86 build184 only");
_Static_assert(sizeof(qa_identity184_options)==32,"options v1 x86 ABI");
_Static_assert(sizeof(LONGLONG)==8,"eight-byte atomic cache");
#define QI_BASE ((uintptr_t)0x400000u)
#define QI_UID ((uintptr_t)0x19fbf88u)
typedef struct qi_requirement {uintptr_t va;uint32_t expected;unsigned bytes;} qi_requirement;
static const qi_requirement qi_requirements[]={
    {0x925bc0,1,4},{0x738c6c,1,4},{0x934450,0x1bde8b8,4},
    {0x10a3c3c,0,4},{0x10a3c10,0,4},{0x19fbfe4,0,1},
    {0x1beb4a1,0,1},{0x88897c,0,1},{0x1a7f0d9,0,1},
    {0x934448,0,1},{0x934449,0,1},{0x93444b,0,1},
    {0x1bde8c0,0,1},{0x1bde920,0,1},{0x1bde958,0,1},
    {0x1a6b0c1,0,1},{0x1a7f0f8,0,4},{0x88f560,0,4},{0x891b9c,0,4},
    {0x1a00b50,0,4},{0x19ff964,0,4},{0x1a00b60,0,4},
    {0x19ff94c,0,4},{0x19ff948,0,4},{0x734a4c,0,4},{0x1a00b64,0,4},
    {0x1bddf88,0,1},{0x1bde090,0,1},{0x1bde198,0,1},
    {0x1bde2a0,0,1},{0x1bde3a8,0,1},{0x1bde4b0,0,1},
    {0x19f623c,0,4},{0x19f6240,0,4},{0x19f6248,0,4},
    {0x19f69bc,0,4},{0x19f69c0,0,4},{0x19f69c8,0,4},
    {0x19f713c,0,4},{0x19f7140,0,4},{0x19f7148,0,4},
    {0x19f78bc,0,4},{0x19f78c0,0,4},{0x19f78c8,0,4}
};
#define QI_FIELDS (sizeof(qi_requirements)/sizeof(qi_requirements[0]))
typedef struct qi_snapshot {uint32_t value[QI_FIELDS];uint64_t uid;} qi_snapshot;
typedef struct qi_signature {uintptr_t va;unsigned bytes;uint8_t expected[20];} qi_signature;
static const qi_signature qi_signatures[]={
    {0x46f800,20,{0x8b,0x4c,0x24,4,0x6b,0xc9,0x68,0x8b,0x81,0x88,0xbf,0x9f,1,0x8b,0x91,0x8c,0xbf,0x9f,1,0xc3}},
    {0x47a650,12,{0x83,0xec,8,0x56,0x8b,0x35,0x20,0x15,0x69,0,0xff,0xd6}},
    {0x4fc841,16,{0xe8,0x0a,0xde,0xf7,0xff,0xa3,0x88,0xbf,0x9f,1,0x89,0x15,0x8c,0xbf,0x9f,1}},
    {0x4b2090,17,{0x8b,0x54,0x24,8,0x56,0x8b,0x74,0x24,8,0x57,0x33,0xc0,0xb9,0x88,0xbf,0x9f,1}},
    {0x65aa2d,20,{0xe8,0xce,0x4d,0xe1,0xff,0x83,0xc4,4,0x3b,0x44,0x24,0x10,0x75,0x2c,0x3b,0x54,0x24,0x14,0x75,0x26}}
};
static volatile LONG qi_attempted,qi_active,qi_fatal;
static DWORD qi_owner;
static uint64_t qi_original,qi_expected;
static uint32_t qi_checks,qi_observations;
static wchar_t qi_directory[MAX_PATH];
static int qi_mode;
static uint32_t qi_peer_low;
static qa_identity184_peer_scope qi_peer_callback;

static int qi_span(uintptr_t at,SIZE_T bytes,int code,int writable)
{
    MEMORY_BASIC_INFORMATION m;
    if(!bytes||VirtualQuery((void *)at,&m,sizeof(m))!=sizeof(m)
        ||m.State!=MEM_COMMIT||m.Type!=MEM_IMAGE||m.AllocationBase!=(void *)QI_BASE
        ||(m.Protect&PAGE_GUARD))return 0;
    DWORD p=m.Protect&0xffu;
    if(code){if(p!=PAGE_EXECUTE_READ)return 0;}
    else if(writable){if(p!=PAGE_READWRITE&&p!=PAGE_WRITECOPY)return 0;}
    else if(p!=PAGE_READONLY&&p!=PAGE_READWRITE&&p!=PAGE_WRITECOPY)return 0;
    uintptr_t begin=(uintptr_t)m.BaseAddress;
    return at>=begin&&at-begin<m.RegionSize&&bytes<=m.RegionSize-(at-begin);
}
static int qi_read(uintptr_t at,void *out,SIZE_T bytes)
{if(!qi_span(at,bytes,0,0))return 0;memcpy(out,(void *)at,bytes);return 1;}
static uint32_t qi_hash(uint64_t value)
{
    uint32_t h=2166136261u;
    for(unsigned i=0;i<8;++i){h=(h^(uint8_t)value)*16777619u;value>>=8;}
    return h;
}
static int qi_next(uint64_t before,uint32_t delta,uint64_t *after)
{
    uint32_t low=(uint32_t)before,high=(uint32_t)(before>>32);
    if(!after||delta!=1||!low||!high||low==UINT32_MAX)return 0;
    *after=(before&UINT64_C(0xffffffff00000000))|(uint64_t)(low+1u);return 1;
}
static int qi_peer_next(uint64_t before,uint32_t low,uint64_t *after)
{
    if(!after||!(uint32_t)before||!(uint32_t)(before>>32)||low<2||low>0x7fffffffu)return 0;
    *after=(before&UINT64_C(0xffffffff00000000))|low;return 1;
}
static int qi_atomic(volatile LONGLONG *at,uint64_t before,uint64_t after)
{
    if(!at||((uintptr_t)at&7u))return 0;
    return (uint64_t)InterlockedCompareExchange64(at,(LONGLONG)after,(LONGLONG)before)==before;
}
static int qi_values_fresh(const qi_snapshot *s,uint32_t *failed)
{
    for(unsigned i=0;i<QI_FIELDS;++i){
        /* Both pre-world frontend states are valid only with every socket,
         * party/channel/queue and level binding still independently empty. */
        if(qi_requirements[i].va==0x925bc0u&&s->value[i]<=1u)continue;
        /* NET_Config can record enabled1 even though OpenIP never opened a
           socket. All actual socket/proxy/queue/party fields still must zero. */
        if(qi_requirements[i].va==0x1a00b64u&&s->value[i]<=1u)continue;
        if(s->value[i]!=qi_requirements[i].expected){*failed=i+1;return 0;}
    }
    return 1;
}
static int qi_capture(qi_snapshot *s,uint32_t *failed)
{
    memset(s,0,sizeof(*s));
    for(unsigned i=0;i<QI_FIELDS;++i)if(!qi_read(qi_requirements[i].va,&s->value[i],qi_requirements[i].bytes)){
        *failed=i+1;return 0;}
    if(!qi_read(QI_UID,&s->uid,8)){*failed=(uint32_t)QI_FIELDS+1;return 0;}
    return qi_values_fresh(s,failed);
}
static int qi_code(qa_identity184_report *r)
{
    for(unsigned i=0;i<sizeof(qi_signatures)/sizeof(qi_signatures[0]);++i){const qi_signature *s=&qi_signatures[i];
        if(!qi_span(s->va,s->bytes,1,0)||memcmp((void *)s->va,s->expected,s->bytes)){
            r->failed_site=i+1;return 0;}
        ++qi_checks;
    }
    return 1;
}
static int qi_regular(const wchar_t *name)
{
    wchar_t path[MAX_PATH];if(!qi_directory[0])return 0;
    size_t n=(size_t)lstrlenW(qi_directory),suffix=(size_t)lstrlenW(name);
    if(n+suffix>=MAX_PATH)return 0;lstrcpyW(path,qi_directory);lstrcatW(path,name);
    HANDLE f=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(f==INVALID_HANDLE_VALUE)return 0;
    BY_HANDLE_FILE_INFORMATION info;int ok=GetFileInformationByHandle(f,&info)
        &&!(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
    CloseHandle(f);return ok;
}
static int qi_flags(void)
{
    int mode=qi_mode==2
        ?qi_regular(L"mw2-trio-peer-test.enable")&&qi_regular(L"mw2-trio-network-test.enable")
            &&!qi_regular(L"mw2-trio-loopback-qa.enable")
        :qi_regular(L"mw2-trio-loopback-qa.enable");
    return mode&&qi_regular(L"mw2-trio-party-test.enable")&&qi_regular(L"mw2-trio-control.enable");
}
static int qi_paths(HMODULE module)
{
    DWORD n=GetModuleFileNameW(module,qi_directory,MAX_PATH);
    if(!n||n>=MAX_PATH){qi_directory[0]=0;return 0;}
    while(n&&qi_directory[n-1]!=L'\\')--n;qi_directory[n]=0;
    return n&&n+40<MAX_PATH;
}
static int qi_scope(void)
{
    uint8_t online=1,lobby=1;
    uint32_t low=0;
    return (qi_mode!=2||(qi_peer_callback&&qi_peer_callback(&low)==1&&low==qi_peer_low))
        &&qi_flags()&&qi_read(0x19fbfe4,&online,1)&&qi_read(0x1beb4a1,&lobby,1)&&!online&&!lobby;
}
static void qi_report(qa_identity184_report *r)
{
    memset(r,0,sizeof(*r));r->version=QA_IDENTITY184_VERSION;r->bytes=sizeof(*r);
    r->attempted=(uint32_t)qi_attempted;r->active=(uint32_t)qi_active;r->fatal=(uint32_t)qi_fatal;
    r->checked_sites=qi_checks;r->observations=qi_observations;
    if(qi_active){r->original_hash=qi_hash(qi_original);r->expected_hash=qi_hash(qi_expected);}
}
static int qi_result(qa_identity184_report *r,int result)
{
    r->attempted=(uint32_t)qi_attempted;r->active=(uint32_t)qi_active;r->fatal=(uint32_t)qi_fatal;
    r->checked_sites=qi_checks;r->observations=qi_observations;r->result=result;return result;
}
static int qi_cache_observation(uint64_t observed,int readable,qa_identity184_report *r)
{
    if(!readable||observed!=qi_expected){
        InterlockedExchange(&qi_fatal,1);return qi_result(r,QA_IDENTITY184_UID_CHANGED);}
    return qi_result(r,QA_IDENTITY184_OK);
}
static int qi_apply(const qa_identity184_options *o,qa_identity184_report *r,
    int mode,uint32_t peer_low,qa_identity184_peer_scope callback)
{
    if(!r)return QA_IDENTITY184_BAD_ARGUMENT;qi_report(r);
    MEMORY_BASIC_INFORMATION own;
    if(!o||o->version!=QA_IDENTITY184_VERSION||o->bytes!=sizeof(*o)||!o->module||!o->main_thread_id
        ||!o->native_frame_ordinal||o->client_role!=1||o->delta!=1||o->loopback_enforced!=1
        ||VirtualQuery((void *)(uintptr_t)&qa_identity184_apply_client,&own,sizeof(own))!=sizeof(own)
        ||own.AllocationBase!=o->module)return qi_result(r,QA_IDENTITY184_BAD_ARGUMENT);
    if(GetCurrentThreadId()!=o->main_thread_id)return qi_result(r,QA_IDENTITY184_WRONG_THREAD);
    if(qi_attempted)return qi_result(r,QA_IDENTITY184_ALREADY_ATTEMPTED);
    qi_mode=mode;qi_peer_low=peer_low;qi_peer_callback=callback;
    if(!qi_paths(o->module)||!qi_flags())return qi_result(r,QA_IDENTITY184_DISABLED);
    if(InterlockedCompareExchange(&qi_attempted,1,0))return qi_result(r,QA_IDENTITY184_ALREADY_ATTEMPTED);
    bootstrap_report identity;
    if((uintptr_t)GetModuleHandleW(NULL)!=QI_BASE||bootstrap_guard_current(&identity,sizeof(identity))!=1
        ||identity.flags!=MW2TRIO_GUARD_ALL)return qi_result(r,QA_IDENTITY184_WRONG_BUILD);
    if(!qi_code(r))return qi_result(r,QA_IDENTITY184_BYTES_DIFFER);
    qi_snapshot first,second;uint64_t next;
    if(!qi_capture(&first,&r->failed_precondition))return qi_result(r,QA_IDENTITY184_NOT_FRESH);
    if(!(mode==2?qi_peer_next(first.uid,peer_low,&next):qi_next(first.uid,o->delta,&next)))
        return qi_result(r,QA_IDENTITY184_BAD_UID);
    HMODULE pinned=NULL;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCWSTR)(uintptr_t)&qa_identity184_apply_client,&pinned)||pinned!=o->module){
        r->win32_error=GetLastError();return qi_result(r,QA_IDENTITY184_PIN_FAILED);}
    uint32_t checked_low=0;
    if(!qi_flags()||(mode==2&&(!callback||callback(&checked_low)!=1||checked_low!=peer_low)))
        return qi_result(r,QA_IDENTITY184_SCOPE_LOST);
    if(!qi_code(r))return qi_result(r,QA_IDENTITY184_BYTES_DIFFER);
    if(!qi_capture(&second,&r->failed_precondition))return qi_result(r,QA_IDENTITY184_NOT_FRESH);
    if(first.uid!=second.uid||!qi_span(QI_UID,8,0,1))return qi_result(r,QA_IDENTITY184_UID_CHANGED);
    qi_owner=o->main_thread_id;qi_original=first.uid;qi_expected=next;
    if(!qi_atomic((volatile LONGLONG *)QI_UID,qi_original,qi_expected)){
        InterlockedExchange(&qi_fatal,1);return qi_result(r,QA_IDENTITY184_UID_CHANGED);}
    InterlockedExchange(&qi_active,1);r->original_hash=qi_hash(qi_original);r->expected_hash=qi_hash(qi_expected);
    return qi_result(r,QA_IDENTITY184_OK);
}
int __cdecl qa_identity184_apply_client(const qa_identity184_options *o,qa_identity184_report *r)
{return qi_apply(o,r,1,0,NULL);}
int __cdecl qa_identity184_apply_peer(const qa_identity184_peer_options *o,qa_identity184_report *r)
{
    if(!r)return QA_IDENTITY184_BAD_ARGUMENT;qi_report(r);
    MEMORY_BASIC_INFORMATION scope_memory;
    if(!o||o->version!=QA_IDENTITY184_VERSION||o->bytes!=sizeof(*o)||!o->scope
        ||(o->role!=1&&o->role!=2)||o->derived_low<2||o->derived_low>0x7fffffffu
        ||(o->derived_low&1u)!=(o->role-1u)
        ||VirtualQuery((void *)(uintptr_t)o->scope,&scope_memory,sizeof(scope_memory))!=sizeof(scope_memory)
        ||scope_memory.AllocationBase!=o->module||scope_memory.State!=MEM_COMMIT
        ||(scope_memory.Protect&(PAGE_GUARD|PAGE_NOACCESS))
        ||(scope_memory.Protect&0xffu)!=PAGE_EXECUTE_READ)
        return qi_result(r,QA_IDENTITY184_BAD_ARGUMENT);
    if(GetCurrentThreadId()!=o->main_thread_id)return qi_result(r,QA_IDENTITY184_WRONG_THREAD);
    if(qi_attempted)return qi_result(r,QA_IDENTITY184_ALREADY_ATTEMPTED);
    uint32_t low=0;
    if(o->scope(&low)!=1||low!=o->derived_low)return qi_result(r,QA_IDENTITY184_SCOPE_LOST);
    qa_identity184_options common={QA_IDENTITY184_VERSION,sizeof(common),o->module,o->main_thread_id,
        o->native_frame_ordinal,1,1,1};
    return qi_apply(&common,r,2,low,o->scope);
}
int __cdecl qa_identity184_observe(qa_identity184_report *r)
{
    if(!r)return QA_IDENTITY184_BAD_ARGUMENT;qi_report(r);
    if(qi_fatal)return qi_result(r,QA_IDENTITY184_UID_CHANGED);
    if(!qi_active)return qi_result(r,QA_IDENTITY184_DISABLED);
    if(GetCurrentThreadId()!=qi_owner){InterlockedExchange(&qi_fatal,1);return qi_result(r,QA_IDENTITY184_WRONG_THREAD);}
    ++qi_observations;
    uint64_t observed;
    if(!qi_scope()){InterlockedExchange(&qi_fatal,1);return qi_result(r,QA_IDENTITY184_SCOPE_LOST);}
    if(!qi_code(r)){InterlockedExchange(&qi_fatal,1);return qi_result(r,QA_IDENTITY184_BYTES_DIFFER);}
    int readable=qi_read(QI_UID,&observed,8);
    return qi_cache_observation(readable?observed:0,readable,r);
}
