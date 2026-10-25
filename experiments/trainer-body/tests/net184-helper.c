/* Own helper: fake socket callees only, no socket or game is opened. */
#include "../src/net184.c"
#include <stdlib.h>

/* Inert transport double. This helper never enumerates adapters or opens I/O. */
int peer_transport184_requested(void){return 0;}
int peer_transport184_initialize(DWORD tid){(void)tid;return -1;}
int peer_transport184_ready(void){return 0;}
int peer_transport184_observe(void){return -1;}
int peer_transport184_get_config(peer_scope184_config *c){(void)c;return -1;}
int peer_transport184_socket_matches(SOCKET s){(void)s;return -1;}
int peer_transport184_tuple_matches(unsigned d,const struct sockaddr *a,int an,const struct sockaddr *b,int bn)
{(void)d;(void)a;(void)an;(void)b;(void)bn;return 0;}

int __cdecl bootstrap_guard_current(bootstrap_report *r, uint32_t bytes)
{ (void)r; (void)bytes; return 0; }

static int calls, failures;
static SOCKET expected_socket;
static const char *expected_buffer;
static const struct sockaddr *expected_address;
static int expected_length, expected_flags, expected_address_len;
static int fake_recv_result = 8;

static void check(int ok, const char *label)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
}

static int WSAAPI fake_send(SOCKET s, const char *buf, int len, int flags,
                          const struct sockaddr *address, int address_len)
{
    ++calls;
    if (s != expected_socket || buf != expected_buffer || len != expected_length
        || flags != expected_flags || address != expected_address || address_len != expected_address_len)
        ++failures;
    WSASetLastError(WSAEWOULDBLOCK);
    return 9;
}

static int WSAAPI fake_recv(SOCKET s, char *buf, int len, int flags,
                          struct sockaddr *address, int *address_len)
{
    ++calls;
    if (s != expected_socket || len != 64 || flags != expected_flags
        || address != expected_address || !address_len || *address_len != expected_address_len) ++failures;
    memcpy(buf, "\xff\xff\xff\xffhba\0", 8);
    WSASetLastError(WSAECONNRESET);
    return fake_recv_result;
}

int main(void)
{
    lstrcpyW(log_path, L"network-helper.log");
    DeleteFileW(log_path);
    original_send = fake_send; original_recv = fake_recv;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address)); address.sin_family = AF_INET;
    unsigned char *ip = (unsigned char *)&address.sin_addr;
    ip[0] = 127; ip[3] = 1;
    unsigned char *port = (unsigned char *)&address.sin_port; port[0] = 32; port[1] = 113;
    const char packet_data[] = "\xff\xff\xff\xff" "coopConnect\0private payload never logged";
    expected_socket = 123; expected_buffer = packet_data; expected_length = sizeof(packet_data);
    expected_flags = 7; expected_address = (const struct sockaddr *)&address;
    expected_address_len = sizeof(address);
    int result = traced_send(expected_socket, packet_data, expected_length, expected_flags,
                             expected_address, expected_address_len);
    check(result == 9 && WSAGetLastError() == WSAEWOULDBLOCK && calls == 1, "send preserves arguments result and socket error");
    char data[64] = {0}; int address_len = sizeof(address);
    result = traced_recv(expected_socket, data, sizeof(data), expected_flags,
                         (struct sockaddr *)&address, &address_len);
    check(result == 8 && WSAGetLastError() == WSAECONNRESET && calls == 2
          && !memcmp(data, "\xff\xff\xff\xffhba\0", 8), "receive preserves bytes result and socket error");
    FILE *f = fopen("network-helper.log", "rb"); char text[2048] = {0};
    size_t size = f ? fread(text, 1, sizeof(text) - 1, f) : 0; if (f) fclose(f);
    check(size && strstr(text, "send command=coopConnect") && strstr(text, "recv command=hba")
          && strstr(text, "peer=127.0.0.1:8305") && !strstr(text, "private payload"), "tokens only no payload data");
    LONG before = budget;
    packet("send", "service", 7, 7, NULL, 0);
    packet("send", "\xff\xff\xff\xffunknown\0", 12, 12, NULL, 0);
    packet("send", packet_data, 4, 4, NULL, 0);
    check(budget == before, "non-coop and short packets ignored");
    budget = 0;
    packet("send", packet_data, sizeof(packet_data), 9, NULL, 0);
    f = fopen("network-helper.log", "rb"); if (f) { fseek(f, 0, SEEK_END); result = (int)ftell(f); fclose(f); } else result = -1;
    check(result == (int)size, "logging budget bounded");
    loopback_qa = 1; state = 1;
    ip[3] = 2;
    IMAGE_THUNK_DATA32 own_slots[2];memset(own_slots,0,sizeof(own_slots));
    own_slots[0].u1.Function=(uintptr_t)traced_send;own_slots[1].u1.Function=(uintptr_t)traced_recv;
    installed_slots[0]=&own_slots[0];installed_slots[1]=&own_slots[1];
    check(net184_loopback_ready(), "QA boundary readiness requires installed latched mode");
    result = traced_send(expected_socket, packet_data, expected_length, expected_flags,
                         expected_address, expected_address_len);
    check(result == 9 && calls == 3 && WSAGetLastError() == WSAEWOULDBLOCK,
          "QA exact loopback preserves original send");
    ip[0] = 10;
    result = traced_send(expected_socket, packet_data, expected_length, expected_flags,
                         expected_address, expected_address_len);
    check(result == SOCKET_ERROR && calls == 3 && WSAGetLastError() == WSAEACCES
          && !net184_loopback_ready(), "QA nonloopback send blocked before original callee");
    qa_boundary_failed = 0; ip[0] = 127; ip[3] = 1;
    result = traced_send(expected_socket, packet_data, expected_length, expected_flags,
                         expected_address, expected_address_len);
    check(result == SOCKET_ERROR && calls == 3 && qa_boundary_failed,
          "QA other loopback address refused");
    qa_boundary_failed=0;ip[0]=10;
    result=traced_send(expected_socket,"unknown service",15,expected_flags,expected_address,expected_address_len);
    check(result==SOCKET_ERROR&&calls==3&&WSAGetLastError()==WSAEACCES&&qa_boundary_failed
          &&qa_refused_external_ip==0,"QA unknown external send refused and fatal before original callee");
    qa_boundary_failed=0;
    refuse_qa_send(SERVICE184_EXTERNAL_IP_TYPE1,expected_address,expected_address_len);
    check(calls==3&&!qa_boundary_failed&&qa_refused_external_ip==1,
          "QA positively classified blocked-service abort-policy fixture; no native context claim");
    ip[0]=127;
    qa_boundary_failed = 0; ip[3] = 2;
    result = traced_send(expected_socket, packet_data, expected_length, expected_flags, NULL, 0);
    check(result == SOCKET_ERROR && calls == 3 && qa_boundary_failed, "QA unknown send address refused");
    qa_boundary_failed = 0;
    result = traced_recv(expected_socket, data, sizeof(data), expected_flags, NULL, NULL);
    check(result == SOCKET_ERROR && calls == 3 && qa_boundary_failed, "QA missing receive address refused before callee");
    qa_boundary_failed = 0; address_len = sizeof(address); ip[0] = 10;
    result = traced_recv(expected_socket, data, sizeof(data), expected_flags,
                         (struct sockaddr *)&address, &address_len);
    check(result == SOCKET_ERROR && calls == 4 && WSAGetLastError() == WSAEACCES
          && qa_boundary_failed, "QA nonloopback receive withheld from game");
    qa_boundary_failed = 0; ip[0] = 127;
    result = traced_recv(expected_socket, data, sizeof(data), expected_flags,
                         (struct sockaddr *)&address, &address_len);
    check(result == 8 && calls == 5 && WSAGetLastError() == WSAECONNRESET
          && !qa_boundary_failed, "QA exact loopback receive preserves original bytes and result");
    fake_recv_result=0;ip[0]=10;
    result=traced_recv(expected_socket,data,sizeof(data),expected_flags,(struct sockaddr *)&address,&address_len);
    check(result==SOCKET_ERROR&&calls==6&&WSAGetLastError()==WSAEACCES&&qa_boundary_failed,
          "QA nonloopback empty datagram withheld and fatal");
    qa_boundary_failed=0;ip[0]=127;
    result=traced_recv(expected_socket,data,sizeof(data),expected_flags,(struct sockaddr *)&address,&address_len);
    check(result==0&&calls==7&&WSAGetLastError()==WSAECONNRESET&&!qa_boundary_failed,
          "QA exact loopback empty datagram preserves result");
    own_slots[0].u1.Function=0;
    check(!net184_loopback_ready(),"QA changed installed import refuses readiness");
    own_slots[0].u1.Function=(uintptr_t)traced_send;
    check(net184_tick() < 0, "QA missing opt-in fatal independently of telemetry");
    loopback_qa = 0;
    direct_peer=1;qa_boundary_failed=0;
    result=traced_send(expected_socket,packet_data,expected_length,expected_flags,expected_address,expected_address_len);
    check(result==SOCKET_ERROR&&calls==7&&qa_boundary_failed&&WSAGetLastError()==WSAEACCES,
          "private peer missing ready scope refuses before original socket send");
    qa_boundary_failed=0;address_len=sizeof(address);
    result=traced_recv(expected_socket,data,sizeof(data),expected_flags,(struct sockaddr *)&address,&address_len);
    check(result==SOCKET_ERROR&&calls==7&&qa_boundary_failed&&WSAGetLastError()==WSAEACCES,
          "private peer missing ready scope refuses before original socket receive");
    direct_peer=0;
    state = -1;
    check(net184_tick() == 0, "refused installation remains dormant");
    peer_install_requested=1;
    check(net184_tick()<0,"explicit private-peer installation refusal remains fatal");
    DeleteFileW(log_path);
    return failures ? 1 : 0;
}
