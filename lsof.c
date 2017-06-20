#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

void error(char* msg) {
	printf(2, msg);
	exit();
}

void read_fd_first_line(char* path, char* fd_sz) {
	char full_path[256];
	strcpy(full_path, path);
	strcpy(full_path + strlen(full_path), "/");
	strcpy(full_path + strlen(full_path), fd_sz);

	int fd = open(full_path, O_RDONLY);
	if (fd < 0) {
		printf(1, "PATH: %s\n", full_path);
		error("Failed to open fd file!");
	}

	char data[256];
	read(fd, &data, 256);
	char *p = data;
	while (*p != '\n') {
		if (*p == '\0')
			error("null terminator reached before nl!");
		p++;
	}
	*(++p) = '\0';
	printf(1, data);
	close(fd);
}

void read_pid_dir(char *pid) {
	char path[256];
	strcpy(path, "/proc/");
	strcpy(path + strlen(path), pid);
	strcpy(path + strlen(path), "/fdinfo");

	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		error("Failed to open fdinfo!");
	}

	struct dirent de = {0};
	while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0) 
        continue;
      
      if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
      	continue;

      printf(1, "%s %s ", pid, de.name);
      read_fd_first_line(path, de.name);
    }

    close(fd);
}

int
main(int argc, char *argv[]){
	int fd = open("/proc", O_RDONLY);
	if (fd < 0) 
		error("Failed to open /proc");

	struct dirent de = {0};
	while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0) 
        continue;
      
      if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0 ||
      	  strcmp(de.name, "blockstat") == 0 || strcmp(de.name, "inodestat") == 0)
      	continue;

      read_pid_dir(de.name);
    }

    close(fd);

	exit();
	return 0;
}