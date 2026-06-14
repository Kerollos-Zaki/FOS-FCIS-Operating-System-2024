// Sleeping locks

#include "inc/types.h"
#include "inc/x86.h"
#include "inc/memlayout.h"
#include "inc/mmu.h"
#include "inc/environment_definitions.h"
#include "inc/assert.h"
#include "inc/string.h"
#include "sleeplock.h"
#include "channel.h"
#include "../cpu/cpu.h"
#include "../proc/user_environment.h"

// not sure
#include <kern/cpu/sched.h>


void init_sleeplock(struct sleeplock *lk, char *name)
{
	init_channel(&(lk->chan), "sleep lock channel");
	init_spinlock(&(lk->lk), "lock of sleep lock");
	strcpy(lk->name, name);
	lk->locked = 0;
	lk->pid = 0;
}
int holding_sleeplock(struct sleeplock *lk)
{
	int r;
	acquire_spinlock(&(lk->lk));
	r = lk->locked && (lk->pid == get_cpu_proc()->env_id);
	release_spinlock(&(lk->lk));
	return r;
}
//==========================================================================

void acquire_sleeplock(struct sleeplock *lk)
{
	//TODO: [PROJECT'24.MS1 - #13] [4] LOCKS - acquire_sleeplock
	// what about protecting queue ? We already protected it in sleep() function
	if(holding_sleeplock(lk)){
		return;
	}
	acquire_spinlock(&(lk->lk)); // -----> guard = 1
	while (lk->locked) { // tol ma sleep lock is busy
		sleep(&(lk->chan), &(lk->lk)); // sleep for running process
		//acquire_spinlock(&(lk->lk));
	}
	lk->locked = 1;
	lk->pid = get_cpu_proc()->env_id;
	release_spinlock(&(lk->lk)); // guard

}

void release_sleeplock(struct sleeplock *lk)
{
	//TODO: [PROJECT'24.MS1 - #14] [4] LOCKS - release_sleeplock
	// what about protecting queue ? We already protected it in wakeup_all() function
	if(!holding_sleeplock(lk)){
		return;
	}
	acquire_spinlock(&(lk->lk));
	wakeup_all(&(lk->chan));
	lk->locked = 0; // free - release lock inside sleeplock
	lk->pid = 0;  // <-----
	release_spinlock(&(lk->lk)); // release spinlock (guard)
}





