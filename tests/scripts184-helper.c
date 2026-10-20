/* Own parser-only helper. No engine/native installation calls are made.
 * Included source is deliberate: exercises the actual private parser/path code.
 * Output files are private test material, never mod distribution assets.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../src/scripts184.c"

/* Fail-closed link stub; the helper cannot validate/patch a game process. */
int __cdecl bootstrap_guard_current(bootstrap_report *r, uint32_t bytes)
{
    if (r && bytes == sizeof(*r)) memset(r,0,sizeof(*r));
    return 0;
}

static unsigned failures, checks;
static void check(int ok, const char *name)
{
    ++checks;
    if (!ok) ++failures;
    printf("%s %s\n",ok ? "PASS" : "FAIL",name);
}

static void patch_case(const char *name, const char *source, const char *patch,
                       const char *expected, unsigned blocks_expected)
{
    size_t size = strlen(source);
    char *copy = malloc(size + 1);
    if (!copy) { check(0,name); return; }
    memcpy(copy,source,size + 1);
    unsigned blocks;
    int accepted = apply_patch(&copy,&size,patch,&blocks);
    check(expected ? accepted && blocks == blocks_expected && strcmp(copy,expected) == 0 : !accepted,name);
    free(copy);
}

static void self_test(void)
{
    patch_case("unique replace","foo foo2","@@@ replace\nfoo2\n@@@ with\nbar\n@@@ end\n","foo bar",1);
    patch_case("ambiguous replace rejected","x x","@@@ replace\nx\n@@@ with\ny\n@@@ end\n",NULL,0);
    patch_case("missing context rejected","a","@@@ replace\nb\n@@@ with\nc\n@@@ end\n",NULL,0);
    patch_case("missing end rejected","a","@@@ replace\na\n@@@ with\nb\n",NULL,0);
    patch_case("wrong operator rejected","a","@@@ replace\na\n@@@ to\nb\n@@@ end\n",NULL,0);
    patch_case("unknown command rejected","a","@@@ execute\nb\n@@@ end\n",NULL,0);
    patch_case("empty search rejected","a","@@@ replace\n@@@ with\nb\n@@@ end\n",NULL,0);
    patch_case("empty replacement supported","abc","@@@ replace\nb\n@@@ with\n@@@ end\n","ac",1);
    patch_case("append own function","a","@@@ append\nf(){}\n@@@ end\n","a\nf(){}\n",1);
    patch_case("terminal end without LF","a","@@@ replace\na\n@@@ with\nb\n@@@ end","b",1);
    patch_case("substring identifiers stay intact","f()\n{ player player2 _player player_x; }\ng()\n{ player; }\n",
        "@@@ subst f\nplayer\n@@@ to\nactor\n@@@ end\n",
        "f()\n{ actor player2 _player player_x; }\ng()\n{ player; }\n",1);
    patch_case("braces in comments and strings","f()\n{ /* } */ x=\"}\"; player; }\n",
        "@@@ subst f\nplayer\n@@@ to\nactor\n@@@ end\n","f()\n{ /* } */ x=\"}\"; actor; }\n",1);
    patch_case("missing function rejected","f(){}","@@@ subst missing\nx\n@@@ to\ny\n@@@ end\n",NULL,0);
    patch_case("later failed block rejects all result","a","@@@ replace\na\n@@@ with\nb\n@@@ end\n@@@ replace\nmissing\n@@@ with\nx\n@@@ end\n",NULL,0);
    patch_case("comment mention is not an operation","a","note @@@ replace\na\n",NULL,0);
    wchar_t name[NAME_LIMIT + 1];
    const char *allowed[] = {"maps/_load.gsc","common_scripts\\utility.gsc","maps/_mw2trio_probe.gsc","maps/mp/name-1.gsc"};
    for (unsigned i=0;i<sizeof(allowed)/sizeof(allowed[0]);++i) check(safe_name(allowed[i],name),allowed[i]);
    const char *blocked[] = {"../x.gsc","maps/../x.gsc","C:/x.gsc","\\\\server\\x.gsc","/maps/x.gsc",
        "maps//x.gsc","maps/","maps/a./x.gsc","CON.gsc","maps/AUX.foo","LPT1","com2.txt","maps/a b.gsc"};
    for (unsigned i=0;i<sizeof(blocked)/sizeof(blocked[0]);++i) check(!safe_name(blocked[i],name),blocked[i]);
    char long_name[NAME_LIMIT + 2];
    memset(long_name,'a',sizeof(long_name)-1); long_name[sizeof(long_name)-1]=0;
    check(!safe_name(long_name,name),"overlong name rejected");
    char *short_source=malloc(2); size_t size=1;
    if (short_source) {
        memcpy(short_source,"x",2);
        check(!replace_region(&short_source,&size,0,0,"",SOURCE_LIMIT+1),"oversize result refused before allocation");
        check(strcmp(short_source,"x")==0 && size==1,"refused allocation preserves owned input");
        free(short_source);
    } else check(0,"bounded-result fixture allocation");
}

static int input_file(const wchar_t *path, char **text, size_t *size)
{
    FILE *file=_wfopen(path,L"rb");
    if(!file) return 0;
    if(fseek(file,0,SEEK_END) || ftell(file)<0) {fclose(file);return 0;}
    long bytes=ftell(file);
    if((unsigned long)bytes>SOURCE_LIMIT || fseek(file,0,SEEK_SET)) {fclose(file);return 0;}
    char *buf=malloc((size_t)bytes+1);
    if(!buf) {fclose(file);return 0;}
    size_t got=fread(buf,1,(size_t)bytes,file); fclose(file);
    if(got!=(size_t)bytes || memchr(buf,0,got)) {free(buf);return 0;}
    size_t n=0;
    for(size_t i=0;i<got;++i) if(buf[i]!='\r')buf[n++]=buf[i];
    buf[n]=0; *text=buf; *size=n; return 1;
}

int wmain(int argc,wchar_t **argv)
{
    if(argc==1) {
        self_test();
        printf("RESULT checks=%u failures=%u\n",checks,failures);
        return failures ? 1 : 0;
    }
    if(argc!=4) {fputs("Usage: scripts184-helper [source patch private-output]\n",stderr);return 2;}
    char *source=NULL,*patch=NULL; size_t size=0,patch_size=0;
    if(!input_file(argv[1],&source,&size) || !input_file(argv[2],&patch,&patch_size)) {
        free(source);free(patch);fputs("Input read failed\n",stderr);return 3;
    }
    unsigned blocks=0;
    int ok=apply_patch(&source,&size,patch,&blocks);
    free(patch);
    if(!ok) {free(source);fprintf(stderr,"Patch rejected after %u blocks\n",blocks);return 4;}
    FILE *out=_wfopen(argv[3],L"wb");
    if(!out) {free(source);return 5;}
    size_t written=fwrite(source,1,size,out);
    int closed=fclose(out);
    printf("PATCH accepted=%d blocks=%u bytes=%u fnv1a=%08lx\n",written==size && !closed,blocks,(unsigned)size,(unsigned long)fnv1a(source,size));
    free(source);
    return written==size && !closed ? 0 : 6;
}
