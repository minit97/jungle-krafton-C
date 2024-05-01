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
#include "mm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "memlib.h"
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};
#define mode 3
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
/*
 * mm_init - initialize the malloc package.
 */
/* Basic constants and macros */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)
#define MAX(x, y) ((x) > (y) ? (x) : (y))
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))
/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)- WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp)-WSIZE))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp)-DSIZE))
/* Given block ptr bp, compute address of its predecessor and successor */
#define FIND_PRED(bp) (*(char **)(bp))
#define FIND_SUCC(bp) (*(char **)((char *)(bp) + WSIZE))
static char *heap_listp;
static char *free_listp;
static void *coalesce(void *bp);
static void remove_list(void *bp)
{
  if (bp == free_listp)
  {
    free_listp = FIND_SUCC(bp);
    return;
  }
    FIND_SUCC(FIND_PRED(bp)) = FIND_SUCC(bp);
    if (FIND_SUCC(bp) != NULL)
      FIND_PRED(FIND_SUCC(bp)) = FIND_PRED(bp);
  return;
}
static void add_list(void *bp)
{
    FIND_SUCC(bp) = free_listp;
    if (free_listp != NULL)
      FIND_PRED(free_listp) = bp;
    free_listp = bp;
}
static void *coalesce(void *bp) {
  size_t size = GET_SIZE(HDRP(bp));
  // char prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) ? 'ALLOCATED' : 'FREE';
  // char next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))) ? 'ALLOCATED' : 'FREE';
  size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  if (prev_alloc && next_alloc) {
    add_list(bp);
    return bp;
  } else if (prev_alloc && !next_alloc) {
    remove_list(NEXT_BLKP(bp));
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  } else if (!prev_alloc && next_alloc) {
    remove_list(PREV_BLKP(bp));
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    bp = PREV_BLKP(bp);
  } else {
    remove_list(PREV_BLKP(bp));
    remove_list(NEXT_BLKP(bp));
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
  add_list(bp);
  return bp;
}
static void *extend_heap(size_t words) {
  char *bp;
  size_t size;
  /* Allocate an even number of words to maintain alignment */
  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
  if ((long)(bp = mem_sbrk(size)) == -1) return NULL;
  /* Initialize free block header/footer and the epilogue header */
  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
  return coalesce(bp);
}
int mm_init(void) {
  /* Create the initial empty heap */
  if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *)-1) return -1;
  PUT(heap_listp, 0);
  PUT(heap_listp + (1 * WSIZE), PACK(2*DSIZE, 1));
  PUT(heap_listp + (2 * WSIZE), NULL); // predecessor
  PUT(heap_listp + (3 * WSIZE), NULL); // successor
  PUT(heap_listp + (4 * WSIZE), PACK(2*DSIZE, 1));
  PUT(heap_listp + (5 * WSIZE), PACK(0, 1));
  free_listp = heap_listp + 2*WSIZE;
  /* Extend the empty heap with a free block of CHUNKSIZE bytes */
  if (extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;
  return 0;
}
/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
#if mode == 1
static void *find_fit(size_t asize) {
  char *bp = heap_listp;
  size_t size;
  while (GET_SIZE(HDRP(bp)) != 0) {
    size = GET_SIZE(HDRP(bp));
    if (asize <= size && !GET_ALLOC(HDRP(bp))) return bp;
    bp = NEXT_BLKP(bp);
  }
  return NULL;
}
#elif mode == 3
static void *find_fit(size_t asize) {
  char *bp = free_listp;
  // size_t size;
  while (GET_ALLOC(HDRP(bp)) != 1) {
    if (asize <= GET_SIZE(HDRP(bp)))
      return bp;
    bp = FIND_SUCC(bp);
  }
  return NULL;
}
#endif
static void place(void *bp, size_t asize) {
  remove_list(bp);
  size_t old_size = GET_SIZE(HDRP(bp));
  if ((old_size - asize) >= (2 * DSIZE)) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(old_size - asize, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(old_size - asize, 0));
    add_list(NEXT_BLKP(bp));
  } else {
    PUT(HDRP(bp), PACK(old_size, 1));
    PUT(FTRP(bp), PACK(old_size, 1));
    }
}
void *mm_malloc(size_t size) {
  size_t asize;
  size_t extendsize;
  char *bp;
  if (size == 0) return NULL;
  if (size <= DSIZE)
    asize = 2 * DSIZE;
  else
    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  /* Search the free list for a fit */
  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    return bp;
  }
  /* No fit found. Get more memory and place the block*/
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    return NULL;
  place(bp, asize);
  return bp;
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
  size_t size = GET_SIZE(HDRP(bp));
  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
  if (ptr == NULL) {
    return mm_malloc(size);
  }
  if (size <= 0) {
    mm_free(ptr);
    return 0;
  }
  void *oldptr = ptr;
  void *newptr;
  size_t copySize;
  newptr = mm_malloc(size);
  if (newptr == NULL) return NULL;
  // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
  copySize = GET_SIZE(HDRP(ptr)) - DSIZE;
  if (size < copySize) copySize = size;
  memcpy(newptr, oldptr, copySize);
  mm_free(oldptr);
  return newptr;
}