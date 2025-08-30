#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// a CPU-bound function
void cpu_work(long iter) {
  for (long i = 0; i < iter; i++) {
    // A volatile variable prevents the compiler from optimizing the loop away.
    volatile int x = 1 + 2*3 - 4 / 1 + 91 - 35 * 1729 + 8;
    (void)x; // Suppress "unused variable" warning.
  }
}

// function to test lottery scheduling
int fork_lottery_child(const char *label, int tickets, int dur) {
  int p = fork();
  if (p==0) {
    if (settickets(tickets) < 0) printf("%s: settickets failed\n", label);

    uint64 start = uptime(), cnt = 0;
    while(uptime() < start + dur){
      cpu_work(5000);
      cnt++;
    }

    printf("%s tickets=%d cnt=%d\n", label, tickets, (int)cnt); // "cnt" will be proportional to the number of "tickets"
    exit(0);
  }
  return p;
}

int
main(void)
{
  int t1 = 10;
  int t2 = 100;
  int dur = 300;

  fork_lottery_child("A", t1, dur);
  fork_lottery_child("B", t2, dur);

  wait(0); 
  wait(0);

  write(1, "\n", 1);
  exit(0);
}
