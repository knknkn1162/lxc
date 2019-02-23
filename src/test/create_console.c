#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/param.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pty.h>
#include <fcntl.h>
#include <termios.h>

/* A simple error-handling function: print an error message based
   on the value in 'errno' and terminate the calling process */

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

struct console {
  int slave_fd; int master_fd; int peer; char *path; char name[MAXPATHLEN]; struct termios *tios;
};

int main(void) {
  struct console console;
  int fd;

  openpty(&console.master_fd, &console.slave_fd, console.name, NULL, NULL);
	fcntl(console.master_fd, F_SETFD, FD_CLOEXEC);
	fcntl(console.slave_fd, F_SETFD, FD_CLOEXEC);
	//fd = open("/usr/local/lib/lxc/debian01/rootfs/dev/tty", O_CLOEXEC | O_RDWR | O_CREAT | O_APPEND, 0600);

  //if(fd == -1) {
  //  errExit("open");
  //}

  //console.peer = fd;


  // test whether a file descriptor refers to a terminal
  if (!isatty(console.peer)) {
    errExit("isatty");
  }

  printf("allocated pty '%s' (%d/%d)\n", console.name, console.master_fd, console.slave_fd);

	console.tios = malloc(sizeof(struct termios));
  // get termios
  if(tcgetattr(console.peer, console.tios) == -1) {
    errExit("tcgetattr");
  }

	console.tios->c_iflag &= ~IGNBRK;
	console.tios->c_iflag &= BRKINT;
	console.tios->c_lflag &= ~(ECHO|ICANON|ISIG);
	console.tios->c_cc[VMIN] = 1;
	console.tios->c_cc[VTIME] = 0;

	/* Set new attributes */
	tcsetattr(console.peer, TCSAFLUSH, console.tios);
  sleep(30);
  return 0;
}
