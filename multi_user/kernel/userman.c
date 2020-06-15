#include "types.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "multiuser.h"

struct {
	struct spinlock lock;
	struct user* users[USERNUM];
}usertable;

struct user curruser;

void setcurruser(struct user* user){

}