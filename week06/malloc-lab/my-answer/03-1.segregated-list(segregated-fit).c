// Perf index = 46 (util) + 7 (thru) = 53/100

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
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define WSIZE 4              
#define DSIZE 8              
#define CHUNKSIZE (1 << 12)  

#define MAX(x, y) (x > y ? x : y)

#define PACK(size, alloc) (size | alloc)                              
#define GET(p) (*(unsigned int *)(p))                                 
#define PUT(p, val) (*(unsigned int *)(p) = (unsigned int)(val))      
#define GET_SIZE(p) (GET(p) & ~0x7)                                   
#define GET_ALLOC(p) (GET(p) & 0x1)                                   
#define HDRP(bp) ((char *)(bp)-WSIZE)                                 
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)          
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE))) 
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))   

static char *heap_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

#define GET_SUCC(bp) (*(void **)((char *)(bp) + WSIZE)) // 다음 가용 블록의 주소
#define GET_PRED(bp) (*(void **)(bp))                   // 이전 가용 블록의 주소
static void splice_free_block(void *bp);                // 가용 리스트에서 제거
static void add_free_block(void *bp);                   // 가용 리스트에 추가

#define SEGREGATED_SIZE (12)                            // segregated list의 class 개수
#define GET_ROOT(class) (*(void **)((char *)(heap_listp) + (WSIZE * class)))
static int get_class(size_t size);


int mm_init(void) {
    // 8워드 크기의 힙 생성, heap_listp에 힙의 시작 주소값 할당(가용 블록만 추적)
    if ((heap_listp = mem_sbrk((SEGREGATED_SIZE + 4) * WSIZE)) == (void *)-1) { 
        return -1;
    }
    PUT(heap_listp, 0);                                                                         // 정렬 패딩
    PUT(heap_listp + (1 * WSIZE), PACK((SEGREGATED_SIZE + 2) * WSIZE, 1));                      // 프롤로그 Header (크기: 헤더 1 + 푸터 1 + segregated list 크기)
    for (int i = 0; i < SEGREGATED_SIZE; i++) {
        PUT(heap_listp + ((2 + i) * WSIZE), NULL);
    }
    PUT(heap_listp + ((SEGREGATED_SIZE + 2) * WSIZE), PACK((SEGREGATED_SIZE + 2) * WSIZE, 1));  // 프롤로그 Footer
    PUT(heap_listp + ((SEGREGATED_SIZE + 3) * WSIZE), PACK(0, 1));                              // 에필로그 Header: 프로그램이 할당한 마지막 블록의 뒤에 위치
    heap_listp += (2 * WSIZE);
    
    if (extend_heap(4) == NULL) {
        return -1;
    }
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

void *mm_malloc(size_t size) {
    size_t asize;      // 조정된 블록 사이즈
    size_t extendsize; // 확장할 사이즈
    char *bp;

    if (size == 0) {
        return NULL;
    }

    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);
    }

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

// 기존에 할당된 메모리 블록의 크기 변경
// `기존 메모리 블록의 포인터`, `새로운 크기`
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

    size_t copySize = GET_SIZE(HDRP(ptr)) - DSIZE;
    if (size < copySize) {
        copySize = size;
    }

    memcpy(newptr, ptr, copySize);
    mm_free(ptr);
    return newptr;
}

static void *extend_heap(size_t words) {
    char *bp;

    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));         // 새 빈 블록 헤더 초기화
    PUT(FTRP(bp), PACK(size, 0));         // 새 빈 블록 푸터 초기화
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 에필로그 블록 헤더 초기화

    return coalesce(bp); // 병합 후 리턴 블록 포인터 반환
}

static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록 할당 상태
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록 할당 상태
    size_t size = GET_SIZE(HDRP(bp));                   // 현재 블록 사이즈

    if (prev_alloc && next_alloc) {
        add_free_block(bp);                         // free_list에 추가
        return bp;                              
    } else if (prev_alloc && !next_alloc) {
        splice_free_block(NEXT_BLKP(bp));           // 가용 블록을 free_list에서 제거
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        splice_free_block(PREV_BLKP(bp));           // 가용 블록을 free_list에서 제거
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));    // 이전 블록 헤더 재설정
        PUT(FTRP(bp), PACK(size, 0));               // 현재 블록 푸터 재설정
        bp = PREV_BLKP(bp);                         // 이전 블록의 시작점으로 포인터 변경
    } else {
        splice_free_block(PREV_BLKP(bp));           // 이전 가용 블록을 free_list에서 제거
        splice_free_block(NEXT_BLKP(bp));           // 다음 가용 블록을 free_list에서 제거
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));    // 이전 블록 헤더 재설정
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));    // 다음 블록 푸터 재설정
        bp = PREV_BLKP(bp);                         // 이전 블록의 시작점으로 포인터 변경
    }
    add_free_block(bp);                             // 현재 병합한 가용 블록을 free_list에 추가
    return bp;                                      // 병합한 가용 블록의 포인터 반환
}

static void *find_fit(size_t asize) {
    int class = get_class(asize);
    void *bp = GET_ROOT(class);

    while (class < SEGREGATED_SIZE) {               // 현재 탐색하는 클래스가 범위 안에 있는 동안 반복
        bp = GET_ROOT(class);
        while (bp != NULL) {
            if ((asize <= GET_SIZE(HDRP(bp)))) {    // 적합한 사이즈의 블록을 찾으면 반환
                return bp;
            }
            bp = GET_SUCC(bp);                      // 다음 가용 블록으로 이동
        }
        class += 1;
    }
    return NULL;
}

static void place(void *bp, size_t asize) {
    splice_free_block(bp);                          // free_list에서 해당 블록 제거

    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE)) {           // 차이가 최소 블록 크기 16보다 같거나 크면 분할
        PUT(HDRP(bp), PACK(asize, 1));              // 현재 블록에는 필요한 만큼만 할당
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp), PACK((csize - asize), 0));    // 남은 크기를 다음 블록에 할당(가용 블록)
        PUT(FTRP(bp), PACK((csize - asize), 0));
        add_free_block(bp);                         // 남은 블록을 free_list에 추가
    } else {
        PUT(HDRP(bp), PACK(csize, 1));              // 해당 블록 전부 사용
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

// 가용 리스트에서 bp에 해당하는 블록을 제거하는 함수
static void splice_free_block(void *bp) {
    int class = get_class(GET_SIZE(HDRP(bp)));
    if (bp == GET_ROOT(class)) {                        // 분리하려는 블록이 free_list 맨 앞에 있는 블록이면 (이전 블록이 없음)
        GET_ROOT(class) = GET_SUCC(GET_ROOT(class));    // 다음 블록을 루트로 변경
        return;
    }
    // 이전 블록의 SUCC을 다음 가용 블록으로 연결
    GET_SUCC(GET_PRED(bp)) = GET_SUCC(bp);
    // 다음 블록의 PRED를 이전 블록으로 변경
    if (GET_SUCC(bp) != NULL) {                         // 다음 가용 블록이 있을 경우만
        GET_PRED(GET_SUCC(bp)) = GET_PRED(bp);
    }
}

// 적합한 가용 리스트를 찾아서 맨 앞에 현재 블록을 추가하는 함수
static void add_free_block(void *bp) {
    int class = get_class(GET_SIZE(HDRP(bp)));
    GET_SUCC(bp) = GET_ROOT(class);                 // bp의 해당 가용 리스트의 루트가 가리키던 블록
    if (GET_ROOT(class) != NULL) {                  // list에 블록이 존재했을 경우만
        GET_PRED(GET_ROOT(class)) = bp;             // 루트였던 블록의 PRED를 추가된 블록으로 연결
    }
    GET_ROOT(class) = bp;
}

// 적합한 가용 리스트를 찾는 함수
int get_class(size_t size) {
    if (size < 16) {    // 최소 블록 크기는 16바이트
        return -1;      // 잘못된 크기
    }

    // 클래스별 최소 크기
    size_t class_sizes[SEGREGATED_SIZE];
    class_sizes[0] = 16;

    // 주어진 크기에 적합한 클래스 검색
    for (int i = 0; i < SEGREGATED_SIZE; i++) {
        if (i != 0) {
            class_sizes[i] = class_sizes[i - 1] << 1;
        }
        if (size <= class_sizes[i]) {
            return i;
        }
    }

    // 주어진 크기가 8192바이트 이상인 경우, 마지막 클래스로 처리
    return SEGREGATED_SIZE - 1;
}