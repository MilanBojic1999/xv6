#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"
#include "kernel/multiuser.h"


int
main(int argc, char *argv[])
{	
	int fd;
	if((fd=open("/etc/passwd",O_RDWR))<0){
		fd=open("/etc/passwd",O_RDWR|O_CREATE);
	}

	int startOfline=0;

	uint uid=getuid();
	int oldlen;
	char buf[128];

	char postdata[128];
	struct user* user = (struct user*)malloc(sizeof(struct user));
	printf("Changing password\n");
	if(argc==1){
		while(1){
			getline(buf,128,fd);
			//printf("UIS: %d = %d\n", uid,line2uid(buf));
			if(strlen(buf) < 1 || uid==line2uid(buf))
				break;
			startOfline+=strlen(buf);
		}
		if(strlen(buf) < 1)
			exit();
	}else{
		char* name=argv[1];
		while(1){
			getline(buf,128,fd);
			//printf("%s\n",buf);
			if(strlen(buf) < 1 || strcmp(name,line2uname(buf))==0)
				break;
			startOfline+=strlen(buf);
		}
		if(strlen(buf) < 1){
			close(fd);
			printf("User not found\n");
			exit();
		}

		if(uid > 0 && uid!=line2uid(buf)){
			printf("You aren't allow to do that\n");
			close(fd);
			exit();
		}

		
	}
	char entpass[16];
	char* oldpasswd=line2passwd(buf);
	oldlen=strlen(oldpasswd);
	int i=0;
	if(uid>0)
		for(;i<3;++i){
			printf("Enter old password: ");
			gets(entpass,16);
			if(strncmp(oldpasswd,entpass,oldlen)==0)
				break;
		}
	free(oldpasswd);

	if(i==3){
		printf("Action unsuccessful\n");
		free(user);
		close(fd);
		exit();
	}
	oldlen=strlen(buf);
	makeuser(buf,user);

	//relsallusers();

	//freecashe(user->username);

	int fdt=open("./temp",O_CREATE|O_RDWR);
	copyf2f(fd,fdt,startOfline+oldlen,0);


	int len=strlen(buf);


	lseek(fd,startOfline,SEEK_SET);
	/*getline(postdata,100,fd);
	printf("-->%s\n", postdata);*/
	
	
	char enter[16];
	char renter[16];
	rshow();
	do{	
		printf("Enter new password: ");
		gets(enter,16);
		printf("Renter new password: ");
		gets(renter,16);
		if(strcmp(enter,renter)==0){
			if(strlen(enter)<6)
				printf("Too small password %d\n",strlen(enter));
			else
				break;
		}

		printf("Passwords don't match :(\n");
	}while(1);
	rshow();
	
	len=strlen(enter);
	enter[len-1]='\0';
	
	strcpy(user->password,enter);
	char* ninfo=userToStr(user);
	
	oldlen = (oldlen < strlen(ninfo)) ? strlen(ninfo) : oldlen;

	char* nlin="\n";

	write(fd,ninfo,oldlen);
	write(fd,nlin,1);
	copyf2f(fdt,fd,0,startOfline+oldlen+1);
	//write(fd,postdata,strlen(postdata));
	unlink("../temp");
	free(ninfo);
	free(user);
	close(fd);
	exit();
}



