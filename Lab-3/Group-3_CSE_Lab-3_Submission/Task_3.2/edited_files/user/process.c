#include "kernel/types.h"
#include "user/user.h"

struct details {
  int L, end, startA, startB;              // length, end-marker, start of A, start of B
  int next_for_A[256], next_for_B[256];    // next positions of A and B [hard-coded]
  int doneA, doneB;                        // doneA = 1 when B reaches end and vice versa
};

static int recv_loc(int mbox_id) {
  int v;
  if (mbox_recv(mbox_id, &v) < 0) return -1;
  return v;
}

int
main(int argc, char* argv[])
{
  int role = atoi(argv[1]);       // 0 for A, 1 for B
  int SHM_KEY = atoi(argv[2]);
  int mail_ab = atoi(argv[3]);
  int mail_ba = atoi(argv[4]);

  struct details *g = (struct details*)shm_get(SHM_KEY);
  if (g == 0) {
    printf("shm_get failed\n");
    exit(1);
  }

  if (role == 0) { // Process A

    int a = g->startA;
    
    while(1) {
      int next_B = (a >= 0 && a < g->L) ? g->next_for_B[a] : g->end;
    
      // send to B
      mbox_send(mail_ab, next_B);
      
      printf("A: got=%d next_for_B=%d\t", a, next_B);
      
      if (next_B == g->end) {
        g->doneA = 1;
        shm_close(SHM_KEY);
        exit(0);
      }
      
      // receive from B
      int next_A = recv_loc(mail_ba);
      
      if (next_A == g->end) {
        g->doneA = 1;
        shm_close(SHM_KEY);
        exit(0);
      }
      
      a = next_A;
    }
    
  } else { // Process B

    int b = g->startB;
    
    while(1) {
      // receive from A
      int next_B = recv_loc(mail_ab);

      int next_A = (b >= 0 && b < g->L) ? g->next_for_A[b] : g->end;
      
      // send to A
      mbox_send(mail_ba, next_A);
      
      printf("B: got=%d next_for_A=%d\n", b, next_A);
      
      if (next_B == g->end) {
        g->doneB = 1;
        shm_close(SHM_KEY);
        exit(0);
      }
      
      if (next_A == g->end) {
        g->doneB = 1;
        shm_close(SHM_KEY);
        exit(0);
      }
      
      b = next_B;
    }
  }

  shm_close(SHM_KEY);
  exit(0);
}