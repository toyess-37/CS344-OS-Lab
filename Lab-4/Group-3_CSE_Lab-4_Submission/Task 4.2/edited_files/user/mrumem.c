#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096
#define NUM_PAGES 50
#define MEM_SIZE (NUM_PAGES * PGSIZE)

int main(void) {
  printf("\n=== MRU Demand Paging Test ===\n\n");

  // Test 1: Lazy Allocation + Demand Paging
  printf("[TEST 1] Lazy Allocation & Demand Paging\n");
  printf("Allocating %d pages (%d bytes)...\n", NUM_PAGES, MEM_SIZE);
  
  char *mem = sbrklazy(MEM_SIZE);
  if (mem == (char *)-1) {
    printf("FAIL: sbrklazy failed\n");
    exit(1);
  }
  printf("Allocated at %p\n", mem);

  printf("Writing to all pages to trigger page faults...\n");
  for (int i = 0; i < NUM_PAGES; i++) {
    mem[i * PGSIZE] = (char)(i % 256);
  }
  printf("All pages accessed\n");

  struct pagestat st;
  if(getpagestat(getpid(), &st) == 0) {
    printf("  PageFaults=%d, SwapIns=%d, SwapOuts=%d\n\n",
           st.num_pagefaults, st.num_swapins, st.num_swapouts);
  }
  dumpmru();

  // Test 2: Data Integrity
  printf("[TEST 2] Data Integrity Check\n");
  printf("Verifying all pages have correct values...\n");
  
  int errors = 0;
  for (int i = 0; i < NUM_PAGES; i++) {
    char expected = (char)(i % 256);
    if (mem[i * PGSIZE] != expected) {
      printf("  ERROR: Page %d - expected %d, got %d\n", 
             i, expected, mem[i * PGSIZE]);
      errors++;
    }
  }

  if (errors == 0) {
    printf("All %d pages have correct data\n\n", NUM_PAGES);
  } else {
    printf("FAIL: %d pages corrupted\n\n", errors);
  }

  // Final stats
  if(getpagestat(getpid(), &st) == 0) {
    printf("Final Stats:\n");
    printf("  PageFaults=%d, SwapIns=%d, SwapOuts=%d\n",
           st.num_pagefaults, st.num_swapins, st.num_swapouts);
  }
  dumpmru();

  if (errors == 0 && st.num_pagefaults >= NUM_PAGES) {
    printf("\nALL TESTS PASSED\n");
  } else {
    printf("\nSOME TESTS FAILED\n");
  }

  exit(0);
}