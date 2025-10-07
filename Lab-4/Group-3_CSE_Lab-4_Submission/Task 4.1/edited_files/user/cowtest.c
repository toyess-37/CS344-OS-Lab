#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096

int main(void){
  char *p = sbrk(PGSIZE);          // alloc 1 page
  p[0] = 10;                       // initial write

  int pid = fork();
  if(pid < 0){ printf("fork failed\n"); exit(1); }

  if(pid == 0){
    // child: write should trigger COW copy
    p[0] = 7;
    printf("child wrote %d\n", p[0]); // 7
    exit(0);
  } else {
    wait(0);
    printf("parent reads %d\n", p[0]); // 10
    exit(0);
  }
}
