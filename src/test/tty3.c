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
  int fd = open("/dev/tty", O_RDWR);
  fprintf(stderr, "/dev/tty: %d\n", fd);
  int numRead;
  char buf[BUF_SIZE];
  while(1) {
    // ssize_t read(int fd, void *buf, size_t count);
    if((numRead = read(fd, buf, BUF_SIZE)) <= 0) {
      errExit("read");
    } else {
      fprintf(stderr, "%d\n", numRead);
      //write(STDOUT_FILENO, buf, numRead);
      write(fd, buf, numRead);
    }
  }
  return 0;
}
