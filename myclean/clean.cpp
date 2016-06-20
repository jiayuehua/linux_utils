#include <arpa/inet.h>
#include <rte_memcpy.h>
#include <rte_ip.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include "clean.h"
#include "ether.h"
#include "ip.h"
#include "clean_ip_item.h"
#include "clean_tcp.h"
//#include "clean_udp.h"
//#include "clean_icmp.h"
#include "clean_http.h"
#include "dpdk.h"


void CleanCoreContext::recvPackets()
{
  RecvNum_ = rte_eth_rx_burst(RxPort_, RxQueue_, RecvBuf_, MAX_PKT_BURST);

  if (RecvNum_) {
    //err_msg("core recv: %d", RecvNum_);
  }
}

void CleanCoreContext::CleanForward(struct rte_mbuf *mbuf)
{
  ForwardBuf_[ForwardNum_++] = mbuf;
}

void CleanCoreContext::InsertIpCleanItem()
{
  int32_t netIpAddr = 0;
  size_t i = 0;
  int r = -1;

  size_t sz =SingletonInstance<DpdkConfig>().ipranges.size();
  for (; i<sz; ++i) {
    r = -1;

    size_t pos = SingletonInstance<DpdkConfig>().ipranges[i].find('/');

    if (pos == std::string::npos) {
      if (0 < inet_pton(AF_INET, SingletonInstance<DpdkConfig>().ipranges[i].c_str(), &netIpAddr)) {
        dstiptable_.insert(IpCleanItem(netIpAddr));
      }
    } else {
      SingletonInstance<DpdkConfig>().ipranges[i][pos] = 0;

      if (0 < inet_pton(AF_INET, SingletonInstance<DpdkConfig>().ipranges[i].c_str(), &netIpAddr)) {
        netIpAddr = ntohl(netIpAddr);
        int32_t n = 32 - atoi(SingletonInstance<DpdkConfig>().ipranges[i].c_str()+pos+1);
        r <<= n;
        int32_t sz = 1 << n;
        netIpAddr &= r;
        int k = 0;

        for (; k < sz ; ++k) {
          dstiptable_.insert(IpCleanItem(netIpAddr+k));
          //char ipStr[20];
          //int t = htonl(netIpAddr+k);
          //inet_ntop(AF_INET, &t , ipStr, sizeof(ipStr)) ;
          //std::cout<<ipStr<<std::endl;
        }
      }
    }
  }
}
CleanCoreContext::CleanCoreContext(int id): httptable_(pool_)
{
  std::string name = boost::lexical_cast<std::string>(id);
  inring_ = rte_ring_create(name.c_str(), 4, rte_lcore_to_socket_id(id), RING_F_SC_DEQ | RING_F_SP_ENQ);

  if (!inring_) {
    err_msg("fail to create outring_: %s", rte_strerror(rte_errno));
  }

 /* uint32_t ipnum = 0;
  int t = inet_pton(AF_INET, "192.168.178.252", &ipnum);

  if (t <= 0) {
    err_msg("initCleanCore inet_pton fail");
  }*/

  //ipnum = ntohl(ipnum);
  InsertIpCleanItem();
  RxPort_ = 0;
  RxQueue_ = 0;
  ReplyTxPort_ = 0;
  ReplyTxQueue_ = 0;
  MacStrToBytes("00:0C:29:1E:77:E2", ReplyDstMac_);
  ForwardTxPort_ = 0;
  ForwardTxQueue_ = 0;
  MacStrToBytes("00:0c:29:4d:6c:58", ForwardDstMac_);
}

void CleanCoreContext::checkPacket(struct rte_mbuf *mbuf)
{
  struct ether_hdr *etherHdr;
  struct ipv4_hdr *ipv4Hdr;
  struct tcp_hdr *tcpHdr;
  struct icmp_hdr *icmpHdr;
  struct udp_hdr *udpHdr;
  int rt = CLEAN_CHECK_PASS;
  etherHdr = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);

  //only check Ipv4 packet, packets of other types are dropped
  if (unlikely(etherHdr->ether_type != rte_be_to_cpu_16(ETHER_TYPE_IPv4))) {
    rte_pktmbuf_free(mbuf);
    return;
  }

  ipv4Hdr = (struct ipv4_hdr*)rte_pktmbuf_mtod_offset(mbuf, struct ipv4_hdr *, sizeof(struct ether_hdr));
  uint32_t desAddr = ntohl(ipv4Hdr->dst_addr);
  struct IpCleanItem *ipItem = dstiptable_.find(desAddr);

  if (!ipItem) {
    //if the ip is not allowed to route, drop it
    err_msg("ip not found");
    rte_pktmbuf_free(mbuf);
    return;
  }

  //ipItem->TimeSeq = TimeNow_;
  char ipStr[20];
  inet_ntop(AF_INET, & ipv4Hdr->dst_addr, ipStr, sizeof(ipStr)) ;
  //err_msg("dst ip: %s",  ipStr);
  ipItem->ipcleanItemStat.set_inpacks(ipItem->ipcleanItemStat.inpacks() + 1);
  ipItem->ipcleanItemStat.set_inbits(ipItem->ipcleanItemStat.inbits() + IpPackBits(ipv4Hdr));
  /* if (!DstIpCleanEnabled(ipItem, TimeNow_))
   {
     err_msg("Don't clean!");
     ipItem->ipcleanItemStat.set_frdpacks(ipItem->ipcleanItemStat.frdpacks() + 1);
     ipItem->ipcleanItemStat.set_frdbits( ipItem->ipcleanItemStat.frdbits()+ IpPackBits(ipv4Hdr)  );
     CleanForward( mbuf);
     return ;
   }

   else*/
  {
    if (DstIpBlack(ipItem, TimeNow_)) {
      ipItem->ipcleanItemStat.set_droppacks(ipItem->ipcleanItemStat.droppacks() + 1);
      ipItem->ipcleanItemStat.set_dropbits(ipItem->ipcleanItemStat.dropbits() + IpPackBits(ipv4Hdr));
      ipItem->ipcleanItemStat.set_blackholepacks(ipItem->ipcleanItemStat.blackholepacks() + 1);
      ipItem->ipcleanItemStat.set_blackholebits(ipItem->ipcleanItemStat.blackholebits() + IpPackBits(ipv4Hdr));
      err_msg("blacked enabled.");
      rte_pktmbuf_free(mbuf);
      return;
    }

    switch (ipv4Hdr->next_proto_id) {
    case IPPROTO_TCP:
      //err_msg("clean tcp check.");
      rt = CleanTcpCheck(mbuf, ipv4Hdr);

      if (rt == CLEAN_CHECK_PASS) {
        tcpHdr = (struct tcp_hdr *)((char *)ipv4Hdr + sizeof(struct ipv4_hdr));
        rt = CleanHttpCheck(mbuf, ipItem);
        //err_msg("clean http check.");
      }

      break;

    case IPPROTO_UDP:   /* fall */
    case IPPROTO_ICMP:  /* fall */
    case IPPROTO_IGMP:
      rt = CLEAN_CHECK_DROP;
      break;
    }

    if (rt == CLEAN_CHECK_DROP) {
      ipItem->ipcleanItemStat.set_droppacks(ipItem->ipcleanItemStat.droppacks() + 1);
      ipItem->ipcleanItemStat.set_dropbits(ipItem->ipcleanItemStat.dropbits() + IpPackBits(ipv4Hdr));
      err_msg("clean check result: DROP");
      rte_pktmbuf_free(mbuf);
    } else if (rt == CLEAN_CHECK_PASS) {
      //err_msg("clean check result: PASS");
      ipItem->ipcleanItemStat.set_frdpacks(ipItem->ipcleanItemStat.frdpacks() + 1);
      ipItem->ipcleanItemStat.set_frdbits(ipItem->ipcleanItemStat.frdbits() + IpPackBits(ipv4Hdr));
      CleanForward(mbuf);
    } else if (rt == CLEAN_CHECK_REPLY) {
      ipItem->ipcleanItemStat.set_tcpreplypacks(ipItem->ipcleanItemStat.tcpreplypacks() + 1);
      ipItem->ipcleanItemStat.set_droppacks(ipItem->ipcleanItemStat.droppacks() + 1);
      ipItem->ipcleanItemStat.set_dropbits(ipItem->ipcleanItemStat.dropbits() + IpPackBits(ipv4Hdr));
      //do nothing
      err_msg("clean check result: SKIP");
    } else {
      ipItem->ipcleanItemStat.set_frdpacks(ipItem->ipcleanItemStat.frdpacks() + 1);
      ipItem->ipcleanItemStat.set_frdbits(ipItem->ipcleanItemStat.frdbits() + IpPackBits(ipv4Hdr));
      //Should not reach here
      err_msg("clean check result DEFAULT: PASS");
      CleanForward(mbuf);
    }
  }
}

void CleanCoreContext::checkPackets()
{
  int i;

  for (i = 0; i < RecvNum_; i++) {
    checkPacket(RecvBuf_[i]);
  }
}

void CleanCoreContext::forwardPackets()
{
  int i, rt;
  struct ether_hdr *etherHdr;

  if (ForwardNum_ == 0) {
    return;
  }

  //set layer 2 mac
  for (i = 0; i < ForwardNum_; i++) {
    etherHdr = rte_pktmbuf_mtod(ForwardBuf_[i], struct ether_hdr *);
    rte_memcpy(etherHdr->s_addr.addr_bytes, etherHdr->d_addr.addr_bytes, 6);
    rte_memcpy(etherHdr->d_addr.addr_bytes, ForwardDstMac_, 6);
  }

  //batch forward packets
  rt = rte_eth_tx_burst(ForwardTxPort_, ForwardTxQueue_, ForwardBuf_, ForwardNum_);

  if (rt < ForwardNum_) {
    do {
      rte_pktmbuf_free(ForwardBuf_[rt]);
    } while (++rt < ForwardNum_);
  }
}

void CleanCoreContext::resetBufs()
{
  RecvNum_ = 0;
  TcpCheckSynNum_ = 0;
  TcpCheckAckNum_ = 0;
  ForwardNum_ = 0;
}

void CleanCoreContext::SendHttpCleanStats()
{
  err_msg("CleanCortext::SendCleanStats");
  RingHttpCleanItem* pRingHttpCleanItem = 0;
  //boost::shared_ptr<OutputList<HttpCleanItem> >  poutlist;
  int j = 0;
  for (PriorityListUnorderSet<HttpCleanItem, PriorityListUnordersetBucketSize>::reverse_iterator i = httptable_.rbegin(); i != httptable_.rend() && i->get().TimeSeq == TimeNow_; i = httptable_.rbegin(), ++j) {
    if (j%CleanOutStatsSize == 0 )
    {
      pRingHttpCleanItem = new RingHttpCleanItem(pool_);
      //poutlist.reset(pRingHttpCleanItem->phttptable_);
    }

    PriorityListUnorderSet<HttpCleanItem, PriorityListUnordersetBucketSize>::value_type * p = & *i;
    httptable_.pop_back_nodelete();
    //std::cout<<*p;
    pRingHttpCleanItem->phttptable_->push_back(*p);

    if ((j+1)%CleanOutStatsSize == 0 )
    {
      for (OutputList<HttpCleanItem>::const_reverse_iterator i = pRingHttpCleanItem->phttptable_->crbegin(); i != pRingHttpCleanItem->phttptable_->crend(); ++i) {
        httptable_.insertInPlace(i->get().httpCleanItemStat_.srcip(), i->get().httpCleanItemStat_.dstip());
        //std::cout<<*i;
      }
      int r = -1;

      if (pRingHttpCleanItem && !pRingHttpCleanItem->phttptable_->empty()) {
        err_msg("CleanCortext::SendCleanStats, http enqueue");
        r = rte_ring_enqueue(globalRing(), pRingHttpCleanItem);

        if (r != 0) {
          err_msg("SendCleanStats Http fail:" , rte_strerror(rte_errno));
        }
      } else {
        err_msg("CleanCortext::SendCleanStats, http stats is Empty!");
      }

      if (r != 0&& pRingHttpCleanItem) {
        std::auto_ptr<RingCleanItem> (pRingHttpCleanItem);
        //return;
      }
      pRingHttpCleanItem = 0;
    }
  }
  int r = -1;

  if (pRingHttpCleanItem && !pRingHttpCleanItem->phttptable_->empty()) {
    err_msg("CleanCortext::SendCleanStats, http enqueue");
    r = rte_ring_enqueue(globalRing(), pRingHttpCleanItem);

    if (r != 0) {
      err_msg("SendCleanStats Http fail:" , rte_strerror(rte_errno));
    }
  } else {
    err_msg("CleanCortext::SendCleanStats, http stats is Empty!");
  }

  if (r != 0&& pRingHttpCleanItem) {
    std::auto_ptr<RingCleanItem> (pRingHttpCleanItem);
    //return;
  }
  pRingHttpCleanItem = 0;
  //for (OutputList<HttpCleanItem>::const_reverse_iterator i = pRingHttpCleanItem->phttptable_->crbegin(); i != pRingHttpCleanItem->phttptable_->crend(); ++i) {
  //  char ipStr[20];
  //  int dstip = htonl(i->get().httpCleanItemStat_.dstip());
  //  int srcip = htonl(i->get().httpCleanItemStat_.srcip());
  //  inet_ntop(AF_INET, &dstip , ipStr, sizeof(ipStr)) ;
  //  err_msg("dst ip: %s",  ipStr);
  //  inet_ntop(AF_INET, &srcip , ipStr, sizeof(ipStr)) ;
  //  err_msg("src ip: %s",  ipStr);
  //  err_msg("HttpCleanItem: conns %d" , i->get().httpCleanItemStat_.httpconns());
  //  err_msg("HttpCleanItem: httpreqs %d " , i->get().httpCleanItemStat_.httpreqs());
  //}


}
void CleanCoreContext::ReadCleanCommand()
{
  struct RingCommandItem *pCommandItem = 0;
  int r = rte_ring_dequeue(inring_, (void**)&pCommandItem);

  if (r == 0 && pCommandItem) {
    pCommandItem->RunCommand(this);
  } else {
    //err_msg("ReadCleanCommand fail rte_ring_dequeue");
  }
}

void CleanCoreContext::SendCleanStats()
{
  SendHttpCleanStats();
  SendIpCleanStats();
}
void CleanCoreContext::SendIpCleanStats()
{

  int j = 0;
  int r = -1;

  RingIpCleanItem* pOutRingIpCleanItem = 0;

  for (UnorderSet<IpCleanItem>::iterator i = dstiptable_.begin(); i != dstiptable_.end(); ++i, ++j) {
    if (i->get().ipcleanItemStat.has_inpacks()) {
      if (j%CleanOutStatsSize ==0)
      {
         pOutRingIpCleanItem  = new RingIpCleanItem;
      }
      char ipStr[20];
      int dstip = htonl(i->get().ipcleanItemStat.ip());
      inet_ntop(AF_INET, &dstip , ipStr, sizeof(ipStr)) ;
      err_msg("**** One ipcleanItemStat ip              %s" , ipStr);
      err_msg("     One ipcleanItemStat inPacks         %ld", i->get().ipcleanItemStat.inpacks());
      err_msg("     One ipcleanItemStat inBits          %ld", i->get().ipcleanItemStat.inbits());
      err_msg("     One ipcleanItemStat frdPacks        %ld", i->get().ipcleanItemStat.frdpacks());
      err_msg("     One ipcleanItemStat frdBits         %ld", i->get().ipcleanItemStat.frdbits());
      err_msg("     One ipcleanItemStat dropPacks       %ld", i->get().ipcleanItemStat.droppacks());
      err_msg("     One ipcleanItemStat dropBits        %ld", i->get().ipcleanItemStat.dropbits());
      err_msg("     One ipcleanItemStat httpReqs        %ld", i->get().ipcleanItemStat.httpreqs());
      err_msg("     One ipcleanItemStat httpConns       %ld", i->get().ipcleanItemStat.httpconns());
      err_msg("     One ipcleanItemStat blackholeBits   %ld", i->get().ipcleanItemStat.blackholebits());
      err_msg("     One ipcleanItemStat blackholePacks  %ld", i->get().ipcleanItemStat.blackholepacks());
      err_msg("     One ipcleanItemStat TcpReplyPacks   %ld", i->get().ipcleanItemStat.tcpreplypacks());
      err_msg("     One ipcleanItemStat TcpAcceptPacks  %ld", i->get().ipcleanItemStat.tcpacceptpacks());
      err_msg("     One ipcleanItemStat TcpDropPacks    %ld", i->get().ipcleanItemStat.tcpdroppacks());
      err_msg("     One ipcleanItemStat TcpForwardPacks %ld", i->get().ipcleanItemStat.tcpforwardpacks());
      err_msg("     One ipcleanItemStat L7DropPacks     %ld", i->get().ipcleanItemStat.l7droppacks());
      err_msg("     One ipcleanItemStat L7ForwardPacks  %ld", i->get().ipcleanItemStat.l7forwardpacks());
      msg::IpClean* pOutIpCleanStat = pOutRingIpCleanItem->ipcleanstats_.add_stats();
      pOutRingIpCleanItem->ipcleanstats_.set_timeseq(TimeNow_ + 1);
      pOutRingIpCleanItem->ipcleanstats_.set_timeunit(SingletonInstance<CleanGlobalContext>().TimeUnit_);
      *pOutIpCleanStat = i->get().ipcleanItemStat;
      pOutIpCleanStat->set_inbits(pOutIpCleanStat->inbits());
      pOutIpCleanStat->set_frdpacks(pOutIpCleanStat->frdpacks());
      pOutIpCleanStat->set_frdbits(pOutIpCleanStat->frdbits());
      pOutIpCleanStat->set_droppacks(pOutIpCleanStat->droppacks());
      pOutIpCleanStat->set_dropbits(pOutIpCleanStat->dropbits());
      pOutIpCleanStat->set_httpreqs(pOutIpCleanStat->httpreqs());
      pOutIpCleanStat->set_httpconns(pOutIpCleanStat->httpconns());
      i->get().ipcleanItemStat.clear_inpacks();
      i->get().ipcleanItemStat.clear_inbits();
      i->get().ipcleanItemStat.clear_frdpacks();
      i->get().ipcleanItemStat.clear_frdbits();
      i->get().ipcleanItemStat.clear_droppacks();
      i->get().ipcleanItemStat.clear_dropbits();
      i->get().ipcleanItemStat.clear_httpreqs();
      i->get().ipcleanItemStat.clear_blackholebits();
      i->get().ipcleanItemStat.clear_blackholepacks();
      i->get().ipcleanItemStat.clear_tcpreplypacks();
      i->get().ipcleanItemStat.clear_tcpacceptpacks();
      i->get().ipcleanItemStat.clear_tcpdroppacks();
      i->get().ipcleanItemStat.clear_tcpforwardpacks();
      i->get().ipcleanItemStat.clear_l7droppacks();
      i->get().ipcleanItemStat.clear_l7forwardpacks();
      if ((j+1)%CleanOutStatsSize ==0)
      {

        r = -1;

        r = rte_ring_enqueue(globalRing(), pOutRingIpCleanItem);
        err_msg("CleanCortext::SendCleanStats, after ip enqueue");


        if (r != 0) {
          err_msg("SendCleanStats Ip fail:" , rte_strerror(rte_errno));
          std::auto_ptr<RingCleanItem> (pOutRingIpCleanItem);
        }
        pOutRingIpCleanItem = 0;
      }
    }
  }

  err_msg("CleanCortext::SendCleanStats, ip enqueue");

  if (pOutRingIpCleanItem && pOutRingIpCleanItem->ipcleanstats_.stats_size()) {
    r = rte_ring_enqueue(globalRing(), pOutRingIpCleanItem);
    err_msg("CleanCortext::SendCleanStats, after ip enqueue");

    if (r != 0) {
      err_msg("SendCleanStats Ip fail:" , rte_strerror(rte_errno));
    }
  } else {
    err_msg("CleanCortext::SendCleanStats, ip stats is Empty!");
  }

  if (r != 0 &&pOutRingIpCleanItem) {
    std::auto_ptr<RingCleanItem> (pOutRingIpCleanItem);
  }

  err_msg("CleanCortext::SendCleanStats, after ip enqueue 2");
}

void CleanCoreContext::Func()
{
  char replyDstMac[18];
  char forwardDstMac[18];
  char ipStr[16];
  err_msg("clean context, ReplyDstMac: %s, ForwardDstMac: %s", MacBytesToStr(ReplyDstMac_, replyDstMac),
          MacBytesToStr(ForwardDstMac_, forwardDstMac));
  TimeNow_ = rte_atomic64_read(&SingletonInstance<rte_atomic64_t>());
  int localTvSec = TimeNow_;
  err_msg("localtvSec: %ld, TimeNow_ %d", localTvSec, TimeNow_);

  for (int i = 0;; ++i) {
    if (i == 8)
      //err_msg("1 i=%d localtvSec: %ld, TimeNow_ %d", i,  localTvSec, TimeNow_);
    {
      i = 0;
      localTvSec = rte_atomic64_read(&SingletonInstance<rte_atomic64_t>());

      if (localTvSec != TimeNow_) {
        //err_msg("2 i=%d localtvSec: %ld, TimeNow_ %d", i,  localTvSec, TimeNow_);
        SendCleanStats();
        TimeNow_ = localTvSec;
        //break;
      }
    }

    //err_msg("Read cleanCommand");
    ReadCleanCommand();
    //err_msg("resetBufs");
    resetBufs();
    //err_msg("recvPackets");
    recvPackets();
    //err_msg("checkPackets");
    checkPackets();
    //err_msg("ForwardPackets");
    forwardPackets();
    //err_msg("SendAckSynPackets");
    SendAckSynPackets();
    //err_msg("ValidateAckPackets");
    ValidateAckPackets();
  }
}
