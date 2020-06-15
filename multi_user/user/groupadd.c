#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "user.h"
#include "kernel/multiuser.h"

int
main(int argc, char *argv[])
{
	if(argc < 2){
		printf("Not enough argument\n");
		exit();
	}

	if(argc!=2 && argc!=4){
		printf("Number of arguments goes brrrrr\n");
		exit();
	}

	char* name=(char*)malloc(sizeof(char)*16);

	strcpy(name,argv[argc-1]);
	
	int fd;

	if((fd=open("/etc/group",O_RDWR))<0){
		printf("You are not allowed to access this file\n");
		exit();
	}

	int used[100];
	char tho[500];

	memset(tho,0,500);

	char buf[128];
	int cnt=0;
	do{
		getline(buf,128,fd);
		if(strlen(buf)>0){
			int num=line2gid2(buf);
			if(num>=1000 && num<1500){
				tho[num-1000]=1;
			}else{
				used[cnt++]=num;
			}
		}else{
			break;
		}
	}while(1);
	char comm[]="-g";
	char correct=1;
	int gid;
	if(strcmp(argv[1],comm)==0){
		gid=atoi(argv[2]);
		correct=1;
		if(gid>=1000 && gid<1500){
			correct=!tho[gid-1000];
		}else{
			for(int u=0;u<100;++u){
				if(gid==used[u]){
					correct=0;
					break;
				}
			}
		}
	}else if(argc!=2){
		printf("Some command are not supported\n");
		free(name);
		exit();
	}

	if(!correct){
		for(int i=0;i<500;++i){
			if(!tho[i]){
				gid=i+1000;
				break;
			}
		}
	}

	struct group* grp=(struct group*)malloc(sizeof(struct group));

	strncpy(grp->groupname,name,16);
	grp->gid=gid;

	char* line=groupToStr(grp);

	int len=strlen(line);
	
	line[len]=':';
	line[len+1]='\0';

	write(fd,line,strlen(line));

	char nl[]="\n";

	write(fd,nl,1);

	free(line);
	free(name);
	free(grp);

	close(fd);
	exit();
}
