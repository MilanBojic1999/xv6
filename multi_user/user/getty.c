#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/multiuser.h"
#include "user.h"

void printissue(void);
void printmotd(void);

char *argv[] = { "sh", 0 };


int
main(int argc, char *argv[])
{	
	int pid, wpid,gid;
	int* groups=(int*)malloc(16*sizeof(int));
	for(;;){
		clear();
		printissue();
		char name[16];
		char passw[16];
		printf("Enter usrname: ");
		gets(name,16);
		name[strlen(name)-1]=0;
		printf("Enter password: ");
		gets(passw,16);
		passw[strlen(passw)-1]=0;

		printmotd();

		struct user *user;
		user=getuser(name,passw);

		if(user==0){
			printf("NO user found\n");
			printf("Please try again\n");
			sleep(200);
			continue;
		}else{
			printf("%d has logged in\n",user->uid);
		}

		memset(groups,-1,16*4);

		gid=open("/etc/group",O_CREATE|O_RDONLY);
		char buf[128];
		int cnt=0;
		char* usernm;
		while(1){
			getline(buf,128,gid);
			if(strlen(buf)<1)
				break;
			for(int i=0;i<16;++i){
				usernm=line2nuser(buf,i);
				if(usernm[0]==0){
					free(usernm);
					usernm=0;
					break;
				}
				if(strcmp(usernm,name)==0){
					groups[cnt++]=line2gid2(buf);
					free(usernm);
					usernm=0;
					break;
				}
				free(usernm);
				usernm=0;

			}
		}

		close(gid);

		printf("init: starting sh\n");
		pid = fork();
		if(pid < 0){
			printf("init: fork failed\n");
			exit();
		}
		if(pid == 0){
			setgroups(cnt,groups);
			setuid(user->uid);
			if(chdir(user->dir) < 0){
				mkdir(user->dir);
				chdir(user->dir);
				int nn=uchown(user->dir,user->uid,user->gid);
				printf("dd: %d\n", nn);
			}
			exec("/bin/sh", argv);
			printf("init: exec sh failed\n");
			exit();
		}
		while((wpid=wait()) >= 0 && wpid != pid)
			printf("zombie!\n");
	}

	//exit();
}

void printissue(void){

	int fd1,r;
	if((fd1=open("/etc/issue",O_RDONLY)) < 0){
		fd1=open("/etc/issue",O_CREATE|O_RDONLY);
		if(fd1 < 0)
			return;
	}
	char buf[64];
	do{
		memset(buf,0,64);
		r=read(fd1,buf,64);
		printf("%s",buf);
	}while(r);
	close(fd1);
	printf("\n");
}

void printmotd(void){
	int fd1,r;
	if((fd1=open("/etc/motd",O_RDONLY)) < 0){
		fd1=open("/etc/motd",O_CREATE|O_RDONLY);
		if(fd1 < 0)
			return;
	}
	char buf[64];
	printf("\n");

	do{
		memset(buf,0,64);
		r=read(fd1,buf,64);
		printf("%s",buf);
	}while(r);
	close(fd1);
	printf("\n");
}
