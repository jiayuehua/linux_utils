#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/igmp.h>
#include <netinet/tcp.h>
#include <rte_ether.h>
#include <rte_ring.h>

#include "clean_http.h"
#include "clean_ip_item.h"
#include "msg.pb.h"

HttpString GetStr = HttpStrMacro("GET ");
HttpString PostStr = HttpStrMacro("POST ");
HttpString PutStr = HttpStrMacro("PUT ");
HttpString HeadStr = HttpStrMacro("HEAD ");


int CleanCoreContext::CleanHttpCheck( struct rte_mbuf *m, IpCleanItem * pIpCleanItem)
{
  struct ether_hdr* eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
  uint16_t mdata_len = rte_pktmbuf_data_len(m);

  if (mdata_len > sizeof(struct ether_hdr)) {
    mdata_len -= sizeof(struct ether_hdr);
    uint16_t ether_type = eth_hdr->ether_type;
    struct ip * pIp = 0 ;

    if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4)) {
      pIp = (struct ip*)(eth_hdr + 1);
    } else if (eth_hdr->ether_type == rte_be_to_cpu_16(ETHER_TYPE_VLAN)) {
      struct vlan_hdr *  vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);
      mdata_len -=  sizeof(struct vlan_hdr);

      if (vlan_hdr->eth_proto != rte_be_to_cpu_16(ETHER_TYPE_IPv4)) {
        return CLEAN_CHECK_DROP;
      }

      pIp = (struct ip *)((char*)eth_hdr + sizeof(struct ether_hdr) + sizeof(struct vlan_hdr));
    }

    if (pIp) {
      if (mdata_len < sizeof(struct ip)) {
        return CLEAN_CHECK_DROP;
      }

      uint16_t   ip_len =  ntohs(pIp->ip_len);
      uint32_t pkt_bytes = (uint32_t) ip_len + sizeof(struct ether_hdr);
      unsigned int ip_hl = pIp->ip_hl << 2;
      uint16_t ip_off = ntohs(pIp->ip_off);

      if ((pIp->ip_hl < 5 ||  ip_len < ip_hl)
          || ((ip_off & IP_MF) && (ip_off && IP_DF))
          || mdata_len < ip_hl
          || (ntohl(pIp->ip_src.s_addr) == 0 || ntohl(pIp->ip_dst.s_addr) == 0)) {
        return CLEAN_CHECK_DROP;
      }

      mdata_len -=  ip_hl;

      if (pIp->ip_p == IPPROTO_TCP && mdata_len >= sizeof(struct tcphdr)) {
        char* L4Hdr = (char*) pIp + ip_hl;
        struct tcphdr * ptcp = (struct tcphdr*)(L4Hdr);
        const char * httpdata = (const char*)ptcp + (ptcp->doff << 2);
        int httplen = (int)(ip_len - ip_hl - (ptcp->doff << 2));
        mdata_len -= ptcp->doff << 2;
        //if (DstHttpCleanEnabled(pIpCleanItem, TimeNow_))
        {
          if ((mdata_len >= (int)GetStr.len) && (strncmp(httpdata, (const char*)GetStr.data, GetStr.len) == 0)
            || (mdata_len >= (int)PostStr.len) && (strncmp(httpdata, (const char*)PostStr.data, PostStr.len) == 0)
            || (mdata_len >= (int)HeadStr.len) && (strncmp(httpdata, (const char*)HeadStr.data, HeadStr.len) == 0)
            || (mdata_len >= (int)PutStr.len) && (strncmp(httpdata, (const char*)PutStr.data, PutStr.len) == 0)) {
              HttpCleanItem  httpItem( ntohl(pIp->ip_src.s_addr), ntohl(pIp->ip_dst.s_addr) );
              HttpCleanItem * pHttpItem = httptable_.insert(httpItem);
              pHttpItem->TimeSeq = TimeNow_;
              pHttpItem->httpCleanItemStat_.set_httpreqs(pHttpItem->httpCleanItemStat_.httpreqs()+1);
              pIpCleanItem->ipcleanItemStat.set_httpreqs(pIpCleanItem->ipcleanItemStat.httpreqs()+1);


              if (pHttpItem->TimeSeq < pHttpItem->BlackExpireTime) {
                ++pHttpItem->BlackPacks;
                pIpCleanItem->ipcleanItemStat.set_l7droppacks(pIpCleanItem->ipcleanItemStat.l7droppacks()+1);
                return CLEAN_CHECK_DROP;
              } else {
                pIpCleanItem->ipcleanItemStat.set_l7forwardpacks(pIpCleanItem->ipcleanItemStat.l7forwardpacks()+1);
                ++pHttpItem->PassPacks;
              }
          }
        }
        pIpCleanItem->ipcleanItemStat.set_tcpforwardpacks(pIpCleanItem->ipcleanItemStat.tcpforwardpacks()+1);

      }

      return CLEAN_CHECK_PASS;
    }

    return CLEAN_CHECK_DROP;
  }
}