#include "types.h"
#include "defs.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "buf.h"
#include "param.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

//Zero i Null direktorijumi
int zread(struct inode *ip, char *dst, int n,int off){
	int target;
	iunlock(ip);
	if(ip->minor==2){
		target=n;
		while(n>0){
			*dst++=0;
			--n;
		}
		ilock(ip);
		return target-n;
	}else{
		ilock(ip);
		return 0;
	}
}


int zwrite(struct inode *ip, char *buf, int n,int off){
	return n;
}


//Kmesg direktorijum
#define ksize 256

char kbuff[ksize];
static int pnt=0;

int kmsg_read(struct inode *ip,char *dst,int n,int off){
	int target=n;
	iunlock(ip);

	//ako je ofset negativan ili veci od velcine kbuff-a	
	int curr=off%ksize;
	if(curr<0) curr+=ksize;

	while(n>0){
		if(curr==pnt){
			break;
		}
		*dst++=kbuff[curr];
		curr=(curr+1)%ksize;
		--n;
	}

	ilock(ip);
	return target-n;
}

void kmsgputc(char c){
	kbuff[pnt]=c;
	pnt=(pnt+1)%ksize;
}

int kmsg_write(struct inode *ip, char *buf, int n,int off){
	return -1;
}

//Random direktorijum


static char seed[32];


void mutate(char c){
	uint ticks0;
	acquire(&tickslock);
	ticks0 = ticks;
	release(&tickslock);

	int sign;

	for(int i=0;i<32;++i){
		sign = (ticks0%3) ? 1:-1;
		seed[i] = (seed[i] +sign*c)%128;
	}
}

static void mutateTime(){

	uint ticks0;
	acquire(&tickslock);
	ticks0 = ticks;
	release(&tickslock);

	int sign;

	for(int i=0;i<32;++i){
		sign = (ticks0%3) ? 1:-1;
		seed[i] = (seed[i] +sign*ticks0)%128;
	}
}

int rand_read(struct inode *ip, char *dst, int n,int off){

	uint ticks0;
	acquire(&tickslock);
	ticks0 = ticks;
	release(&tickslock);
	
	int i,j;
	iunlock(ip);
	for(i=ticks0%32,j=0;j<n;i=(i+1)%32,++j){
		*dst++ = seed[i];

		seed[i]=(seed[i]+(((ticks0*i)%4)*4-2))%128;
	}
	ilock(ip);
	
	mutateTime();

	return n;
}

int rand_write(struct inode *ip, char *buf, int n,int off){

	uint ticks0;
	acquire(&tickslock);
	ticks0 = ticks;
	release(&tickslock);
		
	int i,j;
	char mut=ticks0%128;
	iunlock(ip);
	for(i=ticks0%32,j=0;j<n;i=(i+1)%32){
		mut+=buf[j++]%128;
	}

	mutate(mut);
	ilock(ip);

	return n;
}

int randnum(){
	int buff=0;

	uint ticks0;
	acquire(&tickslock);
	ticks0 = ticks;
	release(&tickslock);

	int i,j;
	for(i=ticks0%32,j=0;j<4;i=(i+1)%32,++j){
		buff=buff*16+seed[i];
	}

	return buff;
}


//Disk direktorijum
static uint EOD=512*1000; //veličina diska

int disk_read(struct inode *ip, char *dst, int n,int off){
	struct buf *bp;
	int m;
	int i;
	iunlock(ip);
	if(off<0) return 0; //ako je off manji od 0
	for(i=0; i<n; i+=m, off+=m, dst+=m){
		if(off >= EOD) break;
		bp = bread(ROOTDEV,off/BSIZE); // čita blok na disku kome pripada off bajt
		m = min(n - i, BSIZE - off%BSIZE); // bira sta je manje svi traženi bajtovi ili koliko 
										   // je bajtova ostalo u bloku posle off bajta
		memmove(dst, bp->data + off%BSIZE, m); //čitamo m bajtova sa trenutnog bloka
		brelse(bp); //oslobađamo blok
	}
	ilock(ip);
	return i;
}

int disk_write(struct inode *ip, char *buf, int n,int off){
	struct buf *bp;
	int m;
	int i;
	if(off<0) return n;
	iunlock(ip);
	for(i=0;i<n;i+=m, buf+=m, off+=m){
		if(off>=EOD) break; 	//proverava da li je ofset iskocio iz memorije
		bp=bread(ROOTDEV,off/BSIZE);
		m = min(n - i, BSIZE - off%BSIZE);
		memmove(bp->data + off%BSIZE, buf, m);
		log_write(bp);
		brelse(bp);

	}
	ilock(ip);
	return n;
}

void randinit();

void iocostinit(void){
	devsw[ZERO].write=zwrite;
	devsw[ZERO].read=zread;
	
	devsw[KMESG].write=kmsg_write;
	devsw[KMESG].read=kmsg_read;

	devsw[RAND].write=rand_write;
	devsw[RAND].read=rand_read;

	devsw[DISK].write=disk_write;
	devsw[DISK].read=disk_read;

	randinit();

	cprintf("DLC devices have been initiated\n");
}

void randinit(){
	//Ovde od "malih" seed-ova pravimo niz seed koji se zapravo koristi za random mehanizam
	long seed1=0x739e858d;
	long seed2=0x2bc7973f;  
	long seed3=0x1342ebce;
	long seed4=0x6d8a031c;
	int shift=0;
	char b1,b2,b3,b4;
	for(int i=0;i<16;++i){
		b1=seed1>>shift & 0xF;
		b2=seed2>>shift & 0xF;
		b3=seed3>>shift & 0xF;
		b4=seed4>>shift & 0xF;
		
		seed[i]=(b1<<1 | b2) & 0xFF;
		seed[i+16]=(b3<<1 | b4) & 0xFF;
		shift+=4;
	}
}