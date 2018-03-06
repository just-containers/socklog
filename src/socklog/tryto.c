#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <skalibs/strerr.h>
#include <skalibs/djbunix.h>
#include <skalibs/iopause.h>
#include <skalibs/tai.h>
#include <skalibs/sig.h>
#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/error.h>
#include <skalibs/sgetopt.h>
#include <skalibs/gccattributes.h>
#include "djb-compat.h"

/* defaults */
#define TIMEOUT 180
#define KTIMEOUT 5
#define TRYMAX  5

#define USAGE " [-vp] [-t seconds] [-k kseconds] [-n tries] prog"
#define WARNING "tryto: warning: "
#define FATAL "tryto: fatal: "

void usage(void) gccattr_noreturn;

const char *progname;
int selfpipe[2];
int try =0;

void sig_child_handler(int s) {
  (void)(s);
  try++;
  fd_write(selfpipe[1], "", 1);
}

void usage(void) {
  strerr_die4x(1, "usage: ", progname, USAGE, "\n");
}

int main (int argc, const char * const *argv, const char * const *envp) {
  int opt;
  tain_t now, deadline;
  iopause_fd x[2];
  int pid;
  int rc =111;
  unsigned long timeout =TIMEOUT;
  unsigned long ktimeout =KTIMEOUT;
  unsigned long trymax =TRYMAX;
  int verbose =0;
  char ch;
  int processor =0;
  unsigned int pgroup =0;
  int cpipe[2];

  progname =*argv;

  while ((opt = sgetopt(argc,argv,"t:k:n:pPvV")) != -1) {
    switch(opt) {
    case 'V':
      strerr_warn1("$Id: tryto.c,v 1.9 2006/02/26 13:30:37 pape Exp $\n", 0);
      /* fall through */
    case '?':
      usage();
    case 't':
      ulong_scan(optarg, &timeout);
      if (timeout <= 0) timeout =TIMEOUT;
      break;
    case 'k':
      ulong_scan(optarg, &ktimeout);
      if (ktimeout <= 0) ktimeout =KTIMEOUT;
      break;
    case 'n':
      ulong_scan(optarg, &trymax);
      break;
    case 'p':
      processor =1;
      break;
    case 'P':
      pgroup =1;
      break;
    case 'v':
      verbose =1;
      break;
    }
  }
  argv +=optind;
  if (!*argv) usage();

  /* create selfpipe */
  if (pipe(selfpipe) == -1) {
    strerr_die2sys(111, FATAL, "unable to create selfpipe: ");
  }
  coe(selfpipe[0]);
  coe(selfpipe[1]);
  ndelay_on(selfpipe[0]);
  ndelay_on(selfpipe[1]);

  ndelay_on(0);
  if (processor) ndelay_on(4);

  sig_block(SIGPIPE);
  sig_block(SIGCHLD);
  sig_catch(SIGCHLD, sig_child_handler);

  /* set timeout */
  tain_now(&now);
  tain_uint(&deadline, timeout);
  tain_add(&deadline, &now, &deadline);
  timeout =0;

  for (;;) {
    int iopausefds;
    char buffer_x_space[BUFFER_INSIZE];
    buffer buffer_x;

    if (processor) {
      buffer_init(&buffer_x, buffer_read, 4, buffer_x_space,
		  sizeof buffer_x_space);
    } else {
      buffer_init(&buffer_x, buffer_read, 0, buffer_x_space,
		  sizeof buffer_x_space);
    }

    /* start real processor */
    if (pipe(cpipe) == -1) {
      strerr_die2sys(111, FATAL, "unable to create pipe for child: ");
    }
    while ((pid =fork()) == -1) {
      strerr_warn4(WARNING, "unable to fork for \"", *argv, "\" pausing: ",
		   strerror(errno));
      sleep(5);
    }
    if (!pid) {
      /* child */

      sig_unblock(SIGPIPE);
      sig_unblock(SIGCHLD);
      sig_uncatch(SIGCHLD);

      close(cpipe[1]);
      fd_move(0, cpipe[0]);
      if (processor) {
	fd_move(2, 5);
	close(4);
      }
      if (pgroup) setsid();
      pathexec_run(*argv, argv, envp);
      strerr_die2sys(111, FATAL, "unable to start child: ");
    }
    close(cpipe[0]);

    x[0].fd =selfpipe[0];
    x[0].events =IOPAUSE_READ;
    if (processor) {
      fd_move(2, 5);
      x[1].fd =4;
    } else {
      x[1].fd =0;
    }
    x[1].events =IOPAUSE_READ;
    iopausefds =2;

    /* feed + watch child */
    for (;;) {
      int r;
      int i;
      char *s;

      sig_unblock(SIGCHLD);
      iopause(x, iopausefds, &deadline, &now);
      sig_block(SIGCHLD);
      
      while (fd_read(selfpipe[0], &ch, 1) == 1) {}

      tain_now(&now);
      if ((timeout =tain_less(&deadline, &now))) break;
      if (wait_nohang(&rc) == pid) break;
      rc =111;

      r = buffer_feed(&buffer_x);
      if (r < 0) {
	if ((errno == EINTR) || (errno == EAGAIN)) continue;
      }
      if (r == 0) {
	if (processor && (buffer_x.fd == 4)) {
	  x[1].fd =0;
	  buffer_init(&buffer_x, buffer_read, 0, buffer_x_space,
		  sizeof buffer_x_space);
	  continue;
	}
	if (iopausefds == 2) {
	  close(cpipe[1]);
	  iopausefds =1;
	}
	continue;
      }
      s =buffer_peek(&buffer_x);
      i =fd_write(cpipe[1], s, r);
      if (i == -1) strerr_die2sys(111, FATAL, "unable to write to child: ");
      if (i < r)
	strerr_die2x(111, FATAL, "unable to write to child: partial write");

      buffer_seek(&buffer_x, r);
    }
    close(cpipe[1]);

    if (timeout) {
      if (wait_nohang(&rc) == pid) break;
      /* child not finished */
      strerr_warn4(WARNING,
		   "child \"", *argv, "\" timed out. sending TERM...", 0);
      kill(pgroup ? -pid : pid, SIGTERM);

      /* ktimeout sec timeout */
      tain_now(&now);
      tain_uint(&deadline, ktimeout);
      tain_add(&deadline, &now, &deadline);
      ktimeout =0;

      for (;;) {
        sig_unblock(SIGCHLD);
        iopause(x, 1, &deadline, &now);
        sig_block(SIGCHLD);

        while (fd_read(selfpipe[0], &ch, 1) == 1) {}

        if (wait_nohang(&rc) == pid) {
	  strerr_warn2(WARNING, "child terminated.", 0);
	  break;
        }
        rc =111;
        tain_now(&now);
        if ((ktimeout =tain_less(&deadline, &now))) break;
      }
      if (ktimeout) {
        strerr_warn4(WARNING, "child \"", *argv,
                     "\" not terminated. sending KILL...", 0);
        kill(pgroup ? -pid : pid, SIGKILL);
      }
      break;
    }
    if (rc == 0) break;
    if (verbose) strerr_warn2(WARNING, "child crashed.", 0);
    if (lseek(0, 0, SEEK_SET) != 0)
	if (verbose) strerr_warn2(WARNING,
				  "unable to lseek fd 0: ", strerror(errno));
    if ((unsigned long)try >= trymax) break;
    sleep(1);
  }

  if (processor && (rc != 0)) {
    for (;;) {
      int r;
      char *s;

      r = buffer_feed(buffer_0);
      if (r < 0) {
	if ((errno == EINTR) || (errno == EAGAIN)) continue;
      }
      if (r == 0) {
	break;
      }
      s =buffer_peek(buffer_0);
      buffer_putflush(buffer_1, s, r);
      buffer_seek(buffer_0, r);
    }
  }
  if (timeout) {
    if (processor) strerr_die2x(0, FATAL, "child timed out, giving up.");
    strerr_die2x(100, FATAL, "child timed out, giving up.");
  }
  if ((unsigned long)try >= trymax) {
    if (processor) strerr_die2x(0, FATAL, "child crashed, giving up.");
    strerr_die2x(rc >> 8, FATAL, "child crashed, giving up.");
  }
  _exit(0);
}
