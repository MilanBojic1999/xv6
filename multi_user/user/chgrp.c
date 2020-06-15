#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/multiuser.h"

int
main(int argc, char *argv[])
{
	if(argc < 3){
		printf("Not enough argument\n");
		exit();
	}

	int gid = atoi(argv[1]);
	char buf[128];

	if(gid==0 && strlen(argv[1]) > 1){
		gid=-1;
		int gfd;
		if((gfd=open("/etc/group",O_RDWR))<0){
			gfd=open("/etc/group",O_RDWR|O_CREATE);
			if(gfd<0){
				printf("You are not allowed to access this file\n");
				exit();
			}
		}


		do{
			getline(buf,128,gfd);
			if(strlen(buf)<1){
				printf("Group doesn't exist: %s\n",argv[1]);
				close(gfd);
				exit();
			}
			if(strcmp(argv[1],line2gname(buf))==0){
				gid=(line2gid2(buf));
				break;
			}
		}while(1);

		close(gfd);
	}else{
		
		int gfd;
		
		if((gfd=open("/etc/group",O_RDWR))<0){
			gfd=open("/etc/group",O_RDWR|O_CREATE);
			if(gfd<0){
				printf("You are not allowed to access this file\n");
				exit();
			}
		}

		do{
			getline(buf,128,gfd);
			if(strlen(buf)<1){
				printf("Group doesn't exist: gid %d\n",gid);
				close(gfd);
				exit();
			}

			if(gid==line2gid2(buf))
				break;
		}while(1);

		close(gfd);
	}

	if(gid<0){
		printf("Not enop\n");
		exit();
	}

	struct stat* stat=(struct stat*)malloc(sizeof(struct stat));

	int uid;
	int fd;
	for(int i=2;i<argc;++i){
		fd=open(argv[i],O_RDWR);

		if(fd<0){
			printf("You are not allowed to open %s\n",argv[i]);
		}else{
			fstat(fd,stat);
			uid=stat->uid;
			chown(fd,uid,gid);
			fstat(fd,stat);
			close(fd);
		}
	}

	free(stat);

	exit();
}
