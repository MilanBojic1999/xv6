// Shm struktura
struct shm{
	int ref; 			// broj referenci na shm objektu
	int enc;			// broj "pokusaja" upisa u objekt
	struct shmpg *head; // glava liste stranica
	struct shmpg *end;  // kraj liste stranica
	int size;			// velicina objekta, umnozak PGSIZE
	char name[16];		// ime objekta, why not?
};

// Pomocna struktura koja drzi stranicu za shm strukturu, u shm struktura ona je clan uvezane liste
struct shmpg{
	char used;			//provera da li je shmpg koriscen
	char* addr;			//fizicka stranica, koju cuva
	struct shmpg *next;	//pokazivac na sledeci shmpg u listi
};

#define SHMNUM 10
