/* Own MIT pure review policy. No hook, process, engine or filesystem access. */
#ifndef BODYASSETS184_POLICY_H
#define BODYASSETS184_POLICY_H
#include <stddef.h>
enum { BA_PASS_OTHER=0, BA_SUBSTITUTE_CANDIDATE=1, BA_REFUSED=2, BA_INVALID=-1 };
typedef struct bodyassets184_input {
    unsigned owner_thread, expected_caller, nonrecursive, live_guards;
    unsigned flag_sealed, files_sealed, fs_game_empty;
    unsigned coop, specialops_valid, specialops;
} bodyassets184_input;
/* The result is a decision only; this file never performs the substitution. */
static inline int bodyassets184_decide(const char *name,size_t bytes,const bodyassets184_input *s)
{
    static const char trainer[]="trainer";
    if(!s||!name||!bytes||bytes>64)return BA_INVALID;
    if(s->owner_thread!=1||s->expected_caller!=1||s->nonrecursive!=1||s->live_guards!=1)return BA_INVALID;
    size_t n=0;while(n<bytes&&name[n])++n;if(n==bytes)return BA_INVALID;
    if(n!=sizeof(trainer)-1)return BA_PASS_OTHER;
    for(size_t i=0;i<n;++i)if(name[i]!=trainer[i])return BA_PASS_OTHER;
    if(s->flag_sealed!=1||s->files_sealed!=1||s->fs_game_empty!=1||s->coop!=1
        ||s->specialops_valid!=1||s->specialops!=0)return BA_REFUSED;
    return BA_SUBSTITUTE_CANDIDATE;
}
#endif
