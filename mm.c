/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
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
    "SÃrar gÃrkur",
    /* First member's full name */
    "Adam Postek",
    /* First member's email address */
    "adp14@hi.is",
    /* Second member's full name (leave blank if none) */
    "Korad Kondzio Michalski",
    /* Second member's email address (leave blank if none) */
    "dkm2@hi.is"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define WSIZE 4
#define DSIZE 8
#define MINBLOCKSIZE 16
#define INITSIZE 16


#define MAX(x, y) ((x) > (y)? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p)        (*(size_t *)(p))
#define PUT(p, val)   (*(size_t *)(p) = (val))
#define GET_SIZE(p)  (GET(p) & ~0x1)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp)     ((void *)(bp) - WSIZE)
#define FTRP(bp)     ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((void *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))
#define NEXT_FREE(bp)(*(void **)(bp))
#define PREV_FREE(bp)(*(void **)(bp + WSIZE))



static void *extend_heap(size_t words);
static void *find_fit(size_t size);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void remove_freeblock(void *bp);

static char *heap_listp = 0;  /* Points to the start of the heap */
static char *free_listp = 0;  /* Poitns to the frist free block */
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	  // Initialize the heap with freelist prologue/epilogoue and space for the
  // initial free block. (32 bytes total)
  if ((heap_listp = mem_sbrk(INITSIZE + MINBLOCKSIZE)) == (void *)-1)
      return -1;
  PUT(heap_listp,             PACK(MINBLOCKSIZE, 1));           // Prologue header
  PUT(heap_listp +    WSIZE,  PACK(MINBLOCKSIZE, 0));           // Free block header

  PUT(heap_listp + (2*WSIZE), PACK(0,0));                       // Space for next pointer
  PUT(heap_listp + (3*WSIZE), PACK(0,0));                       // Space for prev pointer

  PUT(heap_listp + (4*WSIZE), PACK(MINBLOCKSIZE, 0));           // Free block footer
  PUT(heap_listp + (5*WSIZE), PACK(0, 1));                      // Epilogue header

  // Point free_list to the first header of the first free block
  free_listp = heap_listp + (WSIZE);

   return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
  size_t asize;       // Adjusted block size
  size_t extendsize;  // Amount to extend heap by if no fit
  char *bp;

  /* The size of the new block is equal to the size of the header and footer, plus
   * the size of the payload. Or MINBLOCKSIZE if the requested size is smaller.
   */
  asize = MAX(ALIGN(size) + DSIZE, MINBLOCKSIZE);

  // Search the free list for the fit
  if ((bp = find_fit(asize))) {
    place(bp, asize);
    return bp;
  }

  // Otherwise, no fit was found. Grow the heap larger.
  extendsize = MAX(asize, MINBLOCKSIZE);
  if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    return NULL;

  // Place the newly allocated block
  place(bp, asize);

  return bp;

}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














