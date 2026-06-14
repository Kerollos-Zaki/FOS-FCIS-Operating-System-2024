#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
struct Share* get_share(int32 ownerID, char* name);

//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list) ;
	init_spinlock(&AllShares.shareslock, "shares lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [2] Get Size of Share Object:
//==============================
int getSizeOfSharedObject(int32 ownerID, char* shareName)
{
	//[PROJECT'24.MS2] DONE
	// This function should return the size of the given shared object
	// RETURN:
	//	a) If found, return size of shared object
	//	b) Else, return E_SHARED_MEM_NOT_EXISTS
	//
	struct Share* ptr_share = get_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;

	return 0;
}

//===========================================================


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===========================
// [1] Create frames_storage:
//===========================
// Create the frames_storage and initialize it by 0
inline struct FrameInfo** create_frames_storage(int numOfFrames)
{
	//TODO: [PROJECT'24.MS2 - #16] [4] SHARED MEMORY - create_frames_storage()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("create_frames_storage is not implemented yet");
	//Your Code is Here...
	struct FrameInfo** framesStorage = (struct FrameInfo**)kmalloc(numOfFrames * sizeof(struct FrameInfo*));
	if (framesStorage == NULL) { // no storage

		return NULL;
	}
	memset((void *) framesStorage, 0, numOfFrames * sizeof(struct FrameInfo*)); // == numOfFrames * 4 bytes , cuz the size of address is 4 bytes

	return framesStorage;



}

//=====================================
// [2] Alloc & Initialize Share Object:
//=====================================
//Allocates a new shared object and initialize its member
//It dynamically creates the "framesStorage"
//Return: allocatedObject (pointer to struct Share) passed by reference
struct Share* create_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable)
{
    //TODO: [PROJECT'24.MS2 - #16] [4] SHARED MEMORY - create_share()
    //COMMENT THE FOLLOWING LINE BEFORE START CODING
    //panic("create_share is not implemented yet");
    //Your Code is Here...
#if USE_KHEAP
    struct Share* newShareobject = (struct Share*)kmalloc(sizeof(struct Share));
    if (newShareobject == NULL)
	{
		return NULL;
	}
    //&~(1<<32)
    newShareobject->ID = ((uint32)newShareobject & (~(1<<31)));
    newShareobject->ownerID = ownerID;

    newShareobject->size = size;
    strcpy(newShareobject->name,shareName);//name
    newShareobject->isWritable = isWritable;
    newShareobject->references = 1;

    int numOfFrames = ROUNDUP(newShareobject->size, PAGE_SIZE) / PAGE_SIZE;
    newShareobject->framesStorage = create_frames_storage(numOfFrames);
    if (newShareobject->framesStorage == NULL)
    {
        kfree(newShareobject);
        return NULL;
    }
    return newShareobject;
#else
	panic("Shared memory not supported without kernel heap!");
#endif

}

//=============================
// [3] Search for Share Object:
//=============================
//Search for the given shared object in the "shares_list"
//Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share* get_share(int32 ownerID, char* name)
{
	//TODO: [PROJECT'24.MS2 - #17] [4] SHARED MEMORY - get_share()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("get_share is not implemented yet");
	//Your Code is Here...
#if USE_KHEAP

	acquire_spinlock(&AllShares.shareslock);
	struct Share* searchshare;
	LIST_FOREACH(searchshare, &(AllShares.shares_list))
	{
		if (searchshare->ownerID == ownerID && strcmp(searchshare->name, name) == 0) {
			release_spinlock(&AllShares.shareslock);
			return searchshare;
		}
	}

	release_spinlock(&AllShares.shareslock);
	return NULL;
#endif

}

//=========================
// [4] Create Share Object:
//=========================
int createSharedObject(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #19] [4] SHARED MEMORY [KERNEL SIDE] - createSharedObject()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("createSharedObject is not implemented yet");
	//Your Code is Here...
	//cprintf("I'm in createSharedObject fn \n");
	struct Env* myenv = get_cpu_proc(); //The calling environment

	//cprintf("Is Writable attribute in CreateSharedObject: %u \n",isWritable);

	if (get_share(ownerID, shareName) != NULL)
		return E_SHARED_MEM_EXISTS;

	struct Share* newShare = create_share(ownerID, shareName, size, isWritable);
	if (newShare == NULL)
		return E_NO_SHARE;

	acquire_spinlock(&AllShares.shareslock);
	LIST_INSERT_HEAD(&AllShares.shares_list, newShare);
	release_spinlock(&AllShares.shareslock);
	//========================================================
	//cprintf("Share name: %s \n",shareName);
	//cprintf("Share name: %s \n",shareName);


	uint32 *page_ptr = (uint32 *) virtual_address;
	uint32 noOfPages = size / PAGE_SIZE;

	if (MemFrameLists.free_frame_list.size < noOfPages) { // Not enough frames in Physical memory
		return E_NO_SHARE;
	}

	//acquire_spinlock(&(MemFrameLists.mfllock)); // already inside allocate frame
	uint32 page_no = 1; // first page

	while (page_no <= noOfPages) { // condition is true, any no is true except zero

		//cprintf("No of frames before creating page table and allocating frame in smalloc: %u \n",MemFrameLists.free_frame_list.size);
		uint32 *ptr_page_table;
		get_page_table(myenv->env_page_directory, (uint32)page_ptr,&ptr_page_table);
		if(ptr_page_table == NULL){
			create_page_table(myenv->env_page_directory, (uint32)page_ptr);
		}

		struct FrameInfo *ptr_frame_info;
		int ret = allocate_frame(&ptr_frame_info);

		//cprintf("No of frames after allocating frame in smalloc: %u \n",MemFrameLists.free_frame_list.size);

		if (ret == E_NO_MEM) {
			//release_spinlock(&(MemFrameLists.mfllock));
			//return E_NO_MEM;
			return E_NO_SHARE;
		}
		//cprintf("myenv->env_page_directory: %u \n",(uint32)myenv->env_page_directory);
		//cprintf("In smalloc, va: %u \n",(uint32)page_ptr);
		//cprintf("In smalloc, address of frame: %u \n",(uint32)ptr_frame_info);
		// Permission Bits
		// There are available 3 bits for programmer use their number is 9,10,11 (zero based)

		//int32 permissions = pt_get_page_permissions(myenv->env_page_directory,(uint32)page_ptr);
		//cprintf("Permissions Before SMALLOC BEFORE MAPPING: %u \n",permissions);


		ret = map_frame(myenv->env_page_directory, ptr_frame_info,(uint32) page_ptr,(1 << 11) | PERM_PRESENT | PERM_WRITEABLE | PERM_USER);

		if (ret == E_NO_MEM) {
			//release_spinlock(&(MemFrameLists.mfllock));
			//return E_NO_MEM;
			return E_NO_SHARE;
		}

		//permissions = pt_get_page_permissions(myenv->env_page_directory,(uint32)page_ptr);
		//cprintf("Permissions Before SMALLOC AFTER MAPPING: %u \n",permissions);

		newShare->framesStorage[page_no - 1] = ptr_frame_info;
		tlb_invalidate(myenv->env_page_directory, (void *) page_ptr);
		page_ptr = (uint32 *) ((uint32) page_ptr + PAGE_SIZE);
		page_no++;

	}
	//release_spinlock(&(MemFrameLists.mfllock));


	//=========================================================

	return newShare->ID;
}


//======================
// [5] Get Share Object:
//======================
int getSharedObject(int32 ownerID, char* shareName, void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #21] [4] SHARED MEMORY [KERNEL SIDE] - getSharedObject()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("getSharedObject is not implemented yet");
	//Your Code is Here...

	struct Env* myenv = get_cpu_proc(); //The calling environment

	struct Share* share_object = get_share(ownerID,shareName);
	if(share_object == NULL){
		return E_SHARED_MEM_NOT_EXISTS;
	}
	uint32 shared_object_size = share_object->size; // represents no of shared frames , so that number is the number of pages we need to map

	uint32 *page_ptr = (uint32 *) virtual_address;
	uint32 noOfPages = shared_object_size / PAGE_SIZE;
	//cprintf("Size of shared memory: %u \n",shared_object_size);
	//cprintf("No of shared Frames: %u \n",noOfPages);
	//cprintf("virtual address we mapped to shared frame: %u \n",(uint32)virtual_address);
	uint32 page_no = 1; // first page
	while (page_no <= noOfPages) {
		uint32 *ptr_page_table;
		get_page_table(myenv->env_page_directory, (uint32)page_ptr,&ptr_page_table);
		if(ptr_page_table == NULL){
			create_page_table(myenv->env_page_directory, (uint32)page_ptr);
		}


		struct FrameInfo *ptr_frame_info = share_object->framesStorage[page_no - 1];


		//cprintf("Permissions Before: %d \n",pt_get_page_permissions(myenv->env_page_directory,(uint32)page_ptr)); // ghalt 3shan share_object->isWritable y2ma zero or one bs 3shan ndy PERM_WRITABLE bykon decimal bta3ha 2 fa lazm n3ml ORING m3a 2 msh 1 lw hya m3aha permissions write
		//int ret = map_frame(myenv->env_page_directory, ptr_frame_info,(uint32) page_ptr,(1 << 11) | PERM_PRESENT | share_object->isWritable | PERM_USER); // lazm ndelh 1 << 11 3shan n3ml indication en page deh allocated 3shan mynf3sh malloc aw smalloc yhgzha
		if(share_object->isWritable == 1){
			int ret = map_frame(myenv->env_page_directory, ptr_frame_info,(uint32) page_ptr,(1 << 11) | PERM_PRESENT | PERM_WRITEABLE | PERM_USER); // lazm ndelh 1 << 11 3shan n3ml indication en page deh allocated 3shan mynf3sh malloc aw smalloc yhgzha
		}else{
			int ret = map_frame(myenv->env_page_directory, ptr_frame_info,(uint32) page_ptr,(1 << 11) | PERM_PRESENT | PERM_USER); // lazm ndelh 1 << 11 3shan n3ml indication en page deh allocated 3shan mynf3sh malloc aw smalloc yhgzha
		}
		//cprintf("No of references of FRAME SHOULD BE MORE THAN 1 if it's correctly mapped: %u \n",ptr_frame_info->references);
		//cprintf("Permissions After mapping: %d \n",pt_get_page_permissions(myenv->env_page_directory,(uint32)page_ptr));
		//cprintf("Address of shared Frame: %u \n",(uint32)ptr_frame_info);
		//cprintf("Is Writable: %u \n",share_object->isWritable);

		/*if (ret != 0) {
			return E_SHARED_MEM_NOT_EXISTS; // not sure
		}*/

		page_ptr = (uint32 *) ((uint32) page_ptr + PAGE_SIZE);
		page_no++;
	}


	acquire_spinlock(&AllShares.shareslock);
	share_object->references = share_object->references + 1;
	release_spinlock(&AllShares.shareslock);
	return share_object->ID;
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//==========================
// [B1] Delete Share Object:
//==========================
//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself
void free_share(struct Share* ptrShare)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [KERNEL SIDE] - free_share()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("free_share is not implemented yet");
	//Your Code is Here...
	if (ptrShare == NULL) {
		return;
	}

	acquire_spinlock(&AllShares.shareslock);
	LIST_REMOVE(&(AllShares.shares_list), ptrShare);
	release_spinlock(&AllShares.shareslock);

	if (ptrShare->framesStorage != NULL) {
		//cprintf("I'm HERE in free frame storage \n");
		/*uint32 no_pages_allocated = ptrShare->size / PAGE_SIZE;
		for(int i = 0; i < no_pages_allocated;i++){
			kfree(ptrShare->framesStorage[i]);
		}*/
		//cprintf("Array of pointers: %u \n",(uint32)ptrShare->framesStorage);
		//cprintf("Pointer[0] in array of pointers: %u \n",(uint32)ptrShare->framesStorage[0]); // address of the frame not the page cuz [] it dereferences the address
		kfree(ptrShare->framesStorage); // don't forget that array stored in block allocator kernel heap cuz it's 1*4 byte = 4 bytes, so it will call free block

	}
	//cprintf("Frame storage after kfree: %u \n",(uint32)ptrShare->framesStorage);
	ptrShare->framesStorage = NULL;

	kfree(ptrShare);
	return;

}
//========================
// [B2] Free Share Object:
//========================
int freeSharedObject(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [KERNEL SIDE] - freeSharedObject()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("freeSharedObject is not implemented yet");
	//Your Code is Here...

	// free pages  >> permissions
	// unmap
	struct Env* myenv = get_cpu_proc(); //The calling environment

	struct Share* share_object;
	//cprintf("I'm in freeSharedObject fn system call \n");

	acquire_spinlock(&AllShares.shareslock);
	struct Share *element;
	LIST_FOREACH(element, &(AllShares.shares_list))
	{
		if(element->ID == sharedObjectID){
			share_object = element;
			break;
		}
	}
	release_spinlock(&AllShares.shareslock);

	if(share_object == NULL){
		return E_SHARED_MEM_NOT_EXISTS;
	}

	//cprintf("HEREEEEEEEEEEEEEEEEEE \n");

	uint32 shared_object_size = share_object->size; // represents no of shared frames , so that number is the number of pages we need to map

	uint32 *page_ptr = (uint32 *) startVA;
	uint32 noOfPages = shared_object_size / PAGE_SIZE;
	//cprintf("in free shared object , size of shared frame: %u \n",noOfPages);
	uint32 page_no = 1; // first page
	while (page_no <= noOfPages) {
		pt_set_page_permissions(myenv->env_page_directory,(uint32)page_ptr,0,PERM_AVAILABLE);
		//cprintf("No of frames before unmapping in free: %u \n",MemFrameLists.free_frame_list.size);

		unmap_frame(myenv->env_page_directory,(uint32)page_ptr);

		//cprintf("No of frames after unmapping in free: %u \n",MemFrameLists.free_frame_list.size);
		// check page table is empty
		uint32 *ptr_page_table;
		get_page_table(myenv->env_page_directory,(uint32) page_ptr,&ptr_page_table);


		// to check page table is empty
		if(ptr_page_table != NULL){
			uint8 isFound = 0;
			for(uint32 i = 0; i < 1024;i++){
				//cprintf("ptr_page_table[i]: %u \n",(uint32)ptr_page_table[i]);
				if(ptr_page_table[i] != 0){ // why the value is integer, cuz uint32 *ptr_page_table; it's address of type uint32, meaning it will derefernce 4 bytes and it's uint32 * pointer so it's value (after dereference is int), and [] it's dereferencing after adding on the base address the index * sizeof(int) datatype of pointer Remember in SP course pointers, && ptr_page_table[i] != (uint32)NULL >> is wrong
					isFound = 1;
				}
			}

			if(isFound == 0){
				//cprintf("Freeing Page table cuz it's empty \n");
				kfree(ptr_page_table);
				pd_clear_page_dir_entry(myenv->env_page_directory,(uint32) page_ptr);
				tlbflush();
			}
		}


		page_ptr = (uint32 *) ((uint32) page_ptr + PAGE_SIZE);
		page_no++;
	}
	acquire_spinlock(&AllShares.shareslock);
	share_object->references = share_object->references - 1;
	release_spinlock(&AllShares.shareslock);
	//cprintf("No of references of share object after decrementing it: %u \n",share_object->references);
	if(share_object->references == 0){
		free_share(share_object);
	}


	return 0;





}
