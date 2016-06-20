#include <stdint.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <stdio.h>
#include "dpdk.h"
#include "clean.h"
#include "ether.h"
#include "ip.h"
#include "syn_cookie.h"

static void initDpdkPorts()
{
  SingletonInstance<DpdkContext>().PortNum = 1;

  SingletonInstance<DpdkContext>().Ports[0].RxQueueNum = 1;
  SingletonInstance<DpdkContext>().Ports[0].TxQueueNum = 1;
  SingletonInstance<DpdkContext>().Ports[0].PromiscuousEnabled = 0;
}

static void initCleanCore(uint8_t coreId)
{
  uint8_t socketId = rte_lcore_to_socket_id(coreId);
  void *pCleanCoreCtx =   rte_zmalloc_socket(NULL, sizeof(struct CleanCoreContext), 0, socketId);
  if (!pCleanCoreCtx)
  {
    rte_exit(EXIT_FAILURE, "failed to init clean context of core: %d\n", coreId);
  }
  SingletonInstance<DpdkContext>().memoryStorages[coreId].reset(pCleanCoreCtx, &rte_free);
  struct CleanCoreContext *cleanCoreContext =   new(pCleanCoreCtx)CleanCoreContext(coreId);
  if (!cleanCoreContext) {
    rte_exit(EXIT_FAILURE, "failed to init clean context of core: %d\n", coreId);
  }
  //Add IPS
  SingletonInstance<DpdkContext>().CorePtrs[coreId].reset( cleanCoreContext);
}

static void initCleanCores()
{
  initCleanCore(1);
}

int main()
{
  ConfigInit();
  err_msg("first");
  DpdkEalInit();

  err_msg("second");
  initDpdkPorts();

  err_msg("third");
  initCleanCores();
  err_msg("fourth");
  syncookieInit();

  err_msg("firth");
  DpdkStart();

  err_msg("six");
  return 0;
}
