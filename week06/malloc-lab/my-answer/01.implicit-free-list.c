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
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7) // size 8를 ALIGNMENT의 배수로 반올림
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))             // size_t의 크기를 정렬된 값으로 정의?

/* 기본 상수 & 매크로 */
#define WSIZE 4                             // word size
#define DSIZE 8                             // double word size
#define CHUNKSIZE (1<<12)                   // 힙 확장을 위한 기본 크기 (= 초기 빈 블록 크기)
#define MAX(x,y) ((x) > (y) ? (x): (y))

/* 가용 리스트를 접근/순회하는 데 사용할 매크로 */
#define PACK(size, alloc) (size | alloc)                                    // size와 할당 비트를 결합, header와 footer에 저장할 값
#define GET(p) (*(unsigned int *)(p))                                       // p가 참조하는 워드 반환 (포인터라서 직접 역참조 불가능 -> 타입 캐스팅)
#define PUT(p, val) (*(unsigned int *)(p) = (unsigned int)(val))            // p에 val 저장
#define GET_SIZE(p) (GET(p) & ~0x7)                                         // 사이즈 (~0x7: ...11111000, '&' 연산으로 뒤에 세자리 없어짐)
#define GET_ALLOC(p) (GET(p) & 0x1)                                         // 할당 상태
#define HDRP(bp) ((char *)(bp) - WSIZE)                                     // Header 포인터
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)                // Footer 포인터 (Header의 정보를 참조해서 가져오기 때문에, Header의 정보를 변경했다면 변경된 위치의 Footer가 반환됨에 유의)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))     // 다음 블록의 포인터
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))     // 이전 블록의 포인터

/* 전역 힙 변수 및 함수 선언 */
static char *heap_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

                                    // util : 메모리 사용량 / thru : 속도
// #define FIRST_FIT                // Perf index = 44 (util) + 7 (thru) = 52/100
// #define NEXT_FIT                 // Perf index = 43 (util) + 15 (thru) = 58/100
static char *last_allocated;
#define BEST_FIT                    // Perf index = 45 (util) + 7 (thru) = 52/100

int mm_init(void) {

    // 4워드 크기의 힙 생성, heap_listp에 힙의 시작 주소값 할당
    if((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) - 1) {
        return -1;
    };
        
    PUT(heap_listp, 0);                                 // Alignment padding (4bytes)
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));      // Prologue header (4bytes)
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));      // Prologue footer (4bytes)
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));          // Epilogue header
    heap_listp += (2 * WSIZE); 

    // 힙을 CHUNKSIZE bytes로 확장
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {   // 4096 / 4
        return -1;
    }

    return 0;
}

static void *extend_heap(size_t words) {
    char *bp;
    
    // 2워드의 가장 가까운 배수로 반올림 (홀수면 1 더해서 곱함)
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
        last_allocated = bp;
        return bp;
    } else if(prev_alloc && !next_alloc) {                  // Case 2 : 다음 블록이 빈 경우
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if(!prev_alloc && next_alloc) {                  // Case 3 : 이전 블록이 빈 경우
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {                                                // Case 4 : 이전 블록과 다음 블록이 모두 빈 경우
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    last_allocated = bp;

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
    void *bp;
    
    #ifdef FIRST_FIT
        for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
            if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) { // 가용 상태 & 사이즈 적합
                return bp;
            }
        }
    #elif defined(NEXT_FIT)
        if (last_allocated == NULL) {
            last_allocated = heap_listp;
        }

        for (bp = last_allocated; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
            if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
                last_allocated = bp;
                return bp;
            }
        }

        for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0 && last_allocated > bp; bp = NEXT_BLKP(bp)) {
            if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
                last_allocated = bp;
                return bp;
            }
        }
    #elif defined(BEST_FIT)
        void *best_fit = NULL;

        for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
            if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
                // 기존에 할당하려던 공간보다 더 최적의 공간이 나타났을 경우 리턴 블록 포인터 갱신
                if(!best_fit || GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(best_fit))) {
                    best_fit = bp;
                }
            }
        }
        return best_fit;
    #else
    #endif


    return NULL;
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));          // 현재 블록의 크기

    if((csize - asize) >= (2 * DSIZE)) {        // 차이가 최소 블록 크기 16보다 같거나 크면 분할
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));


    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
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
