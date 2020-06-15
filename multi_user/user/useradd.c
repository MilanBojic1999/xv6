#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "user.h"
#include "kernel/multiuser.h"

int
main(int argc, char *argv[])
{	

	if(argc < 2 || argc%2){
		printf("Bad call\n");
		exit();
	}

	int ufd,gfd;

	if((ufd=open("/etc/passwd",O_RDWR))<0){
		ufd=open("/etc/passwd",O_RDWR|O_CREATE);
		if(ufd<0){
			printf("You are not allowed to access this file\n");
			exit();
		}
	}



	if((gfd=open("/etc/group",O_RDWR))<0){
		gfd=open("/etc/group",O_RDWR|O_CREATE);
		if(gfd<0){
			printf("You are not allowed to access this file\n");
			exit();
		}
	}


	char used[500];
	memset(used,0,500);
	char buf[128];
	int uuid;
	do{
		getline(buf,128,ufd);
		uuid=line2uid(buf);
		if(uuid>=1000 && uuid < 1500){
			printf("Dete %d\n",uuid);
			used[uuid-1000]=1;
		}
	}while(strlen(buf)>0);

	do{
		getline(buf,128,gfd);
		uuid=line2gid2(buf);
		if(uuid>=1000 && uuid < 1500)
			used[uuid-1000]=1;
	}while(strlen(buf)>0);

	uuid=10000;

	for(int i=0;i<500;i++){
		if(!used[i]){
			uuid=i+1000;
			break;
		}
	}



	struct user* nus=(struct user*)malloc(sizeof(struct user));

	nus->uid=uuid;
	nus->gid=uuid;

	strcpy(nus->username,argv[argc-1]);
	strcpy(nus->password,"user1");
	strcpy(nus->realname,"user1");
	char dir[16];

	strcpy(dir,"/home/");
	strcpy(dir+6,nus->username);

	strcpy(nus->dir,dir);

	char* option;
	char* info;

	for(int i=1;i<argc-1;i+=2){
		option=argv[i];
		info=argv[i+1];

		if(strncmp(option,"-d",2)==0){
			if(info[0]=='/')
				strcpy(nus->dir,info);
			else{
				strcpy(nus->dir,"/home/");
				strcpy(nus->dir+6,info);
			}
		}else if(strncmp(option,"-u",2)==0){
			int num=atoi(info);
			nus->uid=num;
			nus->gid=num;
		}else if(strncmp(option,"-c",2)==0){
			strcpy(nus->realname,info);
		}else{
			free(nus);
			printf("Unsupported action.\n");
			exit();
		}
	
	}

	lseek(ufd,0,SEEK_END);

	char* userstr=userToStr(nus);

	//userstr[strlen(userstr)-1]='\0';

	write(ufd,userstr,strlen(userstr));

	char nl[1];

	nl[0]='\n';

	write(ufd,nl,1);

	struct group* ngrup=(struct group*)malloc(sizeof(struct group));

	strcpy(ngrup->groupname,nus->username);
	ngrup->gid=nus->uid;

	//char* name=(char*)malloc(sizeof(char)*16);
	strcpy(ngrup->memb[0],nus->username);


	char* gstr=groupToStr(ngrup);

	lseek(gfd,0,SEEK_END);


	write(gfd,gstr,strlen(gstr));

	write(gfd,nl,1);

	free(ngrup);
	free(nus);

	close(ufd);
	close(gfd);

	exit();
}