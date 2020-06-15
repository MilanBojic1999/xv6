#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "fcntl.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "shm.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
	struct cpu *c;

	// Map "logical" addresses to virtual addresses using identity map.
	// Cannot share a CODE descriptor for both kernel and user
	// because it would have to have DPL_USR, but the CPU forbids
	// an interrupt from CPL=0 to DPL=3.
	c = &cpus[cpuid()];
	c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
	c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
	c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
	c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
	lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
	pde_t *pde;
	pte_t *pgtab;

	pde = &pgdir[PDX(va)];
	if(*pde & PTE_P){
		pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
	} else {
		if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
			return 0;
		// Make sure all those PTE_P bits are zero.
		memset(pgtab, 0, PGSIZE);
		// The permissions here are overly generous, but they can
		// be further restricted by the permissions in the page table
		// entries, if necessary.
		*pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
	}
	return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
	char *a, *last;
	pte_t *pte;

	a = (char*)PGROUNDDOWN((uint)va);
	last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
	for(;;){
		if((pte = walkpgdir(pgdir, a, 1)) == 0)
			return -1;
		if(*pte & PTE_P)
			panic("remap");
		*pte = pa | perm | PTE_P;
		if(a == last)
			break;
		a += PGSIZE;
		pa += PGSIZE;
	}
	return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
	void *virt;
	uint phys_start;
	uint phys_end;
	int perm;
} kmap[] = {
	{ (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
	{ (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
	{ (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
	{ (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
	pde_t *pgdir;
	struct kmap *k;

	if((pgdir = (pde_t*)kalloc()) == 0)
		return 0;
	memset(pgdir, 0, PGSIZE);
	if (P2V(PHYSTOP) > (void*)DEVSPACE)
		panic("PHYSTOP too high");
	for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
		if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
		            (uint)k->phys_start, k->perm) < 0) {
			freevm(pgdir);
			return 0;
		}
	return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
	kpgdir = setupkvm();
	switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
	lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
	if(p == 0)
		panic("switchuvm: no process");
	if(p->kstack == 0)
		panic("switchuvm: no kstack");
	if(p->pgdir == 0)
		panic("switchuvm: no pgdir");

	pushcli();
	mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
		sizeof(mycpu()->ts)-1, 0);
	SEG_CLS(mycpu()->gdt[SEG_TSS]);
	mycpu()->ts.ss0 = SEG_KDATA << 3;
	mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
	// setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
	// forbids I/O instructions (e.g., inb and outb) from user space
	mycpu()->ts.iomb = (ushort) 0xFFFF;
	ltr(SEG_TSS << 3);
	lcr3(V2P(p->pgdir));  // switch to process's address space
	popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
	char *mem;

	if(sz >= PGSIZE)
		panic("inituvm: more than a page");
	mem = kalloc();
	memset(mem, 0, PGSIZE);
	mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
	memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
	uint i, pa, n;
	pte_t *pte;

	if((uint) addr % PGSIZE != 0)
		panic("loaduvm: addr must be page aligned");
	for(i = 0; i < sz; i += PGSIZE){
		if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
			panic("loaduvm: address should exist");
		pa = PTE_ADDR(*pte);
		if(sz - i < PGSIZE)
			n = sz - i;
		else
			n = PGSIZE;
		if(readi(ip, P2V(pa), offset+i, n) != n)
			return -1;
	}
	return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
	char *mem;
	uint a;

	if(newsz >= KERNBASE)
		return 0;
	if(newsz < oldsz)
		return oldsz;

	a = PGROUNDUP(oldsz);
	for(; a < newsz; a += PGSIZE){
		mem = kalloc();
		if(mem == 0){
			cprintf("allocuvm out of memory\n");
			deallocuvm(pgdir, newsz, oldsz);
			return 0;
		}
		memset(mem, 0, PGSIZE);
		if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
			cprintf("allocuvm out of memory (2)\n");
			deallocuvm(pgdir, newsz, oldsz);
			kfree(mem);
			return 0;
		}
	}
	return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
	pte_t *pte;
	uint a, pa;

	if(newsz >= oldsz)
		return oldsz;

	a = PGROUNDUP(newsz);
	for(; a  < oldsz; a += PGSIZE){
		pte = walkpgdir(pgdir, (char*)a, 0);
		if(!pte)
			a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
		else if((*pte & PTE_P) != 0){
			pa = PTE_ADDR(*pte);
			if(pa == 0)
				panic("kfree");
			char *v = P2V(pa);
			kfree(v);
			*pte = 0;
		}
	}
	return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
	uint i;

	if(pgdir == 0)
		panic("freevm: no pgdir");
	deallocuvm(pgdir, KERNBASE, 0);
	for(i = 0; i < NPDENTRIES; i++){
		if(pgdir[i] & PTE_P){
			char * v = P2V(PTE_ADDR(pgdir[i]));
			kfree(v);
		}
	}
	kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
	pte_t *pte;

	pte = walkpgdir(pgdir, uva, 0);
	if(pte == 0)
		panic("clearpteu");
	*pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
	pde_t *d;
	pte_t *pte;
	uint pa, i, flags;
	char *mem;

	if((d = setupkvm()) == 0)
		return 0;
	for(i = 0; i < sz; i += PGSIZE){
		if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
			panic("copyuvm: pte should exist");
		if(!(*pte & PTE_P))
			panic("copyuvm: page not present");
		pa = PTE_ADDR(*pte);
		flags = PTE_FLAGS(*pte);
		if((mem = kalloc()) == 0)
			goto bad;
		memmove(mem, (char*)P2V(pa), PGSIZE);
		if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
			kfree(mem);
			goto bad;
		}
	}
	// Ovo ide kroz ceo memorijski prostor i proverava da li postoje mmap prostori
	// Znam da je ovo loše, ali prva verzija je išla kroz sve mapar strukture i 
	// kopirala ih u novi direktorijum ali nije radila. IDKW  
	for(i=sz;i<KERNBASE;i+=PGSIZE){
		if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
			continue;
		if(!(*pte & PTE_P))
			continue;
		pa = PTE_ADDR(*pte);
		flags = PTE_FLAGS(*pte);
		if((mem = kalloc()) == 0)
			goto bad;
		memmove(mem, (char*)P2V(pa), PGSIZE);
		if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
			kfree(mem);
			goto bad;
		}
	}

	return d;

bad:
	freevm(d);
	return 0;
}

// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
	pte_t *pte;

	pte = walkpgdir(pgdir, uva, 0);
	if((*pte & PTE_P) == 0)
		return 0;
	if((*pte & PTE_U) == 0)
		return 0;
	return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
	char *buf, *pa0;
	uint n, va0;

	buf = (char*)p;
	while(len > 0){
		va0 = (uint)PGROUNDDOWN(va);
		pa0 = uva2ka(pgdir, (char*)va0);
		if(pa0 == 0)
			return -1;
		n = PGSIZE - (va - va0);
		if(n > len)
			n = len;
		memmove(pa0 + (va - va0), buf, n);
		len -= n;
		buf += n;
		va = va0 + PGSIZE;
	}
	return 0;
}
//Ovde pronalazimo da li je dobijena addresa valjana, ako nije kernel vraca prvu slobodnu adresu
//u prostoru za MMAP objekte. U slucaiju da ne postoji vraca se 0 
static uint freemapar(pde_t *pgdir,uint addr){
	pde_t *pte;
	struct proc* p=myproc();
	if(p->sz < addr && !(addr%PGSIZE)){ //ako se tražena adresa nalazi u prostoru za MMAP
		pte=walkpgdir(pgdir,(void*)addr,0);					// i da li je na pocetku stranice
		if(pte==0)
			return addr;
	}

	for(uint i=MMAP_AREA;i<KERNBASE;i+=PGSIZE){
		pte=walkpgdir(pgdir,(void*)i,0);
		if(pte==0)
			return i;
	}

	return 0;
	
}
/*
	Funkcija koja mapira specificne povrsinu u korisnickoj memoriji
	Postoje tri razlicite vrste podataka koja se mapiraju:
		1) Nista se ne upisuje
		2) Upisuju se podaci iz fajla
		3) Mapiraju se podaci koje shm objeat poseduje

*/
int mmap(struct proc *p,void* addr,int length,int flags,int perm,struct file *file,int offset){
	char* mem;
	uint start,curr,r,use_shm;
	uint nperm=0;
	struct inode *ip=0;
	struct shm *shm=0;
	struct shmpg *shmpg;
	pde_t *pgdir=p->pgdir;

	start=freemapar(pgdir,((int)addr) ); // vraca prvu adresu koja odgovara za mapiranje

	if(start==0)
		return -1;

	if(perm & PROT_WRITE){  //posto xv6 ne podrzava PROT_READ flag ovde osiguravamo da je odklonimo 
		perm=PROT_WRITE;
	}else{
		perm=0;
	}

	if(flags!=MAP_ANONYMOUS){ //Ako treba specificne podatke da mapiramo onda proveravamo
		if(file->type==FD_SHM){ //Kakve podatke tacno treba da mapiramo SHM ili INODE
			shm=file->shm;
			if(shm==0)
				return -1;
			
			shmpg=shm->head;
		}else{
			ip=file->ip;
			ilock(ip);
		}
	}
	for(int i=0;i<length ;i+=PGSIZE){
		//Ovde se bira kakve podatke mapiramio
		if(flags==MAP_ANONYMOUS){
			mem=kalloc();
			if(!mem){
				cprintf("allocuvm out of memory\n");
				deallocuvm(pgdir,start+i,start);
				if(ip)
					iunlock(ip);
				return -1;
			}
			memset(mem,0,PGSIZE);
		}else if(file->type==FD_SHM){
			mem=shmpg->addr;
			shmpg=shmpg->next;
			if(!shmpg){ //Ovo znaci da smo iscrpeli resurs
				flags=MAP_ANONYMOUS;
			}
		}else{
			mem=kalloc();
			if(!mem){
				cprintf("allocuvm out of memory\n");
				deallocuvm(pgdir,start+i,start);
				if(ip)
					iunlock(ip);
				return -1;
			}
			memset(mem,0,PGSIZE);
			int k=readi(ip,mem,offset+i,PGSIZE);
			if(k!=PGSIZE) //Ovo znaci da smo iscrpeli resurs
				flags=MAP_ANONYMOUS;
		}
		
		//Nakon mapiranja podataka na stranicu, stanicu "ubacujemo" u tabelu stranica
		if(mappages(pgdir, (char*)(start+i), PGSIZE, V2P(mem), perm|PTE_U) < 0){
			cprintf("allocuvm out of memory (2)\n");
			deallocuvm(pgdir, start+i, start);
			kfree(mem);
			if(ip)
				iunlock(ip);
			return -1;
		}
		
	}

	if(ip)
		iunlock(ip);

	return (int*)start;
}
//Izmapiramo memorijski prostor koji je mapiran pomocu mmap-a.
//Mada dopustamo da se oslobodi samo jedna map_entity
int munmap(pde_t *pgdir,struct mapar *mpr,int addr,int length,const char free){

	uint pa;
	pde_t *pde;
	struct map_entity *ent=0;
	int k;
	for(k=0;k<8;k++){ //U maper strukturi biramo entity kojem pripada trazena adresa
		if(mpr->ent[k]==0)
			continue;
		ent=mpr->ent[k];
		if(ent->addr <= addr && ent->addr + ent->len >= addr+length){
			break;
		}
	}
	if(ent==0)
		return -1;
	for(int i=0;i<length;i+=PGSIZE){
		pde=walkpgdir(pgdir,(void*)(addr+i),0);
		if(pde && (*pde & PTE_P) != 0){
			pa = PTE_ADDR(*pde);
			if(pa == 0)
				panic("kfreek");
			char *v = P2V(pa);
			if(free) //Ovo je u slucaju da ako je mapiran shm objekt da slucajno ne 
				kfree(v); //oslobodimo shm stranicu
			*pde = 0;
		}else{
			length=i; //Ako smo stigli do neke stranice koja je ne mapirana etc mmaptests.c:105
			break;
		}
	}
	struct map_entity *nn;

	//cprintf("Brise se: %x %x sa ovim %x %x \n",ent->addr,ent->len,addr,length);

	/*
		Ovde imamo cetri slucaja za mmap_entity:
			1) Brisemo ceo entity
			2) Brisemo sa pocetka entity-a
			3) Brisemo sa kraja entity-a
			4) Brisemo u sredini entity-a tada stvaramo novi entitiy i ubacujemo ga u mapar 
	*/
	if(addr == ent->addr && length == ent->len){
		ent->addr=0;
		ent->len=0;
		mpr->ent[k]=0;
	}else if(addr == ent->addr){
		ent->len=ent->len - length;
		ent->addr=addr+length;
	}else if((addr+length) == (ent->addr+ent->len)){
		ent->len=addr-ent->addr;
	}else{
		nn=entityalloc();
		nn->addr=addr + length;
		nn->len= ent->len - nn->addr + ent->addr;
		//cprintf("Prvi se: %x %x\n",nn->addr,nn->len);
		putent(mpr,nn);
		ent->len=addr - ent->addr;
	}


	return 0;
}
//Update-uje fajl koji je zapisan u mmap prostoru
int msync(struct proc *p,uint addr,int length){
	pte_t *pte;
	uint pa;

	struct mapar *m=getmapar(addr,length);


	if(m==0)
		return -1;
	int fd=m->fd;
	struct file* f=p->ofile[fd];
	if(!f)
		return -1;
	if(f->type == FD_INODE){
		struct file *f=p->ofile[fd];
		int start_addr=m->addr;
		int off=m->off;

		pte_t *pde=p->pgdir;

		struct inode *ip=f->ip;
		begin_op();
		ilock(ip);
		for(int i=0;i<length;i+=PGSIZE){
			if((pte=walkpgdir(pde,(char*)(addr+i),0))==0)
				continue;
			pa = PTE_ADDR(*pte);

			char* v=(char*)P2V(pa);
			writei(ip,v,off+(start_addr - addr)+i,PGSIZE);
		}
		iunlock(ip);
		end_op();
	}

	return 0;
}

//Dobija mapar iz trenutnog procesa koji se nalazi na datoj adresi
struct mapar*
getmapar(int addr,int length){
	struct proc *p=myproc();
	struct mapar *temp;

	for(int i=0;i<10;++i){
		if(p->mmapar[i]){
			temp=p->mmapar[i];
			if(temp->addr <= addr && (temp->addr + temp->len) >= addr){
				return temp;
			}
		}
	}

	return 0;
}
//Potencijalno oslobadja mapar strukturu, ako ono nema ni jednu map_entity objekt u sebi
int relsmapar(pde_t *pgd,int addr,int length){
	struct mapar *m=getmapar(addr,length);
	pde_t *pd;
	char bol=1;
	for(int i=0;i<8;++i){
		if(m->ent[i]!=0){
			//cprintf("%d) %x %x \n",i,m->ent[i]->addr,m->ent[i]->len);
			return 0;
		}
	}
	mmapclose(m);

	return 1;
}
//Pomocna funkcija za shm statistiku
int shmcheck(uint addr){
	struct mapar* mpr=getmapar(addr,0);
	if(mpr==0)
		return 0;
	if(mpr->fd==1000)
		return 0;
	struct proc *p=myproc();
	struct file* f=p->ofile[mpr->fd];
	if(f==0 || f->type!=FD_SHM ||(f->writable)==0)
		return 0;
	if((mpr->flags & PROT_WRITE)==0)
		return 0;
	struct map_entity* ent;
	pde_t *pte;

	for(int i=0;i<8;++i){
		if((mpr->ent[i])==0)
			continue;
		ent=mpr->ent[i];
		for(i=0;i < (ent->len);i+=PGSIZE){
			pte = walkpgdir(p->pgdir, (char*)(ent->addr+i), 0);
			if(!pte)
				return 0;
			*pte = *pte | PTE_W;
		}
	}
	(f->shm->enc)++;
	return 1;
}
// Ovo je trebalo da pomogne oko kopiranja mmap memorije u novi proces, ali ima nekih bagova
// Mozda popravim, ali trenutno nije u upotrebi
struct mapar* copymmap(pte_t *d,pte_t *pgdir,struct mapar *mpr){

	pte_t *pte;
	uint pa, i, flags;
	char *mem;
	int addr=mpr->addr;
	int len=mpr->len;
	for(int j=0;j<8;j++){
		for(i = 0; i < len; i += PGSIZE){
			if((pte = walkpgdir(pgdir, (void *) (addr+i), 0)) == 0)
				return 0;
			if(!(*pte & PTE_P))
				return 0;
			pa = PTE_ADDR(*pte);
			flags = PTE_FLAGS(*pte);
			if((mem = kalloc()) == 0)
				return 0;
			memmove(mem, (char*)P2V(pa), PGSIZE);
			if(mappages(d, (void*)(addr+i), PGSIZE, V2P(mem), flags) < 0) {
				kfree(mem);
				return 0;
			}
		}
	}

	return mpr;
}