// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fcntl.h"

char *argv[] = { "sh", 0 };

void initdev(void){
	mknod("/dev/null",0,1);
	mknod("/dev/zero",0,2);
	mknod("/dev/kmesg",2,1);
	mknod("/dev/random",3,1);
	mknod("/dev/disk",4,1);
}

int
main(void)
{
	int pid, wpid;

	if(getpid() != 1){
		fprintf(2, "init: already running\n");
		exit();
	}

	if(open("/dev/console", O_RDWR) < 0){
		mknod("/dev/console", 1, 1);
		open("/dev/console", O_RDWR);
	}
	dup(0);  // stdout
	dup(0);  // stderr

	initdev();

	for(;;){
		printf("init: starting sh\n");
		pid = fork();
		if(pid < 0){
			printf("init: fork failed\n");
			exit();
		}
		if(pid == 0){
			exec("/bin/sh", argv);
			printf("init: exec sh failed\n");
			exit();
		}
		while((wpid=wait()) >= 0 && wpid != pid)
			printf("zombie!\n");
	}
}
