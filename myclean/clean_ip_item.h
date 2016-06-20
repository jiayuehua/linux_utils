#ifndef CLEAN_IP_ITEM_H_
#define CLEAN_IP_ITEM_H_
#include <utility>
#include <boost/functional/hash.hpp>
#include <boost/unordered_map.hpp>
#include <memory.h>
#include "msg.pb.h"

struct HttpCleanItem {
  HttpCleanItem(){}
  explicit HttpCleanItem(int srcIp, int dstIp){
    httpCleanItemStat_.set_srcip(srcIp);
    httpCleanItemStat_.set_dstip(dstIp);
    httpCleanItemStat_.set_httpconns(0);
    httpCleanItemStat_.set_httpreqs(0);
  }
  msg::CCStat httpCleanItemStat_;

  uint32_t TimeSeq;
  uint32_t BlackExpireTime;

  uint32_t BlackPacks;
  uint32_t PassPacks;

  uint32_t HttpReqs;

  friend bool operator==(const HttpCleanItem& l, const HttpCleanItem& r){
    return l.httpCleanItemStat_.srcip()== r.httpCleanItemStat_.srcip()
        && l.httpCleanItemStat_.dstip()== r.httpCleanItemStat_.dstip() ;
  }
  friend bool operator!=(const HttpCleanItem& l, const HttpCleanItem& r){
    return  !(l == r);
  }
};
struct IpCleanItem {
  IpCleanItem(uint32_t ip):IpCleanEnableTime(0), IpBlackExpireTime(0), HttpCleanEnableTime(0)
   {
     ipcleanItemStat.set_ip(ip);
   }
  uint32_t Ip;

  //uint32_t TimeSeq;
  int64_t IpCleanEnableTime;
  int64_t IpBlackExpireTime;
  int64_t HttpCleanEnableTime;

  msg::IpClean ipcleanItemStat;
  friend bool operator!=(const IpCleanItem& l, const IpCleanItem& r){
    return l.ipcleanItemStat.ip() != r.ipcleanItemStat.ip();
  }
  friend bool operator==(const IpCleanItem& l, const IpCleanItem& r){
    return l.ipcleanItemStat.ip() == r.ipcleanItemStat.ip();
  }
};
namespace boost{
template <>
struct hash<IpCleanItem>: public hash<int>
{
  std::size_t operator()(const IpCleanItem& val)const
  {
    return hash<int>::operator() (val.ipcleanItemStat.ip());
  }
};
}

namespace boost{
template <>
struct hash<HttpCleanItem>: public hash<std::pair<uint32_t, uint32_t> >
{
  std::size_t operator()(const HttpCleanItem& val)const
  {
    return hash<std::pair<uint32_t,uint32_t> >::operator() (std::make_pair(val.httpCleanItemStat_.srcip(), val.httpCleanItemStat_.dstip()) );
  }
};
}
static inline void CleanIpItemReset(struct IpCleanItem *cleanIpItem, uint32_t ip)
{
  memset(cleanIpItem, 0, sizeof(struct IpCleanItem));
  cleanIpItem->Ip = ip;
}

static inline int DstIpBlack(struct IpCleanItem *cleanIpItem, uint32_t now)
{
  return now < cleanIpItem->IpBlackExpireTime;
}

static inline int DstIpCleanEnabled(struct IpCleanItem *cleanIpItem, uint32_t now)
{
  return now < cleanIpItem->IpCleanEnableTime;
}

static inline int DstHttpCleanEnabled(struct IpCleanItem *cleanIpItem, uint32_t now)
{
  return now < cleanIpItem->HttpCleanEnableTime;
}

#endif
