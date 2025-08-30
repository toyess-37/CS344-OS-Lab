#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    printf("usage: mboxtest send recv\n");
    exit(1);
  }

  int a = atoi(argv[1]);
  int b = atoi(argv[2]);

  if (a < 0) a = 0;
  if (b < 0) b = 0;

  int key = 10+getpid();
  int id = mbox_create(key);
  if (id < 0) { 
    printf("mbox_create failed\n"); 
    exit(1); 
  }

  int pid = fork();

  if (pid == 0) { // receiver
    int received = 0;
    while (received++ < b) {
      int v;
      if (mbox_recv(id, &v) < 0) {
        printf("recv: EOF after %d\n", v);
        break;
      }
      printf("recv %d\n", v);
    }
    exit(0);
  }

  // sender
  for (int i=1; i<=a; i++){
    mbox_send(id, i);
    printf("send %d\n", i);
  }
  mbox_close(id);

  wait(0);
  exit(0);
}
