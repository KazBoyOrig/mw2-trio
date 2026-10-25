/* Original pure blocked-packet diagnostic classifier; MIT. */
#include "service184.h"
#include <string.h>

service184_kind service184_classify_blocked(
    const void *packet, size_t bytes,
    uint32_t sending_socket, uint32_t native_udp_socket,
    uint32_t native_address_type, const unsigned char native_ipv4[4],
    const service184_endpoint *destination)
{
    const unsigned char *p = (const unsigned char *)packet;
    if (!p || bytes != 39 || !native_ipv4 || !destination
        || !native_udp_socket || native_udp_socket == UINT32_MAX
        || sending_socket != native_udp_socket || native_address_type != 4
        || destination->family != 2 || destination->port_host_order != 1500
        || memcmp(native_ipv4, destination->ipv4, 4)) return SERVICE184_UNKNOWN;
    /* Native external-IP envelope5, serialization version3, command20,
     * request index0..23, reserved0. No OOB/R command is accepted here. */
    if (p[0] != 5 || p[1] != 3 || p[2] != 0x14 || p[3] >= 24 || p[4] != 0)
        return SERVICE184_UNKNOWN;
    /* Bytes5..6 are sequence;7..14 contain UID. Deliberately do not inspect.
     * The entire remaining type1 payload is six little-endian DWORDs0..5. */
    for (unsigned i = 0; i < 6; ++i) {
        size_t at = 15u + i * 4u;
        if (p[at] != i || p[at + 1] || p[at + 2] || p[at + 3])
            return SERVICE184_UNKNOWN;
    }
    return SERVICE184_EXTERNAL_IP_TYPE1;
}
const char *service184_kind_name(service184_kind kind)
{
    return kind == SERVICE184_EXTERNAL_IP_TYPE1 ? "external_ip_type1" : "unknown";
}
