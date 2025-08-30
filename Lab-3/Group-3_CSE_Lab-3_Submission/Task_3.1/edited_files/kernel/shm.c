#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "shm.h"
#include "defs.h"

static struct {
  struct spinlock lock;
  struct shm_region reg[MAX_SHM];
} shm; // shared memory table --- all the shared pages

void
shminit(void)
{
  initlock(&shm.lock, "shm.table");
  for (int i = 0; i < MAX_SHM; i++) {
    shm.reg[i].used = 0;
    shm.reg[i].key = 0;
    shm.reg[i].page = 0;
    shm.reg[i].ref_count = 0;
  }
}

static int // finds if any page has the given key
find_slot_by_key(int key)
{
  for (int i = 0; i < MAX_SHM; i++)
    if (shm.reg[i].used && shm.reg[i].key == key)
      return i;
  return -1;
}

static int // allocates a shared memory page with the given key
alloc_slot(int key)
{
  for (int i = 0; i < MAX_SHM; i++) {
    if (!shm.reg[i].used) {
      shm.reg[i].used = 1;
      shm.reg[i].key = key;
      shm.reg[i].page = 0;
      shm.reg[i].ref_count = 0;
      return i;
    }
  }
  return -1;
}

int
shm_create(int key)
{
  acquire(&shm.lock);
  int s = find_slot_by_key(key);
  if (s >= 0) { // page exists already
    release(&shm.lock);
    return s;
  }
  s = alloc_slot(key);
  if (s < 0) { 
    release(&shm.lock); 
    return -1; 
  }

  char *pa = kalloc();
  if (pa == 0) {
    shm.reg[s].used = 0;
    release(&shm.lock);
    return -1;
  }
  memset(pa, 0, PGSIZE);
  shm.reg[s].page = pa;
  release(&shm.lock);
  return s;
}

uint64
shm_get(int key)
{
	struct proc *p = myproc();
  acquire(&shm.lock);
  int s = find_slot_by_key(key);
  if (s < 0 || shm.reg[s].page == 0) { 
		release(&shm.lock); 
		return 0; 
	}

  uint64 va = SHMVA(s);
  pte_t *pte = walk(p->pagetable, va, 0);
  if (pte == 0 || ((*pte) & PTE_V) == 0) {
    if (mappages(p->pagetable, va, PGSIZE, (uint64)shm.reg[s].page, PTE_R|PTE_W|PTE_U) < 0) {
      release(&shm.lock); 
			return 0;
    }
    shm.reg[s].ref_count++;
  }
  release(&shm.lock);
  return va;
}

int
shm_close(int key)
{
  struct proc *p = myproc();
  acquire(&shm.lock);
  int s = find_slot_by_key(key);
  if (s < 0) { 
		release(&shm.lock); 
		return -1; 
	}
	
  uint64 va = SHMVA(s);

  pte_t *pte = walk(p->pagetable, va, 0);
  if (pte && ((*pte) & PTE_V)) {
    uvmunmap(p->pagetable, va, 1, 0);
    if (shm.reg[s].ref_count > 0) shm.reg[s].ref_count--;
    if (shm.reg[s].ref_count == 0 && shm.reg[s].page) {
      kfree(shm.reg[s].page);
      shm.reg[s].page = 0;
      shm.reg[s].used = 0; // free slot once page freed
    }
  }
  release(&shm.lock);
  return 0;
}

void
shm_cleanup(struct proc *p)
{
  acquire(&shm.lock);
  for (int s = 0; s < MAX_SHM; s++) {
    if (shm.reg[s].used == 0) continue;
    uint64 va = SHMVA(s);
    pte_t *pte = walk(p->pagetable, va, 0);
    if (pte && ((*pte) & PTE_V)) {
      uvmunmap(p->pagetable, va, 1, 0);
      if (shm.reg[s].ref_count > 0) shm.reg[s].ref_count--;
      if (shm.reg[s].ref_count == 0 && shm.reg[s].page) {
        kfree(shm.reg[s].page);
        shm.reg[s].page = 0;
        shm.reg[s].used = 0;
      }
    }
  }
  release(&shm.lock);
}