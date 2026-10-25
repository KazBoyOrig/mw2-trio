/* Own MIT. Include actual adapter to exercise only its own-page protection and
   own-fixture file/log operations. No tick, wrapper, proof, engine address or
   original loader is invoked. All native callbacks are inert link stubs. */
#include "../src/bodyassets184.c"
static unsigned checks,failures;
static void check(int value,const char *message){++checks;if(!value){++failures;printf("FAIL %s\n",message);}}
int __cdecl bootstrap_guard_current(bootstrap_report *r,uint32_t n){(void)r;(void)n;return 0;}
int __cdecl runtime184_body_channel_ready(void){return 0;}
static DWORD protection(void *p){MEMORY_BASIC_INFORMATION m;return VirtualQuery(p,&m,sizeof(m))==sizeof(m)?m.Protect:0;}
static int write_own(const wchar_t *path,const char *bytes)
{
    HANDLE h=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(h==INVALID_HANDLE_VALUE)return 0;
    DWORD n=0;int good=WriteFile(h,bytes,(DWORD)strlen(bytes),&n,NULL)&&n==strlen(bytes);CloseHandle(h);return good;
}
static int own_size(const wchar_t *path,DWORD wanted)
{
    HANDLE h=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(h==INVALID_HANDLE_VALUE)return 0;BY_HANDLE_FILE_INFORMATION i;int good=GetFileInformationByHandle(h,&i)&&!i.nFileSizeHigh&&i.nFileSizeLow==wanted;CloseHandle(h);return good;
}
typedef struct proof_fixture {
    unsigned reads,waits,dbs,used,order[16],fail_op,fail_read,null_read;
    DWORD result[4];uint32_t so;unsigned probe_fail,stage;
    uint32_t init_value[2],handle[4];unsigned init_reads,init_fail,mutate_mask;
} proof_fixture;
static void reset_proof(proof_fixture *f){memset(f,0,sizeof(*f));f->init_value[0]=f->init_value[1]=1;}
static int op(proof_fixture *f,unsigned id){if(f->used>=16)return 0;f->order[f->used++]=id;return f->fail_op!=id;}
static int fake_read(void *u,uintptr_t a,void *out,size_t n)
{proof_fixture *f=u;++f->reads;if(f->reads==f->fail_read||f->reads>4)return 0;
    if(n!=4||(a!=0x1551bd8u&&a!=0x1551b88u))return 0;
    uint32_t h=f->reads==f->null_read?0:f->handle[f->reads-1]?f->handle[f->reads-1]:a==0x1551bd8u?1:2;
    memcpy(out,&h,4);return 1;}
static DWORD fake_wait(void *u,HANDLE h,DWORD timeout)
{proof_fixture *f=u;if(timeout||(uintptr_t)h<1||(uintptr_t)h>4||f->waits>=4)return WAIT_FAILED;return f->result[f->waits++];}
static int fake_live(void *u){return op(u,1);}static int fake_path(void *u){return op(u,2);}static int fake_fs(void *u){return op(u,3);}
static int fake_db(void *u,unsigned rawinit,ba184_db_diagnostic *d)
{proof_fixture *f=u;unsigned id=++f->dbs==1?4:9;int r=op(f,id)&&db_phase_using(fake_read,fake_wait,f,rawinit,d);
    if(f->dbs==2&&f->mutate_mask)d->read_mask=1;return r;}
static int fake_so(void *u,uint32_t *v){proof_fixture *f=u;*v=f->so;return op(f,5);}
static int fake_coop(void *u,uint8_t *v){*v=1;return op(u,6);}
static int fake_map(void *u){return op(u,7)?0:BA184_DETAIL_MAP_NAME;}
static uint32_t fake_probe(void *u,unsigned stage,ba184_probe_diagnostic *d)
{proof_fixture *f=u;f->stage=stage;if(!op(f,8)||f->probe_fail){d->status=BA184_PROBE_READ;d->step=BA184_STEP_COUNT;d->pass=1;return 0;}
    d->status=BA184_PROBE_OK;return BA184_ALL_PROOF&~(BA184_CAMPAIGN_TRAINER|BA184_PRE_SCRIPT);}
static int fake_init(void *u,uint32_t *v)
{proof_fixture *f=u;unsigned i=f->init_reads++;if(i>1||!op(f,10+i)||f->init_fail==i+1)return 0;*v=f->init_value[i];return 1;}
static const ba184_native_proof_ops fake_ops={fake_live,fake_path,fake_fs,fake_db,fake_so,fake_coop,fake_map,fake_probe,fake_init};
static void proof_diagnostic_tests(void)
{
    proof_fixture f;reset_proof(&f);ba184_native_diagnostic d;ba184_proof p={0};
    const unsigned order[]={1,2,3,10,4,5,6,7,8,9,11};
    check(proof_decide(&fake_ops,&f,7,BA184_RESIDENT,1,&p,&d)&&d.passed==511&&!d.failure&&p.epoch==7&&p.stage==1&&p.bits==BA184_ALL_PROOF
        &&f.waits==4&&f.reads==4&&f.used==11&&!memcmp(f.order,order,sizeof(order)),"native proof success order, canonical captured init and stage");
    for(unsigned i=1;i<=3;++i){reset_proof(&f);f.fail_op=i;
        check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.failure==i&&f.used==i&&!f.waits&&!f.reads,"outer early predicates short-circuit before phase/DB");}
    reset_proof(&f);f.result[0]=WAIT_TIMEOUT;ba184_db_diagnostic strict;
    check(!db_ready_using(fake_read,fake_wait,&f,&strict)&&f.reads==1&&f.waits==1&&strict.wait[1]==0xfffffffeu,"install DB policy remains strict and short-circuiting");
    for(unsigned stage=1;stage<=2;++stage){reset_proof(&f);f.result[0]=f.result[2]=WAIT_TIMEOUT;
        check(proof_decide(&fake_ops,&f,7,stage,1,&p,&d)&&f.waits==4&&d.before.wait[0]==WAIT_TIMEOUT&&d.before.wait[1]==0
            &&d.init_before==1&&d.init_after==1,"resident and registered init1 permit stable main-finalizer pending, worker complete");}
    reset_proof(&f);f.init_value[0]=f.init_value[1]=0;
    check(proof_decide(&fake_ops,&f,7,2,0,&p,&d)&&f.waits==4,"registered init0 requires and accepts both completed events");
    reset_proof(&f);f.init_value[0]=f.init_value[1]=0;f.result[0]=WAIT_TIMEOUT;
    check(!proof_decide(&fake_ops,&f,7,2,0,&p,&d)&&d.failure==BA184_DETAIL_DB_BEFORE&&f.waits==1,"registered init0 pending finalizer refused");
    reset_proof(&f);f.init_value[0]=f.init_value[1]=0;
    check(!proof_decide(&fake_ops,&f,7,1,0,&p,&d)&&d.failure==BA184_DETAIL_INIT_STAGE&&!f.waits,"resident cannot start outside initialization");
    reset_proof(&f);
    check(!proof_decide(&fake_ops,&f,7,2,0,&p,&d)&&d.failure==BA184_DETAIL_INIT_DRIFT&&!f.waits,"core-captured init0 cannot borrow native init1 exception");
    reset_proof(&f);f.init_value[0]=f.init_value[1]=0;
    check(!proof_decide(&fake_ops,&f,7,2,1,&p,&d)&&d.failure==BA184_DETAIL_INIT_DRIFT&&!f.waits,"captured init1/native init0 mismatch refused");
    reset_proof(&f);f.result[1]=WAIT_TIMEOUT;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.failure==BA184_DETAIL_DB_BEFORE&&f.waits==2&&d.before.attempted==3,"worker completion is always mandatory");
    const DWORD bad_waits[]={WAIT_FAILED,WAIT_ABANDONED,1,257,259};
    for(unsigned i=0;i<sizeof(bad_waits)/sizeof(bad_waits[0]);++i){reset_proof(&f);f.result[0]=bad_waits[i];
        check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.before.wait[0]==bad_waits[i]&&f.waits==1,"invalid main event wait never treated as pending");}
    reset_proof(&f);f.null_read=1;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.before.read_mask==1&&!d.before.valid_mask&&!f.waits,"null DB handle refused without wait");
    reset_proof(&f);f.handle[0]=0xffffffffu;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&!f.waits&&!d.before.valid_mask,"invalid sentinel event handle refused");
    reset_proof(&f);f.fail_read=1;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&!d.before.read_mask&&!f.waits,"unreadable event handle refused");
    reset_proof(&f);f.handle[1]=1;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.failure==BA184_DETAIL_DB_BEFORE,"distinct native events cannot alias same handle");
    reset_proof(&f);f.result[2]=WAIT_FAILED;
    check(!proof_decide(&fake_ops,&f,7,2,1,&p,&d)&&d.failure==BA184_DETAIL_DB_AFTER&&d.passed==255&&f.waits==3,"post-read DB failure retained without fourth wait");
    reset_proof(&f);f.result[2]=WAIT_TIMEOUT;
    check(!proof_decide(&fake_ops,&f,7,2,1,&p,&d)&&d.failure==BA184_DETAIL_DB_DRIFT&&f.waits==4,"ready to pending event drift refused");
    reset_proof(&f);f.result[0]=WAIT_TIMEOUT;
    check(!proof_decide(&fake_ops,&f,7,2,1,&p,&d)&&d.failure==BA184_DETAIL_DB_DRIFT,"pending to ready event drift also refused");
    reset_proof(&f);f.handle[2]=3;
    check(!proof_decide(&fake_ops,&f,7,2,1,&p,&d)&&d.failure==BA184_DETAIL_DB_DRIFT,"event handle replacement across proof refused");
    reset_proof(&f);f.mutate_mask=1;
    check(!proof_decide(&fake_ops,&f,7,2,1,&p,&d)&&d.failure==BA184_DETAIL_DB_DRIFT,"read-mask drift refused");
    reset_proof(&f);f.init_fail=1;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.failure==BA184_DETAIL_INIT_READ&&!f.waits,"initial init read failure refuses before event wait");
    reset_proof(&f);f.init_value[0]=2;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.failure==BA184_DETAIL_INIT_VALUE&&!f.waits,"noncanonical init2 refused");
    reset_proof(&f);
    check(!proof_decide(&fake_ops,&f,7,1,2,&p,&d)&&d.failure==BA184_DETAIL_INIT_VALUE,"noncanonical captured init refused");
    reset_proof(&f);f.init_fail=2;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.failure==BA184_DETAIL_INIT_AFTER&&f.waits==4,"final init read failure refuses");
    reset_proof(&f);f.init_value[1]=0;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.failure==BA184_DETAIL_INIT_DRIFT,"init change during proof refuses");
    reset_proof(&f);f.init_value[1]=2;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.failure==BA184_DETAIL_INIT_DRIFT,"final noncanonical init refuses");
    reset_proof(&f);
    check(!proof_decide(&fake_ops,&f,7,3,1,&p,&d)&&d.failure==BA184_DETAIL_INIT_STAGE&&!f.waits,"unknown proof stage refused");
    reset_proof(&f);f.so=1;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.failure==BA184_DETAIL_SO_VALUE&&d.passed==15,"specialops nonzero retained");
    reset_proof(&f);f.fail_op=5;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.failure==BA184_DETAIL_SO_DVAR,"specialops lookup refused separately");
    reset_proof(&f);f.fail_op=7;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.failure==BA184_DETAIL_MAP_NAME,"map gate remains before probe");
    reset_proof(&f);f.probe_fail=1;
    check(!proof_decide(&fake_ops,&f,7,1,1,&p,&d)&&d.failure==BA184_DETAIL_PROBE&&d.probe.status==BA184_PROBE_READ
        &&d.probe.pass==1&&d.passed==127&&f.waits==2,"resource parser failure retains diagnostic and short-circuit");
}
typedef struct wait_fixture {
    unsigned reads,waits,clocks,scopes,hreads,ireads,preads,fail_read,fail_scope;
    uint32_t handles[2],inits[2];uint8_t pauses[2];DWORD returns[3],timeouts[3];
} wait_fixture;
static void reset_wait(wait_fixture *f){memset(f,0,sizeof(*f));f->handles[0]=f->handles[1]=7;f->inits[0]=f->inits[1]=1;}
static int wait_read(void *u,uintptr_t a,void *out,size_t n)
{wait_fixture *f=u;if(++f->reads==f->fail_read)return 0;
    if(a==0x1551b88u&&n==4&&f->hreads<2){memcpy(out,&f->handles[f->hreads++],4);return 1;}
    if(a==0x10a3c34u&&n==4&&f->ireads<2){memcpy(out,&f->inits[f->ireads++],4);return 1;}
    if(a==0x9d9755u&&n==1&&f->preads<2){memcpy(out,&f->pauses[f->preads++],1);return 1;}return 0;}
static DWORD wait_call(void *u,HANDLE h,DWORD timeout)
{wait_fixture *f=u;if((uintptr_t)h!=7||f->waits>=3)return WAIT_FAILED;
    f->timeouts[f->waits]=timeout;return f->returns[f->waits++];}
static DWORD wait_clock(void *u){wait_fixture *f=u;return 100u+37u*f->clocks++;}
static int wait_scope(void *u,uint32_t epoch,unsigned captured)
{wait_fixture *f=u;return ++f->scopes!=f->fail_scope&&epoch==17&&captured==1;}
static void worker_wait_tests(void)
{
    const ba184_worker_wait_ops o={wait_read,wait_call,wait_clock,wait_scope};wait_fixture f;ba184_worker_diagnostic d;
    reset_wait(&f);
    check(worker_wait_using(&o,&f,17,1,1,&d)&&f.waits==2&&!f.timeouts[0]&&!f.timeouts[1]&&!f.clocks&&!d.attempted
        &&f.scopes==2&&f.reads==6,"already signaled worker skips bounded wait and rechecks all postconditions");
    reset_wait(&f);f.returns[0]=WAIT_TIMEOUT;
    check(worker_wait_using(&o,&f,17,1,1,&d)&&f.waits==3&&f.timeouts[0]==0&&f.timeouts[1]==10000&&f.timeouts[2]==0
        &&f.clocks==2&&d.attempted==1&&d.elapsed==37&&d.zero==WAIT_TIMEOUT&&d.timed==0&&d.final==0,"one bounded ten-second worker wait then fresh zero-time confirmation");
    const DWORD failed[]={WAIT_TIMEOUT,WAIT_FAILED,WAIT_ABANDONED,1};
    for(unsigned i=0;i<4;++i){reset_wait(&f);f.returns[0]=WAIT_TIMEOUT;f.returns[1]=failed[i];
        check(!worker_wait_using(&o,&f,17,1,1,&d)&&d.failure==5&&f.waits==2&&f.scopes==1,"bounded timeout/failure never promotes pending and never retries");}
    reset_wait(&f);f.returns[0]=WAIT_FAILED;
    check(!worker_wait_using(&o,&f,17,1,1,&d)&&d.failure==6&&f.waits==1&&!f.clocks,"initial failed wait never enters bounded wait");
    reset_wait(&f);f.returns[1]=WAIT_TIMEOUT;
    check(!worker_wait_using(&o,&f,17,1,1,&d)&&d.failure==7&&f.waits==2,"final zero-time worker recheck is mandatory");
    reset_wait(&f);f.pauses[0]=1;
    check(!worker_wait_using(&o,&f,17,1,1,&d)&&d.failure==2&&!f.waits,"paused worker refuses before any wait");
    reset_wait(&f);f.pauses[1]=1;
    check(!worker_wait_using(&o,&f,17,1,1,&d)&&d.failure==9,"pause transition after wait refuses");
    reset_wait(&f);f.handles[0]=0;
    check(!worker_wait_using(&o,&f,17,1,1,&d)&&d.failure==4&&!f.waits,"null worker handle refuses before wait");
    reset_wait(&f);f.handles[0]=0xffffffffu;
    check(!worker_wait_using(&o,&f,17,1,1,&d)&&d.failure==4&&!f.waits,"invalid worker handle refuses before wait");
    reset_wait(&f);f.handles[1]=8;
    check(!worker_wait_using(&o,&f,17,1,1,&d)&&d.failure==8,"worker handle replacement after wait refuses");
    for(unsigned v=0;v<=2;v+=2){reset_wait(&f);f.inits[0]=v;
        check(!worker_wait_using(&o,&f,17,1,1,&d)&&d.failure==3&&!f.waits,"nonmatching raw init refuses before wait");
        reset_wait(&f);f.inits[1]=v;
        check(!worker_wait_using(&o,&f,17,1,1,&d)&&d.failure==10,"raw init drift/noncanonical post-read refuses");}
    for(unsigned i=1;i<=6;++i){reset_wait(&f);f.fail_read=i;
        check(!worker_wait_using(&o,&f,17,1,1,&d),"every bounded-wait read failure refuses");}
    reset_wait(&f);f.fail_scope=1;
    check(!worker_wait_using(&o,&f,17,1,1,&d)&&d.failure==1&&!f.reads&&!f.waits,"invalid owner/query/epoch scope refuses before reads");
    reset_wait(&f);f.fail_scope=2;
    check(!worker_wait_using(&o,&f,17,1,1,&d)&&d.failure==11,"changed owner/query/epoch scope refuses after wait");
    reset_wait(&f);
    check(worker_wait_using(&o,&f,17,2,1,&d)&&!f.reads&&!f.waits&&!f.scopes,"registered query never waits");
    reset_wait(&f);
    check(!worker_wait_using(&o,&f,17,1,0,&d)&&d.failure==12&&!f.reads&&!f.waits,"resident init0 is ineligible without waiting");
    reset_wait(&f);
    check(!worker_wait_using(&o,&f,17,1,2,&d)&&d.failure==12&&!f.reads&&!f.waits,"noncanonical captured init never waits");
    reset_wait(&f);
    check(!worker_wait_using(&o,&f,17,3,1,&d)&&d.failure==12&&!f.reads&&!f.waits,"unknown stage never waits");
}
int main(void)
{
    proof_diagnostic_tests();worker_wait_tests();
    check(!bodyassets184_ready_now()&&!bodyassets184_source_enabled(),"default-off public predicates inert");
    unsigned char *page=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);DWORD old;
    check(page!=NULL,"own inert page allocation");if(!page)return 1;memcpy(page+19,"abcde",5);
    const ba_guard code={0x4d0450u,279u,NULL},data_a={0x7302b8u,4u,NULL},data_b={0x730368u,4u,NULL},data_c={0x73187cu,12u,NULL};
    const ba_guard data_wrong_width={0x7302b8u,8u,NULL},descriptor_wrong_width={0x73187cu,4u,NULL},unlisted_data={0x7302bcu,4u,NULL};
    const DWORD protections[]={PAGE_EXECUTE_READ,PAGE_READONLY,PAGE_READWRITE,PAGE_EXECUTE,
        PAGE_NOACCESS,PAGE_READWRITE|PAGE_GUARD,PAGE_EXECUTE_READWRITE};
    for(unsigned i=0;i<sizeof(protections)/sizeof(protections[0]);++i){
        check(VirtualProtect(page,4096,protections[i],&old)!=0,"own fixture protection transition");
        DWORD observed=protection(page);
        check(guard_protection(&code,observed)==(protections[i]==PAGE_EXECUTE_READ),"actual code guard requires RX, refuses RO/nonexec/guard/RWX");
        int expected=protections[i]==PAGE_READONLY||protections[i]==PAGE_READWRITE;
        check(guard_protection(&data_a,observed)==expected&&guard_protection(&data_b,observed)==expected
            &&guard_protection(&data_c,observed)==expected,"exact descriptor data guards accept RO/RW only among own anonymous-page cases");
        check(!guard_protection(&data_wrong_width,observed)&&!guard_protection(&descriptor_wrong_width,observed)
            &&!guard_protection(&unlisted_data,observed),"data permission exception does not extend descriptor widths or neighboring addresses");
    }
    check(VirtualProtect(page,4096,PAGE_EXECUTE_READ,&old)!=0,"own fixture RX initial state");
    check(writable(NULL,page+19,5)==1&&protection(page)==PAGE_EXECUTE_READWRITE,"actual adapter expected RX becomes writable");
    check(restore(NULL,page+19,5)&&protection(page)==PAGE_EXECUTE_READ,"actual adapter restores RX");
    check(VirtualProtect(page,4096,PAGE_READWRITE,&old)!=0,"own fixture unexpected old state");
    check(writable(NULL,page+19,5)==-1&&protection(page)==PAGE_READWRITE,"unexpected old protection restored exactly and uncertain result");
    check(!memcmp(page+19,"abcde",5),"protection helper writes no instruction bytes");
    check(VirtualFree(page,0,MEM_RELEASE)!=0,"only own inert page released");

    wchar_t cwd[MAX_PATH],root[MAX_PATH],file[MAX_PATH],extra[MAX_PATH],hard[MAX_PATH];
    DWORD n=GetCurrentDirectoryW(MAX_PATH,cwd);if(!n||n>MAX_PATH-70)return 1;
    swprintf(root,MAX_PATH,L"%ls\\bodyassets184-own-fixture-%lu",cwd,(unsigned long)GetCurrentProcessId());
    check(CreateDirectoryW(root,NULL)!=0,"new own fixture directory");
    swprintf(file,MAX_PATH,L"%ls\\sealed.txt",root);swprintf(extra,MAX_PATH,L"%ls\\sentinel.txt",root);
    swprintf(hard,MAX_PATH,L"%ls\\log-link.txt",root);
    check(write_own(file,"abc"),"new own hash fixture");
    parent_count=1;check(open_pin(&parents[0],root,1,0,NULL),"actual nonreparse directory pin");
    check(open_pin(&files[1],file,0,3,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),"actual exact own-file hash pin");
    check(seal(&parents[0],1)&&seal(&files[1],0),"actual held IDs stable");
    HANDLE writer=CreateFileW(file,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    check(writer==INVALID_HANDLE_VALUE,"held seal denies concurrent write handle");if(writer!=INVALID_HANDLE_VALUE)CloseHandle(writer);
    check(!MoveFileExW(file,hard,MOVEFILE_REPLACE_EXISTING),"held seal denies rename");
    check(write_own(extra,"sentinel"),"new own logger sentinel");
    HANDLE copy_file=CreateFileW(extra,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    check(copy_file!=INVALID_HANDLE_VALUE,"own file for actual copy-on-write data protection");
    HANDLE mapping=copy_file==INVALID_HANDLE_VALUE?NULL:CreateFileMappingW(copy_file,NULL,PAGE_WRITECOPY,0,0,NULL);
    unsigned char *view=mapping?MapViewOfFile(mapping,FILE_MAP_COPY,0,0,0):NULL;
    check(view!=NULL,"own copy-on-write mapping");
    if(view){DWORD observed=protection(view);
        check(observed==PAGE_WRITECOPY&&guard_protection(&data_a,observed)&&guard_protection(&data_b,observed)&&guard_protection(&data_c,observed)
            &&!guard_protection(&code,observed),"actual file WRITECOPY permitted only for exact data guards");
        view[0]='X';observed=protection(view);
        check((observed==PAGE_READWRITE||observed==PAGE_WRITECOPY)&&guard_protection(&data_a,observed)
            &&!guard_protection(&code,observed),"materialized own data copy remains data-only");
        check(UnmapViewOfFile(view)!=0,"own COW view released");}
    if(mapping)CloseHandle(mapping);if(copy_file!=INVALID_HANDLE_VALUE)CloseHandle(copy_file);
    check(own_size(extra,8),"private COW fixture leaves original own file intact");
    check(seal(&parents[0],1),"directory seal tolerates unrelated creation while preserving ID");
    BY_HANDLE_FILE_INFORMATION copy=files[1].info;files[1].info.nFileIndexLow^=1;
    check(!seal(&files[1],0),"file ID disagreement refused");files[1].info=copy;
    files[1].info.nFileSizeLow^=1;check(!seal(&files[1],0),"file size disagreement refused");files[1].info=copy;
    BY_HANDLE_FILE_INFORMATION bad=copy;bad.dwFileAttributes|=FILE_ATTRIBUTE_REPARSE_POINT;
    check(!same(&copy,&bad,0),"reparse metadata never accepted");
    lstrcpyW(logpath,hard);opened=0;record("fixture_before_pins",17);
    check(GetFileAttributesW(hard)==INVALID_FILE_ATTRIBUTES&&report.result==17,"logger creates nothing before trusted pins");
    check(CreateHardLinkW(hard,extra,NULL)!=0,"new own hardlink log trap");
    opened=1;record("fixture_hardlink",18);
    check(own_size(extra,8)&&own_size(hard,8)&&report.result==18,"actual logger refuses linked output before append");
    check(DeleteFileW(hard)!=0,"remove own hardlink");record("fixture_regular",19);
    check(!own_size(hard,0)&&GetFileAttributesW(hard)!=INVALID_FILE_ATTRIBUTES,"actual logger appends only ordinary own output");
    files[0].h=files[2].h=INVALID_HANDLE_VALUE;close_pins();
    check(DeleteFileW(file)&&DeleteFileW(extra)&&DeleteFileW(hard)&&RemoveDirectoryW(root),"only known own fixture files cleaned");
    printf("bodyassets184 actual adapter: %u checks, %u failures; own files/pages only\n",checks,failures);
    return failures?1:0;
}
