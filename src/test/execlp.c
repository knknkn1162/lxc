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
  pid_t child_pid;
  if((child_pid = fork()) == -1) {
    errExit("fork");
  // child
  } else if (child_pid == 0) {
    int fd;
    if((fd = open("/dev/tty", O_RDWR)) == -1) {
      errExit("open");
    }
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    execlp("/bin/bash", "/bin/bash", "-i", (char*)NULL);
    // never reaches!!
    perror("execlp");
    exit(1);
  } else {
    //if(wait(NULL) == -1) {
    //  errExit("wait");
    //}
    printf("end child process\n");
  }

  return 0;
}
