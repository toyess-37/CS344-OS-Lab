#include "kernel/types.h"
#include "user/user.h"

int priority_fork(int priority, char label) {
    int p = fork();
    if (p == 0) {
      setpriority(priority);

      for (long i = 0; i < 1000000000; i++) {
          if (i % 10000000 == 0) write(1, &label, 1);
      }

      fprintf(1, "\nProcess %d finished\n", priority);
      exit(0);
    }
    return p;
}

int main(void) {

  priority_fork(500, 'A');
  priority_fork(1, 'B');
  priority_fork(100, 'C');

  wait(0);
  wait(0);
  wait(0);

  exit(0);
}