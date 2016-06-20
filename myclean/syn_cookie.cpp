#include <time.h>
#include <string.h>

#include "syn_cookie.h"
enum {

  COOKIEBITS = 24 , /* Upper bits store count */
  COOKIEMASK = (((uint32_t)1 << COOKIEBITS) - 1),
  MAX_SYNCOOKIE_AGE = 2, //2 minutes // 1min = 64 s
  MSSINDEX = 2,
  SHA_DIGEST_WORDS = 5,
  K1 = 0x5A827999L ,        /* Rounds  0-19: sqrt(2) * 2^30 */
  K2 = 0x6ED9EBA1L ,        /* Rounds 20-39: sqrt(3) * 2^30 */
  K3 = 0x8F1BBCDCL ,        /* Rounds 40-59: sqrt(5) * 2^30 */
  K4 = 0xCA62C1D6L ,        /* Rounds 60-79: sqrt(10) * 2^30 */
  SHA_WORKSPACE_WORDS = 80
};
static uint32_t syncookie_secret[2][16 - 4 + SHA_DIGEST_WORDS] ; //init with random number while starting

void
syncookieInit(void)
{
  struct timespec now;
  int i, j;

  for (i = 0; i < 2; i++) {
    clock_gettime(CLOCK_REALTIME, &now);
    srand(now.tv_nsec);

    for (j = 0; j < sizeof(syncookie_secret[0]) / sizeof(syncookie_secret[0][0]); j++) {
      syncookie_secret[i][j] = (uint32_t)rand();
    }
  }
}

uint32_t f1(uint32_t x, uint32_t y, uint32_t z)
{
  return z ^ (x & (y ^ z)) ;
}

uint32_t f2(uint32_t x, uint32_t y, uint32_t z)
{
  return x ^ y ^ z;
}

uint32_t f3(uint32_t x, uint32_t y, uint32_t z)
{
  return (x & y) + (z & (x ^ y));
}

static inline uint32_t rol32(uint32_t value, unsigned int shift)
{
  return (value << shift) | (value >> (32 - shift));
}

void sha_transform(uint32_t *digest, uint8_t* in, uint32_t *W)
{
  uint32_t a, b, c, d, e, t, i;

  for (i = 0; i < 16; i++) {
    W[i] = rte_be_to_cpu_32(((const uint32_t *)in)[i]);
  }

  for (i = 0; i < 64; i++) {
    W[i + 16] = rol32(W[i + 13] ^ W[i + 8] ^ W[i + 2] ^ W[i], 1);
  }

  a = digest[0];
  b = digest[1];
  c = digest[2];
  d = digest[3];
  e = digest[4];

  for (i = 0; i < 20; i++) {
    t = f1(b, c, d) + K1 + rol32(a, 5) + e + W[i];
    e = d; d = c; c = rol32(b, 30); b = a; a = t;
  }

  for (; i < 40; i++) {
    t = f2(b, c, d) + K2 + rol32(a, 5) + e + W[i];
    e = d; d = c; c = rol32(b, 30); b = a; a = t;
  }

  for (; i < 60; i++) {
    t = f3(b, c, d) + K3 + rol32(a, 5) + e + W[i];
    e = d; d = c; c = rol32(b, 30); b = a; a = t;
  }

  for (; i < 80; i++) {
    t = f2(b, c, d) + K4 + rol32(a, 5) + e + W[i];
    e = d; d = c; c = rol32(b, 30); b = a; a = t;
  }

  digest[0] += a;
  digest[1] += b;
  digest[2] += c;
  digest[3] += d;
  digest[4] += e;
}

static uint32_t cookie_hash(uint32_t saddr, uint32_t daddr, uint16_t sport, uint16_t dport, uint32_t count, int secret)
{
  uint32_t tmp[16 + 5 + SHA_WORKSPACE_WORDS];
  memcpy(tmp + 4, syncookie_secret[secret], sizeof(syncookie_secret[secret]));
  tmp[0] = (uint32_t)saddr;
  tmp[1] = (uint32_t)daddr;
  tmp[2] = ((uint32_t)sport << 16) + (uint32_t)dport;
  tmp[3] = count;
  sha_transform(tmp + 16, (uint8_t *)tmp, tmp + 16 + 5);
  return tmp[17];
}

static inline uint32_t secure_tcp_syn_cookie(uint32_t saddr, uint32_t daddr, uint16_t sport, uint16_t dport, uint32_t sseq, uint32_t count, uint32_t mssind)
{
  //Where sseq is their sequence number and count increases every minute by 1.
  //As an extra hack, we add a small "data" value that encodes the MSS into the second hash value.
  uint32_t HS = cookie_hash(saddr, daddr, sport, dport, 0, 0) + sseq;
  return (HS + (count << COOKIEBITS) + ((cookie_hash(saddr, daddr, sport, dport, count, 1) + mssind) & COOKIEMASK));
}

static inline uint32_t check_tcp_syn_cookie(uint32_t cookie, uint32_t saddr, uint32_t daddr, uint16_t sport, uint16_t dport, uint32_t sseq, uint32_t count)
{
  uint32_t diff;
  uint32_t HS = cookie_hash(saddr, daddr, sport, dport, 0, 0) + sseq;
  cookie -= HS;
  //Cookie is now reduced to (count * 2^24) ^ (hash % 2^24)
  diff = (count - (cookie >> COOKIEBITS)) & ((uint32_t) - 1 >> COOKIEBITS);

  if (diff >= MAX_SYNCOOKIE_AGE) {
    return (uint32_t) - 1;
  }

  return (cookie - cookie_hash(saddr, daddr, sport, dport, count - diff, 1)) & COOKIEMASK;
}

static uint32_t ads_cookie_generate_linux(struct ipv4_hdr *ipv4_hdr, struct tcp_hdr *tcp_hdr)
{
  uint32_t cookie = 0;
  struct timespec res;
  uint32_t jiffies;
  clock_gettime(CLOCK_REALTIME, &res);
  jiffies = res.tv_sec >> 6;
  cookie = secure_tcp_syn_cookie(ipv4_hdr->src_addr, ipv4_hdr->dst_addr, tcp_hdr->src_port, tcp_hdr->dst_port, ntohl(tcp_hdr->sent_seq), jiffies, MSSINDEX);
  return cookie;
}

// 0: success, 1: fail
static uint16_t ads_cookie_validate_linux(struct ipv4_hdr *ipv4_hdr, struct tcp_hdr *tcp_hdr, int ack)
{
  uint32_t cookie;
  uint32_t seq;
  struct timespec res;
  uint32_t jiffies;
  clock_gettime(CLOCK_REALTIME, &res);
  jiffies = res.tv_sec >> 6;

  if (ack) {
    // syncookie + reset
    cookie = ntohl(tcp_hdr->recv_ack) - 1;
    seq = ntohl(tcp_hdr->sent_seq) - 1;
  } else {
    // syncookie + safe reset
    cookie = ntohl(tcp_hdr->sent_seq);
    seq = ntohl(tcp_hdr->sent_seq);
  }

  uint32_t mssind = check_tcp_syn_cookie(cookie, ipv4_hdr->src_addr, ipv4_hdr->dst_addr, tcp_hdr->src_port, tcp_hdr->dst_port, seq, jiffies);

  if (mssind != MSSINDEX) {
    return 1;
  } else {
    return 0;
  }
}

uint32_t SecureTcpSynCookie(struct ipv4_hdr *ipv4Hdr, struct tcp_hdr *tcpHdr)
{
  return ads_cookie_generate_linux(ipv4Hdr, tcpHdr);
}

int SynCookieValidate(struct ipv4_hdr *ipv4Hdr, struct tcp_hdr *tcpHdr)
{
  return ads_cookie_validate_linux(ipv4Hdr, tcpHdr, 1);
}