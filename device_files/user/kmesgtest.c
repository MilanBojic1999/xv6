#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"

int
main(int argc, char *argv[])
{
	char buff[25];
	int fd=open("/dev/kmesg",O_RDWR);
	read(fd,buff,25);
	printf("%d) %s\n",fd, buff);
	close(fd);
	exit();
}
