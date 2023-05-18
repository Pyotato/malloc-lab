/*
 * mm-seglist
 * 
 * 성능 index = 58 (util) + 40 (thru) = 98/100
 * 
 * 블록 형식:최소 16 바이트
 *  free 블록 구조 : [Header - Pred(전) - Succ(후) - (Empty) - Footer]
 *  할당된 블록 구조 : [Header - Payload - Footer]
 *  Header/Footer: 1 word (블록의 크기 정보) + Least significant bit (LSB)에 할당된 비트 포함 (allocation-bit)
 *  Pred: 1-word(전에 위치한 free 블록의 주소) predecessor free-block.
 *  Succ: 1-word(후에 위치한 free 블록의 소) successor free-block.
 * 
 * 
 * List 구조: explicit-free list 배열 8개 (explicit-free list : free인 블록간에 포인터로 free인 적절한 블록 빠르게 찾을 때 유리함) 
 * -> seglist구현 방식 : size 클래스들을 담을 배열 (size 클래스 별로 free-list 갖고 음음)
 * 힙 구조: [seglist-array[8] | Head-Block[1] | Regular-Blocks ...| Tail-Block[1]]
 * Head/Tail: 1-word(size가 0인 블록 할당).
 * 
 * Policies : 할당할 블록을 고르는 기준
 *    Placement Policy: best-fit (다 돌면서 가장 fragmentation이 발생이 적을 적합한 블록에 할당하기)
 *    Split Policy: 나머지가 16바이트보다 크면 split
 *    Coalescing Policy: free 블록들과 할당된 블록들 모두 양방향 coalescing(합쳐주기)
 *    Heap Extension Policy (힙영역 확장): 
 *      - if size > CHUNK_SIZE : 주어진 size +1 블록 늘려주기
 *      - else: 주어진 각각의 size보다 몇 블록씩 늘려주기 
 * 
 * realloc:
 *  - 새로운 size(new-size) > 현재 size(current-size)? 블록을 늘릴 수 있다면 늘리기 : 늘릴 수 없다면 블록을 reallocate(재할당)하기 (allocate - copy - free).
 *  - 새로운 size(new-size) + 64바이트 < 현재 size(current size) && split 블록.
 *
 * ※ 참고 사항
 * Coalescing 느슨함 : seglist에서 size 다양성을 위해 일부러 coalsce하지 않은 free block 있음
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h
/* =============================================================================== */

team_t team = {
  /* Team name */
  "pyotato",
  /* First member's full name */
  "Hyemin PYo",
  /* First member's email address */
  "pyolovely01@gmail.com",
  /* Second member's full name (leave blank if none) */
  "",
  /* Second member's email address (leave blank if none) */
  ""
};

/* ========================== Function Prototype =============================== */

inline static void* resize_block(void*, size_t); /* pp위치에 할당된 블록 크기 size만큼 바이트 크기 Resize해주기, return error? NULL : resize한 블록의 주소 */
inline static void* reallocate_block(void*, size_t); /* size의 블록을 할당하기 + content 복사 + 예전 블록 free 해주기, return error? NULL : 새 블록의 주소 */
inline static int can_expand(void*, size_t); /* pp에 할당된 블록이 인자로 받은 size만큼 확장 가능한 지 체크 */
inline static void* expand_block(void*, size_t); /* 주어진 size만큼 pp에 할당된 블록 확장해주기, return 새로 확장한 블록의 주소 */
inline static void chop_block(void*, size_t);  /* free 블록을 주어진 크기만큼 작은 free-block으로 쪼개주기*/
inline static void* extend_heap(size_t); /*free 블록을 힙의 끝에 더해주기. return error?NULL:새로 더해준 free 블록의 주소 */
inline static void* first_fit(void*, size_t); /* size보다 크거나 같은 첫 블록 찾기. Return no-fit ? NULL : first-fit의 주소 */
inline static void* best_fit(void*, size_t); /* size보다 크거나 같은 가장 작은 블록 찾기 Return no-fit? NULL: best-fit의 주소*/
inline static void* find_fit(size_t); /* free 블록 size <=size. no fit? NULL : Return fit-block 주소 */
inline static void* place(void*, size_t);/* pp에 free 블록 할당. 나머지 > MIN_BLOCK_SIZE && 블록 Split. Return 할당된 블록 payload 주소 리턴*/
inline static void* coalesce(void*); /* 현재 블록과 이전 AND/OR 이후의 free블록 합쳐주기 . Return coalesced(합쳐준) block 주소*/
inline static void link_block(void*); /* pp에 있는 block을 free-list에 추가해주기 */
inline static void unlink_block(void*); /* free 리스트에서 pp에 있는 블록 제거*/
inline static int seg_index(size_t); /* Return 주어진 size의 블록들을 담고 있을 seglist의 인덱스 */

/* ========================== Compilation Flags =============================== */

// #define DEBUG                 /* heap checker 사용을 위해서는 주석을 해지해주세요*/

#ifdef DEBUG
  static void mm_check(int);
  static void check_seglist(int, int);
  static int check_free_list(int, int);
  #define heap_check(line) (mm_check(line))
#else
  #define heap_check(line)
#endif

/* ================================ Macros ===================================== */

#define WSIZE 4                           /* 바이트 단위의 Word size (4바이트) */
#define DSIZE 8                           /* 바이트 단위의 Double word (8바이트) */
#define CHUNKSIZE (1<<8)                  /* 힙 할당 최소 size*/
#define MIN_BLOCK_SIZE 16                 /* 최소 블록 size (16바이트) */
#define ALIGNMENT 8                       /* Payload Alignment */
#define SEGLIST_NUM 8                     /* seglist에 있는 리스트의 개수(8개) */
#define WTYPE u_int32_t                   /* Word type */
#define BYTE char                         /* Byte type */

/* ------------------------------ Basic Utility ------------------------------- */

/* 포인터 주로를 offset 바이트만큼 옮겨주기 */
#define MOVE_BYTE(ptr, offset) ((WTYPE *)((BYTE *)(ptr) + (offset)))

/* ptr 주소를 offset words만큼 옮겨주기*/
#define MOVE_WORD(ptr, offset) ((WTYPE *)(ptr) + (offset))

/* ptr 주소로부터 word read(읽기) */
#define READ_WORD(ptr) (*(WTYPE *)(ptr))

/* 주소 ptr에 word value write(쓰기) */
#define WRITE_WORD(ptr, value) (*(WTYPE *)(ptr) = (value))

/* ALIGNMENT의  곱에 가장 가까운 바이트 단위로 size를 올림해주기(round up size) */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* x와 y 중 최대값 구하기 */
#define MAX(x, y) (((x) > (y))? (x) : (y))

/* x와 y 중 최솟값 구하기 */
#define MIN(x, y) (((x) < (y))? (x) : (y))

/* ----------------------- Header/Footer Macros ---------------------------- */

/* 블록의 size와 할당된 bit (allocated-bit)를 word로 묶기 */
#define PACK(size, alloc) ((size) | (alloc))

/* header/footer word에서 Hptr가 가르키는 size 읽기 */
#define READ_SIZE(Hptr) (READ_WORD(Hptr) & ~0x7)

/* header/footer word에서 Hptr가 가르키는 (할당된 비트)allocated-bit 읽기 */
#define READ_ALLOC(Hptr) (READ_WORD(Hptr) & 0x1)

/* Hptr가 가르키는 word주소의 size&alocation-bit(할당된 비트) Write */
#define WRITE(Hptr, size, alloc) (WRITE_WORD((Hptr), PACK((size), (alloc))))

/* Hptr가 가르키는 word주소의 size 덮어쓰기 */
#define WRITE_SIZE(Hptr, size) (WRITE((Hptr), (size), READ_ALLOC(Hptr)))

/* Hptr가 가르키는 word주소의 allocation-bit 덮어쓰기 */
#define WRITE_ALLOC(Hptr, alloc) (WRITE((Hptr), READ_SIZE(Hptr), (alloc)))

/* ---------------------------- Payload Macros ------------------------------ */

/* payload pointer pp가 가르키는 header-word pointer 가져오기 */
#define HEADER(pp) (MOVE_WORD(pp, -1))

/* payload pointer pp가 가르키는 footer-word pointer 가져오기*/
#define FOOTER(pp) (MOVE_BYTE(pp, PAYLOAD_SIZE(pp)))

/* 현재 payload pointer pp으로부터 다음 payload 블록 pointer 가져오기 */
#define NEXT_BLOCK(pp) (MOVE_BYTE(pp, BLOCK_SIZE(pp)))

/* 현재 payload pointer pp으로부터 이전 payload 블록 pointer 가져오기 */
#define PREV_BLOCK(pp) (MOVE_BYTE(pp, - READ_SIZE(MOVE_WORD(pp, -2))))

/* payload pointer pp으로부터 블록 size 읽기 */
#define BLOCK_SIZE(pp) (READ_SIZE(HEADER(pp)))

/* payload pointer pp으로부터 payload 크기 읽기 */
#define PAYLOAD_SIZE(pp) (BLOCK_SIZE(pp) - DSIZE)

/* 이전 블록 payload pointer pp가 free 블록인지 크체크 */
#define IS_FREE(pp) (!(READ_ALLOC(HEADER(pp))))

/* 블록 payload pointer pp에서 size와 allocation-bit 설정(set)해주기 */
#define SET_INFO(pp, size, alloc)\
  ((WRITE(HEADER(pp),(size),(alloc))), \
   (WRITE(FOOTER(pp),(size),(alloc))))

/* pp 블록의 사이즈를 header와 footer로 (설정)set해주기 */
#define SET_SIZE(pp, size)\
  ((WRITE_SIZE(HEADER(pp),(size))), \
   (WRITE_SIZE(FOOTER(pp),(size))))

/* pp블록의 header와 footer로 allocation-bit Set(설정) */
#define SET_ALLOC(pp, alloc)\
  ((WRITE_ALLOC(HEADER(pp),(alloc))), \
   (WRITE_ALLOC(FOOTER(pp),(alloc))))

/* payload 주소의 predecessor(앞) 주소 가져오기 */
#define GET_PRED(pp) ((WTYPE *)(READ_WORD(pp)))
/* payload 주소의 successor(뒤) 주소 가져오기 */
#define GET_SUCC(pp) ((WTYPE *)(READ_WORD(MOVE_WORD(pp, 1))))
/* predecessor 주소를 pred_pp로 set(설정)해주기 */
#define SET_PRED(pp, pred_pp) (WRITE_WORD(pp, ((WTYPE) pred_pp)))
/*  successor payload주소를 succ_pp set(설정)해주기 */
#define SET_SUCC(pp, succ_pp) (WRITE_WORD(MOVE_WORD(pp, 1), ((WTYPE) succ_pp)))

/* ======================= Private Global Variables ============================== */

// private global variable
static WTYPE** seglist;       /* free 리스트 포인터들이 담긴 배열 */

/* =========================== Public Functions ================================== */

/* 
 * 말록 패키지 초기화 : 성공 시 0리턴, 실패 시 -1리턴
 * Initialize the malloc package.
 * return 0 on success, -1 on error.
 */
int mm_init(void) {
  /* 초기 빈 힙 생성 : seglist + head +tail */
  void* heap = mem_sbrk((SEGLIST_NUM + 2) * WSIZE);
  if (heap == (void*)-1) return -1;

  seglist = heap;
  heap = MOVE_WORD(heap, SEGLIST_NUM);
  //seglist 초기화
  for(int i = 0; i < SEGLIST_NUM; ++i){
    seglist[i] = NULL;
  }

  WRITE(heap, 0, 1);                          /* Head Word */
  WRITE(MOVE_WORD(heap, 1), 0, 1);            /* Tail Word */

  /* 작은 free 블록 단위로 빈 힙을 늘려주기하기*/
  if (extend_heap(4 * MIN_BLOCK_SIZE) == NULL) return -1;
  heap_check(__LINE__);
  return 0;
}

/* 
 *  최소 size 바이트 크기만큼의 연속된 메모리 블록 할당하기
 *  return error? NULL : 성공적으로 할당한 블록의 주소
 *  Allocate an aligned block of memory of at least size bytes
 *  Return address of allocated block on success, NULL on error.
 */
void* mm_malloc(size_t size) {
  if (size == 0) return NULL;            /* 할당 실패 */
  void* pp;                             /* Payload Pointer */
  size = ALIGN(size + DSIZE);          /* header & footer words  더해주기*/
  size = MAX(size, MIN_BLOCK_SIZE);

  /* free list에서 fit 찾기 */
  if ((pp = find_fit(size)) == NULL) { 
    /* fit (size에 맞는 블록)이 없다면 메모리한테 블록 request */
    if (size > CHUNKSIZE){ /*사이즈가 힙할당 최소사이즈보다 크다면 */
      pp = extend_heap(size); /*free 블록을 힙의 끝에 더해주기*/
    }else{ /*사이즈가 힙할당 최소사이즈보다 작거나 같다면*/
      pp = extend_heap(4 * CHUNKSIZE); /*힙할당 최소 사이즈*4만큼 free블록을 힙의 끝에 더해주기*/
      chop_block(pp, size); /*pp의 free 블록을 주어진 size단위의 작은 블록으로 쪼개기 */
    }
    if (pp == NULL) return NULL; /*할당 실패*/
  }

  pp = place(pp, size);/* * pp에 free 블록 할당, if 나머지 > MIN_BLOCK_SIZE(최소 블록 size (16바이트)) && 블록 split , return 할당한 블록 payload의 주소
  heap_check(__LINE__);
  return pp;
}

/*
 * ptr가 가르키는 할당된 블록을 free하고 coalsce (합쳐주기)
 * Free the allocated block at ptr, and coalesce it.
 */
void mm_free(void* ptr) {
  SET_ALLOC(ptr, 0); /*ptr블록의 header와 footer를 0bit으로 Set(설정)/
  link_block(ptr); /*ptr가 가르키는 블록을 free-list에 추가해주기 */
  coalesce(ptr); /* 현재블록과 인접한 free 블록들을 Coalesce (합쳐주기),합쳐진 블록의 주소 리턴*/
  heap_check(__LINE__);
}

/*
 * # ptr = NULL : allocate block of the given size. (if ptr가 NULL이라면 주어진 사이즈만큼 블록 할당)
 * # size = 0 : free the given block, return NULL. (if size가 0이라면 현재 블록 free시켜주고 null리턴하기)
 * # else: resize the allocated block at ptr. (else : 나머지 경우 ptr가 가르키는 할당된 블록 resize(크기 조정)하기)
 * 
 * Return address of the reallocated block, NULL on error. (return error ? NULL: 재할당된 블록의 주소)
 */
void* mm_realloc(void* ptr, size_t size) {
  if (ptr == NULL){ /*포인터기 널일 경우*/
    return mm_malloc(size); /*size 만큼 힐딩*/
  }else if (size == 0){ /*사이즈가 0이라면*/
    mm_free(ptr); /*포인터가 가르키는 블록 free해주기*/
    return NULL;
  }else{
    ptr = resize_block(ptr, size); /*그 외에는 포인터가 가르키는 블록의 사이즈 바꾸기*/
    heap_check(__LINE__);
    return ptr; /*크기를 수정한 재할당한 블록의 포인터턴리턴*/
  }
}

/* =========================== Private Functions ================================== */

/*
 * pp가 가르키는 블록의 크기를 size 바이트를 갖도록 resize (크기 조정)
 * 성공 시 사이즈 수정한 블록의 주소 return, 실패시 NULL return
 * 
 * Resize the allocated block at pp to have size bytes
 * Return address of the resized block, NULL on error.
 */
static void* resize_block(void* pp, size_t size) {
   /* asize =  [최소 블록 size (16바이트)]와 [size + ALIGNMENT의  곱에 가장 가까운 바이트 단위로 size를 올림해준 값(round up size)] 중에서 더 큰 값 */
  size_t asize = MAX(MIN_BLOCK_SIZE, ALIGN(size + DSIZE)); /*ALIGN(size + DSIZE)) == ((size+(size+바이트 단위의 Double word (8바이트))+(ALIGNMENT-1) & ~0x7)*/ 
  size_t bsize = BLOCK_SIZE(pp); /*bsize = payload pointer pp가 가르키는 payload 크기*/
  size_t csize = bsize - asize; /*pp포인터가 가르키는 payload크기 - size가 가질 수 있는 값들 중 큰 값*/

  if (asize > bsize) { /*payload size가 작을 경우*/
    if (can_expand(pp, asize)) return expand_block(pp, asize); /*pp위치에서 payload size만큼 확장해주고 해당 주소 리턴*/
    return reallocate_block(pp, size); /* * 주어진 size만큼 블록 할당해주고 content 복사 + 예전 블록 free해주기, return Error? NULL :새로운 블록 (재할당해준 새 블록) 주소*/
  }

  // Split only if the fragment is large enough.
  // fragment split은 크기가 최소 블록 size*4 (64바이트) 보다 크거나 같을 때만 해주기 
  if (csize >= (4 * MIN_BLOCK_SIZE)){ 
    SET_INFO(pp, asize, 1); /*블록 payload pointer pp가 가르키는 word주소의 size를 asize으로 설정& 1bit으로 설정(set)해주기*/
    void* fp = NEXT_BLOCK(pp); /*현재 payload pointer pp으로부터 다음 payload 블록 pointer 가져오기 */
    SET_INFO(fp, csize, 0);/*블록 payload pointer pp가 가르키는 word주소의 size를 asize으로 설정& 0bit으로 설정(set)해주기*/
    link_block(fp); /*ptr fp가 가르키는 블록을 free-list에 추가해주기 */
  }

  return pp;
}

/*
 * 주어진 size만큼 블록 할당해주고 content 복사 + 예전 블록 free해주기
 * return Error? NULL :새로운 블록 (재할당해준 새 블록) 주소
 * 
 * Allocate block of the given size, copy content, free old block
 * Return address of the new block, NULL on error.
 */
static void* reallocate_block(void* ptr, size_t size) {
  void *newptr = mm_malloc(size); /*새로운 블록 할당*/
  if (newptr == NULL) return NULL; /*새로 할당 실패 */
  size_t copy_size = MIN(PAYLOAD_SIZE(ptr), size); /*할당 성공 시  ptr가 가르키던 payload와 ptr의 블록 size 중 작은 값을 */
  memcpy(newptr, ptr, copy_size); /*새로운 포인터가 가르키는 블록에 메모리 복사 (복사받을 곳을 가르키는 포인터, 복사할 메모리를 가르키는 포인 , 복사할 데이터 값의 크기(바이트단위))*/
  mm_free(ptr); /*이전 복사를 받았던 곳을 가르키던 포인터 위치의 블록 free해주기*/
  return newptr;
}

/**
 * 포인터 pp가 가르키고 있는 할당된 블록이 size만큼 확장/늘릴 수 있는 지 체크
 * checks if the allocated-block at pp can expand to have the given size
 */
static int can_expand(void* pp, size_t size){
  size_t bsize = BLOCK_SIZE(pp); /*bsize = pp가 가르키는 블록의 크기*/

  for(void* ptr = NEXT_BLOCK(pp); IS_FREE(ptr) ; ptr = NEXT_BLOCK(ptr)){ /*포인터가 가르키는 블록이 free일 때까지 돌면서 */
    bsize += BLOCK_SIZE(ptr); /* 블록 size들을 누적시켜주기 */
    if (bsize >= size) return 1; /*블록의 크기가 할당하고자하는 size보다 크거나 같으면 1리턴*/
  }

  for(void* ptr = pp; !READ_ALLOC(MOVE_WORD(ptr, -2)) ; ){ /*header/footer word에서 ptr 주소를 -2만큼 옮겨준 ptr 가르키는 (할당된 비트)allocated-bit가 NULL일 때까지 읽기*/
    ptr = PREV_BLOCK(ptr);
    bsize += BLOCK_SIZE(ptr); /* 블록 size들을 누적시켜주기 */
    if (bsize >= size) return 1; /* 블록의 크기가 할당하고자하는 size보다 크거나 같으면 1리턴 */
  }

  return 0; /* 확장가불가 */
}

/**
 * 
 * 주어진 size가 될때까지 pp포인터가 가르키는 할당블록 확장(늘려주기)
 * 새로 확장해준 블록의 주소 리턴
 * 
 * expands the allocated-block at pp until it has the given size
 * return address to the new expanded block
*/
static void* expand_block(void *pp, size_t size) {
  void* cpp = pp;
  size_t bsize = BLOCK_SIZE(pp); /*pp 포인터가 가르키는 블록의 크기*/

  for(void* ptr = NEXT_BLOCK(pp); IS_FREE(ptr) ; ptr = NEXT_BLOCK(ptr)){ /*포인터가 가르키는 블록이 free일 때까지 블록들을 돌면서 */
    bsize += BLOCK_SIZE(ptr); /*블록의 사이즈들 누적해주기*/
    unlink_block(ptr); /* free 리스트에서 ptr가 가르키고 있는 블록 제거 */
    if (bsize >= size) break; /*size만큼 확장해줬다면 break*/
  }

  if (bsize >= size) { /*블록의 크기가 size보다 크거나 같으면 확장해줄 필요 없이 cpp포인터가 가르키는 주소의 블록에 size는 bsize만큼, allocated-bit은 로로 set*/
    SET_INFO(cpp, bsize, 1);
    return cpp;
  }

  for(void* ptr = pp; !READ_ALLOC(MOVE_WORD(ptr, -2)) ; ){ /*header/footer word에서 ptr 주소를 -2만큼 옮겨준 ptr 가르키는 (할당된 비트)allocated-bit가 NULL일 때까지 읽기*/
    cpp = ptr = PREV_BLOCK(ptr); 
    bsize += BLOCK_SIZE(ptr); /*ptr의 블록 사이즈만큼 bsize 누적해주기 */
    unlink_block(ptr); /*free 리스트에서 ptr가 가르키고 있는 블록 제거*/
    if (bsize >= size) break; /*size만큼 확장해줬다면 break*/
  }
   
  if (cpp != pp) memmove(cpp, pp, PAYLOAD_SIZE(pp)); /*cpp와 pp가 다를 떄 pp를 pp의 payload 크기만큼의 character을 cpp로 복사*/
  SET_INFO(cpp, bsize, 1); /*cpp 포인터가 가르키는 주소공간에 bsize만큼 블록의 크기 set & allocated bit ==1로 set*/
  return cpp;
}

/**
 * 주어진 free 블록을 주어진 size단위의 작은 블록으로 쪼개기 
 * chop the given free-block into a small free-blocks of the given size.
*/
static void chop_block(void* pp, size_t size){
  if ((pp == NULL) || (size < MIN_BLOCK_SIZE)) return;
  size_t bsize = BLOCK_SIZE(pp);
  if ((size + MIN_BLOCK_SIZE) > bsize) return;
  unlink_block(pp);

  while(bsize >= (size + MIN_BLOCK_SIZE)){ /*블록의 크기가 최소블록크기(16바이트)+size 보다 크거나 같을 동안 free list에 pp가 가르키는 블록 추가해주기*/
    SET_INFO(pp, size, 0);
    link_block(pp);
    pp = NEXT_BLOCK(pp);
    bsize -= size; 
  }

  SET_INFO(pp, bsize, 0);
  link_block(pp);
}

/**
 * 힙의 끝에 size가 aligned(정렬)된 free 블록 더해주기
 * return error? NULL : 새로 free해줘서 한한 블록의 주소
 * 
 * Add free block with aligned size to the end of the heap.
 * Return address of the added free block, NULL on error.
*/
void* extend_heap(size_t size) {
  WTYPE* pp;
  size = ALIGN(size);
  if ((long)(pp = mem_sbrk(size)) == -1) return NULL;

  SET_INFO(pp, size, 0);                      /* Initialize a free block */
  link_block(pp);
  WRITE(HEADER(NEXT_BLOCK(pp)), 0, 1);        /* New Tail Word */

  return pp;
}

/*
 * free list에서 size보다 크거나 같은 첫 블록을 찾아준다.  
 * no-fit ? NULL : 찾은 곳의 주소
 * Find the first block greater than or equal to size
 * Return address of the first-fit, NULL if no-fit.
*/
static void* first_fit(void* free_list, size_t size) {
  for (void* pp = free_list; pp != NULL ; pp = GET_SUCC(pp)) {
    if (size <= BLOCK_SIZE(pp)) return pp;
  }
  return NULL;
}

/* 
 * size와 크기가 같거나 큰 free list의 블록 중에서 가장 작은 블록 찾아줌
 * Return no-fit? NULL : 찾은 곳의 주소
 * 
 * Find the smallest block greater than or equal to size
 * Return address of the best-fit, NULL if no-fit.
*/
static void* best_fit(void* free_list, size_t size) {
  void* pp;
  void* best = NULL;
  size_t best_size = __SIZE_MAX__;

  for (pp = free_list; pp != NULL ; pp = GET_SUCC(pp)) {
    size_t curr_size = BLOCK_SIZE(pp);
    if ((size <= curr_size) && (curr_size < best_size)){
      best = pp;
    }
  }

  return best;
}

/**
 * size와 같거나 큰 free 블록을 찾아줌
 * Return no-fit? NULL리턴 : 찾은 블록 주소
 * 
 * Find a free block with size greater than or equal to size.
 * Return address of a fit-block, NULL if no fit.
*/
static void* find_fit(size_t size) {
  for(int i = seg_index(size); i < SEGLIST_NUM; ++i){
    void* fit = best_fit(seglist[i], size);
    if (fit != NULL) return fit;
  }
  return NULL;
}

/**
 * pp가 가르키는 위치에 free 블록 할당
 * 나머지 > MIN_BLOCK_SIZE(최소 블록 size (16바이트))라면 블록 split 
 * 할당한 블록 payload의 주소를 return
 *
 * Allocate the free block at pp.
 * Split the block if the remainder is greater than MIN_BLOCK_SIZE.
 * Returns the address of the allocated block payload
*/
static void* place(void *pp, size_t size) {
  size_t bsize = BLOCK_SIZE(pp);
  size_t csize = bsize - size;

  unlink_block(pp);
  if (csize < MIN_BLOCK_SIZE){
    SET_ALLOC(pp, 1);
  }else{
    SET_INFO(pp, csize, 0);
    link_block(pp);
    pp = NEXT_BLOCK(pp);
    SET_INFO(pp, size, 1);
  }

  return pp;
}

/**
 * 현재블록과 인접한 free 블록들을 Coalesce (합쳐주기)
 * 합쳐진 블록의 주소 리턴
 * Coalesce the current block with its free previous and/or next blocks.
 * Return the address of the coalesced block.
*/
static void* coalesce(void *pp) {
  void* cpp = pp;                            /* coalesced payload pointer */
  void* prev_footer = MOVE_WORD(pp, -2);
  void* next_header = HEADER(NEXT_BLOCK(pp));
  
  size_t prev_alloc = READ_ALLOC(prev_footer);
  size_t next_alloc = READ_ALLOC(next_header);
  size_t curr_alloc = !IS_FREE(pp);
  size_t size = BLOCK_SIZE(pp);

  if (prev_alloc && next_alloc) return pp;

  if (!curr_alloc) unlink_block(pp);

  if (!next_alloc) {
    size += READ_SIZE(next_header);
    unlink_block(MOVE_WORD(next_header, 1));
  }

  if (!prev_alloc) {
    size += READ_SIZE(prev_footer);
    cpp = PREV_BLOCK(pp);
    unlink_block(cpp);
    if (curr_alloc) memmove(cpp, pp, PAYLOAD_SIZE(pp));
  } 

  SET_INFO(cpp, size, curr_alloc);
  if (!curr_alloc) link_block(cpp);

  return cpp;
}

/**
 * pp 포인터가 가르키고 있는 free-list 주소에 블록 삽입
 *
 * Add the block at pp to the free-list
*/
static void link_block(void* pp){
  int index = seg_index(BLOCK_SIZE(pp));
  WTYPE* list = seglist[index];
  if (list) SET_PRED(list, pp);
  SET_SUCC(pp, list);
  SET_PRED(pp, NULL);
  seglist[index] = pp;
}

/**
 * pp 포인터가 가르키고 있는 free-list에 블록 삭제
 * Remove the block at pp from the free-list 
*/
static void unlink_block(void* pp) {
  int index = seg_index(BLOCK_SIZE(pp));
  WTYPE* pred_pp = GET_PRED(pp);
  WTYPE* succ_pp = GET_SUCC(pp);
  if (pred_pp) SET_SUCC(pred_pp, succ_pp);
  if (succ_pp) SET_PRED(succ_pp, pred_pp);
  if (pp == seglist[index]) seglist[index] = succ_pp;
}

/**
 * size를 포함하고 있을 블록의 seglist 인덱스 반환 (seglist --> 각 size마다 갖고 있는 freelist가 다른 점을 활용해서 빠르게 가장 적절한 크기의 free블록 찾는 거임!) 
 * Returns the index of the seglist that should contain blocks of the given size 
*/
static int seg_index(size_t size){
  if (size <= MIN_BLOCK_SIZE) return 0;
  if (size <= (2 * MIN_BLOCK_SIZE)) return 1;
  if (size <= (4 * MIN_BLOCK_SIZE)) return 2;
  if (size <= (8 * MIN_BLOCK_SIZE)) return 3;
  if (size <= (16 * MIN_BLOCK_SIZE)) return 4;
  if (size <= (64 * MIN_BLOCK_SIZE)) return 5;
  if (size <= (256 * MIN_BLOCK_SIZE)) return 6;
  return 7;
}

/* ========================== Debugging Functions [디버깅 함수] =============================== */

#ifdef DEBUG
/** 
 * 힙 일관성 체크 : 힙에 있는 head와 tail의 word 개수, 블록 내부 header와 footer가 균등하게 있는지 , 블록의 크기>= 최소 블록 size (16바이트)인지, payload  
 * Heap Consistency Checker, checks for:
 * - head and tail words of the heap
 * - block header and footer equality
 * - block size >= minimum size
 * - payload alignment
 * - coalescing (no contiguous free blocks)
 * - total heap size
 * - seglist consistency.
*/
static void mm_check(int line){
  WTYPE* ptr = MOVE_WORD(mem_heap_lo(), SEGLIST_NUM);
  WTYPE* end_ptr = MOVE_BYTE(ptr, mem_heapsize()) - (SEGLIST_NUM + 1); 
  // Check head word (size = 0, allocated)
  if (READ_SIZE(ptr) != 0){
    printf("Error at %d: head-word size = %u\n",line, READ_SIZE(ptr));
  }

  if (READ_ALLOC(ptr) != 1){
    printf("Error at %d: head-word is not allocated\n",line);
  }

  // Check tail word (size = 0, allocated)
  if (READ_SIZE(end_ptr) != 0){
    printf("Error at %d: tail-word size = %u\n",line, READ_SIZE(end_ptr));
  }

  if (READ_ALLOC(end_ptr) != 1){
    printf("Error at %d: tail-word is not allocated\n",line);
  }

  size_t heap_size = (SEGLIST_NUM + 2) * WSIZE;
  int free_count = 0;
  int prev_free = 0;

  // Check regular blocks
  for (ptr = MOVE_WORD(ptr, 2); ptr < end_ptr; ptr = NEXT_BLOCK(ptr)){
    // check header and footer equality
    if (READ_WORD(HEADER(ptr)) != READ_WORD(FOOTER(ptr))){
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
      printf("NOTE at %d: two contiguous free blocks not coalesced\n", line);
    }

    if(IS_FREE(ptr)) ++free_count;
    prev_free = IS_FREE(ptr);
    heap_size += BLOCK_SIZE(ptr);
  }
  // check total heap size
  if (heap_size != mem_heapsize()){
    printf("Error at %d: heap size not accurate, %u should be %u\n",
     line, heap_size, mem_heapsize());
  }
  // check seglist consistency
  check_seglist(line, free_count);
}

/** 
 * 세그리트 일관성 체크, 체크 항목: 세그리스트 포인트, 각 free 리스트에서의 일관성, (힙에 있는 free블록) seglist의 free-block개수가 free_count와 일치하는 지
 * 
 * Seglist Consistency Checker, checks for:
 * - seglist pointer
 * - each free-list consistency
 * - the free-block count in seglist matches the free_count (free-blocks in the heap)
*/
static void check_seglist(int line, int free_count){
  int count = 0;
  // checks the seglist pointer
  if (seglist != mem_heap_lo()){
    printf("Error at %d: Seglist pointer doesn't point to heap start address.\n",
      line);
  }
  // check each free-list in the seglist
  for(int i = 0; i < SEGLIST_NUM; ++i){
    count += check_free_list(line, i);
  }
  // check the free-block count in seglist matches the free_count
  if (count != free_count){
    printf("Error at %d: %d missing free-blocks from the seglist.\n",
      line, (free_count - count));
  }
}

/** 
 * free list 일관성 검사 : 검사 항목 = free인 블록들, 힙 영역에 free 블록 유무, predecessor의 일관성
 * 
 * free-list에 있는 블록의 개수 
 * 
 * Free-List Consistency Checker, checks for:
 * - blocks are free.
 * - free-blocks are in heap range.
 * - the predecessor consistency.
 * Return the number of blocks in the free-list
*/
static int check_free_list(int line, int li){
  void* start_ptr = MOVE_WORD(mem_heap_lo(), SEGLIST_NUM);
  void* end_ptr = MOVE_BYTE(start_ptr, mem_heapsize()) - (SEGLIST_NUM + 1); 
  void* pred_pp = NULL;
  int count = 0;

  for(void* pp = seglist[li]; pp != NULL; pp = GET_SUCC(pp)){
    // check if block is free
    if (!IS_FREE(pp)){
      printf("Error at %d: Seglist[%d] contains an allocated-block %p.\n",
        line, li, pp);
    }
    // check if free-block in heap range
    if (pp <= start_ptr || pp >= end_ptr){
      printf("Error at %d: Seglist[%d] contains a free-block %p out of the heap range.\n",
        line, li, pp);
    }
    // check the predecessor pointer
    if (pred_pp != GET_PRED(pp)){
      printf("Error at %d: in Seglist[%d], inconsistant predecessor link at %p.\n",
        line, li, pp );
    }

    ++count;
    pred_pp = pp;
  }

  return count;
}

#endif
