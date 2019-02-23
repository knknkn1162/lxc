#define _GNU_SOURCE
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <sys/select.h>
#include <syslog.h>
#include <sys/prctl.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/wait.h>

int main(void)
{
  pid_t pid1;
  pid_t pid2;
  int status;

  if (pid1 = fork()) {
    exit(EXIT_SUCCESS);
  }
  /* child process B */
  if (pid2 = fork()) {
    exit(EXIT_SUCCESS);
  } else if (!pid2) {
    // do sth
    int fd;
    if((fd = open("/dev/tty", O_RDWR) == -1)) {
      perror("open");
    }
    execlp("/bin/bash", "/bin/bash", NULL);
    // never reaches!!
    perror("execlp");
  } else {
          /* error */
  }
}
