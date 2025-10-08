# CS344 Operating Systems Labs

This repo keeps my solutions for the CS344 operating systems labs that we solved on top of the xv6 code base. 
Each lab folder has the instructions PDF that the prof gave us plus the code edits we turned in. 
The notes below summarize what every lab is trying to build and the main code ideas.

## Lab 2 – CPU Scheduling Experiments

Lab 2 was about exploring different ways to share the CPU between processes.

### Task 2.1 – Weighted Round Robin
* Goal: give each process a time slice that depends on a per-process priority value.
* Key edits:
  * Added `priority` and `ticks` fields to `struct proc` so the kernel can remember each process’ weight and how much of its slice it has already used.
  * Added `setpriority`/`getpriority` syscalls with simple range checking so user programs can pick a weight from 1 to a max value.
  * Inside the timer interrupt path (`usertrap`/`kerneltrap`) I bumped a process’ `ticks` every time it ran and forced a `yield()` once the ticks hit the chosen priority. That made the scheduler keep a round-robin order but with variable-length quanta.
  * Dropped in a tiny user demo program that forks three kids with different priorities so we can watch the high priority process getting longer bursts.

### Task 2.2 – Lottery Scheduler
* Goal: pick the next process to run using randomness and ticket counts.
* Key edits:
  * Gave each process a `tickets` field plus a simple linear congruential random generator inside `proc.c`.
  * Rewrote the `scheduler()` loop so it adds up the total tickets of runnable processes, draws a winning ticket, and scans for the lucky process. Children inherit their parent’s ticket count.
  * Added the `settickets`/`gettickets` syscalls so user code can adjust its ticket count.
  * The user demo spawns two CPU-bound kids with different ticket counts and prints how many iterations each one manages to finish, showing the weighted share in action.

## Lab 3 – Shared Memory and Mailboxes

Lab 3 moved on to inter-process communication and coordination.

### Task 3.1 – Shared Memory Pages and Mailboxes
* Goal: extend xv6 with two IPC features: shared memory segments and simple mailboxes.
* Key edits:
  * Boot path now calls `shminit()` and `mboxinit()` so the new subsystems are ready before user space starts.
  * Added kernel helpers for creating/getting/closing shared memory slots and mailbox send/receive calls, along with the matching syscall numbers, handlers, and user-space wrappers.
  * Hooked `shm_cleanup(p)` into `freeproc()` so we release shared pages when a process exits.
  * Provided user-space test programs `shmtest` and `mboxtest` to show two processes sharing a string and ping-ponging numbers through a mailbox.

### Task 3.2 – Coordinated Maze Game
* Goal: use the shared memory + mailbox primitives from Task 3.1 to coordinate two worker processes in a toy maze game.
* Key edits:
  * Added a `master` user program that sets up the shared memory for the maze state, spawns two child processes, and prints whether they win or lose after exploring.
  * Added a `process` user program that receives its role (A or B), attaches to the shared state, and exchanges moves via the mailboxes. It keeps things in bounds and marks progress in shared memory.
  * Included a small integer-to-string helper in `ulib.c` so we could format numbers without pulling in heavy code.

## Lab 5 – File System Growth

Lab 5 focused on storage: making the file system support larger files and symbolic links.

### Task 5.1 – Doubly Indirect Blocks for Large Files
* Goal: let xv6 create files that need more data blocks than a single indirect block can cover.
* Key edits:
  * Increased the on-disk size limit by redefining `MAXFILE` and introducing a `NDOUBLY_INDIRECT` slot in the inode (`addrs[NDIRECT+1]`).
  * Extended `bmap()` so when a block number falls beyond the direct and single-indirect ranges it allocates a doubly indirect block, then walks two levels to find or allocate the final data block.
  * Updated `itrunc()` to walk the two-level structure and free every inner block plus the outer indirection block.
  * Tweaked `FSSIZE` to a much bigger value and wrote a `bigfile` user test that writes and reads 3000 blocks to confirm everything works.

### Task 5.2 – Symbolic Links
* Goal: add a `symlink(target, path)` system call and make `open()` optionally follow symlinks.
* Key edits:
  * Introduced the `T_SYMLINK` inode type along with an `O_NOFOLLOW` flag for `open()`.
  * Added the `symlink` syscall that creates a new inode, stores the target path string inside it, and registers the syscall with the dispatcher plus user stubs.
  * Taught `sys_open()` to detect symlinks: by default it reads the stored path and resolves it (with a small depth limit to avoid cycles), but if the caller passes `O_NOFOLLOW` it leaves the link unopened so user code can read the link contents directly.
  * Added a `symlinktest` user program to poke the new behavior.

## How to Use the Repo

Each lab folder keeps:
* the original PDF instructions from the course,
* an `edit_info.txt` file describing the code patches, and
* the modified source arranged in `edited_files/`.

P.S.: Some (logical) errors in parts of implementation of Assignment 4 remain to be fixed. And, my Assignment 1 implementation was trash (but it worked!).
