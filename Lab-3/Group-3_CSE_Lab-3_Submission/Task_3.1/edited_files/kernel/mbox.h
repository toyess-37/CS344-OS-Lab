#define MAX_MBOX   64
#define MBOX_CAP   16

struct mailbox {
  struct spinlock lock;
  int used;
  int key;
  int buf[MBOX_CAP];
  int head, tail, count;
  int closed; // to make sure that the mailbox is closed properly
};

void mboxinit(void);
int  mbox_create(int key);
int  mbox_send(int id, int msg);
int  mbox_recv(int id, int *msg);
int  mbox_close(int id);