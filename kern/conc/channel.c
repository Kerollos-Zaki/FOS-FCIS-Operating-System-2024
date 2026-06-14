/*
 * channel.c
 *
 *  Created on: Sep 22, 2024
 *      Author: HP
 */
#include "channel.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <inc/string.h>
#include <inc/disk.h>

// not sure
#include <kern/conc/sleeplock.h>
#include <inc/environment_definitions.h>

//===============================
// 1) INITIALIZE THE CHANNEL:
//===============================
// initialize its lock & queue
void init_channel(struct Channel *chan, char *name)
{
	strcpy(chan->name, name);
	init_queue(&(chan->queue));
}

//===============================
// 2) SLEEP ON A GIVEN CHANNEL:
//===============================
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// Ref: xv6-x86 OS code
void sleep(struct Channel *chan, struct spinlock* lk)
{
	//TODO: [PROJECT'24.MS1 - #10] [4] LOCKS - sleep

	// acquire lock of queue
	// put running process into blocked queue (channel) and make it blocked
	// release the lock of queue
	// release guard
	acquire_spinlock(&(ProcessQueues.qlock));  // ---- > disable interrupt


	// Add running process into blocked processes queue to be waiting to use this critical section
	// then go to next running process and idk it will acquire sleep key to use this critical section or not
	struct Env* env=get_cpu_proc();

	enqueue(&(chan->queue),env);
	release_spinlock(lk); //---> guard = 0    ---- > Enable interrupt
	env->env_status=ENV_BLOCKED;
	// schedule next process shed()
	sched();
	release_spinlock(&(ProcessQueues.qlock)); //---- > Enable interrupt
	acquire_spinlock(lk);
}

//==================================================
// 3) WAKEUP ONE BLOCKED PROCESS ON A GIVEN CHANNEL:
//==================================================
// Wake up ONE process sleeping on chan.
// The qlock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes
void wakeup_one(struct Channel *chan)
{
	//TODO: [PROJECT'24.MS1 - #11] [4] LOCKS - wakeup_one
	acquire_spinlock(&(ProcessQueues.qlock));
	 if (queue_size(&chan->queue)==0) { // queue is empty / there is no waiting blocked processes
		release_spinlock(&(ProcessQueues.qlock));
		return;
	}
	 // already blocked processes
	// Dequeue one environment
	struct Env *env_ptr = dequeue(&(chan->queue)); // returns only one process from blocked processes queue

	//b8yr status al environment to READY
    env_ptr->env_status= ENV_READY;
    //b enqueue ll ready
	sched_insert_ready0(env_ptr);
	release_spinlock(&(ProcessQueues.qlock));
	// blocked queue (inside it blocked processes wanted to access this critical section)  --- > ready
	// ready queue --- > running
	// running process
}

//====================================================
// 4) WAKEUP ALL BLOCKED PROCESSES ON A GIVEN CHANNEL:
//====================================================
// Wake up all processes sleeping on chan.
// The queues lock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes

void wakeup_all(struct Channel *chan)
{
	//TODO: [PROJECT'24.MS1 - #12] [4] LOCKS - wakeup_all
	acquire_spinlock(&(ProcessQueues.qlock));
	while (queue_size(&chan->queue) > 0) { // mskty queue w tl3na mnha kol blocked processes ely gwaha w dfnaha fy ready queue w khlena status bta3tha "Ready"
		struct Env *env = dequeue(&(chan->queue));
		// b8yr status al environment to READY
		env->env_status = ENV_READY; // Assuming READY is a defined state
		// Add it to the ready queue for the scheduler
		sched_insert_ready0(env);
	}
	release_spinlock(&(ProcessQueues.qlock));


}

