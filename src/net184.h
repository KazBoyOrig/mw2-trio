#pragma once
#include <stdint.h>
/* Fixed local QA address avoids native special treatment of 127.0.0.1. */
#define MW2TRIO_QA_IPV4_HOST 0x7f000002u

typedef struct net184_report {
    uint32_t installed, send_calls, send_oob, send_errors, recv_calls, received_packets;
    uint32_t received_oob, last_send_error, last_recv_error, qa_refused_external_ip, qa_refused_backend;
} net184_report;

/* Main-thread, opt-in, exact-image diagnostics. Ordinary mode preserves the
   original socket traffic and errors. Explicit loopback QA denies every other
   destination before the original socket callee; narrow known service refusals
   may continue, while unknown traffic and lost scope are fatal. */
int net184_tick(void);
void net184_observe(net184_report *out);
/* True only after the latched QA loopback boundary is installed and intact. */
int net184_loopback_ready(void);
int net184_loopback_requested(void);
/* Separate explicit exact-peer mode; never broadens the QA address boundary. */
int net184_peer_ready(void);
int net184_peer_requested(void);
