#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "swap.h"

void
swapinit(void)
{
  initlock(&mru.lock, "mru");
  initlock(&swap_lock, "swap");
  mru.head = 0;
  mru.tail = 0;
  mru.count = 0;
}

// Initialize swap for a process
void
swapinitproc(struct proc *p)
{
  p->swapfile = 0;
  p->swapped = 0;
  p->pstat.num_pagefaults = 0;
  p->pstat.num_swapins = 0;
  p->pstat.num_swapouts = 0;
}

// Free swap resources for a process
void
swapfreeproc(struct proc *p)
{
  // Remove all pages of this process from MRU list
  acquire(&mru.lock);
  struct mrunode *node = mru.head;
  while(node) {
    struct mrunode *next = node->next;
    if(node->proc == p) {
      if(node->prev) node->prev->next = node->next; else mru.head = node->next;
      if(node->next) node->next->prev = node->prev; else mru.tail = node->prev;
      kfree((void*)node);
      if(mru.count > 0) mru.count--;
    }
    node = next;
  }
  release(&mru.lock);
  
  // Free swapped pages metadata
  struct swappage *sp = p->swapped;
  while(sp) {
    struct swappage *next = sp->next;
    kfree((void*)sp);
    sp = next;
  }
  
  p->swapped = 0;
}

// Swap out a page to disk
int
swapout(struct proc *p, uint64 va)
{
  va = PGROUNDDOWN(va);
  pte_t *pte = walk(p->pagetable, va, 0);
  if(pte == 0 || (*pte & PTE_V) == 0)
    return -1;
    
  uint64 pa = PTE2PA(*pte);
  
  // Allocate swap page metadata
  struct swappage *sp = (struct swappage*)kalloc();
  if(sp == 0)
    return -1;
  
  sp->va = va;
  sp->offset = p->pstat.num_swapouts * PGSIZE;
  
  // Check if we have space in swap
  if(sp->offset >= SWAPSPACE) {
    kfree((void*)sp);
    return -1;
  }
  
  sp->next = p->swapped;
  p->swapped = sp;
  
  // Write page to swap space
  acquire(&swap_lock);
  int procidx = p - proc;
  if(procidx < 0 || procidx >= NPROC) {
    release(&swap_lock);
    p->swapped = sp->next;
    kfree((void*)sp);
    return -1;
  }
  memmove(&swapspace[procidx][sp->offset], (void*)pa, PGSIZE);
  release(&swap_lock);
  
  // Update statistics
  p->pstat.num_swapouts++;
  
  // Mark PTE as swapped (clear valid bit, set swapped bit)
  uint64 flags = PTE_FLAGS(*pte);
  *pte = ((sp->offset / PGSIZE) << 10) | (flags & ~PTE_V) | PTE_S;
  
  // Free the physical page
  kfree((void*)pa);
  
  return 0;
}

// Swap in a page from disk
int
swapin(struct proc *p, uint64 va)
{
  va = PGROUNDDOWN(va);
  pte_t *pte = walk(p->pagetable, va, 0);
  if(pte == 0 || (*pte & PTE_S) == 0)
    return -1;
  
  // Find swap page metadata
  struct swappage **spp = &p->swapped;
  struct swappage *sp = 0;
  while(*spp) {
    if((*spp)->va == va) {
      sp = *spp;
      *spp = sp->next;  // Remove from list
      break;
    }
    spp = &((*spp)->next);
  }
  
  if(sp == 0)
    return -1;
  
  // Ensure we are under the MRU resident cap, then allocate physical page
  mru_ensure_capacity();
  uint64 mem = (uint64)kalloc();
  if(mem == 0) {
    // Need to evict a page
    struct mrunode *victim = mru_evict();
    if(victim == 0) {
      // Put swap page back in list
      sp->next = p->swapped;
      p->swapped = sp;
      return -1;
    }
    
    // Swap out the victim page
    if(swapout(victim->proc, victim->va) < 0) {
      // Put swap page back in list
      sp->next = p->swapped;
      p->swapped = sp;
      kfree((void*)victim);
      return -1;
    }
    
    // free the MRU node we just evicted
    kfree((void*)victim);
    mem = (uint64)kalloc();
    if(mem == 0) {
      // Put swap page back in list
      sp->next = p->swapped;
      p->swapped = sp;
      return -1;
    }
  }
  
  // Read page from swap space
  acquire(&swap_lock);
  int procidx = p - proc;
  if(procidx < 0 || procidx >= NPROC) {
    release(&swap_lock);
    kfree((void*)mem);
    sp->next = p->swapped;
    p->swapped = sp;
    return -1;
  }
  memmove((void*)mem, &swapspace[procidx][sp->offset], PGSIZE);
  release(&swap_lock);
  
  // Update statistics
  p->pstat.num_swapins++;
  
  // Update PTE to point to physical page
  uint64 flags = PTE_FLAGS(*pte);
  *pte = PA2PTE(mem) | (flags & ~PTE_S) | PTE_V;
  
  // Free swap page metadata
  kfree((void*)sp);
  
  // Add to MRU list (avoid calling ensure again to prevent extra churn)
  mru_add(p, va);
  
  // Flush TLB
  sfence_vma();
  
  return 0;
}

// Initialize MRU list
void
mru_init(void)
{
  swapinit();
}

// Add a page to the head of MRU list (most recently used)
void
mru_add(struct proc *p, uint64 va)
{
  // Check if page is kernel or page table page
  if(va >= MAXVA)
    return;
  
  // Check if it's a user page
  pte_t *pte = walk(p->pagetable, va, 0);
  if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0)
    return;

  acquire(&mru.lock);
  // If already present, move to head
  va = PGROUNDDOWN(va);
  for(struct mrunode *cur = mru.head; cur; cur = cur->next){
    if(cur->proc == p && cur->va == va){
      if(cur != mru.head){
        if(cur->prev) cur->prev->next = cur->next;
        if(cur->next) cur->next->prev = cur->prev; else mru.tail = cur->prev;
        cur->prev = 0;
        cur->next = mru.head;
        if(mru.head) mru.head->prev = cur;
        mru.head = cur;
      }
      release(&mru.lock);
      return;
    }
  }
  // Not present; create a new node and add to head
  struct mrunode *node = (struct mrunode*)kalloc();
  if(node == 0){
    release(&mru.lock);
    return;
  }
  node->proc = p;
  node->va = va;
  node->prev = 0;
  node->next = mru.head;
  if(mru.head) mru.head->prev = node; else mru.tail = node;
  mru.head = node;
  mru.count++;

  release(&mru.lock);
}

// Remove a page from MRU list
void
mru_remove(struct proc *p, uint64 va)
{
  acquire(&mru.lock);
  
  va = PGROUNDDOWN(va);
  struct mrunode *node = mru.head;
  
  while(node) {
    if(node->proc == p && node->va == va) {
      // Remove from list
      if(node->prev)
        node->prev->next = node->next;
      else
        mru.head = node->next;
        
      if(node->next)
        node->next->prev = node->prev;
      else
        mru.tail = node->prev;
      kfree((void*)node);
      if(mru.count > 0) mru.count--;
      break;
    }
    node = node->next;
  }
  
  release(&mru.lock);
}

// Update a page to be most recently used
void
mru_update(struct proc *p, uint64 va)
{
  acquire(&mru.lock);
  
  va = PGROUNDDOWN(va);
  struct mrunode *node = mru.head;
  
  // Find the node
  while(node) {
    if(node->proc == p && node->va == va) {
      // Already at head?
      if(node == mru.head) {
        release(&mru.lock);
        return;
      }
      
      // Remove from current position
      if(node->prev)
        node->prev->next = node->next;
        
      if(node->next)
        node->next->prev = node->prev;
      else
        mru.tail = node->prev;
      
      // Move to head
      node->next = mru.head;
      node->prev = 0;
      
      if(mru.head)
        mru.head->prev = node;
      
      mru.head = node;
      
      release(&mru.lock);
      return;
    }
    node = node->next;
  }
  
  release(&mru.lock);
  
  // Not found, add it
  mru_add(p, va);
}

// Evict the most recently used page (MRU policy)
struct mrunode*
mru_evict(void)
{
  acquire(&mru.lock);
  
  struct mrunode *victim = mru.head;
  
  if(victim == 0) {
    release(&mru.lock);
    return 0;
  }
  
  // Remove from head
  mru.head = victim->next;
  if(mru.head)
    mru.head->prev = 0;
  else
    mru.tail = 0;
  if(mru.count > 0) mru.count--;
  
  release(&mru.lock);
  
  return victim;
}

// Ensure there is capacity for one more resident page; evict MRU if needed
void
mru_ensure_capacity(void)
{
  // Evict until under cap
  while(1){
    acquire(&mru.lock);
    int need = (mru.count >= MAXRESIDENT);
    release(&mru.lock);
    if(!need) break;
    struct mrunode *victim = mru_evict();
    if(victim == 0) break;
    // Swap out victim
    swapout(victim->proc, victim->va);
    kfree((void*)victim);
  }
}

// Dump MRU list for debugging
void
mru_dump(void)
{
  acquire(&mru.lock);
  
  printf("MRU List (head to tail) [count=%d]:\n", mru.count);
  struct mrunode *node = mru.head;
  int count = 0;
  
  while(node && count < 20) {  // Limit output
    printf("  [pid=%d, va=0x%lx]\n", node->proc->pid, node->va);
    node = node->next;
    count++;
  }
  
  if(node)
    printf("  ... (more pages)\n");
  
  release(&mru.lock);
}

// Get page statistics for a process
int
getpagestat_k(int pid, struct pagestat *st)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->pid == pid) {
      st->num_pagefaults = p->pstat.num_pagefaults;
      st->num_swapins = p->pstat.num_swapins;
      st->num_swapouts = p->pstat.num_swapouts;
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  
  return -1;
}