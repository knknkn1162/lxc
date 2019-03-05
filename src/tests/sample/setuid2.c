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

#define BUF_SIZE 256
#define MAX_SNAME 100

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(void) {
  uid_t uid = getuid();
  printf("uid: %d\n", uid);
  fprintf(stderr, "setuid(%d)\n", uid);
  if(setuid(uid) == -1) {
    errExit("setsid");
  }
  //if(seteuid(0) == -1) {
  //  errExit("seteuid");
  //}
  fprintf(stderr, "setuid(%d)\n", uid + 1);
  // setsid: Operation not permitted without sudo
  if(setuid((uid_t)uid+1) == -1) {
    // 1000
    fprintf(stderr, "geteuid(): %d\n", (int)geteuid());
    errExit("setuid(uid+1)");
  }
  fprintf(stderr, "setuid(0)\n");
  if(setuid(0) == -1) {
    //fprintf(stderr, "geteuid(): %d\n", (int)geteuid()); // 1000
    errExit("setuid(0)");
  }

  return 0;
}
