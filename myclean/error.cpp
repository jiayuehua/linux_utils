#include  "unp.h"

#include  <stdarg.h>    /* ANSI C header file */
#include <stdio.h>
#include  <syslog.h>    /* for syslog() */
#ifndef NO_RTE_LOG
#include  <rte_log.h>
enum
{
  L_INFO = RTE_LOG_INFO,
  L_DEBUG = RTE_LOG_DEBUG,
  L_ERR = RTE_LOG_ERR,
};
#else
enum
{
  L_INFO = LOG_INFO,
  L_DEBUG = LOG_DEBUG,
  L_ERR = LOG_ERR,
};
#endif
int   daemon_proc;    /* set nonzero by daemon_init() */

int openLogFile(const char* p)
{
#ifndef NO_RTE_LOG
  FILE *fp = fopen(p, "a+");
  return rte_openlog_stream(fp);
#endif
  return 0;
}
static void err_doit(int, int, const char *, va_list);

/* Nonfatal error related to a system call.
 * Print a message and return. */

/* $$.ix [err_ret]~function,~source~code$$ */
void
err_ret(const char *fmt, ...)
{
  va_list   ap;

  va_start(ap, fmt);
  err_doit(1, L_INFO, fmt, ap);
  va_end(ap);
  return;
}

/* Fatal error related to a system call.
 * Print a message and terminate. */

/* $$.ix [err_sys]~function,~source~code$$ */
void
err_sys(const char *fmt, ...)
{
  va_list   ap;

  va_start(ap, fmt);
  err_doit(1, L_ERR, fmt, ap);
  va_end(ap);
  exit(1);
}

/* Fatal error related to a system call.
 * Print a message, dump core, and terminate. */

/* $$.ix [err_dump]~function,~source~code$$ */
void
err_dump(const char *fmt, ...)
{
  va_list   ap;

  va_start(ap, fmt);
  err_doit(1, L_ERR, fmt, ap);
  va_end(ap);
  abort();    /* dump core and terminate */
  exit(1);    /* shouldn't get here */
}

/* Nonfatal error unrelated to a system call.
 * Print a message and return. */

/* $$.ix [err_msg]~function,~source~code$$ */
void
err_msg(const char *fmt, ...)
{
  va_list   ap;

  va_start(ap, fmt);
  err_doit(0, L_INFO, fmt, ap);
  va_end(ap);
  return;
}

/* Fatal error unrelated to a system call.
 * Print a message and terminate. */

/* $$.ix [err_quit]~function,~source~code$$ */
void
err_quit(const char *fmt, ...)
{
  va_list   ap;

  va_start(ap, fmt);
  err_doit(0, L_ERR, fmt, ap);
  va_end(ap);
  exit(1);
}

/* Print a message and return to caller.
 * Caller specifies "errnoflag" and "level". */

/* $$.ix [err_doit]~function,~source~code$$ */
static void
err_doit(int errnoflag, int level, const char *fmt, va_list ap)
{
  int   errno_save, n;
  char  buf[MAXLINE];

  errno_save = errno;   /* value caller might want printed */
#ifdef  HAVE_VSNPRINTF
  vsnprintf(buf, sizeof(buf), fmt, ap); /* this is safe */
#else
  vsprintf(buf, fmt, ap);         /* this is not safe */
#endif
  n = strlen(buf);
  if (errnoflag)
  {
    snprintf(buf+n, sizeof(buf)-n, ": %s", strerror(errno_save));
  }
  strcat(buf, "\n");

  if (daemon_proc) {
#ifndef NO_RTE_LOG
    rte_log(level, RTE_LOGTYPE_USER1, "USER1: %s", buf);
#else
    syslog(level, buf);
#endif
  } else {
    fflush(stdout);   /* in case stdout and stderr are the same */
    fputs(buf, stderr);
    fflush(stderr);
  }
  return;
}

void setLogLevel(int loglevel)
{
#ifndef NO_RTE_LOG
  rte_set_log_level(loglevel);
#else
  setlogmask(LOG_UPTO(loglevel));
#endif
}
