#include "types.h"
#include "stat.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

#define MAX_NUM_PROCESSES 50

struct dir_info {
	int pid;
	int used;
};

struct dir_info entries[MAX_NUM_PROCESSES];

int 
procfsisdir(struct inode *ip) {
  cprintf("ISDIR\n");
  return 1;
}

void 
procfsiread(struct inode* dp, struct inode *ip) {
	cprintf("IREAD\n");
	ip->flags &= I_VALID;
}

int
procfsread(struct inode *ip, char *dst, int off, int n) {
	return 0;
}

int
procfswrite(struct inode *ip, char *buf, int n)
{
	panic("Write called for /proc!");
    return 0;
}

void
procfsinit(void)
{
  devsw[PROCFS].isdir = procfsisdir;
  devsw[PROCFS].iread = procfsiread;
  devsw[PROCFS].write = procfswrite;
  devsw[PROCFS].read = procfsread;

  memset(entries, sizeof(entries), 0);
}

void procfs_add_proc(int pid) {
	int i;
	cprintf("procfs_add_proc: %d\n", pid);
	for (i = 0; i < MAX_NUM_PROCESSES; i++) {
		if (!entries[i].used) {
			entries[i].used = 1;
			entries[i].pid = pid;
			return;
		}
	}
	panic("Too many processes in procfs!");
}

void procfs_remove_proc(int pid) {
	cprintf("procfs_remove_proc: %d\n", pid);
	int i;
	for (i = 0; i < MAX_NUM_PROCESSES; i++) {
		if (entries[i].used && entries[i].pid == pid) {
			entries[i].used = 0;
			entries[i].pid = 0;
			return;
		}
	}
	panic("Failed to find process in procfs_remove_proc!");
}