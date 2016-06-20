#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_ether.h>
#include <rte_byteorder.h>
#include <rte_ethdev.h>
#include <netinet/tcp.h>
#include "msg.pb.h"
#include "clean.h"
#include "clean_ip_item.h"
#include "clean_tcp.h"
#include "syn_cookie.h"

static inline void swapIp(struct ipv4_hdr *ipv4Hdr)
{
  uint32_t tmp;
  tmp = ipv4Hdr->src_addr;
  ipv4Hdr->src_addr = ipv4Hdr->dst_addr;
  ipv4Hdr->dst_addr = tmp;
}

static inline void swapPort(struct tcp_hdr *tcpHdr)
{
  uint32_t tmp;
  tmp = tcpHdr->src_port;
  tcpHdr->src_port = tcpHdr->dst_port;
  tcpHdr->dst_port = tmp;
}

static inline void updateIpCksum(struct ipv4_hdr *ipv4Hdr)
{
  ipv4Hdr->hdr_checksum = 0;
  ipv4Hdr->hdr_checksum = rte_ipv4_cksum(ipv4Hdr);
}

static inline void updateTcpCksum(struct ipv4_hdr *ipv4Hdr, struct tcp_hdr *tcpHdr)
{
  tcpHdr->cksum = 0;
  tcpHdr->cksum = rte_ipv4_udptcp_cksum(ipv4Hdr, tcpHdr);
}

int CleanCoreContext::CleanTcpCheck(struct rte_mbuf *mbuf, struct ipv4_hdr *ipv4Hdr)
{
  uint32_t replySyn;
 bool res =  whiteIpStore_.find(htonl(ipv4Hdr->src_addr) );

  if (res) {
    //err_msg("ip found in white ip store, pass\n");
    return CLEAN_CHECK_PASS;
  }

  struct tcphdr *ptcpHdr = (struct tcphdr *)((char*)ipv4Hdr + sizeof(struct ipv4_hdr));

  if
    ((ptcpHdr->syn) && !(ptcpHdr->ack) && !(ptcpHdr->rst) && !(ptcpHdr->fin))
  {
    //challenge by syn cookie
    if (TcpCheckSynNum_ >= MAX_PKT_BURST) {

      return CLEAN_CHECK_DROP;
    } else {
      err_msg("syn pack, add to TcpCheckSynBuf");
      TcpCheckSynBuf_[TcpCheckSynNum_] = mbuf;
      ++TcpCheckSynNum_;
      return CLEAN_CHECK_REPLY;
    }
  } else if (!(ptcpHdr->syn) && (ptcpHdr->ack) && !(ptcpHdr->rst) && !(ptcpHdr->fin)) {
    if (TcpCheckAckNum_ >= MAX_PKT_BURST) {
      return CLEAN_CHECK_DROP;
    } else {
      TcpCheckAckBuf_[TcpCheckAckNum_] = mbuf;
      ++TcpCheckAckNum_;
      err_msg("ack syn check, add to TcpCheckAckBuf");
      return CLEAN_CHECK_REPLY;
    }
  } else {
    err_msg("other packs, drop it");
    return CLEAN_CHECK_DROP;
  }
}

 void CleanCoreContext::prepareAckSynPacket(struct rte_mbuf *mbuf)
{
  struct ether_hdr *etherHdr;
  uint32_t replySyn;
  struct ipv4_hdr *ipv4Hdr = rte_pktmbuf_mtod_offset(mbuf, struct ipv4_hdr *, sizeof(struct ether_hdr));
  struct tcp_hdr *tcpHdr = (struct tcp_hdr *)((char *)ipv4Hdr + sizeof(struct ipv4_hdr));
  //update 3 layer and 4 layer bits
  replySyn = SecureTcpSynCookie(ipv4Hdr, tcpHdr);
  tcpHdr->recv_ack = rte_cpu_to_be_32(rte_be_to_cpu_32(tcpHdr->sent_seq) + 1);
  tcpHdr->sent_seq = rte_cpu_to_be_32(replySyn);

  struct tcphdr * ptcpHdr = (struct tcphdr *)tcpHdr;
  ptcpHdr->syn = 1;
  ptcpHdr->ack = 1;
  swapIp(ipv4Hdr);
  swapPort(tcpHdr);
  updateIpCksum(ipv4Hdr);
  updateTcpCksum(ipv4Hdr, tcpHdr);
}

static inline void prepareResetPacket(struct ipv4_hdr *ipv4Hdr, struct tcp_hdr *tcpHdr)
{
  struct tcphdr * ptcpHdr = (struct tcphdr *)tcpHdr;
  ptcpHdr->rst = 1;
  ptcpHdr->ack = 1;
  uint32_t recvAck = tcpHdr->recv_ack;
  tcpHdr->recv_ack = tcpHdr->sent_seq;
  tcpHdr->sent_seq = recvAck;
  swapIp(ipv4Hdr);
  swapPort(tcpHdr);
  updateIpCksum(ipv4Hdr);
  updateTcpCksum(ipv4Hdr, tcpHdr);
}

void CleanCoreContext::SendAckSynPackets()
{
  int i, rt;
  struct ether_hdr *etherHdr;

  if (TcpCheckSynNum_ == 0) {
    return;
  }

  for (i = 0; i < TcpCheckSynNum_; i++) {
    err_msg("prepare ack syn packet\n");
    prepareAckSynPacket(TcpCheckSynBuf_[i]);
  }

  //set layer 2 mac
  for (i = 0; i < TcpCheckSynNum_; i++) {
    etherHdr = rte_pktmbuf_mtod(TcpCheckSynBuf_[i], struct ether_hdr *);
    rte_memcpy(etherHdr->s_addr.addr_bytes, etherHdr->d_addr.addr_bytes, 6);
    rte_memcpy(etherHdr->d_addr.addr_bytes, ReplyDstMac_, 6);
  }

  rt = rte_eth_tx_burst(ReplyTxPort_, ReplyTxQueue_, TcpCheckSynBuf_, TcpCheckSynNum_);

  //free not sent packets
  if (unlikely(rt < TcpCheckSynNum_)) {
    do {
      rte_pktmbuf_free(TcpCheckSynBuf_[rt]);
    } while (++rt < TcpCheckSynNum_);
  }
}

void CleanCoreContext::ValidateAckPackets()
{
  uint16_t sendNum = 0;
  struct ether_hdr *etherHdr;
  struct ipv4_hdr *ipv4Hdr;
  struct tcp_hdr *tcpHdr;
  int i, rt;

  if (TcpCheckAckNum_ == 0) {
    return;
  }

  for (i = 0; i < TcpCheckAckNum_; i++) {
    ipv4Hdr = rte_pktmbuf_mtod_offset(TcpCheckAckBuf_[i], struct ipv4_hdr *, sizeof(struct ether_hdr));
    tcpHdr = (struct tcp_hdr *)((char *)ipv4Hdr + sizeof(struct ipv4_hdr));

      struct IpCleanItem *ipItem = dstiptable_.find(ntohl(ipv4Hdr->dst_addr));
    if (SynCookieValidate(ipv4Hdr, tcpHdr) == 0) {
      ipItem->ipcleanItemStat.set_tcpacceptpacks(ipItem->ipcleanItemStat.tcpacceptpacks()+1);
      err_msg("pack validate succeed, add to white ip store");

      whiteIpStore_.insert(htonl(ipv4Hdr->src_addr) );
      prepareResetPacket(ipv4Hdr, tcpHdr);
      TcpCheckAckBuf_[sendNum++] == TcpCheckAckBuf_[i];
    } else {
      err_msg("pack validate failed, drop it");
      rte_pktmbuf_free(TcpCheckAckBuf_[i]);
    }
  }

  TcpCheckAckNum_ = sendNum;

  //set layer 2 mac
  for (i = 0; i < TcpCheckAckNum_; i++) {
    etherHdr = rte_pktmbuf_mtod(TcpCheckAckBuf_[i], struct ether_hdr *);
    rte_memcpy(etherHdr->s_addr.addr_bytes, etherHdr->d_addr.addr_bytes, 6);
    rte_memcpy(etherHdr->d_addr.addr_bytes, ReplyDstMac_, 6);
  }

  rt = rte_eth_tx_burst(ReplyTxPort_, ReplyTxQueue_, TcpCheckAckBuf_, TcpCheckAckNum_);

  //free not sent packets
  if (unlikely(rt <  TcpCheckAckNum_)) {
    do {
      rte_pktmbuf_free(TcpCheckAckBuf_[rt]);
    } while (++rt <  TcpCheckAckNum_);
  }
  TcpCheckAckNum_ = 0;
}
