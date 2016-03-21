#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memdom_lib.h"

/* Create memory domain and return it to user */
int memdom_create(){    
    int memdom_id;
    memdom_id = message_to_kernel("memdom,create");
    if( memdom_id == -1 ){
		fprintf(stderr, "memdom_create() failed\n");
        return -1;
    }
    return memdom_id;
}

/* Remove memory domain memdom from kernel */
int memdom_kill(int memdom_id){
    int rv = 0;
    char buf[50];
    sprintf(buf, "memdom,kill,%d", memdom_id);
    rv = message_to_kernel(buf);
    if( rv == -1 ){
		fprintf(stderr, "memdom_kill(%d) failed\n", memdom_id);
        return -1;
    }    
    rlog("Memdom ID %d killed\n", memdom_id);
    return rv;
}

/* Allocate npages pages in memory domain memdom */
void *memdom_alloc(int memdom_id, unsigned long nbytes){
    void *memblock = NULL;

    return memblock;   
}

/* Deallocate data in memory domain memdom */
void memdom_free(int memdom_id, void* data){

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

