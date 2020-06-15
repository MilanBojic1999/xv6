#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"

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

int
main(int argc, char *argv[])
{
	if(argc<3)
		exit();

	int x,y,off_x,off_y;
	off_x=matoi(argv[1]);
	off_y=matoi(argv[2]);
	x=10;
	y=12;
	getcp(&x,&y);
	printf("x: %d, y: %d\n",x,y);
	int nx=x+off_x;
	int ny=y+off_y;
	setcp(nx,ny);
	exit();
}

