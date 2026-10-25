/* Original pure classifier for an already denied native backend send; MIT. */
#ifndef MW2_BACKEND184_H
#define MW2_BACKEND184_H
#include <stddef.h>
#include <stdint.h>
#include "service184.h"

#define BACKEND184_SENDER_VA 0x004d44f0u
#define BACKEND184_SENDER_BYTES 0xb6u
#define BACKEND184_RETURN_VA 0x004d4526u
#define BACKEND184_MAX_BYTES 4096u
#define BACKEND184_SENDER_SHA256 "88e58856c2352c1867d5201b3aa088480a48aef6bc255f8ba9312f39550a4a23"

typedef struct backend184_context {
    uintptr_t native_return_address;
    size_t packet_bytes;
    uint32_t sending_socket, native_backend_socket;
    uint32_t native_address_type, native_port_host_order;
    unsigned char native_ipv4[4];
    unsigned function_sha256_verified; /* Exactly1 after current live B6 guard. */
    unsigned known_coop_token;         /* Nonzero forces UNKNOWN/fatal policy. */
} backend184_context;
typedef enum backend184_kind { BACKEND184_UNKNOWN, BACKEND184_IWNET_3005 } backend184_kind;

/* No packet pointer is accepted. Capture frame0 return address INSIDE the
 * noinline IAT traced_send wrapper, before calling any helper. Caller checks
 * the exact current live native sender hash and image/readable spans before
 * supplying plain context. Native endpoint is1BE8F38, backend socket19FF94C;
 * convert both global port WORD and sockaddr port with ntohs before passing.
 * This classification NEVER permits I/O: every outside attempt still returns
 * SOCKET_ERROR/WSAEACCES; only this known denied class may be nonfatal. */
backend184_kind backend184_classify_blocked(const backend184_context *context,
                                          const service184_endpoint *destination);
const char *backend184_kind_name(backend184_kind kind);
#endif
