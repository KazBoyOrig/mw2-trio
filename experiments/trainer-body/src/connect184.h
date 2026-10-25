/* Own experimental code; MIT. No command registration or hook installation. */
#pragma once
#include <windows.h>

/* Invoke from a registered main-thread command AFTER mw2trio_party_join.
 * Numeric IPv4 only. Ordinary mode accepts loopback/RFC1918/CGNAT space;
 * explicit peer mode instead requires the exact configured eligible endpoint
 * and client role. No whole-prefix permission is implied by peer eligibility.
 * port is normal host-order1..65535; name is printable ASCII1..31 bytes.
 * difficulty0..3 follows the native coopConnect byte (normally current dvar).
 * Requires ordinary mw2-trio-party-test.enable next to the executable.
 * Returns1 for native reliable enqueue only; this is NOT a handshake ACK.
 * Negative values: -1 input/thread, -2 disabled, -3 image/signature,
 * -4 ownership/preconditions, -5 zero UID, -6 native queue/packet failure.
 * Any failure after native state changes requires a fresh process.
 */
int connect184_send(DWORD verified_main_thread_id, const char *numeric_ipv4,
                    unsigned port, const char *display_name, unsigned difficulty);
