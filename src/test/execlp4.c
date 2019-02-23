#define _GNU_SOURCE
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

int main(void) {
  int fd;

  if (fork() != 0)
    _exit(EXIT_SUCCESS);

  // falls through child process
  printf("PID=%ld, PGID=%ld, SID=%ld\n", (long) getpid(), (long) getpgrp(), (long) getsid(0));
  //if(setsid() == -1) {
  //  errExit("setsid");
  //}
  //printf("PID=%ld, PGID=%ld, SID=%ld\n", (long) getpid(), (long) getpgrp(), (long) getsid(0));

  //// bash: cannot set terminal process group (-1): Inappropriate ioctl for device
  //// bash: no job control in this shell
  //execlp("/bin/bash", "/bin/bash", "-i", (char*)NULL);
  // never reaches
  return 0;
}
