#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include "memory_manager.h"

//Initialize the dynamic allocator of kernel heap with the given start address, size & limit
//All pages in the given range should be allocated
//Remember: call the initialize_dynamic_allocator(..) to complete the initialization
//Return:
//	On success: 0
//	Otherwise (if no memory OR initial size exceed the given limit): PANIC
int initialize_kheap_dynamic_allocator(uint32 daStart, uint32 initSizeToAllocate, uint32 daLimit)
{
	//TODO: [PROJECT'24.MS2 - #01] [1] KERNEL HEAP - initialize_kheap_dynamic_allocator
	// Write your code here, remove the panic and write your code
	//panic("initialize_kheap_dynamic_allocator() is not implemented yet...!!");

	initSizeToAllocate = ROUNDUP(initSizeToAllocate,PAGE_SIZE);
	START_KHEAP_BLOCK_ALLOCATOR = (uint32 *)daStart;
	BRK = (uint32 *)(daStart+initSizeToAllocate);// first byte of unmapped region // not sure but that way points to the last byte in the end block
	HARD_LIMIT = (uint32 *)daLimit;
	if((uint32)BRK-1 >= (uint32)HARD_LIMIT){ // OR BRK-1byte
		panic("");
		return -1;
	}
	uint32 noPages = initSizeToAllocate/PAGE_SIZE;
	if(MemFrameLists.free_frame_list.size < noPages){
		panic("");
		return -1;
	}
	acquire_spinlock(&(MemFrameLists.mfllock));
	for(int i = 0,j=0; i < noPages;i++,j+=PAGE_SIZE){
		struct FrameInfo *ptr_frame_info;
		int ret = allocate_frame(&ptr_frame_info);
		if(ret == E_NO_MEM){
			panic("");
			release_spinlock(&(MemFrameLists.mfllock));
			return -1;
		}
		ret = map_frame(ptr_page_directory,ptr_frame_info,daStart+j,PERM_PRESENT | PERM_WRITEABLE);
		if(ret == E_NO_MEM){
			panic("");
			release_spinlock(&(MemFrameLists.mfllock));
			return -1;
		}
	}
	release_spinlock(&(MemFrameLists.mfllock));

	// Block allocator initialization
	//cprintf("Size in kernel heap initalize dynamic allocator: %u \n",initSizeToAllocate); // including size of begin block and end block (8 bytes)
	initialize_dynamic_allocator(daStart,initSizeToAllocate);
	//cprintf("Size of object of FreePagesInfo: %u \n", sizeof(struct FreePagesInfo));

	KH_page_allocator_flag = 0;
	return 0;
}

void* sbrk(int numOfPages)
{
	/* numOfPages > 0: move the segment break of the kernel to increase the size of its heap by the given numOfPages,
	 * 				you should allocate pages and map them into the kernel virtual address space,
	 * 				and returns the address of the previous break (i.e. the beginning of newly mapped memory).
	 * numOfPages = 0: just return the current position of the segment break
	 *
	 * NOTES:
	 * 	1) Allocating additional pages for a kernel dynamic allocator will fail if the free frames are exhausted
	 * 		or the break exceed the limit of the dynamic allocator. If sbrk fails, return -1
	 */

	//MS2: COMMENT THIS LINE BEFORE START CODING==========
	//====================================================

	//TODO: [PROJECT'24.MS2 - #02] [1] KERNEL HEAP - sbrk
	// Write your code here, remove the panic and write your code
	//panic("sbrk() is not implemented yet...!!");
	if(numOfPages <= 0){
		return BRK;
	}
	uint32 noBytes = numOfPages * PAGE_SIZE;
	if(((uint32)BRK)+noBytes-1 >= (uint32) HARD_LIMIT){ // exceed Hard Limit, (uint32)BRK)+noBytes-1 = LAST BYTE OF I CAN ALLOCATE IN IT
		return (void*)-1 ;// casting -1 to address (void pointer)
	}
	if(MemFrameLists.free_frame_list.size < numOfPages){ // No memory
		return (void*)-1 ;// casting -1 to address (void pointer)
	}

	acquire_spinlock(&(MemFrameLists.mfllock));
	for(int i = 0,j=0; i < numOfPages;i++,j+=PAGE_SIZE){
		struct FrameInfo *ptr_frame_info;
		int ret = allocate_frame(&ptr_frame_info);
		if(ret == E_NO_MEM){
			panic("");
			release_spinlock(&(MemFrameLists.mfllock));
			return (void*)-1;
		}
		ret = map_frame(ptr_page_directory,ptr_frame_info,(uint32)BRK+j,PERM_PRESENT | PERM_WRITEABLE);
		if(ret == E_NO_MEM){
			panic("");
			release_spinlock(&(MemFrameLists.mfllock));
			return (void*)-1;
		}
	}
	release_spinlock(&(MemFrameLists.mfllock));
	uint32 *old_BRK = BRK;

	BRK = (uint32 *)(((uint32)BRK)+numOfPages*PAGE_SIZE); // /// CHECK ADDRESSES AGAIN  first byte of unmapped region not the last byte of mapped region
	// start block , end block >> hdod heap
	return old_BRK;

}

//TODO: [PROJECT'24.MS2 - BONUS#2] [1] KERNEL HEAP - Fast Page Allocator

void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'24.MS2 - #03] [1] KERNEL HEAP - kmalloc
	// Write your code here, remove the panic and write your code
	//kpanic_into_prompt("kmalloc() is not implemented yet...!!");
	// use "isKHeapPlacementStrategyFIRSTFIT() ..." functions to check the current strategy

	//cprintf("Size You wanna allocate: %u \n",size);

	//cprintf("No of Pages in Kernel Heap in Page Allocator: %u \n",(KERNEL_HEAP_MAX-((uint32)HARD_LIMIT+4096))/PAGE_SIZE);
	// when checking size + 8 <= DYN_ALLOC_MAX_BLOCK_SIZE  OR size only

	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE){ // Block Allocator Region
		cprintf("I'm in Kmalloc Block ALLOCATOR REGION \n");
		if(isKHeapPlacementStrategyFIRSTFIT()){
			return alloc_block_FF(size);
		}
		if(isKHeapPlacementStrategyBESTFIT()){
			return alloc_block_BF(size);
		}
		//cprintf("I'm in END BLOCK ALLOCATOR REGION \n");
	}else{ // Page Allocator
		cprintf("I'm in Kmalloc Page ALLOCATOR REGION \n");

		if(KH_page_allocator_flag == 0){
			// Page allocator
			LIST_INIT(&(freePagesList_KH));
			//cprintf("Size of object of FreePagesInfo: %u \n",sizeof(struct FreePagesInfo));
			//cprintf("Size of free Block List BEFORE: %u \n",freeBlocksList.size);
			//cprintf("Size of block BEFORE MALLOC: %u \n",*((uint32*)((uint32)LIST_FIRST(&freeBlocksList) - 4))); // without begin block and end block, cuz this is the size of the block and its meta data only
			struct FreePagesInfo *first_page = (struct FreePagesInfo *) kmalloc(sizeof(struct FreePagesInfo));
			first_page->va = (uint32)HARD_LIMIT + PAGE_SIZE;
			//cprintf("AFTER initialization va \n");
			uint32 no_free_pages = KERNEL_HEAP_MAX - ((uint32)HARD_LIMIT + 4096);// first byte of first page in page allocator
			no_free_pages = ROUNDUP(no_free_pages,PAGE_SIZE);
			first_page->no_consecutive_pages = no_free_pages/PAGE_SIZE; // number of pages
			LIST_INSERT_HEAD(&(freePagesList_KH),first_page);
			KH_page_allocator_flag = 1;
		}

		uint32 noOfBytes = ROUNDUP(size,(uint32)PAGE_SIZE);
		uint32 noOfPages = noOfBytes / PAGE_SIZE;



		uint8 isFound = 0; // flag
		uint32 no_consecutive_pages = 0;
		void *Begin_Page_Ptr = NULL;

		if(MemFrameLists.free_frame_list.size < noOfPages){ // Not enough frames in Physical memory
			return NULL;
		}

		struct FreePagesInfo *element;
		LIST_FOREACH(element, &(freePagesList_KH))
		{
			// 3 cases
			// size exactly equal > remove
			// size greater > split , remove ,add (DON"T REMOVE JUST MODIFY ON IT)
			// size doesn't fit

			if(element->no_consecutive_pages == noOfPages){

				Begin_Page_Ptr= (void *)element->va;
				free_block(element);
				LIST_REMOVE(&(freePagesList_KH), element);
				// Mark pages as it reversed either permissions , va : size in array or list

				isFound = 1;
				break;
			}else if(element->no_consecutive_pages > noOfPages){
				Begin_Page_Ptr=(void *)element->va;
				element->va = element->va + (noOfPages * PAGE_SIZE);
				element->no_consecutive_pages = element->no_consecutive_pages - noOfPages;
				isFound = 1;
				break;
			}

		}
		if(isFound == 0){ // Not enough pages in VM
			return NULL;
		}



		// isFound == 1
		uint32 *page_ptr = (uint32 *) Begin_Page_Ptr;
		acquire_spinlock(&(MemFrameLists.mfllock));
		uint32 page_no = 1; // first page
		while(page_no <= noOfPages){ // condition is true, any no is true except zero
			struct FrameInfo *ptr_frame_info;
			int ret = allocate_frame(&ptr_frame_info);
			if(ret == E_NO_MEM){
				release_spinlock(&(MemFrameLists.mfllock));
				return NULL;
			}
			// Permission Bits
			// There are available 3 bits for programmer use their number is 9,10,11 (zero based)
			if(noOfPages == 1){
				ret = map_frame(ptr_page_directory,ptr_frame_info,(uint32)page_ptr,PERM_PRESENT | PERM_WRITEABLE);
			}else if((page_no == 1 || page_no == noOfPages) && noOfPages > 1){ //noOfPages > 1 : means consecutive, This condition for the first and last pages in Consecutive page we mark them by the same one available bit in available bits permissions, we use only one of the 3
				ret = map_frame(ptr_page_directory,ptr_frame_info,(uint32)page_ptr,(1 << 9) | PERM_PRESENT | PERM_WRITEABLE); // 1 << 8 3ml fy bit no 7 tkon b 1
			}else{
				ret = map_frame(ptr_page_directory,ptr_frame_info,(uint32)page_ptr,PERM_PRESENT | PERM_WRITEABLE);
			}
			if(ret == E_NO_MEM){
				release_spinlock(&(MemFrameLists.mfllock));
				return NULL;
			}
			page_ptr = (uint32 *) ((uint32)page_ptr + PAGE_SIZE);
			page_no++;
		}
		release_spinlock(&(MemFrameLists.mfllock));
		return Begin_Page_Ptr;

		// allocate Frame and mapping va of page to a frame
		// check what allocate frame and map frame returns cuz it may return NO_MEM, I won't check size of free frames list first
	}
	return NULL;
}

void kfree(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #04] [1] KERNEL HEAP - kfree
	// Write your code here, remove the panic and write your code
	//panic("kfree() is not implemented yet...!!");

	//you need to get the size of the given allocation using its address
	//refer to the project presentation and documentation for details

	uint32 va = (uint32) virtual_address;
	if(va >= (uint32)START_KHEAP_BLOCK_ALLOCATOR && va < (uint32) BRK){ // Block Allocator Region
		free_block(virtual_address);

	}else if(va >= ((uint32)HARD_LIMIT + 4) && va < (uint32)KERNEL_HEAP_MAX){ //Page Allocator Region
		uint32 *ptr_page_table;
		get_page_table(ptr_page_directory,va,&ptr_page_table); // we must put condition if it returns zero or no mem , or create parameter is zero or one
		struct FrameInfo *frame_info_ptr = get_frame_info(ptr_page_directory,va,&ptr_page_table); // bndeha virtual addr of page w btrg3lna frame ely elpage mrbota beha
		//uint32 permissions = pt_get_page_permissions(frame_info_ptr->proc,va); // the process which allocated the frame it's allocated and mapped by it's page (process of frame is the process of page)
		if(frame_info_ptr == NULL){
			// it's a free page , doesn't have corresponding frame (not mapped)
			panic("");
			return;
		}
		uint32 permissions = pt_get_page_permissions(ptr_page_directory,va); // the process which allocated the frame it's allocated and mapped by it's page (process of frame is the process of page)
		acquire_spinlock(&(MemFrameLists.mfllock));

		uint32 *begin_page = NULL;
		uint32 *last_page = NULL;


		uint32 page_no = 1; // == no of frames allocated
		if((permissions & (1 << 9)) == 512){ // permissions & (1 << 9)) == 1 by binary - dr rezk 2al 3la trick dy bs msh fakerha
			begin_page = (uint32 *)va;
			uint32 END_FLAG = 0;
			while(1){ // va >= ((uint32)HARD_LIMIT + 4) && va < (uint32)KERNEL_HEAP_MAX
				if((permissions & (1 << 9)) == 512 && page_no > 1){ // permissions & (1 << 9)) == 1 to check the bit no 10 is 1 or zero, if the result is 1 so it was 1 cuz 1&1=1 and if res = 0 it was 0 cuz 0&1=0
					END_FLAG = 1;
					last_page = (uint32 *)va;
				}
				pt_set_page_permissions(ptr_page_directory,va,0,PERM_AVAILABLE);
				unmap_frame(ptr_page_directory,va);
				if(END_FLAG){
					break;
				}
				va = va + PAGE_SIZE;
				permissions = pt_get_page_permissions(ptr_page_directory,va); // <-------- HERE OR ABOVE
				page_no++;
			}
		}else{
			//cprintf("I'm In single free page \n");
			// just free this page and its corresponding frame (there is no consecutive allocated pages together just a single one)
			pt_set_page_permissions(ptr_page_directory,va,0,PERM_AVAILABLE);
			unmap_frame(ptr_page_directory,va);

			// for free pages list
			begin_page = (uint32 *)va;
		}
		release_spinlock(&(MemFrameLists.mfllock));
		// Frame of the givan page virtual address (UNMAP)


		// Free Pages Info List
		// last_page > NULL : Single Page : NOT Consecutive
		//1. Single page only




		// page_no variable indicates to number of pages

		//2. Consecutive Pages
		uint32 diff_right;
		uint32 diff_left;
		uint32 isFoundInList = 0;
		struct FreePagesInfo *element;
		LIST_FOREACH(element, &(freePagesList_KH))
		{

			if(va < element->va){
				// insert before

				// kmalloc if needed in case
				if(element == LIST_FIRST(&(freePagesList_KH))){

					if(page_no == 1){ // single page not consecutive
						diff_right = element->va - (uint32)begin_page;
					}else{ // consecutive
						diff_right = element->va - (uint32)last_page; // consecutive
					}
					if(diff_right == PAGE_SIZE){
						element->va = (uint32)begin_page;
						element->no_consecutive_pages = element->no_consecutive_pages + page_no;
					}else{
						struct FreePagesInfo *first_page = (struct FreePagesInfo *) kmalloc(sizeof(struct FreePagesInfo));
						first_page->va = (uint32)begin_page;
						first_page->no_consecutive_pages = page_no; // number of pages, 3dd pages ely b3dy fadya (i'm included)
						LIST_INSERT_HEAD(&(freePagesList_KH),first_page);
					}

				}
				else{
					if(page_no == 1){ // single page not consecutive
						diff_right = element->va - (uint32)begin_page;
					}else{ // consecutive
						diff_right = element->va - (uint32)last_page;
					}
					struct FreePagesInfo *prev_page_in_list = LIST_PREV(element);
					diff_left = (uint32)begin_page - (prev_page_in_list->va + ((prev_page_in_list->no_consecutive_pages -1) * PAGE_SIZE));
					// element >> daymn ely b3dy
					// prev_page_in_list >> ely ably
					if(diff_right == PAGE_SIZE && diff_left != PAGE_SIZE){
						element->va = (uint32)begin_page;
						element->no_consecutive_pages = element->no_consecutive_pages + page_no;

					}else if(diff_right != PAGE_SIZE && diff_left == PAGE_SIZE){
						prev_page_in_list->no_consecutive_pages = prev_page_in_list->no_consecutive_pages + page_no;

					}else if(diff_right == PAGE_SIZE && diff_left == PAGE_SIZE){
						//cprintf("I'm HERE in Merging all blocks to one virtually \n");
						//cprintf("prev_page_in_list va: %u \n",prev_page_in_list->va);
						//cprintf("next_page_in_list va: %u \n",element->va);
						prev_page_in_list->no_consecutive_pages = prev_page_in_list->no_consecutive_pages + page_no + element->no_consecutive_pages;
						free_block(element);
						LIST_REMOVE(&(freePagesList_KH),element);

					}else{ //diff_right != PAGE_SIZE && diff_left != PAGE_SIZE
						// hn7otha fy elnos we don't need to calculate min difference between them kda kda h7otha between them
						struct FreePagesInfo *page_insert = (struct FreePagesInfo *) kmalloc(sizeof(struct FreePagesInfo));
						page_insert->va = (uint32)begin_page;
						page_insert->no_consecutive_pages = page_no; // number of pages
						LIST_INSERT_BEFORE(&(freePagesList_KH),element,page_insert);
					}

				}




				isFoundInList = 1;
				break;
			}
			// va elsoghyr ykon fy elawl , w va elkber ykon fy alakhr to keep it sorted


		}
		// 2 cases: isFoundInList == 0
		// akbr wahd fa ygeb ely 2blh
		// or en list aslun fadya fa yb2a hoa awl element
		//LIST_SIZE(&(freePagesList_KH)) > 0 : not empty
		// LIST_SIZE(&(freePagesList_KH)) == 0 : empty : No free pages , Page allocator in Kernel HEAP is FULL ALL ALLOCATED
		if(isFoundInList == 0 && LIST_SIZE(&(freePagesList_KH)) > 0){ // means ml2tsh va fy list asghr mny fa ana hb2a akhr element
			// check 3la ely 2bly difference b page size 3shan lw feh merging
			struct FreePagesInfo *last_page_in_list = LIST_LAST(&(freePagesList_KH));
			diff_left = (uint32)begin_page - (last_page_in_list->va + ((last_page_in_list->no_consecutive_pages - 1) * PAGE_SIZE));
			if(diff_left == PAGE_SIZE){
				//cprintf("IT SHOULD BE HERE \n");
				//cprintf("No of consecutive pages: %u \n",last_page_in_list->no_consecutive_pages);
				last_page_in_list->no_consecutive_pages = last_page_in_list->no_consecutive_pages + page_no;
			}else{
				//cprintf("IT SHOULD not BE HERE \n");
				struct FreePagesInfo *last_page_in_list = (struct FreePagesInfo *) kmalloc(sizeof(struct FreePagesInfo));
				last_page_in_list->va = (uint32)begin_page;
				last_page_in_list->no_consecutive_pages = page_no; // number of pages
				LIST_INSERT_TAIL(&(freePagesList_KH),last_page_in_list);
			}

		}else if(isFoundInList == 0 && LIST_SIZE(&(freePagesList_KH)) == 0){
			struct FreePagesInfo *first_element_in_list = (struct FreePagesInfo *) kmalloc(sizeof(struct FreePagesInfo));
			first_element_in_list->va = (uint32)begin_page;
			first_element_in_list->no_consecutive_pages = page_no; // number of pages
			LIST_INSERT_HEAD(&(freePagesList_KH),first_element_in_list);
		}


	}else{ // Invalid
		panic("");
	}
	return;

}


unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #05] [1] KERNEL HEAP - kheap_physical_address
	// Write your code here, remove the panic and write your code
	//panic("kheap_physical_address() is not implemented yet...!!");

	//return the physical address corresponding to given virtual_address
	//refer to the project presentation and documentation for details

	uint32 *ptr_page_table;
	get_page_table(ptr_page_directory,virtual_address,&ptr_page_table);
	//uint32 *ptr_page_table = ptr_page_directory[PDX(virtual_address)]; //get_page_table();
	struct FrameInfo* ptr_frame_info = get_frame_info(ptr_page_directory, virtual_address, &ptr_page_table);
	if(ptr_frame_info == NULL){ // page not mapped to a frame
		return 0;
	}
	//uint32 *frame_no = ptr_page_table[PTX(virtual_address)]; // frame addr + permissions (shifting 3shan nshel permissions)
	//uint32 physical_addr = to_physical_address(ptr_frame_info); // Frame Base address
	uint32 page_offset = PGOFF(virtual_address);
	uint32 frame_no = to_frame_number(ptr_frame_info);
	uint32 addr = (frame_no * PAGE_SIZE) + page_offset;
	// physical addr rkm byte not rkm frame no

	// two ways to reach physical addr
	// 1. frame no * frame_size + offset ,, or concatenating base frame address with offset not adding (concatenationg maybe using shifting)
	return addr;
	// return physical_addr + page_offset;
	//EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED ==================
}

unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT'24.MS2 - #06] [1] KERNEL HEAP - kheap_virtual_address
	// Write your code here, remove the panic and write your code
	//panic("kheap_virtual_address() is not implemented yet...!!");
	// virtual address of the frames for this physical addr not va for pages mapped to this physical address

	struct FrameInfo* ptr_frame_info = to_frame_info(physical_address);
	if(ptr_frame_info->references == 0){ // there is no page mapped to that frame
		return 0;
	}
	//cprintf("Frame info addr: %p \n",ptr_frame_info);
	uint32 frame_no = to_frame_number(ptr_frame_info);
	uint32 offset = physical_address - (frame_no * PAGE_SIZE);

	//uint32 base_frame_addr = ROUNDDOWN(physical_address,PAGE_SIZE);
	//uint32 offset = physical_address - base_frame_addr;
	//return (uint32)ptr_frame_info + offset;
	return (uint32)(ptr_frame_info->base_va_page) + offset; // lazm casting 3shan my3mlsh pointers arithmetic
	//return the virtual address corresponding to given physical_address
	//refer to the project presentation and documentation for details

	//EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED ==================
}
//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, if moved to another loc: the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

void *krealloc(void *virtual_address, uint32 new_size)
{
	// new_size: dh elsize ely 3ayz saaaafy y3mlh allocate


	// this virtual address I assume it's already allocated
	//TODO: [PROJECT'24.MS2 - BONUS#1] [1] KERNEL HEAP - krealloc
	// Write your code here, remove the panic and write your code
	//panic("krealloc() is not implemented yet...!!");
	if(new_size == 0){
		kfree(virtual_address);
		return NULL;
	}
	if(virtual_address == NULL){
		return kmalloc(new_size);
	}
	uint32 va = (uint32) virtual_address;
	if(va >= KERNEL_HEAP_START && va < (uint32)BRK){
		// Block Allocator
		// k realloc > kernel heap realloc (specific only for kernel not user) , we said kernel heap already all pages have entries in page tables
		// and in initialize kernel heap dynamic allocator , we map the pages of the block allocator to frames , and even if we use sbrk()
		// so it's already mapped to frames don't worry

		// va is the first byte after the header
		uint32 *header = (uint32 *) (va - 4);
		// this address is already allocated so the size will be odd cuz LSB = 1 to indicate it's allocated take care

		uint32 old_size = (*header) - 9; // i think it will be -9 to calculate actual size of allocated block without header and footer, -9 to exclude header and footer size and LSB = 1 cuz it was just indication that this block was allocated

		if(old_size > new_size){ // hnsghr
			return realloc_block_FF(virtual_address,new_size);
		}else if (old_size < new_size){ // hnkbr
			if(new_size <= DYN_ALLOC_MAX_BLOCK_SIZE){
				// I'm in also block allocator range
				return realloc_block_FF(virtual_address,new_size); // make sure it it can't , it will return null but will not free the virtual address and it is still valid
			}else{
				// I will be in Page allocator region

				// 1. Don't forget to copy data before freeing blocks in block allocator
				// 2. I need to free the blocks in block allocator
				// 3. allocate pages in page allocator
				// make sure there is enough pages and frames in page allocator first before freeing 3shan fy case htrg3 null w tseb old virtual address remains valid zy ma hoa msh htms7 haga

				uint32 *addr_first_page = kmalloc(new_size); // this checks on no of pages and no of frames inside it
				if(addr_first_page == NULL){
					return NULL;
				}
				memcpy((void *)addr_first_page,virtual_address,old_size);// khaly balk hn3ml copy lldata mn size elsoghyr msh elkbeer bs azon kan lazem oldsize - 8, 3shan hoa by3ml copy lldata mn first byte after header w byhsb 3dd bytes fa elmafrod tdelh size mn gher size header w footer
				kfree(virtual_address); // this will call free block
				return addr_first_page;
				// imp question
				// old size we do include size of header and footer (8 bytes) or we should exclude it, meaning the actual size of allocated block (payload)

				// ana msh faker fy parameter kan byb3t size with its meta data wla actual size ely 3ayz yhgzh saaafy
				// w lw hkarn old size b new size , old size hkon 3amel include l size of header and footer 3shan b2a hoa size bta3 block kolhh




			}

		}else{ // new_size == old_size
			return virtual_address; // OR return NULL , I don't know
		}


	}else if(va >= (uint32)HARD_LIMIT+PAGE_SIZE && va < KERNEL_HEAP_MAX){
		// PAGE ALLOCATOR

		// To know old size to be able to compare it with new size
		// it was better if we created data structure so we can get size we allocated in O(1) instead looping and checking on permissions
		uint32 old_size;

		uint32 virtualAddress = va;
		uint32 permissions = pt_get_page_permissions(ptr_page_directory,virtualAddress); // the process which allocated the frame it's allocated and mapped by it's page (process of frame is the process of page)
		//acquire_spinlock(&(MemFrameLists.mfllock));
		uint32 no_pages = 0; // no frames = no of pages
		if((permissions & (1 << 9)) == 512){ // meaning consecutive not single
			uint32 END_FLAG = 0;
			while(1){
				if((permissions & (1 << 9)) == 512){
					END_FLAG = 1;
				}
				no_pages++;
				if(END_FLAG){
					break;
				}
				virtualAddress = virtualAddress + PAGE_SIZE;
				permissions = pt_get_page_permissions(ptr_page_directory,virtualAddress); // <-------- HERE OR ABOVE

			}
		}else{ // single page
			no_pages = 1;
		}

		old_size = no_pages * PAGE_SIZE; // bytes

		//new_size = ROUND_UP(new_size,PAGE_SIZE);

		if(old_size > new_size){ // hysghr
			if(new_size <= DYN_ALLOC_MAX_BLOCK_SIZE){
				// will move from page allocator to block allocator
				// don't forget to move data , free pages in page allocator , allocate in block allocator, but you need to allocate first to make sure there is space or not

				uint32 *addr_block = kmalloc(new_size); // this will call alloc_block_ff
				if(addr_block == NULL){
					return NULL;
				}
				memcpy((void *)addr_block,virtual_address,new_size);// not sure but maybe right hta lw hnsghr bs hnnkl eldata elbakya fy block
				kfree(virtual_address); // this will free the pages in Kernel Heap page allocator
				return addr_block;
			}else{
				// still in page allocator but will (unmap pages) unmap inside it not link the page to that frame and free the frame (if no of references = 0) , it will add it to free frames list
				// not implemented yetttttttt !!!!!!!!!
				uint32 begin_free = va + (new_size);
				uint32 no_free_pages = (old_size - new_size)/PAGE_SIZE;
				uint32 no_allocated_pages = no_pages - no_free_pages;

				// there will no case that I was single page w hoa 3ayz ysghr (kont hroh b2a l block allocator), min 2 pages w hoa 3ayz msln yshel 1 page mnhm

				// nwhmha en begin_free tkon start page , base addr
				if(no_free_pages > 1){ // to mark first consecutive page available bit = 1, kda kda last conseuctive page mhdsh gh gmbha fa hya still b 1
					pt_set_page_permissions(ptr_page_directory,begin_free,512,0); // set available bit
				}else{ // kfree single page
					pt_set_page_permissions(ptr_page_directory,begin_free,0,512); // clear, 3shan awhmh enha single page
				}

				kfree((void *) begin_free);


				// make the last consecutive page available bit set by one to mart it that it was the last page in consecutive pages
				if(no_allocated_pages > 1){ // ehna fy goz2 allocated pages , daymn start page is marked , 3ayzen nhdd fen new last page (either in consecutive pages case or single page)
					pt_set_page_permissions(ptr_page_directory,(begin_free - PAGE_SIZE),512,0); // set available bit by 1
				}else{
					pt_set_page_permissions(ptr_page_directory,(begin_free - PAGE_SIZE),0,512);
				}

				return virtual_address;

			}

		}else if(old_size < new_size){ // hnkbr
			// check consecutive pages after the last page allocated in consecutive pages
			// if no. of consecutive pages hykdy fa ht3ml map to frame (w hya automatic htshelha mn free frames list), lw laa h3ml kmalloc w h3ml copy ldata w h3ml free l pages eladema w return new addr
			// mmkn aslun mykonsh feh frames fy memory fadya (free frames list) b3edn en mmkn mykonsh feh aslun no of consecutive pages fadya fy virtual memory

			// check it is mapped to frame or not to check the page is free or not

			uint32 addr_of_page_after = va + (no_pages * PAGE_SIZE);

			/*uint32 *ptr_page_table;
			get_page_table(ptr_page_directory,addr_of_page_after,&ptr_page_table);*/
			uint32 isFoundInListAndSufficient = 0;
			struct FreePagesInfo *element;
			void *Begin_Page_Ptr = NULL;
			uint32 difference_pages = (new_size/PAGE_SIZE) - no_pages;
			LIST_FOREACH(element, &(freePagesList_KH))
			{
				if(element->va == addr_of_page_after){
					if(element->no_consecutive_pages == difference_pages){
						// allocate it , remove it free pages list
						Begin_Page_Ptr = (void *)element->va;
						free_block(element);
						LIST_REMOVE(&(freePagesList_KH), element);
						isFoundInListAndSufficient = 1;
						break;
					}else if(element->no_consecutive_pages > new_size/PAGE_SIZE){
						// allocate it ,split it, remove it free pages list
						Begin_Page_Ptr = (void *)element->va;
						element->va = element->va + (difference_pages * PAGE_SIZE);
						element->no_consecutive_pages = element->no_consecutive_pages - difference_pages;
						isFoundInListAndSufficient = 1;
						break;

					}
				}
			}
			if(isFoundInListAndSufficient == 0){
				uint32 *addr_new_pages = kmalloc(new_size);
				if(addr_new_pages == NULL){
					return NULL;
				}
				memcpy((void *)addr_new_pages,virtual_address,old_size);
				kfree(virtual_address);
				return addr_new_pages;
			}else{
				// use available bits
				// see the code of kmalloc()
				if(MemFrameLists.free_frame_list.size < difference_pages){
					return NULL;
				}

				if(no_pages == 1){ // it was single page, so its available bit permission remains one
					uint32 *page_ptr = (uint32 *) Begin_Page_Ptr;
					acquire_spinlock(&(MemFrameLists.mfllock));
					uint32 page_no = 1; // first page
					while(page_no <= difference_pages){ // condition is true, any no is true except zero
						struct FrameInfo *ptr_frame_info;
						int ret = allocate_frame(&ptr_frame_info);
						if(ret == E_NO_MEM){
							release_spinlock(&(MemFrameLists.mfllock));
							return NULL;
						}
						// Permission Bits
						// There are available 3 bits for programmer use their number is 9,10,11 (zero based)
						if(page_no == difference_pages){ // page_no == difference_pages --> Last Consecutive Page OR // he will allocate one page after existing allocated pages
							ret = map_frame(ptr_page_directory,ptr_frame_info,(uint32)page_ptr,(1 << 9) | PERM_PRESENT | PERM_WRITEABLE); // 1 << 8 3ml fy bit no 7 tkon b 1
						}else{ // pages ely fy elnos fy consecutive pages
							ret = map_frame(ptr_page_directory,ptr_frame_info,(uint32)page_ptr,PERM_PRESENT | PERM_WRITEABLE);
						}
						if(ret == E_NO_MEM){
							release_spinlock(&(MemFrameLists.mfllock));
							return NULL;
						}
						page_ptr = (uint32 *) ((uint32)page_ptr + PAGE_SIZE);
						page_no++;
					}
					release_spinlock(&(MemFrameLists.mfllock));
					return Begin_Page_Ptr;

				}else{ // old size >>> Consecutive Pages
					pt_set_page_permissions(ptr_page_directory,((uint32)virtual_address + old_size),0,512); // clear the availabe bit of the old last page cuz it will be in the middle of consecutive pages not the last page

					uint32 *page_ptr = (uint32 *) Begin_Page_Ptr;
					acquire_spinlock(&(MemFrameLists.mfllock));
					uint32 page_no = 1; // first page
					while(page_no <= difference_pages){ // condition is true, any no is true except zero
						struct FrameInfo *ptr_frame_info;
						int ret = allocate_frame(&ptr_frame_info);
						if(ret == E_NO_MEM){
							release_spinlock(&(MemFrameLists.mfllock));
							return NULL;
						}
						// Permission Bits
						// There are available 3 bits for programmer use their number is 9,10,11 (zero based)
						if(page_no == difference_pages){ // page_no == difference_pages --> Last Consecutive Page OR // he will allocate one page after existing allocated pages
							ret = map_frame(ptr_page_directory,ptr_frame_info,(uint32)page_ptr,(1 << 9) | PERM_PRESENT | PERM_WRITEABLE); // 1 << 8 3ml fy bit no 7 tkon b 1
						}else{ // pages ely fy elnos fy consecutive pages (ay haga fy elnos)
							ret = map_frame(ptr_page_directory,ptr_frame_info,(uint32)page_ptr,PERM_PRESENT | PERM_WRITEABLE);
						}
						if(ret == E_NO_MEM){
							release_spinlock(&(MemFrameLists.mfllock));
							return NULL;
						}
						page_ptr = (uint32 *) ((uint32)page_ptr + PAGE_SIZE);
						page_no++;
					}
					release_spinlock(&(MemFrameLists.mfllock));
					return Begin_Page_Ptr;


				}



			}




		}else{ // old_size == new_size
			return virtual_address; // I don't know return NULL or what
		}
	}
	return NULL;
}
