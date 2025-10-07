#ifndef __SWAP_H__
#define __SWAP_H__

extern struct proc proc[NPROC];

// Global MRU list
struct {
  struct spinlock lock;
  struct mrunode *head;  // Most recently used
  struct mrunode *tail;  // Least recently used
  int count;             // number of resident pages tracked
} mru;

// We allocate MRU nodes dynamically via kalloc/kfree.

// proc[] comes from proc.c; needed to index per-proc swapspace

// Simple swap space - in-memory for simplicity
// In a real system, this would be on disk
#define SWAPSPACE (MAXSWAPPAGES * PGSIZE)
static char swapspace[NPROC][SWAPSPACE];
static struct spinlock swap_lock;

void swapinit(void);
void swapinitproc(struct proc *p);
void swapfreeproc(struct proc *p);
int swapout(struct proc *p, uint64 va);
int swapin(struct proc *p, uint64 va);
void mru_init(void);
void mru_add(struct proc *p, uint64 va);
void mru_remove(struct proc *p, uint64 va);
void mru_touch(struct proc *p, uint64 va);
struct mrunode* mru_evict(void);
void mru_ensure_capacity(void);
void dumpmru(void);
int getpagestat_k(int pid, struct pagestat *st);

#endif // __SWAP_H__