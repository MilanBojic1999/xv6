struct user{
	char username[16];		// username of user
	char password[16];		// password of user
	char realname[16];		// real name of user
	ushort	 uid;			// User's ID
	ushort	 gid;			// Main Group's ID where user belonge
	char dir[16];			// Users directorium
};

struct group{
	char groupname[16]; 
	ushort  gid;
	char* memb[16];
};

#define USERNUM 16
