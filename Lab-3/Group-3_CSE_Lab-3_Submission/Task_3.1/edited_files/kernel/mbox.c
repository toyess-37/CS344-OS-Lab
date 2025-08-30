#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "mbox.h"
#include "defs.h"

static struct {
  struct mailbox box[MAX_MBOX];
} mboxes; // table of mbox

void
mboxinit(void)
{
  for (int i = 0; i < MAX_MBOX; i++) {
    initlock(&mboxes.box[i].lock, "mbox");
    mboxes.box[i].used = 0;
    mboxes.box[i].key = 0;
    mboxes.box[i].head = mboxes.box[i].tail = mboxes.box[i].count = 0;
    mboxes.box[i].closed = 0;
  }
}

static int // if mboxes.box[i].key == key then it returns i
find_id_by_key(int key)
{
  for (int i = 0; i < MAX_MBOX; i++)
    if (mboxes.box[i].used && mboxes.box[i].key == key)
      return i;
  return -1;
}

int
mbox_create(int key)
{
  int id = find_id_by_key(key);
  if (id >= 0) return id;

  for (int i = 0; i < MAX_MBOX; i++) {
    if (!mboxes.box[i].used) {
      mboxes.box[i].used = 1;
      mboxes.box[i].key = key;

      acquire(&mboxes.box[i].lock);
      mboxes.box[i].head = mboxes.box[i].tail = mboxes.box[i].count = 0;
      mboxes.box[i].closed = 0;
      release(&mboxes.box[i].lock);
      return i;
    }
  }
  return -1;
}

int
mbox_send(int id, int msg)
{
  if (id < 0 || id >= MAX_MBOX) return -1;

  struct mailbox *b = &mboxes.box[id];
  acquire(&b->lock);

  if (!b->used || b->closed) {
    release(&b->lock);
    return -1;
  }

  while (b->count == MBOX_CAP && !b->closed) {
    sleep(b, &b->lock);
  } // busy waiting

  if (b->closed) {
    release(&b->lock);
    return -1;
  }

  b->buf[b->tail] = msg;
  b->tail = (b->tail + 1) % MBOX_CAP;
  b->count++;
  wakeup(b);

  release(&b->lock);
  return 0;
}

int
mbox_recv(int id, int *msg)
{
  if (id < 0 || id >= MAX_MBOX) return -1;

  struct mailbox *b = &mboxes.box[id];
  acquire(&b->lock);

  if (!b->used) {
    release(&b->lock);
    return -1;
  }
  
  while (b->count == 0 && !b->closed) {
    sleep(b, &b->lock);
  }

  if (b->count == 0 && b->closed) { // end of the mailbox entries
    release(&b->lock);
    return -1;
  }

  int v = b->buf[b->head];
  b->head = (b->head + 1) % MBOX_CAP;
  b->count--;
  wakeup(b);
  release(&b->lock);

  *msg = v;
  return 0;
}

int 
mbox_close(int id)
{
  if (id < 0 || id >= MAX_MBOX) return -1;

  struct mailbox *b = &mboxes.box[id];
  acquire(&b->lock);

  if (!b->used) {
    release(&b->lock);
    return -1;
  } 

  b->closed = 1;
  wakeup(b);

  release(&b->lock);
  return 0;
}