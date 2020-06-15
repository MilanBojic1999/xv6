#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"
#include "kernel/x86.h"

char*
strcpy(char *s, const char *t)
{
	char *os;

	os = s;
	while((*s++ = *t++) != 0)
		;
	return os;
}

char*
strncpy(char *s, const char *t, int n)
{
	char *os;

	os = s;
	while(n-- > 0 && (*s++ = *t++) != 0)
		;
	while(n-- > 0)
		*s++ = 0;
	return os;
}

// Like strncpy but guaranteed to NUL-terminate.
char*
safestrcpy(char *s, const char *t, int n)
{
	char *os;

	os = s;
	if(n <= 0)
		return os;
	while(--n > 0 && (*s++ = *t++) != 0)
		;
	*s = 0;
	return os;
}

int
strcmp(const char *p, const char *q)
{
	while(*p && *p == *q)
		p++, q++;
	return (uchar)*p - (uchar)*q;
}

int
strncmp(const char *p, const char *q, uint n)
{
	while(n > 0 && *p && *p == *q)
		n--, p++, q++;
	if(n == 0)
		return 0;
	return (uchar)*p - (uchar)*q;
}


uint
strlen(const char *s)
{
	int n;

	for(n = 0; s[n]; n++)
		;
	return n;
}

void*
memset(void *dst, int c, uint n)
{
	stosb(dst, c, n);
	return dst;
}

char*
strchr(const char *s, char c)
{
	for(; *s; s++)
		if(*s == c)
			return (char*)s;
	return 0;
}

char*
gets(char *buf, int max)
{
	int i, cc;
	char c;

	for(i=0; i+1 < max; ){
		cc = read(0, &c, 1);
		if(cc < 1)
			break;
		buf[i++] = c;
		if(c == '\n' || c == '\r')
			break;
	}
	buf[i] = '\0';
	return buf;
}

int
stat(const char *n, struct stat *st)
{
	int fd;
	int r;

	fd = open(n, O_RDONLY);
	if(fd < 0)
		return -1;
	r = fstat(fd, st);
	close(fd);
	return r;
}

int
uchmod(const char *p, int mode){
	int fd,r;

	fd = open(p,O_RDWR);
	if(fd < 0)
		return -1;

	r=chmod(fd,mode);
	close(fd);

	return r;
}

int
uchown(const char *p, int owner, int group){
	int fd,r;

	fd = open(p,O_RDWR);
	if(fd < 0)
		return -1;

	r=chown(fd,owner,group);
	close(fd);
	return r;
}


int
atoi(const char *s)
{
	int n;

	n = 0;
	while('0' <= *s && *s <= '9')
		n = n*10 + *s++ - '0';
	return n;
}

void*
memmove(void *vdst, const void *vsrc, int n)
{
	char *dst;
	const char *src;

	dst = vdst;
	src = vsrc;
	while(n-- > 0)
		*dst++ = *src++;
	return vdst;
}

char* getline(char *buf,int max,int fd){
	int i,cc;
	char c;
	for(i=0;i<max-1;){
		cc=read(fd,&c,1);
		if(cc<1)
			break;
		buf[i++]=c;
		if(c == '\n' || c == '\r')
			break;
	}
	buf[i]='\0';
	return buf;
}

void copyf2f(int fd1,int fd2,int pos1,int pos2){
	char buf[128];
	if(pos1 >= 0)
		lseek(fd1,pos1,SEEK_SET);
	if(pos2 >= 0)
		lseek(fd2,pos2,SEEK_SET);

	do{
		getline(buf,128,fd1);
		write(fd2,buf,strlen(buf));
	}while(strlen(buf) > 0);
}