#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"

int
main(int argc, char *argv[])
{
	char *buff;
	buff="Ovo je test\0";
	int fd=open("/dev/zero",O_RDWR);
	read(fd,buff+5,5);
	printf("Ovo je  \" %s \" nad testovima\n", buff);
	close(fd);
	exit();
}
