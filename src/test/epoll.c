#define _GNU_SOURCE
#include <sys/epoll.h>
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
#include <sys/inotify.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define MAX_EVENTS 10

int main(void) {
  int epfd;
  int nfds;
  // struct epoll_event { uint32_t events; epoll_data_t data; }
  struct epoll_event ev;
  struct epoll_event events[MAX_EVENTS];
  int fd, wd;
  int num = 10;
  int* ans;

  // lxc_main_open
  if((epfd = epoll_create(4)) == -1) {
    errExit("epoll_create");
  }

  if((fd = inotify_init()) < 0) {
    errExit("inotify_init");
  }

  // nonnegative watch descriptor
  if((wd = inotify_add_watch(fd, "/home/vagrant/shared/lxc/src/test", IN_MODIFY | IN_CREATE)) < 0) {
    errExit("inotify_add_watch");
  } 

  ev.events = EPOLLIN;
  //ev.data.ptr = &num;
  ev.data.fd = fd;

  // Input to
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
    errExit("epoll_ctl");
  }

  while(1) {
    int i;
    char buf[1000];
    int numRead;
    char* p;
    struct inotify_event* e;
    // int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
    nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
    printf("nfds: %d\n", nfds);

    if(nfds < 0) {
      if(errno == EINTR) {continue;}
      return -1;
    }
    for(i = 0; i < nfds; i++) {
      if (events[i].events & EPOLLIN) {
        numRead = read(events[i].data.fd, buf, 1000);
        printf("numread: %d\n", numRead);
        for(p = buf; p < buf + numRead;) {
          /*
              struct inotify_event {
            int      wd;      
            uint32_t mask;    
            uint32_t cookie;  
            uint32_t len;     
            char     name[];  
        };
          */
          e = (struct inotify_event *)p;
          printf("cookie =%4d; ", e->cookie);
          printf("mask =%d; ", e->mask);
          printf("len =%d; ", e->len);
          printf("name = %s\n", e->name);
          p += sizeof(struct inotify_event) + e->len;
        }
      }
    }
  }
  return 0;
}
