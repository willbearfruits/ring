// The network for Link on Linux: one UDP socket per listened port, the multicast group joined on every interface,
// this machine's first non-loopback IPv4 address as its own.
#include "../core/net.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
struct net_state net;
static struct { uint16_t port; int fd; udp_handler h; } L[4]; static int nl;
void net_init(void)
{
    memset(&net, 0, sizeof net); net.phase = NET_NO_LINK; snprintf(net.status, sizeof net.status, "no interface");
    struct ifaddrs *ifa; if (getifaddrs(&ifa) != 0) return;
    for (struct ifaddrs *p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET || (p->ifa_flags & IFF_LOOPBACK) || !(p->ifa_flags & IFF_UP)) continue;
        if (strncmp(p->ifa_name, "docker", 6) == 0 || strncmp(p->ifa_name, "veth", 4) == 0 || strncmp(p->ifa_name, "br-", 3) == 0 || strncmp(p->ifa_name, "tailscale", 9) == 0) continue;
        net.ip = ntohl(((struct sockaddr_in *) p->ifa_addr)->sin_addr.s_addr); net.phase = NET_READY;
        int s = socket(AF_INET, SOCK_DGRAM, 0); if (s >= 0) { struct ifreq r; memset(&r, 0, sizeof r); strncpy(r.ifr_name, p->ifa_name, IFNAMSIZ - 1); if (ioctl(s, SIOCGIFHWADDR, &r) == 0) memcpy(net.mac, r.ifr_hwaddr.sa_data, 6); close(s); }
        snprintf(net.status, sizeof net.status, "%s %u.%u.%u.%u", p->ifa_name, net.ip >> 24, (net.ip >> 16) & 255, (net.ip >> 8) & 255, net.ip & 255);
        break;
    }
    freeifaddrs(ifa);
}
bool net_listen(uint16_t port, udp_handler h)
{
    if (nl >= 4) return false;
    const int fd = socket(AF_INET, SOCK_DGRAM, 0); if (fd < 0) return false;
    const int one = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
#ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);
#endif
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = htonl(INADDR_ANY) };
    if (bind(fd, (struct sockaddr *) &a, sizeof a) != 0) { close(fd); return false; }
    fcntl(fd, F_SETFL, O_NONBLOCK);
    L[nl].port = port; L[nl].fd = fd; L[nl].h = h; nl++;
    return true;
}
bool net_join(uint32_t group)
{
    for (int i = 0; i < nl; i++) { struct ip_mreq m = { .imr_multiaddr.s_addr = htonl(group), .imr_interface.s_addr = htonl(INADDR_ANY) }; setsockopt(L[i].fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof m); const unsigned char ttl = 1, loop = 1; setsockopt(L[i].fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl); setsockopt(L[i].fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof loop); }
    return true;
}
bool net_send(uint32_t dst, uint16_t src_port, uint16_t dst_port, const void *data, int len)
{
    int fd = -1; for (int i = 0; i < nl; i++) if (L[i].port == src_port) fd = L[i].fd;
    if (fd < 0) return false;
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(dst_port), .sin_addr.s_addr = htonl(dst) };
    return sendto(fd, data, (size_t) len, 0, (struct sockaddr *) &a, sizeof a) == len;
}
void net_work(uint64_t now)
{
    static uint8_t buf[1500];
    for (int i = 0; i < nl; i++) {
        for (int k = 0; k < 16; k++) {
            struct sockaddr_in from; socklen_t fl = sizeof from;
            const ssize_t n = recvfrom(L[i].fd, buf, sizeof buf, 0, (struct sockaddr *) &from, &fl);
            if (n <= 0) break;
            L[i].h(ntohl(from.sin_addr.s_addr), ntohs(from.sin_port), net.ip, buf, (int) n, now);
        }
    }
}
