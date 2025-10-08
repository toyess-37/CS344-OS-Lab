#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

static void die(const char *m){ printf("%s (error)\n", m); exit(1); }

int
main(void)
{
  int fd;
  // 1) base file
  fd = open("t.txt", O_CREATE|O_RDWR); 
  if(fd<0) die("open t.txt");
  if(write(fd, "hello", 5)!=5) die("write t.txt");
  close(fd);

  // 2) make symlink
  if(symlink("t.txt", "l.txt") < 0) die("symlink failed");

  // 3) open through symlink (follow)
  fd = open("l.txt", O_RDONLY); 
  if(fd<0) die("open l.txt");
  char buf[6]={0}; 
  if(read(fd, buf, 5)!=5) die("read via link");
  close(fd);
  if(strcmp(buf, "hello")!=0) die("content mismatch");
  printf("symlink follow OK\n");

  // 4) O_NOFOLLOW: read the link's target path
  fd = open("l.txt", O_RDONLY | O_NOFOLLOW); 
  if(fd<0) die("open nofollow");
  char tbuf[64]={0}; 
  int n = read(fd, tbuf, sizeof(tbuf)-1); 
  close(fd);
  if(n<=0) die("read nofollow");
  printf("nofollow target='%s'\n", tbuf);

  // 5) recursive link & depth limit
  // Try a loop: lA -> lB, lB -> lA
  unlink("lA"); unlink("lB");
  if(symlink("lB", "lA") < 0) die("symlink lA");
  if(symlink("lA", "lB") < 0) die("symlink lB");
  int fdloop = open("lA", O_RDONLY);
  if(fdloop >= 0){ printf("expected loop to fail but it opened\n"); close(fdloop); }
  else { printf("expected loop detection; successful test!\n"); }

  printf("symlink tests done\n");
  exit(0);
}
