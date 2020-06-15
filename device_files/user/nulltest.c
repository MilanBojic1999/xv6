#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"

int
main(int argc, char *argv[])
{
	char* buff;
	buff="Ovo je test\0";
	int fd=open("/dev/null",O_RDWR);
	read(fd,buff,20);
	printf("%d) Ovo je %s nestp\n",fd, buff);
	close(fd);
	exit();
}
