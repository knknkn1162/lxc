#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
 
 int main(void) {
 
     int fd;
      //int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
     uid_t ruid, euid, suid;
     getresuid(&ruid, &euid, &suid);
     printf("real: %d, effective: %d, saved: %d\n", ruid, euid, suid);
     /* create a file */
     fd = creat("setuid.out", 0644);
     if(fd!=-1) close(fd);
     return 0;
 }
