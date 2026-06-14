/*
 * chunk_operations.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include <kern/trap/fault_handler.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/proc/user_environment.h>
#include "kheap.h"
#include "memory_manager.h"
#include <inc/queue.h>

//extern void inctst();

/******************************/
/*[1] RAM CHUNKS MANIPULATION */
/******************************/

//===============================
// 1) CUT-PASTE PAGES IN RAM:
//===============================
//This function should cut-paste the given number of pages from source_va to dest_va on the given page_directory
//	If the page table at any destination page in the range is not exist, it should create it
//	If ANY of the destination pages exists, deny the entire process and return -1. Otherwise, cut-paste the number of pages and return 0
//	ALL 12 permission bits of the destination should be TYPICAL to those of the source
//	The given addresses may be not aligned on 4 KB
int cut_paste_pages(uint32* page_directory, uint32 source_va, uint32 dest_va, uint32 num_of_pages)
{
	//[PROJECT] [CHUNK OPERATIONS] cut_paste_pages
	// Write your code here, remove the panic and write your code
	panic("cut_paste_pages() is not implemented yet...!!");
}

//===============================
// 2) COPY-PASTE RANGE IN RAM:
//===============================
//This function should copy-paste the given size from source_va to dest_va on the given page_directory
//	Ranges DO NOT overlapped.
//	If ANY of the destination pages exists with READ ONLY permission, deny the entire process and return -1.
//	If the page table at any destination page in the range is not exist, it should create it
//	If ANY of the destination pages doesn't exist, create it with the following permissions then copy.
//	Otherwise, just copy!
//		1. WRITABLE permission
//		2. USER/SUPERVISOR permission must be SAME as the one of the source
//	The given range(s) may be not aligned on 4 KB
int copy_paste_chunk(uint32* page_directory, uint32 source_va, uint32 dest_va, uint32 size)
{
	//[PROJECT] [CHUNK OPERATIONS] copy_paste_chunk
	// Write your code here, remove the //panic and write your code
	panic("copy_paste_chunk() is not implemented yet...!!");
}

//===============================
// 3) SHARE RANGE IN RAM:
//===============================
//This function should copy-paste the given size from source_va to dest_va on the given page_directory
//	Ranges DO NOT overlapped.
//	It should set the permissions of the second range by the given perms
//	If ANY of the destination pages exists, deny the entire process and return -1. Otherwise, share the required range and return 0
//	If the page table at any destination page in the range is not exist, it should create it
//	The given range(s) may be not aligned on 4 KB
int share_chunk(uint32* page_directory, uint32 source_va,uint32 dest_va, uint32 size, uint32 perms)
{
	//[PROJECT] [CHUNK OPERATIONS] share_chunk
	// Write your code here, remove the //panic and write your code
	panic("share_chunk() is not implemented yet...!!");
}

//===============================
// 4) ALLOCATE CHUNK IN RAM:
//===============================
//This function should allocate the given virtual range [<va>, <va> + <size>) in the given address space  <page_directory> with the given permissions <perms>.
//	If ANY of the destination pages exists, deny the entire process and return -1. Otherwise, allocate the required range and return 0
//	If the page table at any destination page in the range is not exist, it should create it
//	Allocation should be aligned on page boundary. However, the given range may be not aligned.
int allocate_chunk(uint32* page_directory, uint32 va, uint32 size, uint32 perms)
{
	//[PROJECT] [CHUNK OPERATIONS] allocate_chunk
	// Write your code here, remove the //panic and write your code
	panic("allocate_chunk() is not implemented yet...!!");
}

//=====================================
// 5) CALCULATE ALLOCATED SPACE IN RAM:
//=====================================
void calculate_allocated_space(uint32* page_directory, uint32 sva, uint32 eva, uint32 *num_tables, uint32 *num_pages)
{
	//[PROJECT] [CHUNK OPERATIONS] calculate_allocated_space
	// Write your code here, remove the panic and write your code
	panic("calculate_allocated_space() is not implemented yet...!!");
}

//=====================================
// 6) CALCULATE REQUIRED FRAMES IN RAM:
//=====================================
//This function should calculate the required number of pages for allocating and mapping the given range [start va, start va + size) (either for the pages themselves or for the page tables required for mapping)
//	Pages and/or page tables that are already exist in the range SHOULD NOT be counted.
//	The given range(s) may be not aligned on 4 KB
uint32 calculate_required_frames(uint32* page_directory, uint32 sva, uint32 size)
{
	//[PROJECT] [CHUNK OPERATIONS] calculate_required_frames
	// Write your code here, remove the panic and write your code
	panic("calculate_required_frames() is not implemented yet...!!");
}

//=================================================================================//
//===========================END RAM CHUNKS MANIPULATION ==========================//
//=================================================================================//

/*******************************/
/*[2] USER CHUNKS MANIPULATION */
/*******************************/

//======================================================
/// functions used for USER HEAP (malloc, free, ...)
//======================================================

//=====================================
/* DYNAMIC ALLOCATOR SYSTEM CALLS */
//=====================================
void* sys_sbrk(int numOfPages)
{
	/* numOfPages > 0: move the segment break of the current user program to increase the size of its heap
	 * 				by the given number of pages. You should allocate NOTHING,
	 * 				and returns the address of the previous break (i.e. the beginning of newly mapped memory).
	 * numOfPages = 0: just return the current position of the segment break
	 *
	 * NOTES:
	 * 	1) As in real OS, allocate pages lazily. While sbrk moves the segment break, pages are not allocated
	 * 		until the user program actually tries to access data in its heap (i.e. will be allocated via the fault handler).
	 * 	2) Allocating additional pages for a process’ heap will fail if, for example, the free frames are exhausted
	 * 		or the break exceed the limit of the dynamic allocator. If sys_sbrk fails, the net effect should
	 * 		be that sys_sbrk returns (void*) -1 and that the segment break and the process heap are unaffected.
	 * 		You might have to undo any operations you have done so far in this case.
	 */

	//TODO: [PROJECT'24.MS2 - #11] [3] USER HEAP - sys_sbrk
	/*====================================*/
	/*Remove this line before start coding*/
	//return (void*)-1 ;
	/*====================================*/
	struct Env* env = get_cpu_proc(); //the current running Environment to adjust its break limit

	if(numOfPages <= 0){
		return env->USERH_BRK;
	}

	uint32 noBytes = numOfPages * PAGE_SIZE;
	if(((uint32)env->USERH_BRK)+noBytes-1 >= (uint32) env->USERH_HARD_LIMIT){ // exceed Hard Limit
		return (void*)-1 ;// casting -1 to address (void pointer)
	}


	// new_BRK : ((uint32)env->USERH_BRK)+noBytes-1
	for(uint32 start_page_addr = (uint32)env->USERH_BRK; start_page_addr < ((uint32)env->USERH_BRK)+noBytes;start_page_addr+=PAGE_SIZE){
		//---------------------------- NOT SURE
		uint32 *ptr_page_table;
		get_page_table(env->env_page_directory, start_page_addr,&ptr_page_table);
		if(ptr_page_table == NULL){
			create_page_table(env->env_page_directory, start_page_addr);
		}
		//---------------------------- NOT SURE
		pt_set_page_permissions(env->env_page_directory, start_page_addr, PERM_USER | 1024, 0); // mark page it's reserved but not allocated , allocated in user heap
	}

	uint32 *old_USERH_BRK = env->USERH_BRK;

	env->USERH_BRK = (uint32 *)(((uint32)env->USERH_BRK)+numOfPages*PAGE_SIZE); // /// CHECK ADDRESSES AGAIN  first byte of unmapped region not the last byte of mapped region
	// start block , end block >> hdod heap
	return (void *)old_USERH_BRK;


}
//====================================
// BY US NOT DOCTOR

void *getConsecutiveFreePages(struct Env* env,uint32 size){
	//cprintf("I'm in getConsecutiveFreePages after SYSTEM CALL (KERNEL MODE) \n");


	//env = get_cpu_proc();


	uint32 noOfPages = size / PAGE_SIZE;

	//cprintf("USERH Hard Limit: %u \n",(uint32)env->USERH_HARD_LIMIT);
	void *va_first_page = (void *) ((uint32) env->USERH_HARD_LIMIT+ PAGE_SIZE);

	uint32 isFound = 0;
	uint32 no_consecutive_pages = 0;
	void *Begin_Page_Ptr = NULL;

	uint32 no_pages = 0;
	for (uint32 page = (uint32) va_first_page;page < (uint32) USER_HEAP_MAX; page += PAGE_SIZE){
		//cprintf("Checking on page: %u \n",page);
		no_pages++;
		uint32 *ptr_page_table;
		get_page_table(env->env_page_directory,page,&ptr_page_table);
		if(ptr_page_table == NULL){
			// not marked
			//create_page_table(env->env_page_directory, page);
			if (no_consecutive_pages == 0) {
				Begin_Page_Ptr = (void *) page; // virtual address of first page free
			}
			no_consecutive_pages++;
		} else {
			uint32 permissions = pt_get_page_permissions(env->env_page_directory, page);
			//cprintf("permissions: %u \n",permissions);
			if (((permissions & 1024) == 0) && ((permissions & 2048) == 0)) { // not marked page
				if (no_consecutive_pages == 0) {
					Begin_Page_Ptr = (void *) page; // virtual address of first page free
				}
				no_consecutive_pages++;
			} else {
				no_consecutive_pages = 0;
				Begin_Page_Ptr = NULL;
			}
		}
		if (no_consecutive_pages >= noOfPages) {
			isFound = 1;
			break;
		}

	}
	if (isFound == 0) {
		return NULL;
	}
	//cprintf("No of pages: %u \n",no_consecutive_pages);
	//cprintf("I'm in the END getConsecutiveFreePages after SYSTEM CALL (KERNEL MODE) \n");
	//cprintf("Begin Address: %u \n",(uint32)Begin_Page_Ptr);

	return Begin_Page_Ptr;
}

//=====================================
// 1) ALLOCATE USER MEMORY:
//=====================================
void allocate_user_mem(struct Env* e, uint32 virtual_address, uint32 size)
{
	/*====================================*/
	/*Remove this line before start coding*/
//	inctst();
//	return;
	/*====================================*/

	//TODO: [PROJECT'24.MS2 - #13] [3] USER HEAP [KERNEL SIDE] - allocate_user_mem()
	// Write your code here, remove the panic and write your code
	//panic("allocate_user_mem() is not implemented yet...!!");

	//cprintf("I'm in ALLOCATE_USER_MEM IN USER HEAP \n");

	while (size > 0){

		uint32 *ptr_page_table;
		get_page_table(e->env_page_directory, virtual_address,&ptr_page_table);


		if(ptr_page_table == NULL){
			create_page_table(e->env_page_directory, virtual_address);
		}
		pt_set_page_permissions(e->env_page_directory, virtual_address, 1024 | PERM_USER,0);

		virtual_address = virtual_address + PAGE_SIZE;
		size -= PAGE_SIZE;
	}

}
// ===================================
// 2) FREE USER MEMORY:
//=====================================
void free_user_mem(struct Env* e, uint32 virtual_address, uint32 swize) // free >>>> user <<<< memory hoa mwdh enha l user heap mlhash 3laka b kernel heap
{
	/*====================================*/
	/*Remove this line before start coding*/
//	inctst();
//	return;
	/*====================================*/

	//TODO: [PROJECT'24.MS2 - #15] [3] USER HEAP [KERNEL SIDE] - free_user_mem
	// Write your code here, remove the panic and write your code
	//panic("free_user_mem() is not implemented yet...!!");

	// remove unmarked permission in all pages in that range (size)(consecutive pages)
	// unmap frame  (if perm_PERS = 1) meaning it has frame in memory so it's in working set , so we need to unmap and free that frame (add it in freeframeslist)
	// remove it from working set element it it was mapped to frame
	// remove the flag of reserved page (WE WILL PUT THE FLAG OF RESERVED PAGE IN Fault handler placement)
	// delete from disk

	//cprintf("I'm in free user mem \n");


	while(swize > 0){

		uint32 permissions = pt_get_page_permissions(e->env_page_directory, virtual_address);

		if((permissions & 2048) == 2048){ // this flag means this page is page faulted (reserved (allocated));
			// it's either on disk or memory(frame)
			if((permissions & PERM_PRESENT) == 0){ // it's same like get_frame_info == NULL
				// this page stored on disk maybe cuz of Replacement
				pf_remove_env_page(e, virtual_address);

			}else{
				// this page stored in memory (frame)
				// so it has working set element in working set list of this env (process)
				//struct WorkingSetElement *element = (struct WorkingSetElement *)page_to_workingset[virtual_address];
				//cprintf("Address of working set element i wanna delete: %u \n",(uint32)element);
				uint32 *ptr_page_table;
				get_page_table(e->env_page_directory,virtual_address,&ptr_page_table);

				struct FrameInfo* ptr_frame_info; // for the frame of va of the user heap page not the working set element
				ptr_frame_info = get_frame_info(e->env_page_directory,virtual_address,&ptr_page_table);

				struct WorkingSetElement *element = ptr_frame_info->addr_working_set_element;

				unmap_frame(e->env_page_directory,virtual_address); // that frame was mapped to the page user heap

				// =================================================
				// Not sure
				if (e->page_last_WS_element == element)
				{
					if(e->page_last_WS_element == LIST_LAST(&(e->page_WS_list))){
						e->page_last_WS_element = LIST_FIRST(&(e->page_WS_list));
					}else{
						e->page_last_WS_element = LIST_NEXT(element);
					}

				}

				// =================================================

				//cprintf("In free user mem, BEFORE LIST REMOVE \n");
				LIST_REMOVE(&(e->page_WS_list),element);
				//cprintf("In free user mem, AFTER LIST REMOVE \n");

				//cprintf("In free user mem, BEFORE kfree \n");
				kfree(element);// free the pages and frames that allocated for working set element in kernel heap
				//cprintf("In free user mem, after kfree \n");
				//page_to_workingset[virtual_address] = 0;
				//pt_set_page_permissions(e->env_page_directory, virtual_address, 0, PERM_PRESENT);
				ptr_frame_info->addr_working_set_element = NULL;
			}
			pt_set_page_permissions(e->env_page_directory, virtual_address, 0, 2048);
		}

		pt_set_page_permissions(e->env_page_directory, virtual_address, 0, 1024); // clear the bit that flags that this page is marked
		swize -= PAGE_SIZE;
		virtual_address += PAGE_SIZE;

	}



	//TODO: [PROJECT'24.MS2 - BONUS#3] [3] USER HEAP [KERNEL SIDE] - O(1) free_user_mem
}

//=====================================
// 2) FREE USER MEMORY (BUFFERING):
//=====================================
void __free_user_mem_with_buffering(struct Env* e, uint32 virtual_address, uint32 size)
{
	// your code is here, remove the panic and write your code
	panic("__free_user_mem_with_buffering() is not implemented yet...!!");
}

//=====================================
// 3) MOVE USER MEMORY:
//=====================================
void move_user_mem(struct Env* e, uint32 src_virtual_address, uint32 dst_virtual_address, uint32 size)
{
	//[PROJECT] [USER HEAP - KERNEL SIDE] move_user_mem
	//your code is here, remove the panic and write your code
	panic("move_user_mem() is not implemented yet...!!");
}

//=================================================================================//
//========================== END USER CHUNKS MANIPULATION =========================//
//=================================================================================//

