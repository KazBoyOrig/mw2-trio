/* Own MIT exact184 private source prototype. Default OFF; no game is launched
   or installed by this module/build. Only original stock zone-loader argument
   substitution; no persistent category, body assignment, provider or transport.
   Runtime remains UNTESTED. The owned synchronous read channel is required. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "bootstrap.h"
#include "pure_sha256.h"
#include "bodyassets184.h"
#include "bodyassets184_core.h"
#include "bodyassets184_tx.h"
#include "bodyassets184_probe.h"
extern int __cdecl runtime184_body_channel_ready(void);
_Static_assert(sizeof(void *)==4,"exact x86 only");
typedef struct ba_guard {uintptr_t va;uint32_t bytes;const char *sha;} ba_guard;
#include "bodyassets184-guards.inc"
typedef struct ba_file {HANDLE h;BY_HANDLE_FILE_INFORMATION info;wchar_t path[MAX_PATH];} ba_file;
typedef void (__cdecl *loader_fn)(const char *,int);
typedef void *(__cdecl *find_fn)(const char *);
static ba184_state state;
static bodyassets184_report report={.version=1};
static DWORD owner;
static wchar_t directory[MAX_PATH],enable[MAX_PATH],logpath[MAX_PATH],canonical[MAX_PATH];
static char native_directory[256];
static ba_file files[3],parents[20];static unsigned parent_count;
static unsigned char replacement[5];
static const unsigned char stock[5]={0xe8,0x72,0xd7,4,0};
static int opened,initialized;
static uint32_t logged_resident_epoch,logged_registered_epoch;
typedef struct ba184_db_diagnostic {uint32_t read_mask,valid_mask,attempted,wait[2],handle[2];} ba184_db_diagnostic;
typedef struct ba184_native_diagnostic {uint32_t passed,failure,so,coop,init_before,init_after,captured_init;ba184_db_diagnostic before,after;ba184_probe_diagnostic probe;} ba184_native_diagnostic;
static ba184_native_diagnostic proof_detail;
/* Owner-thread synchronous query only. Never a reusable readiness cache. */
static struct {DWORD tid;uint32_t init,epoch;int active,worker_checked;} query_context;
typedef struct ba184_worker_diagnostic {uint32_t failure,attempted,zero,timed,final,elapsed;} ba184_worker_diagnostic;
static ba184_worker_diagnostic worker_detail;
static int seal(const ba_file *,int);
static int read_memory(void *unused,uintptr_t a,void *out,size_t n)
{
    (void)unused;MEMORY_BASIC_INFORMATION m;
    if(!n||a<0x10000u||a>0x7ffeffffu||n-1>0x7ffeffffu-a||VirtualQuery((void *)a,&m,sizeof(m))!=sizeof(m)
        ||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return 0;
    DWORD p=m.Protect&0xffu;if(p!=PAGE_READONLY&&p!=PAGE_READWRITE&&p!=PAGE_WRITECOPY
        &&p!=PAGE_EXECUTE_READ&&p!=PAGE_EXECUTE_READWRITE&&p!=PAGE_EXECUTE_WRITECOPY)return 0;
    uintptr_t b=(uintptr_t)m.BaseAddress;if(a<b||a-b>=m.RegionSize||n>m.RegionSize-(a-b))return 0;
    memcpy(out,(void *)a,n);return 1;
}
static int text_a(uintptr_t a,char *out,size_t cap)
{for(size_t i=0;i<cap;++i){unsigned char c;if(!read_memory(NULL,a+i,&c,1)||c>126||(c&&c<32))return 0;out[i]=(char)c;if(!c)return 1;}return 0;}
static void record(const char *event,int result)
{
    report.result=result;if(!logpath[0]||!opened||!parent_count)return;
    for(unsigned i=0;i<parent_count;++i)if(!seal(&parents[i],1))return;
    char line[768];
    int n=snprintf(line,sizeof(line),"tick=%lu bodyassets184 event=%s result=%d installed=%lu calls=%lu substituted=%lu epoch=%lu ready=%lu fatal=%lu failed_guard=%lu validation=0x%lx stage=%lu p_mask=0x%lx p_fail=%lu probe=%lu/%lu/%lu/%lu db_pre=%lu/%lu/%lu/%08lx/%08lx db_post=%lu/%lu/%lu/%08lx/%08lx so=%08lx coop=%lu init=%08lx/%08lx/%08lx worker=%lu/%lu/%08lx/%08lx/%08lx/%lu\r\n",
        (unsigned long)GetTickCount(),event,result,(unsigned long)state.installed,(unsigned long)state.calls,
        (unsigned long)state.substitutions,(unsigned long)state.epoch,(unsigned long)state.ready_epoch,
        (unsigned long)state.fatal,(unsigned long)report.failed_guard,(unsigned long)report.validation_bits,(unsigned long)state.proof_stage,
        (unsigned long)proof_detail.passed,(unsigned long)proof_detail.failure,(unsigned long)proof_detail.probe.status,
        (unsigned long)proof_detail.probe.step,(unsigned long)proof_detail.probe.pass,(unsigned long)proof_detail.probe.asset,
        (unsigned long)proof_detail.before.read_mask,(unsigned long)proof_detail.before.valid_mask,(unsigned long)proof_detail.before.attempted,
        (unsigned long)proof_detail.before.wait[0],(unsigned long)proof_detail.before.wait[1],
        (unsigned long)proof_detail.after.read_mask,(unsigned long)proof_detail.after.valid_mask,(unsigned long)proof_detail.after.attempted,
        (unsigned long)proof_detail.after.wait[0],(unsigned long)proof_detail.after.wait[1],
        (unsigned long)proof_detail.so,(unsigned long)proof_detail.coop,
        (unsigned long)proof_detail.captured_init,(unsigned long)proof_detail.init_before,(unsigned long)proof_detail.init_after,
        (unsigned long)worker_detail.failure,(unsigned long)worker_detail.attempted,(unsigned long)worker_detail.zero,
        (unsigned long)worker_detail.timed,(unsigned long)worker_detail.final,(unsigned long)worker_detail.elapsed);
    if(n<=0||(size_t)n>=sizeof(line))return;
    HANDLE f=CreateFileW(logpath,FILE_APPEND_DATA|FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(f!=INVALID_HANDLE_VALUE){BY_HANDLE_FILE_INFORMATION info;
        if(GetFileType(f)==FILE_TYPE_DISK&&GetFileInformationByHandle(f,&info)&&info.nNumberOfLinks==1
            &&!(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))){
            DWORD wrote;WriteFile(f,line,(DWORD)n,&wrote,NULL);}
        CloseHandle(f);}
}
static int paths(void)
{
    if(directory[0])return 1;DWORD n=GetModuleFileNameW(NULL,directory,MAX_PATH);if(!n||n>=MAX_PATH)return 0;
    while(n&&directory[n-1]!=L'\\')--n;if(n<3||n+70>=MAX_PATH)return 0;directory[n]=0;
    if(directory[1]!=L':'||directory[2]!=L'\\')return 0;
    for(DWORD i=0;i<n;++i){if(directory[i]<32||directory[i]>126)return 0;native_directory[i]=(char)directory[i];}
    native_directory[n-1]=0;
    lstrcpyW(enable,directory);lstrcatW(enable,L"mw2-trio-body-assets-test.enable");
    lstrcpyW(logpath,directory);lstrcatW(logpath,L"bodyassets184.log");return 1;
}
static int same(const BY_HANDLE_FILE_INFORMATION *a,const BY_HANDLE_FILE_INFORMATION *b,int directory_entry)
{return a->dwVolumeSerialNumber==b->dwVolumeSerialNumber&&a->nFileIndexHigh==b->nFileIndexHigh&&a->nFileIndexLow==b->nFileIndexLow
    &&(directory_entry||(a->nFileSizeHigh==b->nFileSizeHigh&&a->nFileSizeLow==b->nFileSizeLow
    &&a->ftLastWriteTime.dwLowDateTime==b->ftLastWriteTime.dwLowDateTime&&a->ftLastWriteTime.dwHighDateTime==b->ftLastWriteTime.dwHighDateTime
    ))
    &&!(b->dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT);}
static int open_pin(ba_file *f,const wchar_t *path,int is_directory,uint32_t bytes,const char *sha)
{
    lstrcpyW(f->path,path);f->h=CreateFileW(path,sha?GENERIC_READ:FILE_READ_ATTRIBUTES,
        is_directory?FILE_SHARE_READ|FILE_SHARE_WRITE:FILE_SHARE_READ,NULL,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT|(is_directory?FILE_FLAG_BACKUP_SEMANTICS:FILE_FLAG_SEQUENTIAL_SCAN),NULL);
    if(f->h==INVALID_HANDLE_VALUE)return 0;
    if(!GetFileInformationByHandle(f->h,&f->info)||(f->info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)
        ||!!(f->info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=!!is_directory)return 0;
    if(is_directory)return 1;
    if(f->info.nFileSizeHigh||(!sha&&f->info.nFileSizeLow>128)||(sha&&f->info.nFileSizeLow!=bytes))return 0;
    if(sha){unsigned char buffer[16384],digest[32];char hex[65];DWORD got;uint32_t total=0;pure_sha256_context hash;pure_sha256_init(&hash);
        for(;;){if(!ReadFile(f->h,buffer,sizeof(buffer),&got,NULL))return 0;if(!got)break;
            if(got>bytes-total)return 0;total+=got;pure_sha256_update(&hash,buffer,got);}
        pure_sha256_final(&hash,digest);pure_sha256_hex(digest,hex);if(total!=bytes||strcmp(hex,sha))return 0;}
    return 1;
}
static int seal(const ba_file *f,int is_directory)
{
    BY_HANDLE_FILE_INFORMATION a,b;if(f->h==INVALID_HANDLE_VALUE||!GetFileInformationByHandle(f->h,&a)||!same(&f->info,&a,is_directory))return 0;
    HANDLE h=CreateFileW(f->path,FILE_READ_ATTRIBUTES,is_directory?FILE_SHARE_READ|FILE_SHARE_WRITE:FILE_SHARE_READ,NULL,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT|(is_directory?FILE_FLAG_BACKUP_SEMANTICS:0),NULL);
    if(h==INVALID_HANDLE_VALUE)return 0;int ok=GetFileInformationByHandle(h,&b)&&same(&f->info,&b,is_directory)
        &&!!(b.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==!!is_directory;CloseHandle(h);return ok;
}
static void close_pins(void)
{for(unsigned i=0;i<3;++i)if(files[i].h!=INVALID_HANDLE_VALUE){CloseHandle(files[i].h);files[i].h=INVALID_HANDLE_VALUE;}
    for(unsigned i=0;i<parent_count;++i)if(parents[i].h!=INVALID_HANDLE_VALUE){CloseHandle(parents[i].h);parents[i].h=INVALID_HANDLE_VALUE;}parent_count=0;opened=0;}
static int pins(void)
{
    for(unsigned i=0;i<3;++i)files[i].h=INVALID_HANDLE_VALUE;
    wchar_t path[MAX_PATH];lstrcpyW(path,directory);size_t end=wcslen(path)-1;
    for(size_t i=2;i<=end;++i)if(path[i]==L'\\'){
        wchar_t save=path[i+1];path[i+1]=0;if(parent_count>=20)return 0;
        if(!open_pin(&parents[parent_count++],path,1,0,NULL))return 0;path[i+1]=save;}
    if(!parent_count)return 0;
    DWORD n=GetFinalPathNameByHandleW(parents[parent_count-1].h,canonical,MAX_PATH,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!n||n>=MAX_PATH)return 0;
    /* The native loader formats an absolute EXE-root path, not CWD. Tie that
       spelling to the held final directory identity and reject alias fallback. */
    wchar_t expected[MAX_PATH];lstrcpyW(expected,directory);
    size_t length=wcslen(expected);if(length&&expected[length-1]==L'\\')expected[length-1]=0;
    length=wcslen(canonical);if(length&&canonical[length-1]==L'\\')canonical[length-1]=0;
    if(wcsncmp(canonical,L"\\\\?\\",4)||lstrcmpiW(canonical+4,expected))return 0;
    if(!open_pin(&files[0],enable,0,0,NULL))return 0;
    lstrcpyW(path,directory);lstrcatW(path,L"zone\\russian\\");
    /* Pin the two remaining directory components too; no reparse ancestor. */
    for(size_t i=wcslen(directory);i<wcslen(path);++i)if(path[i]==L'\\'){
        wchar_t save=path[i+1];path[i+1]=0;if(parent_count>=20||!open_pin(&parents[parent_count++],path,1,0,NULL))return 0;path[i+1]=save;}
    lstrcpyW(path,directory);lstrcatW(path,L"zone\\russian\\so_killspree_trainer.ff");
    if(!open_pin(&files[1],path,0,7232351,"64b82970259d6a9e981f302636d16ae85475f6a867372f051a9b5771bf5ced23"))return 0;
    lstrcpyW(path,directory);lstrcatW(path,L"zone\\russian\\trainer.ff");
    if(!open_pin(&files[2],path,0,116795855,"6157a5c0d59ac7b6bd5023f3106465225fc7c26775537035f4dfb3334a2cdcc7"))return 0;
    opened=1;return 1;
}
static int path_language(void)
{
    uint32_t ptr;char lang[32],root[256];
    if(!read_memory(NULL,0x19fc2a8u,&ptr,4)||!text_a(ptr,lang,sizeof(lang))||strcmp(lang,"russian")
        ||!text_a(0x19f8970u,root,sizeof(root))||lstrcmpiA(root,native_directory))return 0;
    HMODULE k=GetModuleHandleW(L"kernel32.dll");uint32_t api;
    const uintptr_t slots[]={0x6911d0,0x6911dc,0x6911e0,0x691184,0x691188,0x69118c,0x691190};
    const char *names[]={"CreateFileA","GetModuleFileNameA","GetModuleHandleA","SetEvent","ResetEvent","CreateEventA","WaitForSingleObject"};
    if(!k)return 0;for(unsigned i=0;i<sizeof(slots)/sizeof(slots[0]);++i)
        if(!read_memory(NULL,slots[i],&api,4)||(uintptr_t)GetProcAddress(k,names[i])!=api)return 0;
    return 1;
}
static int guard_protection(const ba_guard *g,DWORD protection)
{
    /* These exact immutable descriptor values live in the writable PE .data
       section. Do not infer their page protection from executable guards. */
    if(g->va==0x7302b8u||g->va==0x730368u||g->va==0x73187cu){
        unsigned bytes=g->va==0x73187cu?12:4;
        return g->bytes==bytes&&(protection==PAGE_READONLY||protection==PAGE_READWRITE||protection==PAGE_WRITECOPY);}
    /* All remaining frozen ranges are original .text code. In addition to
       readable hashing, retain the native execution protection requirement. */
    return g->va>=0x401000u&&g->va<0x690aefu&&g->bytes<=0x690aefu-g->va
        &&protection==PAGE_EXECUTE_READ;
}
static int live(unsigned installed)
{
    report.failed_guard=0;
    for(unsigned i=0;i<sizeof(ba_guards)/sizeof(ba_guards[0]);++i){const ba_guard *g=&ba_guards[i];MEMORY_BASIC_INFORMATION m;
        if(VirtualQuery((void *)g->va,&m,sizeof(m))!=sizeof(m)||m.State!=MEM_COMMIT||m.Type!=MEM_IMAGE||m.AllocationBase!=(void *)0x400000u
            ||!guard_protection(g,m.Protect)
            ||g->va<(uintptr_t)m.BaseAddress||g->bytes>m.RegionSize-(g->va-(uintptr_t)m.BaseAddress))return 0;
        char hash[65];pure_sha256_data((void *)g->va,g->bytes,hash);if(strcmp(hash,g->sha)){report.failed_guard=i+1;return 0;}}
    unsigned char now[5];return read_memory(NULL,BA184_CALL_VA,now,5)&&!memcmp(now,installed?replacement:stock,5);
}
static int dvar(const char *name,unsigned type,uint32_t *value)
{
    uintptr_t p=(uintptr_t)((find_fn)0x4bca20u)(name);unsigned char d[32];char actual[128];uint32_t at;
    if(!p||(p&3u)||!read_memory(NULL,p,d,32)||d[12]!=type)return 0;memcpy(&at,d,4);
    if(!text_a(at,actual,sizeof(actual))||strcmp(actual,name))return 0;memcpy(value,d+16,4);return 1;
}
static int fs_empty(void){uint32_t p;char first;return dvar("fs_game",7,&p)&&read_memory(NULL,p,&first,1)&&!first;}
typedef DWORD (*ba184_wait_fn)(void *,HANDLE,DWORD);
static int db_phase_using(ba184_read_fn read,ba184_wait_fn wait,void *context,unsigned rawinit,ba184_db_diagnostic *out)
{
    ba184_db_diagnostic local={0,0,0,{0xfffffffeu,0xfffffffeu},{0,0}};ba184_db_diagnostic *d=out?out:&local;*d=local;
    if(!read||!wait||rawinit>1)return 0;
    const uintptr_t a[]={0x1551bd8u,0x1551b88u};
    for(unsigned i=0;i<2;++i){uint32_t h;if(!read(context,a[i],&h,4))return 0;d->read_mask|=1u<<i;
        d->handle[i]=h;if(!h||h==0xffffffffu)return 0;d->valid_mask|=1u<<i;d->attempted|=1u<<i;
        d->wait[i]=wait(context,(HANDLE)(uintptr_t)h,0);
        if(d->wait[i]!=WAIT_OBJECT_0&&!(rawinit==1&&i==0&&d->wait[i]==WAIT_TIMEOUT))return 0;}
    if(d->handle[0]==d->handle[1])return 0;
    return 1;
}
static int db_ready_using(ba184_read_fn read,ba184_wait_fn wait,void *context,ba184_db_diagnostic *out)
{return db_phase_using(read,wait,context,0,out);}
static int db_same(const ba184_db_diagnostic *a,const ba184_db_diagnostic *b)
{return a->read_mask==3&&a->valid_mask==3&&a->attempted==3
    &&a->read_mask==b->read_mask&&a->valid_mask==b->valid_mask&&a->attempted==b->attempted
    &&a->wait[0]==b->wait[0]&&a->wait[1]==b->wait[1]&&a->handle[0]==b->handle[0]&&a->handle[1]==b->handle[1];}
static DWORD native_wait(void *u,HANDLE h,DWORD timeout){(void)u;return WaitForSingleObject(h,timeout);}
static int db_ready_detail(ba184_db_diagnostic *d){return db_ready_using(read_memory,native_wait,NULL,d);}
static int db_ready(void){return db_ready_detail(NULL);}
static uint32_t validate(void *unused,unsigned phase,unsigned installed)
{
    (void)unused;uint32_t bits=0;
    if(GetCurrentThreadId()!=owner)return 0;
    if(live(installed))bits|=BA184_IMAGE|BA184_LIVE;
    if(opened&&seal(&files[0],0))bits|=BA184_FLAG;
    if(opened&&seal(&files[1],0)&&seal(&files[2],0))bits|=BA184_FILES;
    int good=opened;for(unsigned i=0;i<parent_count&&good;++i)good=seal(&parents[i],1);
    if(good)bits|=BA184_PATH;if(path_language())bits|=BA184_LANGUAGE;
    if(runtime184_body_channel_ready())bits|=BA184_OWN_CHANNEL|BA184_PRESCRIPT_BARRIER;
    if(phase==BA184_INSTALL){uint32_t running=1,init=1;
        if((bits&BA184_LIVE)&&fs_empty()&&dvar("sv_running",0,&running)&&!running
            &&read_memory(NULL,0x10a3c34u,&init,4)&&!init&&db_ready())bits|=BA184_OWNER_PHASE;}
    else bits|=BA184_OWNER_PHASE;
    report.validation_bits=bits;return bits;
}
static int pin(void *unused){(void)unused;HMODULE m;return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)(uintptr_t)&bodyassets184_tick,&m)!=0;}
static int query_rx(void *unused,unsigned char *site,size_t n){(void)unused;MEMORY_BASIC_INFORMATION m;
    return (uintptr_t)site==BA184_CALL_VA&&n==5&&VirtualQuery(site,&m,sizeof(m))==sizeof(m)&&m.State==MEM_COMMIT
        &&m.Type==MEM_IMAGE&&m.AllocationBase==(void *)0x400000u&&m.Protect==PAGE_EXECUTE_READ;}
static int writable(void *u,unsigned char *site,size_t n)
{
    (void)u;DWORD old,ignored;
    if(!VirtualProtect(site,n,PAGE_EXECUTE_READWRITE,&old))return 0;
    if(old==PAGE_EXECUTE_READ)return 1;
    /* Query and protection change are not atomic. Never report clean refusal
       after changing an unexpected protection. Best effort restore; fatal. */
    VirtualProtect(site,n,old,&ignored);return -1;
}
static int restore(void *u,unsigned char *site,size_t n){(void)u;DWORD old;return VirtualProtect(site,n,PAGE_EXECUTE_READ,&old)!=0;}
static int flush(void *u,unsigned char *site,size_t n){(void)u;return FlushInstructionCache(GetCurrentProcess(),site,n)!=0;}
static int patch(void *u,const unsigned char before[5],const unsigned char after[5])
{const ba184_tx_ops tx={query_rx,writable,restore,flush};return ba184_tx(&tx,u,(unsigned char *)BA184_CALL_VA,before,after);}
static int original(void *u,const char *name,int second){(void)u;((loader_fn)BA184_ORIGINAL_VA)(name,second);return 1;}
static int publish(void *u,uint32_t epoch,uint32_t ready){(void)u;(void)epoch;(void)ready;return 1;} /* internal state only */
typedef struct ba184_native_proof_ops {
    int (*live)(void *),(*path)(void *),(*fs)(void *),(*db)(void *,unsigned,ba184_db_diagnostic *);
    int (*so)(void *,uint32_t *),(*coop)(void *,uint8_t *),(*map)(void *);
    uint32_t (*probe)(void *,unsigned,ba184_probe_diagnostic *);
    int (*init)(void *,uint32_t *);
} ba184_native_proof_ops;
static int native_live(void *u){(void)u;return live(1);}
static int native_path(void *u){(void)u;return path_language();}
static int native_fs(void *u){(void)u;return fs_empty();}
static int native_db(void *u,unsigned rawinit,ba184_db_diagnostic *d){(void)u;return db_phase_using(read_memory,native_wait,NULL,rawinit,d);}
static int native_init(void *u,uint32_t *v){(void)u;return read_memory(NULL,0x10a3c34u,v,4);}
static int native_so(void *u,uint32_t *v){(void)u;return dvar("specialops",0,v);}
static int native_coop(void *u,uint8_t *v){(void)u;return read_memory(NULL,0x88897cu,v,1);}
static int native_map(void *u)
{
    (void)u;uint32_t p;char map[128];if(!dvar("mapname",7,&p))return BA184_DETAIL_MAP_DVAR;
    if(!text_a(p,map,sizeof(map)))return BA184_DETAIL_MAP_TEXT;
    return strcmp(map,"trainer")?BA184_DETAIL_MAP_NAME:0;
}
static uint32_t native_probe(void *u,unsigned stage,ba184_probe_diagnostic *d)
{(void)u;return ba184_probe_diagnose(read_memory,NULL,stage,d);}
typedef struct ba184_worker_wait_ops {
    ba184_read_fn read;ba184_wait_fn wait;DWORD (*clock)(void *);
    int (*scope)(void *,uint32_t,unsigned);
} ba184_worker_wait_ops;
/* Controlled bounded diagnostic only. No render/DB native call, no event write,
   no pending acceptance. The subsequent normal proof starts from fresh reads. */
static int worker_wait_using(const ba184_worker_wait_ops *o,void *u,uint32_t epoch,unsigned stage,unsigned captured,ba184_worker_diagnostic *d)
{
    memset(d,0,sizeof(*d));d->zero=d->timed=d->final=0xfffffffeu;
    if(captured>1||(stage!=BA184_RESIDENT&&stage!=BA184_REGISTERED)
        ||(stage==BA184_RESIDENT&&captured!=1)){d->failure=12;return 0;}
    if(stage==BA184_REGISTERED)return 1;
    if(!o||!o->read||!o->wait||!o->clock||!o->scope||!epoch||!o->scope(u,epoch,captured)){d->failure=1;return 0;}
    uint8_t pause=1;uint32_t init=0,h=0,after=0;
    if(!o->read(u,0x9d9755u,&pause,1)||pause){d->failure=2;return 0;}
    if(!o->read(u,0x10a3c34u,&init,4)||init!=captured){d->failure=3;return 0;}
    if(!o->read(u,0x1551b88u,&h,4)||!h||h==0xffffffffu){d->failure=4;return 0;}
    d->zero=o->wait(u,(HANDLE)(uintptr_t)h,0);
    if(d->zero==WAIT_TIMEOUT){DWORD start=o->clock(u);d->attempted=1;
        d->timed=o->wait(u,(HANDLE)(uintptr_t)h,10000);d->elapsed=o->clock(u)-start;
        if(d->timed!=WAIT_OBJECT_0){d->failure=5;return 0;}}
    else if(d->zero!=WAIT_OBJECT_0){d->failure=6;return 0;}
    d->final=o->wait(u,(HANDLE)(uintptr_t)h,0);
    if(d->final!=WAIT_OBJECT_0){d->failure=7;return 0;}
    if(!o->read(u,0x1551b88u,&after,4)||after!=h){d->failure=8;return 0;}
    if(!o->read(u,0x9d9755u,&pause,1)||pause){d->failure=9;return 0;}
    if(!o->read(u,0x10a3c34u,&init,4)||init!=captured){d->failure=10;return 0;}
    if(!o->scope(u,epoch,captured)){d->failure=11;return 0;}return 1;
}
static DWORD native_clock(void *u){(void)u;return GetTickCount();}
static int native_worker_scope(void *u,uint32_t epoch,unsigned captured)
{
    (void)u;uint32_t so;uint8_t coop=0;
    return query_context.active&&query_context.tid==owner&&GetCurrentThreadId()==owner
        &&query_context.epoch==epoch&&query_context.init==captured&&captured==1
        &&state.installed&&!state.fatal&&!state.busy&&!state.revoked&&state.returned&&state.candidate
        &&state.epoch==epoch&&state.early_epoch!=epoch&&state.proof_stage==BA184_RESIDENT
        &&validate(NULL,BA184_FRAME,1)==BA184_ALL_VALIDATION&&native_fs(NULL)
        &&native_so(NULL,&so)&&so==0&&native_coop(NULL,&coop)&&coop==1&&native_map(NULL)==0;
}
static const ba184_worker_wait_ops worker_ops={read_memory,native_wait,native_clock,native_worker_scope};
static const ba184_native_proof_ops proof_ops={native_live,native_path,native_fs,native_db,native_so,native_coop,native_map,native_probe,native_init};
static int proof_decide(const ba184_native_proof_ops *o,void *u,uint32_t epoch,unsigned stage,unsigned captured_init,ba184_proof *out,ba184_native_diagnostic *d)
{
    memset(d,0,sizeof(*d));d->so=d->coop=d->init_before=d->init_after=0xffffffffu;d->captured_init=captured_init;
    if(!o->live(u)){d->failure=BA184_DETAIL_LIVE;return 0;}d->passed|=1;
    if(!o->path(u)){d->failure=BA184_DETAIL_PATH;return 0;}d->passed|=2;
    if(!o->fs(u)){d->failure=BA184_DETAIL_FS;return 0;}d->passed|=4;
    if(!o->init(u,&d->init_before)){d->failure=BA184_DETAIL_INIT_READ;return 0;}
    if(captured_init>1||d->init_before>1){d->failure=BA184_DETAIL_INIT_VALUE;return 0;}
    if(d->init_before!=captured_init){d->failure=BA184_DETAIL_INIT_DRIFT;return 0;}
    if((stage!=BA184_RESIDENT&&stage!=BA184_REGISTERED)||(stage==BA184_RESIDENT&&captured_init!=1)){
        d->failure=BA184_DETAIL_INIT_STAGE;return 0;}
    if(!o->db(u,captured_init,&d->before)){d->failure=BA184_DETAIL_DB_BEFORE;return 0;}d->passed|=8;
    if(!o->so(u,&d->so)){d->failure=BA184_DETAIL_SO_DVAR;return 0;}
    if(d->so){d->failure=BA184_DETAIL_SO_VALUE;return 0;}d->passed|=16;
    uint8_t coop=0;if(!o->coop(u,&coop)){d->failure=BA184_DETAIL_COOP_READ;return 0;}d->coop=coop;
    if(coop!=1){d->failure=BA184_DETAIL_COOP_VALUE;return 0;}d->passed|=32;
    d->failure=(uint32_t)o->map(u);if(d->failure)return 0;d->passed|=64;
    uint32_t bits=o->probe(u,stage,&d->probe);if(!bits){d->failure=BA184_DETAIL_PROBE;return 0;}d->passed|=128;
    if(!o->db(u,captured_init,&d->after)){d->failure=BA184_DETAIL_DB_AFTER;return 0;}d->passed|=256;
    if(!db_same(&d->before,&d->after)){d->failure=BA184_DETAIL_DB_DRIFT;return 0;}
    if(!o->init(u,&d->init_after)){d->failure=BA184_DETAIL_INIT_AFTER;return 0;}
    if(d->init_after>1||d->init_after!=captured_init){d->failure=BA184_DETAIL_INIT_DRIFT;return 0;}
    out->epoch=epoch;out->stage=stage;out->bits=bits|BA184_CAMPAIGN_TRAINER|BA184_PRE_SCRIPT;return 1;
}
static int proof(void *u,uint32_t epoch,unsigned stage,ba184_proof *out)
{
    (void)u;if(!query_context.active||query_context.tid!=owner||GetCurrentThreadId()!=owner){
        memset(&proof_detail,0,sizeof(proof_detail));proof_detail.failure=BA184_DETAIL_QUERY_CONTEXT;record("proof_detail",0);return 0;}
    if(stage==BA184_RESIDENT){int r=0;
        if(!query_context.worker_checked){query_context.worker_checked=1;
            r=worker_wait_using(&worker_ops,NULL,epoch,stage,query_context.init,&worker_detail);}
        if(!r){memset(&proof_detail,0,sizeof(proof_detail));proof_detail.failure=BA184_DETAIL_WORKER_WAIT;}
        record("worker_wait",r);if(!r)return 0;}
    int result=proof_decide(&proof_ops,NULL,epoch,stage,query_context.init,out,&proof_detail);
    if(!result)record("proof_detail",0);return result;
}
static const ba184_ops ops={validate,pin,patch,original,publish,proof};
static __attribute__((noinline,force_align_arg_pointer)) void __cdecl wrapper(const char *name,int second)
{
    char bounded[64];DWORD error=GetLastError();uint32_t so=1;uint8_t coop=0;
    if(GetCurrentThreadId()!=owner||(uintptr_t)__builtin_return_address(0)!=BA184_RETURN_VA
        ||!state.installed||state.fatal||state.busy)ExitProcess(0xe18400b0u);
    if(!text_a((uintptr_t)name,bounded,sizeof(bounded))||!live(1)||!dvar("specialops",0,&so))ExitProcess(0xe18400b1u);
    read_memory(NULL,0x88897cu,&coop,1);unsigned empty=(unsigned)fs_empty();
    SetLastError(error);int r=ba184_call(&state,GetCurrentThreadId(),(uintptr_t)__builtin_return_address(0),name,strlen(bounded)+1,second,coop,1,so,empty);
    error=GetLastError();record("loader_return",r);if(r<0)ExitProcess(0xe18400b2u);SetLastError(error);
}
int __cdecl bodyassets184_tick(DWORD tid)
{
    if(state.installed){int r=ba184_observe(&state,GetCurrentThreadId());if(tid!=owner)r=BA184_WRONG_THREAD;return r<0?-1:1;}
    if(report.attempted)return state.fatal?-1:0;
    if(!paths())return 0;DWORD a=GetFileAttributesW(enable);if(a==INVALID_FILE_ATTRIBUTES)return 0;
    report.requested=report.attempted=1;owner=tid;
    if(!tid||GetCurrentThreadId()!=tid||(uintptr_t)GetModuleHandleW(NULL)!=0x400000u){record("refused_owner_base",BA184_WRONG_THREAD);return 0;}
    bootstrap_report host;if(!bootstrap_guard_current(&host,sizeof(host))||host.flags!=MW2TRIO_GUARD_ALL||!live(0)){record("refused_image_live",BA184_GUARD);return 0;}
    if(!runtime184_body_channel_ready()){report.missing_contract=1;record("refused_channel",BA184_MISSING_PRESCRIPT_CONTRACT);return 0;}
    if(!pins()){record("refused_files_path",BA184_GUARD);close_pins();return 0;}
    ba184_initialize(&state,&ops,NULL,owner);initialized=1;
    replacement[0]=0xe8;int32_t delta=(int32_t)((uintptr_t)&wrapper-(BA184_CALL_VA+5u));memcpy(replacement+1,&delta,4);
    int r=ba184_install(&state,GetCurrentThreadId(),1,replacement);record("install",r);
    if(!state.installed&&!state.fatal)close_pins();return state.fatal?-1:state.installed?1:0;
}
int __cdecl bodyassets184_ready_now(void)
{
    if(!initialized||!state.installed)return 0;uint32_t init;
    DWORD tid=GetCurrentThreadId();
    if(tid!=owner||query_context.active){state.ready_epoch=0;state.revoked=1;state.result=BA184_GUARD;return 0;}
    if(!read_memory(NULL,0x10a3c34u,&init,4)){
        state.ready_epoch=0;state.revoked=1;state.result=BA184_GUARD;return 0;}
    query_context.tid=tid;query_context.init=init;query_context.epoch=state.epoch;query_context.worker_checked=0;query_context.active=1;
    int ready=ba184_ready(&state,tid,init);
    query_context.active=0;query_context.tid=0;query_context.epoch=0;query_context.init=0xffffffffu;query_context.worker_checked=0;
    if(ready&&state.proof_stage==BA184_RESIDENT&&logged_resident_epoch!=state.epoch){
        logged_resident_epoch=state.epoch;record("resident_proof",BA184_OK);}
    if(ready&&state.proof_stage==BA184_REGISTERED&&logged_registered_epoch!=state.epoch){
        logged_registered_epoch=state.epoch;record("registered_proof",BA184_OK);}
    if(!ready&&state.fatal)record("proof_refused",state.result);return ready;
}
int __cdecl bodyassets184_source_enabled(void)
{
    if(!initialized||!ba184_source_enabled(&state,GetCurrentThreadId()))return 0;
    uint32_t so,p;uint8_t coop=0;char map[128];
    if(!dvar("specialops",0,&so)||so||!read_memory(NULL,0x88897cu,&coop,1)||coop!=1
        ||!fs_empty()||!dvar("mapname",7,&p)||!text_a(p,map,sizeof(map))||strcmp(map,"trainer")){
        state.ready_epoch=0;state.revoked=1;return 0;}
    return 1;
}
void __cdecl bodyassets184_observe(bodyassets184_report *out)
{report.installed=state.installed;report.fatal=state.fatal;report.epoch=state.epoch;report.ready_epoch=state.ready_epoch;if(initialized)report.result=state.result;
    report.proof_mask=proof_detail.passed;report.proof_failure=proof_detail.failure;report.probe_status=proof_detail.probe.status;
    report.probe_step=proof_detail.probe.step;report.probe_pass=proof_detail.probe.pass;if(out)*out=report;}
