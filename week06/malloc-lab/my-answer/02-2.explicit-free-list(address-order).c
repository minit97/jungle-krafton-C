// explicit-free-list(address-order)    : Perf index = 46 (util) + 10 (thru) = 56/100

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    "jungle-krafton",
    "Minit",
    "dndb4902@naver.com",
    "",
    ""
};

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define MAX(x,y) ((x) > (y) ? (x): (y))
#define PACK(size, alloc) (size | alloc)
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (unsigned int)(val))
#define GET_SIZE(p) (GET(p) & ~0x7)         
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

static char *heap_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/* 명시적 가용 리스트 */
#define GET_SUCC(bp) (*(void **)((char *)(bp) + WSIZE))     // 다음 가용 블록의 주소
#define GET_PRED(bp) (*(void **)(bp))                       // 이전 가용 블록의 주소
static void splice_free_block(void *bp);
static void add_free_block(void *bp);


int mm_init(void) {
    if((heap_listp = mem_sbrk(8 * WSIZE)) == (void *) - 1) {
        return -1;
    }
    PUT(heap_listp, 0);                                     // 정렬 패딩
    PUT(heap_listp + (1 * WSIZE), PACK(2 * WSIZE, 1));      // 프롤로그 Header
    PUT(heap_listp + (2 * WSIZE), PACK(2 * WSIZE, 1));      // 프롤로그 Footer
    PUT(heap_listp + (3 * WSIZE), PACK(4 * WSIZE, 0));      // 첫 가용 블록의 헤더
    PUT(heap_listp + (4 * WSIZE), NULL);                    // 이전 가용 블록 주소
    PUT(heap_listp + (5 * WSIZE), NULL);                    // 이후 가용 블록 주소
    PUT(heap_listp + (6 * WSIZE), PACK(4 * WSIZE, 0));      // 첫 가용 블록 푸터
    PUT(heap_listp + (7 * WSIZE), PACK(0, 1));              // 에필로그 Header
    heap_listp += (4 * WSIZE);

    if(extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }

    return 0;
}

static void *extend_heap(size_t words) {
    char *bp;
    
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));           // free 블록의 header
    PUT(FTRP(bp), PACK(size, 0));           // free 블록의 footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // new epilogue header

    return coalesce(bp);
}

static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));     // 이전 블록의 할당 여부
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));     // 이후 블록의 할당 여부
    size_t size = GET_SIZE(HDRP(bp));                       // 현재 블록의 사이즈


    if (prev_alloc && next_alloc) {                         // Case 1 : 모두 할당되었을 때
        add_free_block(bp);
        return bp;
    } else if(prev_alloc && !next_alloc) {                  // Case 2 : 다음 블록이 빈 경우
        splice_free_block(NEXT_BLKP(bp)); // 가용 블록을 free_list에서 제거
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        add_free_block(bp);
    } else if(!prev_alloc && next_alloc) {                  // Case 3 : 이전 블록이 빈 경우
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {                                                // Case 4 : 이전 블록과 다음 블록이 모두 빈 경우
        splice_free_block(NEXT_BLKP(bp));                   // 다음 가용 블록을 free_list에서 제거
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;  // 병합된 블록의 포인터 반환
}

void *mm_malloc(size_t size) {
    char *bp;

    if (size == 0) {
        return NULL;
    }

    size_t asize;           // 조정된 블록 사이즈
    if (size <= DSIZE) {
        asize = 2 * DSIZE;  // 최소 블록 크기 16 바이트 할당 (헤더4 + 푸터4 + 저장공간 8)
    }else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);   // 8배수로 올림처리
    }

    
    // 가용 블록 검색
    if((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // 확장할 블록 사이즈
    size_t extendsize = MAX(asize, CHUNKSIZE);  
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);

    return bp;
}

static void *find_fit(size_t asize){
   // void *bp;
    
    // for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = GET_SUCC(bp)) {
    //     if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
    //         return bp;
    //     }
    // }
    void *bp = heap_listp;
    while (bp != NULL) {                        // 다음 가용 블럭이 있는 동안 반복
    
        if ((asize <= GET_SIZE(HDRP(bp)))) {    // 적합한 사이즈의 블록을 찾으면 반환
            return bp;
        }
        bp = GET_SUCC(bp);                      // 다음 가용 블록으로 이동
    }
    
    return NULL;
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));              // 현재 블록의 크기

    if((csize - asize) >= (2 * DSIZE)) {            // 차이가 최소 블록 크기 16보다 같거나 크면 분할
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));

        GET_SUCC(bp) = GET_SUCC(PREV_BLKP(bp));     // 루트였던 블록의 PRED를 추가된 블록으로 연결
        if (PREV_BLKP(bp) == heap_listp) {
            heap_listp = bp;
        } else {
            GET_PRED(bp) = GET_PRED(PREV_BLKP(bp));
            GET_SUCC(GET_PRED(PREV_BLKP(bp))) = bp;
        }

        if (GET_SUCC(bp) != NULL) {                 // 다음 가용 블록이 있을 경우만
            GET_PRED(GET_SUCC(bp)) = bp;
        }

    } else {
        splice_free_block(bp);
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

// 가용 리스트에서 bp에 해당하는 블록을 제거하는 함수
static void splice_free_block(void *bp) {   

    // 분리하려는 블록이 free_list 맨 앞에 있는 블록이면 (이전 블록이 없음)
    if (bp == heap_listp) {
        heap_listp = GET_SUCC(heap_listp); // 다음 블록을 루트로 변경
        return;
    }
    // 이전 블록의 SUCC을 다음 가용 블록으로 연결
    GET_SUCC(GET_PRED(bp)) = GET_SUCC(bp);
    // 다음 블록의 PRED를 이전 블록으로 변경
    if (GET_SUCC(bp) != NULL) {  // 다음 가용 블록이 있을 경우만
        GET_PRED(GET_SUCC(bp)) = GET_PRED(bp);
    }
}

// 가용 리스트에서 주소 오름차순에 맞게 현재 블록을 추가하는 함수
static void add_free_block(void *bp) {
    void *currentp = heap_listp;
    if (currentp == NULL) {
        heap_listp = bp;
        GET_SUCC(bp) = NULL;
        return;
    }

    // 검사중인 주소가 추가하려는 블록의 주소보다 작을 동안 반복
    while (currentp < bp)  {
        if (GET_SUCC(currentp) == NULL || GET_SUCC(currentp) > bp) {
            break;
        }
        currentp = GET_SUCC(currentp);
    }

    GET_SUCC(bp) = GET_SUCC(currentp);      // 루트였던 블록의 PRED를 추가된 블록으로 연결
    GET_SUCC(currentp) = bp;                // bp의 SUCC은 루트가 가리키던 블록
    GET_PRED(bp) = currentp;                // bp의 SUCC은 루트가 가리키던 블록

    // 다음 가용 블록이 있을 경우만
    if (GET_SUCC(bp) != NULL) {
        GET_PRED(GET_SUCC(bp)) = bp;
    }
}

void mm_free(void *ptr) {
    size_t size = GET_SIZE(HDRP(ptr));
		
    // 헤더와 푸터를 0으로 할당하고 coalesce를 호출하여 가용 메모리를 이어준다.
    PUT(HDRP(ptr), PACK(size, 0)); 
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}


// 기존에 할당된 메모리 블록의 크기 변경    // '기존 메모리 블록 포인터', '새로운 크기'
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    if (size <= 0) {
        mm_free(ptr);
        return 0;
    }
    
    void *newptr = mm_malloc(size);
    if (newptr == NULL) {
      return NULL;
    }

    size_t copySize = GET_SIZE(HDRP(ptr)) - DSIZE;  //payload만큼 복사
    if (size < copySize) {
      copySize = size;
    }

    memcpy(newptr, ptr, copySize);   // 새 블록으로 데이터 복사
    mm_free(ptr);
    return newptr;
}
