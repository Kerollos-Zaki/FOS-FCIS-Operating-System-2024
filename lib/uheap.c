#include <inc/lib.h>


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
uint32 marked_page_size[131072]; // size : USER HEAP MAX - USER HEAP START
int32 va_page_id_share_object[131072];
//=============================================
// [1] CHANGE THE BREAK LIMIT OF THE USER HEAP:
//=============================================
/*2023*/
void* sbrk(int increment)
{
	return (void*) sys_sbrk(increment);
}

//=================================
// [2] ALLOCATE SPACE IN USER HEAP:
//=================================
void* malloc(uint32 size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'24.MS2 - #12] [3] USER HEAP [USER SIDE] - malloc()
	// Write your code here, remove the panic and write your code
	//panic("malloc() is not implemented yet...!!");
	//return NULL;
	//Use sys_isUHeapPlacementStrategyFIRSTFIT() and	sys_isUHeapPlacementStrategyBESTFIT()
	//to check the current strategy


	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE){ // Block Allocator

		cprintf("I'm in Malloc BLOCK ALLOCATOR REGION \n");

		// WHO CALLS SYS_SBRK()
		// HOW TO KNOW USER HEAP PLACEMENT STRATEGY
		/*if(sys_get_heap_strategy() == UHP_PLACE_FIRSTFIT){
			return alloc_block_FF(size);
		}
		if(sys_get_heap_strategy() == UHP_PLACE_BESTFIT){
			return alloc_block_BF(size);
		}*/
		//cprintf("I'm in Malloc Block Allocator region \n");
		if(sys_isUHeapPlacementStrategyFIRSTFIT()){ // system call
			return alloc_block_FF(size);
		}
		if(sys_isUHeapPlacementStrategyBESTFIT()){
			return alloc_block_BF(size);
		}
		//cprintf("I'm in Malloc Block Allocator region \n");
		// using system call
		//return alloc_block_FF(size);
	}else{

		cprintf("I'm in Malloc Page Allocator region \n");

		//cprintf("No of pages in user heap: %u \n",(USER_HEAP_MAX - USER_HEAP_START)/PAGE_SIZE);
		size = ROUNDUP(size, PAGE_SIZE);
		uint32 noOfPages = size / PAGE_SIZE;
		//cprintf("In Malloc: No of pages I wanna allocate: %u \n",noOfPages);
		//cprintf("User Hard Limit in Malloc of myEnv: %u \n",(uint32)myEnv->USERH_HARD_LIMIT);
		struct Env* env = (struct Env*)myEnv;
		void *start_addr = sys_getConsecutiveFreePages(env,size); // by us
		//cprintf("After sys_getConsecutiveFreePages in malloc \n");
		if(start_addr == NULL){ // no consecutive sufficient pages
			return NULL;
		}

		uint32 page_no_index = ((uint32) start_addr - ((uint32)myEnv->USERH_HARD_LIMIT + 4096))/PAGE_SIZE;

		marked_page_size[page_no_index] = noOfPages;

		//cprintf("No of consecutive marked page: %u \n",marked_page_size[page_no_index]);

		sys_allocate_user_mem((uint32) start_addr, size); // to mark - by doctor
		//cprintf("I'm in the END OF Malloc Page Allocator region \n");
		return start_addr;
	}
	return NULL;



}

//=================================
// [3] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #14] [3] USER HEAP [USER SIDE] - free()
	// Write your code here, remove the panic and write your code
	//panic("free() is not implemented yet...!!");


	uint32 va = (uint32) virtual_address;
	cprintf("I'm in free function user heap\n");
	if(va >= USER_HEAP_START && va < (uint32)myEnv->USERH_BRK){
		// Block allocator range
		free_block(virtual_address);
		return;

	}else if(va >= (uint32)myEnv->USERH_HARD_LIMIT + 4096 && va < USER_HEAP_MAX){
		uint32 page_no_index = (va - ((uint32)myEnv->USERH_HARD_LIMIT + 4096))/PAGE_SIZE;

		uint32 no_consecutive_pages = marked_page_size[page_no_index];

		sys_free_user_mem(va, no_consecutive_pages * PAGE_SIZE);

		marked_page_size[page_no_index] = 0;

		return;
	}
	panic("");
	return;
}


//=================================
// [4] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'24.MS2 - #18] [4] SHARED MEMORY [USER SIDE] - smalloc()
	// Write your code here, remove the panic and write your code
	//panic("smalloc() is not implemented yet...!!");
	//return NULL;

	//cprintf("I'm in Smalloc \n");

	size = ROUNDUP(size, PAGE_SIZE);
	struct Env* env = (struct Env*)myEnv;

	void *start_addr = sys_getConsecutiveFreePages(env,size);
	if (start_addr == NULL) { // No pages enough in virtual memory
		return NULL;
	}


	int32 shared_id = sys_createSharedObject(sharedVarName, size, isWritable, start_addr);
	if (shared_id == E_NO_MEM || shared_id == E_SHARED_MEM_EXISTS) {
		return NULL;
	}

	//cprintf("in smalloc before setting it\n");
	//cprintf("ID in smalloc: %d \n",shared_id);

	uint32 page_no_index = ((uint32) start_addr - ((uint32)myEnv->USERH_HARD_LIMIT + 4096))/PAGE_SIZE;
	va_page_id_share_object[page_no_index] = shared_id; // for sfree
	//cprintf("in smalloc after setting it\n");
	return start_addr;

}

//========================================
// [5] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//TODO: [PROJECT'24.MS2 - #20] [4] SHARED MEMORY [USER SIDE] - sget()
	// Write your code here, remove the panic and write your code
	//panic("sget() is not implemented yet...!!");
	//return NULL;

	//cprintf("IS in SGET ... \n");
	//cprintf("ownerEnvID: %u \n",ownerEnvID);
	//cprintf("sharedVarName: %s \n",sharedVarName);


	int shared_size = sys_getSizeOfSharedObject(ownerEnvID, sharedVarName);
	if (shared_size == E_SHARED_MEM_NOT_EXISTS) {
		return NULL;
	}

	struct Env* env = (struct Env*)myEnv;
	void *start_addr = sys_getConsecutiveFreePages(env,shared_size);
	if (start_addr == NULL) { // No pages enough in virtual memory
		return NULL;
	}

	int shared_object_id = sys_getSharedObject(ownerEnvID, sharedVarName,start_addr);

	if (shared_object_id == E_SHARED_MEM_NOT_EXISTS) {
		return NULL;
	}

	uint32 page_no_index = ((uint32) start_addr - ((uint32)myEnv->USERH_HARD_LIMIT + 4096))/PAGE_SIZE;
	va_page_id_share_object[page_no_index] = shared_object_id; // for sfree

	return start_addr;
}


//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_freeSharedObject(...); which switches to the kernel mode,
//	calls freeSharedObject(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the freeSharedObject() function is empty, make sure to implement it.

void sfree(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [USER SIDE] - sfree()
	// Write your code here, remove the panic and write your code
	//panic("sfree() is not implemented yet...!!");

	//cprintf("I'm in sfree \n");
	uint32 page_no_index = ((uint32) virtual_address - ((uint32)myEnv->USERH_HARD_LIMIT + 4096))/PAGE_SIZE;
	uint32 share_object_id = va_page_id_share_object[page_no_index];
	//cprintf("Share Object ID in sfree: %u \n",share_object_id);

	sys_freeSharedObject(share_object_id,virtual_address);


	va_page_id_share_object[page_no_index] = 0; // for sfree



}


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//[PROJECT]
	// Write your code here, remove the panic and write your code
	panic("realloc() is not implemented yet...!!");
	return NULL;

}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//

void expand(uint32 newSize)
{
	panic("Not Implemented");

}
void shrink(uint32 newSize)
{
	panic("Not Implemented");

}
void freeHeap(void* virtual_address)
{
	panic("Not Implemented");

}
