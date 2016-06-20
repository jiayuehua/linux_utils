#ifndef CLEAN_HTTPH_
#define CLEAN_HTTPH_

#include <rte_tcp.h>

#include "clean.h"
#include "clean_ip_item.h"

typedef struct {
  size_t      len;
  u_char     *data;
} HttpString;

#define HttpStrMacro(str)     { sizeof(str) - 1, (u_char *) str }


int CleanHttpCheck(struct CleanCoreContext *coreContext, struct rte_mbuf *m, IpCleanItem * pIpCleanItem);
#endif
