// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void consputc(int);
static void terputc(char,int);
static void terchange(int);

static int panicked = 0;
//static int terminal=1;


char term_mem[6][25*80];
uchar activ=0;
ushort pointers[6];
uint sleepers[6];
static struct {
	struct spinlock lock;
	int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
	static char digits[] = "0123456789abcdef";
	char buf[16];
	int i;
	uint x;

	if(sign && (sign = xx < 0))
		x = -xx;
	else
		x = xx;

	i = 0;
	do{
		buf[i++] = digits[x % base];
	}while((x /= base) != 0);

	if(sign)
		buf[i++] = '-';

	while(--i >= 0){
		terputc(buf[i],activ);
		consputc(buf[i]);
	}
}

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
	int i, c, locking;
	uint *argp;
	char *s;


	locking = cons.locking;
	if(locking)
		acquire(&cons.lock);

	if (fmt == 0)
		panic("null fmtt");

	argp = (uint*)(void*)(&fmt + 1);
	for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
		if(c != '%'){
			consputc(c);
			terputc(c,activ);
			continue;
		}
		c = fmt[++i] & 0xff;
		if(c == 0)
			break;
		switch(c){
		case 'd':
			printint(*argp++, 10, 1);
			break;
		case 'x':
		case 'p':
			printint(*argp++, 16, 0);
			break;
		case 's':
			if((s = (char*)*argp++) == 0)
				s = "(null)";
			for(; *s; s++){
				consputc(*s);
				terputc(*s,activ);
			}
			break;
		case '%':
			consputc('%');
			terputc('%',activ);
			break;
		default:
			// Print unknown % sequence to draw attention.
			consputc('%');
			terputc('%',activ);
			consputc(c);
			terputc(c,activ);
			break;
		}
	}

	if(locking)
		release(&cons.lock);
}

void
panic(char *s)
{
	int i;
	uint pcs[10];

	cli();
	cons.locking = 0;
	// use lapiccpunum so that we can call panic from mycpu()
	cprintf("lapicid %d: panic: ", lapicid());
	cprintf(s);
	cprintf("\n");
	getcallerpcs(&s, pcs);
	for(i=0; i<10; i++)
		cprintf(" %p", pcs[i]);
	panicked = 1; // freeze other CPU
	for(;;)
		;
}

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static short bColor=0x0;
static short tColor=0x7;

void getANSICommand(int comm){
	if(comm==0){
			bColor=0x0;
			tColor=0x7;
			return;
	}else if(comm==39){
		tColor=0x7;
		return;
	}else if(comm==49){
		bColor=0x0;
		return;
	}
	 
	/**
	ovaj if-else blok mapira ANSI escape kod u kod koji odgovara
	xv6-ici. Nadam se da se ovo očekivalo od nas, ali nemoj za zlo
	da uzimate ako nije
	*/
	if(comm%10==1){
		comm=((comm/10)*10)+4;
	}else if(comm%10==4){
		comm=((comm/10)*10)+1;
	}else if(comm%10==3){
		comm=((comm/10)*10)+6;
	}else if(comm%10==6){
		comm=((comm/10)*10)+3;
	}

	if(comm/10==3){
		tColor=comm%10;
	}else if(comm/10==4){
		bColor=comm%10;
	}else{
		bColor=0x6;
	}
}


static void
cgaputc(int c)
{
	int pos;

	// Cursor position: col + 80*row.
	outb(CRTPORT, 14);
	pos = inb(CRTPORT+1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT+1);

	if(c == '\n')
		pos += 80 - pos%80;
	else if(c == BACKSPACE){
		if(pos > 0) --pos;
	} else
		crt[pos++] = (c&0xff) | (tColor | bColor<<4)<<8;  // black on white

	if(pos < 0 || pos > 25*80)
		panic("pos under/overflow");

	if((pos/80) >= 24){  // Scroll up.
		memmove(crt, crt+80, sizeof(crt[0])*23*80);
		pos -= 80;
		memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
	}

	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	crt[pos] = ' ' | 0x0700;

}

void
consputc(int c)
{

	if(panicked){
		cli();
		for(;;)
			;
	}

	static char status=0;
	static char buffer=0;

	if(c == BACKSPACE){
		uartputc('\b'); uartputc(' '); uartputc('\b');
	}else
		uartputc(c);

	if(status==0){
		if(c==033){
			status=1;
		}else{
			cgaputc(c);
		}
	}else if(status==1){
		if(c=='[')
			status=2;
		else
			status=0;
	}else if(status==2){
		if(c=='m' || c==';'){
				getANSICommand(buffer);
				buffer=0;
				if(c=='m')
					status=0;
			}else{
				buffer=buffer*10+((int)(c-'0'));
			}
	} 
}


#define INPUT_BUF 128
struct in{
	char buf[INPUT_BUF];
	uint r;  // Read index
	uint w;  // Write index
	uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

int show=1;


void restartConsole(){
	memset(crt,0,sizeof(crt[0])*24*80);
	int pos=0;

	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	crt[pos] = ' ' | 0x0700;
}

void
consoleintr(int (*getc)(void))
{
	int c, doprocdump = 0;

	acquire(&cons.lock);
	while((c = getc()) >= 0){
		switch(c){
		case C('P'):  // Process listing.
			// procdump() locks cons.lock indirectly; invoke later
			doprocdump = 1;
			break;
		case C('U'):  // Kill line.
			while(input.e != input.w &&
			      input.buf[(input.e-1) % INPUT_BUF] != '\n'){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case C('H'): case '\x7f':  // Backspace
			if(input.e != input.w){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case C('E'):
			show=!show;
			break;
		case 131: case 132: case 133: case 134: case 135: case 136:
			release(&cons.lock);
			restartConsole();
			terchange(c%10-1);
			//cprintf("\033[42;34mTerminal br. %d\033[0m\n",c%10);
			acquire(&cons.lock);
			break;
		default:
			if(c != 0 && input.e-input.r < INPUT_BUF){
				c = (c == '\r') ? '\n' : c;
				input.buf[input.e++ % INPUT_BUF] = c;
				if(show)
					consputc(c);
				else if(c=='\n'){
						consputc(c);
				}
				if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
					input.w = input.e;
					wakeup(&sleepers[activ]);
				}
			}
			break;
		}
	}
	release(&cons.lock);
	if(doprocdump) {
		procdump();  // now call procdump() wo. cons.lock held
	}
}

int
consoleread(struct inode *ip, char *dst, int n)
{
	uint target;
	int c;

	iunlock(ip);
	target = n;
	acquire(&cons.lock);
	while(n > 0){
		while(input.r == input.w){
			if(myproc()->killed){
				release(&cons.lock);
				ilock(ip);
				return -1;
			}
			sleep(&sleepers[ip->minor-1], &cons.lock);
		}
		c = input.buf[input.r++ % INPUT_BUF];
		if(c == C('D')){  // EOF
			if(n < target){
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
			}
			break;
		}
		*dst++ = c;
		terputc(c,ip->minor-1);
		--n;
		if(c == '\n')
			break;
	}
	release(&cons.lock);
	ilock(ip);


	return target - n;

}

int
consolewrite(struct inode *ip, char *buf, int n)
{
	int i;

	iunlock(ip);
	acquire(&cons.lock);
	for(i = 0; i < n; i++){
		terputc(buf[i],ip->minor-1);
		if(ip->minor==activ+1)
			consputc(buf[i] & 0xff);
	}
	release(&cons.lock);
	ilock(ip);

	return n;
}

void terputc(char c,int term){
	int pnt=pointers[term];
	term_mem[term][pnt++]=c;
	if(pnt>=25*80){
			memmove(term_mem[term],term_mem[term]+80,sizeof(char)*24*80);
			pnt-=80;
			memset(term_mem[term]+pnt,0,sizeof(char)*(25*80-pnt));
		}
	pointers[term]=pnt;
}

void terchange(int nw){
	activ=nw;
	acquire(&cons.lock);
	int i;
	for(i=0;i<pointers[nw];++i){
		consputc(term_mem[nw][i]);
	}
	release(&cons.lock);

	input.w=input.r=input.e=0;
}

int getact(void){
	return activ;
}

void
consoleinit(void)
{
	initlock(&cons.lock, "console");

	devsw[CONSOLE].write = consolewrite;
	devsw[CONSOLE].read = consoleread;
	cons.locking = 1;
	int i;
	for(i=0;i<6;++i){
		memset(term_mem[i],0,sizeof(char)*25*80);
		pointers[i]=0;
	}
	ioapicenable(IRQ_KBD, 0);
}
