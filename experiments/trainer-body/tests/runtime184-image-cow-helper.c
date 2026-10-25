/* Own MIT test. Private allocated bytes only; no game address is executed. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <psapi.h>
/* Own bytes pre-exist in the file-backed .data page; never a native pointer. */
static __attribute__((used)) unsigned char fixture_initial_table[16]={
    0x10,0x9e,0x5c,0,0,0,0,0,0,0,0,0,0xb0,0x9e,0x5c,0
};
static unsigned char *fixture_base;
static SIZE_T fixture_bytes;
static int fixture_is_image;
static void *fixture_view;
static SIZE_T WINAPI fixture_query(LPCVOID at,PMEMORY_BASIC_INFORMATION out,SIZE_T bytes)
{
    SIZE_T got=VirtualQuery(at,out,bytes);
    uintptr_t p=(uintptr_t)at,b=(uintptr_t)fixture_base;
    if(got==sizeof(*out)&&((p>=b&&p-b<fixture_bytes)
        ||(fixture_is_image&&out->AllocationBase==fixture_view))){
        if(!fixture_is_image)out->Type=MEM_IMAGE;
        out->AllocationBase=(void *)(uintptr_t)0x400000u;
    }
    return got;
}
#define VirtualQuery fixture_query
#include "../src/runtime184.c"
#undef VirtualQuery
int __cdecl bodyassets184_ready_now(void){return 0;}
int __cdecl bootstrap_guard_current(bootstrap_report *out,uint32_t bytes)
{if(out&&bytes==sizeof(*out))memset(out,0,bytes);return 0;}
static unsigned checks,failures;
static void check(int ok,const char *text)
{++checks;failures+=!ok;printf("%s %s\n",ok?"PASS":"FAIL",text);}
int main(int argc,char **argv)
{
    SYSTEM_INFO si;GetSystemInfo(&si);SIZE_T page=si.dwPageSize;
    if(page!=4096)return 2;
    fixture_is_image=argc==2&&!strcmp(argv[1],"--own-image");
    if(argc>1&&!fixture_is_image)return 2;
    void *view=NULL;HANDLE file=INVALID_HANDLE_VALUE,mapping=NULL;
    DWORD protection[4]={PAGE_EXECUTE_READ,PAGE_READONLY,PAGE_EXECUTE_READ,PAGE_EXECUTE_READ};
    fixture_bytes=page*4;
    if(fixture_is_image){
        wchar_t path[MAX_PATH];DWORD len=GetModuleFileNameW(NULL,path,MAX_PATH);
        if(!len||len>=MAX_PATH)return 3;
        file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file==INVALID_HANDLE_VALUE)return 3;
        mapping=CreateFileMappingW(file,NULL,PAGE_READONLY|SEC_IMAGE,0,0,NULL);
        if(!mapping)return 3;
        view=MapViewOfFile(mapping,FILE_MAP_READ,0,0,0);if(!view)return 3;fixture_view=view;
        IMAGE_DOS_HEADER *dos=view;IMAGE_NT_HEADERS32 *nt=(void *)((char *)view+dos->e_lfanew);
        IMAGE_SECTION_HEADER *section=IMAGE_FIRST_SECTION(nt);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE||nt->Signature!=IMAGE_NT_SIGNATURE
            ||strncmp((char *)section->Name,".text",5)||section->VirtualAddress!=0x1000
            ||section->Misc.VirtualSize<0x3001)return 3;
        fixture_base=(unsigned char *)view+section->VirtualAddress;
        for(unsigned i=0;i<4;++i){MEMORY_BASIC_INFORMATION m;
            if(VirtualQuery(fixture_base+i*page,&m,sizeof(m))!=sizeof(m)||m.Type!=MEM_IMAGE)return 3;
            protection[i]=m.Protect;
            printf("image original page%u protection=0x%lx type=0x%lx\n",i,(unsigned long)m.Protect,(unsigned long)m.Type);
            DWORD old;if(!VirtualProtect(fixture_base+i*page,page,PAGE_EXECUTE_READWRITE,&old))return 4;
        }
    }else fixture_base=VirtualAlloc(NULL,fixture_bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!fixture_base)return 3;
    rt_build_plan();
    const unsigned offsets[RUNTIME184_PATCH_COUNT]={0xc67,0x1508,0x1514,0x22d7,0x3f50,0xc82,0xc95};
    for(unsigned i=0;i<RUNTIME184_PATCH_COUNT;++i){
        if(fixture_is_image&&(i==1||i==2)){
            uintptr_t rva=(uintptr_t)fixture_initial_table-(uintptr_t)GetModuleHandleW(NULL);
            rt_plan[i].va=(uintptr_t)view+rva+(i==2?12:0);
        }else{rt_plan[i].va=(uintptr_t)fixture_base+offsets[i];memcpy((void *)rt_plan[i].va,rt_plan[i].before,rt_plan[i].n);}
    }
    for(unsigned i=0;i<4;++i){DWORD old;if(!VirtualProtect(fixture_base+i*page,page,protection[i],&old))return 4;}
    runtime184_report report;rt_report(&report);
    if(fixture_is_image){MEMORY_BASIC_INFORMATION m;VirtualQuery((void *)rt_plan[1].va,&m,sizeof(m));
        printf("untouched image table before protection=0x%lx type=0x%lx\n",(unsigned long)m.Protect,(unsigned long)m.Type);
        check(m.Protect==PAGE_WRITECOPY,"own untouched image data begins WRITECOPY");
    }
    SetLastError(ERROR_ALREADY_EXISTS);
    int result=rt_transaction(&report);DWORD stale=GetLastError();
    printf("transaction result=%d failed_site=%u restored=%u win32_error=%u last_error=%lu\n",result,report.failed_site,report.protections_restored,report.win32_error,(unsigned long)stale);
    if(fixture_is_image){MEMORY_BASIC_INFORMATION m;VirtualQuery((void *)rt_plan[1].va,&m,sizeof(m));
        PSAPI_WORKING_SET_EX_INFORMATION ws;memset(&ws,0,sizeof(ws));ws.VirtualAddress=(void *)rt_plan[1].va;
        BOOL got=QueryWorkingSetEx(GetCurrentProcess(),&ws,sizeof(ws));
        printf("table after protection=0x%lx type=0x%lx ws_ok=%d valid=%u shared=%u ws_protection=0x%lx\n",(unsigned long)m.Protect,(unsigned long)m.Type,got,(unsigned)ws.VirtualAttributes.Valid,(unsigned)ws.VirtualAttributes.Shared,(unsigned long)ws.VirtualAttributes.Win32Protection);
        check(result==RUNTIME184_FATAL_RESTORE_FAILED&&report.protections_restored==0,"unrelated image COW page remains outside exact table exception");
        check(m.Protect==PAGE_READWRITE&&m.Type==MEM_IMAGE&&got&&ws.VirtualAttributes.Valid&&!ws.VirtualAttributes.Shared,"canonicalized COW is actual private resident RW image data");
        check(VirtualProtect((void *)rt_plan[1].va,4,PAGE_WRITECOPY,&stale),"explicit WRITECOPY restore succeeds");
        VirtualQuery((void *)rt_plan[1].va,&m,sizeof(m));check(m.Protect==PAGE_READWRITE,"successful explicit WRITECOPY restore still reports RW");
        check(UnmapViewOfFile(view)&&CloseHandle(mapping)&&CloseHandle(file),"own image mapping released");
        printf("RESULT checks=%u failures=%u; exact image COW reproduction, no native game code executed\n",checks,failures);
        return failures?1:0;
    }
    check(result==RUNTIME184_OK,"real four-page RX/RO transaction succeeds");
    check(report.protections_restored==1,"success reports protections restored");
    for(unsigned i=0;i<RUNTIME184_PATCH_COUNT;++i)check(!memcmp((void *)rt_plan[i].va,rt_plan[i].after,rt_plan[i].n),"complete replacement retained");
    for(unsigned i=0;i<4;++i){MEMORY_BASIC_INFORMATION m;VirtualQuery(fixture_base+i*page,&m,sizeof(m));check(m.Protect==protection[i],"actual page protection exactly restored");}
    runtime184_report again;rt_report(&again);result=rt_transaction(&again);
    check(result==RUNTIME184_BYTES_DIFFER&&again.failed_site==1,"already-patched fixture refuses before any store");
    HMODULE pinned=NULL;BOOL pin=GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)(uintptr_t)&runtime184_install_preengine,&pinned);
    check(pin&&pinned==GetModuleHandleW(NULL),"same containing-module pin API succeeds on helper");
    if(fixture_is_image){check(UnmapViewOfFile(view)&&CloseHandle(mapping)&&CloseHandle(file),"own image mapping released");}
    else check(VirtualFree(fixture_base,0,MEM_RELEASE),"own private pages released");
    printf("RESULT checks=%u failures=%u own_image=%d; actual VirtualProtect/Flush, fixture-only query shim; no native game calls\n",checks,failures,fixture_is_image);
    return failures?1:0;
}
