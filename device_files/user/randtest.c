#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"

int
main(int argc, char *argv[])
{
	char* buff;
	int fd=open("/dev/random",O_RDWR);
	buff="OvoJeTestBra";
	char dst[5];
	read(fd,dst,5);
	printf("%d) Ovo je %d t1\n",fd,dst[0]*16+dst[1]);

	read(fd,dst,5);
	printf("Ovo je %d t2\n", dst[0]*16+dst[1]);

	write(fd,buff,10);
	read(fd,dst,5);
	printf("Ovo je %d nestp\n", dst[0]*16+dst[1]);

	close(fd);
	exit();
}
