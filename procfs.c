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

#define PROCFS_DIR 		0
#define PROCFS_PID		1
#define PROCFS_FILE 	2
#define PROCFS_FD		3


struct process_entry {
	int pid;
	int used;
};

struct process_entry process_entries[MAX_NUM_PROCESSES];

struct dirent dir_entries[MAX_NUM_PROCESSES+2];

struct dirent subdir_entries[5];

char blockstat_str[1024];

int procfs_layer_inums[4] = {0};

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
	return num;
}

void update_dir_entries(int inum) {
	int i, j = 4, index = 5;
	memset(dir_entries, sizeof(dir_entries), 0);

	memmove(&dir_entries[0].name, ".", 2);
	memmove(&dir_entries[1].name, "..", 3);
	memmove(&dir_entries[2].name, "blockstat", 10);
	memmove(&dir_entries[3].name, "inodestat", 10);
	dir_entries[0].inum = inum;
	dir_entries[1].inum = 1;
	dir_entries[2].inum = 3;
	dir_entries[3].inum = 4;

	for (i = 0; i < MAX_NUM_PROCESSES; i++) {
		if (process_entries[i].used) {
			itoa(dir_entries[j].name, process_entries[i].pid);
			dir_entries[j].inum = index;
			index++;
			j++;
		}
	}
}

int 
procfsisdir(struct inode *ip) {
	if (ip->minor == PROCFS_DIR || ip->minor == PROCFS_PID || ip->minor == PROCFS_FILE)
		return 1;
	else
		return 0;
}

void 
procfsiread(struct inode* dp, struct inode *ip) {
	ip->type = T_DEV;
  	ip->major = 2;
  	ip->minor = dp->minor + 1;
  	ip->size = 0;
	ip->flags |= I_VALID;
	ip->nlink = 1;

	procfs_layer_inums[dp->minor] = dp->inum;
}

int read_procfs_dir(struct inode* ip, char *dst, int off, int n) {
	update_dir_entries(ip->inum);

	if (off + n > sizeof(dir_entries))
		n = sizeof(dir_entries) - off;
	memmove(dst, (char*)(&dir_entries) + off, n);
	return n;
}

int find_pid_by_inum(int inum) {
	int i;
	for (i = 0; i < MAX_NUM_PROCESSES+2; i++)
		if (dir_entries[i].inum == inum)
			return atoi(dir_entries[i].name);
	panic("find_pid_by_inum");
}

int read_procfs_pid(struct inode* ip, char *dst, int off, int n) {
	struct dirent temp_entries[5];
	memmove(&temp_entries, &subdir_entries, sizeof(subdir_entries));

	temp_entries[0].inum = ip->inum;
	temp_entries[1].inum = procfs_layer_inums[PROCFS_DIR];

	temp_entries[2].inum = find_cwd_by_pid(find_pid_by_inum(ip->inum))->inum;

	int i, index = 2;
	for (i = 3; i < 5; i++) {
		temp_entries[i].inum = ip->inum * 100 + i;
		index++;
	}

	if (off + n > sizeof(temp_entries))
		n = sizeof(temp_entries) - off;
	memmove(dst, (char*)(&temp_entries) + off, n);
	return n;
}

int read_procfs_file(struct inode* ip, char *dst, int off, int n) {
	/*int i;
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
	}*/
	return 0;
}

int read_procfs_fd(struct inode* ip, char *dst, int off, int n) {
	return 0;
}

int
procfsread(struct inode *ip, char *dst, int off, int n) {
	//cprintf("READ: %d %d\n", ip->inum, ip->minor);
	switch(ip->minor)
	{
		case PROCFS_DIR:
			return read_procfs_dir(ip, dst, off, n);
		case PROCFS_PID:
			return read_procfs_pid(ip, dst, off, n);
			break;
		case PROCFS_FILE:
			return read_procfs_file(ip, dst, off, n);
		case PROCFS_FD:
			return read_procfs_fd(ip, dst, off, n);
		default:
			panic("procfsread: invalid minor!");
	}
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

  memset(&process_entries, sizeof(process_entries), 0);

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
			return;
		}
	}
	panic("Failed to find process in procfs_remove_proc!");
}