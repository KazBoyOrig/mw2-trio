/* Own inactive build184 LAN candidate; MIT. */
#pragma once
#include <stdint.h>
#include <windows.h>
typedef struct netopen184_report {
    uint32_t version, attempted, opened, last_result;
    uint32_t startup_result, socket_present, bound_port, configured_state;
} netopen184_report;

/* Explicit main-thread command, before party_join/lan_host/connect. Fresh menu
 * only, exact184 + ordinary party opt-in. Calls own WSAStartup(1.1), then full
 * native OpenIP; no custom transport and no online/auth/identity changes.
 * Native OpenIP enumerates the local hostname/interfaces; SOCKS must be off.
 * Returned1 means native UDP socket opened, not remote connectivity.
 * Call once per process; WSA reference remains owned until process exit.
 * No production registration/hook installation is included.
 */
int netopen184_open(DWORD main_thread_id, netopen184_report *out, uint32_t bytes);
int netopen184_qa_ready(void);
int netopen184_peer_ready(void);
int netopen184_peer_observe(void);
