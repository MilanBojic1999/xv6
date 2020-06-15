#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "shm.h"

#define SHMPG 100

struct {
	struct spinlock lock;
	struct shmpg shmpg[SHMPG];
}shmpgtable;


struct {
	struct spinlock lock;
	struct shm shm[SHMPG];
} shmtabel;

void shminit(void){
	initlock(&shmtabel.lock, "shmtabel");
	initlock(&shmpgtable.lock, "shmpgtabel");	
}
// Zauzimanje slobodne shm page stavke
static struct shmpg* getshmpg(void){
	struct shmpg* sh;
	acquire(&shmpgtable.lock);
	for(sh=shmpgtable.shmpg;sh < shmpgtable.shmpg + SHMPG;sh++){
		if(sh->used==0){
			sh->used=1;
			release(&shmpgtable.lock);		
			return sh;
		}
	}
	release(&shmpgtable.lock);
	return 0;
}

// Oslobodjavanje shm objekta
static int shmfree(struct shm* shm){
	shm->size = 0;
	shm->ref  = 0;
	shm->head = 0;
	shm->end  = 0;
	shm->enc  = 0;
	shm->name[0] = 0;
}

// Oslobodjavanje shm page 
static void freeshmpg(struct shmpg* sh){
	int i=0;
	acquire(&shmpgtable.lock);
	char* mem=sh->addr;
	kfree(mem);
	sh->next = 0;
	sh->used = 0;
	sh->addr = 0;
	release(&shmpgtable.lock);
}

// Menjanje velicine shm objekta
int ftruncate(struct shm* shm,int length){
	int old=shm->size;
	if(length > old){
		char* mem;
		struct shmpg *sh;
		
		for(int i=old;i<length;i+=PGSIZE){
			sh=getshmpg();
			if(sh==0)
				return -1;
			mem=kalloc();
			sh->addr=mem;
			sh->next=shm->head;
			shm->head=sh;
		}
		shm->end = sh;
		shm->size=length;

	}else{
		struct shmpg *sh=shm->head;
		struct shmpg *temp;
		for(int i=PGSIZE;i<length;i+=PGSIZE){
			sh=sh->next;
		}
		shm->end=sh;
		sh=sh->next;
		begin_op();
		while(sh){
			temp=sh;
			freeshmpg(temp);
			sh=sh->next;
		}
		end_op();
		shm->size=length;
	}
	return 0;
}

//Alociraj shm objekt
struct shm*
shmalloc(const char *name){
	struct shm *s;
	acquire(&shmtabel.lock);
	for(s=shmtabel.shm;s < shmtabel.shm + NFILE; s++){
		if(strncmp(s->name,name,16)==0){
			s->ref++;
			release(&shmtabel.lock);
			return s;
		}		
	}

	for(s=shmtabel.shm;s<shmtabel.shm + NFILE;s++){
		if(s->ref<=0){
			s->ref=1;
			memmove(s->name,name,16);
			release(&shmtabel.lock);
			return s;
		}
	}
	release(&shmtabel.lock);
	return 0;
}
// Dobijanje vec postojeceg shm objekta po imenu
struct shm*
shmname(const char* path){
	struct shm *s;
	acquire(&shmtabel.lock);
	for(s=shmtabel.shm;s < shmtabel.shm + NFILE; s++){
		if(strncmp(s->name,path,16)==0){
			//s->ref++;
			release(&shmtabel.lock);
			return s;
		}		
	}
	release(&shmtabel.lock);
	return 0;

}
// Smanjenje reference na shm objkta
int shmclose(struct shm* shm){
	acquire(&shmtabel.lock);
	if(shm->ref < 0)
		return -1;
	(shm->ref)--;
	if(shm->ref > 0){
		release(&shmtabel.lock);
		return 0;
	}

	ftruncate(shm,0); //ovo brise sve stranice u shmobjektu
	shmfree(shm);
	release(&shmtabel.lock);

	return 0;
}
// POvecanje reference na shm objkta
void shmdup(struct shm* shm){
	if(shm==0)
		panic("No shm");
	acquire(&shmtabel.lock);
	if(shm->ref < 1)
		panic("shmdup");
	shm->ref++;
	release(&shmtabel.lock);
}
