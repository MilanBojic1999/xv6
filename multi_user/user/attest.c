#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/multiuser.h"
#include "user.h"


struct user *users[USERNUM];

struct group *groups[USERNUM];


void makeuser(char* buf,struct user* user){
	char name[16];
	char pasw[16];
	char realn[16];
	int uid=0;
	int gid=0;
	char dir[16];
	int k=0, j=0;

	for(;j<16 && buf[k]!=':';++k){
		name[j++]=buf[k];

	}
	name[j]=0;
	j=0;
	++k;

	for(;j<16 && buf[k]!=':';++k){
		pasw[j++]=buf[k];
	}
	pasw[j]=0;
	j=0;
	++k;
	for(;buf[k]!=':';++k){
		uid=10*uid+(buf[k]-'0');
	}
	++k;

	for(;buf[k]!=':';++k){
		gid=10*gid+(buf[k]-'0');
	}
	j=0;
	++k;

	for(;j<16 && buf[k]!=':';++k){
		realn[j++]=buf[k];
	}
	realn[j]=0;
	j=0;
	++k;

	for(;j<16 && buf[k]!='\n';++k){
		dir[j++]=buf[k];
	}
	dir[j]=0;

	strncpy(user->username,name,16);
	strncpy(user->password,pasw,16);
	strncpy(user->realname,realn,16);
	strncpy(user->dir,dir,16);
	user->uid=uid;
	user->gid=gid;
}

void makegroup(char* buf,struct group* group){
	char name[16];
	int gid=0;
	char user[16];
	int u=0;
	int k=0;
	int i=0;
	for(;buf[k]!=':';++k){
		name[i++]=buf[k];
	}
	i=0;
	++k;
	for(;buf[k]!=':';++k){
		gid=10*gid+(buf[k]-'0');
	}
	++k;
	strncpy(group->groupname,name,16);
	group->gid=gid;

	memset(group->memb,0,16);

	for(int uu=0;uu<16;++uu)
		group->memb[uu]=(char*)malloc(16*sizeof(char));

	while(1){
		int i=0;
		memset(user,0,16);
		for(;buf[k]!=':' && buf[k]!='\n';++k){
			user[i++]=buf[k];
		}
		user[i]='\0';

		if(user){
			strcpy(group->memb[u],user);
			++u;
		}

		if(buf[k]=='\n')
			break;
		++k;
	}

	//if(u<16)
	group->memb[u]=0;
}


struct user* getuser(char* name, char* passwd){
	struct user* us;
	/*for(int i=0; i<USERNUM ; ++i){
		if(users[i]==0)
			continue;
		us=users[i];
		if(strcmp(us->username,name)==0 && strcmp(us->password,passwd)==0){
			casheuser(us);
			return us;
		}
			
	}

	fprintf(2, "No such user in cashe: %s\n",name);*/
	int fd,r;
	if((fd=open("/etc/passwo",O_RDONLY)) < 0){
		fd=open("/etc/passwd",O_CREATE|O_RDONLY);
		if(fd<0)
			return -1;
	}

	us=(struct user*)malloc(sizeof(struct user));
	char buf[128];
	int size=strlen(name);
	buf[0]='1';
	while(1){
		printf("NO\n");


		do{
			memset(buf,0,128);
			getline(buf,128,fd);
			//printf("%s\n",buf);
		}while(strlen(buf) > 0 && strncmp(buf,name,size));

		printf("YES\n");

		if(strlen(buf)==0)
			break;
		makeuser(buf,us);
		if(strcmp(us->password,passwd)){
			//printf("%s vs. %s\n",us->password,passwd);
			continue;
		}
		casheuser(us);
		
		printf("This hahs: %s\n",us->dir);
		close(fd);
		return us;
	}
	free(us);
	close(fd);
	return 0;
}


void casheuser(struct user* user){
	static int i=0;
	struct user *old;
	if(users[i]==0){
		users[i]=user;
		i=(i+1)%USERNUM;
		return;
	}

	old=users[i];
	char used=0;
	for(int k=i+1;k!=i;k=(k+1)%USERNUM){
		if(old==users[k]){
			used=1;
			break;
		}
	}
	users[i]=user;
	i=(i+1)%USERNUM;
	if(!used)
		free(old);
}

void freecashe(const char* name){
	struct user* user=0;
	struct user* temp;
	for(int i=0;i<USERNUM;++i){
		temp=users[i];
		printf("%d\n",temp);
		if(users[i]==0)
			continue;
		printf("ss: %s vs. %s\n",users[i]->username,name);
		if(strcmp(users[i]->username,name)==0){
			user=users[i];
			users[i]=0;
		}
		
	}
	if(user)
		free(user);
}

void relsallusers(void){
	for(int i=0;i<USERNUM;++i){
		if(users[i]==0)
			continue;
		free(users[i]);
		users[i]=0;
	}
}

char* line2uname(char* line){
	char* ret=(char*)malloc(sizeof(char)*16);
	for(int i=0;line[i]!=':';i++)
		ret[i]=line[i];

	return ret;
}

char* line2passwd(char* line){
	char* ret=(char*)malloc(sizeof(char)*16);
	int i,j=0;
	for(i=0;line[i]!=':';i++);

		i++;

	for(;line[i]!=':';++i)
		ret[j++]=line[i];

	return ret;
}

int line2uid(char* line){
	int ret=0;
	int i=0,j=0;
	for(int k=0;k<2;++k){
		for(;line[i]!=':';i++);
		i++;
	}

	for(;line[i]!=':';++i)
		ret=ret*10+(line[i]-'0');

	return ret;
}

int line2gid(char* line){
	int ret=0;
	int i=0,j=0;
	for(int k=0;k<3;++k){
		for(;line[i]!=':';i++);
		i++;
	}

	for(;line[i]!=':';++i)
		ret=ret*10+(line[i]-'0');

	return ret;
}

char* line2rlname(char* line){
	char* ret=(char*)malloc(sizeof(char)*16);
	int i=0,j=0;
	for(int k=0;k<4;++k){
		for(;line[i]!=':';i++);
		i++;
	}

	for(;line[i]!=':';++i)
		ret[j++]=line[i];

	return ret;
}

char* line2dir(char* line){
	char* ret=(char*)malloc(sizeof(char)*16);
	int i=0,j=0;
	for(int k=0;k<5;++k){
		for(;line[i]!=':';i++);
		i++;
	}

	for(;line[i]!='\0' && line[i]!='\n';++i)
		ret[j++]=line[i];

	return ret;
}

char* line2gname(char* line){
	char* ret=(char*)malloc(sizeof(char)*16);
	for(int i=0;line[i]!=':';i++)
		ret[i]=line[i];

	return ret;
}

int line2gid2(char* line){
	int ret=0;
	int i=0,j=0;
	for(;line[i]!=':';i++);
	i++;

	for(;line[i]!=':';++i)
		ret=ret*10+(line[i]-'0');

	return ret;
}

char* line2nuser(char* line, int num){
	char* ret=(char*)malloc(sizeof(char)*16);
	int i=0,j=0;
	for(int k=0;k<2+num;++k){
		for(;line[i]!=':' && line[i] !='\n';i++);

		if(line[i]=='\n'){
			ret[0]=0;
			return ret;
		}
		i++;
	}

	for(;line[i]!=':' && line[i]!='\0' && line[i]!='\n';++i)
		ret[j++]=line[i];

	ret[j]='\0';

	return ret;
}

char* userToStr(struct user* user){
	char* line=(char*)malloc(sizeof(char)*128);
	int len=0;
	int i=0;
	printf("%s %s %d %d %s\n",user->username,user->password,user->uid,user->gid,user->dir);
	len=strlen(user->username);
	strncpy(line+i,user->username,len);
	i+=len;
	line[i++]=':';
	
	len=strlen(user->password);
	strncpy(line+i,user->password,len);
	i+=len;
	line[i++]=':';
	
	char nums[10];
	uitoa(user->uid,nums);
	len=strlen(nums);
	strncpy(line+i,nums,len);
	i+=len;
	line[i++]=':';

	uitoa(user->gid,nums);
	len=strlen(nums);
	strncpy(line+i,nums,len);
	i+=len;
	line[i++]=':';

	len=strlen(user->realname);
	strncpy(line+i,user->realname,len);
	i+=len;
	line[i++]=':';

	len=strlen(user->dir);
	strncpy(line+i,user->dir,len);
	i+=len;
	//line[i++]='\n';

	printf("%s\n",line);
	return line;
}

void uitoa(int num,char* buf){
	if(num==0){
		buf[0]='0';
		return;
	}
	char revs[9];
	int i=0;
	while(num>0){
		revs[i++] = (num%10 +'0');
		num/=10;
	}
	revs[i]='\0';

	int len=strlen(revs);

	for(int j=0;j<len;++j){
		buf[j]=revs[--i];
	}
	buf[len]='\0';
}

char* groupToStr(struct group *grp){
	char* line=(char*)malloc(sizeof(char)*128);
	int i=0;
	int len=0;

	len=strlen(grp->groupname);
	strncpy(line+i,grp->groupname,len);
	i+=len;
	line[i++]=':';

	char num[10];
	uitoa(grp->gid,num);
	len=strlen(num);
	strncpy(line+i,num,len);
	i+=len;
	line[i++]=':';

	for(int j=0;j<16;j++){
		if(grp->memb[j]){
			len=strlen(grp->memb[j]);
			strncpy(line+i,grp->memb[j],len);
			i+=len;
			line[i++]=':';
		}
	}

	line[i-1]='\0';

	printf("-_>%s\n",line);

	return line;

}