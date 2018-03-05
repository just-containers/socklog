#ifndef STRERR_SOCKLOG_H
#define STRERR_SOCKLOG_H

#include <errno.h>

struct strerr {
      struct strerr *who;
      const char *x;
      const char *y;
      const char *z;
} ;

extern struct strerr strerr_sys;
extern void strerr_sysinit(void);

#endif
