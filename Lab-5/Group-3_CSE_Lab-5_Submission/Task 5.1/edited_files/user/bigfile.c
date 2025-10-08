#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define FILENAME "bigfile"
#define BLOCK_SIZE 1024  // Block size in xv6
#define NUM_BLOCKS 3000  // Number of blocks to write (exceeds single-indirect limit)
#define FILE_SIZE (BLOCK_SIZE * NUM_BLOCKS)

void write_bigfile() {
  int fd = open(FILENAME, O_CREATE | O_WRONLY);
  if (fd < 0) {
    printf("bigfile: failed to create file %s\n", FILENAME);
    exit(1);
  }

  printf("bigfile: writing %d blocks (%d bytes) to %s...\n", NUM_BLOCKS, FILE_SIZE, FILENAME);

  char buffer[BLOCK_SIZE];
  for (int i = 0; i < NUM_BLOCKS; i++) {
    // Fill the buffer with a pattern based on the block number
    for (int j = 0; j < BLOCK_SIZE; j++) {
      buffer[j] = (i + j) % 256;
    }

    // Write the buffer to the file
    if (write(fd, buffer, BLOCK_SIZE) != BLOCK_SIZE) {
      printf("bigfile: write failed at block %d\n", i);
      close(fd);
      exit(1);
    }

    // Print progress every 500 blocks
    if (i % 500 == 0) {
      printf("bigfile: written %d/%d blocks\n", i, NUM_BLOCKS);
    }
  }

  printf("bigfile: successfully wrote %d blocks\n", NUM_BLOCKS);
  close(fd);
}

void verify_bigfile() {
  int fd = open(FILENAME, O_RDONLY);
  if (fd < 0) {
    printf("bigfile: failed to open file %s for reading\n", FILENAME);
    exit(1);
  }

  printf("bigfile: verifying %d blocks (%d bytes) in %s...\n", NUM_BLOCKS, FILE_SIZE, FILENAME);

  char buffer[BLOCK_SIZE];
  char expected[BLOCK_SIZE];
  for (int i = 0; i < NUM_BLOCKS; i++) {
    // Fill the expected buffer with the same pattern used during writing
    for (int j = 0; j < BLOCK_SIZE; j++) {
      expected[j] = (i + j) % 256;
    }

    // Read a block from the file
    if (read(fd, buffer, BLOCK_SIZE) != BLOCK_SIZE) {
      printf("bigfile: read failed at block %d\n", i);
      close(fd);
      exit(1);
    }

    // Verify the block's contents
    if (memcmp(buffer, expected, BLOCK_SIZE) != 0) {
      printf("bigfile: data mismatch at block %d\n", i);
      close(fd);
      exit(1);
    }

    // Print progress every 500 blocks
    if (i % 500 == 0) {
      printf("bigfile: verified %d/%d blocks\n", i, NUM_BLOCKS);
    }
  }

  printf("bigfile: successfully verified %d blocks\n", NUM_BLOCKS);
  close(fd);
}

int main(void) {
  printf("bigfile: starting test\n");

  // Step 1: Write a large file
  write_bigfile();

  // Step 2: Verify the file's contents
  verify_bigfile();

  printf("bigfile: test completed successfully\n");
  exit(0);
}