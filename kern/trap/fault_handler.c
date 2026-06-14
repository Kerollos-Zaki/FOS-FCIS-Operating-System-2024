/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/mem/memory_manager.h>



//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE)
{
	assert(LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE ;
}
void setPageReplacmentAlgorithmCLOCK(){_PageRepAlgoType = PG_REP_CLOCK;}
void setPageReplacmentAlgorithmFIFO(){_PageRepAlgoType = PG_REP_FIFO;}
void setPageReplacmentAlgorithmModifiedCLOCK(){_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;}
/*2018*/ void setPageReplacmentAlgorithmDynamicLocal(){_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;}
/*2021*/ void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps){_PageRepAlgoType = PG_REP_NchanceCLOCK;  page_WS_max_sweeps = PageWSMaxSweeps;}

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE){return _PageRepAlgoType == LRU_TYPE ? 1 : 0;}
uint32 isPageReplacmentAlgorithmCLOCK(){if(_PageRepAlgoType == PG_REP_CLOCK) return 1; return 0;}
uint32 isPageReplacmentAlgorithmFIFO(){if(_PageRepAlgoType == PG_REP_FIFO) return 1; return 0;}
uint32 isPageReplacmentAlgorithmModifiedCLOCK(){if(_PageRepAlgoType == PG_REP_MODIFIEDCLOCK) return 1; return 0;}
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal(){if(_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK(){if(_PageRepAlgoType == PG_REP_NchanceCLOCK) return 1; return 0;}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt){_EnableModifiedBuffer = enableIt;}
uint8 isModifiedBufferEnabled(){  return _EnableModifiedBuffer ; }

void enableBuffering(uint32 enableIt){_EnableBuffering = enableIt;}
uint8 isBufferingEnabled(){  return _EnableBuffering ; }

void setModifiedBufferLength(uint32 length) { _ModifiedBufferLength = length;}
uint32 getModifiedBufferLength() { return _ModifiedBufferLength;}

//===============================
// FAULT HANDLERS
//===============================

//==================
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault  = 0;

struct Env* last_faulted_env = NULL;
void fault_handler(struct Trapframe *tf)
{
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//	cprintf("\n************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	//If same fault va for 3 times, then panic
	//UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env* cur_env = get_cpu_proc();
	if (last_fault_va == fault_va && last_faulted_env == cur_env)
	{
		num_repeated_fault++ ;
		if (num_repeated_fault == 3)
		{
			print_trapframe(tf);
			panic("Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n", before_last_fault_va, before_last_eip, fault_va);
		}
	}
	else
	{
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32)tf->tf_eip;
	last_fault_va = fault_va ;
	last_faulted_env = cur_env;
	/******************************************************/
	//2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3) {
		userTrap = 1;
	}
	if (!userTrap)
	{
		struct cpu* c = mycpu();
		//cprintf("trap from KERNEL\n");
		if (cur_env && fault_va >= (uint32)cur_env->kstack && fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va >= (uint32)c->stack && fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!", c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	//2017: Check stack underflow for User
	else
	{
		//cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	//get a pointer to the environment that caused the fault at runtime
	//cprintf("curenv = %x\n", curenv);
	struct Env* faulted_env = cur_env;
	if (faulted_env == NULL)
	{
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ( (faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT) != PERM_PRESENT)
	{
		// we have a table fault =============================================================
		//		cprintf("[%s] user TABLE fault va %08x\n", curenv->prog_name, fault_va);
		//		print_trapframe(tf);

		faulted_env->tableFaultsCounter ++ ;

		table_fault_handler(faulted_env, fault_va);
	}
	else
	{
		if (userTrap)
		{
			/*============================================================================================*/
			//TODO: [PROJECT'24.MS2 - #08] [2] FAULT HANDLER I - Check for invalid pointers
			//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
			//your code is here
			//cprintf("USER TRAP \n");
			//cprintf("I'm In User Trap\n");

			// Two tests
			// PART I: Test the Pointer Validation inside fault_handler(): [70%] >> this check this function
			// PART II: PLACEMENT: Test the Invalid Access to a NON-EXIST page in Page File, Stack & Heap: [30%] >> this will call page fault handler function so make sure it's handled "Placement"
			// case 1 : pointing to kernel address (AFTER USER LIMIT )

			//env_page_ws_print(faulted_env);
			int permission = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
			/*if((permission & PERM_USER) == 0){ // if permission & PERM_USER == PERM_USER SO IT's USER , It's like permission & PERM_USER != 0
				cprintf("Permissions: %u \n",permission);
				cprintf("Kernel HEAP START: %u \n",KERNEL_HEAP_START);
				cprintf("Kernel HEAP END : %u \n",KERNEL_HEAP_MAX);

				cprintf("USER HEAP START: %u \n",USER_HEAP_START);
				cprintf("USER HEAP END : %u \n",USER_HEAP_MAX);

				cprintf("USER STACK BOTTOM: %u \n",USTACKBOTTOM);
				cprintf("USER STACK TOP: %u \n",USTACKTOP);


				cprintf("USER_LIMIT ADDR(this addr and what after is KERNEL): %u \n",USER_LIMIT);
				cprintf("Address : %u \n",fault_va);
				cprintf("CASE 1: POITNING TO KERENL ADDRESS\n");
				env_exit();
			}*/
			//cprintf("I'm in invalid pointer check fault hander, fault va: %u \n",fault_va);
			//cprintf("Permissions: %u \n",permission);
			//cprintf("Kernel HEAP START: %u \n",KERNEL_HEAP_START);
			//cprintf("Kernel HEAP END : %u \n",KERNEL_HEAP_MAX);

			//cprintf("USER HEAP START: %u \n",USER_HEAP_START);
			//cprintf("USER HEAP END : %u \n",USER_HEAP_MAX);

			//cprintf("USER STACK BOTTOM: %u \n",USTACKBOTTOM);
			//cprintf("USER STACK TOP: %u \n",USTACKTOP);


			//cprintf("USER_LIMIT ADDR(this addr and what after is KERNEL): %u \n",USER_LIMIT);
			//cprintf("Faulted va : %u \n",fault_va);

		    if (fault_va >= USER_LIMIT) { // <---------------------- NOT SUREEEEEE
		    	//cprintf("Faulted va : %u \n",fault_va);
		    	//cprintf("CASE 1: POITNING TO KERENL ADDRESS\n");
				env_exit();
			}



		    // case2 : pointing to unmarked page(PERM_USER not set) in USER HEAP
		    // && ((permission & PERM_PRESENT) == 0)
			if(fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX){
				if (((permission & 1024) == 0) && ((permission & 2048) == 0)) { // if it's unmarked --- NOT SURE OF ((permission & PERM_PRESENT) == 0) >> 3dltha 3shan fault handler
					/*cprintf("unmarked page in user heap \n");
					cprintf("Faulted va : %u \n",fault_va);
					cprintf("Permissions of faulted va: %d \n",permission);
					cprintf("Perm PRESENT(AVAILABLE)(EXIST): %u \n",(permission & PERM_PRESENT));
					cprintf("CASE 2: unmarked page in user heap\n");*/
					env_exit();
				}
			}
		    /*if(fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX){
				if ((permission & PERM_USER) == 0) { // if it's unmarked
					//cprintf("unmarked page in user heap \n");
					cprintf("CASE 2: unmarked page in user heap\n");
					env_exit();
				}
		    }*/


			/*int found_in_WS = 0; ---> logic dh ghalt bs kan by3ml bypass l test cases doctor w kan by3dy m3rfsh ezay
			struct WorkingSetElement *element;
			LIST_FOREACH(element, &(faulted_env->page_WS_list))
			{
				if (fault_va == element->virtual_address) {
					found_in_WS++;
					break;
				}
			}
			if (found_in_WS == 0) {
				cprintf("pointing to unmarked page \n");
				env_exit();
			}*/



			//permission validation
			// the page is present in frame (in RAM) but it's read only
		    // case 3 Exist but with read-only permissions
			//cprintf("Address: %u \n",fault_va);
			//cprintf("Permissions: %u \n",permission);
			//cprintf("Permission READ: %d \n",(permission & PERM_WRITEABLE));
			//cprintf("Permission PRESENT: %d \n",(permission & PERM_PRESENT));
			if (((permission & PERM_PRESENT) != 0) && ((permission & PERM_WRITEABLE) == 0)) { // if it's readable
				//cprintf("CASE 3: PERMISSION VALIDATION READ ONLY\n");
				env_exit();
			}

			//cprintf("End OF Invalid pointer fault handler function \n");
		}

			/*============================================================================================*/


		/*2022: Check if fault due to Access Rights */
		int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
		if (perms & PERM_PRESENT)
			panic("Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n", fault_va) ;
		/*============================================================================================*/


		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter ++ ;

		//		cprintf("[%08s] user PAGE fault va %08x\n", curenv->prog_name, fault_va);
		//		cprintf("\nPage working set BEFORE fault handler...\n");
		//		env_page_ws_print(curenv);

		if(isBufferingEnabled())
		{
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		}
		else
		{
			//page_fault_handler(faulted_env, fault_va);
			page_fault_handler(faulted_env, fault_va);
		}
		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(curenv);


	}

	/*************************************************************/
	//Refresh the TLB cache
	tlbflush();
	/*************************************************************/
}

//=========================
// [2] TABLE FAULT HANDLER:
//=========================
void table_fault_handler(struct Env * curenv, uint32 fault_va)
{
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory, (uint32)fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}

//=========================
// [3] PAGE FAULT HANDLER:
//=========================
void page_fault_handler(struct Env * faulted_env, uint32 fault_va)
{
	//cprintf("BEGINNING OF page_fault_handler \n");

	//cprintf("I'm in invalid pointer check fault hander, fault va: %u \n",fault_va);
	//cprintf("Size of List: %u \n",faulted_env->page_WS_list.size);
	//cprintf("Max size of list: %u \n",faulted_env->page_WS_max_size);
	//env_page_ws_print(faulted_env);

	//cprintf("Faulted va Before Condition: %u \n",fault_va);

#if USE_KHEAP
		struct WorkingSetElement *victimWSElement = NULL;
		uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
#else
		int iWS =faulted_env->page_last_WS_index;
		uint32 wsSize = env_page_ws_get_size(faulted_env);
#endif

	//cprintf("Size of list: %u \n",wsSize);
	//cprintf("Max size of list: %u \n",(faulted_env->page_WS_max_size));
	if(wsSize < (faulted_env->page_WS_max_size))
	{
		//cprintf("PLACEMENT=========================WS Size = %d\n", wsSize );
		//TODO: [PROJECT'24.MS2 - #09] [2] FAULT HANDLER I - Placement
		// Write your code here, remove the panic and write your code
		//panic("page_fault_handler().PLACEMENT is not implemented yet...!!");
		//refer to the project presentation and documentation for details

		// I think we will call the function of create working set and add it in the list , and allocate the frame for the page

		cprintf("BEGINNING OF page_fault_handler when size not full\n");

		struct FrameInfo* frame_info;
		int allocation_result = allocate_frame(&frame_info);
		if (allocation_result != 0) {
			env_exit();
			return;
		}
		//cprintf("ALLOCATED DONE IN page_fault_handler \n");


		//cprintf("MAPPED DONE IN page_fault_handler \n");

		//cprintf("BEFORE READING page_fault_handler \n");
		// WE MUST MAP FRAME FIRST (MAP VA PAGE TO FRAME IN MEMORY) BEFORE READING FROM DISK, CUZ READING FROM DISK YOU WILL STORE (WRITE) DATA IN VA SO IT SHOULD BE MAPPED TO FRAME
		int mapping_result = map_frame(faulted_env->env_page_directory,frame_info, fault_va, PERM_PRESENT | PERM_WRITEABLE | PERM_USER); // I dk about the permissions
		if (mapping_result != 0) {
			//free_frame(frame_info);
			free_frame(frame_info);
			env_exit();
			return;
		}

		// Read it from disk
		int ret = pf_read_env_page(faulted_env, (void *)fault_va);
		if(ret ==  E_PAGE_NOT_EXIST_IN_PF){
			//cprintf("I'm IN E_PAGE_NOT_EXIST_IN_PF\n");
			if (!((fault_va < (uint32) USER_HEAP_MAX && fault_va >= (uint32) USER_HEAP_START) || (fault_va < (uint32) USTACKTOP && fault_va >= (uint32) USTACKBOTTOM))) {
				//cprintf("USER HEAP START: %u \n",USER_HEAP_START);
				//cprintf("USER HEAP MAX: %u \n",USER_HEAP_MAX);
				//cprintf("KERNEL HEAP START: %u \n",KERNEL_HEAP_START);
				//cprintf("KERNEL HEAP MAX: %u \n",KERNEL_HEAP_MAX);
				//cprintf("address of user Limit: %u \n",USER_LIMIT);
				//cprintf("Fault va: %u \n",fault_va);
				//cprintf("I'm IN E_PAGE_NOT_EXIST_IN_PF AND I WILL EXIT\n");
				//cprintf("HEREEEEEEEEEEEEEE");
				free_frame(frame_info);
				env_exit();
				//return;
			}
			//cprintf("I'm STACK OR HEAP PAGE\n");
		}
		// h3rf mnen en dy kernel page w kant 3la desk wla user page?
		// khaly balk en eltakhzen kolh dynamic alloaction fy kernel heap 3shan kda bn3ml system call , user > less privilage
		// wkman khaly balk fy invalid pointer ehna 3mlna check 3la enh lw kernel address ytl3 bra fa dh kda akeed 100% mn user , bs hndelh PERM_WRITEABLE wla elmafrod la?


		//cprintf("AFTER READING page_fault_handler \n");
		//cprintf("Size of working Set element: %u \n",faulted_env->page_WS_list.size);
		//cprintf("Max Size of working Set element: %u \n",faulted_env->page_WS_max_size);

		// Create Working set element and adding it to the list of WORKING SET ELEMENT LIST linked to that process
		//env_page_ws_print(faulted_env);
		struct WorkingSetElement *element = env_page_ws_list_create_element(faulted_env,fault_va);// it returs the address of the allocated page
		//env_page_ws_print(faulted_env);
		// Add it in working list
		if(element == NULL){
			free_frame(frame_info);
			env_exit();
			return;
		}
		LIST_INSERT_TAIL(&(faulted_env->page_WS_list),element);
		//==============================================================
		//VIMP FOR Marking the page is faulted and save the address of working set element cuz when freeing to be in O(1)


		pt_set_page_permissions(faulted_env->env_page_directory, fault_va, 2048, 0); // <<<<<<

		//cprintf("Before adding address of working list mapped to that va in array in page fault handler function\n");
		//cprintf("fault va: %u \n",fault_va);
		//cprintf("address of working set element: %u \n",(uint32)element);
		//cprintf("access array: %u \n",page_to_workingset[fault_va]);
		//page_to_workingset[fault_va] = (uint32)element;

		frame_info->addr_working_set_element = element;  // <<<<<<<<<<<

		//cprintf("After adding address of working list mapped to that va in array in page fault handler function\n");
		//===============================================================
		if(faulted_env->page_WS_list.size == faulted_env->page_WS_max_size){
			faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
		}




		//cprintf("SUCCEDDED IN LAST WORKING SET ELEMENT 20\n");
		//cprintf("Size of working Set element: %u \n",faulted_env->page_WS_list.size);
		// lw stack page aw heap page hoa talbnha fy msh htkon 3la disk bs hoa 3ayz ystkhdmha fa ht7otha brdk fy working set list


		//uint32 *ptr_page_table;
		//int table_exists = get_page_table(faulted_env->env_page_directory,fault_va, &ptr_page_table);
		// va exists 3ady fy page table hta lw hoa fy disk, bs PERM_PRESENT = 0


		//faulted_env->page_last_WS_element = NULL;
		//faulted_env->page_last_WS_element =(struct WorkingSetElement *)fault_va;
		//cprintf("AFTER INSERTING page_fault_handler \n");
		// update
		//LIST_ENTRY(WorkingSetElement)prev_next_info;
		tlb_invalidate(faulted_env->env_page_directory, (void *) fault_va);


		// Don't forget acquire and release a lock if you need to



	}
	else
	{
		//cprintf("I'm In fault handler Replacement \n");
		//env_page_ws_print(faulted_env);

		//cprintf("REPLACEMENT=========================WS Size = %d\n", wsSize );
		//cprintf("Max size of list should be: %u \n",faulted_env->page_WS_max_size);
		//refer to the project presentation and documentation for details
		//TODO: [PROJECT'24.MS3] [2] FAULT HANDLER II - Replacement
		// Write your code here, remove the panic and write your code
		//panic("page_fault_handler() Replacement is not implemented yet...!!");


		//cprintf("Max Sweep counter (N) %d \n",page_WS_max_sweeps);
		//cprintf("Faulted va: %u \n",fault_va);
		//cprintf("USER HEAP START: %u \n",(uint32)USER_HEAP_START);
		//cprintf("USER HEAP MAX: %u \n",(uint32)USER_HEAP_MAX);

		cprintf("Faulted va: %x \n",fault_va);
		if(page_WS_max_sweeps >= 0){ // +ve >> replace modified page if it reaches "n"
			cprintf("N is positive \n");
			uint8 ptr_flag_isAdded= 0;
			while(1 == 1){

				int32 permissions = pt_get_page_permissions(faulted_env->env_page_directory,faulted_env->page_last_WS_element->virtual_address);
				if((permissions & PERM_USED) == PERM_USED){ // bit = 1
					pt_set_page_permissions(faulted_env->env_page_directory,faulted_env->page_last_WS_element->virtual_address,0,PERM_USED);
					faulted_env->page_last_WS_element->sweeps_counter = 0;
					cprintf("PERM_USED = 1 \n");
					cprintf("last_ws_elment: %u \n",(uint32)faulted_env->page_last_WS_element);
					cprintf("Sweep counter of working set element before: %u \n",faulted_env->page_last_WS_element->sweeps_counter);


				}else{ // used bit = zero
					cprintf("PERM_USED = 0 \n");
					cprintf("last_ws_elment: %u \n",(uint32)faulted_env->page_last_WS_element);
					cprintf("Sweep counter of working set element before: %u \n",faulted_env->page_last_WS_element->sweeps_counter);
					faulted_env->page_last_WS_element->sweeps_counter = faulted_env->page_last_WS_element->sweeps_counter + 1;
					cprintf("Sweep counter of working set element after incrementing: %u \n",faulted_env->page_last_WS_element->sweeps_counter);
					if(faulted_env->page_last_WS_element->sweeps_counter >= page_WS_max_sweeps){ // replace victim
						cprintf("I will replace ............. \n");
						uint32 *ptr_page_table;
						get_page_table(faulted_env->env_page_directory,faulted_env->page_last_WS_element->virtual_address,&ptr_page_table);
						struct FrameInfo* ptr_frame_info = get_frame_info(faulted_env->env_page_directory,faulted_env->page_last_WS_element->virtual_address,&ptr_page_table);

						if((permissions & PERM_MODIFIED) == PERM_MODIFIED){ // Modified Page >> save on disk
							cprintf("I'm saving on disk .... \n");
							int ret = pf_update_env_page(faulted_env, faulted_env->page_last_WS_element->virtual_address, ptr_frame_info);
						}
						// if not modified or modified (it doesn't matter)
						// Replacing the victim , delete it
						cprintf("HEREEEEEEEEEE1 \n");
						ptr_frame_info->addr_working_set_element = NULL;
						//free(((void *)faulted_env->page_last_WS_element->virtual_address)); // Clear used bit , ...
						pt_set_page_permissions(faulted_env->env_page_directory, faulted_env->page_last_WS_element->virtual_address, 0, 2048);
						unmap_frame(faulted_env->env_page_directory,faulted_env->page_last_WS_element->virtual_address);


						cprintf("HEREEEEEEEEEE2 \n");
						// Add faulted page in memory (frame)
						int allocation_result = allocate_frame(&ptr_frame_info);
						if (allocation_result != 0) {
							cprintf("Before env exit ... \n");
							env_exit();
							return;
						}
						cprintf("HEREEEEEEEEEE3 \n");

						int mapping_result = map_frame(faulted_env->env_page_directory,ptr_frame_info, fault_va, PERM_PRESENT | PERM_WRITEABLE | PERM_USER); // I dk about the permissions
						if (mapping_result != 0) {
							//free_frame(frame_info);
							free_frame(ptr_frame_info);
							cprintf("Before env exit ... \n");
							env_exit();
							return;
						}


						// Read it from disk
						int ret = pf_read_env_page(faulted_env, (void *)fault_va);
						cprintf("HEREEEEEEEEEE3.5 \n");
						if(ret ==  E_PAGE_NOT_EXIST_IN_PF){
							if (!((fault_va < (uint32) USER_HEAP_MAX && fault_va >= (uint32) USER_HEAP_START) || (fault_va < (uint32) USTACKTOP && fault_va >= (uint32) USTACKBOTTOM))) {
								free_frame(ptr_frame_info);
								cprintf("Before env exit ... \n");
								env_exit();

							}

						}
						cprintf("HEREEEEEEEEEE4 \n");

						cprintf("HEREEEEEEEEEE5 \n");
						faulted_env->page_last_WS_element->virtual_address = fault_va;
						faulted_env->page_last_WS_element->sweeps_counter = 0;
						ptr_frame_info->addr_working_set_element = faulted_env->page_last_WS_element;

						//VIMP FOR Marking the page is faulted and save the address of working set element cuz when freeing to be in O(1)


						pt_set_page_permissions(faulted_env->env_page_directory, fault_va, 2048, 0); // <<<<<<
						ptr_flag_isAdded = 1;
						cprintf("After setting ptr_flag_isAdded = 1; \n");
					}
					// if less than N do thing we already incremented sweep counter
				}



				if(faulted_env->page_last_WS_element == LIST_LAST(&(faulted_env->page_WS_list))){
					cprintf("I'm last element \n");
					cprintf("Addr of last working set element in list: %u \n",(uint32)faulted_env->page_last_WS_element);
					faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
				}else{
					cprintf("Addr of current working set element in list: %u \n",(uint32)faulted_env->page_last_WS_element);
					faulted_env->page_last_WS_element = LIST_NEXT(faulted_env->page_last_WS_element);
					cprintf("Addr of next working set element in list: %u \n",(uint32)faulted_env->page_last_WS_element);
				}
				if(ptr_flag_isAdded == 1){
					cprintf("Before break ... \n");
					env_page_ws_print(faulted_env);
					tlb_invalidate(faulted_env->env_page_directory, (void *) fault_va);
					break;
				}

			}

		}else{ // -ve, Replace modified bit if it reaches n+1 not n
			cprintf("N is negative \n");
			uint8 ptr_flag_isAdded= 0;
			cprintf("In Negative--- page_WS_max_sweeps: %d \n",page_WS_max_sweeps);
			//int32 max_sweep_ctr_N = ((uint32)page_WS_max_sweeps & (~(1<<31)));
			int32 max_sweep_ctr_N = page_WS_max_sweeps * -1;
			//cprintf("In Negative--- max_sweep_ctr_N: %u \n",max_sweep_ctr_N);
			//cprintf("In Negative--- test: %d \n",~page_WS_max_sweeps);
			while(1 == 1){

				int32 permissions = pt_get_page_permissions(faulted_env->env_page_directory,faulted_env->page_last_WS_element->virtual_address);
				if((permissions & PERM_USED) == PERM_USED){ // bit = 1
					pt_set_page_permissions(faulted_env->env_page_directory,faulted_env->page_last_WS_element->virtual_address,0,PERM_USED);
					faulted_env->page_last_WS_element->sweeps_counter = 0;


				}else{ // used bit = zero
					faulted_env->page_last_WS_element->sweeps_counter = faulted_env->page_last_WS_element->sweeps_counter + 1;

					if(((permissions & PERM_MODIFIED) == PERM_MODIFIED && faulted_env->page_last_WS_element->sweeps_counter == max_sweep_ctr_N + 1) || ((permissions & PERM_MODIFIED) == 0 && faulted_env->page_last_WS_element->sweeps_counter == max_sweep_ctr_N)){
						cprintf("I will replace ............. \n");
						uint32 *ptr_page_table;
						get_page_table(faulted_env->env_page_directory,faulted_env->page_last_WS_element->virtual_address,&ptr_page_table);
						struct FrameInfo* ptr_frame_info = get_frame_info(faulted_env->env_page_directory,faulted_env->page_last_WS_element->virtual_address,&ptr_page_table);

						if((permissions & PERM_MODIFIED) == PERM_MODIFIED){ // Modified Page >> save on disk
							int ret = pf_update_env_page(faulted_env, faulted_env->page_last_WS_element->virtual_address, ptr_frame_info);
						}
						// if not modified or modified (it doesn't matter)
						// Replacing the victim , delete it

						ptr_frame_info->addr_working_set_element = NULL;
						//free(((void *)faulted_env->page_last_WS_element->virtual_address)); // Clear used bit , ...

						// unmap frame and clear the bit that indicated that it's allocated page but it's still marked page
						unmap_frame(faulted_env->env_page_directory,faulted_env->page_last_WS_element->virtual_address);
						pt_set_page_permissions(faulted_env->env_page_directory, faulted_env->page_last_WS_element->virtual_address, 0, 2048);

						// Add faulted page in memory (frame)
						int allocation_result = allocate_frame(&ptr_frame_info);
						if (allocation_result != 0) {
							env_exit();
							return;
						}

						int mapping_result = map_frame(faulted_env->env_page_directory,ptr_frame_info, fault_va, PERM_PRESENT | PERM_WRITEABLE | PERM_USER); // I dk about the permissions
						if (mapping_result != 0) {
							//free_frame(frame_info);
							free_frame(ptr_frame_info);
							env_exit();
							return;
						}

						// Read it from disk
						int ret = pf_read_env_page(faulted_env, (void *)fault_va);
						if(ret ==  E_PAGE_NOT_EXIST_IN_PF){
							if (!((fault_va < (uint32) USER_HEAP_MAX && fault_va >= (uint32) USER_HEAP_START) || (fault_va < (uint32) USTACKTOP && fault_va >= (uint32) USTACKBOTTOM))) {
								free_frame(ptr_frame_info);
								env_exit();

							}

						}



						faulted_env->page_last_WS_element->virtual_address = fault_va;
						faulted_env->page_last_WS_element->sweeps_counter = 0;
						ptr_frame_info->addr_working_set_element = faulted_env->page_last_WS_element;

						//VIMP FOR Marking the page is faulted and save the address of working set element cuz when freeing to be in O(1)


						pt_set_page_permissions(faulted_env->env_page_directory, fault_va, 2048, 0); // <<<<<< it's already marked 3shan kda etrma 3leha fault fa bn3ml kman allocated bit
						ptr_flag_isAdded = 1;

					}
				}



				if(faulted_env->page_last_WS_element == LIST_LAST(&(faulted_env->page_WS_list))){
					cprintf("I'm last element \n");
					cprintf("Addr of last working set element in list: %u \n",(uint32)faulted_env->page_last_WS_element);
					faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
				}else{
					cprintf("Addr of current working set element in list: %u \n",(uint32)faulted_env->page_last_WS_element);
					faulted_env->page_last_WS_element = LIST_NEXT(faulted_env->page_last_WS_element);
					cprintf("Addr of next working set element in list: %u \n",(uint32)faulted_env->page_last_WS_element);
				}
				if(ptr_flag_isAdded){
					tlb_invalidate(faulted_env->env_page_directory, (void *) fault_va);
					break;
				}

			}



		}
	}

}

void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va)
{
	//[PROJECT] PAGE FAULT HANDLER WITH BUFFERING
	// your code is here, remove the panic and write your code
	panic("__page_fault_handler_with_buffering() is not implemented yet...!!");
}

