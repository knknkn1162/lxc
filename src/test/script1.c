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

#define BUF_SIZE 256
#define MAX_SNAME 100

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)
static struct termios prevTermios;

// restore the tty settings to the state at the startup
static void ttyReset(void) {

  if(tcsetattr(STDIN_FILENO, TCSANOW, &prevTermios) == -1) {
    perror("tcsetattr");
    exit(1);
  }
}

// Handle in raw mode which is one of the noncannonical modes.
int ttySetRaw(int fd, struct termios *prevTermios) {
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

    syslog(LOG_INFO, "slaveFd: atty: %s, ttyname(master): %s, ptsname(slave): %s\n", isatty(slaveFd) ? "true" : "false", ttyname(slaveFd), ptsname(slaveFd));
    syslog(LOG_INFO, "PID=%ld, PGID=%ld, SID=%ld\n", (long) getpid(), (long) getpgrp(), (long) getsid(0));

    // without dup2..
    // bash: cannot set terminal process group (-1): Inappropriate ioctl for device
    // bash: no job control in this shell
    execlp("/bin/bash", "/bin/bash", "-i", NULL);
    // never reaches!!
    perror("execlp");
    exit(1);
  } else {
    close(slaveFd);
    // parent operates masterFd.
    fd_set inFds;
    char buf[BUF_SIZE];
    int numRead;
    int logFd;

    // set raw mode to read/write by each charecter, not by newline.
    int fd = open("/dev/tty", O_RDWR);
    ttySetRaw(fd, &prevTermios); atexit(ttyReset);

    if((logFd = open("log.log", O_WRONLY | O_CREAT | O_TRUNC, 0600)) == -1) {
      errExit("scriptFd open");
    }
    syslog(LOG_INFO, "masterFd: atty: %s, ttyname(master): %s, ptsname(slave): %s\n", isatty(masterFd) ? "true" : "false", ttyname(masterFd), ptsname(masterFd));
    syslog(LOG_INFO, "PID=%ld, PGID=%ld, SID=%ld\n", (long) getpid(), (long) getpgrp(), (long) getsid(0));

    while(1) {
      memset(buf, 0, BUF_SIZE);
      // wait on select(1)
      FD_ZERO(&inFds);
      FD_SET(STDIN_FILENO, &inFds);
      FD_SET(masterFd, &inFds);

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
        if(write(masterFd, buf, numRead) != numRead) {
          errExit("end: write to masterFd");
        }
      }

      if(FD_ISSET(masterFd, &inFds)) {
        syslog(LOG_INFO, "reaches masterFd\n");
        if((numRead = read(masterFd, buf, BUF_SIZE)) <= 0) {
          // assume that the user types exit command
          exit(EXIT_SUCCESS);
        }
        syslog(LOG_INFO, "[master] masterFd -> STDOUT_FILENO: %.*s (%d)\n", numRead, buf, numRead);
        if(write(STDOUT_FILENO, buf, numRead) != numRead) {
          errExit("master: write(stdin)");
        }

        // logging
        if(write(logFd, buf, numRead) != numRead) {
          errExit("master: write(log)");
        }
      }
    }
  } // end fork
  return 0;
}
