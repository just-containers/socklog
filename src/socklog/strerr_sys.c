/* Public domain. */

#include <skalibs/error.h>
#include <skalibs/strerr.h>
#include "error.h"
#include "strerr.h"

struct strerr strerr_sys;

void strerr_sysinit(void)
{
  strerr_sys.who = 0;
  strerr_sys.x = error_str(errno);
  strerr_sys.y = "";
  strerr_sys.z = "";
}
