/* Own MIT generic5-byte transaction. No native address is built into this API. */
#pragma once
#include <stddef.h>
typedef struct ba184_tx_ops {
    int (*query_rx)(void *,unsigned char *,size_t);
    /* 1 acquired writable state;0 unchanged failure;-1 uncertain state. */
    int (*writable)(void *,unsigned char *,size_t);
    int (*restore_rx)(void *,unsigned char *,size_t);
    int (*flush)(void *,unsigned char *,size_t);
} ba184_tx_ops;
/* Caller owns an exclusive execution boundary and a pinned target lifetime.
   Returns1 installed+RX,0 original+RX (or no write),-1 uncertain. Five bytes are
   not an atomic store. This cannot supply the missing native worker barrier. */
int ba184_tx(const ba184_tx_ops *,void *,unsigned char *,const unsigned char [5],const unsigned char [5]);
