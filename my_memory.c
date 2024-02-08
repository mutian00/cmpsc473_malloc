// Include files
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#define  N_OBJS_PER_SLAB  64

// Functional prototypes
void setup( int malloc_type, int mem_size, void* start_of_memory );
void *my_malloc( int size );
void my_free( void *ptr );
void *buddy_alloc( int size );

//////////////////////////////////////////////////////////////////////////////////////////
//
// Data Structures Section
//
//////////////////////////////////////////////////////////////////////////////////////////

// Basic data struct for slab allocator
typedef struct data data;
struct data {
    void *start; // needs to be void pointer
    int size;
    uint64_t bitmask;
};

// Basic node struct for buddy alloc
typedef struct node node;
struct node {
    data *data;
    node *next;
};

// Slab table entry struct as described in class
typedef struct slab_table_entry slab_table_entry;
struct slab_table_entry {
    int type; // this is size of one obj within slab
    int size; // size of entire slab
    int num_objs; // number of objs in slab, set to 64 based on project guidelines
    int objs_used; // number of objs currently allocated
    data *slab_data; // linked list of slabs of this entry type
};

// Basic node, but for the slab queue
typedef struct slabNode slabNode;
struct slabNode {
    slab_table_entry *data;
    slabNode *next;
};

// Basic queue struct
typedef struct queue queue;
struct queue {
    int size;
    node *head;
    node *end;
};

// Queue, but for slab alloc
typedef struct slabQ slabQ;
struct slabQ {
    int size;
    slabNode *head;
    slabNode *end;
};

//////////////////////////////////////////////////////////////////////////////////////////
//
// Global Variables
//
//////////////////////////////////////////////////////////////////////////////////////////

queue** availableList; // array of pointers to linked lists for avaliable holes
queue *allocatedQ; // linked list of allocated mem
slabQ *slabQueue;
int memType;
void *memStart;
int memSize;
int slab_size_counter;
int available_arr_size;

//////////////////////////////////////////////////////////////////////////////////////////
//
// Helper Functions
//
//////////////////////////////////////////////////////////////////////////////////////////

// Priority enqueue function based on the start address of the pointer given
int enqueue(queue *qToQ, data *dataToQ) {
    node *tempNode1, *tempNode2, *newNode;
    int size = qToQ->size;
    newNode = malloc(sizeof(node));
    newNode->data = dataToQ;
    newNode->next = NULL;
    tempNode1 = qToQ->head;

    // Check if the size is greater than 1
    if (qToQ->size > 1) {
        tempNode2 = tempNode1->next;

        // This checks the case that the new node should become the head
        if (tempNode1->data->start > newNode->data->start) {
            newNode->next = qToQ -> head;
            qToQ->head = newNode;
        }
        else {
            for (int iterator = 0; iterator < size - 1; iterator++) {
                // The new node should be somewhere in the middle
                if (tempNode2->data->start > newNode->data->start) {
                    tempNode1->next = newNode;
                    newNode->next = tempNode2;
                    break;
                }
                tempNode2 = tempNode2->next;
                tempNode1 = tempNode1->next;
                
            }
            // The new node should be at the very end
            if (tempNode1->data->start <= newNode->data->start) {
                qToQ->end = newNode;
                tempNode1->next = newNode;
            }
        }
    }
    else {
        // If size is only 1
        if (qToQ -> size == 1) {
            // Here new node should be the head.
            if (tempNode1->data->start > newNode->data->start) {
                newNode->next = qToQ->head;
                qToQ->head = newNode;
            }
            else {
                // Here new node should be the end.
                qToQ->head->next = newNode;
                qToQ->end = newNode;
            }
        }
        else {
            // New node is the only node.
            qToQ->head = newNode;
            qToQ->end = newNode;
        }
    }
    qToQ -> size++;
    return(0);
}

// Same priority enqueue function, but just for the slab data structure - still based on the start address
int slabEnqueue(slabQ *qToQ, slab_table_entry *dataToQ) {
    slabNode *tempNode1, *tempNode2, *newNode;
    int size = qToQ -> size;
    newNode = malloc(sizeof(slabNode));
    newNode->data = dataToQ;
    newNode->next = NULL;
    tempNode1 = qToQ->head;

    // Check if the size is greater than 1
    if (qToQ->size > 1) {
        tempNode2 = tempNode1->next;

        // This checks the case that the new node should become the head
        if (tempNode1->data->slab_data->start > newNode->data->slab_data->start) {
            newNode->next = qToQ->head;
            qToQ->head = newNode;
        }
        else {
            for (int iterator = 0; iterator < size - 1; iterator++) {
                // The new node should be somewhere in the middle
                if (tempNode2->data->slab_data->start > newNode->data->slab_data->start) {
                    tempNode1->next = newNode;
                    newNode->next = tempNode2;
                    break;
                }
                tempNode2 = tempNode2 -> next;
                tempNode1 = tempNode1 -> next;
                
            }
            // The new node should be at the very end
            if (tempNode1->data->slab_data->start <= newNode->data->slab_data->start) {
                qToQ->end = newNode;
                tempNode1->next = newNode;
            }
        }
    }
    else {
        // If size is only 1
        if (qToQ->size == 1) {
            // Here new node should be the head.
            if (tempNode1->data -> slab_data->start > newNode->data->slab_data->start) {
                newNode->next = qToQ->head;
                qToQ->head = newNode;
            }
            else {
                // Here new node should be the end.
                qToQ->head->next = newNode;
                qToQ->end = newNode;
            }
        }
        else {
            // New node is the only node.
            qToQ->head = newNode;
            qToQ->end = newNode;
        }
    }
    qToQ->size++;
    return(0);
}

// Simple and general dequeue function to get rid of the head of the input queue and decrease queue size.
void *dequeue(queue *qToDQ) {
    data *returnData = qToDQ->head->data;
    if (qToDQ->size > 1) {
        node *temp = qToDQ->head->next;
        qToDQ->head = temp;
    }
    else {
        qToDQ->head = NULL;
        qToDQ->end = NULL;
    }

    // Probably want a free
    qToDQ->size--;
    return;
}

// Simple and general dequeue function to get rid of the head of the input queue and decrease queue size.
void *dequeueSlab(slabQ *qToDQ) {
    if (qToDQ->size > 1) {
        slabNode *temp = qToDQ->head->next;
        qToDQ->head = temp;
    }
    else {
        qToDQ->head = NULL;
        qToDQ->end = NULL;
    }

    slabQueue->size--;
    return;
}

// Same as rip function below, simply made for the slab data structures
int slabRip(void *addr, slabQ *qToParse) {
    int size = qToParse->size;
    // If head is tid to remove, remove it.
    if(qToParse->head->data->slab_data->start + 4 == addr) {
        dequeueSlab(qToParse);
        return(0);
    }

    // If not the head and no other nodes, nothing to do here.
    if(qToParse->size <= 1)
        return(0);

    // Parse all the nodes, searching for the proper tid.
    slabNode *finderNode = qToParse->head->next, *prevNode = qToParse->head;
    for(int iterator = 0; iterator < size - 1; iterator++) {
        // If we find the tid, then we can rip it properly and decrease the size.
        if (finderNode->data->slab_data->start + 4 == addr) {
            prevNode->next = finderNode->next;
            qToParse->size--;
            break;
        }
        finderNode = finderNode->next;
        prevNode = prevNode->next;
    }
    free(finderNode);
    return(0);
}

// Basic rip function to find and remove a specific item from the main queue data structure properly
int rip(void *addr, queue *qToParse) {
    int size = qToParse -> size;
    // If head is tid to remove, remove it.
    if(qToParse->head->data->start == addr) {
        dequeue(qToParse);
        return(0);
    }

    // If not the head and no other nodes, nothing to do here.
    if(qToParse->size <= 1)
        return(0);

    // Parse all the nodes, searching for the proper tid.
    node *finderNode = qToParse->head->next, *prevNode = qToParse->head;
    for(int iterator = 0; iterator < size - 1; iterator++) {
        // If we find the tid, then we can rip it properly and decrease the size.
        if (finderNode->data -> start == addr) {
            prevNode->next = finderNode->next;
            qToParse->size--;
            break;
        }
        finderNode = finderNode->next;
        prevNode = prevNode->next;
    }
    // Probably want a free
    return(0);
}

// Finds the size we will want to request from buddy as a power of two
int findSize(int memsize) {
    int counter = 1;
    int temp = 1024;
    // iterate all possible powers of two - see which is lowest, but valid
    while(temp < memsize) {
        counter++;
        temp = temp*2;
    }
    return(counter);
}

// Checks if all tables of a given input size are full
int tablesAreFull( int size ) {
    // checks if ALL type = size slab tables are full for the given size
    if (slabQueue->size <= 0 ) {
        return(1);
    }
    slabNode *tempNode = slabQueue->head;

    // Iterate through the whole queue, if any of the types line up with our input, check if it is not full - if not:
    for(int slabIterator = 0; slabIterator < slabQueue->size; slabIterator++) {
        if ( tempNode->data->type == size && tempNode->data->objs_used < tempNode->data->num_objs) {
            // then tables are not full - return false
            return(0);
        }
        if (slabIterator+1 == slabQueue->size) {
            break;
        }
        tempNode = tempNode->next;
    }
    // otherwise if all are full, return true
    return(1);
}

// Similar idea as TablesAreFull, but this will take in a size and check if we have any slab table for the type yet
int notInSlabQueue( int size ) {
    // Takes in a size and checks if that type already exists in the slab table list
    if (slabQueue->size <= 0 ) {
        return(1);
    }
    slabNode *tempNode = slabQueue->head;

    // Iterates through the slabQueue and returns false if type is in the table, true otherwise
    for(int slabIterator = 0; slabIterator < slabQueue->size; slabIterator++) {
        if ( tempNode->data->type == size ) {
            return(0);
        }
        if (slabIterator+1 == slabQueue->size) {
            break;
        }
        tempNode = tempNode->next;
    }
    return(1);
}

// Takes in a size and will return a pointer to the slabNode of the first open table position it finds in the queue
slabNode *getOpenSlabPointer( int size ) {
    //Takes in a size and checks if that type already exists in the slab table list
    if (slabQueue->size <= 0 ) {
        return(-1);
    }
    slabNode *tempNode = slabQueue->head;

    // Parses slabQueue and if the type matches and it has an opening, it returns a pointer to the slabNode
    for(int slabIterator = 0; slabIterator < slabQueue->size; slabIterator++) {
        if ( tempNode->data->type == size && tempNode->data->objs_used < tempNode->data->num_objs) {
            return(tempNode);
        }
        if (slabIterator+1 == slabQueue->size) {
            break;
        }
        tempNode = tempNode->next;
    }
    // Negative 1 if no such item exists
    return(-1);
}

// Takes in a size and pointer, will attempt to slab_alloc on the given size if possible, setting testReturn to -1 if it cannot
void *addNewEntry(int size, int *testReturn) {
    int temp;
    testReturn = &temp;
    // grabs entirely new section from the buddy allocator, or fails out and sets value at testReturn to -1
    void *startPointer = buddy_alloc( size );

    // This sets our testReturn to -1 for testing if this function fails (in this case by buddy_alloc)
    if ( startPointer == (void*)-1 ) {
        *testReturn = -1;
        return(startPointer);
    }

    //setup new table entry:
    data *newEntrydata = malloc(sizeof(data));
    newEntrydata->size = 1;
    newEntrydata->start = startPointer;
    newEntrydata->bitmask = 0b1000000000000000000000000000000000000000000000000000000000000000;
    slab_table_entry *newEntry = malloc(sizeof(slab_table_entry));
    newEntry->type = (size-4)/N_OBJS_PER_SLAB;
    newEntry->size = size;
    newEntry->num_objs = N_OBJS_PER_SLAB;
    newEntry->objs_used = 1;
    newEntry->slab_data = newEntrydata;

    // Enqueue our table entry in slabQueue
    slabEnqueue(slabQueue, newEntry);

    // Return our pointer offset by 4, cast as a char * to allow easy pointer arithmetic (single bytes offset)
    return((char*) newEntrydata->start+4);
}

// Find the integer position of the first zero in a bitmask
int getFirstOpenPosition(uint64_t mask) {
    uint64_t tempMask = 0b1000000000000000000000000000000000000000000000000000000000000000;
    int counter = 0;
    
    // As long as we have a valid binary position, check if we make it into zero by bitwise and-ing it
    // with our tempMask variable - this will ensure we find the first 0 in the bitmask and return that
    // integer position starting from 0
    while (counter < N_OBJS_PER_SLAB) {
        if ((tempMask & mask) == 0) {
            return(counter);
        }
        counter++;
        tempMask = (uint64_t) tempMask>>1;
    }
    return(-1);
}

// Adds new item to existing slab (or if it cannot, call addNewEntry), or fails out and sets value at testReturn to -1
void *addToExisting(int size, int *testReturn) {
    int temp = 0, temp_objs_used = 0;
    uint64_t tempBitmask, bareBitmask = 0b1000000000000000000000000000000000000000000000000000000000000000;
    int tableIndex, mapping;
    testReturn = &temp;
    void *returnPointer;
    slabNode *nodeOfInterest;

    // Try to add new entry if existing size is full
    if ( tablesAreFull(size) ) {
        returnPointer = addNewEntry((size*N_OBJS_PER_SLAB + 4),&temp);
        if (temp != -1)
            return(returnPointer);
        else {
            *testReturn = -1;
            return(returnPointer);
        }
    }

    // Otherwise add to existing table

    // Get the node we want to add to
    nodeOfInterest = getOpenSlabPointer(size);
    tempBitmask = nodeOfInterest->data->slab_data->bitmask;

    // Get the position
    mapping = getFirstOpenPosition(tempBitmask);

    // Setup the new bitmask and apply it
    temp_objs_used = nodeOfInterest->data->objs_used;
    bareBitmask = bareBitmask>>mapping;
    tempBitmask = tempBitmask | bareBitmask;
    nodeOfInterest->data->slab_data->bitmask = tempBitmask;

    // Get our return pointer and add one to objs_used
    returnPointer = (char*) nodeOfInterest->data->slab_data->start + mapping*size + 4;
    nodeOfInterest->data->objs_used++;

    return(returnPointer);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// Major Functions
//
//////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////
//
// Function     : setup
// Description  : initialize the memory allocation system
//
// Inputs       : malloc_type - the type of memory allocation method to be used [0..3] where
//                (0) Buddy System
//                (1) Slab Allocation

void setup( int malloc_type, int mem_size, void* start_of_memory ){
    memType = malloc_type;
    memSize = mem_size;
    memStart = start_of_memory;

    allocatedQ = malloc(sizeof(queue)); // linked list of allocated memory

    // init the array of linked lists
    available_arr_size = findSize(memSize); // find the number of elements needed in the array
    availableList = malloc(available_arr_size*sizeof(queue*)); // allocated array of pointers, one for each power of 2
    for (int i=0; i<available_arr_size; i++) {
        availableList[i] = malloc(sizeof(queue)); // each entry in array is pointer to linked list
        availableList[i]->head = NULL; // init values to null so we dont have garbage data
        availableList[i]->end = NULL;
        availableList[i]->size = 0;
        
        // initialize the largest memory region with oen node, which is the only hole
        if (i==available_arr_size-1) {
            data *firstEntry = malloc(sizeof(data)); // create data
            firstEntry -> size = memSize; // size is entire initial memory region
            firstEntry -> start = memStart; // start is the start_of_memory region

            node *firstNode = malloc(sizeof(node)); // create node based on data
            firstNode -> next = NULL;
            firstNode -> data = firstEntry;
            enqueue(availableList[i], firstEntry); // want to queue in largest power of two array index
        }
    }

    //slab table only needed if slab allocator
    if (malloc_type==1) {
        slabQueue = malloc(sizeof(slabQ));
        slabQueue->end = NULL;
        slabQueue->head = NULL;
        slabQueue->size = 0;
    }
}

// function for buddy malloc
void *buddy_alloc( int size ) {
    int new_size = 1024; // min size is 1kb
    int arr_ind = 0;
    while (size > new_size) {
        // find the highest power of 2 above the requested size
        new_size = new_size*2;
        arr_ind++; // also increment to find the correct array index
    }
    if (arr_ind > available_arr_size-1) {
        return(-1);
    }
    while (availableList[arr_ind]->head == 0) {
        // while the correct size does not have an avaliable hole
        // we loop through the array and find the next highest empty avaliable hole
        int temp_ind = arr_ind+1;
        if (temp_ind > available_arr_size - 1) {
            return(-1);
        }
        while (availableList[temp_ind]->head == 0) {
            temp_ind++;
            if (temp_ind > available_arr_size-1) {
                // if the requested region is too large for avaliable holes, return -1 and don't allocate
                return(-1);
            }
        }

        // next, split larger hole into two smaller hole
        data *newEntry1 = malloc(sizeof(data)); // create new data entry
        newEntry1 -> size = 1024<<(temp_ind-1); // size is 2^temp_ind-1*1024
        newEntry1 -> start = availableList[temp_ind]->head->data->start; // start is same pointer of larger hole
        enqueue(availableList[temp_ind-1], newEntry1); // enqueue smaller hole

        data *newEntry2 = malloc(sizeof(data)); // create second new data entry
        newEntry2 -> size = 1024<<(temp_ind-1); // size is 2^temp_ind-1*1024
        newEntry2 -> start = (availableList[temp_ind]->head->data->start)+(1024<<(temp_ind-1)); // start is pointer of larger hole + size of smaller hole
        enqueue(availableList[temp_ind-1], newEntry2); // enqueue smaller hole

        dequeue(availableList[temp_ind]); // remove larger hole
    }

    enqueue(allocatedQ, availableList[arr_ind]->head->data); // enqueue the new data
    void *new_mem_ptr = availableList[arr_ind]->head->data->start; // remember the pointer to alloc region
    dequeue(availableList[arr_ind]); // dequeue the hole, not available anymore
    return(new_mem_ptr+4); // return the alloc region pointer
}

// Slab allocates based on an input size
void *my_slab( int size ) {
    int typeSize = size + 4;
    int testReturnVal = 0;
    void *returnPointer;

    // If not yet allocated, add a new entry
    // Otherwise, add it to an existing entry
    if ( notInSlabQueue( typeSize ))
        returnPointer = addNewEntry((typeSize*N_OBJS_PER_SLAB + 4), &testReturnVal);
    else
        returnPointer = addToExisting(typeSize, &testReturnVal);

    // Check if our return value is bad, otherwise return the pointer we received back
    if ( testReturnVal == -1 )
        return(-1);
    else
        return(returnPointer);
}

////////////////////////////////////////////////////////////////////////////
//
// Function     : my_malloc
// Description  : allocates memory segment using specified allocation algorithm
//
// Inputs       : size - size in bytes of the memory to be allocated
// Outputs      : -1 - if request cannot be made with the maximum mem_size requirement

void *my_malloc( int size ){
    if (memType == 0)
        return(buddy_alloc(size+4));
    else
        return(my_slab(size));
}

// function for buddy free
void *buddy_free( void *ptr ) {
    int valid_mem = 0; // bool to see if ptr is a valid allocated mem region

    // loop through the ll of allocated mem
    node *free_node = allocatedQ->head;
    while (free_node!=0) {
        if (free_node->data->start+4 == ptr) {
            valid_mem = 1;
            break;
        }
        free_node = free_node->next;
    }

    // if not a valid alloc mem region, return fail
    if (valid_mem==0) {
        return((void*)-1);
    }

    // remove node from alloc queue
    rip(free_node->data->start, allocatedQ);

    // add current node to avaliable holes
    int proper_ind = findSize(free_node->data->size)-1; // find the proper avaliable hole index
    enqueue(availableList[proper_ind], free_node->data);

    while (proper_ind<available_arr_size) {
        // while we are still within the range of avaliable holes
        node *current_node = availableList[proper_ind]->head;
        node *prev_node = NULL;
        int merged = 0;
        int flag = 0;

        // loop through current (proper_ind) sized holes
        while (current_node!=0) {
            if (current_node->data->start == free_node->data->start) {
                // if we found the node we just freed
                if (prev_node!=0 && prev_node->data->start+prev_node->data->size==current_node->data->start) {
                    // if the previous hole is the hole right before the current hole
                    if ((prev_node->data->start-memStart)%(1024<<(proper_ind))==0) {
                        // if the previous node is on the boundary of a higher power of two
                        // this calculation makes it so that if we combine these into a larger hole, it is correctly spaced
                        free_node->data->size = free_node->data->size*2; // set size to x2 because we combined two holes
                        free_node->data->start = prev_node->data->start; // set start to new start
                        rip(prev_node->data->start, availableList[proper_ind]); // rip both holes from current ll
                        rip(current_node->data->start, availableList[proper_ind]);
                        proper_ind++; // increment the index to higher ll
                        enqueue(availableList[proper_ind], free_node->data); // add new hole to higher ll
                        merged = 1; // set to true
                        flag = 0;
                    }
                }
                else if (current_node->next!=0 && current_node->data->start+current_node->data->size==current_node->next->data->start) {
                    // else if the current hole is the hole right before the next hole
                    if ((current_node->data->start-memStart)%(1024<<(proper_ind-1))==0) {
                        // if the current node is on the boundary of a higher power of two
                        // this calculation makes it so that if we combine these into a larger hole, it is correctly spaced
                        free_node->data->size = free_node->data->size*2; // set size to x2 because we combined two holes
                        free_node->data->start = current_node->data->start; // set start to new start
                        rip(current_node->next->data->start, availableList[proper_ind]); // rip both holes from current ll
                        rip(current_node->data->start, availableList[proper_ind]);
                        proper_ind++; // increment the index to higher ll
                        enqueue(availableList[proper_ind], free_node->data); // add new hole to higher ll
                        merged = 1; // set to true
                        flag = 1;
                    }
                }
                break; // if neither happens, we know the most recently freed hole cant be merged
                // note: because as other holes are freed they are automatically merged, we are done
            }
            prev_node = current_node;
            current_node = current_node->next;
        } 
        // If we merged, we can now free some no-longer used structures based on which block we entered
        if (merged == 1) {
            if (flag == 0) {
                free(prev_node);
                free(current_node);
            }
            else if (flag == 1) {
                free(current_node);
            }
        }

        if (merged==0) {
            // if nothing was merged, we can break else we need to keep checking if we can merge the new hole
            break;
        }
    }
}

// Frees a pointer to a slab allocated structure
void *slab_free( void *ptr ) {
    int tempType = 0;
    uint64_t mask = 1;
    slabNode *tempNode = slabQueue->head;
    slabNode *tempNodePrev = NULL;

    // Parses through our slabQueue backwards, looking for when we enter the range of our given pointer
    for (int tableIterator = 0; tableIterator < slabQueue->size; tableIterator++) {
        if (tempNode->data->slab_data->start <= ptr && (void *) ((char*) tempNode->data->slab_data->start + (N_OBJS_PER_SLAB-1)*tempNode->data->type + 4) >= ptr) {
            // Once in the range, check each spot in the bitmask for the input pointer
            tempType = tempNode->data->type;
            for (int spotFinder = 0; spotFinder < N_OBJS_PER_SLAB; spotFinder++) {
                if((void *) ((char*) tempNode->data->slab_data->start + spotFinder*tempType + 4) == ptr) {
                    // Once we find it, if the slab will continue to not be empty, just flip the bitmask bit, otherwise...
                    mask = ~(mask << (N_OBJS_PER_SLAB - spotFinder - 1));
                    tempNode->data->slab_data->bitmask = tempNode->data->slab_data->bitmask & mask;
                    tempNode->data->objs_used--;
                    // We will free the slab through buddy_free and rip the slab from our table
                    if (tempNode->data->slab_data->bitmask == 0) {
                        buddy_free(tempNode->data->slab_data->start);
                        slabRip(ptr, slabQueue);
                        return;
                    }
                }
            }
            // Pointer is invalid
            return(-1);
        }
        if (tableIterator+1 == slabQueue->size) {
            break;
        }
        tempNode = tempNode->next;
    }
    // Pointer is invalid
    return(-1);
}

////////////////////////////////////////////////////////////////////////////
//
// Function     : my_free
// Description  : deallocated the memory segment being passed by the pointer
//
// Inputs       : ptr - pointer to the memory segment to be free'd
// Outputs      :

void my_free( void *ptr )
{
    if (memType == 0)
        return(buddy_free(ptr));
    else
        return(slab_free(ptr));
}
