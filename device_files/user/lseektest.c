#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"

int
main(int argc, char *argv[])
{	
	int fd=open("/home/README",O_RDONLY);
	char buf[64];
	read(fd,buf,64);
	printf("First 64 chars: %s\n",buf);
	
	lseek(fd,128,0); // direktno postavljamo na 128 bajt
	read(fd,buf,64);
	printf("Chars 128 to 172: %s\n",buf);

	lseek(fd,-64,1);	//Ovde se vraćamo unazada o pozicije odakle smo stigli
	read(fd,buf,64);
	printf("Chars later: %s\n",buf);

	lseek(fd,-64,2);	//od kraja vraćamo se za 64 bajta
	read(fd,buf,64);
	printf("Last 64 chars: %s\n",buf);


	close(fd);
	exit();
}
