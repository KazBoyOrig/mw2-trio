/* Own MIT; caller-supplied operations, no OS or process access. */
#include "bodyassets184_tx.h"
#include <string.h>
int ba184_tx(const ba184_tx_ops *o,void *c,unsigned char *site,const unsigned char before[5],const unsigned char after[5])
{
    if(!o||!o->query_rx||!o->writable||!o->restore_rx||!o->flush||!site||!before||!after)return 0;
    if(!o->query_rx(c,site,5)||memcmp(site,before,5))return 0;
    int acquired=o->writable(c,site,5);
    if(acquired!=1)return acquired==0?0:-1;
    if(memcmp(site,before,5)){
        return o->restore_rx(c,site,5)&&o->query_rx(c,site,5)?0:-1;
    }
    memcpy(site,after,5);
    int flushed=!memcmp(site,after,5)&&o->flush(c,site,5);
    int restored=o->restore_rx(c,site,5)&&o->query_rx(c,site,5);
    if(restored&&memcmp(site,after,5))return -1;
    if(flushed&&restored)return 1;
    if(o->writable(c,site,5)!=1)return -1;
    memcpy(site,before,5);
    flushed=!memcmp(site,before,5)&&o->flush(c,site,5);
    restored=o->restore_rx(c,site,5)&&o->query_rx(c,site,5);
    return flushed&&restored&&!memcmp(site,before,5)?0:-1;
}
