#ifndef SYN_COOKIE_H_
#define SYN_COOKIE_H_

#include <rte_ip.h>
#include <rte_tcp.h>

void syncookieInit(void);
uint32_t SecureTcpSynCookie(struct ipv4_hdr *ipv4Hdr, struct tcp_hdr *tcpHdr);
int SynCookieValidate(struct ipv4_hdr *ipv4Hdr, struct tcp_hdr *tcpHdr);

#endif