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
#include <syslog.h>
#include <sys/socket.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(void) {
  // bidirectional communication.(pipe is uni-directional)
  // int socketpair(int domain, int type, int protocol, int sockfd[2]);
  int sv[2];
  pid_t child_pid;
  socketpair(AF_LOCAL, SOCK_STREAM, 0, sv);

  child_pid = fork();
  if(child_pid == -1) {
    errExit("fork");
  // child
  } else if (child_pid == 0) {
    int num;
    close(sv[0]);
    sv[0] = -1;
    write(sv[1], &(int) { 1 }, sizeof(int));
    if(read(sv[1], &num, sizeof(int)) < 0) {
      errExit("read");
    } else if (num == 0) {
      printf("ok\n");
    }
    exit(EXIT_SUCCESS);
  }
  close(sv[1]); sv[1] = -1;
  int num;
  if(read(sv[0], &num, sizeof(int)) > 0) {
    write(sv[0], &(int) { 0 }, sizeof(int));
  }
  exit(EXIT_SUCCESS);

  return 0;
}
