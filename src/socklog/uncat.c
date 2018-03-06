#include <unistd.h>
#include <errno.h>
#include <skalibs/strerr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/iopause.h>
#include <skalibs/sig.h>
#include <skalibs/buffer.h>
#include "djb-compat.h"

/* defaults */
#define TIMEOUT 300
#define SIZEMAX 1024

#define USAGE " [-vo] [-t timeout] [-s size] prog"
#define WARNING "uncat: warning: "
#define FATAL "uncat: fatal: "


const char *progname;
int exitasap =0;

void exit_asap() {
  exitasap =1;
}

void usage () {
  strerr_die4x(1, "usage: ", progname, USAGE, "\n");
}

int main (int argc, const char * const *argv, const char * const *envp) {
  int opt;
  unsigned long timeout =TIMEOUT;
  unsigned long sizemax =SIZEMAX;
  int verbose =0;
  int once =0;
  static stralloc sa;
  int eof =0;

  progname =*argv;

  sig_block(SIGTERM);
  sig_catch(SIGTERM, exit_asap);

  while ((opt = sgetopt(argc, (char const *const *)argv, "t:s:voV")) != -1) {
    switch(opt) {
    case 'V':
      strerr_warn1("$Id: uncat.c,v 1.8 2004/02/28 15:52:00 pape Exp $\n", 0);
    case '?':
      usage();
    case 't':
      ulong_scan(optarg, &timeout);
      if (timeout <= 0) timeout =TIMEOUT;
      break;
    case 's':
      ulong_scan(optarg, &sizemax);
      if (sizemax <= 0) sizemax =SIZEMAX;
      break;
    case 'v':
      verbose =1;
      break;
    case 'o':
      once =1;
      break;
    }
  }
  argv +=optind;
  if (!*argv) usage();
  
  if (verbose) strerr_warn2(WARNING, "starting.", 0);

  ndelay_on(0);

  for (;;) {
    tain_t now, deadline;
    int cpipe[2];
    int pid;
    int wstat;

    stralloc_copys(&sa, "");

    /* set timeout */
    tain_now(&now);
    tain_uint(&deadline, timeout);
    tain_add(&deadline, &now, &deadline);

    /* read fd 0, stop at maxsize bytes or timeout */
    for (;;) {
      int r;
      char *s;
      iopause_fd iofd;

      tain_now(&now);
      if (tain_less(&deadline, &now)) {
      	if (verbose && sa.len) strerr_warn2(WARNING, "timeout reached.", 0);
      	break;
      }
      iofd.fd =0;
      iofd.events =IOPAUSE_READ;

      sig_unblock(SIGTERM);
      iopause(&iofd, 1, (tain_t *)&deadline, (tain_t *)&now);
      sig_block(SIGTERM);
      
      if (exitasap) {
      	if (verbose) strerr_warn2(WARNING, "got sigterm.", 0);
      	break;
      }
      
      r = buffer_fill(buffer_0);
      if (r < 0) {
	if (errno == EAGAIN) continue;
	strerr_die2sys(111, FATAL, "unable to read fd 0: ");
      }
      if (r == 0) {
	if (verbose) strerr_warn2(WARNING, "end of input.", 0);
	if (! once) continue;
	eof++;
  	break;
      }
      if ((unsigned int)r >= sizemax) r = sizemax;
      if ((sa.len +r) > sizemax) {
      	if (verbose) strerr_warn2(WARNING, "max size reached.", 0);
      	break;
      }
      s =buffer_peek(buffer_0);
      if (! stralloc_catb(&sa, s, r)) {
	strerr_die2sys(111, FATAL, "out of memory: ");
      }
      buffer_seek(buffer_0, r);
    }
    if (sa.len) {
      /* run prog to process sa.s */
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

	sig_uncatch(SIGTERM);
	sig_unblock(SIGTERM);

	close(cpipe[1]);
	fd_move(0, cpipe[0]);
	fd_copy(1, 2);

	if (verbose) strerr_warn2(WARNING, "starting child.", 0);
	pathexec_run(*argv, argv, envp);
	strerr_die2sys(111, FATAL, "unable to start child: ");
      }
      
      close(cpipe[0]);
      if (fd_write(cpipe[1], sa.s, sa.len) < sa.len) {
	strerr_warn2(WARNING, "unable to write to child: ", strerror(errno));
      }
      close(cpipe[1]);
      
      if (wait_pid(pid, &wstat) != pid) {
	strerr_die2sys(111, FATAL, "wait_pid: ");
      }
      if (wait_crashed(wstat)) {
	strerr_warn2(WARNING, "child crashed.", 0);
      } else {
	if (verbose) strerr_warn2(WARNING, "child exited.", 0);
      }
    }

    if (exitasap || eof) break;
  }

  if (verbose) strerr_warn2(WARNING, "exit.", 0);
  _exit(0);
}
