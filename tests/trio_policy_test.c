/* Own MIT offline tests. No Windows, game, process or network APIs. */
#include "trio_policy.h"
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; if (failures <= 8) printf("FAIL line %u: %s\n", (unsigned)__LINE__, #c); } } while (0)

static trio_member member(unsigned id, const char *name)
{
    trio_member m;
    memset(&m, 0, sizeof(m)); m.occupied = 1;
    m.uid[0] = (uint8_t)id; m.address[0] = 4; m.address[4] = (uint8_t)id;
    size_t n = strlen(name); if (n > 31) n = 31;
    memcpy(m.name, name, n);
    return m;
}

static uint32_t random_state = 0x184C0DEu;
static uint32_t random32(void)
{
    uint32_t x = random_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return random_state = x;
}

int main(void)
{
    trio_roster host, decoded, before;
    trio_member h = member(10, "host"), a = member(20, "peer1"), b = member(30, "peer2"), fourth = member(40, "fourth");
    uint8_t wire[512], ack[TRIO_ACK_BYTES], old_ack[TRIO_ACK_BYTES], canonical[512];
    unsigned recipient = 77;
    CHECK(trio_start(&host, 0x18400000001ULL, &h));
    CHECK(!trio_ready(&host));
    CHECK(trio_upsert(&host, &a) == 1);
    CHECK(host.revision == 2 && host.acknowledged == 1);
    CHECK(trio_encode_ack(&host, 1, old_ack, sizeof(old_ack)) == TRIO_ACK_BYTES);
    CHECK(trio_accept_ack(&host, 1, old_ack, sizeof(old_ack)));
    CHECK(host.acknowledged == 3 && !trio_ready(&host));
    CHECK(trio_upsert(&host, &b) == 2);
    CHECK(host.revision == 3 && host.acknowledged == 1);
    before = host;
    CHECK(trio_upsert(&host, &fourth) == -1);
    CHECK(!memcmp(&before, &host, sizeof(host)));
    CHECK(!trio_accept_ack(&host, 1, old_ack, sizeof(old_ack)));
    CHECK(host.acknowledged == 1);
    CHECK(trio_upsert(&host, &a) == 1 && host.revision == 3);
    CHECK(trio_upsert(&host, &h) == -1);
    size_t bytes = trio_encode_roster(&host, 2, wire, sizeof(wire));
    CHECK(bytes == TRIO_ROSTER_MAX);
    CHECK(trio_decode_roster(wire, bytes, &decoded, &recipient));
    CHECK(recipient == 2 && decoded.revision == 3 && decoded.generation == host.generation);
    for (unsigned i = 0; i < 3; ++i) CHECK(!memcmp(&host.members[i], &decoded.members[i], sizeof(trio_member)));
    CHECK(trio_encode_ack(&decoded, 2, ack, sizeof(ack)) == TRIO_ACK_BYTES);
    CHECK(!trio_accept_ack(&host, 1, ack, sizeof(ack)));
    CHECK(trio_accept_ack(&host, 2, ack, sizeof(ack)));
    CHECK(host.acknowledged == 5 && !trio_ready(&host));
    CHECK(trio_accept_ack(&host, 2, ack, sizeof(ack)) && !trio_ready(&host));
    CHECK(trio_encode_ack(&decoded, 1, ack, sizeof(ack)) == TRIO_ACK_BYTES);
    CHECK(trio_accept_ack(&host, 1, ack, sizeof(ack)) && trio_ready(&host));
    CHECK(trio_accept_ack(&host, 1, ack, sizeof(ack)) && trio_ready(&host));
    for (size_t n = 0; n <= TRIO_ACK_BYTES + 1; ++n) {
        before = host;
        CHECK(trio_accept_ack(&host, 1, ack, n) == (n == TRIO_ACK_BYTES));
        CHECK(!memcmp(&host, &before, sizeof(host)));
    }
    for (unsigned pos = 0; pos < TRIO_ACK_BYTES; ++pos) {
        uint8_t changed[TRIO_ACK_BYTES]; memcpy(changed, ack, sizeof(changed));
        for (unsigned value = 0; value < 256; ++value) {
            changed[pos] = (uint8_t)value; before = host;
            CHECK(trio_accept_ack(&host, 1, changed, sizeof(changed)) == (value == ack[pos]));
            CHECK(!memcmp(&host, &before, sizeof(host)));
        }
    }

    /* Every truncation and every extra byte is rejected without destination writes. */
    for (size_t n = 0; n < bytes; ++n) {
        memset(&decoded, 0xA5, sizeof(decoded)); before = decoded; recipient = 77;
        CHECK(!trio_decode_roster(wire, n, &decoded, &recipient));
        CHECK(!memcmp(&before, &decoded, sizeof(decoded)) && recipient == 77);
    }
    CHECK(!trio_decode_roster(wire, bytes + 1, &decoded, &recipient));
    /* Decoder input can share the destination object's storage. */
    union { trio_roster roster; uint8_t input[512]; } shared;
    memcpy(shared.input, wire, bytes); recipient = 99;
    CHECK(trio_decode_roster(shared.input, bytes, &shared.roster, &recipient));
    CHECK(recipient == 2 && !memcmp(&shared.roster.members[2], &host.members[2], sizeof(trio_member)));
    /* A structurally valid snapshot with a changed other peer cannot ACK host data. */
    CHECK(trio_decode_roster(wire, bytes, &decoded, &recipient));
    decoded.members[1].name[0] = 'X';
    CHECK(trio_encode_ack(&decoded, 2, old_ack, sizeof(old_ack)) == TRIO_ACK_BYTES);
    before = host;
    CHECK(!trio_accept_ack(&host, 2, old_ack, sizeof(old_ack)));
    CHECK(!memcmp(&before, &host, sizeof(host)));
    CHECK(trio_reconnect(&host, &a) == 1 && host.revision == 4 && host.acknowledged == 1);
    CHECK(!trio_accept_ack(&host, 1, ack, sizeof(ack)) && !trio_ready(&host));
    for (unsigned count = 0; count < 256; ++count) {
        uint8_t copy[512]; memcpy(copy, wire, bytes); copy[10] = (uint8_t)count;
        CHECK(trio_decode_roster(copy, bytes, &decoded, &recipient) == (count == 3));
    }
    for (unsigned slot = 0; slot < 256; ++slot) {
        uint8_t copy[512]; memcpy(copy, wire, bytes); copy[11] = (uint8_t)slot;
        CHECK(trio_decode_roster(copy, bytes, &decoded, &recipient) == (slot == 1 || slot == 2));
    }
    uint8_t bad[512]; memcpy(bad, wire, bytes);
    memcpy(bad, "coopAcce", 8); CHECK(!trio_decode_roster(bad, bytes, &decoded, &recipient));
    memcpy(bad, wire, bytes); bad[8] = 2; CHECK(!trio_decode_roster(bad, bytes, &decoded, &recipient));
    memcpy(bad, wire, bytes); bad[24 + 61] = 0; CHECK(!trio_decode_roster(bad, bytes, &decoded, &recipient));
    memcpy(bad, wire, bytes); memcpy(bad + 24 + 61 + 1, bad + 25, 8); CHECK(!trio_decode_roster(bad, bytes, &decoded, &recipient));
    memcpy(bad, wire, bytes); memset(bad + 24 + 1 + 8 + 20, 'x', 32); CHECK(!trio_decode_roster(bad, bytes, &decoded, &recipient));
    memcpy(bad, wire, bytes); bad[24 + 1 + 8 + 20 + 31] = 'x'; CHECK(!trio_decode_roster(bad, bytes, &decoded, &recipient));
    memcpy(bad, wire, bytes); memset(bad + 12, 0, 8); CHECK(!trio_decode_roster(bad, bytes, &decoded, &recipient));
    memcpy(bad, wire, bytes); memset(bad + 20, 0, 4); CHECK(!trio_decode_roster(bad, bytes, &decoded, &recipient));

    /* Reconnect updates only its stable slot, invalidates old generation/revision ACKs. */
    a.address[4] = 99;
    CHECK(trio_upsert(&host, &a) == 1 && host.revision == 5 && host.acknowledged == 1);
    CHECK(!trio_accept_ack(&host, 1, ack, sizeof(ack)) && !trio_ready(&host));
    CHECK(trio_remove(&host, 1) && host.revision == 6 && host.acknowledged == 1);
    CHECK(!trio_remove(&host, 0) && !trio_remove(&host, 3));
    CHECK(trio_upsert(&host, &fourth) == 1);
    CHECK(trio_encode_ack(&host, 1, ack, sizeof(ack)) == TRIO_ACK_BYTES);
    trio_roster restarted;
    CHECK(trio_start(&restarted, host.generation + 1, &h));
    CHECK(trio_upsert(&restarted, &fourth) == 1);
    CHECK(!trio_accept_ack(&restarted, 1, ack, sizeof(ack)));
    host.revision = UINT32_MAX; before = host;
    fourth.address[4] = 11;
    CHECK(trio_upsert(&host, &fourth) == -2 && !memcmp(&host, &before, sizeof(host)));
    CHECK(trio_remove(&host, 2) == -2 && !memcmp(&host, &before, sizeof(host)));
    CHECK(trio_reconnect(&host, &fourth) == -2 && !memcmp(&host, &before, sizeof(host)));
    CHECK(trio_reconnect(&host, &h) == -1 && !memcmp(&host, &before, sizeof(host)));
    CHECK(!trio_start(&host, 0, &h));
    trio_member invalid = member(50, "invalid"); memset(invalid.name, 'a', 32);
    CHECK(!trio_start(&host, 1, &invalid));
    CHECK(trio_upsert(&host, &invalid) == -1);
    CHECK(!trio_encode_roster(&host, 0, wire, sizeof(wire)));
    CHECK(!trio_encode_roster(&host, 2, wire, TRIO_ROSTER_MAX - 1));
    CHECK(!trio_encode_ack(&host, 0, ack, sizeof(ack)));
    CHECK(!trio_encode_ack(&host, 2, ack, sizeof(ack) - 1));
    CHECK(!trio_accept_ack(&host, 3, ack, sizeof(ack)));
    CHECK(!trio_accept_ack(&host, 2, ack, sizeof(ack) - 1));

    /* Deterministic mutation/length fuzzing of the real decoder and ACK policy. */
    CHECK(trio_start(&host, 77, &h)); CHECK(trio_upsert(&host, &a) == 1); CHECK(trio_upsert(&host, &b) == 2);
    bytes = trio_encode_roster(&host, 2, wire, sizeof(wire));
    unsigned accepted = 0, rejected = 0;
    for (unsigned trial = 0; trial < 200000; ++trial) {
        memcpy(bad, wire, bytes);
        size_t n = bytes;
        unsigned mode = random32() % 4;
        if (mode == 0) n = random32() % sizeof(bad);
        if (n > bytes) memset(bad + bytes, 0xCC, n - bytes);
        unsigned mutations = 1 + random32() % 5;
        while (mutations--) bad[random32() % sizeof(bad)] ^= (uint8_t)(1 + random32() % 255);
        memset(&decoded, 0x5A, sizeof(decoded)); before = decoded; recipient = 99;
        if (trio_decode_roster(bad, n, &decoded, &recipient)) {
            ++accepted;
            size_t produced = trio_encode_roster(&decoded, recipient, canonical, sizeof(canonical));
            CHECK(produced == n && !memcmp(bad, canonical, n));
        } else {
            ++rejected;
            CHECK(!memcmp(&before, &decoded, sizeof(decoded)) && recipient == 99);
        }
        trio_roster prior = host;
        size_t an = random32() % (TRIO_ACK_BYTES + 8);
        CHECK(!trio_accept_ack(&host, random32() % 5, bad, an));
        CHECK(!memcmp(&host, &prior, sizeof(host)));
    }
    printf("checks=%u failures=%u mutation_cases=200000 accepted=%u rejected=%u\n", checks, failures, accepted, rejected);
    return failures ? 1 : 0;
}
