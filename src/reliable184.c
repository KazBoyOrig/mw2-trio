/* Original token-only diagnostics; MIT. No game or socket dependencies. */
#include "reliable184.h"
#include <string.h>

typedef struct allowed_command {
    const char *text;
    reliable184_command value;
} allowed_command;
static const allowed_command allowed[] = {
    {"coopConnect", RELIABLE184_CONNECT}, {"coopAccept", RELIABLE184_ACCEPT},
    {"coopAcceptAck", RELIABLE184_ACCEPT_ACK}, {"coopLaunch", RELIABLE184_LAUNCH},
    {"coopLoaded", RELIABLE184_LOADED}, {"coopRestart", RELIABLE184_RESTART},
    {"coopRestartAck", RELIABLE184_RESTART_ACK}, {"hb", RELIABLE184_HEARTBEAT},
    {"hba", RELIABLE184_HEARTBEAT_ACK}, {"disLobby", RELIABLE184_DISCONNECT},
    {"bw", RELIABLE184_BANDWIDTH}
};
static int same(const unsigned char *text, size_t bytes, const char *literal)
{
    size_t n = strlen(literal);
    return n == bytes && memcmp(text, literal, n) == 0;
}
reliable184_token reliable184_decode(const void *packet, size_t bytes)
{
    reliable184_token out = {RELIABLE184_NOT_RECOGNIZED,
                             RELIABLE184_COMMAND_UNKNOWN, RELIABLE184_REASON_NONE};
    const unsigned char *p = (const unsigned char *)packet;
    if (!p || bytes < 6 || p[0] != 255 || p[1] != 255 || p[2] != 255
        || p[3] != 255 || p[4] != 'R') return out;
    if (p[5] == 'A') {
        if (bytes < 9 || bytes > 11 || p[6] != ' ' || p[bytes - 1] != 0) return out;
        unsigned value = 0;
        for (size_t i = 7; i < bytes - 1; ++i) {
            if (p[i] < '0' || p[i] > '9') return out;
            value = value * 10u + (unsigned)(p[i] - '0');
        }
        /* Native snprintf("RA %i", unsigned-byte) emits canonical decimal. */
        if (value > 255 || (bytes > 9 && p[7] == '0')) return out;
        out.outer = RELIABLE184_RA;
        return out;
    }
    if (p[5] != 0 || bytes < 7) return out;
    out.outer = RELIABLE184_R;
    /* Offset6 is one sequence byte, including0 or255. Ignore its value. */
    size_t available = bytes - 7;
    if (available > 64) available = 64;
    size_t n = 0;
    while (n < available && p[7 + n]) ++n;
    if (n == available) return out;
    const unsigned char *text = p + 7;
    if (n >= 10 && !memcmp(text, "coopReject", 10)
        && (n == 10 || text[10] == ' ')) {
        out.first_command = RELIABLE184_REJECT;
        out.rejection = RELIABLE184_REASON_UNKNOWN;
        if (same(text, n, "coopReject COOP_WRONGSTATE")) out.rejection = RELIABLE184_WRONGSTATE;
        else if (same(text, n, "coopReject COOP_FULL")) out.rejection = RELIABLE184_FULL;
        else if (same(text, n, "coopReject COOP_WRONGPROTOCOL")) out.rejection = RELIABLE184_WRONGPROTOCOL;
        return out;
    }
    for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); ++i)
        if (same(text, n, allowed[i].text)) {
            out.first_command = allowed[i].value;
            break;
        }
    return out;
}
const char *reliable184_outer_name(reliable184_outer value)
{
    switch (value) {
    case RELIABLE184_R: return "R";
    case RELIABLE184_RA: return "RA";
    default: return "unknown";
    }
}
const char *reliable184_command_name(reliable184_command value)
{
    if (value == RELIABLE184_REJECT) return "coopReject";
    for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); ++i)
        if (allowed[i].value == value) return allowed[i].text;
    return "unknown";
}
const char *reliable184_reason_name(reliable184_reason value)
{
    switch (value) {
    case RELIABLE184_REASON_NONE: return "none";
    case RELIABLE184_WRONGSTATE: return "COOP_WRONGSTATE";
    case RELIABLE184_FULL: return "COOP_FULL";
    case RELIABLE184_WRONGPROTOCOL: return "COOP_WRONGPROTOCOL";
    default: return "unknown";
    }
}
