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
#include <sys/epoll.h>
#include <syslog.h>

#define BUF_SIZE 256
#define MAX_SNAME 100
#define MAX_EVENTS 10

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct epoll_handler {
  int fd;
};

int main(void) {
  int epfd;
  int nfds;
  // struct epoll_event { uint32_t events; epoll_data_t data; }
  struct epoll_event events[MAX_EVENTS];
  fd_set inFds;
  int numRead;
  uint32_t ev_option = EPOLLIN | EPOLLET;
  //
  // lxc_main_open
  if((epfd = epoll_create(4)) == -1) {
    errExit("epoll_create");
  }


  // register ttyFd to epfd
  {
    int ttyFd = open("/dev/tty", O_RDWR);
    struct epoll_event ev;
    struct epoll_handler data = { ttyFd };
    // By default, the epoll mechanism provides level-triggered notification.
    ev.events = ev_option;
    ev.data.ptr = &data;
    //ev.data.fd = ttyFd;
    // Input to
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, ttyFd, &ev) == -1) {
      errExit("epoll_ctl");
    }
  }

  while(1) {
    int i;
    int numRead;
    char* p;
    // int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
    nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);

    if(nfds < 0) {
      if(errno == EINTR) {continue;}
      return -1;
    }

    for(i = 0; i < nfds; i++) {
      if (events[i].events & ev_option) {
        fprintf(stderr, "catch\n");
        char newBuf[BUF_SIZE];
        char buf[BUF_SIZE];
        int num;
        struct epoll_handler* dat = events[i].data.ptr;
        //if((numRead = read(dat->fd, buf, BUF_SIZE)) <= 0) {
        //  errExit("read");
        //}
        //// fprintf(stderr, "%d -> numRead: %d\n", dat->fd, numRead);
        //num = snprintf(newBuf, BUF_SIZE, "numRead: %d, buf: %s", numRead, buf);
        //write(dat->fd, newBuf, num);
      }
    }
  }
  return 0;
}
