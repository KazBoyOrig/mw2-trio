/* Own MIT code. Offline candidate policy, not integrated into the game. */
#ifndef TRIO_POLICY_H
#define TRIO_POLICY_H
#include <stddef.h>
#include <stdint.h>

#define TRIO_MEMBERS 3u
#define TRIO_ROSTER_MAX 207u
#define TRIO_ACK_BYTES 64u

typedef struct trio_member {
    uint8_t occupied;
    uint8_t uid[8];       /* Opaque native identity; never fabricated by policy. */
    uint8_t address[20];  /* Opaque native netadr representation. */
    char name[32];
} trio_member;

typedef struct trio_roster {
    uint64_t generation;
    uint32_t revision;
    trio_member members[TRIO_MEMBERS];
    uint8_t acknowledged;
} trio_roster;

int trio_start(trio_roster *r, uint64_t generation, const trio_member *host);
/* Returns stable slot 1/2, -1 full/invalid, -2 revision exhausted. */
int trio_upsert(trio_roster *r, const trio_member *peer);
/* On a genuinely new native connection incarnation, even for the same UID,
   address and name. Retransmissions use upsert; adapters must distinguish them. */
int trio_reconnect(trio_roster *r, const trio_member *peer);
int trio_remove(trio_roster *r, unsigned slot);
int trio_ready(const trio_roster *r); /* Exactly 3 members, same generation/revision. */
size_t trio_encode_roster(const trio_roster *r, unsigned recipient, uint8_t *out, size_t cap);
/* Failures leave destination and recipient unchanged. Input may alias out;
   recipient must be separate from out. No network side effects. This decoder
   performs structural validation; native apply must bind expected host/session
   and reject stale/replayed revisions before updating client game state. */
int trio_decode_roster(const uint8_t *wire, size_t bytes, trio_roster *out, unsigned *recipient);
size_t trio_encode_ack(const trio_roster *r, unsigned local, uint8_t *out, size_t cap);
/* Native adapter must first bind packet to an existing known sender slot+UID.
   ACK includes SHA-256 of the complete canonical recipient snapshot. This
   policy provides freshness/content readiness only, not authentication. */
int trio_accept_ack(trio_roster *r, unsigned known_sender, const uint8_t *wire, size_t bytes);
#endif
