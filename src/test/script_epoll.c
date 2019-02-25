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
#include <pty.h>
#include <sys/epoll.h>

#define BUF_SIZE 256
#define MAX_SNAME 100
#define MAX_EVENTS 10

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)
static struct termios prevTermios;

typedef int (*callback_t)(int fd, void *data);
struct epoll_handler {
  int fd;
  callback_t callback;
  void* data;
};

// restore the tty settings to the state at the startup
static void ttyReset(void) {

  if(tcsetattr(STDIN_FILENO, TCSANOW, &prevTermios) == -1) {
    perror("tcsetattr");
    exit(1);
  }
}

// Handle in raw mode which is one of the noncannonical modes.
static int ttySetRaw(int fd, struct termios *prevTermios) {
  struct termios t;
  if(tcgetattr(fd, &t) == -1) {
    return -1;
  }

  if(prevTermios != NULL) {
    *prevTermios = t;
  }

  // disable
  // // IEXTEN .. Enable extended processing of input characters
  t.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
  //t.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR | INPCK | ISTRIP | IXON | PARMRK);

  // disable "Perform output postprocessing"
  //t.c_oflag &= ~OPOST;
  //take one character
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;

  if(tcsetattr(fd, TCSAFLUSH, &t) == -1) {
    return -1;
  }
  return 0;
}

int echo_handler(int fd, void* data) {
  int* fds = (int*)data;
  int numRead;
  int* fdi;
  char buf[BUF_SIZE];

  syslog(LOG_INFO, "read :fd %d", fd);
  if((numRead = read(fd, buf, BUF_SIZE)) <= 0) {
    // assume that the user types exit command
    syslog(LOG_INFO, "read exit %d", numRead);
    exit(EXIT_SUCCESS);
  }

  syslog(LOG_INFO, "write %s", buf);
  for(fdi = fds; *fdi != -1; fdi++) {
    syslog(LOG_INFO, "write fd: %d", *fdi);
    if(write(*fdi, buf, numRead) != numRead) {
      errExit("end: write to masterFd");
    }
  }
  return 0;
}

int main(void) {
  int masterFd, slaveFd;

  pid_t childPid;
  //struct winsize ws;
  char ptsName[MAX_SNAME];

	if (openpty(&masterFd, &slaveFd, ptsName, NULL, NULL)) {
		errExit("failed to allocate a pty");
		return -1;
	}

  // get prev termios before fork
  tcgetattr(STDIN_FILENO, &prevTermios);

  // 2. create child process that is connected to the parent by a pseudoterminal pair;
  // The Child process operates slaveFd
  if((childPid = fork()) == 0) {
    char ptsName[MAX_SNAME];

    // no longer need master file descriptor
    close(masterFd);
    setsid();

    // 2.c) Use dup to duplicate the file descriptor for the slave device on standard input, output and error
    dup2(slaveFd, STDIN_FILENO);
    dup2(slaveFd, STDOUT_FILENO);
    dup2(slaveFd, STDERR_FILENO);

    // without dup2..
    // bash: cannot set terminal process group (-1): Inappropriate ioctl for device
    // bash: no job control in this shell
    execlp("/bin/bash", "/bin/bash", "-i", NULL);
    // never reaches!!
    perror("execlp");
    exit(1);
  } else {
    int epfd;
    int nfds;
    struct epoll_event events[MAX_EVENTS];
    // parent operates masterFd.
    char buf[BUF_SIZE];
    close(slaveFd);

    int logFd;
    struct epoll_event ev;
    if((epfd = epoll_create(2)) == -1) {
      errExit("epoll_create");
    }

    if((logFd = open("log.log", O_WRONLY | O_CREAT | O_TRUNC, 0600)) == -1) {
      errExit("scriptFd open");
    }
    printf("logFd: %d\n", logFd);

    ttySetRaw(STDIN_FILENO, &prevTermios); atexit(ttyReset);
    {
      //int ttyFd = open("/dev/tty", O_RDWR);
      //printf("ttyFd: %d\n", ttyFd);
      struct epoll_event ev;
      ev.events = EPOLLIN;
      ev.data.ptr = &(struct epoll_handler) {
        .fd = STDIN_FILENO, .callback = echo_handler, .data = &(int[]) {masterFd, -1}
      };
      if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1) {
        errExit("epoll_ctl ttyFd");
      }
    }

    // register masterFd
    {
      struct epoll_event ev;
      ev.events = EPOLLIN;
      ev.data.ptr = &(struct epoll_handler) {
        .fd = masterFd, .callback = echo_handler, .data = &(int[]) {STDOUT_FILENO, logFd, -1}
      };
      if (epoll_ctl(epfd, EPOLL_CTL_ADD, masterFd, &ev) == -1) {
        errExit("epoll_ctl masterFd");
      }
    }

    while(1) {
      int i;
      memset(buf, 0, BUF_SIZE);

      // int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
      nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
      syslog(LOG_INFO, "reaches %d\n", nfds);

      if(nfds < 0) {
        if(errno == EINTR) {continue;}
        return -1;
      }

      for(i = 0; i < nfds; i++) {
        if (events[i].events & EPOLLIN) {
          char newBuf[BUF_SIZE];
          char buf[BUF_SIZE];
          int num;
          struct epoll_handler* handler = events[i].data.ptr;
          int* fdi;
          int numRead;
          syslog(LOG_INFO, "epoll fd: %d\n", handler->fd);
          handler->callback(handler->fd, handler->data);
        } else if (events[i].events & (EPOLLHUP | EPOLLERR)) {
          syslog(LOG_INFO, "exit %d", ((struct epoll_handler*)(events[i].data.ptr))->fd);
          exit(EXIT_SUCCESS);
        }
      }
    }
  } // end fork
  return 0;
}
