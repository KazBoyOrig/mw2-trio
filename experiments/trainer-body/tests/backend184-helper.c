/* Original pure fixture test; MIT. No game or network dependencies. */
#include <stdio.h>
#include <string.h>
#include "backend184.h"
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"failure line%u\n",(unsigned)__LINE__); return 1; } } while (0)
static backend184_context fixture(void)
{
    backend184_context c;
    memset(&c,0,sizeof(c));c.native_return_address=BACKEND184_RETURN_VA;
    c.packet_bytes=120;c.sending_socket=42;c.native_backend_socket=42;
    c.native_address_type=4;c.native_port_host_order=3005;
    c.native_ipv4[0]=192;c.native_ipv4[2]=2;c.native_ipv4[3]=1;
    c.function_sha256_verified=1;return c;
}
int main(void)
{
    const service184_endpoint destination={2,{192,0,2,1},3005};
    backend184_context c=fixture(),copy=c;
    CHECK(backend184_classify_blocked(&c,&destination)==BACKEND184_IWNET_3005);
    CHECK(!memcmp(&c,&copy,sizeof(c)));
    CHECK(backend184_classify_blocked(NULL,&destination)==BACKEND184_UNKNOWN);
    CHECK(backend184_classify_blocked(&c,NULL)==BACKEND184_UNKNOWN);
    for(size_t bytes=0;bytes<=4100;++bytes) {
        c=fixture();c.packet_bytes=bytes;
        CHECK(backend184_classify_blocked(&c,&destination)==
              (bytes>=1&&bytes<=4096?BACKEND184_IWNET_3005:BACKEND184_UNKNOWN));
    }
    c=fixture();c.packet_bytes=SIZE_MAX;
    CHECK(backend184_classify_blocked(&c,&destination)==BACKEND184_UNKNOWN);
    for(unsigned i=0;i<256;++i) {
        c=fixture();c.function_sha256_verified=i;
        CHECK(backend184_classify_blocked(&c,&destination)==(i==1?BACKEND184_IWNET_3005:BACKEND184_UNKNOWN));
        c=fixture();c.known_coop_token=i;
        CHECK(backend184_classify_blocked(&c,&destination)==(i==0?BACKEND184_IWNET_3005:BACKEND184_UNKNOWN));
        c=fixture();c.native_address_type=i;
        CHECK(backend184_classify_blocked(&c,&destination)==(i==4?BACKEND184_IWNET_3005:BACKEND184_UNKNOWN));
    }
    c=fixture();c.native_return_address=0x0046177cu;
    CHECK(backend184_classify_blocked(&c,&destination)==BACKEND184_UNKNOWN);
    c=fixture();c.native_return_address--;
    CHECK(backend184_classify_blocked(&c,&destination)==BACKEND184_UNKNOWN);
    c=fixture();c.sending_socket++;
    CHECK(backend184_classify_blocked(&c,&destination)==BACKEND184_UNKNOWN);
    c=fixture();c.sending_socket=c.native_backend_socket=0;
    CHECK(backend184_classify_blocked(&c,&destination)==BACKEND184_UNKNOWN);
    c=fixture();c.sending_socket=c.native_backend_socket=UINT32_MAX;
    CHECK(backend184_classify_blocked(&c,&destination)==BACKEND184_UNKNOWN);
    c=fixture();c.native_port_host_order=0xbd0bu;
    CHECK(backend184_classify_blocked(&c,&destination)==BACKEND184_UNKNOWN);
    for(unsigned i=0;i<4;++i) {
        c=fixture();c.native_ipv4[i]^=1;
        CHECK(backend184_classify_blocked(&c,&destination)==BACKEND184_UNKNOWN);
    }
    for(unsigned i=0;i<65536;++i) {
        service184_endpoint d=destination;d.port_host_order=i;c=fixture();
        CHECK(backend184_classify_blocked(&c,&d)==(i==3005?BACKEND184_IWNET_3005:BACKEND184_UNKNOWN));
        c.native_port_host_order=i;
        CHECK(backend184_classify_blocked(&c,&destination)==(i==3005?BACKEND184_IWNET_3005:BACKEND184_UNKNOWN));
    }
    service184_endpoint d=destination;d.family=6;c=fixture();
    CHECK(backend184_classify_blocked(&c,&d)==BACKEND184_UNKNOWN);
    CHECK(!strcmp(backend184_kind_name(BACKEND184_IWNET_3005),"iwnet_3005"));
    CHECK(!strcmp(backend184_kind_name((backend184_kind)99),"unknown"));
    printf("backend184: %u pure context checks passed; no payload/game/network calls\n",checks);
    return 0;
}
