#include <rte_timer.h>
#include <rte_eal.h>
#include <rte_pci.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_launch.h>
#include <rte_errno.h>
#include <rte_cycles.h>
#include <json/json.h>
#include <memory>
#include <string>
#include <vector>
#include "unp.h"
#include "clean_ip_item.h"
#include "clean.h"
#include "dpdk.h"

enum {
  MEMPOOL_CACHE_SIZE = 512,
  MBUF_PAYLOAD_LEN   = 2048,
  PKT_MBUF_SIZE = sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM + MBUF_PAYLOAD_LEN,
};
int jsonstrcmp(const char* l, const char* r)
{
  return strncmp(l + 1, r, strlen(l) - 2);
}
char *  jsonstrncpy(char *dest, const char *src , size_t n)
{
  size_t l = strlen(src);
  strncpy(dest, src + 1, n - 1);

  if (l <= n - 1) {
    dest[l - 2] = 0;
  }

  return dest;
}

void jsonstr2string(std::string* dst, const char *src)
{
  size_t l = strlen(src);
  dst->assign(src + 1, l - 2);
}
enum {BUFSZ = 512};
int ConfigInit(void)
{
  FILE * fp = fopen("./config.json", "r");

  if (!fp) {
    return 1 ;
  }

  char buf [BUFSZ];
  int stringlen = 0;
  enum json_tokener_error jerr;
  struct json_tokener* tok = json_tokener_new();
  struct json_object * configObj = 0;
  memset(&SingletonInstance<DpdkConfig>(), 0 , sizeof(SingletonInstance<DpdkConfig>()));

  do {
    stringlen = fread(buf, 1, BUFSZ, fp);

    if (stringlen < 0) {
      fprintf(stderr, "fread error!");
    }

    configObj = json_tokener_parse_ex(tok, buf, stringlen);
  } while ((jerr = json_tokener_get_error(tok)) == json_tokener_continue && stringlen > 0);

  if (json_object_is_type(configObj, json_type_object)) {
    char *key; struct json_object *val; struct lh_entry *entry;

    for (entry = json_object_get_object(configObj)->head; (entry ? (key = (char*)entry->k, val = (struct json_object*)entry->v, entry) : 0); entry = entry->next) {
      if (strcmp(key, "Log") == 0 && json_object_is_type(val, json_type_object)) {
        char *key1; struct json_object *val1; struct lh_entry *entry1;

        for (entry1 = json_object_get_object(val)->head; (entry1 ? (key1 = (char*)entry1->k, val1 = (struct json_object*)entry1->v, entry1) : 0); entry1 = entry1->next) {
          if (strcmp(key1, "LogFile") == 0 && json_object_is_type(val1, json_type_string)) {
            jsonstr2string(&SingletonInstance<DpdkConfig>().Log.LogFile,  json_object_to_json_string(val1));

            if (openLogFile(SingletonInstance<DpdkConfig>().Log.LogFile.c_str()) != 0) {
              fprintf(stderr, "openLogFile Fail");
            }
          } else if (strcmp(key1, "LogLevel") == 0 && json_object_is_type(val1, json_type_string)) {
            const char* logLevel = json_object_to_json_string(val1);
            SingletonInstance<DpdkConfig>().Log.LogLevel = RTE_LOG_INFO;

            if (jsonstrcmp(logLevel, "EMERG") == 0) {
              SingletonInstance<DpdkConfig>().Log.LogLevel = RTE_LOG_EMERG;
            } else if (jsonstrcmp(logLevel, "ALERT") == 0) {
              SingletonInstance<DpdkConfig>().Log.LogLevel = RTE_LOG_ALERT;
            } else if (jsonstrcmp(logLevel, "CRIT") == 0) {
              SingletonInstance<DpdkConfig>().Log.LogLevel = RTE_LOG_CRIT;
            } else if (jsonstrcmp(logLevel, "ERR") == 0) {
              SingletonInstance<DpdkConfig>().Log.LogLevel = RTE_LOG_ERR;
            } else if (jsonstrcmp(logLevel, "WARNING") == 0) {
              SingletonInstance<DpdkConfig>().Log.LogLevel = RTE_LOG_WARNING;
            } else if (jsonstrcmp(logLevel, "NOTICE") == 0) {
              SingletonInstance<DpdkConfig>().Log.LogLevel = RTE_LOG_NOTICE;
            } else if (jsonstrcmp(logLevel, "INFO") == 0) {
              SingletonInstance<DpdkConfig>().Log.LogLevel = RTE_LOG_INFO;
            } else if (jsonstrcmp(logLevel, "DEBUG") == 0) {
              SingletonInstance<DpdkConfig>().Log.LogLevel = RTE_LOG_DEBUG;
            }

            setLogLevel(SingletonInstance<DpdkConfig>().Log.LogLevel);
          }
        }
      } else if (strcmp(key, "CoreList") == 0 && json_object_is_type(val, json_type_string)) {
        jsonstr2string(&SingletonInstance<DpdkConfig>().CoreList,  json_object_to_json_string(val));
      } else if (strcmp(key, "MemChannel") == 0 && json_object_is_type(val, json_type_string)) {
        jsonstr2string(&SingletonInstance<DpdkConfig>().MemChannelNum,  json_object_to_json_string(val));
      } else if (strcmp(key, "NumaEnabled") == 0 &&  json_object_is_type(val, json_type_int)) {
        SingletonInstance<DpdkConfig>().NumaEnabled = json_object_get_int(val);
      } else if (strcmp(key, "PromiscuousEnabled") == 0 && json_object_is_type(val, json_type_int)) {
        SingletonInstance<DpdkConfig>().PromiscuousEnabled = json_object_get_int(val);
      } else if (strcmp(key, "RxRingSize") == 0 && json_object_is_type(val, json_type_int)) {
        SingletonInstance<DpdkConfig>().RxRingSize = json_object_get_int(val);
      } else if (strcmp(key, "TxRingSize") == 0 && json_object_is_type(val, json_type_int)) {
        SingletonInstance<DpdkConfig>().TxRingSize = json_object_get_int(val);
      } else if (strcmp(key, "RxBurst") == 0 && json_object_is_type(val, json_type_int)) {
        SingletonInstance<DpdkConfig>().RxBurst = json_object_get_int(val);
      } else if (strcmp(key, "TxBurst") == 0 && json_object_is_type(val, json_type_int)) {
        SingletonInstance<DpdkConfig>().TxBurst = json_object_get_int(val);
      } else if (strcmp(key, "MemPoolCacheSize") == 0 && json_object_is_type(val, json_type_int)) {
        SingletonInstance<DpdkConfig>().MemPoolCacheSize = json_object_get_int(val);
      } else if (strcmp(key, "Devices") == 0 && json_object_is_type(val, json_type_array)) {
        int arraylen = json_object_array_length(val);
        int i = 0;
        SingletonInstance<DpdkConfig>().Devices.resize(arraylen);

        for (; i < arraylen; ++i) {
          struct json_object * pobj = json_object_array_get_idx(val, i);

          if (json_object_is_type(pobj, json_type_object)) {
            char *key1; struct json_object *val1; struct lh_entry *entry1;

            for (entry1 = json_object_get_object(pobj)->head; (entry1 ? (key1 = (char*)entry1->k, val1 = (struct json_object*)entry1->v, entry1) : 0); entry1 = entry1->next) {
              if (strcmp(key1, "Name") == 0 && json_object_is_type(val1, json_type_string)) {
                jsonstr2string(& SingletonInstance<DpdkConfig>().Devices[i].Name , json_object_to_json_string(val1));
              } else if (strcmp(key1, "Mac") == 0 && json_object_is_type(val1, json_type_string)) {
                jsonstr2string(& SingletonInstance<DpdkConfig>().Devices[i].Mac , json_object_to_json_string(val1));
              } else if (strcmp(key1, "RxQueueNum") == 0 && json_object_is_type(val1, json_type_int)) {
                SingletonInstance<DpdkConfig>().Devices[i].RxQueueNum = json_object_get_int(val1);
              } else if (strcmp(key1, "TxQueueNum") == 0 && json_object_is_type(val1, json_type_int)) {
                SingletonInstance<DpdkConfig>().Devices[i].TxQueueNum = json_object_get_int(val1);
              }
            }
          }
        }
      } else if (strcmp(key, "Clean") == 0 && json_object_is_type(val, json_type_object)) {
        char *key1; struct json_object *val1; struct lh_entry *entry1;

        for (entry1 = json_object_get_object(val)->head; (entry1 ? (key1 = (char*)entry1->k, val1 = (struct json_object*)entry1->v, entry1) : 0); entry1 = entry1->next) {
          if (strcmp(key1, "TimeUnit") == 0 && json_object_is_type(val1, json_type_int)) {
            SingletonInstance<DpdkConfig>().Clean.TimeUnit = json_object_get_int(val1);
          } else if (strcmp(key1, "SynTimeLoopNum") == 0 && json_object_is_type(val1, json_type_int)) {
            SingletonInstance<DpdkConfig>().Clean.SynTimeLoopNum = json_object_get_int(val1);
          } else if (strcmp(key1, "RecvBatchNum") == 0 && json_object_is_type(val1, json_type_int)) {
            SingletonInstance<DpdkConfig>().Clean.RecvBatchNum = json_object_get_int(val1);
          } else if (strcmp(key1, "CheckSynBatchNum") == 0 && json_object_is_type(val1, json_type_int)) {
            SingletonInstance<DpdkConfig>().Clean.CheckSynBatchNum = json_object_get_int(val1);
          } else if (strcmp(key1, "CheckAckBatchNum") == 0 && json_object_is_type(val1, json_type_int)) {
            SingletonInstance<DpdkConfig>().Clean.CheckAckBatchNum = json_object_get_int(val1);
          } else if (strcmp(key1, "HttpStatLimit") == 0 && json_object_is_type(val1, json_type_int)) {
            SingletonInstance<DpdkConfig>().Clean.HttpStatLimit = json_object_get_int(val1);
          } else if (strcmp(key1, "IpStoreSize") == 0 && json_object_is_type(val1, json_type_int)) {
            SingletonInstance<DpdkConfig>().Clean.IpStoreSize = json_object_get_int(val1);
          } else if (strcmp(key1, "GrayIpStoreSize") == 0 && json_object_is_type(val1, json_type_int)) {
            SingletonInstance<DpdkConfig>().Clean.GrayIpStoreSize = json_object_get_int(val1);
          } else if (strcmp(key1, "HttpBanStoreSize") == 0 && json_object_is_type(val1, json_type_int)) {
            SingletonInstance<DpdkConfig>().Clean.HttpBanStoreSize = json_object_get_int(val1);
          } else if (strcmp(key1, "PipePoolSize") == 0 && json_object_is_type(val1, json_type_int)) {
            SingletonInstance<DpdkConfig>().Clean.PipePoolSize = json_object_get_int(val1);
          } else if (strcmp(key1, "PipeMsgSize") == 0 && json_object_is_type(val1, json_type_int)) {
            SingletonInstance<DpdkConfig>().Clean.PipeMsgSize = json_object_get_int(val1);
          } else if (strcmp(key1, "PipeBufferSize") == 0 && json_object_is_type(val1, json_type_int)) {
            SingletonInstance<DpdkConfig>().Clean.PipeBufferSize = json_object_get_int(val1);
          }
        }
      } else if (strcmp(key, "CleanCores") == 0 && json_object_is_type(val, json_type_array)) {
        int arraylen = json_object_array_length(val);
        int i = 0;
        SingletonInstance<DpdkConfig>().CleanCores.resize(arraylen);

        for (; i < arraylen; ++i) {
          struct json_object * pobj = json_object_array_get_idx(val, i);

          if (json_object_is_type(pobj, json_type_object)) {
            char *key1; struct json_object *val1; struct lh_entry *entry1;

            for (entry1 = json_object_get_object(pobj)->head; (entry1 ? (key1 = (char*)entry1->k, val1 = (struct json_object*)entry1->v, entry1) : 0); entry1 = entry1->next) {
              if (strcmp(key1, "CoreId") == 0 && json_object_is_type(val1, json_type_int)) {
                SingletonInstance<DpdkConfig>().CleanCores.back().CoreId = json_object_get_int(val1);
              } else if (strcmp(key1, "RxDevice") == 0 && json_object_is_type(val1, json_type_string)) {
                jsonstr2string(& SingletonInstance<DpdkConfig>().CleanCores.back().RxDevice , json_object_to_json_string(val1));
              } else if (strcmp(key1, "RxQueue") == 0 && json_object_is_type(val1, json_type_int)) {
                SingletonInstance<DpdkConfig>().CleanCores.back().RxQueue = json_object_get_int(val1);
              } else if (strcmp(key1, "ReplyTxDevice") == 0 && json_object_is_type(val1, json_type_string)) {
                jsonstr2string(& SingletonInstance<DpdkConfig>().CleanCores.back().ReplyTxDevice , json_object_to_json_string(val1));
              } else if (strcmp(key1, "ReplyTxQueue") == 0 && json_object_is_type(val1, json_type_int)) {
                SingletonInstance<DpdkConfig>().CleanCores.back().ReplyTxQueue = json_object_get_int(val1);
              } else if (strcmp(key1, "ForwardTxDevice") == 0 && json_object_is_type(val1, json_type_string)) {
                jsonstr2string(& SingletonInstance<DpdkConfig>().CleanCores.back().ForwardTxDevice , json_object_to_json_string(val1));
              } else if (strcmp(key1, "ForwardTxQueue") == 0 && json_object_is_type(val1, json_type_int)) {
                SingletonInstance<DpdkConfig>().CleanCores.back().ForwardTxQueue = json_object_get_int(val1);
              }
            }
          }
        }
      } else if (strcmp(key, "IpRanges") == 0 && json_object_is_type(val, json_type_array)) {
        int arraylen = json_object_array_length(val);
        int i = 0;

        for (; i < arraylen; ++i) {
          struct json_object * pobj = json_object_array_get_idx(val, i);

          if (json_object_is_type(pobj, json_type_string)) {
        SingletonInstance<DpdkConfig>().ipranges.push_back("");
            jsonstr2string(&SingletonInstance<DpdkConfig>().ipranges[i], json_object_to_json_string(pobj));
          }
        }

      }
    }
  }

  fclose(fp);
  json_object_put(configObj);
  json_tokener_free(tok);
  return 0;
}

static void
timer0_cb(__attribute__((unused)) struct rte_timer *tim,
          __attribute__((unused)) void *arg)
{
  rte_atomic64_inc(&SingletonInstance<rte_atomic64_t>());
  //err_msg("time0_cb %ld", rte_atomic64_read(&SingletonInstance<rte_atomic64_t>()));
}


static void initTimer(struct rte_timer* timer0)
{
  rte_timer_subsystem_init();
  /* init timer structures */
  rte_timer_init(timer0);
  /* load timer0, every second, on master lcore, reloaded automatically */
  uint64_t hz = rte_get_timer_hz();
  unsigned lcore_id = rte_lcore_id();
  rte_atomic64_init(&SingletonInstance<rte_atomic64_t>());
  struct timeval tv;
  int r = gettimeofday(&tv, 0);

  if (r != 0) {
    err_msg("initTime, gettimeofday failed!");
    return;
  }

  rte_atomic64_set(&SingletonInstance<rte_atomic64_t>(), tv.tv_sec / SingletonInstance<CleanGlobalContext>().TimeUnit_);
  rte_timer_reset(timer0, hz * SingletonInstance<CleanGlobalContext>().TimeUnit_ , PERIODICAL, lcore_id, timer0_cb, NULL);
}

static struct rte_mempool *PktMbufPool[DPDK_MAX_SOCKET_NUM];

static int
initEalArgs(char ealArgs[][DPDK_MAX_EAL_ARG_LENGTH + 1], char *ealArgPtrs[])
{
  int n = 0;
  int i = 0;
  strncpy(ealArgs[n++], "EXEC_NAME_PADDING", DPDK_MAX_EAL_ARG_LENGTH);
  strncpy(ealArgs[n++], "-l", DPDK_MAX_EAL_ARG_LENGTH);
  strncpy(ealArgs[n++], SingletonInstance<DpdkContext>().CoreList, DPDK_MAX_EAL_ARG_LENGTH);
  strncpy(ealArgs[n++], "-n", DPDK_MAX_EAL_ARG_LENGTH);
  strncpy(ealArgs[n++], SingletonInstance<DpdkContext>().MemChannelNum, DPDK_MAX_EAL_ARG_LENGTH);

  for (i = 0; i < n; i++) {
    ealArgPtrs[i] = ealArgs[i];
  }

  return n;
}

void DpdkEalInit()
{
  char ealArgs[DPDK_MAX_EAL_ARG_NUM][DPDK_MAX_EAL_ARG_LENGTH + 1];
  char* ealArgPtrs[DPDK_MAX_EAL_ARG_NUM];
  int ealArgNum = 0;
  int rt = 0;
  ealArgNum = initEalArgs(ealArgs, ealArgPtrs);
  rt = rte_eal_init(ealArgNum, ealArgPtrs);

  if (rt < 0) {
    rte_exit(EXIT_FAILURE, "failed to init EAL\n");
  }
}

static uint32_t getPktMbufPoolSize()
{
  uint32_t total = 0;
  uint32_t coreNum = 0;
  int i = 0;
  coreNum = rte_lcore_count();
  total += coreNum * SingletonInstance<DpdkContext>().MemPoolCacheSize;

  for (i = 0; i < SingletonInstance<DpdkContext>().PortNum; i++) {
    total += SingletonInstance<DpdkContext>().Ports[i].RxQueueNum * SingletonInstance<DpdkContext>().RxRingSize;
    total += SingletonInstance<DpdkContext>().Ports[i].TxQueueNum * SingletonInstance<DpdkContext>().TxRingSize;
    total += SingletonInstance<DpdkContext>().Ports[i].RxQueueNum * SingletonInstance<DpdkContext>().RxBurst;
    total += SingletonInstance<DpdkContext>().Ports[i].TxQueueNum * SingletonInstance<DpdkContext>().TxBurst;
  }

  total = RTE_MAX(total, (uint32_t)8192);
  return total;
}

static void initMemory()
{
  int socketId;
  int coreId;
  char s[64];
  int pktBufPoolSize;
  pktBufPoolSize = getPktMbufPoolSize();

  for (coreId = 0; coreId < RTE_MAX_LCORE; coreId++) {
    if (!rte_lcore_is_enabled(coreId)) {
      continue;
    }

    if (SingletonInstance<DpdkContext>().NumaEnabled) {
      socketId = rte_lcore_to_socket_id(coreId);
    } else {
      socketId = 0;
    }

    if (!PktMbufPool[socketId]) {
      err_msg("create buf pool of socket: %d\n", socketId);
      PktMbufPool[socketId] = rte_mempool_create(s, pktBufPoolSize, PKT_MBUF_SIZE, MEMPOOL_CACHE_SIZE,
                              sizeof(struct rte_pktmbuf_pool_private),
                              rte_pktmbuf_pool_init, NULL,
                              rte_pktmbuf_init, NULL,
                              socketId, 0);

      if (!PktMbufPool[socketId]) {
        rte_exit(EXIT_FAILURE, "failed to init pkt mbuf pool on socket %d\n", socketId);
      } else {
        //to log
      }
    }
  }
}

void initPortConf(struct rte_eth_conf* pRteEthConf)
{
  pRteEthConf->rxmode.split_hdr_size = 0;
  pRteEthConf->rxmode.header_split   = 0; /**< Header Split disabled */
  pRteEthConf->rxmode.hw_ip_checksum = 0; /**< IP checksum offload disable */
  pRteEthConf->rxmode.hw_vlan_filter = 0; /**< VLAN filtering disabled */
  pRteEthConf->rxmode.jumbo_frame    = 0; /**< Jumbo Frame Support disable */
  pRteEthConf->rxmode.hw_strip_crc   = 0; /**< CRC stripped by hardware */
  pRteEthConf->txmode.mq_mode   = ETH_MQ_TX_NONE; /**< CRC stripped by hardware */
  //pRteEthConf->rxmode.mq_mode        = ETH_MQ_RX_RSS;
  pRteEthConf->rxmode.max_rx_pkt_len = ETHER_MAX_LEN;
  //pRteEthConf->rxmode.split_hdr_size = 0;
  //pRteEthConf->rxmode.header_split   = 0; /**< Header Split disabled */
  //pRteEthConf->rxmode.hw_ip_checksum = 1; /**< IP checksum offload enabled */
  //pRteEthConf->rxmode.hw_vlan_filter = 0; /**< VLAN filtering disabled */
  //pRteEthConf->rxmode.jumbo_frame    = 0; /**< Jumbo Frame Support disable */
  //pRteEthConf->rxmode.hw_strip_crc   = 0; /**< CRC stripped by hardware */
  //pRteEthConf->rx_adv_conf.rss_conf.rss_key    = NULL;
  //pRteEthConf->rx_adv_conf.rss_conf.rss_hf    = ETH_RSS_IPV4|ETH_RSS_IPV6;
  //pRteEthConf->txmode.mq_mode   = ETH_MQ_TX_NONE; /**< CRC stripped by hardware */
}
int GetEthDevSocketId(uint8_t port)
{
  if (SingletonInstance<DpdkContext>().NumaEnabled) {
    return rte_eth_dev_socket_id(port);
  } else {
    return 0;
  }
}
static void PortInit(uint8_t port, struct DpdkPort *pPortCfg)
{
  struct rte_eth_conf tpPortConf;
  initPortConf(&tpPortConf);
  int rt;
  int i;

  if (port >= rte_eth_dev_count()) {
    rte_exit(EXIT_FAILURE, "PortInit, port id error: %d\n", port);
  }

  err_msg("setupNicDevice PortId: %d, RxQueue: %d, TxQueue: %d\n", port, pPortCfg->RxQueueNum, pPortCfg->TxQueueNum);
  rt = rte_eth_dev_configure(port, pPortCfg->RxQueueNum, pPortCfg->TxQueueNum, &tpPortConf);

  if (rt != 0) {
    rte_exit(EXIT_FAILURE, "rte_eth_dev_configure failed: err=%d, port=%d\n", rt, port);
  }

  int socketId = GetEthDevSocketId(port);

  for (i = 0; i < pPortCfg->TxQueueNum; i++) {
    rt = rte_eth_tx_queue_setup(port, i, SingletonInstance<DpdkContext>().TxRingSize, socketId, NULL);

    if (rt < 0) {
      rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: err=%d, port=%d\n", rt, port);
    }
  }

  for (i = 0; i < pPortCfg->RxQueueNum; i++) {
    rt = rte_eth_rx_queue_setup(port, i, SingletonInstance<DpdkContext>().RxRingSize, socketId, NULL, PktMbufPool[0]);

    if (rt < 0) {
      rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup: err=%d, port=%d\n", rt, port);
    }
  }

  rt = rte_eth_dev_start(port);

  if (rt < 0) {
    rte_exit(EXIT_FAILURE, "rte_eth_dev_start: err=%d, port=%d, errmsg:%s\n", rt, port, rte_strerror(rt));
  }

  if (SingletonInstance<DpdkContext>().Ports[port].PromiscuousEnabled) {
    rte_eth_promiscuous_enable(port);
  }
}

static void PortInits()
{
  uint8_t i;

  for (i = 0; i < SingletonInstance<DpdkContext>().PortNum; i++) {
    err_msg("setup port %d\n", i);
    PortInit(i, &(SingletonInstance<DpdkContext>().Ports[i]));
    err_msg("setup port %d complete\n", i);
  }
}

/* main processing loop */
static int mainLoop(__attribute__((unused)) void *dummy)
{
  err_msg("mainLoop");

  if (SingletonInstance<DpdkContext>().CorePtrs[rte_lcore_id()]) {
    err_msg("mainLoop run");
    SingletonInstance<DpdkContext>().CorePtrs[rte_lcore_id()]->Func();
  }
}


struct rte_ring* globalRing()
{
  static rte_ring* p = 0;

  if (p == 0) {
    p = rte_ring_create("globalring", 1024, SOCKET_ID_ANY, RING_F_SC_DEQ);

    if (!p) {
      err_msg("create global ring  fail!, %s", rte_strerror(rte_errno));
    } else {
      err_msg("create global ring  success!");
    }
  }

  return p;
}

void DpdkStart()
{
  //init memory
  err_msg("init memory...\n");

  initMemory();
  globalRing();
  struct rte_timer timer0;
  initTimer(&timer0);
  //start nic device
  err_msg("start nic devices...\n");
  PortInits();
  rte_eal_mp_remote_launch(mainLoop, NULL, SKIP_MASTER);
  int coreId;
  runUnixDomainClient();
  RTE_LCORE_FOREACH_SLAVE(coreId) {
    if (rte_eal_wait_lcore(coreId) < 0) {
      return ;
    }
  }
}

bool ReadWorkThread::readWorkThread(std::string *pArray)
{
  ++i_;

  if (i_ == DPDK_MAX_CORE_NUM) {
    i_ = 0;
  }

  if (!pArray) {
    err_msg("ReadWorkThread::readWorkThread threadId:%d pArray is Null!", i_);
    return false;
  }

  if (SingletonInstance<DpdkContext>().CorePtrs[i_ % DPDK_MAX_CORE_NUM]) {
    RingCleanItem* p = 0;
    int n = rte_ring_dequeue(globalRing(), (void**)&p);

    //err_msg("Main thread rte_ring_dequeue, thread id: %d!", i_ );
    if (n == 0 && p) {
      err_msg("Main thread rte_ring_dequeue success!");
      std::auto_ptr<RingCleanItem> RingCleanItemPtr(p);
      pArray->resize(p->getByteSize());
      p->SerilizeToArray(&((*pArray)[0]), pArray->size());

      if (p->needTranverseBack()) {
        RingCommandItem* pRingCommandItem = p->converttoRingCommandItem();
        n = rte_ring_enqueue(SingletonInstance<DpdkContext>().CorePtrs[i_ % DPDK_MAX_CORE_NUM]->inring_, pRingCommandItem);

        if (n != 0) {
          std::auto_ptr<RingCommandItem> temp(pRingCommandItem);
          err_msg("launchApp rte_ring_enqueue fail, %s", rte_strerror(rte_errno));
        }
      }

      return true;
    } else {
      //err_msg("LaunchApp rte_ring_deque: no object");
    }
  }

  return false;
}

void sendtoWorkerThreads(char* buf, int len)
{
  //Read from domain socket
  msg::Actions actions;

  if (len < 4) {
    return ;
  }

  int t = buf[0];
  RingCommandItem* pRingCommandItem = SingletonInstance<RingCommandItemCreator>().create(t);
  int r = pRingCommandItem->ParseFromArray((char*)buf + 4, len - 4);

  if (r) {
    bool firstItem = true;
    RingCommandItem* pRingCommandItemCopy = 0;

    for (int i = 0; i < DPDK_MAX_CORE_NUM; ++i) {
      if (SingletonInstance<DpdkContext>().CorePtrs[i]) {
        if (firstItem) {
          pRingCommandItemCopy = pRingCommandItem;
          firstItem = false;
        } else {
          pRingCommandItemCopy = pRingCommandItem->clone();
        }

        if (pRingCommandItemCopy) {
          int n = rte_ring_enqueue(SingletonInstance<DpdkContext>().CorePtrs[i]->inring_, pRingCommandItemCopy);

          if (n != 0) {
            err_msg("launchApp rte_ring_enqueue fail, %s", rte_strerror(rte_errno));
            std::auto_ptr<RingCommandItem>temp(pRingCommandItemCopy);
          }
        }
      }
    }
  }
}
