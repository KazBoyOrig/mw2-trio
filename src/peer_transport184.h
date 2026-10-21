/* Own MIT opt-in adapter. File/interface/socket-metadata reads only. */
#ifndef MW2_PEER_TRANSPORT184_H
#define MW2_PEER_TRANSPORT184_H
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include "peer_scope184.h"

#define PEER_TRANSPORT184_VERSION 1u
#define PEER_TRANSPORT184_ENABLE L"mw2-trio-peer-test.enable"
#define PEER_TRANSPORT184_CONFIG L"mw2-trio-peer.cfg"
enum peer_transport184_result {
    PEER_TRANSPORT184_OK=1, PEER_TRANSPORT184_DISABLED=0,
    PEER_TRANSPORT184_ARGUMENT=-1, PEER_TRANSPORT184_THREAD=-2,
    PEER_TRANSPORT184_CONFLICT=-3, PEER_TRANSPORT184_FILE=-4,
    PEER_TRANSPORT184_CONFIG_INVALID=-5, PEER_TRANSPORT184_INTERFACE=-6,
    PEER_TRANSPORT184_NOT_READY=-7, PEER_TRANSPORT184_SCOPE_CHANGED=-8,
    PEER_TRANSPORT184_BINDING=-9, PEER_TRANSPORT184_ONCE=-10,
    PEER_TRANSPORT184_CONTEXT=-11
};
typedef struct peer_transport184_report {
    uint32_t version,bytes,attempted,ready,failed,role;
    int32_t result;
    uint32_t observations,interface_checks,socket_checks,win32_error;
} peer_transport184_report;

/* Fixed ordinary files next to the EXE; no account/path configuration.
 * requested: read-only selector, 1 explicit peer opt-in,0 absent,negative invalid
 * file/QA conflict. initialize: one owner-thread attempt, outside DllMain,
 * validates/latches file+interface; never opens/configures a socket or engine.
 * Existing fixed127002 QA and peer mode must not coexist. Parent also enforces
 * its own control/network/party flags, exact build, online0/platform-lobby0.
 */
int peer_transport184_requested(void);
int peer_transport184_initialize(DWORD owner_tid);
int peer_transport184_ready(void);
/* Every owner main-thread callback before optional work. Re-reads ordinary
 * flag/config, exact bytes+identity+mtime, and current adapter ownership/prefix.
 * Failure is sticky. GetAdaptersAddresses has no hard realtime latency bound;
 * bounded same-PC runs passed, but no general frame-budget bound is proven.
 */
int peer_transport184_observe(void);
/* Immutable sealed snapshot, copy-out only; never return a mutable static ptr. */
int peer_transport184_get_config(peer_scope184_config *out);
int peer_transport184_identity_low(uint32_t *out);
/* Call after native open, then EVERY frame with parent's actual native socket.
 * First exact successful metadata probe latches this socket handle. Subsequent
 * handle/bind drift is fatal; no socket creation, native globals or WSAStartup.
 */
int peer_transport184_socket_matches(SOCKET native_game_socket);
/* Exact directional tuple only; caller supplies readable AF_INET sockaddr
 * spans and converts a failed/mismatched result into its boundary policy.
 * Backend denial MUST run first; no total provider/process egress guarantee.
 */
int peer_transport184_tuple_matches(unsigned direction,const struct sockaddr *source,int source_len,
                                   const struct sockaddr *destination,int destination_len);
void peer_transport184_get_report(peer_transport184_report *out);
#endif
