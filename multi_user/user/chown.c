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

	if(argv[1][0]==':' && strlen(argv[1])==1){
		printf("Nothing has changed\n");
		exit();
	}

	char* name=(char*)malloc(sizeof(char)*16);
	char* group=(char*)malloc(sizeof(char)*16);

	int n=0, g=0;

	char bol=1;

	for(int i=0;argv[1][i];++i){
		if(argv[1][i]==':'){
			bol=0;
		}else if(bol){
			name[n++]=argv[1][i];
		}else{
			group[g++]=argv[1][i];
		}
	}

	name[n]='\0';
	group[g]='\0';

	int uid=atoi(name);
	int gid=atoi(group);

	char uidused=1;
	char gidused=1;

	if(uid==0 && name[0]!='0'){
		uidused=0;
	}

	if(gid==0 && group[0]!='0'){
		gidused=0;
	}

	if(n==0){
		uid = -10;
	}else if(uidused){
		int ufd;
		if((ufd=open("/etc/passwd",O_RDWR))<0){
			ufd=open("/etc/passwd",O_RDWR|O_CREATE);
			if(ufd<0){
				printf("You are not allowed to access this file\n");
				exit();
			}
		}

		char buf[128];
		while(1){
			getline(buf,128,ufd);
			if(strlen(buf)<1){
				printf("There is no user with %d uid\n",uid);
				close(ufd);
				exit();
			}
			if(uid==line2uid(buf)){
				break;
			}
		}
		close(ufd);
	}else{
		int ufd;
		if((ufd=open("/etc/passwd",O_RDWR))<0){
			ufd=open("/etc/passwd",O_RDWR|O_CREATE);
			if(ufd<0){
				printf("You are not allowed to access this file\n");
				exit();
			}
		}

		char buf[128];
		while(1){
			getline(buf,128,ufd);
			if(strlen(buf)<1){
				printf("There is no user with name %s\n",name);
				close(ufd);
				exit();
			}
			if(strcmp(name,line2uname(buf))==0){
				uid=line2uid(buf);
				break;
			}
		}
		close(ufd);
	}

	if(g==0){
		gid = -10;
	}else if(gidused){
		int gfd;
		if((gfd=open("/etc/group",O_RDWR))<0){
			gfd=open("/etc/group",O_RDWR|O_CREATE);
			if(gfd<0){
				printf("You are not allowed to access this file\n");
				exit();
			}
		}

		char buf[128];
		while(1){
			getline(buf,128,gfd);
			if(strlen(buf)<1){
				printf("There is no group with %d gid\n",gid);
				close(gfd);
				exit();
			}
			if(gid==line2gid2(buf)){
				break;
			}
		}
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

		char buf[128];
		while(1){
			getline(buf,128,gfd);
			if(strlen(buf)<1){
				printf("There is no group with %d gid\n",gid);
				close(gfd);
				exit();
			}
			if(strcmp(group,line2gname(buf))==0){
				gid=line2gid2(buf);
				break;
			}
		}
		close(gfd);
	}

	printf("%d : %d\n",uid,gid);

	int fd;
	int nuid,ngid;
	struct stat* stat=(struct stat*)malloc(sizeof(struct stat));

	for(int i=2;i<argc;++i){
		fd=open(argv[i],O_RDWR);

		if(fd<0){
			printf("You are not allowed to open %s\n",argv[i]);
		}else{
			fstat(fd,stat);
			
			if(uid<0){
				nuid=stat->uid;
			}else{
				nuid=uid;
			}

			if(gid<0){
				ngid=stat->gid;
			}else{
				ngid=gid;
			}

			chown(fd,nuid,ngid);
			fstat(fd,stat);
			close(fd);
		}
	}
	free(stat);
	exit();
}
