/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"
#include "../inc/uheap.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//=====================================================
// 1) GET BLOCK SIZE (including size of its meta data):
//=====================================================
__inline__ uint32 get_block_size(void* va)
{
	uint32 *curBlkMetaData = ((uint32 *)va - 1) ;
	return (*curBlkMetaData) & ~(0x1);
}

//===========================
// 2) GET BLOCK STATUS:
//===========================
__inline__ int8 is_free_block(void* va)
{
	uint32 *curBlkMetaData = ((uint32 *)va - 1) ;
	return (~(*curBlkMetaData) & 0x1) ;
}

//===========================
// 3) ALLOCATE BLOCK:
//===========================

void *alloc_block(uint32 size, int ALLOC_STRATEGY)
{
	void *va = NULL;
	switch (ALLOC_STRATEGY)
	{
	case DA_FF:
		va = alloc_block_FF(size);
		break;
	case DA_NF:
		va = alloc_block_NF(size);
		break;
	case DA_BF:
		va = alloc_block_BF(size);
		break;
	case DA_WF:
		va = alloc_block_WF(size);
		break;
	default:
		cprintf("Invalid allocation strategy\n");
		break;
	}
	return va;
}

//===========================
// 4) PRINT BLOCKS LIST:
//===========================

void print_blocks_list(struct MemBlock_LIST list)
{
	cprintf("=========================================\n");
	struct BlockElement* blk ;
	cprintf("\nDynAlloc Blocks List:\n");
	LIST_FOREACH(blk, &list)
	{
		cprintf("(size: %d, isFree: %d)\n", get_block_size(blk), is_free_block(blk)) ;
	}
	cprintf("=========================================\n");

}
//
////********************************************************************************//
////********************************************************************************//

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

bool is_initialized = 0;
//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
void initialize_dynamic_allocator(uint32 daStart, uint32 initSizeOfAllocatedSpace)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (initSizeOfAllocatedSpace % 2 != 0) initSizeOfAllocatedSpace++; //ensure it's multiple of 2
		if (initSizeOfAllocatedSpace == 0) // we are who added || initSizeOfAllocatedSpace < 24 not doctor
			return;
		if(initSizeOfAllocatedSpace < 24){
			initSizeOfAllocatedSpace = 24;
		}
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'24.MS1 - #04] [3] DYNAMIC ALLOCATOR - initialize_dynamic_allocator
	// initSizeOfAllocatedSpace : Size of Begin block + End Block + total size of First block (with its metadata , 4bytes for header and 4 bytes for footer)
	LIST_INIT(&freeBlocksList); // size = 0

	// -->>>>>> MODIFIED IN MS2 : NOT SURE RIGHT OR NOT
	/*if (initSizeOfAllocatedSpace == 0) // we are who added || initSizeOfAllocatedSpace < 24 not doctor
		return;
	if(initSizeOfAllocatedSpace < 24){
		initSizeOfAllocatedSpace = 24;
	}*/
	//cprintf("Size of initalized block allocator kernel heap: %u \n",initSizeOfAllocatedSpace - 8); // without calculating begin block and end block , that's the actual size of the first free block
	// if you want the total size of the dynamic allocator (block allocator in kernel heap) don't subtract 8
	uint32 *BeginBlock = (uint32 *) daStart;
	*BeginBlock = 1; // 0| 1


	uint32 *EndBlock = (uint32 *) (daStart + initSizeOfAllocatedSpace - 4);
	*EndBlock = 1;

	uint32 *header = (uint32 *) (daStart + 4);
	*header = initSizeOfAllocatedSpace - 8;


	uint32 *footer = (uint32 *) (daStart + initSizeOfAllocatedSpace - 8);
	*footer = initSizeOfAllocatedSpace - 8;


	struct BlockElement* first_block = (struct BlockElement*)(daStart + 8);

	LIST_INSERT_HEAD(&freeBlocksList,first_block);
	return;


}
//==================================
// [2] SET BLOCK HEADER & FOOTER:
//==================================
void set_block_data(void* va, uint32 totalSize, bool isAllocated)
{
	//TODO: [PROJECT'24.MS1 - #05] [3] DYNAMIC ALLOCATOR - set_block_data

	// total size >>> size of block with metadata(header+footer)
	if(totalSize % 2 != 0 && isAllocated == 0){ // impossible but we will keep it
		totalSize = totalSize + 1;
	}
	if(totalSize < 16){ // we added it not doctor
		return;
	}

	//Set the header & footer of the block at the given va with the given info (totalSize & isAllocated)
	// header = va - 4    , footer = total size  - 8 (to be at the first byte at footer not the last byte)
	// Don't forget casting
	uint32 *header = (uint32 *) ((uint32)va - 4);
	*header = totalSize | isAllocated; // 4 | 0 = 4 (even --> free), 4 | 1 = 5 (odd --> allocated)

	uint32 *footer = (uint32 *) ((uint32)va + totalSize - 8);
	*footer = totalSize | isAllocated; //dereference , to write integer value at that address(byte) integer --> 4 bytes

}


//=========================================
// [3] ALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *alloc_block_FF(uint32 size)
{
	if(size == 0){
		return NULL;
	}
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (size % 2 != 0) size++;	//ensure that the size is even (to use LSB as allocation flag)
		if (size < DYN_ALLOC_MIN_BLOCK_SIZE)
			size = DYN_ALLOC_MIN_BLOCK_SIZE ;
		if (!is_initialized)
		{
			uint32 required_size = size + 2*sizeof(int) /*header & footer*/ + 2*sizeof(int) /*da begin & end*/ ;
			uint32 da_start = (uint32)sbrk(ROUNDUP(required_size, PAGE_SIZE)/PAGE_SIZE);
			uint32 da_break = (uint32)sbrk(0);
			initialize_dynamic_allocator(da_start, da_break - da_start);
		}
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'24.MS1 - #06] [3] DYNAMIC ALLOCATOR - alloc_block_FF
	// size : it's without header and footer (so you should add 8 bytes to it to represent total block size with its metadata)
	// loop through Free Block List and check we need size of block (header size >= size)
	//header size = header + footer + size (size of block + meta data ) (size of entire block)
	// Do we need to check on the size of list?
	// Do we need to check the boundaries of heap?

	//cprintf("I'm in alloc_block_FF function \n");
	//cprintf("I will allocate: %u bytes \n",size+8); // including its metadata (header and footer)
	uint32 isFound = 0; // flag

	uint32 totalBlockSize = size + 8 ;// Size of the block + metadata (4 bytes for header + 4 bytes for footer)

	struct BlockElement *element;

	LIST_FOREACH(element, &(freeBlocksList))
	{
		uint32 *header = (uint32 *)((uint32)element - 4);
		uint32 *footer = (uint32 *) ((uint32)element+(*header)-8); // element is a pointer that stores the address of the first byte of the block after header

		if(*header >= totalBlockSize){ // == , >
			isFound = 1;
			// we should split or internal framgentation
			if(*header - totalBlockSize < 16){ // header will be always greater than or equal to size
				// Don't split - it's padding , internal fragmentation
				// two steps
				//1. make header and footer bitwise with 1 to be allocated flag
				*header = (*header)|1; //header++;
				*footer = (*footer)|1; //footer++;
				//2. remove it from list
				LIST_REMOVE(&freeBlocksList,element);

			}else{
				//split it to another block
				// How?
				uint32 size_split_block = (*header) - totalBlockSize; // total size of splitted block and its metadata

				// we will shift the footer of this block to the right , and we will create new header and footer for the splitted block
				uint32 *footer_new = (uint32 *)footer;
				footer = (uint32 *)((uint32)element+size); //footer_old

				uint32 *header_new = (uint32 *)((uint32)footer+4);
				*footer = (size + 8) | 1; //allocated
				*header = (size + 8) | 1;



				*header_new = size_split_block | 0; // free - not allocated
				*footer_new = size_split_block | 0;

				struct BlockElement *splittedBlock = (struct BlockElement *)((uint32)header_new + 4); // address of first byte after header of splitted block

				LIST_INSERT_AFTER(&freeBlocksList, element, splittedBlock);

				LIST_REMOVE(&freeBlocksList,element);
				// -modifying the address and the value(the size) of header and footer (allocated block)
				// -Remove it from the list
				// -add header , footer , Block pointer to address
				// -add it to free list
				// use functions you created above

			}
			break;
		}

	}
	if(isFound == 0){
		//cprintf("I'm in dynamic allocator .. totalBlockSize BEFORE ROUNDING: %u \n",totalBlockSize);
		totalBlockSize = ROUNDUP(totalBlockSize,PAGE_SIZE);
		//cprintf("I'm in dynamic allocator .. totalBlockSize After ROUNDING: %u \n",totalBlockSize);
		uint32 noPages = totalBlockSize/PAGE_SIZE;
		//cprintf("I'm in dynamic allocator .. No of Pages: %u \n",noPages);

		//cprintf("BEFORE SBRK \n");
		void *addr = sbrk(noPages);
		//cprintf("AFTER SBRK \n");
		if((uint32) addr == -1){
			return NULL;
		}

		// Check the previous block is free or not
		// if free remove the address of newly free block (TEST CASE in free block which maybe previous block is free or allocated but i'm the end block so there is no block after me)
		// and set header and footer by sum of the two blocks size


		// add the new free block in free list
		// set its header and footer
		uint32 *header_newBlock = (uint32 *)((uint32)addr - 4); // first byte of previous END BLOCK

		struct BlockElement *newBlock = (struct BlockElement *) ((uint32)addr); // first byte after header

		// totalBlockSize which is (no of pages * page size) is the size of total Block with its meta data (Header + footer + payload) but without END BLOCK (4bytes) cuz we just expand,the 4 bytes of end block are already existed before sbrk we just shifting it to the end
		//uint32 *footer_newBlock = (uint32 *) ((uint32) header_newBlock + totalBlockSize);
		uint32 *footer_newBlock = (uint32 *) ((uint32) header_newBlock + totalBlockSize - 8);
		//cprintf("TOTALBLOCKSIZE: %u",totalBlockSize);
		//cprintf("noPages * PAGE_SIZE: %u",noPages * PAGE_SIZE);
		*header_newBlock = (noPages * PAGE_SIZE) | 1;
		*footer_newBlock = (noPages * PAGE_SIZE) | 1;
		//cprintf("HEADER SIZE: %u \n",*header_newBlock);
		//cprintf("FOOTER SIZE: %u \n",*footer_newBlock);
		//cprintf("HEADER ADDRESS: %p \n",header_newBlock);
		//cprintf("New Block ADDRESS: %p \n",newBlock);
		//cprintf("Footer ADDRESS: %p \n",footer_newBlock);
		//cprintf("Difference between Footer and Block data: %u",(uint32)footer_newBlock-((uint32)newBlock));
		//cprintf("BEFORE FREE FUNCTION \n");
		uint32 *END_BLOCK = (uint32 *)(((uint32)header_newBlock)+totalBlockSize);
		*END_BLOCK = 0 | 1;
		//cprintf("DIFFERENCE BETWEEN FOOTER AND END BLOCK %u \n:",(uint32)END_BLOCK - (uint32) footer_newBlock);
		free_block(newBlock); // trick to illusion that this block is allocated and I want to free that block, so i don't care if the previous block is free or alloctaed ,function will handle it
		//cprintf("After FREE FUNCTION \n");
		// move END_BLOCK
		//uint32 *END_BLOCK = (uint32 *) (((uint32)BRK) - 4);
		//uint32 *END_BLOCK = (uint32 *)(((uint32)addr)+(noPages*PAGE_SIZE)-4); /// CHECK ADDRESSES AGAIN

		// Allocate the required space he wanted
		//cprintf("BEFORE CALLING ALLOC BLOCK FF AGAIN IN RECURSION AFTER SBRK \n");
		return alloc_block_FF(size);
	}

	return element; // function must return address because it's return type is void *




}
//=========================================
// [4] ALLOCATE BLOCK BY BEST FIT:
//=========================================
void *alloc_block_BF(uint32 size)
{
	//TODO: [PROJECT'24.MS1 - BONUS] [3] DYNAMIC ALLOCATOR - alloc_block_BF
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("alloc_block_BF is not implemented yet");
	//Your Code is Here...

	if(size == 0){
		return NULL;
	}
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (size % 2 != 0) size++;	//ensure that the size is even (to use LSB as allocation flag)
		if (size < DYN_ALLOC_MIN_BLOCK_SIZE)
			size = DYN_ALLOC_MIN_BLOCK_SIZE ;
		if (!is_initialized)
		{
			uint32 required_size = size + 2*sizeof(int) /*header & footer*/ + 2*sizeof(int) /*da begin & end*/ ;
			uint32 da_start = (uint32)sbrk(ROUNDUP(required_size, PAGE_SIZE)/PAGE_SIZE);
			uint32 da_break = (uint32)sbrk(0);
			initialize_dynamic_allocator(da_start, da_break - da_start);
		}
	}
	//==================================================================================
	//==================================================================================
	//size : it's without header and footer (so you should add 8 bytes to it to represent total block size with its metadata)
	uint32 found_flag = 0;
	uint32 totalBlockSize = size + 8 ;

	uint32 minSizeAllocate;
	struct BlockElement *blockAddress;

	struct BlockElement *element;
	uint32 flag_inloop = 0;
	LIST_FOREACH(element, &(freeBlocksList))
	{
		uint32 *header = (uint32 *)((uint32)element - 4);
		uint32 *footer = (uint32 *) ((uint32)element+(*header)-8); // element is a pointer that stores the address of the first byte of the block after header

		//(*header) >= totalBlockSize;  differenceBytes >= 0
		if(flag_inloop == 0){
			if((*header) >= totalBlockSize){
				uint32 differenceBytes = (*header) - totalBlockSize;
				minSizeAllocate = differenceBytes;
				blockAddress = element;
				found_flag = 1;
				flag_inloop = 1;
				if(minSizeAllocate == 0){
					break;
				}
			}
		}else{
			if((*header) >= totalBlockSize){
				uint32 differenceBytes = (*header) - totalBlockSize;
				if(differenceBytes < minSizeAllocate && differenceBytes >= 0){
					minSizeAllocate = differenceBytes;
					blockAddress = element;
					found_flag = 1; // useless
					if(minSizeAllocate == 0){
						break;
					}
				}
			}
		}

	}

	if(found_flag == 0){
		totalBlockSize = ROUNDUP(totalBlockSize,PAGE_SIZE);
		uint32 noPages = totalBlockSize/PAGE_SIZE;
		void *addr = sbrk(noPages);
		if((uint32) addr == -1){
			return NULL;
		}
		uint32 *header_newBlock = (uint32 *)((uint32)addr - 4); // first byte of previous END BLOCK

		struct BlockElement *newBlock = (struct BlockElement *) ((uint32)addr); // first byte after header

		// totalBlockSize which is (no of pages * page size) is the size of total Block with its meta data (Header + footer + payload) but without END BLOCK (4bytes) cuz we just expand,the 4 bytes of end block are already existed before sbrk we just shifting it to the end
		uint32 *footer_newBlock = (uint32 *) ((uint32) header_newBlock + totalBlockSize - 8);

		*header_newBlock = (noPages * PAGE_SIZE) | 1;
		*footer_newBlock = (noPages * PAGE_SIZE) | 1;

		uint32 *END_BLOCK = (uint32 *)(((uint32)header_newBlock)+totalBlockSize);
		*END_BLOCK = 0 | 1;
		//cprintf("DIFFERENCE BETWEEN FOOTER AND END BLOCK %u \n:",(uint32)END_BLOCK - (uint32) footer_newBlock);
		free_block(newBlock); // trick to illusion that this block is allocated and I want to free that block, so i don't care if the previous block is free or alloctaed ,function will handle it
		// Allocate the required space he wanted
		return alloc_block_BF(size);
	}

	uint32 *header = (uint32 *) ((uint32)blockAddress - 4);
	uint32 *footer = (uint32 *) ((uint32)blockAddress + (*header) - 8);
	if(minSizeAllocate < 16){
		// Don't split - it's padding , internal fragmentation
		//1. make header and footer bitwise with 1 to be allocated flag
		*header = (*header)|1; //header++;
		*footer = (*footer)|1; //footer++;
		LIST_REMOVE(&freeBlocksList,blockAddress);
	}else{
		// Splitting
		uint32 size_split_block = minSizeAllocate; // total size of splitted block and its metadata
		// we will shift the footer of this block to the right , and we will create new header and footer for the splitted block
		uint32 *footer_new = (uint32 *)footer;
		footer = (uint32 *)((uint32)blockAddress+size); //footer_old
		uint32 *header_new = (uint32 *)((uint32)footer+4);
		*footer = (size + 8) | 1; //allocated
		*header = (size + 8) | 1;

		*header_new = size_split_block | 0; // free - not allocated
		*footer_new = size_split_block | 0;

		struct BlockElement *splittedBlock = (struct BlockElement *)((uint32)header_new + 4); // address of first byte after header of splitted block

		LIST_INSERT_AFTER(&freeBlocksList, blockAddress, splittedBlock);
		LIST_REMOVE(&freeBlocksList,blockAddress);
	}


	return blockAddress;


}

//===================================================
// [5] FREE BLOCK WITH COALESCING:
//===================================================
void free_block(void *va)
{
	//TODO: [PROJECT'24.MS1 - #07] [3] DYNAMIC ALLOCATOR - free_block
	if(va == NULL){
		return;
	}

	uint32 base = (uint32) va;
	struct BlockElement* allocated_block = (struct BlockElement*)(base);
	uint32 *header = (uint32 *) ((base) - 4);
	// HERE IS THE TRICK
	// THE ALLOCATED BLOCK IS ODD NUMBER JUST FOR INDICATION LSB = 1 it's allocated block
	// but the real (actual size) is header - 1 LSB=0 so you must calculate that way to each first byte in the footer correctly not the second byte(because of the 1 byte we added cuz it's odd number we add the real size by 1 just for indication that it is odd not actual number of bytes
	uint32 actualSize = ((*header)-1);
	uint32 *footer = (uint32 *) (base + (actualSize) - 8);

	if(*header % 2 == 0){
		return;
	}

	uint32 *footer_prev = (uint32 *)((base) - 8);
	uint32 *header_next = (uint32 *)((base) + (actualSize) - 4);

	if((*footer_prev %2 != 0 && *header_next % 2 !=0) || (*footer_prev == 1 && *header_next % 2 !=0) || (*footer_prev %2 != 0 && *header_next == 1)){
		*header = (*header)-1; // indication it will be free
		*footer = (*footer)-1;
		struct BlockElement *element;
		uint32 flag_added = 0;

		// If it's first block so it will be always the first address in free list
		if(*footer_prev == 1 && *header_next % 2 !=0){
			LIST_INSERT_HEAD(&freeBlocksList,allocated_block);
			return;
		}

		// if it's last block so it will be always the last address in free list
		if(*footer_prev %2 != 0 && *header_next == 1){
			LIST_INSERT_TAIL(&freeBlocksList,allocated_block);
			return;
		}


		LIST_FOREACH(element, &(freeBlocksList))
		{
			if((uint32)element > (uint32)allocated_block){
				flag_added = 1;
				LIST_INSERT_BEFORE(&freeBlocksList, element, allocated_block);
				break;
			}
		}
		if(flag_added == 0){
			LIST_INSERT_TAIL(&freeBlocksList,allocated_block);
		}
	}
	else if((*footer_prev % 2 == 0 && *header_next % 2 !=0) || (*footer_prev % 2 == 0 && *header_next % 2 == 1)){
		*header = (*header)-1;
		*footer = (*footer)-1;
		uint32 *header_prev = (uint32 *)((uint32)va - 4 - (*footer_prev));
		*header_prev = (*header) + (*footer_prev);
		*footer = (*header_prev);
	}else if((*footer_prev % 2 != 0 && *header_next % 2 ==0) || (*footer_prev == 1 && *header_next % 2 == 0)){
		*header = (*header)-1;
		*footer = (*footer)-1;
		*header = (*header) + (*header_next);
		uint32 *footer_next = (uint32 *) (((uint32)header_next) + (*header_next) -4) ;
		*footer_next = (*header);
		struct BlockElement *mergedBlock = (struct BlockElement*) ((uint32)header+4);
		struct BlockElement *already_freeBlock = (struct BlockElement*) ((uint32)header_next+4);
		//1. add header of merged block before header of free block
		LIST_INSERT_BEFORE(&freeBlocksList, already_freeBlock, mergedBlock);

		LIST_REMOVE(&freeBlocksList,already_freeBlock);
	}
	else{ // *footer_prev % 2 == 0 && *header_next % 2 == 0
		*header = (*header)-1;
		*footer = (*footer)-1;
		uint32 *header_prev = (uint32 *)((uint32)va - 4 - (*footer_prev)); // imp
		*header_prev = (*header_prev) + (*header) + (*header_next);
		uint32 *footer_next = (uint32 *) (((uint32)header_next) + (*header_next) -4) ; // imp
		*footer_next = (*header_prev);

		struct BlockElement *mergedBlock = (struct BlockElement *)((uint32)header_prev+4);

		struct BlockElement *removed_block = (struct BlockElement *)((uint32)header_next+4);

		LIST_REMOVE(&freeBlocksList,removed_block);

	}
}

//=========================================
// [6] REALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *realloc_block_FF(void* va, uint32 new_size)
{
	//TODO: [PROJECT'24.MS1 - #08] [3] DYNAMIC ALLOCATOR - realloc_block_FF
	//realloc_block_FF(va, 0) is equivalent to calling free(va) and returning NULL.
	//realloc_block_FF(NULL, n) is equivalent to calling alloc_FF(n) and return the allocated address.
	//realloc_block_FF(NULL, 0) is equivalent to calling alloc_FF(0), which should just return NULL.
	//new_size : it's without header and footer (so you should add 8 bytes to it to represent total block size with its metadata)
	if(new_size == 0 && va != NULL){
		free_block(va);
		return NULL;
	}else if(va == NULL && new_size != 0){
		return alloc_block_FF(new_size);
	}else if(va == NULL && new_size == 0){
		return alloc_block_FF(0);
	}

	if(new_size % 2 == 1){
		new_size= new_size+1;
	}



	struct BlockElement* allocated_block = (struct BlockElement*)((uint32)va); // already allocated
	uint32 *header = (uint32 *) ((uint32)allocated_block-4);

	if(*header % 2 == 0){ // it's already free block not allocated block , and we are sure there is no free block after it cuz it would be already merged
		uint32 *footer = (uint32 *) ((uint32)allocated_block + (*header) - 8);
		uint32 requiredBlockTotalSize = new_size + 8;
		if(*header == requiredBlockTotalSize){
			*header = (*header) | 1;
			*footer = (*footer) | 1;
			LIST_REMOVE(&freeBlocksList,allocated_block);
			return allocated_block;
		}else if(*header < requiredBlockTotalSize){ // we are sure there is no free block after it
			return alloc_block_FF(new_size);
		}else{ // *header > requiredBlockTotalSize
			// only 2 cases padding , splitted
			uint32 differenceBytes = (*header) - requiredBlockTotalSize; // even - even = even
			if(differenceBytes < 16){
				// Padding , Internal Fragmentation
				*header = (*header) | 1;
				*footer = (*footer) | 1;
				LIST_REMOVE(&freeBlocksList,allocated_block);
				return allocated_block;
			}else{
				// Splitted Block
				uint32 *footer_new = (uint32 *)footer; // for splitted block of padding (internal fragemntation)
				footer = (uint32 *)((uint32)allocated_block+new_size); //footer_old, new_size : without header and footer
				uint32 *header_new = (uint32 *)((uint32)footer+4); // allocated block
				*footer = (new_size + 8) | 1; //allocated
				*header = (new_size + 8) | 1;

				*header_new = differenceBytes | 1; // splitted block becomes allocated because we will call freeff() but it shouldn't be
				*footer_new = differenceBytes | 1;
				struct BlockElement *splittedBlock = (struct BlockElement *)((uint32)header_new + 4); // address of first byte after header of splitted block
				LIST_REMOVE(&freeBlocksList,allocated_block);
				free_block(splittedBlock);
				return allocated_block;
			}
		}
	}

	// Don't forget it should be allocated block so when you calculate the size decrease it by one
	uint32 actualSize = (*header)-1; // i'm sure it's allocated tlama wsl lhna
	uint32 *footer = (uint32 *) ((uint32)allocated_block + actualSize - 8);

	uint32 requiredBlockTotalSize = new_size + 8;
	// Always check about you are in between Begin Block and End block (HEAP Limits)
	if(requiredBlockTotalSize == actualSize){
		return allocated_block;
	}else if(requiredBlockTotalSize < actualSize){
		uint32 differenceBytes = actualSize - requiredBlockTotalSize;
		if(differenceBytes < 16){
			// don't split - It's padding (internal fragmentation)
			// what about if the next block is free or allocated?
			uint32 *header_next = (uint32 *) ((uint32)va + requiredBlockTotalSize -4);
			if((*header_next != 1) && (*header_next % 2 == 0)){
				uint32 *footer_new = (uint32 *)footer; // for splitted block of padding (internal fragemntation)
				footer = (uint32 *)((uint32)allocated_block+new_size); //footer_old, new_size : without header and footer
				uint32 *header_new = (uint32 *)((uint32)footer+4); // allocated block

				*footer = (new_size + 8) | 1; //allocated
				*header = (new_size + 8) | 1;

				*header_new = differenceBytes | 1; // splitted block becomes allocated because we will call freeff() but it shouldn't be
				*footer_new = differenceBytes | 1;
				struct BlockElement *splittedBlock = (struct BlockElement *)((uint32)header_new + 4); // address of first byte after header of splitted block
				free_block(splittedBlock);
		    }
			return allocated_block;
		}else{
			// Splitted Block , difference bytes >= 16bytes
			uint32 *footer_new = (uint32 *)footer; // for splitted block
			footer = (uint32 *)((uint32)allocated_block+new_size); //footer_old, new_size : without header and footer
			uint32 *header_new = (uint32 *)((uint32)footer+4); // allocated block
			*footer = (new_size + 8) | 1; //allocated
			*header = (new_size + 8) | 1;

			*header_new = differenceBytes | 1; // splitted block becomes allocated because we will call freeff() but it shouldn't be
			*footer_new = differenceBytes | 1;

			//print_blocks_list(freeBlocksList);
			struct BlockElement *splittedBlock = (struct BlockElement *)((uint32)header_new + 4); // address of first byte after header of splitted block
			free_block(splittedBlock);
			//print_blocks_list(freeBlocksList);
			return allocated_block;

		}
	}else{ // requiredBlockTotalSize > actualSize


		uint32 free_after_sufficient = 0;

		uint32 *header_next = (uint32 *) ((uint32)va + actualSize - 4); // Free Block BEFORE SPLITTING
		uint32 *footer_next;
		if(*header_next %2 == 0){
			footer_next = (uint32 *) ((uint32)header_next + (*header_next) - 4 ); // Free Block BEFORE SPLITTING
		}else{
			footer_next = (uint32 *) ((uint32)header_next + (*header_next) - 5 ); // Free Block BEFORE SPLITTING
		}

		struct BlockElement *WholeFreeBlock = (struct BlockElement *) ((uint32)header_next + 4);
		if((*header_next != 1) && (*header_next % 2 == 0)){
			uint32 totalTwoBlocksSize = actualSize + (*header_next);
			if(totalTwoBlocksSize >= requiredBlockTotalSize){
				free_after_sufficient = 1;
				uint32 differenceBytes = totalTwoBlocksSize -requiredBlockTotalSize; // <-------
				if(differenceBytes < 16){
					// PADDING - Internal Fragmentation
					struct BlockElement* free_block = (struct BlockElement*) ((uint32)header_next + 4);
					*header = (*header)+(*header_next); // <------- ODD/EVEN , footer = odd , footer_next = even , even + odd = odd
					footer = footer_next;
					*footer = (*header);

					LIST_REMOVE(&freeBlocksList,free_block);
					return allocated_block;
				}else{
					// Splitted Block
					uint32 *footer_split = (uint32 *)footer_next;
					footer_next = (uint32 *)((uint32)allocated_block+new_size); //footer_old, new_size : without header and footer
					footer = (uint32 *)footer_next;

					uint32 *header_split = (uint32 *)((uint32)footer+4);

					// LSB , ODD

					*footer = (new_size + 8) | 1; //allocated
					*header = (new_size + 8) | 1;

					*header_split = differenceBytes | 0; // splitted block becomes allocated because we will call freeff() but it shouldn't be
					*footer_split = differenceBytes | 0;

					struct BlockElement *splittedBlock = (struct BlockElement *) ((uint32)header_split + 4);
					LIST_INSERT_AFTER(&freeBlocksList,WholeFreeBlock,splittedBlock);
					LIST_REMOVE(&freeBlocksList,WholeFreeBlock);
					return allocated_block;


				}
			}
		}
		if(free_after_sufficient == 0){ // either if it's the last block or if the next block is allocated or if the next is free block but the totalSizeofTwoBlocks is not sufficient
			struct BlockElement* block_addr = alloc_block_FF(new_size);
			if(block_addr != NULL){
				uint32 noCopiedBytes = (*header) - 9; // cuz it's allocated we need payload size without footer and header
				memcpy((void *)block_addr,(void *)allocated_block,noCopiedBytes); // size : block without (metadata) , destination,source : address of first byte after header
				/*for(uint32 i = 0; i < noCopiedBytes;i++){
					*((struct BlockElement *)((uint32) block_addr + i)) = *((struct BlockElement *)((uint32) allocated_block + i)); // to copy the content of the data byte by byte
				}*/
				free_block(allocated_block);
			}
			return block_addr; // new size without header and footer , inside alloc we add 8 bytes for header and footer

		}


	}
	return NULL;
}

/*********************************************************************************************/
/*********************************************************************************************/
/*********************************************************************************************/
//=========================================
// [7] ALLOCATE BLOCK BY WORST FIT:
//=========================================
void *alloc_block_WF(uint32 size)
{
	panic("alloc_block_WF is not implemented yet");
	return NULL;
}

//=========================================
// [8] ALLOCATE BLOCK BY NEXT FIT:
//=========================================
void *alloc_block_NF(uint32 size)
{
	panic("alloc_block_NF is not implemented yet");
	return NULL;
}
