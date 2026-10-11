// What Link needs from a network: UDP in and out, a multicast group, this machine's address. The Linux host
// does it with sockets (host/net_linux.c); bare metal with its own stack.
#pragma once
#include "lib.h"
enum { NET_NONE, NET_NO_LINK, NET_READY };
struct net_state { int phase; uint8_t mac[6]; uint32_t ip; char status[80]; };
extern struct net_state net;
typedef void (*udp_handler)(uint32_t src_ip, uint16_t src_port, uint32_t dst_ip, const uint8_t *data, int len, uint64_t now_ms);
void net_init(void);
void net_work(uint64_t now_ms);
bool net_listen(uint16_t port, udp_handler h);
bool net_send(uint32_t dst, uint16_t src_port, uint16_t dst_port, const void *data, int len);
bool net_join(uint32_t group);
#define NET_IP(a, b, c, d) ((uint32_t) (a) << 24 | (uint32_t) (b) << 16 | (uint32_t) (c) << 8 | (uint32_t) (d))
