#include "memory.h"

#ifdef USE_DPDK_MALLOC
#include <rte_malloc.h>
#include <rte_lcore.h>
void* MALLOC(size_t sz)
{
  return  rte_malloc_socket(NULL, sz, 0, rte_lcore_to_socket_id(rte_lcore_id()));
}

void* CALLOC(size_t sz, size_t itemSz)
{
  return rte_calloc_socket(NULL, sz, itemSz, 0, rte_lcore_to_socket_id(rte_lcore_id()));
}
void FREE (void* p)
{
  return rte_free(p);
}
#else
#include <stdlib.h>
void* MALLOC(size_t sz)
{
  return  malloc(sz);
}

void* CALLOC(size_t sz, size_t itemSz)
{
  return calloc(sz, itemSz);
}
void FREE(void* p)
{
  return free(p);
}
#endif