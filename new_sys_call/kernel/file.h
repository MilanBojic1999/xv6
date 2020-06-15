struct file {
	enum { FD_NONE, FD_PIPE, FD_INODE, FD_SHM } type;
	int ref; // reference count
	char readable;
	char writable;
	struct pipe *pipe;
	struct inode *ip;
	struct shm *shm;
	uint off;
};
// Mapar = map + area : nastaje kao rezultat mmap funkcije
struct mapar {
	uint addr;  // adresa na kojoj je mapirana (pocetak)
	uint len;   // duzinu koja zauzima od addr
	int flags;	// permisije sa kojom je napravljena
	struct map_entity *ent[8]; // U slucaju da mamorijski prostor razbijen na vise ne povezanih delova
	uint off; 	// offset of fajla koji je mapiran u mapar prostoru
	int fd;		// Fajl deskriptor fajla koji je mapiran ( fd = 1000 ako nijedan fajl nije mapiran)
	int ref;  // Broj referenic mapar 
};

// Pomocna struktura za mapar, koja sluzi da za povezan prostor cuva podatke 
struct map_entity{
	uint addr;
	uint len;
};

// in-memory copy of an inode
struct inode {
	uint dev;           // Device number
	uint inum;          // Inode number
	int ref;            // Reference count
	struct sleeplock lock; // protects everything below here
	int valid;          // inode has been read from disk?

	short type;         // copy of disk inode
	short major;
	short minor;
	short nlink;
	uint size;
	uint addrs[NDIRECT+1];
};

// table mapping major device number to
// device functions
struct devsw {
	int (*read)(struct inode*, char*, int);
	int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
