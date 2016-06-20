#ifndef CLEAN_H_
#define CLEAN_H_
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <stdint.h>
#include <rte_ring.h>
#include <rte_atomic.h>
#include <rte_atomic_64.h>
#include <rte_mbuf.h>
#include "listunorderset.h"
#include "unorderset.h"
#include "prioritylistunorderset.h"
#include "clean_ip_item.h"
#include "msg.pb.h"
#include "unp.h"
enum { MAX_PKT_BURST = 32};

struct rte_ring* globalRing();
template<class T>
T &SingletonInstance()
{
  static T t;
  return t;
}
enum CLEAN_RESULT {
  CLEAN_CHECK_PASS  ,
  CLEAN_CHECK_DROP  ,
  CLEAN_CHECK_REPLY
};

struct CleanCoreContext;
void CleanCoreFunc(CleanCoreContext *);

struct CleanGlobalContext {
  CleanGlobalContext():TimeUnit_(1){}
  uint8_t TimeUnit_;
};

enum {
  RingActionCommandType = 5,
  RingHttpCommandType,
};
enum{PriorityListUnordersetBucketSize = 3};
enum{UnorderSetBucketSize = 3};
enum{whiteIpsBucketSize = 7};
struct CleanCoreContext {
  uint32_t TimeNow_;

  uint8_t RxPort_;
  uint8_t RxQueue_;
  uint8_t RecvNum_;
  struct rte_mbuf *RecvBuf_[MAX_PKT_BURST];

  uint8_t ReplyTxPort_;
  uint8_t ReplyTxQueue_;
  uint8_t ReplyDstMac_[6];

  struct rte_mbuf *TcpCheckSynBuf_[MAX_PKT_BURST];
  uint8_t TcpCheckSynNum_;

  struct rte_mbuf *TcpCheckAckBuf_[MAX_PKT_BURST];
  uint8_t TcpCheckAckNum_;

  uint8_t ForwardTxPort_;
  uint8_t ForwardTxQueue_;
  uint8_t ForwardDstMac_[6];
  uint8_t ForwardNum_;
  struct rte_mbuf *ForwardBuf_[MAX_PKT_BURST];

  ListUnorderSet<uint32_t,whiteIpsBucketSize> whiteIpStore_;
  UnorderSet<IpCleanItem, UnorderSetBucketSize> dstiptable_;
  PriorityListUnorderSet<HttpCleanItem, PriorityListUnordersetBucketSize>::pool_type pool_;
  PriorityListUnorderSet<HttpCleanItem, PriorityListUnordersetBucketSize> httptable_;
  struct rte_ring* inring_;

  CleanCoreContext(int id);
  void Func();
private:
  void checkPacket(struct rte_mbuf *mbuf);
  void checkPackets();
  void forwardPackets();
  void resetBufs();
  void ReadCleanCommand();
  void SendCleanStats();
  void SendIpCleanStats();
  void SendHttpCleanStats();
  void recvPackets();
  void CleanForward(struct rte_mbuf *mbuf);
  void InsertIpCleanItem();
  int CleanTcpCheck(struct rte_mbuf *mbuf, struct ipv4_hdr *ipv4Hdr);
  void prepareAckSynPacket(struct rte_mbuf *mbuf);
  void SendAckSynPackets();
  void ValidateAckPackets();
  int CleanHttpCheck(struct rte_mbuf *m, IpCleanItem * pIpCleanItem);
};

struct RingCommandItem {
  int type_;
  RingCommandItem(int type): type_(type) {}
  virtual bool ParseFromArray(void *data , int sz) {return false;}
  virtual RingCommandItem * clone()const {return 0;}
  virtual void RunCommand(CleanCoreContext* coreContext)
  {
    RunCommandImpl(coreContext);
    delete this;
  }
  virtual ~RingCommandItem() {}
private:
  virtual void RunCommandImpl(CleanCoreContext* coreContext) {}
};

struct RingActionCommandItem: public RingCommandItem {
  RingActionCommandItem(): RingCommandItem(RingActionCommandType) {}
  virtual RingCommandItem * clone()const {return new RingActionCommandItem(*this);}
  msg::Actions actions_;
  virtual bool ParseFromArray(void *data , int sz) {
    if (!actions_.ParseFromArray(data, sz))
    {
      return false;
    }
    int sz1 =actions_.actionl4s_size();
    for (int i = 0; i<sz1; ++i)
    {
      actions_.mutable_actionl4s(i)->set_ip(ntohl(actions_.actionl4s(i).ip()) );
    }

    sz1 =actions_.actionl7s_size();
    for (int i = 0; i<sz1; ++i)
    {
      actions_.mutable_actionl7s(i)->set_ip(ntohl(actions_.actionl7s(i).ip()) );
    }

    sz1 =actions_.actionbhl4s_size();
    for (int i = 0; i<sz1; ++i)
    {
      actions_.mutable_actionbhl4s(i)->set_ip(ntohl(actions_.actionbhl4s(i).ip()) );
    }
    sz1 =actions_.actionbhl7s_size();
    for (int i = 0; i<sz1; ++i)
    {
      actions_.mutable_actionbhl7s(i)->set_dstip(ntohl(actions_.actionbhl7s(i).dstip()) );
      actions_.mutable_actionbhl7s(i)->set_srcip(ntohl(actions_.actionbhl7s(i).srcip()) );
    }
  }
  virtual void RunCommandImpl(CleanCoreContext* coreContext)
  {
    size_t sz = actions_.actionl4s_size();

    for (int i = 0; i < sz; ++i) {
      IpCleanItem*p =  coreContext->dstiptable_.find(IpCleanItem(actions_.actionl4s(i).ip()));

      if (p) {
        p->IpCleanEnableTime = actions_.actionl4s(i).expire()/SingletonInstance<CleanGlobalContext>().TimeUnit_;
      }
    }
    sz = actions_.actionl7s_size();

    for (int i = 0; i < sz; ++i) {
      IpCleanItem*p =  coreContext->dstiptable_.find(IpCleanItem(actions_.actionl7s(i).ip()));

      if (p) {
        p->HttpCleanEnableTime = actions_.actionl7s(i).expire()/ SingletonInstance<CleanGlobalContext>().TimeUnit_;
      }
    }

    sz = actions_.actionbhl4s_size();

    for (int i = 0; i < sz; ++i) {
      IpCleanItem*p =  coreContext->dstiptable_.find(IpCleanItem(actions_.actionbhl4s(i).ip()));

      if (p) {
        p->IpBlackExpireTime = actions_.actionbhl4s(i).expire()/ SingletonInstance<CleanGlobalContext>().TimeUnit_;
      }
    }

    sz = actions_.actionbhl7s_size();

    for (int i = 0; i < sz; ++i) {
      HttpCleanItem*p =  coreContext->httptable_.find(HttpCleanItem(actions_.actionbhl7s(i).srcip(), actions_.actionbhl7s(i).dstip()));

      if (p) {
        p->BlackExpireTime = actions_.actionbhl7s(i).expire()/ SingletonInstance<CleanGlobalContext>().TimeUnit_;
      }
    }
  }
};

struct RingHttpCommandItem: public RingCommandItem {

  RingHttpCommandItem(boost::shared_ptr<OutputList<HttpCleanItem> > OutputListPtr): RingCommandItem(RingHttpCommandType), phttptable_(OutputListPtr) {}
  virtual void RunCommandImpl(CleanCoreContext* coreContext) {}
  boost::shared_ptr<OutputList<HttpCleanItem> > phttptable_;
};

enum {
  RingIpCleanItemType = 7,
  RingHttpCleanItemType = 8,
};

enum {
  PBVERSION = 1,
};

struct RingCleanItem {
  virtual ~RingCleanItem() {}
  int type_;
  RingCleanItem(int type): type_(type) {}
  virtual bool needTranverseBack() const = 0;
  virtual RingCommandItem* converttoRingCommandItem() const {return 0;};

  void SerilizeToArray(char * buf, int len)const
  {
    err_msg("version :%d", PBVERSION);
    buf[0] = PBVERSION;
    err_msg("SerilizeToArray type :%d", type_);
    buf[1] = type_;
    err_msg("len :%d", len-4);
    buf[2] = htons(len - 4);
    SerilizeToArrayImpl(buf+4, len - 4);
    //err_msg("len :%d", len-4);
  }

  int getByteSize()const
  {
    return getByteSizeImpl() + 4;
  }
private:
  virtual void SerilizeToArrayImpl(char * buf, int len)const = 0;
  virtual int getByteSizeImpl()const = 0;
};

struct RingHttpCleanItem: public RingCleanItem {
  virtual bool needTranverseBack() const { return true;};
  virtual RingCommandItem* converttoRingCommandItem()
  {
    RingHttpCommandItem * p = new RingHttpCommandItem(phttptable_);
    return p;
  }

  boost::shared_ptr< OutputList<HttpCleanItem>> phttptable_;

  mutable msg::CCStats httpCleanstats_;

  RingHttpCleanItem(PriorityListUnorderSet<HttpCleanItem, PriorityListUnordersetBucketSize>::pool_type & pool)
    : RingCleanItem(RingHttpCleanItemType), phttptable_(new OutputList<HttpCleanItem>(pool))
  {

  }

  virtual void SerilizeToArrayImpl(char * buf, int len)const
  {
    err_msg("Http serilizetoArrayImpl");


    httpCleanstats_.SerializeToArray(buf, len);
  }

  virtual int getByteSizeImpl()const
  {
    httpCleanstats_.set_timeunit(SingletonInstance<CleanGlobalContext>().TimeUnit_);
    if (phttptable_->empty())
    {
      return 0;
    }
    OutputList<HttpCleanItem>::const_reverse_iterator i = phttptable_->crbegin();
    httpCleanstats_.set_timeseq(i->get().TimeSeq+1);
    for (; i != phttptable_->crend(); ++i) {
      err_msg("Http serilizetoArrayImpl One Item");

      msg::CCStat* pHttpCleanStat = httpCleanstats_.add_stats();
      *pHttpCleanStat = i->get().httpCleanItemStat_;
      char ipStr[20];
      int dstip = htonl(pHttpCleanStat->dstip());
      int srcip = htonl(pHttpCleanStat->srcip());
      inet_ntop(AF_INET, &dstip , ipStr, sizeof(ipStr)) ;
      err_msg("dst ip: %s",  ipStr);
      inet_ntop(AF_INET, &srcip , ipStr, sizeof(ipStr)) ;
      err_msg("src ip: %s",  ipStr);
      err_msg("HttpCleanItem: conns %d" , pHttpCleanStat->httpconns());
      err_msg("HttpCleanItem: httpreqs %d " , pHttpCleanStat->httpreqs());
      pHttpCleanStat->set_dstip(htonl(pHttpCleanStat->dstip())) ;
      pHttpCleanStat->set_srcip(htonl(pHttpCleanStat->srcip())) ;

    }
    return httpCleanstats_.ByteSize();
  }
};

struct RingIpCleanItem: public RingCleanItem {
  virtual bool needTranverseBack() const { return false;}
  mutable msg::CleanStats ipcleanstats_;
  virtual int getByteSizeImpl()const
  {
    int sz = ipcleanstats_.stats_size();
    if(sz == 0)
    {
      return 0;
    }
    err_msg("RingIpCleanItem getByteSizeImpl");
    for (int i = 0; i<sz; ++i)
    {
      err_msg("RingIpCleanItem getByteSizeImpl One Item");
      char ipStr[20];
      int dstip = htonl(ipcleanstats_.stats(i).ip());
      inet_ntop(AF_INET, &dstip , ipStr, sizeof(ipStr)) ;
      err_msg("dst ip: %s",  ipStr);
      ipcleanstats_.mutable_stats(i)->set_ip(htonl(ipcleanstats_.stats(i).ip() ));
      err_msg("      inPacks         %ld",ipcleanstats_.stats(i) .inpacks         () );
      err_msg("      inBits          %ld",ipcleanstats_.stats(i) .inbits          () );
      err_msg("      frdPacks        %ld",ipcleanstats_.stats(i) .frdpacks        () );
      err_msg("      frdBits         %ld",ipcleanstats_.stats(i) .frdbits         () );
      err_msg("      dropPacks       %ld",ipcleanstats_.stats(i) .droppacks       () );
      err_msg("      dropBits        %ld",ipcleanstats_.stats(i) .dropbits        () );
      err_msg("      httpReqs        %ld",ipcleanstats_.stats(i) .httpreqs        () );
      err_msg("      httpConns       %ld",ipcleanstats_.stats(i) .httpconns       () );
      err_msg("      blackholeBits   %ld",ipcleanstats_.stats(i) .blackholebits   () );
      err_msg("      blackholePacks  %ld",ipcleanstats_.stats(i) .blackholepacks  () );
      err_msg("      TcpReplyPacks   %ld",ipcleanstats_.stats(i) .tcpreplypacks   () );
      err_msg("      TcpAcceptPacks  %ld",ipcleanstats_.stats(i) .tcpacceptpacks  () );
      err_msg("      TcpDropPacks    %ld",ipcleanstats_.stats(i) .tcpdroppacks    () );
      err_msg("      TcpForwardPacks %ld",ipcleanstats_.stats(i) .tcpforwardpacks () );
      err_msg("      L7DropPacks     %ld",ipcleanstats_.stats(i) .l7droppacks     () );
      err_msg("      L7ForwardPacks  %ld",ipcleanstats_.stats(i) .l7forwardpacks  () );
    }
    return ipcleanstats_.ByteSize();
  }
  void SerilizeToArrayImpl(char * buf, int len)const
  {
    ipcleanstats_.SerializeToArray(buf, len);
  }
  RingIpCleanItem(): RingCleanItem(RingIpCleanItemType) {}
};

enum{
  CleanOutStatsSize = 256,
};

template<class B, class T>
B* createT()
{
  return new T;
}

typedef RingCommandItem* (*RingCommandItemCreateFunc)();

class RingCommandItemCreator
{
  std::map<int, RingCommandItemCreateFunc> creators_;
public:
  RingCommandItemCreator()
  {
    creators_.insert(std::make_pair((const int)RingActionCommandType, &createT<RingCommandItem, RingActionCommandItem>));
  }
  RingCommandItem *create(int t)const
  {
    std::map<int, RingCommandItemCreateFunc>::const_iterator i = creators_.find(t);

    if (i != creators_.end()) {
      return (* i->second)();
    }
  }
};

#endif

