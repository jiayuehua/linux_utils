#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <rte_cycles.h>
#include <rte_timer.h>
#include "unp.h"
#include "dpdk.h"
enum {MAXEVENTS = 1};
enum { TIMER_RESOLUTION_CYCLES = 20000000ULL}; /* around 10ms at 2 Ghz */
int
runUnixDomainClient()
{
  int         sockfd;
  struct sockaddr_un  cliaddr, servaddr;

  err_msg("RununixdomainClient");
  sockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);

  bzero(&cliaddr, sizeof(cliaddr));   /* bind an address for us */
  cliaddr.sun_family = AF_LOCAL;
  //strcpy(cliaddr.sun_path, tmpnam(NULL));
  strcpy(cliaddr.sun_path, "/tmp/unixdgclient8");
  int val = 1;
  unlink(cliaddr.sun_path);


  bind(sockfd, (SA *) &cliaddr, sizeof(cliaddr));

  bzero(&servaddr, sizeof(servaddr)); /* fill in server's address */
  servaddr.sun_family = AF_LOCAL;
  strcpy(servaddr.sun_path, UNIXDG_PATH);
  dg_client( sockfd, (const SA *) &servaddr, sizeof(servaddr));
}

void
dg_client(int sfd, const SA* pservaddr, socklen_t len)
{
  err_msg("master dg_client");
  int var = Fcntl(sfd, F_GETFL, 0);
  Fcntl(sfd, F_SETFL, var | O_NONBLOCK);
  int s;
  int efd;
  struct epoll_event events[MAXEVENTS];
  struct epoll_event event;
  event.data.fd = sfd;
  event.events = EPOLLIN  | EPOLLET;
  efd = epoll_create1(0);

  if (efd == -1) {
    err_sys("epoll_create");
  }

  s = epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);

  if (s == -1) {
    err_sys("epoll_ctl");
  }


  std::string resBuf;
  ReadWorkThread r;
  uint64_t prev_tsc = 0, cur_tsc, diff_tsc;
  for (;;) {
    cur_tsc = rte_rdtsc();
    diff_tsc = cur_tsc - prev_tsc;

  //err_msg("master dg_client cur_tsc %ld", cur_tsc);
    if (diff_tsc > TIMER_RESOLUTION_CYCLES) {
  //err_msg("master dg_client diff_tsc %ld", diff_tsc);
      rte_timer_manage();
      prev_tsc = cur_tsc;
    }
    int n, j;
    n = epoll_wait(efd, events, MAXEVENTS, 0);
    int done = 0;
    int workerThreadId;
    char * buf = 0;
    int len = 0;
    //err_msg("master readworkthread start");
    int ret =  r.readWorkThread(&resBuf);

    //err_msg("master readworkthread end");
    if (ret && !resBuf.empty()) {
    err_msg("master readworkthread success");
      ssize_t count = sendto(sfd, resBuf.c_str(), resBuf.size(), 0, pservaddr, len);

      if (count == -1) {
        if (errno != EAGAIN) {
          err_msg("dg_client sendto fail");
        }
        else{
          err_msg("dg_client sendto fail EAGAIN");
        }
      } else if (count == 0) {
        err_msg("master sendto success");
      }
    }

    for (j = 0; j < n; j++) {
      if ((events[j].events & EPOLLERR) ||
          (events[j].events & EPOLLHUP)
         ) {
        continue;
      } else if (sfd == events[j].data.fd) {
        if (events[j].events & EPOLLIN) {
          int done = 0;

          while (1) {
            ssize_t count;
            char buf[512];
            count = recvfrom(events[j].data.fd, buf, sizeof buf, 0, 0, 0);

            if (count == -1) {
              if (errno != EAGAIN) {
                err_msg("dg_client recvfrom");
              }

              break;
            } else if (count == 0) {
              break;
            }

            sendtoWorkerThreads(buf, count);
          }
        }
      }
    }
  }

  close(sfd);
}
