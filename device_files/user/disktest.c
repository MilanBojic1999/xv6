#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"

void doit(int block){
}

int
main(int argc, char *argv[])
{	
	int dd=open("/dev/disk",O_RDWR);
	char buf[2024];

	if(argc==2){
		int n=atoi(argv[1]);
		lseek(dd,512*n,0);
	}else{
		char *test="Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
		lseek(dd,512*560+1,0);
		write(dd,test,128);	
		lseek(dd,-64,1);
	}
	read(dd,buf,2024);
	printf("%s\n",buf);

	close(dd);
	exit();
}
