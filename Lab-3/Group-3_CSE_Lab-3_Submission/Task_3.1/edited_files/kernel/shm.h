#define MAX_SHM   64
#define SHM_BASE  ((uint64)0x40000000ULL)
#define SHMVA(slot) (SHM_BASE + ((uint64)(slot) * PGSIZE)) // virtual address slots

struct shm_region {
  int used;     // 1 if allocated
  int key;      // application key
  char *page;     // physical page
  int ref_count;   // mappings of the processes
};

void shminit(void);
int shm_create(int key); // returns the id if successful, -1 if failed
uint64 shm_get(int key); // returns the VA if successful, 0 if failed
int shm_close(int key); // returns 0 if successful, -1 if failed
void shm_cleanup(struct proc *p); // unmap any shared memory for p