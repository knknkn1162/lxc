#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/param.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pty.h>
#include <fcntl.h>
#include <termios.h>

/* A simple error-handling function: print an error message based
   on the value in 'errno' and terminate the calling process */

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

int main(void) {
  struct stat buf;
  int masterFd;
  int slaveFd;
  char name[MAXPATHLEN];
  // O_NOCTTY: don't assign controlling terminal
  // or you can use `posix_openpt(3)`.
  if((masterFd = open("/dev/ptmx", O_RDONLY | O_NOCTTY)) == -1) {
    errExit("open /dev/ptmx");
  }
  // to open slave device, we should exec the psuedoterminal related functions; `grantpt(3)` & `unlockpt(3)`.
  if (grantpt(masterFd) == -1 || unlockpt(masterFd) == -1) {
    close(masterFd);
    errExit("grantpt or unlockpt");
  }

  // /dev/pts is mounted in devpts filesystem.
  // cat /proc/self/mountinfo
  // 24 23 0:21 / /dev/pts rw,nosuid,noexec,relatime shared:3 - devpts devpts rw,gid=5,mode=620,ptmxmode=000
  printf("atty: %s, ttyname(master): %s, ptsname(slave): %s\n", isatty(masterFd) ? "true" : "false", ttyname(masterFd), ptsname(masterFd));

  if(slaveFd = open(ptsname(masterFd), O_RDWR) == -1) {
    errExit("slave open");
  }
  close(masterFd); close(slaveFd);

  // Or you can use openpty to get master & slave psuedoterminal descriptor. Note that this function is not POSIX based function.
  // LDFLAGS=-lutil is required to compile.
  openpty(&masterFd, &slaveFd, name, NULL, NULL);

  // get the name of slave device
  printf("allocated pty '%s' (%d/%d)\n", name, masterFd, slaveFd);
  return 0;
}
