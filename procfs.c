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

#define INUM_CWD 2
#define INUM_FDINFO 3
#define INUM_STATUS 4

struct process_entry {
	int pid;
	int used;
};

struct process_entry process_entries[MAX_NUM_PROCESSES];

struct dirent dir_entries[MAX_NUM_PROCESSES+2];

struct dirent subdir_entries[5];

struct dirent fd_dir_entries[NOFILE];

int root_inum = 1;
int proc_inum = 0;

void itoa(char s[], int n)
 {
     int i=0, pm=n;
     int t, j;
     char c;
     
     if (pm < 0)  
         n = -n;          

     do 
     {       
         s[i++] = n % 10 + '0';   
     } while ((n /= 10) > 0);
     
     if (pm < 0)
         s[i++] = '-';
     
     s[i] = '\0';
     
     //reverse
     for (t = 0, j = strlen(s)-1; t<j; t++, j--) 
     {
         c = s[t];
         s[t] = s[j];
         s[j] = c;
     }
 }

int atoi(char *buf) {
	int num = 0;

	while (*buf != '\0') {
		num = num * 10 + ((int)(*buf) - '0');
		buf++;
	}
	return num;
}

char*
strcpy(char *s, char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int find_pid_by_inum(int inum) {
	int i;
	for (i = 0; i < MAX_NUM_PROCESSES+2; i++)
		if (dir_entries[i].inum == inum)
			return atoi(dir_entries[i].name);
	panic("find_pid_by_inum");
}

int find_fd_by_inum(int inum) {
	int i;
	for (i = 0; i < NOFILE; i++)
		if (fd_dir_entries[i].inum == inum)
			return atoi(fd_dir_entries[i].name);
	panic("find_fd_by_inum");
}

void update_dir_entries(int inum) {
	int i, j = 4, index = 202;
	memset(dir_entries, sizeof(dir_entries), 0);

	memmove(&dir_entries[0].name, ".", 2);
	memmove(&dir_entries[1].name, "..", 3);
	memmove(&dir_entries[2].name, "blockstat", 10);
	memmove(&dir_entries[3].name, "inodestat", 10);
	dir_entries[0].inum = inum;
	dir_entries[1].inum = root_inum;
	dir_entries[2].inum = 200;
	dir_entries[3].inum = 201;

	for (i = 0; i < MAX_NUM_PROCESSES; i++) {
		if (process_entries[i].used) {
			itoa(dir_entries[j].name, process_entries[i].pid);
			dir_entries[j].inum = index;
			index++;
			j++;
		}
	}
}

void update_fd_entries(int inum) {
	int i, j = 2, index = 2;
	memset(fd_dir_entries, sizeof(fd_dir_entries), 0);

	memmove(&fd_dir_entries[0].name, ".", 2);
	memmove(&fd_dir_entries[1].name, "..", 3);
	fd_dir_entries[0].inum = inum;
	fd_dir_entries[1].inum = inum / 10;

	struct proc* p = find_proc_by_pid(find_pid_by_inum(inum / 10));
	for(i = 0; i < NOFILE; i++) {
    	if(p->ofile[i]) {
    		itoa(fd_dir_entries[j].name, i);
    		fd_dir_entries[j].inum = inum * 10 + index;
    		index++;
    		j++;
    	}
   	}
}

int 
procfsisdir(struct inode *ip) {
	//cprintf("isdir: %d %d\n", ip->minor, ip->inum);
	if (ip->minor == 0 || (ip->minor == 1 && ip->minor != 200 && ip->minor != 201) || (ip->minor == 2 && ip->inum % 10 == INUM_FDINFO))
		return 1;
	else 
		return 0;
}

void 
procfsiread(struct inode* dp, struct inode *ip)
{
	if (ip->inum < 200)
		return;

	ip->major = PROCFS;
	ip->size = 0;
	ip->nlink = 1;
	ip->type = T_DEV;
	ip->flags |= I_VALID;
	ip->minor = dp->minor + 1;

	if (dp->inum < ip->inum)
		ip->minor = dp->minor + 1;
	else if (dp->inum < ip->inum)
		ip->minor = dp->minor - 1;
}

int read_procfs_layer_0(struct inode* ip, char *dst, int off, int n) {
	update_dir_entries(ip->inum);

	if (off + n > sizeof(dir_entries))
		n = sizeof(dir_entries) - off;
	memmove(dst, (char*)(&dir_entries) + off, n);
	return n;
}

int read_procfs_pid_dir(struct inode* ip, char *dst, int off, int n) {
	struct dirent temp_entries[5];
	memmove(&temp_entries, &subdir_entries, sizeof(subdir_entries));

	temp_entries[0].inum = ip->inum;
	temp_entries[1].inum = proc_inum;

	temp_entries[2].inum = find_proc_by_pid(find_pid_by_inum(ip->inum))->cwd->inum;

	int i, index = 2;
	for (i = 3; i < 5; i++) {
		temp_entries[i].inum = ip->inum * 10 + i;
		index++;
	}

	if (off + n > sizeof(temp_entries))
		n = sizeof(temp_entries) - off;
	memmove(dst, (char*)(&temp_entries) + off, n);
	return n;
}

int read_procfs_blockstat(struct inode* ip, char* dst, int off, int n) {
	if (n == sizeof(struct dirent))
		return 0;

	int free_blocks = 0, total_blocks = 0, num_hits = 0, num_tries = 0;
	blockstat(&free_blocks, &total_blocks, &num_hits, &num_tries);

	char data[256] = {0};

	strcpy(data, "Free blocks: ");
	itoa(data + strlen(data), free_blocks);
	strcpy(data + strlen(data), "\nTotal blocks: ");
	itoa(data + strlen(data), total_blocks);
	strcpy(data + strlen(data), "\nHit ratio: ");
	itoa(data + strlen(data), num_hits);
	strcpy(data + strlen(data), " / ");
	itoa(data + strlen(data), num_tries);
	strcpy(data + strlen(data), "\n");

	if (off + n > strlen(data))
		n = strlen(data) - off;
	memmove(dst, (char*)(&data) + off, n);
	return n;
}

int read_procfs_inodestat(struct inode* ip, char* dst, int off, int n) {
	if (n == sizeof(struct dirent))
		return 0;

	char data[256] = {0};

	strcpy(data, "Free inodes: ");
	itoa(data + strlen(data), get_free_inodes());
	strcpy(data + strlen(data), "\nValid inodes: ");
	itoa(data + strlen(data), get_valid_inodes());
	strcpy(data + strlen(data), "\nRefs per inode: ");
	itoa(data + strlen(data), get_total_refs());
	strcpy(data + strlen(data), " / ");
	itoa(data + strlen(data), get_used_inodes());
	strcpy(data + strlen(data), "\n");

	if (off + n > strlen(data))
		n = strlen(data) - off;
	memmove(dst, (char*)(&data) + off, n);
	return n;
}

int read_procfs_layer_1(struct inode* ip, char *dst, int off, int n) {
	switch (ip->inum) {
		case 200:
			return read_procfs_blockstat(ip, dst, off, n);
		case 201:
			return read_procfs_inodestat(ip, dst, off, n);
		default:
			return read_procfs_pid_dir(ip, dst, off, n);
	}
}

int read_procfs_file_status(struct inode* ip, char *dst, int off, int n) {
	char status[250] = {0};
	char szBuf[100] = {0};
	char* procstate[6] = { "UNUSED", "EMBRYO", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE" };
    struct proc* p = 0;
    int pid = 0;

	pid = find_pid_by_inum(ip->inum / 10);
    p = find_proc_by_pid(pid);

    int size = strlen(procstate[p->state]);
    strcpy(status, procstate[p->state]);
    status[size] = ' ';
    itoa(szBuf, p->sz);
    strcpy(status + size + 1, szBuf);

    status[strlen(status)] = '\n';

    int status_size = strlen(status);

    if (off + n > status_size)
        n = status_size - off;
    memmove(dst, (char*)(&status) + off, n);
    return n;
}

int read_procfs_fdinfo(struct inode* ip, char *dst, int off, int n) {
	update_fd_entries(ip->inum);

	if (off + n > sizeof(fd_dir_entries))
		n = sizeof(fd_dir_entries) - off;
	memmove(dst, (char*)(&fd_dir_entries) + off, n);
	return n;
}

int read_procfs_layer_2(struct inode* ip, char *dst, int off, int n) {
	switch (ip->inum % 10) {
	    case INUM_CWD:
	        panic("procfsread called for cwd!");
	        break;
        case INUM_FDINFO:
            return read_procfs_fdinfo(ip, dst, off, n);
            break;
        case INUM_STATUS:
        	return read_procfs_file_status(ip, dst, off, n);
	}
	return 0;
}

int read_procfs_layer_3(struct inode* ip, char *dst, int off, int n) {
	if (n == sizeof(struct dirent))
		return 0;

	struct proc* p = find_proc_by_pid(find_pid_by_inum(ip->inum / 100));
	int fd = find_fd_by_inum(ip->inum);
	struct file* f = p->ofile[fd];

	char *file_types[3] = { "FD_NONE", "FD_PIPE", "FD_INODE" };

	char data[250] = {0};
	strcpy(data, "Refs: ");
	itoa(data + strlen(data), f->ref);
	strcpy(data + strlen(data), " Inode: ");
	itoa(data + strlen(data), f->ip->inum);
	strcpy(data + strlen(data), " Type: ");
	strcpy(data + strlen(data), file_types[f->type]);
	strcpy(data + strlen(data), "\nPosition: ");
	itoa(data + strlen(data), f->off);
	strcpy(data + strlen(data), " Readable: ");
	itoa(data + strlen(data), f->readable);
	strcpy(data + strlen(data), " Writable: ");
	itoa(data + strlen(data), f->writable);
	strcpy(data + strlen(data), "\n");

	if (off + n > strlen(data))
		n = strlen(data) - off;
	memmove(dst, (char*)(&data) + off, n);
	return n;
}

int
procfsread(struct inode *ip, char *dst, int off, int n) {
	//cprintf("READ: %d %d\n", ip->inum, ip->minor);
	switch(ip->minor)
	{
		case 0:
			return read_procfs_layer_0(ip, dst, off, n);
		case 1:
			return read_procfs_layer_1(ip, dst, off, n);
			break;
		case 2:
			return read_procfs_layer_2(ip, dst, off, n);
		case 3:
			return read_procfs_layer_3(ip, dst, off, n);
		default:
			cprintf("Minor: %d", ip->minor);
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
	//cprintf("procfs_add_proc: %d\n", pid);
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
	//cprintf("procfs_remove_proc: %d\n", pid);
	int i;
	for (i = 0; i < MAX_NUM_PROCESSES; i++) {
		if (process_entries[i].used && process_entries[i].pid == pid) {
			process_entries[i].used = process_entries[i].pid = 0;
			return;
		}
	}
	panic("Failed to find process in procfs_remove_proc!");
}