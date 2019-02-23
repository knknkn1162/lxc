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

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)
int main(void) {
  pid_t pid = fork();

  // parent
  if(pid) {
    sleep(5);
    exit(EXIT_SUCCESS);
  // child
  } else {
    if (prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0)) {
      return -1;
    }
    while(1) { sleep(1); fprintf(stderr, "."); }
    exit(EXIT_SUCCESS);
  }
}
