#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "user.h"
#include "kernel/multiuser.h"

int
main(int argc, char *argv[])
{
	if(argc<2){
		printf("Argc is too small\n");
		exit();
	}

	char* oldname=argv[argc-1];

	int ufd,gfd;

	if((ufd=open("/etc/passwd",O_RDWR)) < 0){
		if((ufd=open("/etc/passwd",O_RDWR|O_CREATE))<0){
			printf("Bad access\n");
			exit();
		}
	}

	int buf[128];

	while(1){
		getline(buf,128,fd);
		printf("UIS: %d = %d\n", uid,line2uid(buf));
		if(strlen(buf) < 1 || strcmp(oldname,line2uname)==0)
			break;
	}

	if(strlen(buf)< 1 ){
		close(ufd);
		printf("No such user in the system\n");
		exit();
	}

	struct user *user;
	makeuser(buf,user);


}


void dealgroups(char* oldname,char* newname,char* groups,char rem){
	int fd=open("/etc/group/",O_CREATE|O_RDWR);
	if(fd<0){
		printf("Bad access\n");
		return;
	}
	char gname[16];

	int j=0;
	char buf[128];
	int listgroup=0;
	char* grparr[16];
	grparr[0]=(char*)malloc(sizeof(char)*16);
	for(int i=0;groups[i];++i){
		if(groups[i]==','){
			grparr[listgroup][j]='\0';
			j=0;
			listgroup++;
			grparr[listgroup]=(char*)malloc(sizeof(char)*16);
		}else{
			grparr[listgroup][j++] = groups[i];
		}
	}
	
	grparr[listgroup][j]='\0';
	j=0;
	listgroup++;
	struct group* grp=(struct group*)malloc(sizeof(struct group));
	int k=0;
	int currpos=0;
	char postdata[4096];
	while(1){
		getline(buf,0,fd);
		if(strlen(buf)<1)
			break;
		currpos=currpos+strlen(buf);
		strcpy(gname,line2gname(buf));

		for(k=0;k<listgroup;++k){
			if(strcmp(gname,grparr[k])==0){
				makegroup(buf,grp);
				char posnull=-1;
				for(char u=0;u<16;++u){
					if(strcmp(oldname,grp->memb[u])==0){
						if(rem){
							grp->memb[u]=0;
						}
						else{
							strcpy(grparr->memb[u],newname);	
						}
						break;
					}else if(posnull<0 && grp->memb[u]==0){
						posnull=u;
					}
				}
				if(posnull>=0)
					strcpy(grp->memb[posnull],newname);

				char* line=groupToStr(grp);
			}
		}
	}

}

void dealdir(struct user,char* ndir){

}