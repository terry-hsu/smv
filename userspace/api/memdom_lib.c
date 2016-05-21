#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <limits.h>
#include "memdom_lib.h"

struct memdom_metadata_struct *memdom[MAX_MEMDOM];

/* Create memory domain and return it to user */
int memdom_create(){    
    int memdom_id;
    memdom_id = message_to_kernel("memdom,create");
    if( memdom_id == -1 ){
		fprintf(stderr, "memdom_create() failed\n");
        return -1;
    }
    /* Allocate metadata to hold memdom info */
    memdom[memdom_id] = (struct memdom_metadata_struct*) malloc(sizeof(struct memdom_metadata_struct));
    memdom[memdom_id]->memdom_id = memdom_id;
    memdom[memdom_id]->start = NULL; // memdom_alloc will do the actual mmap
    memdom[memdom_id]->total_size = 0;
    memdom[memdom_id]->free_list_head = NULL;
    memdom[memdom_id]->free_list_tail = NULL;
    pthread_mutex_init(&memdom[memdom_id]->mlock, NULL);

    return memdom_id;
}

/* Remove memory domain memdom from kernel */
int memdom_kill(int memdom_id){
    int rv = 0;
    char buf[50];
    struct free_list_struct *free_list;

    /* Bound checking */
    if( memdom_id > MAX_MEMDOM ) {
		fprintf(stderr, "memdom_kill(%d) failed\n", memdom_id);
        return -1;
    }

    /* Free mmap */
    if( memdom[memdom_id]->start ) {
        rv = munmap(memdom[memdom_id]->start, memdom[memdom_id]->total_size);
        if( rv != 0 ) {
            fprintf(stderr, "memdom munmap failed, start: %p, sz: 0x%lx bytes\n", memdom[memdom_id]->start, memdom[memdom_id]->total_size);
        }
    }

    /* Free all free_list_struct in this memdom */
    free_list = memdom[memdom_id]->free_list_head;
    while( free_list ) {
        struct free_list_struct *tmp = free_list;
        free_list = free_list->next;
        printf("freeing free_list addr: %p, size: 0x%lx bytes\n", tmp->addr, tmp->size);
        free(tmp);
    }

    /* Free memdom metadata */
    free(memdom[memdom_id]);
    
    /* Send kill memdom info to kernel */
    sprintf(buf, "memdom,kill,%d", memdom_id);
    rv = message_to_kernel(buf);
    if( rv == -1 ){
		fprintf(stderr, "memdom_kill(%d) failed\n", memdom_id);
        return -1;
    }    
    rlog("Memdom ID %d killed\n", memdom_id);
    return rv;
}

/* mmap memory in memdom 
 * Caller should hold memdom lock
 */
void *memdom_mmap(int memdom_id,
                  unsigned long addr, unsigned long len, 
                  unsigned long prot, unsigned long flags, 
                  unsigned long fd, unsigned long pgoff){
    void *base = NULL;
    int rv = 0;
    char buf[50];

    /* Store memdom id in current->mmap_memdom_id in kernel */
    sprintf(buf, "memdom,mmapregister,%d", memdom_id);
    rv = message_to_kernel(buf);
    if( rv == -1 ){
		fprintf(stderr, "memdom_mmap_register(%d) failed\n", memdom_id);
        return NULL;
    }    
    rlog("Memdom ID %d registered for mmap\n", memdom_id);
    
    /* Call the actual mmap with memdom flag */
    flags |= MAP_MEMDOM;
    base = (void*) mmap(NULL, len, prot, flags, fd, pgoff);
    if( base == MAP_FAILED ) {
        perror("memdom_mmap: ");
        return NULL;
    }
    memdom[memdom_id]->start = base;
    memdom[memdom_id]->total_size = len;

    rlog("Memdom ID %d mmaped at %p\n", memdom_id, base);

    printf("[%s] memdom %d mmaped 0x%lx bytes at %p\n", __func__, memdom_id, len, base);
    return base;
}

/* Return privilege status of ribbon rib in memory domain memdom */
unsigned long memdom_priv_get(int memdom_id, int ribbon_id){
    int rv = 0;
    char buf[100];
    sprintf(buf, "memdom,priv,%d,%d,get", memdom_id, ribbon_id);
    rv = message_to_kernel(buf);
    if( rv == -1 ){
        rlog("kernel responded error");
        return -1;
    }    
    rlog("Ribbon %d in memdom %d has privilege: 0x%x\n", ribbon_id, memdom_id, rv);
    // ! should return privilege
    return rv;
}

/* Add privilege of ribbon rib in memory domain memdom */
int memdom_priv_add(int memdom_id, int ribbon_id, unsigned long privs){
    int rv = 0;
    char buf[100];
    sprintf(buf, "memdom,priv,%d,%d,add,%lu", memdom_id, ribbon_id, privs);
    rv = message_to_kernel(buf);
    if( rv == -1 ){
        rlog("kernel responded error");
        return -1;
    }    
    rlog("Ribbon %d in memdom %d has (after added)privilege: 0x%x\n", ribbon_id, memdom_id, rv);
    // ! should return privilege
    return rv;
}

/* Delete privilege of ribbon rib in memory domain memdom */
int memdom_priv_del(int memdom_id, int ribbon_id, unsigned long privs){
    int rv = 0;
    char buf[100];
    sprintf(buf, "memdom,priv,%d,%d,del,%lu", memdom_id, ribbon_id, privs);
    rv = message_to_kernel(buf);
    if( rv == -1 ){
        rlog("kernel responded error");
        return -1;
    }    
    rlog("Ribbon %d in memdom %d has (after deleted)privilege: 0x%x\n", ribbon_id, memdom_id, rv);    
    // ! should return privilege
    return rv;
    
}

/* Modify privilege of ribbon rib in memory domain memdom */
int memdom_priv_mod(int memdom_id, int ribbon_id, unsigned long privs){
    int rv = 0;
    char buf[100];
    sprintf(buf, "memdom,priv,%d,%d,mod,%lu", memdom_id, ribbon_id, privs);
    rv = message_to_kernel(buf);
    if( rv == -1 ){
        rlog("kernel responded error");
        return -1;
    }    
    rlog("Ribbon %d in memdom %d has (after modified)privilege: %d\n", ribbon_id, memdom_id, rv);    
    // ! should return privilege
    return rv;
}


/* Get the memdom id for global memory used by main thread */
int memdom_main_id(void){
    int rv = 0;
    char buf[100];
    sprintf(buf, "memdom,mainid");
    rv = message_to_kernel(buf);
    if( rv == -1 ){
        rlog("kernel responded error");
        return -1;
    }    
    rlog("Global memdom id: %d\n", rv);    
    return rv;
}

/* Get the memdom id of a memory address */
int memdom_query_id(void *obj){
    int rv = 0;
    char buf[1024];
    unsigned long addr;
    addr = (unsigned long)obj;
    sprintf(buf, "memdom,queryid,%lu", addr);
    rv = message_to_kernel(buf);
    if( rv == -1 ){
        rlog("kernel responded error");
        return -1;
    }    
    rlog("obj in memdom %d\n", rv);    
    return rv;
}

/* Get calling thread's defualt memdom id */
int memdom_private_id(void){
    int rv = 0;
    char buf[1024];
#ifdef THREAD_PRIVATE_STACK
    sprintf(buf, "memdom,privateid");
    rv = message_to_kernel(buf);
    if( rv == -1 ){
        rlog("kernel responded error");
        return -1;
    }    
#else
    rv = 0;
#endif
    rlog("private memdom id: %d\n", rv);    
    return rv;
}

void dumpFreeListHead(int memdom_id){
    struct free_list_struct *walk = memdom[memdom_id]->free_list_head;
    while ( walk ) {
        printf("[%s] memdom %d free_list addr: %p, sz: 0x%lx\n", 
                __func__, memdom_id, walk->addr, walk->size);
        walk = walk->next;
    }
}

/* Insert a free list struct to the head of memdom free list 
 * Reclaimed chunks are inserted to head
 */
void free_list_insert_to_head(int memdom_id, struct free_list_struct *new_free_list){
    int rv;
    struct free_list_struct *head = memdom[memdom_id]->free_list_head;
    if( head ) {
        new_free_list->next = head;
    }
    memdom[memdom_id]->free_list_head = new_free_list;
    printf("[%s] memdom %d inserted free list addr: %p, size: 0x%lx\n", __func__, memdom_id, new_free_list->addr, new_free_list->size);
}

/* Initialize free list */
void free_list_init(int memdom_id){
    struct free_list_struct *new_free_list;

    /* The first free list should be the entire mmap region */
    new_free_list = (struct free_list_struct*) malloc (sizeof(struct free_list_struct));
    new_free_list->addr = memdom[memdom_id]->start;
    new_free_list->size = memdom[memdom_id]->total_size;   
    new_free_list->next = NULL;
    memdom[memdom_id]->free_list_head = NULL;   // reclaimed chunk are inserted to head   
    memdom[memdom_id]->free_list_tail = new_free_list; 
    printf("[%s] memdom %d: free_list addr: %p, size: 0x%lx bytes\n", __func__, memdom_id, new_free_list->addr, new_free_list->size);
}

/* Round up the number to the nearest multiple */
unsigned long round_up(unsigned long numToRound, int multiple){
    int remainder = 0;
    if( multiple == 0 ) {
        return 0;
    }
    remainder = numToRound % multiple;
    if( remainder == 0 ) {
        return numToRound;
    }
    return numToRound + multiple - remainder;
}

/* Allocate memory in memory domain memdom */
void *memdom_alloc(int memdom_id, unsigned long sz){
    char *memblock = NULL;
    struct free_list_struct *free_list = NULL;
    
    /* Memdom 0 is in global memdom, use malloc */
    if(memdom_id == 0){
        memblock = (char*) malloc(sz);   
        return memblock;
    }

    pthread_mutex_lock(&memdom[memdom_id]->mlock);

    printf("[%s] memdom %d allocating sz 0x%lx bytes\n", __func__, memdom_id, sz);

    /* First time this memdom allocates memory */
    if( !memdom[memdom_id]->start ) {
        /* Call mmap to set up initial memory region */
        memblock = (char*) memdom_mmap(memdom_id, 0, MEMDOM_HEAP_SIZE, 
                                       PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_MEMDOM, 0, 0);
        if( memblock == MAP_FAILED ) {
            fprintf(stderr, "Failed to memdom_alloc using mmap for memdom %d\n", memdom_id);
            memblock = NULL;
            goto out;
        }

        /* Initialize free list */
        free_list_init(memdom_id);
    }

    /* Round up size to multiple of cache line size: 64B 
     * Note that the size of should block_header + the actual data
     * --------------------------------------
     * | block header|      your data       |
     * --------------------------------------
     */
    sz = round_up ( sz + sizeof(struct block_header_struct), CHUNK_SIZE);
    printf("[%s] request rounded to 0x%lx bytes\n", __func__, sz);

    /* Get memory from the tail of free list, if the last free list is not available for allocation,
     * start searching the free list from the head until first fit is found.
     */
    free_list = memdom[memdom_id]->free_list_tail;

    /* Allocate from tail: 
     * check if the last element in free list is available, 
     * allocate memory from it */
    printf("[%s] memdom %d search from tail for 0x%lx bytes\n", __func__, memdom_id, sz);     
    if ( free_list && sz <= free_list->size ) {
        memblock = (char*)free_list->addr;

        /* Adjust the last free list addr and size*/
        free_list->addr = (char*)free_list->addr + sz;
        free_list->size = free_list->size - sz;

        printf("[%s] memdom %d last free list available, free_list addr: %p, remaining sz: 0x%lx bytes\n", 
                __func__, memdom_id, free_list->addr, free_list->size);
        /* Last chunk is now allocated, tail is not available from now */
        if( free_list->size == 0 ) {
            free(free_list);
            memdom[memdom_id]->free_list_tail = NULL;
            printf("[%s] free_list size is 0, freed this free_list_struct, the next allocate should request from free_list_head\n", __func__);
        }
        goto out;
    }

    /* Allocate from head: 
     * ok the last free list is not available, 
     * let's start searching from the head for the first fit */
    printf("[%s] memdom %d search from head for 0x%lx bytes\n", __func__, memdom_id, sz);     
    dumpFreeListHead(memdom_id);
    free_list = memdom[memdom_id]->free_list_head;
    struct free_list_struct *prev = NULL;
    while (free_list) {
        if( prev ) {
            printf("[%s] memdom %d prev->addr %p, prev->size 0x%lx bytes\n", __func__, memdom_id, prev->addr, prev->size);
        }
        if( free_list ) {
            printf("[%s] memdom %d free_list->addr %p, free_list->size 0x%lx bytes\n", __func__, memdom_id, free_list->addr, free_list->size);
        }
        
        /* Found free list! */
        if( sz <= free_list->size ) {

            /* Get memory address */
            memblock = (char*)free_list->addr;

            /* Adjust free list:
             * if the remaining chunk size if greater then CHUNK_SIZE
             */
            if( free_list->size - sz >= CHUNK_SIZE ) {
                char *ptr = (char*)free_list->addr;
                ptr = ptr + sz;
                free_list->addr = (void*)ptr;
                free_list->size = free_list->size - sz;
                printf("[%s] Adjust free list to addr %p, sz 0x%lx\n", 
                        __func__, free_list->addr, free_list->size);
            }
            /* Remove this free list struct: 
             * since there's no memory to allcoate from here anymore 
             */
            else{                
                if ( free_list == memdom[memdom_id]->free_list_head ) {
                    memdom[memdom_id]->free_list_head = memdom[memdom_id]->free_list_head->next;
                    printf("[%s] memdom %d set free_list_head to free_list_head->next\n", __func__, memdom_id);
                }
                else {
                    prev->next = free_list->next;
                    printf("[%s] memdom %d set prev->next to free_list->next\n", __func__, memdom_id);
                }
                free(free_list);

                printf("[%s] memdom %d removed free list\n", __func__, memdom_id);
            }
            goto out;
        }

        /* Move pointer forward */
        prev = free_list;
        free_list = free_list->next;
    }   
   
out:   
    if( !memblock ) {
        fprintf(stderr, "memdom_alloc failed: no memory can be allocated in memdom %d\n", memdom_id);
    }
    else{    
        /* Record allocated memory in the block header for free to use later */
        struct block_header_struct header;
        header.addr = (void*)memblock;
        header.memdom_id = memdom_id;
        header.size = sz;
        memcpy(memblock, &header, sizeof(struct block_header_struct));
        memblock = memblock + sizeof(struct block_header_struct);
        printf("[%s] header: addr %p, allocated 0x%lx bytes and returning data addr %p\n", __func__, header.addr, sz, memblock);
    }

    pthread_mutex_unlock(&memdom[memdom_id]->mlock);
    return (void*)memblock;
}

/* Deallocate data in memory domain memdom */
void memdom_free(void* data){
    struct block_header_struct header;
    char *memblock = NULL;
    int memdom_id = -1;

    /* Read the header stored ahead of the actual data */
    memblock = (char*) data - sizeof(struct block_header_struct);
    memcpy(&header, memblock, sizeof(struct block_header_struct));
    memdom_id = header.memdom_id;

    pthread_mutex_lock(&memdom[memdom_id]->mlock);
 
    /* Free the memory */
    printf("[%s] block header addr: %p, freeing 0x%lx bytes in memdom %d\n", __func__, header.addr, header.size, header.memdom_id);
    memset(memblock, 0, header.size);

    /* Create a new free list node */
    struct free_list_struct *free_list = (struct free_list_struct *) malloc(sizeof(struct free_list_struct));
    free_list->addr = memblock;
    free_list->size = header.size;
    free_list->next = NULL;

    /* Insert the block into free list head */
    free_list_insert_to_head(header.memdom_id, free_list);   

    pthread_mutex_unlock(&memdom[memdom_id]->mlock);
}

