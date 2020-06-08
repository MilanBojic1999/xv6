#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/multiuser.h"

int atoi8(const char* num);

int
main(int argc, char *argv[])
{
	int i;
	if(argc < 3){
		printf("Not enough argument\n");
		exit();
	}

	int num=atoi8(argv[1]);


	int mod;

	if(num<0){
		int mode=0;
		char remove=1;
		switch(argv[1][2]){
			case 'r':
				mode=4;
				break;
			case 'w':
				mode=2;
				break;
			case 'x':
				mode=1;
				break;
			default:
				printf("Not supported action\n");
				exit();

		}

		switch(argv[1][0]){
			case 'u':
				mode= mode << 6;
				break;
			case 'g':
				mode= mode << 3;
				break;
			case 'o':
				break;
			case 'a':
				mode=mode + (mode << 3) + (mode << 6);
				break;
			default:
				printf("Not supported action\n");
				exit();
		}

		if(argv[1][1]=='-'){
			remove=1;
		}else if(argv[1][1]=='+'){
			remove=0;
		}else{
			printf("Not supported action\n");
				exit();
		}

		int fd;
		struct stat* stat=(struct stat*)malloc(sizeof(struct stat));
		int oldmode;
		for(int i=2;i<argc;++i){
			fd=open(argv[i],O_RDWR);
			if(fd<0){
				printf("You are not allowed to open %s\n",argv[i]);
			}else{
				fstat(fd,stat);
				oldmode=stat->mod;
				if(remove)
					chmod(fd,oldmode & ~mode);
				else
					chmod(fd,oldmode | mode);
				close(fd);
			}
		}
		free(stat);
	}else{
		if(num > 0777){
			printf("Bad mode %d\n",num);
			exit();
		}

		mod=num;
		
		int fd;
		for(int i=2;i<argc;++i){
			fd=open(argv[i],O_RDWR);
			if(fd<0){
				printf("You are not allowed to open %s\n",argv[i]);

			}else{
				chmod(fd,mod);
				close(fd);
			}
		}
	}

	exit();
}

int atoi8(const char* s){
	if(*s < '0' || *s >'7')
		return -1;

	int n=0;

	while('0' <= *s && *s <= '7')
		n = n*8 + *s++ - '0';
	return n;

}

