#include "kernel/types.h"
#include "user/user.h"

struct details {
  int L, end, startA, startB;              // length, end-marker, start of A, start of B
  int next_for_A[256], next_for_B[256];    // next positions of A and B [hard-coded]
  int doneA, doneB;                        // doneA = 1 when B reaches end and vice versa
};

int
main(int argc, char *argv[])
{
  if (argc < 4) {
    printf("format: master [1<= length of maze <= 255] [startA] [startB]\n");
    exit(1);
  }

  int base = 10 + getpid();         // unique keys every time we run
  int SHM_KEY  = base;              // shared page key
  int AB_KEY = base + 100;         // mailbox A->B key
  int BA_KEY = base + 101;         // mailbox B->A key

  int shm_new = shm_create(SHM_KEY);
  if (shm_new < 0) {
    printf("shm_create failed\n");
    exit(1);
  }

  struct details *g = (struct details*)shm_get(SHM_KEY); 
  if (g==0) {
    printf("shm_get failed\n");
    exit(1);
  }

  g->L = atoi(argv[1]);
  if (g->L <= 0 || g->L > 255) {
    printf("length of maze must be >= 1 and <= 255, but got %d\n", g->L);
    exit(1);
  }

  g->end = -1;

  g->startA = atoi(argv[2]);
  g->startB = atoi(argv[3]);
  if (g->startA < 0 || g->startA >= g->L || g->startB < 0 || g->startB >= g->L) {
    printf("start positions must be in [0..%d), but got A=%d, B=%d\n", g->L, g->startA, g->startB);
    exit(1);
  }

  g->doneA = g->doneB = 0;

  for (int i=0; i<g->L; i++) {
    g->next_for_A[i] = ((i+1) < g->L ? (i+1) : g->end);
    g->next_for_B[i] = ((i+2) < g->L ? (i+2) : g->end);
  }

  int mail_ab = mbox_create(AB_KEY);
  int mail_ba = mbox_create(BA_KEY);

  printf("shm_key=%d, A->B id=%d, B->A id=%d\n", SHM_KEY, mail_ab, mail_ba);

  if (mail_ab < 0 || mail_ba < 0) {
    printf("mbox_create failed\n");
    exit(1);
  }

  // Fork two processes for A and B. A gets role 0 and B gets role 1.
  if (fork() == 0) {
    char role[8], shm[8], ab[8], ba[8];
    itoa10(0, role); itoa10(SHM_KEY, shm); itoa10(mail_ab, ab); itoa10(mail_ba, ba);
    char *args[] = {"process", role, shm, ab, ba, 0};
    exec("process", args);
    printf("master: exec failed\n");
    exit(1);
  }

  if (fork() == 0) {
    char role[8], shm[8], ab[8], ba[8];
    itoa10(1, role); itoa10(SHM_KEY, shm); itoa10(mail_ab, ab); itoa10(mail_ba, ba);
    char *args[] = {"process", role, shm, ab, ba, 0};
    exec("process", args);
    printf("master: exec failed\n");
    exit(1);
  }

  wait(0);
  wait(0);

  if (g->doneA && g->doneB) {
    printf("WIN: both processes reached end\n");
  } else if (g->doneA) {
    printf("LOSS: B didn't finish\n");
  } else if (g->doneB) {
    printf("LOSS: A didn't finish\n");
  } else {
    printf("LOSS: neither finished\n");
  }

  mbox_close(mail_ab);
  mbox_close(mail_ba);

  exit(0);
}
