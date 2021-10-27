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
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/////////////////////////////////////////////////////////////////////////////
/* Basic contants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes)*/
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x)>(y)?(x):(y))

/* Pack a size and allocated bit into a word */
/* 将大小和已分配位结合起来组成头部或者脚部 */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)      (*(unsigned int *)(p)) /* 读取和返回参数p引用的字 */
#define PUT(p,val)  (*(unsigned int *)(p) = (val)) /* 将val存放在参数p指向的字中 */

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)    ((char *)(bp)-WSIZE)
#define FTRP(bp)    ((char *)(bp)+GET_SIZE(HDRP(bp))-DSIZE)

/* Gvien block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char *)(bp)+GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))
///////////////////////////////////////////////////////////////////////////////



//函数声明
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void *best_fit(size_t asize);
static void place(void *bp, size_t asize);

//私有全局变量
static char *heap_listp = 0;

//./mdriver -t ./traces -V

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* 创建初始化的空heap */
    if((heap_listp = mem_sbrk(4*WSIZE))==(void *)-1)
        return -1;
    PUT(heap_listp, 0);                            /* 对齐（蓝色） alignment padding*/
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));   /* 序言头（蓝色） prologue header*/
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));   /* 序言脚（蓝色） prologue footer*/
    PUT(heap_listp + (3*WSIZE), PACK(0,1));        /* 结尾块（蓝色） epilogue header (普通快1)*/
    heap_listp +=(2*WSIZE); /*指向真正的头 见书 图 9-42*/

    if(extend_heap(CHUNKSIZE/WSIZE)==NULL){
        return -1;
    }
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;       //合适的块大小
    size_t extendsize;  //如果不匹配，则需要的额外heap
    char *bp;

    //忽略
    if(size ==0){
        return NULL;
    }

    //调整包括额外开销和对齐需求的块大小
    if(size <= DSIZE){
        asize = 2*DSIZE;
    }
    else{
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) /(DSIZE));
    }

    //搜索空闲块来适配
    if((bp = best_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }    

    //如果不匹配
    extendsize = MAX(asize,CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp, asize);
    return bp;
    
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    
    if(bp == 0)
        return;
    
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
    
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize = GET_SIZE(HDRP(ptr));
    void *new_ptr;
    size_t asize;
    size_t rsize;
    /*
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
    */
    if(ptr == NULL){
        return mm_malloc(size);
    }
    if(size == 0){
        mm_free(ptr);
        return 0;
    }

    //调整包括额外开销和对齐需求的块大小
    if(size <= DSIZE){
        asize = 2*DSIZE;
    }
    else{
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) /(DSIZE));
    }
    //合并一次，提高利用率
    new_ptr = coalesce(ptr);
    oldsize = GET_SIZE(HDRP(new_ptr));
	PUT(HDRP(new_ptr), PACK(oldsize, 1));
	PUT(FTRP(new_ptr), PACK(oldsize, 1));
    if(new_ptr != ptr){
        memcpy(new_ptr, ptr, GET_SIZE(HDRP(ptr)) - DSIZE);
    }

    //比较新旧size大小
    if(oldsize == asize){
        return ptr;
    }
    else if(oldsize > asize){
        place(new_ptr,asize);
        return new_ptr;
    }
    else{
        //开辟新空间
        new_ptr = mm_malloc(asize);
        memcpy(new_ptr, ptr, asize);
        mm_free(ptr);
        return new_ptr;

    }

}


static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // Allocate an even number of words to maintain alignment
    // 保持对齐
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == (void *)-1){
        return NULL;
    }
        
    // Initialize free block header/footer and the epilpgue header
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); //获取 前一个块的 脚部 分配位
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); //获取 后一个块的 头部 分配位
    size_t size = GET_SIZE(HDRP(bp));   //获取 该块的 大小

    if(prev_alloc && next_alloc){                      //case 1:前后都已分配
        return bp;
    }
    else if(prev_alloc && !next_alloc) {                  //case 2:前一块已分配，后一块未分配
    
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if(!prev_alloc && next_alloc) {                  //case 3:前一块未分配，后一块已分配
        
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))+GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

//首次匹配
static void *find_fit(size_t size)
{
    void *bp;
    
    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp)))){
            return bp;
        }
    }
    return NULL;
}

//最佳匹配
static void *best_fit(size_t asize)
{
    void *bp = heap_listp;
	size_t size;
	void *best = NULL;
	size_t min_size = 0;
	
	while((size = GET_SIZE(HDRP(bp))) != 0){
		if(size >= asize && !GET_ALLOC(HDRP(bp)) && (min_size == 0 || min_size>size)){	//记录最小的合适的空闲块
			min_size = size;
			best = bp;
		}
		bp = NEXT_BLKP(bp);
	}
	return best;
}


static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else{
        PUT(HDRP(bp), PACK(csize,1));
        PUT(FTRP(bp), PACK(csize,1));
    }
    
}













