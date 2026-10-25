/* Own MIT candidate: pure two-participant configuration, no native integration. */
#ifndef MW2_PEER_SCOPE184_H
#define MW2_PEER_SCOPE184_H
#include <stddef.h>
#include <stdint.h>

#define PEER_SCOPE184_VERSION 1u
#define PEER_SCOPE184_MAX_CONFIG 256u
enum peer_scope184_role { PEER_SCOPE184_HOST=1, PEER_SCOPE184_CLIENT=2 };
enum peer_scope184_direction { PEER_SCOPE184_SEND=1, PEER_SCOPE184_RECEIVE=2 };
enum peer_scope184_result {
    PEER_SCOPE184_OK=1, PEER_SCOPE184_BAD_ARGUMENT=-1,
    PEER_SCOPE184_BAD_CONFIG=-2, PEER_SCOPE184_ALREADY_ATTEMPTED=-3,
    PEER_SCOPE184_NOT_READY=-4, PEER_SCOPE184_CONFIG_CHANGED=-5,
    PEER_SCOPE184_CONTEXT_CHANGED=-6
};
typedef struct peer_scope184_endpoint {
    uint8_t ipv4[4];
    uint32_t port; /* Host byte order, range1..65535. */
} peer_scope184_endpoint;
typedef struct peer_scope184_config {
    uint32_t version,role;
    uint8_t nonce[16]; /* Agreed identity seed, not a password/authenticator. */
    peer_scope184_endpoint local,peer;
    uint32_t source_bytes;
    uint8_t source_fingerprint[32]; /* Exact input bytes, including line endings. */
} peer_scope184_config;
typedef struct peer_scope184_context {
    uint32_t attempted,state; /* Caller owns zero-initialized lifetime storage. */
    int32_t last_result;
    peer_scope184_config config;
    uint8_t seal[32];
} peer_scope184_context;

/* Exactly seven unique key=value lines, any order, final newline required:
 * version=1; role=host|client; session_nonce=32hex; local_ip; local_port;
 * peer_ip; peer_port. LF or consistently CRLF. No spaces/comments/blank lines,
 * unknown keys, embedded NUL, numeric leading zeros or DNS. On failure *out is
 * unchanged. Input/output must be disjoint valid spans, not C strings.
 */
int peer_scope184_parse(const void *,size_t,peer_scope184_config *out);
/* Endpoint eligibility is deliberately conservative: no0/8,127/8,169.254/16,
 * multicast/reserved>=224, or last octet0/255. Explicit other IPv4 (e.g.26/8)
 * is eligible, NEVER a prefix-wide I/O permission. No ownership proof here. */
int peer_scope184_endpoint_eligible(const peer_scope184_endpoint *);
/* Additional pure caller-supplied subnet check. For prefix1..30 excludes that
 * subnet's network and directed broadcast; /31,/32 allow host semantics.
 * 'network' must be canonical for prefix. Caller must supply actual prefix.
 */
int peer_scope184_prefix_host(const uint8_t ip[4],const uint8_t network[4],unsigned prefix);
int peer_scope184_pair_compatible(const peer_scope184_config *,const peer_scope184_config *);
/* SHA256(domain-with-NUL || nonce16), big-endian first DWORD &7FFFFFFE,
 * zero->2; host low=base,client low=base|1. Original high is opaque and unchanged.
 * No UID memory/provider access, validation replacement or account identity. */
int peer_scope184_derive(const peer_scope184_config *,uint32_t original_high,uint64_t *out);
/* One attempt for this context lifetime, even if parsing fails. No reset API.
 * Failed/repeated latch or lost observation poisons this context. Source and
 * context/output storage must be disjoint. Parent must preserve ownership,
 * prevent deliberate reinitialization, and enforce process-lifetime opt-in. */
int peer_scope184_latch(peer_scope184_context *,const void *,size_t);
int peer_scope184_observe(peer_scope184_context *,const void *,size_t);
int peer_scope184_context_derive(peer_scope184_context *,uint32_t original_high,uint64_t *out);
/* ONLY an exact directional tuple predicate. Caller must first deny backend
 * provenance/shape and verify native socket, owned actual bind, mode/build and
 * successful-receive metadata. A match does not authorize I/O/authentication.
 * SEND source=local,destination=peer; RECEIVE source=peer,destination=local.
 */
int peer_scope184_matches(peer_scope184_context *,unsigned direction,
                         const peer_scope184_endpoint *source,
                         const peer_scope184_endpoint *destination);
#endif
