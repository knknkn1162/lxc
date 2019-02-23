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
#include <sys/wait.h>

#define BUF_SIZE 256
#define MAX_SNAME 100

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

int main(void) {
  int masterFd, slaveFd;
  pid_t childPid;
  //struct winsize ws;

  // 1. open the psuedoterminal master device
  // O_NOCTTY: don't assign controlling terminal
  masterFd = posix_openpt(O_RDWR | O_NOCTTY);
  // or simply you can open /dev/ptmx
  //if((masterFd = open("/dev/ptmx", O_RDONLY | O_NOCTTY)) == -1) {
  //  errExit("open /dev/ptmx");
  //}

  // creates a child process that executes a set-user-ID-root program.
  grantpt(masterFd);

  // allow the calling process to perform whatever initialization is required for the pseudoterminal slave
  unlockpt(masterFd);

  // 2. create child process that is connected to the parent by a pseudoterminal pair;
  // The Child process operates slaveFd
  if((childPid = fork()) == 0) {
    char ptsName[MAX_SNAME];
    // 2.a) Call setsid to start a new session, of which the child is the session leader. This step also causes the child to lose its controlling terminal.
    setsid();

    // retrieve pts name, typically "/dev/pts/*"
    ptsname_r(masterFd, ptsName, MAX_SNAME);

    // no longer need master file descriptor
    close(masterFd);

    // 2.b) Open the psuedoterminal slave device that corresponds to the master device. Since the child process is a session leader, and it doesn't have a controlling terminal, the psuedo terminal slave becomes the controlling terminal for the child process.
    // Make the given terminal the controlling terminal of the calling process.  The calling process must be a session  leader and not have a controlling terminal already
    slaveFd = open(ptsName, O_RDWR);

    // 2.c) Use dup to duplicate the file descriptor for the slave device on standard input, output and error
    dup2(slaveFd, STDIN_FILENO);
    dup2(slaveFd, STDOUT_FILENO);
    dup2(slaveFd, STDERR_FILENO);
    printf("slaveFd: atty: %s, ttyname(master): %s, ptsname(slave): %s\n", isatty(slaveFd) ? "true" : "false", ttyname(slaveFd), ptsname(slaveFd));
    printf("PID=%ld, PGID=%ld, SID=%ld\n", (long) getpid(), (long) getpgrp(), (long) getsid(0));

    // without dup2..
    // bash: cannot set terminal process group (-1): Inappropriate ioctl for device
    // bash: no job control in this shell
    execlp("/bin/bash", "/bin/bash", NULL);
    // never reaches!!
    perror("execlp");
    exit(1);
  } else {
    // parent operates masterFd.
    fd_set inFds;
    char buf[BUF_SIZE];
    int numRead;
    while(1) {
      memset(buf, 0, BUF_SIZE);
      // wait on select(1)
      FD_ZERO(&inFds);
      FD_SET(STDIN_FILENO, &inFds);
      //FD_SET(masterFd, &inFds);

      // int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
      if(select(masterFd + 1, &inFds, NULL, NULL, NULL) == -1) {
        perror("select");
      }

      if(FD_ISSET(STDIN_FILENO, &inFds)) {
        syslog(LOG_INFO, "reaches stdin_fileno\n");
        if((numRead = read(STDIN_FILENO, buf, BUF_SIZE)) <= 0) {
          errExit("end: read from stdin");
        }
        syslog(LOG_INFO, "[master] STDIN_FILENO -> masterFd: %.*s (%d)\n", numRead, buf, numRead);
        if(write(STDOUT_FILENO, buf, numRead) != numRead) {
          errExit("end: write to masterFd");
        }
      }
    }
  } // end fork
  return 0;
}
