/* Own inert fixture strings and decision inputs only. No game/native calls. */
#include <stdio.h>
#include <string.h>
#include "../src/bodyassets184_policy.h"
static unsigned checks,failed;
static void check(int ok,const char *why){++checks;failed+=!ok;if(!ok)printf("FAIL %s\n",why);}
int main(void)
{
    bodyassets184_input good={1,1,1,1,1,1,1,1,1,0};
    check(bodyassets184_decide("trainer",8,&good)==BA_SUBSTITUTE_CANDIDATE,"exact bounded trainer candidate");
    check(bodyassets184_decide("trainer",64,&good)==BA_SUBSTITUTE_CANDIDATE,"long readable bound retains exact name");
    check(bodyassets184_decide("trainer",7,&good)==BA_INVALID,"missing NUL at bound");
    check(bodyassets184_decide("trainer",0,&good)==BA_INVALID,"zero bound");
    check(bodyassets184_decide("trainer",65,&good)==BA_INVALID,"overlarge bound");
    check(bodyassets184_decide(NULL,8,&good)==BA_INVALID,"null name");
    check(bodyassets184_decide("trainer",8,NULL)==BA_INVALID,"null context");
    const char *other[]={"","Trainer","TRAINER","trainer2","trainer "," trainer","so_killspree_trainer","airport","train","trainer/","trainer\\","trainer\n"};
    for(unsigned i=0;i<sizeof(other)/sizeof(other[0]);++i)
        check(bodyassets184_decide(other[i],strlen(other[i])+1,&good)==BA_PASS_OTHER,"nonexact name passthrough");
    for(unsigned which=0;which<10;++which){
        bodyassets184_input bad=good;unsigned *v[]={&bad.owner_thread,&bad.expected_caller,&bad.nonrecursive,&bad.live_guards,
            &bad.flag_sealed,&bad.files_sealed,&bad.fs_game_empty,&bad.coop,&bad.specialops_valid,&bad.specialops};
        *v[which]=which==9?1:0;
        check(bodyassets184_decide("trainer",8,&bad)==(which<4?BA_INVALID:BA_REFUSED),"one failed prerequisite");
        *v[which]=2;
        check(bodyassets184_decide("trainer",8,&bad)==(which<4?BA_INVALID:BA_REFUSED),"noncanonical prerequisite value");
    }
    bodyassets184_input absent=good;absent.flag_sealed=0;absent.files_sealed=0;absent.fs_game_empty=0;
    check(bodyassets184_decide("airport",8,&absent)==BA_PASS_OTHER,"unrelated map unaffected by target prerequisites");
    char mutable_name[8]="trainer",saved[8];memcpy(saved,mutable_name,8);
    check(bodyassets184_decide(mutable_name,8,&good)==BA_SUBSTITUTE_CANDIDATE&&!memcmp(saved,mutable_name,8),"decision never writes caller string");
    char nonnul[64];memset(nonnul,'x',sizeof(nonnul));
    check(bodyassets184_decide(nonnul,sizeof(nonnul),&good)==BA_INVALID,"bounded unterminated input");
    printf("bodyassets184 pure policy: %u checks, %u failures; no native integration\n",checks,failed);
    return failed?1:0;
}
