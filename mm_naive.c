/*
<<<<<<< HEAD
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * Perf index = 44 (util) + 9 (thru) = 54/100
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
=======
 * mm-implicit.c - implicit free list based malloc package 
 * 
 * Perf index = 46 (util) + 9 (thru) = 55/100
 * 
 * Block Format: Minimum size is 8 bytes.
 *  - Allocated-Block Format: [Header - Payload]
 *  - Free-Block Format: [Header - (Space) - Footer]
 * Header/Footer: 1-word holds [block size, prev-bit, alloc-bit]
 *  - prev-bit: is previous block allocated.
 *  - alloc-bit: is current block allocated.
 * List Format: implicit free list
 *    [Head-Block[1] | Regular-Blocks(F/A) ...| Tail-Block[1]]
 * Placement Policy: using first-fit algorithm.
 * Split Policy: always split if remainder is greater than 8 bytes
 * Coalescing Policy: bi-direction coalescing after each unallocation
 * 
 * realloc: split if new size is less than the block size by 8 bytes
 *          allocate-copy-free if new size is greater than block size
>>>>>>> 8dfd593a2100c509002a4821259be4836466ae65
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

<<<<<<< HEAD
// mdriver 구동을 위한 team정보 struct 설정
team_t team = {
    /* Team name */

    "team 1",
    /* First member's full name */
    "hyemin Pyo",
    /* First member's email address */
    "pyolovely01@gmail.com",

    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* ================================ Macros ===================================== */
// 각종 변수,함수 설정
#define WSIZE 4 // word and header footer 사이즈를 byte로.
#define DSIZE 8 // double word size를 byte로
#define CHUNKSIZE (1<<12)

/* ------------------------------ Basic Utility ------------------------------- */

#define MAX(x,y) ((x)>(y)? (x) : (y))

// size를 pack하고 개별 word 안의 bit를 할당 (size와 alloc을 비트연산)
#define PACK(size,alloc) ((size)| (alloc))

/* address p위치에 words를 read와 write를 한다. */
#define GET(p) (*(unsigned int*)(p))
#define PUT(p,val) (*(unsigned int*)(p)=(val))

// address p위치로부터 size를 읽고 field를 할당
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* given block ptr bp, 그것의 header와 footer의 주소를 계산*/
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* GIVEN block ptr bp, 이전 블록과 다음 블록의 주소를 계산*/
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size =  GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc){ // case 1 - 이전과 다음 블록이 모두 할당 되어있는 경우, 현재 블록의 상태는 할당에서 가용으로 변경
        return bp;
    }
    else if (prev_alloc && !next_alloc){ // case2 - 이전 블록은 할당 상태, 다음 블록은 가용상태. 현재 블록은 다음 블록과 통합 됨.
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블록의 헤더만큼 사이즈 추가?
        PUT(HDRP(bp),PACK(size,0)); // 헤더 갱신
        PUT(FTRP(bp), PACK(size,0)); // 푸터 갱신
    }
    else if(!prev_alloc && next_alloc){ // case 3 - 이전 블록은 가용상태, 다음 블록은 할당 상태. 이전 블록은 현재 블록과 통합. 
        size+= GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size,0)); 
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0)); // 헤더를 이전 블록의 BLKP만큼 통합?
        bp = PREV_BLKP(bp);
    }
    else { // case 4- 이전 블록과 다음 블록 모두 가용상태. 이전,현재,다음 3개의 블록 모두 하나의 가용 블록으로 통합.
        size+= GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 이전 블록 헤더, 다음 블록 푸터 까지로 사이즈 늘리기
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}
static void *extend_heap(size_t words){ // 새 가용 블록으로 힙 확장
    char *bp;
    size_t size;
    /* alignment 유지를 위해 짝수 개수의 words를 Allocate */
    size = (words%2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    /* free block 헤더와 푸터를 init하고 epilogue 헤더를 init*/
    PUT(HDRP(bp), PACK(size,0)); // free block header
    PUT(FTRP(bp),PACK(size,0)); // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); // new epilogue header 추가

    /* 만약 prev block이 free였다면, coalesce해라.*/
    return coalesce(bp);
}

static char *heap_listp;
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
 /* create 초기 빈 heap*/
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1){
        return -1;
    }
    PUT(heap_listp,0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1));
    PUT(heap_listp + (3*WSIZE), PACK(0,1));
    heap_listp+= (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE)==NULL)
        return -1;
    return 0;
}


// 블록을 반환하고 경계 태그 연결 사용 -> 상수 시간 안에 인접한 가용 블록들과 통합하는 함수들
void mm_free(void *bp){ 
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp),PACK(size,0)); // header, footer 들을 free 시킨다.
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}


static void *find_fit(size_t asize){ // first fit 검색을 수행
    void *bp;
    for (bp= heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if (!GET_ALLOC(HDRP(bp)) && (asize<=GET_SIZE(HDRP(bp)))){
            return bp;
        }
    }
    return NULL; 
}

static void place(void *bp, size_t asize){ // 요청한 블록을 가용 블록의 시작 부분에 배치, 나머지 부분의 크기가 최소 블록크기와 같거나 큰 경우에만 분할하는 함수.
    size_t csize = GET_SIZE(HDRP(bp));
    if ( (csize-asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize,1));
        PUT(FTRP(bp), PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize,0));
        PUT(FTRP(bp), PACK(csize-asize,0));
    }
    else{
        PUT(HDRP(bp), PACK(csize,1));
        PUT(FTRP(bp), PACK(csize,1));
    }
} 
/* * mm_malloc - Allocate a block by incrementing the brk pointer.  *     Always allocate a block whose size is a multiple of the alignment.  */
 void *mm_malloc(size_t size) // 가용 리스트에서 블록 할당 하기
{
    size_t asize; // 블록 사이즈 조정
    size_t extendsize; // heap에 맞는 fit이 없으면 확장하기 위한 사이즈
    char *bp;

    /* 거짓된 요청 무시*/
    if (size == 0) return NULL;

    /* overhead, alignment 요청 포함해서 블록 사이즈 조정*/
    if (size <= DSIZE){
        asize = 2*DSIZE;
    }
    else {
        asize = DSIZE* ( (size + (DSIZE)+ (DSIZE-1)) / DSIZE );

    }
    /* fit에 맞는 free 리스트를 찾는다.*/
    if ((bp = find_fit(asize)) != NULL){
        place(bp,asize);
        return bp;
    }

    /* fit 맞는게 없다. 메모리를 더 가져와 block을 위치시킨다.*/
    extendsize = MAX(asize,CHUNKSIZE);
    if ( (bp=extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp,asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */

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
    copySize = GET_SIZE(HDRP(oldptr));  
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
=======
/* =============================================================================== */

team_t team = {
  /*team*/
  "team 1",
  /* First member's full name */
  "Hyemin Pyo",
  /* First member's email address */
  "pyolovely01@gmail.com",
  /* Second member's full name (leave blank if none) */
  "",
  /* Second member's email address (leave blank if none) */
  ""
};

/* ========================== Function Prototype =============================== */

inline static void* resize_block(void*, size_t);
inline static void* reallocate_block(void*, size_t);
inline static void* extend_heap(size_t);
inline static void* first_fit(size_t);
inline static void* best_fit(size_t);
inline static void* find_fit(size_t);
inline static void place(void*, size_t);
inline static void* coalesce(void*);
inline static void set_prev_bit(void*, size_t);

/* ========================== Compilation Flags =============================== */

// #define DEBUG                 /* uncomment to turn-on heap checker */

#ifdef DEBUG
  static void mm_check(int line);
  #define heap_check(line) (mm_check(line))
#else
  #define heap_check(line)
#endif

/* ================================ Macros ===================================== */

#define WSIZE 4                           /* Word size in bytes */
#define DSIZE 8                           /* Double word size in bytes */
#define CHUNKSIZE (1<<8)                  /* Minimum heap allocation size */
#define MIN_BLOCK_SIZE 8                  /* Minimum block size */
#define ALIGNMENT 8                       /* Payload Alignment */
#define WTYPE u_int32_t                   /* Word type */
#define BYTE char                         /* Byte type */

/* ------------------------------ Basic Utility ------------------------------- */

/* Move the address ptr by offset bytes */
#define MOVE_BYTE(ptr, offset) ((WTYPE *)((BYTE *)(ptr) + (offset)))
/* Move the address ptr by offset words */
#define MOVE_WORD(ptr, offset) ((WTYPE *)(ptr) + (offset))
/* Read a word from address ptr */
#define READ_WORD(ptr) (*(WTYPE *)(ptr))
/* Write a word value to address ptr */
#define WRITE_WORD(ptr, value) (*(WTYPE *)(ptr) = (value))
/* rounds up size (in bytes) to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
/* Get the maximum of x and y */
#define MAX(x, y) ((x) > (y)? (x) : (y))
/* Get the minimum of x and y */
#define MIN(x, y) ((x) < (y)? (x) : (y))

/* ----------------------- Header/Footer Macros ---------------------------- */

/* Pack the size, prev-allocated and allocation bits into a word */
#define PACK(size, prev, alloc) ((size) | (prev << 1) | (alloc))
/* Read the size from header/footer word at address Hptr */
#define READ_SIZE(Hptr) (READ_WORD(Hptr) & ~0x7)
/* Read the allocation-bit from header/footer word at address Hptr */
#define READ_ALLOC(Hptr) (READ_WORD(Hptr) & 0x1)
/* Read the prev-allocated-bit from header/footer word at address Hptr */
#define READ_PREV_ALLOC(Hptr) ((READ_WORD(Hptr) & 0x2) >> 1)
/* Write the size, prev-allocated and allocation bits to the word at address Hptr */
#define WRITE(Hptr, size, prev, alloc)\
  (WRITE_WORD((Hptr), PACK((size), (prev), (alloc))))

/* Write the size to the word at address Hptr */
#define WRITE_SIZE(Hptr, size)\
  (WRITE((Hptr), (size), READ_PREV_ALLOC(Hptr), READ_ALLOC(Hptr)))

/* Write allocation-bit to the word at address Hptr */
#define WRITE_ALLOC(Hptr, alloc)\
  (WRITE((Hptr), READ_SIZE(Hptr), READ_PREV_ALLOC(Hptr), alloc))

/* Write prev-allocated-bit to the word at address Hptr */
#define WRITE_PREV_ALLOC(Hptr, prev)\
  (WRITE((Hptr), READ_SIZE(Hptr), prev, READ_ALLOC(Hptr)))

/* ---------------------------- Payload Macros ------------------------------ */

/* Get the header-word pointer from the payload pointer pp */
#define HEADER(pp) (MOVE_WORD(pp, -1))
/* Get the footer-word pointer from the payload pointer pp */
#define FOOTER(pp) (MOVE_BYTE(pp, (BLOCK_SIZE(pp) - DSIZE)))
/* Get next block payload pointer from pp (current payload pointer) */
#define NEXT_BLOCK(pp) (MOVE_BYTE(pp, BLOCK_SIZE(pp)))
/* Get previous block payload pointer from pp (current payload pointer) */
#define PREV_BLOCK(pp) (MOVE_BYTE(pp, - READ_SIZE(MOVE_WORD(pp, -2))))
/* Read the block size at the payload pp */
#define BLOCK_SIZE(pp) (READ_SIZE(HEADER(pp)))
/* Read the payload size at pp */
#define PAYLOAD_SIZE(pp) (BLOCK_SIZE(pp) - WSIZE)
/* Gets the block allocation status (alloc-bit) */
#define GET_ALLOC(pp) (READ_ALLOC(HEADER(pp)))
/* Gets the previous block allocation status (prev-alloc-bit) */
#define GET_PREV_ALLOC(pp) (READ_PREV_ALLOC(HEADER(pp)))
/* Check if the block at the payload pp is free */
#define IS_FREE(pp) (!(GET_ALLOC(pp)))
/* Check if the previous block of the payload pp is free */
#define IS_PREV_FREE(pp) (!(GET_PREV_ALLOC(pp)))


/* Sets the size, prev-allocated and allocation-bit to header of block at pp */
#define SET_HEADER(pp, size, prev, alloc) ((WRITE(HEADER(pp),(size),(prev),(alloc))))
/* Sets the size to header of block at pp */
#define SET_HEADER_SIZE(pp, size) ((WRITE_SIZE(HEADER(pp),(size))))
/* Sets the allocation-bit to header of block at pp */
#define SET_HEADER_ALLOC(pp, alloc) ((WRITE_ALLOC(HEADER(pp),(alloc))))
/* Sets the prev-allocated-bit to header of block at pp */
#define SET_HEADER_PREV_ALLOC(pp, prev) ((WRITE_PREV_ALLOC(HEADER(pp),(prev))))

/* Sets the size, prev-allocated and allocation-bit to header/footer of block at pp */
#define SET_INFO(pp, size, prev, alloc)\
  ((WRITE(HEADER(pp),(size),(prev),(alloc))), \
   (WRITE(FOOTER(pp),(size),(prev),(alloc))))

/* Sets the size to header/footer of block at pp */
#define SET_SIZE(pp, size) (SET_INFO((pp),(size),GET_PREV_ALLOC(pp),GET_ALLOC(pp)))
/* Sets the allocation-bit to header/footer of block at pp */
#define SET_ALLOC(pp, alloc) (SET_INFO((pp),BLOCK_SIZE(pp),GET_PREV_ALLOC(pp),(alloc)))
/* Sets the prev-allocated-bit to header/footer of block at pp */
#define SET_PREV_ALLOC(pp, prev) (SET_INFO((pp),BLOCK_SIZE(pp),(prev),GET_ALLOC(pp)))

/* ======================= Private Global Variables ============================== */

// private global variable
static WTYPE* heap_listp;     /* pointer to first block payload */

/* =========================== Public Functions ================================== */

/* 
 * Initialize the malloc package.
 * return 0 on success, -1 on error.
 */
int mm_init(void) {
  /* Create the initial empty heap */
  if ((heap_listp = mem_sbrk(DSIZE)) == (WTYPE*)-1) return -1;

  WRITE(heap_listp, 0,0,1);                    /* Head Word */
  WRITE(heap_listp + 1, 0,1,1);                /* Tail Word */

  heap_listp += 2;

  /* Extend the empty heap with a free block of CHUNKSIZE bytes */
  if (extend_heap(CHUNKSIZE) == NULL) return -1;
  heap_check(__LINE__);
  return 0;
}

/* 
 *  Allocate an aligned block of memory of at least size bytes
 *  Return address of allocated block on success, NULL on error.
 */
void* mm_malloc(size_t size) {
  if (size == 0) return NULL;
  void* pp;                             /* Payload Pointer */
  size = ALIGN(size + WSIZE);           /* Add header word */

  /* Search the free list for a fit */
  if ((pp = find_fit(size)) == NULL) {
    /* No fit found, request a block from the memory */
    pp = extend_heap(MAX(size, CHUNKSIZE));
    if (pp == NULL) return NULL;
  }

  place(pp, size);
  heap_check(__LINE__);
  return pp;
}

/*
 * Free the allocated block at ptr, and coalesce it.
 */
void mm_free(void* ptr) {
  SET_ALLOC(ptr, 0);
  set_prev_bit(NEXT_BLOCK(ptr), 0);
  coalesce(ptr);
  heap_check(__LINE__);
}

/*
 * # ptr = NULL : allocate block of the given size.
 * # size = 0 : free the given block, return NULL.
 * # else: resize the allocated block at ptr.
 * 
 * Return address of the reallocated block, NULL on error.
 */
void* mm_realloc(void* ptr, size_t size) {
  if (ptr == NULL){
    return mm_malloc(size);
  }else if (size == 0){
    mm_free(ptr);
    return NULL;
  }else{
    ptr = resize_block(ptr, size);
    heap_check(__LINE__);
    return ptr;
  }
}

/* =========================== Private Functions ================================== */

/*
 * Resize the allocated block at pp to have size bytes
 * Return address of the resized block, NULL on error.
 */
static void* resize_block(void* pp, size_t size) {
  size_t asize = MAX(MIN_BLOCK_SIZE, ALIGN(size + WSIZE));
  size_t bsize = BLOCK_SIZE(pp);
  size_t csize = bsize - asize;

  if (asize > bsize) return reallocate_block(pp, size);

  if (csize >= MIN_BLOCK_SIZE){
    SET_HEADER_SIZE(pp, asize);
    void* free_part = NEXT_BLOCK(pp);
    SET_INFO(free_part, csize, 1, 0);
    set_prev_bit(NEXT_BLOCK(free_part), 0);
    coalesce(free_part);
  }

  return pp;
}

/*
 * Allocate block of the given size, copy content, free old block
 * Return address of the new block, NULL on error.
 */
static void* reallocate_block(void* ptr, size_t size) {
  void *newptr = mm_malloc(size);
  if (newptr == NULL) return NULL;
  size_t copy_size = MIN(PAYLOAD_SIZE(ptr), size);
  memcpy(newptr, ptr, copy_size);
  mm_free(ptr);
  return newptr;
}

/**
 * Add free block with aligned size to the end of the heap.
 * Return address of the added free block, NULL on error.
*/
void* extend_heap(size_t size) {
  WTYPE* pp;
  size = ALIGN(size);
  if ((long)(pp = mem_sbrk(size)) == -1) return NULL;

  size_t prev_bit = GET_PREV_ALLOC(pp);
  SET_INFO(pp, size, prev_bit, 0);            /* Initialize a free block */
  SET_HEADER(NEXT_BLOCK(pp), 0,0,1);          /* New Tail Word */

  if (!prev_bit) return coalesce(pp);   /* coalesce if previous block is free */
  return pp;
}

/* Find the first block greater than or equal to size
 * Return address of the first-fit, NULL if no-fit.
*/
static void* first_fit(size_t size) {
  void* pp;

  for (pp = heap_listp; BLOCK_SIZE(pp) > 0; pp = NEXT_BLOCK(pp)) {
    if (IS_FREE(pp) && (size <= BLOCK_SIZE(pp))) return pp;
  }

  return NULL;
}

/* Find the smallest block greater than or equal to size
 * Return address of the best-fit, NULL if no-fit.
*/
static void* best_fit(size_t size) {
  void* pp;
  void* best = NULL;
  size_t best_size = __SIZE_MAX__;

  for (pp = heap_listp; BLOCK_SIZE(pp) > 0; pp = NEXT_BLOCK(pp)) {
    size_t curr_size = BLOCK_SIZE(pp);
    if (IS_FREE(pp) && (size <= curr_size) && (curr_size < best_size)){
      best = pp;
    }
  }

  return best;
}

/**
 * Find a free block with size greater than or equal to size.
 * Return address of a fit-block, NULL if no fit.
*/
static void* find_fit(size_t size) {
  return first_fit(size);
}

/**
 * Allocate the free block at pp.
 * Split the block if the remainder is greater than MIN_BLOCK_SIZE.
*/
static void place(void *pp, size_t size) {
  size_t bsize = BLOCK_SIZE(pp);

  if (bsize < (size + MIN_BLOCK_SIZE)){
    SET_ALLOC(pp, 1);
    set_prev_bit(NEXT_BLOCK(pp), 1);
  }else{
    SET_HEADER(pp, size, GET_PREV_ALLOC(pp), 1);
    pp = NEXT_BLOCK(pp);
    SET_INFO(pp, bsize-size, 1, 0);
  }
}

/**
 * Coalesce the current block with its free previous and/or next blocks.
 * Return the address of the coalesced free-block.
*/
static void* coalesce(void *pp) {
  size_t prev_alloc = GET_PREV_ALLOC(pp);
  size_t next_alloc = GET_ALLOC(NEXT_BLOCK(pp));
  size_t size = BLOCK_SIZE(pp);
  size_t prev_bit = prev_alloc;

  if (prev_alloc && next_alloc) return pp;
  
  if (!next_alloc) size += BLOCK_SIZE(NEXT_BLOCK(pp));

  if (!prev_alloc) {
    pp = PREV_BLOCK(pp);
    size += BLOCK_SIZE(pp);
    prev_bit = GET_PREV_ALLOC(pp);
  }

  SET_INFO(pp, size, prev_bit, 0);
  return pp;
}
/**
 * Sets the prev-bit in the (free/allocated) block at pp.
*/
static void set_prev_bit(void* pp, size_t prev_bit){
  if (IS_FREE(pp)){
    SET_PREV_ALLOC(pp, prev_bit);
  }else{
    SET_HEADER_PREV_ALLOC(pp, prev_bit);
  }
}

/* ========================== Debugging Functions =============================== */

#ifdef DEBUG
/** 
 * Heap Consistency Checker, checks for:
 * - head and tail words of the heap
 * - block header and footer equality for free blocks
 * - block size >= minimum size
 * - payload alignment
 * - coalescing (no contiguous free blocks)
 * - prev-bit correctness.
 * - total heap size
*/
static void mm_check(int line){
  WTYPE* ptr = mem_heap_lo();
  WTYPE* end_ptr = MOVE_BYTE(ptr, mem_heapsize()) - 1;
  // Check head word (size = 0, prev is free, allocated)
  if (READ_SIZE(ptr) != 0){
    printf("Error at %d: head-word size = %u\n",line, READ_SIZE(ptr));
  }

  if (READ_ALLOC(ptr) != 1){
    printf("Error at %d: head-word is not allocated\n",line);
  }

  if (READ_PREV_ALLOC(ptr)){
    printf("Error at %d: head-word is prev is allocated\n",line);
  }

  // Check tail word (size = 0, allocated)
  if (READ_SIZE(end_ptr) != 0){
    printf("Error at %d: tail-word size = %u\n",line, READ_SIZE(end_ptr));
  }

  if (READ_ALLOC(end_ptr) != 1){
    printf("Error at %d: tail-word is not allocated\n",line);
  }

  size_t heap_size = DSIZE;
  int prev_free = 0;

  // Check regular blocks
  for (ptr = heap_listp; ptr < end_ptr; ptr = NEXT_BLOCK(ptr)){
    // check header and footer equality
    if (IS_FREE(ptr) && (READ_WORD(HEADER(ptr)) != READ_WORD(FOOTER(ptr)))){
      printf("Error at %d: at block %p => header = %u, footer = %u\n",
        line, ptr, READ_WORD(HEADER(ptr)), READ_WORD(FOOTER(ptr)));
    }
    // check that block size >= minimum size
    if (BLOCK_SIZE(ptr) < MIN_BLOCK_SIZE){
      printf("Error at %d: block %p has size < min size, (%u < %u)\n",
        line, ptr, BLOCK_SIZE(ptr), MIN_BLOCK_SIZE);
    }
    // check payload alignment
    if((unsigned) ptr % ALIGNMENT){
      printf("Error at %d: block %p is not aligned to %d\n",
        line, ptr, ALIGNMENT);
    }
    // check coalescing.
    if (prev_free && IS_FREE(ptr)){
      printf("Error at %d: two contiguous free blocks not coalesced\n", line);
    }
    // check prev-allocated bit
    if (prev_free != IS_PREV_FREE(ptr)){
      printf("Error at %d: block %p prev-bit is incorrect\n", line, ptr);
    }

    prev_free = IS_FREE(ptr);
    heap_size += BLOCK_SIZE(ptr);
  }
  // check total heap size
  if (heap_size != mem_heapsize()){
    printf("Error at %d: heap size not accurate, %u should be %u\n",
     line, heap_size, mem_heapsize());
  }
}

#endif
>>>>>>> 8dfd593a2100c509002a4821259be4836466ae65
