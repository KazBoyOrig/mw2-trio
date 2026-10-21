/* Original pure classifier for an already denied native backend send; MIT. */
#include "backend184.h"
#include <string.h>

backend184_kind backend184_classify_blocked(const backend184_context *c,
                                          const service184_endpoint *destination)
{
    if (!c || !destination || c->known_coop_token || c->function_sha256_verified != 1
        || c->native_return_address != BACKEND184_RETURN_VA
        || !c->packet_bytes || c->packet_bytes > BACKEND184_MAX_BYTES
        || !c->native_backend_socket || c->native_backend_socket == UINT32_MAX
        || c->sending_socket != c->native_backend_socket
        || c->native_address_type != 4 || c->native_port_host_order != 3005
        || destination->family != 2 || destination->port_host_order != 3005
        || memcmp(c->native_ipv4, destination->ipv4, 4)) return BACKEND184_UNKNOWN;
    return BACKEND184_IWNET_3005;
}
const char *backend184_kind_name(backend184_kind kind)
{
    return kind == BACKEND184_IWNET_3005 ? "iwnet_3005" : "unknown";
}
