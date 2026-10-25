/* Own MIT code. Private, versioned roster/readiness candidate; no engine hooks. */
#include "trio_policy.h"
#include "../src/pure_sha256.h"
#include <string.h>

static const uint8_t magic[8] = {'M','W','2','T','R','I','O',0};

static int member_valid(const trio_member *m)
{
    size_t end = 0;
    if (!m || m->occupied != 1) return 0;
    while (end < sizeof(m->name) && m->name[end]) {
        unsigned char c = (unsigned char)m->name[end];
        if (c < 32 || c == 127) return 0;
        ++end;
    }
    return end < sizeof(m->name);
}

static void member_copy(trio_member *dst, const trio_member *src)
{
    memset(dst, 0, sizeof(*dst));
    dst->occupied = 1;
    memcpy(dst->uid, src->uid, sizeof(dst->uid));
    memcpy(dst->address, src->address, sizeof(dst->address));
    size_t len = 0;
    while (len < 31 && src->name[len]) ++len;
    memcpy(dst->name, src->name, len);
}

static uint8_t occupied(const trio_roster *r)
{
    uint8_t bits = 0;
    for (unsigned i = 0; i < TRIO_MEMBERS; ++i)
        if (r->members[i].occupied) bits |= (uint8_t)(1u << i);
    return bits;
}

static int valid(const trio_roster *r)
{
    if (!r || !r->generation || !r->revision || r->members[0].occupied != 1) return 0;
    for (unsigned i = 0; i < TRIO_MEMBERS; ++i) {
        if (!r->members[i].occupied) continue;
        if (!member_valid(&r->members[i])) return 0;
        for (unsigned j = 0; j < i; ++j)
            if (r->members[j].occupied && !memcmp(r->members[i].uid, r->members[j].uid, 8)) return 0;
    }
    return (r->acknowledged & (uint8_t)~occupied(r)) == 0;
}

static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (i * 8));
}
static uint32_t get32(const uint8_t *p)
{
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i) v |= (uint32_t)p[i] << (i * 8);
    return v;
}
static void put64(uint8_t *p, uint64_t v)
{
    put32(p, (uint32_t)v); put32(p + 4, (uint32_t)(v >> 32));
}
static uint64_t get64(const uint8_t *p)
{
    return get32(p) | ((uint64_t)get32(p + 4) << 32);
}
static void header(uint8_t *p, unsigned kind, unsigned count, unsigned recipient, const trio_roster *r)
{
    memcpy(p, magic, 8); p[8] = 1; p[9] = (uint8_t)kind;
    p[10] = (uint8_t)count; p[11] = (uint8_t)recipient;
    put64(p + 12, r->generation); put32(p + 20, r->revision);
}

int trio_start(trio_roster *r, uint64_t generation, const trio_member *host)
{
    if (!r || !generation || !member_valid(host)) return 0;
    trio_roster next;
    memset(&next, 0, sizeof(next));
    next.generation = generation; next.revision = 1; next.acknowledged = 1;
    member_copy(&next.members[0], host);
    *r = next;
    return 1;
}

int trio_upsert(trio_roster *r, const trio_member *peer)
{
    if (!valid(r) || !member_valid(peer)) return -1;
    unsigned slot = TRIO_MEMBERS;
    for (unsigned i = 0; i < TRIO_MEMBERS; ++i)
        if (r->members[i].occupied && !memcmp(r->members[i].uid, peer->uid, 8)) { slot = i; break; }
    if (slot == 0) return -1;
    if (slot == TRIO_MEMBERS)
        for (unsigned i = 1; i < TRIO_MEMBERS; ++i)
            if (!r->members[i].occupied) { slot = i; break; }
    if (slot == TRIO_MEMBERS) return -1;
    trio_member canonical;
    member_copy(&canonical, peer);
    if (r->members[slot].occupied
        && !memcmp(r->members[slot].uid, canonical.uid, 8)
        && !memcmp(r->members[slot].address, canonical.address, 20)
        && !memcmp(r->members[slot].name, canonical.name, 32)) return (int)slot;
    if (r->revision == UINT32_MAX) return -2;
    r->members[slot] = canonical;
    ++r->revision; r->acknowledged = 1;
    return (int)slot;
}

int trio_remove(trio_roster *r, unsigned slot)
{
    if (!valid(r) || slot == 0 || slot >= TRIO_MEMBERS || !r->members[slot].occupied) return 0;
    if (r->revision == UINT32_MAX) return -2;
    memset(&r->members[slot], 0, sizeof(r->members[slot]));
    ++r->revision; r->acknowledged = 1;
    return 1;
}

int trio_reconnect(trio_roster *r, const trio_member *peer)
{
    if (!valid(r) || !member_valid(peer)) return -1;
    unsigned slot = TRIO_MEMBERS;
    for (unsigned i = 1; i < TRIO_MEMBERS; ++i)
        if (r->members[i].occupied && !memcmp(r->members[i].uid, peer->uid, 8)) { slot = i; break; }
    if (slot == TRIO_MEMBERS) return -1;
    if (r->revision == UINT32_MAX) return -2;
    trio_member canonical;
    member_copy(&canonical, peer);
    r->members[slot] = canonical; ++r->revision; r->acknowledged = 1;
    return (int)slot;
}

int trio_ready(const trio_roster *r)
{
    return valid(r) && occupied(r) == 7 && r->acknowledged == 7;
}

size_t trio_encode_roster(const trio_roster *r, unsigned recipient, uint8_t *out, size_t cap)
{
    if (!valid(r) || recipient == 0 || recipient >= TRIO_MEMBERS || !r->members[recipient].occupied || !out) return 0;
    unsigned count = 0;
    for (unsigned i = 0; i < TRIO_MEMBERS; ++i) count += r->members[i].occupied != 0;
    size_t bytes = 24 + (size_t)count * 61;
    if (cap < bytes) return 0;
    uint8_t serialized[TRIO_ROSTER_MAX];
    header(serialized, 1, count, recipient, r);
    size_t pos = 24;
    for (unsigned i = 0; i < TRIO_MEMBERS; ++i) {
        const trio_member *m = &r->members[i];
        if (!m->occupied) continue;
        serialized[pos++] = (uint8_t)i;
        memcpy(serialized + pos, m->uid, 8); pos += 8;
        memcpy(serialized + pos, m->address, 20); pos += 20;
        char name[32] = {0};
        size_t len = 0;
        while (len < 31 && m->name[len]) ++len;
        memcpy(name, m->name, len);
        memcpy(serialized + pos, name, 32); pos += 32;
    }
    memcpy(out, serialized, bytes);
    return bytes;
}

int trio_decode_roster(const uint8_t *wire, size_t bytes, trio_roster *out, unsigned *recipient)
{
    if (!wire || !out || !recipient || bytes < 24 || memcmp(wire, magic, 8)
        || wire[8] != 1 || wire[9] != 1 || wire[10] < 2 || wire[10] > 3
        || wire[11] == 0 || wire[11] >= TRIO_MEMBERS || bytes != 24 + (size_t)wire[10] * 61) return 0;
    trio_roster next;
    memset(&next, 0, sizeof(next));
    next.generation = get64(wire + 12); next.revision = get32(wire + 20);
    size_t pos = 24;
    unsigned previous = 0;
    for (unsigned n = 0; n < wire[10]; ++n) {
        unsigned slot = wire[pos++];
        if (slot >= TRIO_MEMBERS || (n == 0 ? slot != 0 : slot <= previous)) return 0;
        previous = slot;
        trio_member *m = &next.members[slot]; m->occupied = 1;
        memcpy(m->uid, wire + pos, 8); pos += 8;
        memcpy(m->address, wire + pos, 20); pos += 20;
        memcpy(m->name, wire + pos, 32); pos += 32;
        if (!member_valid(m)) return 0;
        size_t end = 0;
        while (m->name[end]) ++end;
        for (; end < 32; ++end) if (m->name[end]) return 0;
    }
    unsigned local = wire[11];
    if (!valid(&next) || !next.members[local].occupied) return 0;
    next.acknowledged = 1;
    *out = next; *recipient = local;
    return 1;
}

size_t trio_encode_ack(const trio_roster *r, unsigned local, uint8_t *out, size_t cap)
{
    if (!valid(r) || local == 0 || local >= TRIO_MEMBERS || !r->members[local].occupied || !out || cap < TRIO_ACK_BYTES) return 0;
    uint8_t snapshot[TRIO_ROSTER_MAX], serialized[TRIO_ACK_BYTES];
    size_t bytes = trio_encode_roster(r, local, snapshot, sizeof(snapshot));
    if (!bytes) return 0;
    header(serialized, 2, 0, local, r);
    memcpy(serialized + 24, r->members[local].uid, 8);
    pure_sha256_context hash;
    pure_sha256_init(&hash); pure_sha256_update(&hash, snapshot, bytes);
    pure_sha256_final(&hash, serialized + 32);
    memcpy(out, serialized, sizeof(serialized));
    return TRIO_ACK_BYTES;
}

int trio_accept_ack(trio_roster *r, unsigned known_sender, const uint8_t *wire, size_t bytes)
{
    if (!valid(r) || !wire || bytes != TRIO_ACK_BYTES || known_sender == 0 || known_sender >= TRIO_MEMBERS
        || !r->members[known_sender].occupied || memcmp(wire, magic, 8) || wire[8] != 1 || wire[9] != 2
        || wire[10] != 0 || wire[11] != known_sender || get64(wire + 12) != r->generation
        || get32(wire + 20) != r->revision || memcmp(wire + 24, r->members[known_sender].uid, 8)) return 0;
    uint8_t snapshot[TRIO_ROSTER_MAX], digest[32];
    size_t count = trio_encode_roster(r, known_sender, snapshot, sizeof(snapshot));
    if (!count) return 0;
    pure_sha256_context hash;
    pure_sha256_init(&hash); pure_sha256_update(&hash, snapshot, count);
    pure_sha256_final(&hash, digest);
    if (memcmp(digest, wire + 32, 32)) return 0;
    r->acknowledged |= (uint8_t)(1u << known_sender);
    return 1;
}
