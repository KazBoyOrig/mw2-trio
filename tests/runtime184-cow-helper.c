/* Own MIT regression. No game files, code execution or native engine calls.
 * Exact metadata is synthetic; resident/private/protection checks use a real
 * owned fixed-address page. Shared/API-error cases are explicitly synthetic. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { WS_REAL, WS_ERROR, WS_INVALID, WS_SHARED, WS_EXECUTABLE };
static unsigned ws_mode,ws_calls;
static int image_metadata;
static BOOL WINAPI cow_workingset(HANDLE process,PVOID buffer,DWORD bytes)
{
    ++ws_calls;
    if(ws_mode==WS_REAL)return K32QueryWorkingSetEx(process,buffer,bytes);
    if(ws_mode==WS_ERROR){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    if(bytes!=sizeof(PSAPI_WORKING_SET_EX_INFORMATION))return FALSE;
    PSAPI_WORKING_SET_EX_INFORMATION *w=buffer;
    w->VirtualAttributes.Flags=0;
    w->VirtualAttributes.Valid=ws_mode!=WS_INVALID;
    w->VirtualAttributes.Shared=ws_mode==WS_SHARED;
    w->VirtualAttributes.Win32Protection=ws_mode==WS_EXECUTABLE?PAGE_EXECUTE_READWRITE:PAGE_READWRITE;
    return TRUE;
}
static SIZE_T WINAPI cow_query(LPCVOID address,PMEMORY_BASIC_INFORMATION out,SIZE_T bytes)
{
    SIZE_T got=VirtualQuery(address,out,bytes);
    if(image_metadata&&address==(void *)0x731000u&&got==sizeof(*out)){
        out->Type=MEM_IMAGE;out->AllocationBase=(void *)0x400000u;
    }
    return got;
}
#define K32QueryWorkingSetEx cow_workingset
#define VirtualQuery cow_query
#include "../src/runtime184.c"
#undef VirtualQuery
#undef K32QueryWorkingSetEx

int __cdecl bootstrap_guard_current(bootstrap_report *out,uint32_t bytes)
{if(out&&bytes==sizeof(*out))memset(out,0,bytes);return 0;}
static unsigned checks,failures;
static void check(int ok,const char *name)
{++checks;failures+=!ok;printf("%s %s\n",ok?"PASS":"FAIL",name);}
static void metadata_refusal(rt_page p,MEMORY_BASIC_INFORMATION m,const char *name)
{
    unsigned before=ws_calls;
    check(!rt_private_table_copy(&p,&m)&&ws_calls==before,name);
}
int main(void)
{
    SYSTEM_INFO system;GetSystemInfo(&system);
    if(system.dwPageSize!=4096){puts("ERROR unsupported test page size");return 2;}
    /* VirtualAlloc never replaces an existing region. Refuse if unavailable. */
    void *allocation=VirtualAlloc((void *)0x730000u,8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(allocation!=(void *)0x730000u){
        if(allocation)VirtualFree(allocation,0,MEM_RELEASE);
        puts("ERROR own fixed fixture address unavailable; no existing memory changed");return 3;
    }
    volatile unsigned char *owned=(void *)0x731000u;*owned=0xA5;
    MEMORY_BASIC_INFORMATION actual;
    check(VirtualQuery((void *)owned,&actual,sizeof(actual))==sizeof(actual)
        &&actual.Type==MEM_PRIVATE&&actual.Protect==PAGE_READWRITE,"fixture is an owned private RW page, not a native game image");
    PSAPI_WORKING_SET_EX_INFORMATION resident;memset(&resident,0,sizeof(resident));resident.VirtualAddress=(void *)owned;
    check(K32QueryWorkingSetEx(GetCurrentProcess(),&resident,sizeof(resident))&&resident.VirtualAttributes.Valid
        &&!resident.VirtualAttributes.Shared&&resident.VirtualAttributes.Win32Protection==PAGE_READWRITE,"real working-set proof is resident/private/RW");
    rt_page p={(void *)owned,PAGE_WRITECOPY,0};
    MEMORY_BASIC_INFORMATION m=actual;m.Type=MEM_IMAGE;m.AllocationBase=(void *)0x400000u;
    check(rt_private_table_copy(&p,&m),"exact table metadata plus real private RW proof accepts canonical COW");
    rt_page changed=p;changed.writable=1;metadata_refusal(changed,m,"unfinished restore refuses before working-set query");
    changed=p;changed.va=(void *)0x731004u;metadata_refusal(changed,m,"unaligned pointer is not the exact table page");
    changed=p;changed.va=(void *)0x732000u;metadata_refusal(changed,m,"other data page refuses");
    changed=p;changed.va=(void *)0x612000u;metadata_refusal(changed,m,"native code page refuses");
    changed=p;changed.old=PAGE_READWRITE;metadata_refusal(changed,m,"old RW is not the COW exception");
    changed=p;changed.old=PAGE_EXECUTE_READ;metadata_refusal(changed,m,"old executable protection refuses");
    changed=p;changed.old=PAGE_EXECUTE_WRITECOPY;metadata_refusal(changed,m,"old executable COW protection refuses");
    MEMORY_BASIC_INFORMATION altered=m;altered.Protect=PAGE_EXECUTE_READWRITE;metadata_refusal(p,altered,"observed executable RW refuses");
    altered=m;altered.Protect=PAGE_READONLY;metadata_refusal(p,altered,"observed read-only refuses");
    altered=m;altered.Protect=PAGE_READWRITE|PAGE_GUARD;metadata_refusal(p,altered,"guard modifier refuses");
    altered=m;altered.State=MEM_RESERVE;metadata_refusal(p,altered,"uncommitted metadata refuses");
    altered=m;altered.Type=MEM_PRIVATE;metadata_refusal(p,altered,"non-image metadata refuses");
    altered=m;altered.AllocationBase=(void *)0x410000u;metadata_refusal(p,altered,"wrong image base refuses");
    altered=m;altered.AllocationBase=NULL;metadata_refusal(p,altered,"absent image base refuses");
    for(unsigned mode=WS_ERROR;mode<=WS_EXECUTABLE;++mode){
        ws_mode=mode;unsigned before=ws_calls;
        const char *names[]={"","synthetic working-set API error refuses","synthetic nonresident proof refuses",
            "synthetic shared proof refuses","synthetic executable proof refuses"};
        check(!rt_private_table_copy(&p,&m)&&ws_calls==before+1,names[mode]);
    }
    ws_mode=WS_REAL;
    DWORD old;
    check(VirtualProtect((void *)owned,4096,PAGE_EXECUTE_READWRITE,&old),"own data page changed to executable only for negative protection test");
    check(!rt_private_table_copy(&p,&m),"real executable working-set proof refuses despite synthetic RW metadata");
    check(VirtualProtect((void *)owned,4096,PAGE_READONLY,&old),"own page changed to read-only");
    check(!rt_private_table_copy(&p,&m),"real read-only working-set proof refuses despite synthetic RW metadata");
    check(VirtualProtect((void *)owned,4096,PAGE_READWRITE,&old),"own RW protection restored");
    *owned=0x5A;
    check(rt_private_table_copy(&p,&m),"real private proof accepts again after RW restore");
    check(VirtualFree((void *)owned,4096,MEM_DECOMMIT),"own proof page decommitted");
    check(!rt_private_table_copy(&p,&m),"real nonresident/uncommitted working-set proof refuses synthetic committed metadata");
    check(VirtualAlloc((void *)owned,4096,MEM_COMMIT,PAGE_READWRITE)==(void *)owned,"own proof page recommitted");
    *owned=0x5A;
    image_metadata=1;
    check(rt_restore(&p,1,4096),"restore recognizes canonical private RW with exact table metadata");
    rt_page failed=p;failed.writable=1;
    check(!rt_restore(&failed,1,4096)&&failed.writable,"real impossible private-page WRITECOPY restore failure remains fatal");
    rt_page exact=p;exact.old=PAGE_READWRITE;ws_mode=WS_ERROR;unsigned before=ws_calls;
    check(rt_restore(&exact,1,4096)&&ws_calls==before,"exact protection restore does not need or bypass working-set proof");
    ws_mode=WS_REAL;changed=p;changed.old=PAGE_EXECUTE_READ;
    check(!rt_restore(&changed,1,4096),"restore cannot reinterpret an executable page as table COW");
    image_metadata=0;
    check(VirtualFree(allocation,0,MEM_RELEASE),"own fixed region released");
    printf("RESULT checks=%u failures=%u; exact metadata synthetic, real own-page working set, no game/native engine execution\n",checks,failures);
    return failures?1:0;
}
