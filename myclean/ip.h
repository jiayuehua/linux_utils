#ifndef IP_H_
#define IP_H_

static inline uint32_t IpPackBits(struct ipv4_hdr *ipv4Hdr)
{
  //TODO
  return RTE_MAX(64, (rte_be_to_cpu_16(ipv4Hdr->total_length) + 18/*MAC Header*/)) * 8;
}

#endif
