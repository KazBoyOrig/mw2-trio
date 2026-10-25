/* Review-only x86 candidate. Own MIT implementation; concepts and own localized
 * MW2COOP_STUNNED literal adapted from stxlvn/mw2_coop, pinned 8fcb6bc (MIT).
 * Preserve the upstream MIT notice if integrated. No game assets embedded.
 * No account, service, admission, networking, player-capacity or progress patch.
 */
#include "runtime184.h"
#include "bodyassets184.h"
#include "bootstrap.h"
#include "loader_safe.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <psapi.h>

_Static_assert(sizeof(void *)==4,"exact build184 x86 only");
_Static_assert(sizeof(runtime184_options)==32,"options v1 x86 ABI");
#define IMAGE_BASE ((uintptr_t)0x400000u)
#define FILTER_CALL ((uintptr_t)0x4e4c67u)
#define FIND_DVAR ((uintptr_t)0x4bca20u)
#define FORMAT_DVAR ((uintptr_t)0x4e7470u)
#define FORMAT_READABLE_CALL ((uintptr_t)0x4e4c82u)
#define FORMAT_FALLBACK_CALL ((uintptr_t)0x4e4c95u)
#define SCR_STRING ((uintptr_t)0x4d5f20u)
#define SET_ORIGINAL ((uintptr_t)0x5c9e10u)
#define SET_UNINIT_ORIGINAL ((uintptr_t)0x5c9eb0u)
#define LOCALIZE_CALL ((uintptr_t)0x6192d7u)
#define FIND_ASSET ((uintptr_t)0x4f6f10u)
#define ERROR_ENTRY ((uintptr_t)0x612f50u)
#define ERROR_MESSAGE ((uintptr_t)0x18e3f90u)
#define COOP_READ_MASK 0x20c4u

typedef struct rt_dvar { uint32_t name,description,flags;uint8_t type,pad[3],current[16]; } rt_dvar;
typedef struct rt_localize { const char *value,*name; } rt_localize;
_Static_assert(sizeof(rt_dvar)==0x20&&offsetof(rt_dvar,flags)==8&&offsetof(rt_dvar,current)==0x10,"dvar prefix only");
_Static_assert(sizeof(rt_localize)==8,"localize entry x86");
typedef void *(__cdecl *rt_find_dvar)(const char *);
typedef const char *(__cdecl *rt_format_dvar)(const char *,const char *);
typedef int (__cdecl *rt_body_ready)(void);
typedef const char *(__cdecl *rt_scr_string)(unsigned);
typedef void (__cdecl *rt_builtin)(void);
typedef void *(__cdecl *rt_find_asset)(int,const char *);

static volatile LONG rt_attempted,rt_installed,rt_fatal;
static volatile LONG rt_changes,rt_sets,rt_localized,rt_errors,rt_drops;
static DWORD rt_owner,last_tick;
static wchar_t rt_log[MAX_PATH];
static SRWLOCK rt_lock=SRWLOCK_INIT;
static runtime184_report rt_last;
static char previous_state[256],previous_qa[256],previous_command[256];
/* Referenced by the naked wrapper's assembly; never freed after native install. */
static __attribute__((used,unused)) void *rt_error_original;
static void *rt_error_page;

static int rt_read(uintptr_t at,void *out,SIZE_T bytes)
{
    MEMORY_BASIC_INFORMATION m;
    if(!bytes||VirtualQuery((void *)at,&m,sizeof(m))!=sizeof(m))return 0;
    DWORD p=m.Protect&0xffu;
    if(m.State!=MEM_COMMIT||(m.Protect&PAGE_GUARD)||(p!=PAGE_READONLY&&p!=PAGE_READWRITE
        &&p!=PAGE_WRITECOPY&&p!=PAGE_EXECUTE_READ&&p!=PAGE_EXECUTE_READWRITE&&p!=PAGE_EXECUTE_WRITECOPY))return 0;
    uintptr_t begin=(uintptr_t)m.BaseAddress;
    if(at<begin||at-begin>=m.RegionSize||bytes>m.RegionSize-(at-begin))return 0;
    memcpy(out,(const void *)at,bytes);return 1;
}
static int rt_text(uintptr_t at,char *out,size_t cap)
{
    size_t n=0;if(!cap)return 0;
    for(;n+1<cap;++n){unsigned char c;if(!rt_read(at+n,&c,1)){out[0]=0;return 0;}
        if(!c){out[n]=0;return 1;}out[n]=(c>=32&&c!=127)?(char)c:'.';}
    out[n]=0;return 0;
}
static int rt_equal_name(const char *a,const char *b)
{
    for(size_t n=0;n<128;++n){unsigned char x=(unsigned char)a[n],y=(unsigned char)b[n];
        if(x>='A'&&x<='Z')x=(unsigned char)(x+32);if(y>='A'&&y<='Z')y=(unsigned char)(y+32);
        if(x!=y)return 0;if(!x)return 1;}return 0;
}
static void rt_emit(const char *kind,const char *text,uint32_t caller,uint32_t pointer)
{
    if(!rt_log[0])return;
    char line[900];loader_text t={line,sizeof(line),0};line[0]=0;
    loader_string(&t,"tick=");loader_unsigned(&t,GetTickCount());loader_string(&t," tid=");loader_unsigned(&t,GetCurrentThreadId());
    loader_string(&t," ");loader_string(&t,kind);loader_string(&t," caller=0x");loader_hex(&t,caller,8);
    loader_string(&t," cached_message_pointer=0x");loader_hex(&t,pointer,8);loader_string(&t," text=");loader_string(&t,text);loader_string(&t,"\r\n");
    HANDLE f=CreateFileW(rt_log,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f!=INVALID_HANDLE_VALUE){DWORD wrote;WriteFile(f,line,(DWORD)t.used,&wrote,NULL);CloseHandle(f);}
}
static int rt_flag_pointer(void *pointer,const char *requested)
{
    if(GetCurrentThreadId()!=rt_owner||!pointer)return 0;
    rt_dvar d;char name[128];uintptr_t at=(uintptr_t)pointer;
    if((at&3u)||!rt_read(at,&d,sizeof(d))||d.type>8||!rt_text(d.name,name,sizeof(name))||!rt_equal_name(name,requested))return 0;
    if(d.flags&COOP_READ_MASK)return 1;
    MEMORY_BASIC_INFORMATION m;
    if(VirtualQuery((void *)(at+8),&m,sizeof(m))!=sizeof(m)||m.State!=MEM_COMMIT||(m.Protect&PAGE_GUARD)
        ||((m.Protect&0xffu)!=PAGE_READWRITE&&(m.Protect&0xffu)!=PAGE_EXECUTE_READWRITE))return 0;
    uintptr_t begin=(uintptr_t)m.BaseAddress;
    if(at+8<begin||at+8-begin>m.RegionSize-4)return 0;
    LONG before=InterlockedOr((volatile LONG *)(at+8),0x80);
    if(!(before&0x80))InterlockedIncrement(&rt_changes);
    return 1;
}
static const char *const rt_external[]={"start","mw2coop_god","mw2coop_qa","mw2coop_qa_phase"};
static const char rt_body_key[]="mw2trio_body_assets_ready";
static int rt_is_body_key(const char *name)
{char key[128];return rt_text((uintptr_t)name,key,sizeof(key))&&!strcmp(key,rt_body_key);}
/* The returned pointer authorizes visibility only. The two downstream value
 * CALLs independently intercept the exact owned name; no synthetic Dvar exists. */
static void *rt_filter_lookup(const char *name,rt_find_dvar find)
{
    DWORD saved=GetLastError();
    if(rt_is_body_key(name)){
        void *d=NULL;
        if(rt_installed&&!rt_fatal&&GetCurrentThreadId()==rt_owner){
            rt_dvar coop;char actual[16];void *candidate=find("coop");
            if(candidate&&!((uintptr_t)candidate&3u)&&rt_read((uintptr_t)candidate,&coop,sizeof(coop))
                &&coop.type==0&&coop.current[0]==1&&(coop.flags&COOP_READ_MASK)
                &&rt_text(coop.name,actual,sizeof(actual))&&!strcmp(actual,"coop"))d=candidate;
        }
        SetLastError(saved);return d;
    }
    SetLastError(saved);void *d=find(name);DWORD error=GetLastError();
    if(rt_installed&&GetCurrentThreadId()==rt_owner){char key[128];if(rt_text((uintptr_t)name,key,sizeof(key)))
        for(unsigned i=0;i<4;++i)if(rt_equal_name(key,rt_external[i])){rt_flag_pointer(d,key);break;}}
    SetLastError(error);return d;
}
static void *__cdecl __attribute__((force_align_arg_pointer,noinline)) rt_filter_find(const char *name)
{return rt_filter_lookup(name,(rt_find_dvar)FIND_DVAR);}
static int __cdecl rt_body_live_ready(void)
{
    if(!runtime184_body_channel_ready())return 0;
    return bodyassets184_ready_now()==1;
}
/* Callback arguments make the exact dispatcher independently testable using
 * own stubs; production always passes the pinned native formatter and proof. */
static const char *rt_value_dispatch(const char *name,const char *fallback,
    rt_format_dvar format,rt_body_ready ready,uintptr_t caller)
{
    DWORD saved=GetLastError();
    if(rt_is_body_key(name)){
        int allowed=caller==FORMAT_READABLE_CALL+5u||caller==FORMAT_FALLBACK_CALL+5u;
        int yes=allowed&&ready&&ready()==1;
        SetLastError(saved);return yes?"1":"0";
    }
    SetLastError(saved);return format(name,fallback);
}
static const char *__cdecl __attribute__((force_align_arg_pointer,noinline)) rt_script_value(const char *name,const char *fallback)
{
    uintptr_t caller=(uintptr_t)__builtin_return_address(0);
    return rt_value_dispatch(name,fallback,(rt_format_dvar)FORMAT_DVAR,rt_body_live_ready,caller);
}
static void rt_after_script_set(void)
{
    if(!rt_installed||GetCurrentThreadId()!=rt_owner)return;
    /* Original validated arg0 and returned normally. VM arguments are still
       owned by its caller, which has not returned from this table callback. */
    const char *name=((rt_scr_string)SCR_STRING)(0);char key[128];
    if(rt_text((uintptr_t)name,key,sizeof(key))){void *d=((rt_find_dvar)FIND_DVAR)(key);rt_flag_pointer(d,key);}
    InterlockedIncrement(&rt_sets);
}
static void __cdecl __attribute__((force_align_arg_pointer,noinline)) rt_set(void)
{((rt_builtin)SET_ORIGINAL)();DWORD e=GetLastError();rt_after_script_set();SetLastError(e);}
static void __cdecl __attribute__((force_align_arg_pointer,noinline)) rt_set_uninit(void)
{((rt_builtin)SET_UNINIT_ORIGINAL)();DWORD e=GetLastError();rt_after_script_set();SetLastError(e);}

/* Own mod literal from the pinned MIT framework; Russian bytes are CP1251. */
static const char rt_key[]="MW2COOP_STUNNED";
static const rt_localize rt_en={"You are stunned. Wait for the other player",rt_key};
static const rt_localize rt_ru={
    "\xc2\xfb \xee\xe3\xeb\xf3\xf8\xe5\xed\xfb. \xce\xe6\xe8\xe4\xe0\xe9\xf2\xe5 \xe2\xf2\xee\xf0\xee\xe3\xee "
    "\xe8\xe3\xf0\xee\xea\xe0",rt_key};
static void *__cdecl __attribute__((force_align_arg_pointer,noinline)) rt_localized_find(int type,const char *name)
{
    char key[32];
    if(type==27&&name&&rt_text((uintptr_t)name,key,sizeof(key))&&strcmp(key,rt_key)==0){
        rt_localize entry;unsigned char yes[2]={0,0};
        void *native=((rt_find_asset)FIND_ASSET)(27,"MENU_YES");
        int russian=rt_read((uintptr_t)native,&entry,sizeof(entry))
            &&rt_read((uintptr_t)entry.value,yes,sizeof(yes))&&yes[0]==0xc4&&yes[1]==0xe0;
        InterlockedIncrement(&rt_localized);return (void *)(russian?&rt_ru:&rt_en);
    }
    return ((rt_find_asset)FIND_ASSET)(type,name);
}

/* Complete logging before original control flow; no lock is held over a native
   longjmp. Bounded first-eight/every-hundredth disk messages; counters cover all. */
static void __cdecl __attribute__((used,noinline,force_align_arg_pointer)) rt_error_note(uint32_t caller)
{
    DWORD saved=GetLastError();LONG count=InterlockedIncrement(&rt_errors);
    if(!TryAcquireSRWLockExclusive(&rt_lock)){InterlockedIncrement(&rt_drops);SetLastError(saved);return;}
    uint32_t pointer=0;char message[256]={0};
    rt_read(ERROR_MESSAGE,&pointer,4);
    if(!rt_text(pointer,message,sizeof(message))&&!message[0])memcpy(message,"<unreadable or absent>",23);
    rt_last.last_error_caller=caller;rt_last.last_error_message_pointer=pointer;
    memcpy(rt_last.cached_error_message,message,sizeof(message));
    if(count<=8||count%100==0)rt_emit("SCRIPT_ERROR_CORE_ENTRY_CACHED_MESSAGE",message,caller,pointer);
    ReleaseSRWLockExclusive(&rt_lock);SetLastError(saved);
}
static void __attribute__((naked,noinline)) rt_error_entry(void)
{
    __asm__ __volatile__(
        "pushfl\n\tpushal\n\tmovl %esp,%ebp\n\tandl $-16,%esp\n\tsubl $512,%esp\n\tfxsave (%esp)\n\tcld\n\t"
        "pushl 36(%ebp)\n\tcall _rt_error_note\n\taddl $4,%esp\n\tfxrstor (%esp)\n\tmovl %ebp,%esp\n\tpopal\n\tpopfl\n\tjmp *_rt_error_original\n\t");
}

typedef struct rt_sig{uintptr_t va;uint8_t n,b[64];}rt_sig;
static const rt_sig rt_signatures[]={
 {0x4bca20,12,{0x57,0x8b,0x7c,0x24,8,0xe8,0x56,0x6a,0x17,0,0x5f,0xc3}},
 {0x4e4c60,19,{0x56,0x8b,0x74,0x24,8,0x57,0x56,0xe8,0xb4,0x7d,0xfd,0xff,0x83,0xc4,4,0x85,0xc0,0x74,0x1b}},
 {0x4e4c73,7,{0xf7,0x40,8,0xc4,0x20,0,0}},
 {0x4d5f20,10,{0x8b,0x44,0x24,4,0x50,0xe8,0xc6,0xff,0xf8,0xff}},
 {0x731504,12,{0x80,0x6a,0x6a,0,0x10,0x9e,0x5c,0,0,0,0,0}},
 {0x731510,12,{0x78,0xb4,0x6d,0,0xb0,0x9e,0x5c,0,0,0,0,0}},
 {0x5c9e10,14,{0x81,0xec,0,8,0,0,0x56,0x6a,0,0xe8,2,0xc1,0xf0,0xff}},
 {0x5c9eb0,9,{0x56,0xe8,0x5a,0x17,0xe8,0xff,0x83,0xf8,2}},
 {0x6192d0,19,{0x8b,0x44,0x24,4,0x50,0x6a,0x1b,0xe8,0x34,0xdc,0xed,0xff,0x83,0xc4,8,0x85,0xc0,0x74,3}},
 {0x4f6f10,11,{0x51,0x53,0x8b,0x1d,0x6c,0x11,0x69,0,0x55,0x56,0x57}},
 {0x612f50,16,{0x80,0x3d,0x8f,0x3f,0x8e,1,0,0x75,0x30,0x80,0x3d,0x34,0x8a,0x58,1,0}},
 {0x457ae0,13,{0x83,0x3d,0x90,0x3f,0x8e,1,0,0x75,0x21,0x8b,0x44,0x24,4}}
 ,{0x4e4c7c,18,{0x8b,0x44,0x24,0x10,0x50,0x56,0xe8,0xe9,0x27,0,0,0x83,0xc4,8,0x8b,0xf8,0xeb,0x3e}}
 ,{0x4e4c8e,12,{0x8b,0x7c,0x24,0x10,0x53,0x57,0x56,0xe8,0xd6,0x27,0,0}}
 ,{0x4e7470,59,{0x57,0x8b,0x7c,0x24,8,0xe8,6,0xc0,0x14,0,0x85,0xc0,0x75,6,0x8b,0x44,0x24,0x0c,0x5f,0xc3,
    0x8b,0x50,0x10,0x83,0xec,0x10,0x8b,0xcc,0x89,0x11,0x8b,0x50,0x14,0x89,0x51,4,0x8b,0x50,0x18,0x89,
    0x51,8,0x8b,0x50,0x1c,0x50,0x89,0x51,0x0c,0xe8,0x3a,0xe6,0xfd,0xff,0x83,0xc4,0x14,0x5f,0xc3}}
};
typedef struct rt_patch{uintptr_t va;SIZE_T n;uint8_t before[7],after[7];}rt_patch;
static rt_patch rt_plan[RUNTIME184_PATCH_COUNT];
static int rt_image(uintptr_t va,const void *bytes,SIZE_T n)
{
    MEMORY_BASIC_INFORMATION m;uint8_t observed[64];
    return n<=sizeof(observed)&&VirtualQuery((void *)va,&m,sizeof(m))==sizeof(m)
        &&m.Type==MEM_IMAGE&&(uintptr_t)m.AllocationBase==IMAGE_BASE
        &&rt_read(va,observed,n)&&memcmp(observed,bytes,n)==0;
}
static int rt_check_signatures(runtime184_report *r)
{
    for(unsigned i=0;i<sizeof(rt_signatures)/sizeof(rt_signatures[0]);++i){const rt_sig *s=&rt_signatures[i];
        if(!rt_image(s->va,s->b,s->n)){r->failed_site=i+1;return 0;}++r->checked_sites;}return 1;
}
static void rt_branch(uint8_t *out,uint8_t opcode,uintptr_t at,uintptr_t to)
{uint32_t delta=(uint32_t)(to-at-5u);out[0]=opcode;memcpy(out+1,&delta,4);}
static void rt_build_plan(void)
{
    rt_plan[0]=(rt_patch){FILTER_CALL,5,{0xe8,0xb4,0x7d,0xfd,0xff},{0}};
    rt_plan[1]=(rt_patch){0x731508,4,{0x10,0x9e,0x5c,0},{0}};
    rt_plan[2]=(rt_patch){0x731514,4,{0xb0,0x9e,0x5c,0},{0}};
    rt_plan[3]=(rt_patch){LOCALIZE_CALL,5,{0xe8,0x34,0xdc,0xed,0xff},{0}};
    rt_plan[4]=(rt_patch){ERROR_ENTRY,7,{0x80,0x3d,0x8f,0x3f,0x8e,1,0},{0}};
    rt_plan[5]=(rt_patch){FORMAT_READABLE_CALL,5,{0xe8,0xe9,0x27,0,0},{0}};
    rt_plan[6]=(rt_patch){FORMAT_FALLBACK_CALL,5,{0xe8,0xd6,0x27,0,0},{0}};
    rt_branch(rt_plan[0].after,0xe8,FILTER_CALL,(uintptr_t)rt_filter_find);
    uint32_t to=(uint32_t)(uintptr_t)rt_set;memcpy(rt_plan[1].after,&to,4);
    to=(uint32_t)(uintptr_t)rt_set_uninit;memcpy(rt_plan[2].after,&to,4);
    rt_branch(rt_plan[3].after,0xe8,LOCALIZE_CALL,(uintptr_t)rt_localized_find);
    rt_branch(rt_plan[4].after,0xe9,ERROR_ENTRY,(uintptr_t)rt_error_entry);rt_plan[4].after[5]=0x90;rt_plan[4].after[6]=0x90;
    rt_branch(rt_plan[5].after,0xe8,FORMAT_READABLE_CALL,(uintptr_t)rt_script_value);
    rt_branch(rt_plan[6].after,0xe8,FORMAT_FALLBACK_CALL,(uintptr_t)rt_script_value);
}
int __cdecl runtime184_body_channel_ready(void)
{
    if(!rt_installed||rt_fatal||GetCurrentThreadId()!=rt_owner)return 0;
    for(unsigned i=0;i<RUNTIME184_PATCH_COUNT;++i)if(!rt_image(rt_plan[i].va,rt_plan[i].after,rt_plan[i].n)){
        InterlockedExchange(&rt_fatal,1);return 0;}
    const rt_sig *format=&rt_signatures[sizeof(rt_signatures)/sizeof(rt_signatures[0])-1u];
    if(format->va!=FORMAT_DVAR||!rt_image(format->va,format->b,format->n)){
        InterlockedExchange(&rt_fatal,1);return 0;}
    return 1;
}
typedef struct rt_page{void *va;DWORD old;int writable;}rt_page;
static int rt_private_table_copy(const rt_page *p,const MEMORY_BASIC_INFORMATION *m)
{
    /* Exact data-table page only. Windows canonicalizes an image WRITECOPY
       page to private READWRITE after modification, including after a successful
       VirtualProtect(..., WRITECOPY). Code pages keep exact protection checks.
       See Microsoft VirtualQuery copy-on-write documentation and the own-image
       regression fixture; no permission is added by this recognition. */
    if(p->writable||p->va!=(void *)0x731000u||p->old!=PAGE_WRITECOPY
        ||m->Protect!=PAGE_READWRITE||m->State!=MEM_COMMIT||m->Type!=MEM_IMAGE
        ||m->AllocationBase!=(void *)IMAGE_BASE)return 0;
    PSAPI_WORKING_SET_EX_INFORMATION working;
    memset(&working,0,sizeof(working));working.VirtualAddress=p->va;
    if(!K32QueryWorkingSetEx(GetCurrentProcess(),&working,sizeof(working)))return 0;
    return working.VirtualAttributes.Valid&&!working.VirtualAttributes.Shared
        &&working.VirtualAttributes.Win32Protection==PAGE_READWRITE;
}
static int rt_restore(rt_page *p,unsigned count,SIZE_T page)
{
    int ok=1;for(unsigned i=0;i<count;++i){DWORD ignored;MEMORY_BASIC_INFORMATION m={0};
        if(p[i].writable){if(VirtualProtect(p[i].va,page,p[i].old,&ignored))p[i].writable=0;else ok=0;}
        if(VirtualQuery(p[i].va,&m,sizeof(m))!=sizeof(m)
            ||(m.Protect!=p[i].old&&!rt_private_table_copy(&p[i],&m))){
            char detail[128];loader_text d={detail,sizeof(detail),0};detail[0]=0;
            loader_string(&d,"expected=0x");loader_hex(&d,p[i].old,8);
            loader_string(&d," observed=0x");loader_hex(&d,m.Protect,8);
            rt_emit("PROTECTION_RESTORE_MISMATCH",detail,(uint32_t)(uintptr_t)p[i].va,0);ok=0;}}return ok;
}
static int rt_transaction(runtime184_report *r)
{
    SYSTEM_INFO si;GetSystemInfo(&si);SIZE_T page=si.dwPageSize;rt_page pages[10];unsigned count=0;
    if(!page||(page&(page-1u)))return RUNTIME184_TRANSACTION_FAILED;
    for(unsigned i=0;i<RUNTIME184_PATCH_COUNT;++i){rt_patch *s=&rt_plan[i];
        if(!rt_image(s->va,s->before,s->n)){r->failed_site=i+1;return RUNTIME184_BYTES_DIFFER;}
        uintptr_t first=s->va&~(uintptr_t)(page-1u),last=(s->va+s->n-1u)&~(uintptr_t)(page-1u);
        for(uintptr_t at=first;;at+=page){unsigned j=0;for(;j<count&&pages[j].va!=(void *)at;++j){}
            if(j==count){MEMORY_BASIC_INFORMATION m;if(count==10||VirtualQuery((void *)at,&m,sizeof(m))!=sizeof(m)
                ||m.State!=MEM_COMMIT||m.Type!=MEM_IMAGE||(uintptr_t)m.AllocationBase!=IMAGE_BASE)return RUNTIME184_TRANSACTION_FAILED;
                pages[count++]=(rt_page){(void *)at,m.Protect,0};}if(at==last)break;}}
    for(unsigned i=0;i<count;++i){DWORD old;if(!VirtualProtect(pages[i].va,page,PAGE_EXECUTE_READWRITE,&old)){
            r->win32_error=GetLastError();r->protections_restored=(uint32_t)rt_restore(pages,count,page);
            return r->protections_restored?RUNTIME184_TRANSACTION_FAILED:RUNTIME184_FATAL_RESTORE_FAILED;}
        pages[i].writable=1;if(old!=pages[i].old){
            char detail[128];loader_text d={detail,sizeof(detail),0};detail[0]=0;
            loader_string(&d,"expected=0x");loader_hex(&d,pages[i].old,8);
            loader_string(&d," replaced=0x");loader_hex(&d,old,8);
            rt_emit("PROTECTION_OPEN_MISMATCH",detail,(uint32_t)(uintptr_t)pages[i].va,0);
            r->protections_restored=(uint32_t)rt_restore(pages,count,page);return RUNTIME184_FATAL_RESTORE_FAILED;}}
    /* No write occurred until every full guard and page protection succeeded. */
    int good=1;for(unsigned i=0;i<RUNTIME184_PATCH_COUNT;++i)memcpy((void *)rt_plan[i].va,rt_plan[i].after,rt_plan[i].n);
    for(unsigned i=0;i<RUNTIME184_PATCH_COUNT;++i)if(!FlushInstructionCache(GetCurrentProcess(),(void *)rt_plan[i].va,rt_plan[i].n)
        ||!rt_image(rt_plan[i].va,rt_plan[i].after,rt_plan[i].n))good=0;
    r->protections_restored=(uint32_t)rt_restore(pages,count,page);
    if(good&&r->protections_restored)return RUNTIME184_OK;
    r->win32_error=GetLastError();
    /* Reopen ALL pages before any rollback store. A failed reopen never writes RX. */
    for(unsigned i=0;i<count;++i){DWORD old;if(!VirtualProtect(pages[i].va,page,PAGE_EXECUTE_READWRITE,&old)){
            rt_restore(pages,count,page);return RUNTIME184_FATAL_RESTORE_FAILED;}pages[i].writable=1;}
    for(unsigned i=0;i<RUNTIME184_PATCH_COUNT;++i)memcpy((void *)rt_plan[i].va,rt_plan[i].before,rt_plan[i].n);
    good=1;for(unsigned i=0;i<RUNTIME184_PATCH_COUNT;++i)if(!FlushInstructionCache(GetCurrentProcess(),(void *)rt_plan[i].va,rt_plan[i].n)
        ||!rt_image(rt_plan[i].va,rt_plan[i].before,rt_plan[i].n))good=0;
    r->protections_restored=(uint32_t)rt_restore(pages,count,page);
    return good&&r->protections_restored?RUNTIME184_TRANSACTION_FAILED:RUNTIME184_FATAL_RESTORE_FAILED;
}
static int rt_initial(void)
{uint32_t base,count;return rt_read(0x10a3c10,&base,4)&&rt_read(0x10a3c3c,&count,4)&&base==0&&count==0;}
static int rt_paths(HMODULE module)
{
    wchar_t directory[MAX_PATH],path[MAX_PATH];DWORD n=GetModuleFileNameW(module,directory,MAX_PATH);
    if(!n||n>=MAX_PATH)return 0;while(n&&directory[n-1]!=L'\\')--n;directory[n]=0;
    if(n+40>=MAX_PATH)return 0;
    const wchar_t *names[]={L"mw2-trio-two-player-test.enable",L"mw2-trio-control.enable"};
    for(unsigned i=0;i<2;++i){lstrcpyW(path,directory);lstrcatW(path,names[i]);DWORD a=GetFileAttributesW(path);
        if(a==INVALID_FILE_ATTRIBUTES||(a&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))return 0;}
    lstrcpyW(rt_log,directory);lstrcatW(rt_log,L"runtime184.log");return 1;
}
static void rt_report(runtime184_report *r){memset(r,0,sizeof(*r));r->version=RUNTIME184_API_VERSION;r->bytes=sizeof(*r);}

int __cdecl runtime184_install_preengine(const runtime184_options *o,runtime184_report *r)
{
    if(!r)return RUNTIME184_BAD_ARGUMENT;rt_report(r);
    MEMORY_BASIC_INFORMATION own;
    if(!o||o->version!=RUNTIME184_API_VERSION||o->bytes!=sizeof(*o)||!o->module||!o->main_thread_id
        ||VirtualQuery((void *)(uintptr_t)&runtime184_install_preengine,&own,sizeof(own))!=sizeof(own)||own.AllocationBase!=o->module)return r->result=RUNTIME184_BAD_ARGUMENT;
    if(GetCurrentThreadId()!=o->main_thread_id)return r->result=RUNTIME184_WRONG_THREAD;
    if(o->diagnostic_tick!=1)return r->result=RUNTIME184_NOT_PREENGINE;
    if(!rt_paths(o->module))return r->result=RUNTIME184_DISABLED;
    if(!o->enter_quiescence||!o->leave_quiescence)return r->result=RUNTIME184_NO_QUIESCENCE;
    if(InterlockedCompareExchange(&rt_attempted,1,0))return r->result=RUNTIME184_ALREADY_ATTEMPTED;
    bootstrap_report identity;
    if((uintptr_t)GetModuleHandleW(NULL)!=IMAGE_BASE||bootstrap_guard_current(&identity,sizeof(identity))!=1
        ||identity.flags!=MW2TRIO_GUARD_ALL)return r->result=RUNTIME184_WRONG_BUILD;
    if(!rt_initial())return r->result=RUNTIME184_NOT_PREENGINE;
    if(!rt_check_signatures(r))return r->result=RUNTIME184_BYTES_DIFFER;
    rt_build_plan();rt_error_page=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!rt_error_page)return r->result=RUNTIME184_ALLOCATION_FAILED;
    memcpy(rt_error_page,rt_plan[4].before,7);rt_branch((uint8_t *)rt_error_page+7,0xe9,(uintptr_t)rt_error_page+7,ERROR_ENTRY+7);
    DWORD old;if(!VirtualProtect(rt_error_page,4096,PAGE_EXECUTE_READ,&old)||!FlushInstructionCache(GetCurrentProcess(),rt_error_page,12)){
        r->win32_error=GetLastError();VirtualFree(rt_error_page,0,MEM_RELEASE);rt_error_page=NULL;return r->result=RUNTIME184_ALLOCATION_FAILED;}
    rt_error_original=rt_error_page;
    if(!o->enter_quiescence(o->quiescence_context)){VirtualFree(rt_error_page,0,MEM_RELEASE);rt_error_page=NULL;rt_error_original=NULL;return r->result=RUNTIME184_NO_QUIESCENCE;}
    int result;
    if(!rt_initial()||!rt_check_signatures(r))result=RUNTIME184_BYTES_DIFFER;
    else{HMODULE pinned=NULL;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCWSTR)(uintptr_t)&runtime184_install_preengine,&pinned)||pinned!=o->module)result=RUNTIME184_ALLOCATION_FAILED;
        else{rt_owner=o->main_thread_id;result=rt_transaction(r);}}
    if(result==RUNTIME184_OK){InterlockedExchange(&rt_installed,1);r->installed=1;rt_last=*r;}
    if(result==RUNTIME184_FATAL_RESTORE_FAILED)InterlockedExchange(&rt_fatal,1);
    o->leave_quiescence(o->quiescence_context);
    if(result!=RUNTIME184_OK&&!rt_fatal){VirtualFree(rt_error_page,0,MEM_RELEASE);rt_error_page=NULL;rt_error_original=NULL;}
    char detail[192];loader_text diagnostic={detail,sizeof(detail),0};detail[0]=0;
    loader_string(&diagnostic,"result=");loader_signed(&diagnostic,result);
    loader_string(&diagnostic," checked_sites=");loader_unsigned(&diagnostic,r->checked_sites);
    loader_string(&diagnostic," failed_site=");loader_unsigned(&diagnostic,r->failed_site);
    loader_string(&diagnostic," protections_restored=");loader_unsigned(&diagnostic,r->protections_restored);
    loader_string(&diagnostic," win32_error=");loader_unsigned(&diagnostic,r->win32_error);
    rt_emit(result==RUNTIME184_OK?"INSTALL_READY_NO_NETWORK":"INSTALL_REFUSED_OR_FAILED",detail,0,0);
    return r->result=result;
}
static int rt_raw(const char *name,char *out,size_t cap)
{
    rt_dvar d;void *p=((rt_find_dvar)FIND_DVAR)(name);if(!p||!rt_read((uintptr_t)p,&d,sizeof(d))){out[0]=0;return 0;}
    if(d.type!=7){out[0]=0;return 0;}uint32_t value;memcpy(&value,d.current,4);return rt_text(value,out,cap);
}
int __cdecl runtime184_tick(runtime184_report *r)
{
    if(!r)return RUNTIME184_BAD_ARGUMENT;rt_report(r);
    if(rt_fatal)return r->result=RUNTIME184_FATAL_RESTORE_FAILED;
    if(!rt_installed)return r->result=RUNTIME184_DISABLED;
    if(GetCurrentThreadId()!=rt_owner)return r->result=RUNTIME184_WRONG_THREAD;
    r->installed=1;
    for(unsigned i=0;i<RUNTIME184_PATCH_COUNT;++i)if(!rt_image(rt_plan[i].va,rt_plan[i].after,rt_plan[i].n)){
        r->failed_site=i+1;InterlockedExchange(&rt_fatal,1);return r->result=RUNTIME184_BYTES_DIFFER;}
    const rt_sig *format=&rt_signatures[sizeof(rt_signatures)/sizeof(rt_signatures[0])-1u];
    if(format->va!=FORMAT_DVAR||!rt_image(format->va,format->b,format->n)){
        r->failed_site=RUNTIME184_PATCH_COUNT+1u;InterlockedExchange(&rt_fatal,1);return r->result=RUNTIME184_BYTES_DIFFER;}
    for(unsigned i=0;i<4;++i){void *p=((rt_find_dvar)FIND_DVAR)(rt_external[i]);rt_dvar d;
        if(p&&rt_read((uintptr_t)p,&d,sizeof(d))){r->external_found_mask|=1u<<i;rt_flag_pointer(p,rt_external[i]);
            if(rt_read((uintptr_t)p,&d,sizeof(d))&&(d.flags&COOP_READ_MASK))r->external_readable_mask|=1u<<i;}}
    r->flags_changed=(uint32_t)rt_changes;r->script_sets=(uint32_t)rt_sets;r->localized_hits=(uint32_t)rt_localized;
    r->script_error_entries=(uint32_t)rt_errors;r->diagnostic_drops=(uint32_t)rt_drops;
    AcquireSRWLockShared(&rt_lock);r->checked_sites=rt_last.checked_sites;r->protections_restored=rt_last.protections_restored;
    r->last_error_caller=rt_last.last_error_caller;r->last_error_message_pointer=rt_last.last_error_message_pointer;
    memcpy(r->cached_error_message,rt_last.cached_error_message,sizeof(r->cached_error_message));ReleaseSRWLockShared(&rt_lock);
    rt_raw("mw2coop_state",r->state,sizeof(r->state));rt_raw("mw2coop_qa_log",r->qa_log,sizeof(r->qa_log));rt_raw("mw2coop_cmd",r->command,sizeof(r->command));
    DWORD now=GetTickCount();if((DWORD)(now-last_tick)>=250){last_tick=now;
        if(strcmp(previous_state,r->state)){memcpy(previous_state,r->state,sizeof(previous_state));rt_emit("SCRIPT_STATE",r->state,0,0);}
        if(strcmp(previous_qa,r->qa_log)){memcpy(previous_qa,r->qa_log,sizeof(previous_qa));rt_emit("SCRIPT_QA",r->qa_log,0,0);}
        if(strcmp(previous_command,r->command)){memcpy(previous_command,r->command,sizeof(previous_command));rt_emit("SCRIPT_REQUEST_NOT_EXECUTED",r->command,0,0);}}
    return r->result=RUNTIME184_OK;
}
