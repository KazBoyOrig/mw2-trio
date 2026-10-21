/* Original token-only diagnostics; MIT. No game or socket dependencies. */
#ifndef MW2_RELIABLE184_H
#define MW2_RELIABLE184_H
#include <stddef.h>

typedef enum reliable184_outer {
    RELIABLE184_NOT_RECOGNIZED, RELIABLE184_R, RELIABLE184_RA
} reliable184_outer;
typedef enum reliable184_command {
    RELIABLE184_COMMAND_UNKNOWN,
    RELIABLE184_CONNECT, RELIABLE184_ACCEPT, RELIABLE184_ACCEPT_ACK,
    RELIABLE184_REJECT, RELIABLE184_LAUNCH, RELIABLE184_LOADED,
    RELIABLE184_RESTART, RELIABLE184_RESTART_ACK,
    RELIABLE184_HEARTBEAT, RELIABLE184_HEARTBEAT_ACK,
    RELIABLE184_DISCONNECT, RELIABLE184_BANDWIDTH
} reliable184_command;
typedef enum reliable184_reason {
    RELIABLE184_REASON_NONE, RELIABLE184_REASON_UNKNOWN,
    RELIABLE184_WRONGSTATE, RELIABLE184_FULL, RELIABLE184_WRONGPROTOCOL
} reliable184_reason;
typedef struct reliable184_token {
    reliable184_outer outer;
    reliable184_command first_command;
    reliable184_reason rejection;
} reliable184_token;

/* Input must describe readable bytes for its entire size. No text is copied,
 * retained or returned. R has no generic inner length prefix: decode ONLY its
 * first inner string at byte7. Binary fields and later batched records are not
 * scanned. RA accepts exactly "FFFFFFFF RA <0..255> NUL" (without spaces around
 * the FFFFFFFF/header boundary); its sequence is intentionally not returned.
 * Unknown/malformed inputs yield constant enum values only. */
reliable184_token reliable184_decode(const void *packet, size_t bytes);
const char *reliable184_outer_name(reliable184_outer value);
const char *reliable184_command_name(reliable184_command value);
const char *reliable184_reason_name(reliable184_reason value);
#endif
