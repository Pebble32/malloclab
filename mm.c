/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never combined or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Reput_in this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "S�rar g�rkur",
    /* First member's full name */
    "Adam Postek",
    /* First member's email address */
    "adp14@hi.is",
    /* Second member's full name (leave blank if none) */
    "Dorian Konrad Michalski",
    /* Second member's email address (leave blank if none) */
    "dkm2@hi.is"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*Global variables*/
/* Sizes for memory word */
#define DOUBLE_WORD 8   /* Size of a double word */
#define SINGLE_WORD 4   /* Size of a single word */

/* Heap configuration */
#define HEAP_EXTANTION_SIZ (1<<12) /* Bytes to extend the heap by */

/* Memory block classes and sizes */
#define NUMBER_CLASS_SIZE 17 /* Total number of size classes */
#define MIN_BLOCK_SIZE (4*SINGLE_WORD) /* Size accounting for header, footer, pred, and succ */


/*
 * Combine the size and allocated bit to form 
 * a word which can be used as a header or footer.
 */     
#define PACK(size, alloc) ((size) | (alloc))

/* Access word-level operations at address p (a void pointer) */
#define GET(p)(*(unsigned int *) (p))  /* Read a word at address p */
#define PUT(p, val)(*(unsigned int *) (p) = (val))  /* Write a word at address p */

/* Extract size or allocation info from address p (pointing to a header/footer) */
#define GET_SIZE(p) (GET(p) & ~0x7) /* Zero out the last 3 bits to get size */
#define GET_ALLOC(p) (GET(p) & 0x1)  /* Extract the last bit for allocation info */


/* Compute addresses related to a given block pointer (first payload byte) */
#define HEADER_POINTER(block_p) ((char *) (block_p) - SINGLE_WORD) /* Address of header, with pointer arithmetic in bytes */
#define FOOTER_POINTER(block_p) ((char *) (block_p) + GET_SIZE(HEADER_POINTER(block_p)) - DOUBLE_WORD) /* Address of footer */
#define PRED(block_p) ((char *) (block_p))  /* Address of predecessor */
#define SUCC(block_p) ((char *) (block_p) + SINGLE_WORD) /* Address of successor */


/* Macros to find maximum and minimum of two values */
#define MAX(x, y) ((x) > (y) ? (x) : (y))  /* Get the maximum of x and y */
#define MIN(x, y) ((x) > (y) ? (y) : (x))  /* Get the minimum of x and y */

/* Compute addresses related to adjacent blocks for a given block pointer */
#define PREV_BLOCK(block_p) ((char *)(block_p) - GET_SIZE(((char *)(block_p) - DOUBLE_WORD)))  /* Address of previous block */
#define NEXT_BLOCK(block_p) ((char *)(block_p) + GET_SIZE(HEADER_POINTER(block_p))) /* Address of next block */

/* Compute addresses for predecessor and successor pointers for a given block */
#define PRED_BLOCK(block_p) ((char *) GET(PRED(block_p)))  /* Address of predecessor block */
#define SUCC_BLOCK(block_p) ((char *) GET(SUCC(block_p)))  /* Address of successor block */


/* private global variables */
static char *heap_listp = 0; // first block pointer of the heap
static char **freelist_p = 0; // pointer to the start an array of block pointers (free blocks) of different size classes


/* private helper function definitions */
static void *extend_heap(size_t word);
static void *combine(void *block_p);
static void *find_fit(size_t adj_size);
static void put_in(void *block_p, size_t adj_size);
static void insert(void* block_p);
static void delete(void* block_p);
static int get_size_class(size_t adj_size);
static int is_list_ptr(void *ptr);



int mm_init(void)
{
    // initialize an empty heap without alignment padding
    if((heap_listp = mem_sbrk(SINGLE_WORD*(NUMBER_CLASS_SIZE + 2 + 1))) == (void *) -1){
      return -1;
    }

    // Initialize pointers for size classes from 16 bytes and set freelist_p.
    memset(heap_listp,0,NUMBER_CLASS_SIZE*SINGLE_WORD);
    freelist_p = (char **) heap_listp;
    
    // set up the prologue and epilogue blocks
    heap_listp += NUMBER_CLASS_SIZE*SINGLE_WORD;
    PUT(heap_listp,PACK(DOUBLE_WORD,1)); // START prologue
    PUT(heap_listp + (1*SINGLE_WORD), PACK(DOUBLE_WORD,1)); // END prologue
    PUT(heap_listp + (2*SINGLE_WORD), PACK(0,1)); // START epilogue
    heap_listp += (1*SINGLE_WORD); // Adjust pointer for the END of epilogue block

    // extend the empty heap with free block with size of HEAP_EXTANTION_SIZ 
    if(extend_heap(HEAP_EXTANTION_SIZ/SINGLE_WORD) == NULL){
      return -1;
    }

    return 0;
}


void *mm_malloc(size_t size) {
    size_t adj_size; // adjusted block size for overhead
    size_t extend_size; // amount to extend the heap if no suitable free block is found
    char *block_p;

    // return NULL for non-positive size requests
    if (size <= 0)
        return NULL;

    // adjust block size to include overhead and alignment requirements
    if (size <= DOUBLE_WORD) {
        adj_size = 2 * DOUBLE_WORD;
    } else {
        adj_size = DOUBLE_WORD * ((size + DOUBLE_WORD - 1) / DOUBLE_WORD + 1);
    }

    // search the free list for a block that fits the adjusted size
    if ((block_p = find_fit(adj_size)) != NULL) {
        put_in(block_p, adj_size); // allocate and mark the block
        return block_p;
    }

    // if no suitable block is found, extend the heap
    extend_size = MAX(adj_size, HEAP_EXTANTION_SIZ);
    if ((block_p = extend_heap(extend_size/SINGLE_WORD)) == NULL)
        return NULL; // heap extension failed

    put_in(block_p, adj_size); // allocate and mark the new block

    return block_p;
}



void mm_free(void *block_p) {
    size_t size = GET_SIZE(HEADER_POINTER(block_p));
    
    // mark block's header and footer as unallocated
    PUT(HEADER_POINTER(block_p), PACK(size, 0));
    PUT(FOOTER_POINTER(block_p), PACK(size, 0));

    // zero out just in case
    PUT(PRED(block_p), 0);
    PUT(SUCC(block_p), 0);

    // combine with adjacent free blocks if any and insert into the free list
    insert(combine(block_p));
}


void *mm_realloc(void *block_p, size_t size)
{
    
    // catch block_p NULL since that is the same a malloc 
    if (block_p == NULL) {
       return mm_malloc(size);
    }
    
    // catch size 0
    if (size == 0) {
        mm_free(block_p);
        return NULL;
    }

    size_t old_size = GET_SIZE(HEADER_POINTER(block_p));
    size_t adj_size, nextblc_size, copy_size;
    void *oldblock_p = block_p;
    void *newblock_p;

    // check if we getting smaller or bigger
    // allowes us to use memory better
    // calculate adjusted size
    if (size <= DOUBLE_WORD) {
        adj_size = 2*DOUBLE_WORD;
    } else {
        adj_size = DOUBLE_WORD * ((size + DOUBLE_WORD - 1) / DOUBLE_WORD + 1);
    }

    // Catch samee adjusted size and old size
    if (adj_size == old_size) {
        return block_p;
    }
    
    // check if adjusted size is smaller or bigger
    if (adj_size < old_size) {
        // we split the block if the remainder is big
        if (old_size - adj_size >= MIN_BLOCK_SIZE) {
            // shrink the old block
            PUT(HEADER_POINTER(block_p), PACK(adj_size, 1));
            PUT(FOOTER_POINTER(block_p), PACK(adj_size, 1));
            // construct the new block
            newblock_p = NEXT_BLOCK(block_p);
            PUT(HEADER_POINTER(newblock_p), PACK(old_size - adj_size, 0));
            PUT(FOOTER_POINTER(newblock_p), PACK(old_size - adj_size, 0));
            // insert new block into free list
            // zero out just in case
            PUT(PRED(newblock_p), 0);
            PUT(SUCC(newblock_p), 0);
            insert(newblock_p);
        }
        
        return oldblock_p;
    } else {
        // this is to imporve utilization
        nextblc_size = GET_SIZE(HEADER_POINTER(NEXT_BLOCK(block_p)));
        if (!GET_ALLOC(HEADER_POINTER(NEXT_BLOCK(block_p)))) {
            if (nextblc_size + old_size >= adj_size) {
                // we combine
                // delete next block
                delete(NEXT_BLOCK(block_p));
                // make large block
                PUT(HEADER_POINTER(block_p), PACK(old_size + nextblc_size, 1));
                PUT(FOOTER_POINTER(block_p), PACK(old_size + nextblc_size, 1));
                return oldblock_p;
            }
        }

        // Allocate new block, copy data, and free old block
        newblock_p = mm_malloc(size);
        copy_size = old_size - DOUBLE_WORD;
        memcpy(newblock_p, oldblock_p, copy_size);
        mm_free(block_p);
        return newblock_p;
    }
}



static void *extend_heap(size_t word){
    char *block;
    size_t size;

    // adjust for double-word alignment
    size = (word % 2 == 0) ? word * SINGLE_WORD : (word + 1) * SINGLE_WORD;

    // attempt to extend the heap
    block = mem_sbrk(size);
    if ((long) block == -1) {
        return NULL; // extension was unsuccessful
    }

    // if the extension was successful, initialize block headers and the epilogue
    PUT(HEADER_POINTER(block), PACK(size,0));  // block's header
    PUT(FOOTER_POINTER(block), PACK(size,0));  // block's footer
    PUT(HEADER_POINTER(NEXT_BLOCK(block)), PACK(0,1)); // new epilogue

    // combine with the previous block if it was free
    block = combine(block);

    // add the block to the sorted list
    insert(block);

    return block;
}



static void *find_fit(size_t allocate_size){
    int c_size = get_size_class(allocate_size); // determine the size class
    void *c_p, *block; // pointers for the class and block
    size_t block_size; // size of the current block

    // iterate to find a suitable fit
    while (c_size < NUMBER_CLASS_SIZE){
        c_p = freelist_p + c_size;

        // if the current class size is not empty, scan the free list for a match
        block = (void *) GET(c_p);
        if (block != NULL){
            while (block != NULL){
                block_size = GET_SIZE(HEADER_POINTER(block));
                if (allocate_size <= block_size){
                    return block;
                }
                block = SUCC_BLOCK(block);
            }
        }

        // increment to check the next size class
        c_size++;
    }

    // no matching block was found
    return NULL; 
}


static int is_list_ptr(void *pointer){
    unsigned int start = (unsigned int) freelist_p;
    unsigned int end = start + SINGLE_WORD*(NUMBER_CLASS_SIZE-1);
    unsigned int pointer_val = (unsigned int) pointer;

    if((end - pointer_val) % SINGLE_WORD == 1){
        return 0;
    }
    if(pointer_val > end || pointer_val < start){
        return 0;
    }
  
    return 1;

}


static void *combine(void *block_p) {
    size_t prev_allocation = GET_ALLOC(FOOTER_POINTER(PREV_BLOCK(block_p)));
    size_t next_allocation = GET_ALLOC(HEADER_POINTER(NEXT_BLOCK(block_p)));
    size_t size = GET_SIZE(HEADER_POINTER(block_p));
    
    // check if prev allocation and next allocation are same or different
    if (!prev_allocation && next_allocation) {
        // delete prev block
        delete(PREV_BLOCK(block_p));
        size = size + GET_SIZE(HEADER_POINTER(PREV_BLOCK(block_p)));
        PUT(FOOTER_POINTER(block_p), PACK(size, 0));
        PUT(HEADER_POINTER(PREV_BLOCK(block_p)), PACK(size, 0));
        block_p = PREV_BLOCK(block_p);
    } else if (prev_allocation && !next_allocation) {
        // delete next block
        delete(NEXT_BLOCK(block_p));
        size = size + GET_SIZE(HEADER_POINTER(NEXT_BLOCK(block_p)));
        PUT(HEADER_POINTER(block_p), PACK(size, 0));
        PUT(FOOTER_POINTER(block_p), PACK(size, 0));
    } else if (prev_allocation && next_allocation) {
        return block_p; // if same, do nothing
    } else {
        // delete both next and prev block
        delete(PREV_BLOCK(block_p));
        delete(NEXT_BLOCK(block_p));
        size = size + GET_SIZE(HEADER_POINTER(PREV_BLOCK(block_p))) + GET_SIZE(FOOTER_POINTER(NEXT_BLOCK(block_p)));
        PUT(HEADER_POINTER(PREV_BLOCK(block_p)), PACK(size, 0));
        PUT(FOOTER_POINTER(NEXT_BLOCK(block_p)), PACK(size, 0));
        block_p = PREV_BLOCK(block_p);
    }

    
    return block_p;
}


static void put_in(void *block_p, size_t adj_size) {
    
    size_t csize = GET_SIZE(HEADER_POINTER(block_p));

    if ((csize - adj_size) < MIN_BLOCK_SIZE) {
        // difference not big enough, we just allocate the block
        delete(block_p);
        PUT(HEADER_POINTER(block_p), PACK(csize, 1));
        PUT(FOOTER_POINTER(block_p), PACK(csize, 1));
    } else { // difference is big enough create a new block
        // Delete block from free list
        delete(block_p);
        // allocate block
        PUT(HEADER_POINTER(block_p), PACK(adj_size, 1));
        PUT(FOOTER_POINTER(block_p), PACK(adj_size, 1));
        // make a new free block from the remainder
        block_p = NEXT_BLOCK(block_p);
        PUT(HEADER_POINTER(block_p), PACK(csize - adj_size, 0));
        PUT(FOOTER_POINTER(block_p), PACK(csize - adj_size, 0));
        // zero out just in case
        PUT(PRED(block_p), 0);
        PUT(SUCC(block_p), 0);
        insert(block_p);
    }

}

static void insert(void *block_p) {
    size_t block_size = GET_SIZE(HEADER_POINTER(block_p));
    char **class_ptr; // pointer to the start of the size class
    unsigned int block_ptr_value = (unsigned int) block_p;

    class_ptr = freelist_p + get_size_class(block_size);

    // check if the size class is empty
    if (!GET(class_ptr)) {
        PUT(class_ptr, block_ptr_value);
        PUT(PRED(block_p), (unsigned int) class_ptr);
        PUT(SUCC(block_p), 0);
    } else { // size class has blocks
        // link with the previous head block
        PUT(PRED(block_p), (unsigned int) class_ptr);
        PUT(SUCC(block_p), GET(class_ptr));
        PUT(PRED(GET(class_ptr)), block_ptr_value);
        PUT(class_ptr, block_ptr_value);
    }
}


static void delete(void *block_p) {
    int pre = !is_list_ptr(PRED_BLOCK(block_p));
    int suc = (SUCC_BLOCK(block_p) != (void *) 0);
    
    // if block_p is a middle block
    if (pre && suc) {
        PUT(SUCC(PRED_BLOCK(block_p)), (unsigned int) SUCC_BLOCK(block_p));
        PUT(PRED(SUCC_BLOCK(block_p)), (unsigned int) PRED_BLOCK(block_p));
    } else if (pre && !suc) {
        // if block_p is the last block
        PUT(SUCC(PRED_BLOCK(block_p)), 0);
    } else if (!pre && !suc) {
        // if block_p is both the first and the last block
        PUT(PRED_BLOCK(block_p), (unsigned int) SUCC_BLOCK(block_p));
    } else {
        // if block_p is the first block of a free list and has successors
        PUT(PRED_BLOCK(block_p), (unsigned int) SUCC_BLOCK(block_p));
        PUT(PRED(SUCC_BLOCK(block_p)), (unsigned int) PRED_BLOCK(block_p));
    }
    
    // zero out just in case
    PUT(PRED(block_p), 0);
    PUT(SUCC(block_p), 0);
}




static int get_size_class(size_t size) {
    int class_val = 0; // begin with class 0
    int accumulated_remainder = 0; // track remainders

    // check size and assign class_val
    while (size > MIN_BLOCK_SIZE && class_val < NUMBER_CLASS_SIZE-1) {
        accumulated_remainder += size % 2;
        size >>= 1; // devide by two 
        class_val++;
    }

    // adjust class_val if needed
    if (class_val < NUMBER_CLASS_SIZE-1 && accumulated_remainder && size == MIN_BLOCK_SIZE) {
        class_val++;
    }

    return class_val;
}



