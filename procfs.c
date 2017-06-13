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

struct process_entry {
	int pid;
	int used;
	struct inode* cwd;
};

struct process_entry process_entries[MAX_NUM_PROCESSES];

struct dirent dir_entries[MAX_NUM_PROCESSES+2];

struct dirent subdir_entries[7];

char blockstat_str[1024];

void itoa(char *buf, int num) {
	int num_digits = 0;
	int temp_num = num;
	while (temp_num > 0) {
		num_digits++;
		temp_num = temp_num / 10;
	}

	int i = num_digits - 1;
	while (num > 0) {
		buf[i] = (char)(num % 10) + '0';
		i--;
		num = num / 10;
	}
	buf[i] = 0;
}

int atoi(char *buf) {
	int num = 0;

	while (*buf != '\0') {
		num = num * 10 + ((int)(*buf) - '0');
		buf++;
	}
	cprintf("%d\n", num);
	return num;
}

void update_dir_entries() {
	int i, j = 4, inum = 5;
	memset(dir_entries, sizeof(dir_entries), 0);

	memmove(&dir_entries[0].name, ".", 2);
	memmove(&dir_entries[1].name, "..", 3);
	memmove(&dir_entries[2].name, "blockstat", 10);
	memmove(&dir_entries[3].name, "inodestat", 10);
	dir_entries[0].inum = 2;
	dir_entries[1].inum = 1;
	dir_entries[2].inum = 3;
	dir_entries[3].inum = 4;

	for (i = 0; i < MAX_NUM_PROCESSES; i++) {
		if (process_entries[i].used) {
			itoa(dir_entries[j].name, process_entries[i].pid);
			dir_entries[j].inum = inum;
			inum++;
			j++;
		}
	}
}

int 
procfsisdir(struct inode *ip) {
	return 1;
}

void 
procfsiread(struct inode* dp, struct inode *ip) {
	// Minor = 0 -- /proc
	// Minor = 1 -- subdir in /proc (inum is the index of the dirent)
	// Minor > 1 -- file in subdir in /proc (minor is the index of the subdir, inum is the index of the file)

	if (dp->minor == 0)
  		ip->minor = 1;
  	else if (dp->minor == 1)
  		ip->minor = dp->inum;
  	else 
  		panic("Invalid minor of dp!");

	ip->type = T_DEV;
  	ip->major = 2;
  	ip->nlink = 1;

  	if (!(ip->flags & I_VALID))
		ip->flags |= I_VALID;

	//cprintf("IREAD: %x %d %d %d %d %x %d %d %d %d\n", ip, ip->type, ip->major, ip->minor, ip->inum, dp, dp->type, dp->major, dp->minor, dp->inum);
}

int read_parent(char *dst, int off, int n) {
	update_dir_entries();

	if (off + n > sizeof(dir_entries))
		n = sizeof(dir_entries) - off;
	memmove(dst, (char*)(&dir_entries) + off, n);
	return n;
}

int read_dir(char *dst, int off, int n) {
	if (off + n > sizeof(subdir_entries))
		n = sizeof(subdir_entries) - off;
	memmove(dst, (char*)(&subdir_entries) + off, n);
	return n;
}

int read_cwd(int pid, char *dst, int off, int n) {
	memset(blockstat_str, sizeof(blockstat_str), 0);
	return 0;
}

int read_file(struct inode* ip, char *dst, int off, int n) {
	int i;
	for (i = 0; i < MAX_NUM_PROCESSES; i++) {
		if (dir_entries[i].inum == ip->minor) {
			switch (ip->inum) {
				case 3:
					return read_cwd(atoi(dir_entries[i].name), dst, off, n);
				case 4:
					return read_cwd(atoi(dir_entries[i].name), dst, off, n);
				case 5:
					return read_cwd(atoi(dir_entries[i].name), dst, off, n);
				default:
					panic("Unknown file requested!");
			}
		}
	}
	return 0;
}

int
procfsread(struct inode *ip, char *dst, int off, int n) {
	if (ip->minor == 0) 
		return read_parent(dst, off, n);
	else if (ip->minor == 1)
		return read_dir(dst, off, n);
	else
		return read_file(ip, dst, off, n);
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

  memset(process_entries, sizeof(process_entries), 0);

  memmove(subdir_entries[0].name, ".", 2);
  memmove(subdir_entries[1].name, "..", 3);
  memmove(subdir_entries[2].name, "cwd", 4);
  memmove(subdir_entries[3].name, "fdinfo", 7);
  memmove(subdir_entries[4].name, "status", 7);

  int i;
  subdir_entries[0].inum = 2;
  subdir_entries[1].inum = 1;
  for (i = 2; i < 5; i++)
  	subdir_entries[i].inum = i + 1;
}

void procfs_add_proc(int pid, char* cwd) {
	int i;
	cprintf("procfs_add_proc: %d\n", pid);
	for (i = 0; i < MAX_NUM_PROCESSES; i++) {
		if (!process_entries[i].used) {
			process_entries[i].used = 1;
			process_entries[i].pid = pid;
			memmove(&process_entries[i].cwd, cwd, strlen(cwd));
			return;
		}
	}
	panic("Too many processes in procfs!");
}

void procfs_remove_proc(int pid) {
	cprintf("procfs_remove_proc: %d\n", pid);
	int i;
	for (i = 0; i < MAX_NUM_PROCESSES; i++) {
		if (process_entries[i].used && process_entries[i].pid == pid) {
			process_entries[i].used = process_entries[i].pid = 0;
			process_entries[i].cwd = 0;
			return;
		}
	}
	panic("Failed to find process in procfs_remove_proc!");
}

void procfs_update_cwd(int pid, struct inode* cwd) {
	cprintf("procfs_add_proc: %d %x\n", pid, cwd);
	int i;
	for (i = 0; i < MAX_NUM_PROCESSES; i++) {
		if (process_entries[i].used && process_entries[i].pid == pid) {
			process_entries[i].cwd = cwd;
			return;
		}
	}
	panic("Can't find process in procfs_update_cwd!");
}