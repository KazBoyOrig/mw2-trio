/* Own code; MIT. Opt-in pre-engine exact-build cache-only experiment.
   No index-limit/roster/wire/UI/auth patch. Only 31 classified operands and one
   constructor CALL operand may change. See private cache-three184.json audit. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "bootstrap.h"
#include "cache184.h"
#include "loader_safe.h"

_Static_assert(sizeof(void *)==4,"cache184 requires x86");
_Static_assert(sizeof(cache184_options)==32,"cache184 options ABI");
_Static_assert(sizeof(cache184_report)==96,"cache184 report ABI");
#define CACHE_OLD_BASE 0x00934454u
#define CACHE_STRIDE 0x38u
#define CACHE_ALLOCATION_COUNT CACHE184_ALLOCATION_COUNT
#define CACHE_TRAVERSAL_COUNT CACHE184_TRAVERSAL_COUNT
#define CACHE_BYTES (CACHE_ALLOCATION_COUNT*CACHE_STRIDE)
#define CACHE_PREFIX 0x00934448u
#define CACHE_PREFIX_BYTES 0x7cu
#define CACHE_RESET_CALL 0x0048e669u
#define CACHE_RESET_RETURN 0x0048e66eu
#define CACHE_MEMSET 0x00671ac0u
#define CACHE_RESET_EXIT 0xe1840040u
#define CACHE_CANARY 0xc3u
#define CACHE_CANARY_MIN 64u
#define CACHE_MAX_PAGES (CACHE184_SITE_COUNT*2u)

enum cache_kind { CACHE_ADDRESS, CACHE_END, CACHE_RESET_CALL_KIND };
typedef struct cache_site {
    uintptr_t va;
    uint8_t length, offset, kind, field;
    uint8_t original[11];
} cache_site;
static const cache_site cache_sites[CACHE184_SITE_COUNT]={
    {0x4141d8,11,3,CACHE_ADDRESS,0x2c,{0xc7,0x04,0xcd,0x80,0x44,0x93,0x00,0x00,0x00,0x00,0x00}},
    {0x4141ec,7,3,CACHE_ADDRESS,0x2c,{0x8b,0x04,0xd5,0x80,0x44,0x93,0x00}},
    {0x43d2b8,11,3,CACHE_ADDRESS,0x2c,{0xc7,0x04,0xcd,0x80,0x44,0x93,0x00,0x00,0x00,0x00,0x00}},
    {0x43d2cc,7,3,CACHE_ADDRESS,0x2c,{0x8b,0x04,0xd5,0x80,0x44,0x93,0x00}},
    {0x444332,6,2,CACHE_ADDRESS,0x24,{0x89,0x86,0x78,0x44,0x93,0x00}},
    {0x44433f,6,2,CACHE_ADDRESS,0,{0x8d,0x8e,0x54,0x44,0x93,0x00}},
    {0x444355,6,2,CACHE_ADDRESS,0x28,{0x89,0x96,0x7c,0x44,0x93,0x00}},
    {0x448be9,6,2,CACHE_ADDRESS,0x34,{0x89,0x90,0x88,0x44,0x93,0x00}},
    {0x448bef,6,2,CACHE_ADDRESS,0,{0x8d,0x80,0x54,0x44,0x93,0x00}},
    {0x45a020,7,3,CACHE_ADDRESS,0x24,{0x89,0x04,0xcd,0x78,0x44,0x93,0x00}},
    {0x45ddfc,5,1,CACHE_ADDRESS,0x24,{0xbe,0x78,0x44,0x93,0x00}},
    {0x45de54,6,2,CACHE_END,0x24,{0x81,0xfe,0xe8,0x44,0x93,0x00}},
    {0x48f299,6,2,CACHE_ADDRESS,0x30,{0x89,0x90,0x84,0x44,0x93,0x00}},
    {0x48f29f,6,2,CACHE_ADDRESS,0,{0x8d,0x80,0x54,0x44,0x93,0x00}},
    {0x490bc1,7,3,CACHE_ADDRESS,0,{0x8d,0x04,0xc5,0x54,0x44,0x93,0x00}},
    {0x4a5c2d,7,3,CACHE_ADDRESS,0x28,{0x8b,0x04,0xcd,0x7c,0x44,0x93,0x00}},
    {0x4faaf1,7,3,CACHE_ADDRESS,0x28,{0x89,0x14,0xcd,0x7c,0x44,0x93,0x00}},
    {0x57bbfe,7,3,CACHE_ADDRESS,0x24,{0x89,0x04,0xd5,0x78,0x44,0x93,0x00}},
    {0x57bc61,7,3,CACHE_ADDRESS,0,{0x8d,0x04,0xd5,0x54,0x44,0x93,0x00}},
    {0x57bd53,7,3,CACHE_ADDRESS,0x28,{0x89,0x04,0xcd,0x7c,0x44,0x93,0x00}},
    {0x57be19,7,3,CACHE_ADDRESS,0x2c,{0x89,0x04,0xcd,0x80,0x44,0x93,0x00}},
    {0x57c05c,7,3,CACHE_ADDRESS,0x28,{0x8b,0x04,0xd5,0x7c,0x44,0x93,0x00}},
    {0x57c239,6,2,CACHE_ADDRESS,0x28,{0x89,0xb8,0x7c,0x44,0x93,0x00}},
    {0x57c244,6,2,CACHE_ADDRESS,0x2c,{0x89,0x88,0x80,0x44,0x93,0x00}},
    {0x57c2de,7,3,CACHE_ADDRESS,0x24,{0x89,0x34,0xcd,0x78,0x44,0x93,0x00}},
    {0x57c4f0,5,1,CACHE_ADDRESS,0x2c,{0xbf,0x80,0x44,0x93,0x00}},
    {0x57c5f6,6,2,CACHE_END,0x2c,{0x81,0xff,0xf0,0x44,0x93,0x00}},
    {0x57c613,5,1,CACHE_ADDRESS,0x20,{0xbe,0x74,0x44,0x93,0x00}},
    {0x57c6c7,6,2,CACHE_END,0x20,{0x81,0xfe,0xe4,0x44,0x93,0x00}},
    {0x57c6ec,5,1,CACHE_ADDRESS,0,{0xbf,0x54,0x44,0x93,0x00}},
    {0x57c780,6,2,CACHE_END,0,{0x81,0xff,0xc4,0x44,0x93,0x00}},
    {CACHE_RESET_CALL,5,1,CACHE_RESET_CALL_KIND,0,{0xe8,0x52,0x34,0x1e,0x00}}
};
typedef struct cache_buffer {
    uint8_t *reservation,*committed,*data;
    SIZE_T reserved_bytes,committed_bytes,prefix_bytes,page_bytes;
} cache_buffer;
typedef struct cache_plan {
    uintptr_t va;
    uint8_t length,offset;
    uint8_t old_bytes[11],new_bytes[11];
} cache_plan;
typedef struct cache_page { void *address; DWORD original; int writable; } cache_page;
typedef struct cache_ops {
    BOOL (WINAPI *protect)(LPVOID,SIZE_T,DWORD,PDWORD);
    BOOL (WINAPI *flush)(HANDLE,LPCVOID,SIZE_T);
} cache_ops;
typedef void *(__cdecl *cache_memset_fn)(void *,int,size_t);
static const cache_ops cache_native_ops={VirtualProtect,FlushInstructionCache};
static cache_buffer cache_allocation;
static cache_plan cache_native_plan[CACHE184_SITE_COUNT];
static volatile LONG cache_attempted,cache_installed,cache_fatal,cache_runtime_observed;
static DWORD cache_owner_tid,cache_owner_pid;
static HMODULE cache_owner_module;
static int cache_trusted_host;
static cache184_report cache_last;

static int cache_read(uintptr_t address,void *out,SIZE_T bytes)
{
    SIZE_T done=0;
    while(done<bytes) {
        uintptr_t at=address+done; MEMORY_BASIC_INFORMATION mi;
        if(at<address||VirtualQuery((void *)at,&mi,sizeof(mi))!=sizeof(mi))return 0;
        DWORD p=mi.Protect&0xffu;
        if(mi.State!=MEM_COMMIT||(mi.Protect&PAGE_GUARD)
            ||(p!=PAGE_READONLY&&p!=PAGE_READWRITE&&p!=PAGE_WRITECOPY
            &&p!=PAGE_EXECUTE_READ&&p!=PAGE_EXECUTE_READWRITE&&p!=PAGE_EXECUTE_WRITECOPY))return 0;
        uintptr_t start=(uintptr_t)mi.BaseAddress;
        if(at<start||at-start>=mi.RegionSize)return 0;
        SIZE_T take=mi.RegionSize-(at-start);if(take>bytes-done)take=bytes-done;
        memcpy((uint8_t *)out+done,(void *)at,take);done+=take;
    }
    return 1;
}
static int cache_initial_phase(void)
{
    uint32_t base,count,entities,local;uint8_t coop;
    return cache_read(0x010a3c10u,&base,4)&&!base&&cache_read(0x010a3c3cu,&count,4)&&!count
        &&cache_read(0x010a3c18u,&entities,4)&&!entities&&cache_read(0x00738c6cu,&local,4)&&local==1
        &&cache_read(0x0088897cu,&coop,1)&&!coop;
}
static int cache_enable(HMODULE module)
{
    wchar_t path[MAX_PATH];const wchar_t name[]=L"mw2-trio-cache-test.enable";
    DWORD n=GetModuleFileNameW(module,path,MAX_PATH);
    if(!n||n>=MAX_PATH)return 0;
    while(n&&path[n-1]!=L'\\'&&path[n-1]!=L'/')--n;
    if(!n||n+sizeof(name)/sizeof(name[0])>MAX_PATH)return 0;
    memcpy(path+n,name,sizeof(name));
    HANDLE f=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(f==INVALID_HANDLE_VALUE)return 0;
    BY_HANDLE_FILE_INFORMATION info;
    int ok=GetFileInformationByHandle(f,&info)&&!(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
    CloseHandle(f);return ok;
}
static int cache_allocate(cache_buffer *g)
{
    SYSTEM_INFO si;GetSystemInfo(&si);g->page_bytes=si.dwPageSize;
    if(!g->page_bytes||(g->page_bytes&(g->page_bytes-1)))return 0;
    g->prefix_bytes=CACHE_CANARY_MIN+((CACHE_OLD_BASE-CACHE_CANARY_MIN)&(g->page_bytes-1));
    g->committed_bytes=(g->prefix_bytes+CACHE_BYTES+CACHE_CANARY_MIN+g->page_bytes-1)&~(g->page_bytes-1);
    g->reserved_bytes=g->committed_bytes+2*g->page_bytes;
    g->reservation=VirtualAlloc(NULL,g->reserved_bytes,MEM_RESERVE,PAGE_NOACCESS);
    if(!g->reservation)return 0;
    g->committed=VirtualAlloc(g->reservation+g->page_bytes,g->committed_bytes,MEM_COMMIT,PAGE_READWRITE);
    if(!g->committed)return 0;
    g->data=g->committed+g->prefix_bytes;
    memset(g->committed,CACHE_CANARY,g->committed_bytes);memset(g->data,0,CACHE_BYTES);return 1;
}
static void cache_release(cache_buffer *g)
{
    if(g->reservation)VirtualFree(g->reservation,0,MEM_RELEASE);memset(g,0,sizeof(*g));
}
static int cache_guards(const cache_buffer *g)
{
    MEMORY_BASIC_INFORMATION before,after,data;
    if(!g->reservation||!g->committed||!g->data)return 0;
    if(VirtualQuery(g->reservation,&before,sizeof(before))!=sizeof(before)
        ||VirtualQuery(g->committed+g->committed_bytes,&after,sizeof(after))!=sizeof(after)
        ||VirtualQuery(g->committed,&data,sizeof(data))!=sizeof(data))return 0;
    if(before.State!=MEM_RESERVE||after.State!=MEM_RESERVE||before.AllocationBase!=g->reservation
        ||after.AllocationBase!=g->reservation||data.State!=MEM_COMMIT||data.Protect!=PAGE_READWRITE
        ||data.AllocationBase!=g->reservation||data.RegionSize<g->committed_bytes)return 0;
    for(SIZE_T i=0;i<g->prefix_bytes;++i)if(g->committed[i]!=CACHE_CANARY)return 0;
    for(SIZE_T i=g->prefix_bytes+CACHE_BYTES;i<g->committed_bytes;++i)if(g->committed[i]!=CACHE_CANARY)return 0;
    return 1;
}
static void *__cdecl cache_reset_wrapper(void *,int,size_t);
/* Optional addresses are used only by the own-process helper's private fixture;
   public installation always passes NULL and uses the exact native manifest. */
static void cache_make_plan(cache_plan *plan,uintptr_t base,const uintptr_t *addresses)
{
    for(unsigned i=0;i<CACHE184_SITE_COUNT;++i) {
        const cache_site *s=&cache_sites[i];cache_plan *p=&plan[i];
        p->va=addresses?addresses[i]:s->va;p->length=s->length;p->offset=s->offset;
        memcpy(p->old_bytes,s->original,s->length);memcpy(p->new_bytes,s->original,s->length);
        /* Allocate/reset three records, but preserve native traversal of only
           indices0/1. R+3*stride endpoints are FUTURE after shared roster API. */
        uint32_t value=(uint32_t)(base+s->field+(s->kind==CACHE_END?CACHE_TRAVERSAL_COUNT*CACHE_STRIDE:0));
        if(s->kind==CACHE_RESET_CALL_KIND)value=(uint32_t)(uintptr_t)&cache_reset_wrapper-(uint32_t)(p->va+5);
        memcpy(p->new_bytes+p->offset,&value,4);
    }
}
static int cache_check_plan(const cache_plan *plan,int patched,cache184_report *r)
{
    uint8_t bytes[11];r->checked_sites=0;
    for(unsigned i=0;i<CACHE184_SITE_COUNT;++i) {
        const cache_plan *p=&plan[i];const uint8_t *want=patched?p->new_bytes:p->old_bytes;
        if(!cache_read(p->va,bytes,p->length)||memcmp(bytes,want,p->length)){r->failed_site=i;return 0;}
        ++r->checked_sites;
    }
    return 1;
}
static int cache_collect_pages(const cache_plan *plan,cache_page *pages,unsigned *count,DWORD type,void *allocation)
{
    SYSTEM_INFO si;GetSystemInfo(&si);*count=0;
    if(!si.dwPageSize||(si.dwPageSize&(si.dwPageSize-1)))return 0;
    for(unsigned i=0;i<CACHE184_SITE_COUNT;++i) {
        uintptr_t first=plan[i].va&~((uintptr_t)si.dwPageSize-1);
        uintptr_t last=(plan[i].va+plan[i].length-1)&~((uintptr_t)si.dwPageSize-1);
        for(uintptr_t page=first;page<=last;page+=si.dwPageSize) {
            unsigned j;for(j=0;j<*count;++j)if(pages[j].address==(void *)page)break;
            if(j<*count)continue;
            MEMORY_BASIC_INFORMATION mi;
            if(*count>=CACHE_MAX_PAGES||VirtualQuery((void *)page,&mi,sizeof(mi))!=sizeof(mi)
                ||mi.State!=MEM_COMMIT||mi.Type!=type||mi.AllocationBase!=allocation||(mi.Protect&PAGE_GUARD)
                ||(mi.Protect!=PAGE_EXECUTE_READ&&mi.Protect!=PAGE_EXECUTE_READWRITE&&mi.Protect!=PAGE_EXECUTE_WRITECOPY))return 0;
            pages[*count].address=(void *)page;pages[*count].original=mi.Protect;pages[*count].writable=0;++*count;
        }
    }
    return 1;
}
static int cache_pages_restore(cache_page *pages,unsigned count,const cache_ops *ops)
{
    SYSTEM_INFO si;GetSystemInfo(&si);int ok=1;
    for(unsigned i=0;i<count;++i) {
        DWORD ignored;MEMORY_BASIC_INFORMATION mi;
        if(pages[i].writable) {
            if(!ops->protect(pages[i].address,si.dwPageSize,pages[i].original,&ignored))ok=0;
            else pages[i].writable=0;
        }
        if(VirtualQuery(pages[i].address,&mi,sizeof(mi))!=sizeof(mi)||mi.Protect!=pages[i].original)ok=0;
    }
    return ok;
}
static int cache_pages_writeable(cache_page *pages,unsigned count,const cache_ops *ops)
{
    SYSTEM_INFO si;GetSystemInfo(&si);
    for(unsigned i=0;i<count;++i) {
        DWORD old;
        if(!ops->protect(pages[i].address,si.dwPageSize,PAGE_EXECUTE_READWRITE,&old))return 0;
        pages[i].writable=1;
        if(old!=pages[i].original&&old!=PAGE_EXECUTE_READWRITE)return 0;
    }
    return 1;
}
static void cache_write_operands(const cache_plan *plan,int patched)
{
    for(unsigned i=0;i<CACHE184_SITE_COUNT;++i) {
        const cache_plan *p=&plan[i];const uint8_t *bytes=patched?p->new_bytes:p->old_bytes;
        memcpy((void *)(p->va+p->offset),bytes+p->offset,4);
    }
}
/* Exact same transaction core is exercised on private helper-owned pages.
   Production supplies only MEM_IMAGE/0x400000 and real Kernel32 operations. */
static int cache_transaction(const cache_plan *plan,int install_now,DWORD type,void *allocation,const cache_ops *ops,cache184_report *r)
{
    cache_page pages[CACHE_MAX_PAGES];unsigned count;int before=!install_now;
    if(!cache_check_plan(plan,before,r))return CACHE184_BYTES_DIFFER;
    if(!cache_guards(&cache_allocation))return CACHE184_GUARD_CORRUPTION;
    if(!cache_collect_pages(plan,pages,&count,type,allocation))return CACHE184_TRANSACTION_FAILED;
    if(!cache_pages_writeable(pages,count,ops)) {
        r->win32_error=GetLastError();r->protections_restored=cache_pages_restore(pages,count,ops);
        return r->protections_restored?CACHE184_TRANSACTION_FAILED:CACHE184_FATAL_RESTORE_FAILED;
    }
    if(!cache_check_plan(plan,before,r)||!cache_guards(&cache_allocation)) {
        r->protections_restored=cache_pages_restore(pages,count,ops);
        return r->protections_restored?CACHE184_TRANSACTION_FAILED:CACHE184_FATAL_RESTORE_FAILED;
    }
    cache_write_operands(plan,install_now);
    int flushed=ops->flush(GetCurrentProcess(),NULL,0);
    if(!flushed)r->win32_error=GetLastError();
    int matched=cache_check_plan(plan,install_now,r);
    r->protections_restored=cache_pages_restore(pages,count,ops);
    if(flushed&&matched&&r->protections_restored&&cache_guards(&cache_allocation))return CACHE184_OK;
    if(!r->win32_error)r->win32_error=GetLastError();
    if(!cache_pages_writeable(pages,count,ops)) {
        r->protections_restored=cache_pages_restore(pages,count,ops);return CACHE184_FATAL_RESTORE_FAILED;
    }
    cache_write_operands(plan,before);
    flushed=ops->flush(GetCurrentProcess(),NULL,0);matched=cache_check_plan(plan,before,r);
    r->protections_restored=cache_pages_restore(pages,count,ops);
    return flushed&&matched&&r->protections_restored?CACHE184_TRANSACTION_FAILED:CACHE184_FATAL_RESTORE_FAILED;
}
static void cache_report_init(cache184_report *r)
{
    memset(r,0,sizeof(*r));r->version=CACHE184_API_VERSION;r->bytes=sizeof(*r);r->failed_site=UINT32_MAX;
}
static void cache_snapshot(cache184_report *r)
{
    r->installed=cache_installed!=0;r->fatal=cache_fatal!=0;
    r->cache_base=(uint32_t)(uintptr_t)cache_allocation.data;r->cache_bytes=cache_allocation.data?CACHE_BYTES:0;
    r->allocation_count=cache_allocation.data?CACHE_ALLOCATION_COUNT:0;
    r->traversal_count=cache_allocation.data?CACHE_TRAVERSAL_COUNT:0;
    r->guards_ok=cache_guards(&cache_allocation);r->runtime_observed=cache_runtime_observed!=0;
    r->reset_calls=cache_last.reset_calls;r->reset_phase=cache_last.reset_phase;r->reset_markers=cache_last.reset_markers;
    r->reset_failure=cache_last.reset_failure;r->reset_tid=cache_last.reset_tid;r->reset_caller=cache_last.reset_caller;
    r->reset_destination=cache_last.reset_destination;r->reset_value=cache_last.reset_value;r->reset_bytes=cache_last.reset_bytes;
}
static int cache_reset_refuse(unsigned why,int *ok)
{
    cache_last.reset_failure=why;cache_last.reset_phase=CACHE184_RESET_REFUSED;cache_fatal=1;*ok=0;return 0;
}
/* Parameterized only to let own helper test the real checks/copy/reset logic on
   private buffers. Native wrapper below supplies fixed original values/target. */
static void *cache_reset_checked(void *dst,int value,size_t bytes,uintptr_t caller,void *expected_dst,uintptr_t expected_caller,cache_memset_fn original,int *ok)
{
    cache_last.reset_phase=CACHE184_RESET_ENTERED;cache_last.reset_markers=0;cache_last.reset_failure=0;
    cache_last.reset_tid=GetCurrentThreadId();cache_last.reset_caller=(uint32_t)caller;
    cache_last.reset_destination=(uint32_t)(uintptr_t)dst;cache_last.reset_value=(uint32_t)value;cache_last.reset_bytes=(uint32_t)bytes;*ok=0;
    if(!cache_trusted_host||!cache_installed||cache_fatal||GetCurrentProcessId()!=cache_owner_pid){cache_reset_refuse(CACHE184_RESET_FAILURE_HOST,ok);return NULL;}
    cache_last.reset_markers|=1;
    if(GetCurrentThreadId()!=cache_owner_tid){cache_reset_refuse(CACHE184_RESET_FAILURE_THREAD,ok);return NULL;}
    cache_last.reset_markers|=2;
    if(caller!=expected_caller){cache_reset_refuse(CACHE184_RESET_FAILURE_CALLER,ok);return NULL;}
    cache_last.reset_markers|=4;
    if(dst!=expected_dst||value!=0||bytes!=CACHE_PREFIX_BYTES){cache_reset_refuse(CACHE184_RESET_FAILURE_ARGS,ok);return NULL;}
    cache_last.reset_markers|=8;
    if(!cache_guards(&cache_allocation)){cache_reset_refuse(CACHE184_RESET_FAILURE_GUARD_BEFORE,ok);return NULL;}
    cache_last.reset_markers|=16;
    void *result=original(dst,value,bytes);
    cache_last.reset_phase=CACHE184_RESET_ORIGINAL_DONE;
    if(result!=dst){cache_reset_refuse(CACHE184_RESET_FAILURE_RETURN,ok);return NULL;}
    cache_last.reset_markers|=32;
    memset(cache_allocation.data,0,CACHE_BYTES);cache_last.reset_phase=CACHE184_RESET_CACHE_ZEROED;
    if(!cache_guards(&cache_allocation)){cache_reset_refuse(CACHE184_RESET_FAILURE_GUARD_AFTER,ok);return NULL;}
    cache_last.reset_markers|=64;cache_last.reset_phase=CACHE184_RESET_COMPLETE;++cache_last.reset_calls;*ok=1;return result;
}
static __declspec(noinline) void *__cdecl cache_reset_wrapper(void *dst,int value,size_t bytes)
{
    int ok;void *result=cache_reset_checked(dst,value,bytes,(uintptr_t)__builtin_return_address(0),
        (void *)CACHE_PREFIX,CACHE_RESET_RETURN,(cache_memset_fn)CACHE_MEMSET,&ok);
    if(!ok&&cache_trusted_host&&GetCurrentProcessId()==cache_owner_pid&&(uintptr_t)GetModuleHandleW(NULL)==0x00400000u) {
        /* Outside DllMain. Persist the marker before stopping; never call the
           engine or attempt to clear an unverified destination after refusal. */
        wchar_t path[MAX_PATH];DWORD n=GetModuleFileNameW(cache_owner_module,path,MAX_PATH);
        if(n&&n<MAX_PATH-32) {
            while(n&&path[n-1]!=L'\\')--n;path[n]=0;lstrcatW(path,L"cache184-fatal.log");
            char line[384];loader_text text={line,sizeof(line),0};line[0]=0;
            loader_string(&text,"phase=reset-refused failure=");loader_unsigned(&text,cache_last.reset_failure);
            loader_string(&text," markers=0x");loader_hex(&text,cache_last.reset_markers,2);
            loader_string(&text," tid=");loader_unsigned(&text,cache_last.reset_tid);
            loader_string(&text," caller=0x");loader_hex(&text,cache_last.reset_caller,8);
            loader_string(&text," destination=0x");loader_hex(&text,cache_last.reset_destination,8);
            loader_string(&text," value=0x");loader_hex(&text,cache_last.reset_value,8);
            loader_string(&text," bytes=0x");loader_hex(&text,cache_last.reset_bytes,8);
            loader_string(&text," exit=0xe1840040\r\n");
            HANDLE file=CreateFileW(path,FILE_APPEND_DATA|FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
            if(file!=INVALID_HANDLE_VALUE) {
                BY_HANDLE_FILE_INFORMATION info;DWORD written;
                if(GetFileInformationByHandle(file,&info)&&!(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)))
                    WriteFile(file,line,(DWORD)text.used,&written,NULL);
                CloseHandle(file);
            }
        }
        ExitProcess(CACHE_RESET_EXIT); /* exact guarded own host only */
    }
    return result;
}
static int cache_options_ok(const cache184_options *o)
{
    return o&&o->version==CACHE184_API_VERSION&&o->bytes==sizeof(*o)&&o->module&&o->main_thread_id;
}
static int cache_unchanged_guards(void)
{
    static const uint8_t ctor[9]={0x6a,0x7c,0x6a,0x00,0x68,0x48,0x44,0x93,0x00};
    static const uint8_t crt[10]={0x8b,0x54,0x24,0x0c,0x8b,0x4c,0x24,0x04,0x85,0xd2};
    uint8_t actual[10];
    return cache_read(0x48e660u,actual,sizeof(ctor))&&!memcmp(actual,ctor,sizeof(ctor))
        &&cache_read(CACHE_MEMSET,actual,sizeof(crt))&&!memcmp(actual,crt,sizeof(crt));
}
int __cdecl cache184_install_preengine(const cache184_options *o,cache184_report *r)
{
    if(!r)return CACHE184_BAD_ARGUMENT;cache_report_init(r);
    if(!cache_options_ok(o))return r->result=CACHE184_BAD_ARGUMENT;
    if(GetCurrentThreadId()!=o->main_thread_id)return r->result=CACHE184_WRONG_THREAD;
    if(o->preengine_epoch!=CACHE184_PREENGINE_EPOCH)return r->result=CACHE184_NOT_PREENGINE;
    if(!cache_enable(o->module))return r->result=CACHE184_DISABLED;
    if(!o->enter_quiescence||!o->leave_quiescence)return r->result=CACHE184_NO_QUIESCENCE;
    if(cache_attempted)return r->result=CACHE184_ALREADY_ATTEMPTED;
    bootstrap_report identity;
    if((uintptr_t)GetModuleHandleW(NULL)!=0x00400000u||bootstrap_guard_current(&identity,sizeof(identity))!=1
        ||identity.flags!=MW2TRIO_GUARD_ALL)return r->result=CACHE184_WRONG_BUILD;
    if(!cache_initial_phase())return r->result=CACHE184_NOT_PREENGINE;
    cache_make_plan(cache_native_plan,0,NULL);
    if(!cache_unchanged_guards()||!cache_check_plan(cache_native_plan,0,r))return r->result=CACHE184_BYTES_DIFFER;
    if(InterlockedCompareExchange(&cache_attempted,1,0))return r->result=CACHE184_ALREADY_ATTEMPTED;
    if(!cache_allocate(&cache_allocation)){r->win32_error=GetLastError();cache_release(&cache_allocation);return r->result=CACHE184_ALLOCATION_FAILED;}
    cache_make_plan(cache_native_plan,(uintptr_t)cache_allocation.data,NULL);
    if(!o->enter_quiescence(o->quiescence_context)){cache_release(&cache_allocation);return r->result=CACHE184_NO_QUIESCENCE;}
    int result=CACHE184_NOT_PREENGINE;
    if(cache_initial_phase()&&cache_unchanged_guards()&&cache_guards(&cache_allocation)
        &&cache_read(CACHE_OLD_BASE,cache_allocation.data,2u*CACHE_STRIDE)) {
        HMODULE pinned=NULL;
        if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCWSTR)(uintptr_t)&cache184_install_preengine,&pinned)&&pinned==o->module) {
            cache_owner_tid=o->main_thread_id;cache_owner_pid=GetCurrentProcessId();cache_owner_module=pinned;cache_trusted_host=1;
            result=cache_transaction(cache_native_plan,1,MEM_IMAGE,(void *)0x00400000u,&cache_native_ops,r);
        }else{r->win32_error=GetLastError();result=CACHE184_BAD_ARGUMENT;}
    }
    if(result==CACHE184_OK)cache_installed=1;
    if(result==CACHE184_FATAL_RESTORE_FAILED)cache_fatal=1;
    o->leave_quiescence(o->quiescence_context);
    if(result!=CACHE184_OK&&!cache_fatal){cache_release(&cache_allocation);cache_trusted_host=0;}
    cache_last.protections_restored=r->protections_restored;cache_snapshot(r);return r->result=result;
}
int __cdecl cache184_observe(cache184_report *r)
{
    if(!r)return CACHE184_BAD_ARGUMENT;cache_report_init(r);
    if(!cache_installed&&!cache_fatal)return r->result=CACHE184_DISABLED;
    if(GetCurrentThreadId()!=cache_owner_tid)return r->result=CACHE184_WRONG_THREAD;
    cache_runtime_observed=1;cache_snapshot(r);r->protections_restored=cache_last.protections_restored;
    if(cache_last.reset_failure)return r->result=CACHE184_FATAL_RESET_CONTRACT;
    if(cache_fatal)return r->result=CACHE184_FATAL_RESTORE_FAILED;
    if(!r->guards_ok){cache_fatal=1;r->fatal=1;return r->result=CACHE184_GUARD_CORRUPTION;}
    return r->result=CACHE184_OK;
}
int __cdecl cache184_rollback_preengine(const cache184_options *o,cache184_report *r)
{
    if(!r)return CACHE184_BAD_ARGUMENT;cache_report_init(r);
    if(!cache_options_ok(o))return r->result=CACHE184_BAD_ARGUMENT;
    if(GetCurrentThreadId()!=o->main_thread_id||GetCurrentThreadId()!=cache_owner_tid)return r->result=CACHE184_WRONG_THREAD;
    if(cache_fatal){cache_snapshot(r);return r->result=CACHE184_FATAL_RESTORE_FAILED;}
    if(!cache_installed)return r->result=CACHE184_DISABLED;
    if(o->preengine_epoch!=1||cache_runtime_observed||cache_last.reset_calls||!cache_initial_phase())return r->result=CACHE184_NOT_PREENGINE;
    if(!o->enter_quiescence||!o->leave_quiescence||!o->enter_quiescence(o->quiescence_context))return r->result=CACHE184_NO_QUIESCENCE;
    int result=cache_initial_phase()&&cache_unchanged_guards()
        ?cache_transaction(cache_native_plan,0,MEM_IMAGE,(void *)0x00400000u,&cache_native_ops,r):CACHE184_NOT_PREENGINE;
    if(result==CACHE184_OK)cache_installed=0;
    if(result==CACHE184_FATAL_RESTORE_FAILED)cache_fatal=1;
    o->leave_quiescence(o->quiescence_context);
    if(result==CACHE184_OK){cache_release(&cache_allocation);cache_trusted_host=0;}
    cache_last.protections_restored=r->protections_restored;cache_snapshot(r);return r->result=result;
}
