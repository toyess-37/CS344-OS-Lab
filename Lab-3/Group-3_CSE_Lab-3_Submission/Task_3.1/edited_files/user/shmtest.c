#include "kernel/types.h"
#include "user/user.h"

#define KEY 1

int
main(void)
{
  int id = shm_create(KEY);
  if (id < 0) { 
    printf("shmtest: create failed\n"); 
    exit(1); 
  }

  int pid = fork();
  if (pid == 0) { // child
    char *p = (char*)shm_get(KEY);
    if (p==0) { 
      printf("child: shm_get failed\n"); 
      exit(1); 
    }
    strcpy(p, "hello");
    shm_close(KEY);
    exit(0);
  }

  char *p = (char*)shm_get(KEY);
  if (p==0) { 
    printf("parent: shm_get failed\n"); 
    exit(1); 
  }

  wait(0);
  printf("%s\n", p);  // prints hello

  shm_close(KEY);
  exit(0);
}
