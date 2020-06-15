#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"

typedef struct dd{
	int ifd; 	//input file
	int ofd; 	//output file
	int bs;  	//block size
	int count;	//num of transfered blocks
	int skip;	//skiped blocks inside input file
	int seek;	//skiped blocks inside output file
}dd;

int matoi(const char* str);
int execute(dd* dd);

dd* init(){
	dd *temp=(dd*)malloc(sizeof(dd));
	temp->ifd=open("/dev/console",O_RDONLY);
	temp->ofd=open("/dev/console",O_WRONLY);
	temp->bs=512;

	temp->count=-1;

	temp->skip=0;
	temp->seek=0;

	return temp;
}

void parscomm(dd* ds, char* str){
	char comm[5];
	int n=strlen(str);
	int i=0;
	int j=0;

	while(str[i]!='='){
		comm[i]=str[i];
		if(i==5) return;
		++i;
	}
	char atr[n-i];
	comm[i++]=0;
	while(str[i]!=0){
		atr[j++]=str[i++];
	}
	atr[j]=0;

	if(!strcmp(comm,"if")){
		close(ds->ifd);
		int fd=open(atr,O_RDONLY);
		ds->ifd=fd;
	}else if(!strcmp(comm,"of")){
		close(ds->ofd);
		int fd=open(atr,O_WRONLY|O_CREATE);
		ds->ofd=fd;
	}else if(!strcmp(comm,"bs")){
		int bb=matoi(atr);
		ds->bs=bb;
	}else if(!strcmp(comm,"count")){
		int bb=matoi(atr);
		ds->count=bb;
	}else if(!strcmp(comm,"skip")){
		int bb=matoi(atr);
		ds->skip=bb;
	}else if(!strcmp(comm,"seek")){
		int bb=matoi(atr);
		ds->seek=bb;
	}else{
		fprintf(2, "Nepoznata komanda %s , ovo je lose\n",comm);
	}


}

int
main(int argc, char *argv[])
{
	printf("dd program\n\n");

	dd *proc=init();
	for(int i=1;i<argc;++i){
		parscomm(proc,argv[i]);
	}

	int k=execute(proc);
	fprintf(2,"\nb:%d, B:%d\n",k,k/8); //display of bits and bytes transfered

	//end of user program
	close(proc->ifd);
	close(proc->ofd);
	proc->ifd=0;
	proc->ofd=0;
	free(proc);

	exit();
}

int execute(dd *dd){
	int size=dd->bs;
	char buff[size];
	lseek(dd->ifd,dd->skip*size,0);
	lseek(dd->ofd,dd->seek*size,0);
	int n=dd->count;
	int info=0;
	if(n<0){
		int br=0;
		do{
			br=read(dd->ifd,buff,size);
			info+=write(dd->ofd,buff,br);
		}while(br==size);
	}else{
		for(int i=0;i<n;++i){
			int br=read(dd->ifd,buff,size);
			info+=write(dd->ofd,buff,br);
		}
	}

	return info;	
}


int matoi(const char* str){
	int flag=1;
	int i=0;
	if(str[0]=='-'){
		flag=-1;
		i=1;
	}
	int buff=0;

	for(;str[i]!='\0';++i){
		if(str[i]<'0' || str[i]>'9')
			return 0;
		buff=buff*10+(str[i]-'0');	

	}

	return buff*flag;
}
