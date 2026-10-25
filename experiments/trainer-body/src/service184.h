/* Original pure blocked-packet diagnostic classifier; MIT. */
#ifndef MW2_SERVICE184_H
#define MW2_SERVICE184_H
#include <stddef.h>
#include <stdint.h>

typedef struct service184_endpoint {
    uint32_t family;             /* Native Windows AF_INET is2. */
    unsigned char ipv4[4];       /* Byte order in sockaddr_in.sin_addr. */
    uint32_t port_host_order;
} service184_endpoint;
typedef enum service184_kind {
    SERVICE184_UNKNOWN, SERVICE184_EXTERNAL_IP_TYPE1
} service184_kind;

/* This classifies an already BLOCKED outbound attempt for diagnostics/abort
 * policy only. It never authorizes a send. Caller supplies readable spans and
 * a validated current native UDP socket plus type/IPv4 from the native endpoint
 * global. No native pointers or UID/sequence bytes are retained or returned.
 * Native endpoint port is deliberately not an argument: the sender overrides
 * only its stack-local address copy to1500; the global port is unchanged.
 * Type2 requests are unsupported and remain UNKNOWN. */
service184_kind service184_classify_blocked(
    const void *packet, size_t bytes,
    uint32_t sending_socket, uint32_t native_udp_socket,
    uint32_t native_address_type, const unsigned char native_ipv4[4],
    const service184_endpoint *destination);
const char *service184_kind_name(service184_kind kind);
#endif
