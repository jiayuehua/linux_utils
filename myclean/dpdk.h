#ifndef DPDK_H_
#define DPDK_H_
#include <boost/shared_array.hpp>
#include <boost/shared_ptr.hpp>
#include <stdint.h>
#include <string>
#include <stddef.h>
#include "memory.h"
enum {
  DPDK_MAX_EAL_ARG_NUM = 32,
  DPDK_MAX_EAL_ARG_LENGTH = 128,
  DPDK_MAX_PORT_NUM = 2,
  DPDK_MAX_CORE_NUM = 4,
  DPDK_MAX_SOCKET_NUM = 2,
  DPDK_MAX_RX_QUEUE_NUM_PER_PORT = 64,
  DPDK_MAX_TX_QUEUE_NUM_PER_PORT = 64,
};

enum {
  CONST_PCI_DEV_ID_LEN = 12,
  CONST_MAX_PORT_NAME_LEN = 32
};
int ConfigInit(void);

struct DpdkConfig {
  struct LogConfig {
    std::string LogFile;
    int  LogLevel;
  } Log;
  struct Device {
    std::string Name;
    std::string Mac;
    int32_t  RxQueueNum;
    int32_t  TxQueueNum;
  };
  std::vector<Device> Devices;
  struct SClean {
    int32_t TimeUnit ;
    int32_t SynTimeLoopNum ;
    int32_t RecvBatchNum ;
    int32_t CheckSynBatchNum ;
    int32_t CheckAckBatchNum ;
    int32_t HttpStatLimit ;
    int32_t IpStoreSize ;
    int32_t GrayIpStoreSize;
    int32_t HttpBanStoreSize;
    int32_t PipePoolSize;
    int32_t PipeMsgSize;
    int32_t PipeBufferSize;
  } Clean;

  struct CleanCore {
    int32_t CoreId;
    std::string RxDevice ;
    int32_t RxQueue ;
    std::string ReplyTxDevice ;
    int32_t ReplyTxQueue ;
    std::string ForwardTxDevice ;
    int32_t ForwardTxQueue ;
  };
  std::vector <CleanCore> CleanCores;
  std::string CoreList;
  std::string MemChannelNum;

  bool  NumaEnabled;
  bool  PromiscuousEnabled;
  uint16_t RxRingSize;
  uint16_t TxRingSize;
  uint16_t RxBurst;
  uint16_t TxBurst;
  uint16_t MemPoolCacheSize;
  std::string LocalUdsPath;
  std::string AgentUdsPath;
  std::vector<std::string> ipranges;
};
struct DpdkPort {
  char Name[CONST_MAX_PORT_NAME_LEN + 1];
  char PciDevId[CONST_PCI_DEV_ID_LEN + 1];
  int RxQueueNum;
  int TxQueueNum;
  uint8_t PromiscuousEnabled;
};

struct CleanCoreContext;
typedef void (*DpdkCoreFunc)(CleanCoreContext *);

struct DpdkContext {
  DpdkContext(): NumaEnabled ( 0), RxRingSize ( 2048), TxRingSize ( 2048), RxBurst ( 2048), TxBurst ( 256), MemPoolCacheSize ( 512)
  {
    strncpy(CoreList, "0-1", sizeof(CoreList));
    strncpy(MemChannelNum, "1", sizeof(MemChannelNum));
    memset(CorePtrs, 0, sizeof CorePtrs);
  }

  /*dpdk related config*/
  char CoreList[DPDK_MAX_EAL_ARG_LENGTH+1];
  char MemChannelNum[DPDK_MAX_EAL_ARG_LENGTH+1];
  uint8_t NumaEnabled;
  uint16_t RxRingSize;
  uint16_t TxRingSize;
  uint16_t RxBurst;
  uint16_t TxBurst;
  uint16_t MemPoolCacheSize;

  struct DpdkPort Ports[DPDK_MAX_PORT_NUM];
  uint8_t PortNum;
  /*core thread config*/
  boost::shared_ptr<void> memoryStorages[DPDK_MAX_CORE_NUM];
  boost::shared_ptr<CleanCoreContext> CorePtrs[DPDK_MAX_CORE_NUM];
};

void DpdkEalInit();
void initPortConf(struct rte_eth_conf* pRteEthConf);
void DpdkStart();

void sendtoWorkerThreads(char* buf, int len);
class ReadWorkThread
{
  int i_;
public:
  ReadWorkThread(): i_(0){}
  bool readWorkThread(std::string* p);
};
int runUnixDomainClient();
int openLogFile(const char* p);
void setLogLevel(int loglevel);
#endif

