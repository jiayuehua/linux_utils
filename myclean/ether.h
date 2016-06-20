#ifndef ETHER_H
#define ETHER_H

#include <stdint.h>

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_common.h>

static inline void MacStrToBytes(const char *str, uint8_t bytes[])
{
  sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]);
}

static inline char* MacBytesToStr(uint8_t *bytes, char *str)
{
  ether_format_addr(str, ETHER_ADDR_FMT_SIZE, (struct ether_addr *) bytes);
  return str;
}



#endif
