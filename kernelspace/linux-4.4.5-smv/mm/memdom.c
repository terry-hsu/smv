#include <linux/ribbon.h>
#include <linux/memdom.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>

/* SLAB cache for ribbon_struct structure  */
static struct kmem_cache *memdom_cachep;

/** void memdom_init(void)
 *  Create slab cache for future memdom_struct allocation This
 *  is called by start_kernel in main.c 
 */
void memdom_init(void){
    memdom_cachep = kmem_cache_create("memdom_struct",
                                      sizeof(struct memdom_struct), 0,
                                      SLAB_HWCACHE_ALIGN | SLAB_NOTRACK, NULL);
    if( !memdom_cachep ) {
        printk(KERN_INFO "[%s] memdom slabs initialization failed...\n", __func__);
    } else{
        printk(KERN_INFO "[%s] memdom slabs initialized\n", __func__);
    }
}

/* Create a memdom and update metadata */
int memdom_create(void){
    int memdom_id = -1;
    struct mm_struct *mm = current->mm;
    struct memdom_struct *memdom = NULL;

    /* SMP: protect shared memdom bitmap */
    mutex_lock(&mm->smv_metadataMutex);

    /* Are we having too many memdoms? */
    if( atomic_read(&mm->num_memdoms) == SMV_ARRAY_SIZE ) {
        goto err;
    }

    /* Find available slot in the bitmap for the new ribbon */
    memdom_id = find_first_zero_bit(mm->memdom_bitmapInUse, SMV_ARRAY_SIZE);
    if( memdom_id == SMV_ARRAY_SIZE ) {
        goto err;        
    }

    /* Create the actual memdom struct */
    memdom = allocate_memdom();
    memdom->memdom_id = memdom_id;
    bitmap_zero(memdom->ribbon_bitmapRead, SMV_ARRAY_SIZE);    
    bitmap_zero(memdom->ribbon_bitmapWrite, SMV_ARRAY_SIZE);    
    bitmap_zero(memdom->ribbon_bitmapExecute, SMV_ARRAY_SIZE);    
    bitmap_zero(memdom->ribbon_bitmapAllocate, SMV_ARRAY_SIZE);    
    mutex_init(&memdom->memdom_mutex);

    /* Record this new memdom to mm */
    mm->memdom_metadata[memdom_id] = memdom;

    /* Set bit in memdom bitmap */
    set_bit(memdom_id, mm->memdom_bitmapInUse);

    /* Increase total number of memdom count in mm_struct */
    atomic_inc(&mm->num_memdoms);

    printk(KERN_INFO "Created new memdom with ID %d, #memdom: %d / %d\n", 
            memdom_id, atomic_read(&mm->num_memdoms), SMV_ARRAY_SIZE);
    goto out;

err:
    printk(KERN_ERR "Too many memdoms, cannot create more.\n");
    memdom_id = -1;
out:
    mutex_unlock(&mm->smv_metadataMutex);
    return memdom_id;
}
EXPORT_SYMBOL(memdom_create);

/* Find the first (in bit order) ribbon in the memdom. Called by memdom_kill */
int find_first_ribbon(struct memdom_struct *memdom){
    int ribbon_id = 0;

    mutex_lock(&memdom->memdom_mutex);

    /* Check read permission */
    ribbon_id = find_first_bit(memdom->ribbon_bitmapRead, SMV_ARRAY_SIZE);
    if( ribbon_id != SMV_ARRAY_SIZE ) {
        goto out;
    }

    /* Check write permission */
    ribbon_id = find_first_bit(memdom->ribbon_bitmapWrite, SMV_ARRAY_SIZE);
    if( ribbon_id != SMV_ARRAY_SIZE ) {
        goto out;
    }

    /* Check allocate permission */
    ribbon_id = find_first_bit(memdom->ribbon_bitmapAllocate, SMV_ARRAY_SIZE);
    if( ribbon_id != SMV_ARRAY_SIZE ) {
        goto out;
    }

    /* Check execute permission */
    ribbon_id = find_first_bit(memdom->ribbon_bitmapExecute, SMV_ARRAY_SIZE);

out:
    mutex_unlock(&memdom->memdom_mutex);
    return ribbon_id;
}

/* Free a memory domain metadata and remove it from mm_struct */
int memdom_kill(int memdom_id, struct mm_struct *mm){
    struct memdom_struct *memdom = NULL;
    int ribbon_id = 0;

    if( memdom_id > LAST_MEMDOM_INDEX ) {
        printk(KERN_ERR "[%s] Error, out of bound: memdom %d\n", __func__, memdom_id);
        return -1;
    }

    /* When user space program calls memdom_kill, mm_struct is NULL
     * If free_all_memdoms calls this function, it passes the about-to-destroy mm_struct, not current->mm */
    if( !mm ) {
        mm = current->mm;
    }
    
    /* SMP: protect shared memdom bitmap */
    mutex_lock(&mm->smv_metadataMutex);
    memdom = mm->memdom_metadata[memdom_id];

    /* TODO: check if current task has the permission to delete the memdom, only master thread can do this */
    
    /* Clear memdom_id-th bit in memdom_bitmapInUse */
    if( test_bit(memdom_id, mm->memdom_bitmapInUse) ) {
        clear_bit(memdom_id, mm->memdom_bitmapInUse);  
        mutex_unlock(&mm->smv_metadataMutex);
    } else {
        printk(KERN_ERR "Error, trying to delete a memdom that does not exist: memdom %d, #memdoms: %d\n", memdom_id, atomic_read(&mm->num_memdoms));
        mutex_unlock(&mm->smv_metadataMutex);
        return -1;
    }

    /* Clear all ribbon_bitmapR/W/E/A bits for this memdom in all ribbons */    
    do {
        ribbon_id = find_first_ribbon(memdom);
        if( ribbon_id != SMV_ARRAY_SIZE ) {
            ribbon_leave_memdom(memdom_id, ribbon_id, mm);             
        }
    } while( ribbon_id != SMV_ARRAY_SIZE );
    
    /* Free the actual memdom struct */
    free_memdom(memdom);
    mm->memdom_metadata[memdom_id] = NULL;

    /* Decrement memdom count */
    mutex_lock(&mm->smv_metadataMutex);
    atomic_dec(&mm->num_memdoms);
    mutex_unlock(&mm->smv_metadataMutex);

    printk(KERN_INFO "Deleted memdom with ID %d, #memdoms: %d / %d\n", 
            memdom_id, atomic_read(&mm->num_memdoms), SMV_ARRAY_SIZE);

    return 0;
}
EXPORT_SYMBOL(memdom_kill);

/* Free all the memdoms in this mm_struct */
void free_all_memdoms(struct mm_struct *mm){
    int index = 0;
    while( atomic_read(&mm->num_memdoms) > 0 ){
        index = find_first_bit(mm->memdom_bitmapInUse, SMV_ARRAY_SIZE);
        printk(KERN_INFO "[%s] killing memdom %d, remaining #memdom: %d\n", __func__, index, atomic_read(&mm->num_memdoms));
        memdom_kill(index, mm);
    }
}

/* Set bit in memdom->ribbon_bitmapR/W/E/A */
int memdom_priv_add(int memdom_id, int ribbon_id, int privs){
    struct ribbon_struct *ribbon; 
    struct memdom_struct *memdom; 
    struct mm_struct *mm = current->mm;

    if( ribbon_id > LAST_RIBBON_INDEX || memdom_id > LAST_MEMDOM_INDEX ) {
        printk(KERN_ERR "[%s] Error, out of bound: ribbon %d / memdom %d\n", __func__, ribbon_id, memdom_id);
        return -1;
    }

    mutex_lock(&mm->smv_metadataMutex);
    ribbon = current->mm->ribbon_metadata[ribbon_id];
    memdom = current->mm->memdom_metadata[memdom_id];
    mutex_unlock(&mm->smv_metadataMutex);

    if( !memdom || !ribbon ) {
        printk(KERN_ERR "[%s] memdom %p || ribbon %p not found\n", __func__, memdom, ribbon);
        return -1;
    }       
    if( !ribbon_is_in_memdom(memdom_id, ribbon->ribbon_id) ) {
        printk(KERN_ERR "[%s] ribbon %d is not in memdom %d, please make ribbon join memdom first.\n", __func__, ribbon_id, memdom_id);
        return -1;  
    }
    
    /* TODO: Add privilege check to see if current thread can change the privilege */

    /* Set privileges in memdom's bitmap */   
    mutex_lock(&memdom->memdom_mutex);
    if( privs & MEMDOM_READ ) {
        set_bit(ribbon_id, memdom->ribbon_bitmapRead);
        printk(KERN_INFO "[%s] Added read privilege for ribbon %d in memdmo %d\n", __func__, ribbon_id, memdom_id);
    }
    if( privs & MEMDOM_WRITE ) {
        set_bit(ribbon_id, memdom->ribbon_bitmapWrite);
        printk(KERN_INFO "[%s] Added write privilege for ribbon %d in memdmo %d\n", __func__, ribbon_id, memdom_id);
    }
    if( privs & MEMDOM_EXECUTE ) {
        set_bit(ribbon_id, memdom->ribbon_bitmapExecute);
        printk(KERN_INFO "[%s] Added execute privilege for ribbon %d in memdmo %d\n", __func__, ribbon_id, memdom_id);
    }
    if( privs & MEMDOM_ALLOCATE ) {
        set_bit(ribbon_id, memdom->ribbon_bitmapAllocate);
        printk(KERN_INFO "[%s] Added allocate privilege for ribbon %d in memdmo %d\n", __func__, ribbon_id, memdom_id);
    }    
    mutex_unlock(&memdom->memdom_mutex);     
     
    return 0;
}
EXPORT_SYMBOL(memdom_priv_add);

/* Clear bit in memdom->ribbon_bitmapR/W/E/A */
int memdom_priv_del(int memdom_id, int ribbon_id, int privs){
    struct ribbon_struct *ribbon = NULL;
    struct memdom_struct *memdom = NULL;
    struct mm_struct *mm = current->mm;

    if( ribbon_id > LAST_RIBBON_INDEX || memdom_id > LAST_MEMDOM_INDEX ) {
        printk(KERN_ERR "[%s] Error, out of bound: ribbon %d / memdom %d\n", __func__, ribbon_id, memdom_id);
        return -1;
    }

    mutex_lock(&mm->smv_metadataMutex);
    ribbon = current->mm->ribbon_metadata[ribbon_id];
    memdom = current->mm->memdom_metadata[memdom_id];
    mutex_unlock(&mm->smv_metadataMutex);

    if( !memdom || !ribbon ) {
        printk(KERN_ERR "[%s] memdom %p || ribbon %p not found\n", __func__, memdom, ribbon);
        return -1;
    }       
    if( !ribbon_is_in_memdom(memdom_id, ribbon->ribbon_id) ) {
        printk(KERN_ERR "[%s] ribbon %d is not in memdom %d, please make ribbon join memdom first.\n", __func__, ribbon_id, memdom_id);
        return -1;  
    }
    
    /* TODO: Add privilege check to see if current thread can change the privilege */

    /* Clear privileges in memdom's bitmap */   
    mutex_lock(&memdom->memdom_mutex);
    if( privs & MEMDOM_READ ) {
        clear_bit(ribbon_id, memdom->ribbon_bitmapRead);
        printk(KERN_INFO "[%s] Revoked read privilege for ribbon %d in memdmo %d\n", __func__, ribbon_id, memdom_id);
    }
    if( privs & MEMDOM_WRITE ) {
        clear_bit(ribbon_id, memdom->ribbon_bitmapWrite);
        printk(KERN_INFO "[%s] Revoked write privilege for ribbon %d in memdmo %d\n", __func__, ribbon_id, memdom_id);
    }
    if( privs & MEMDOM_EXECUTE ) {
        clear_bit(ribbon_id, memdom->ribbon_bitmapExecute);
        printk(KERN_INFO "[%s] Revoked execute privilege for ribbon %d in memdmo %d\n", __func__, ribbon_id, memdom_id);
    }
    if( privs & MEMDOM_ALLOCATE ) {
        clear_bit(ribbon_id, memdom->ribbon_bitmapAllocate);
        printk(KERN_INFO "[%s] Revoked allocate privilege for ribbon %d in memdmo %d\n", __func__, ribbon_id, memdom_id);
    }            
    mutex_unlock(&memdom->memdom_mutex);

    return 0;
}
EXPORT_SYMBOL(memdom_priv_del);

/* Return ribbon's privileges in a given memdom and return to caller */
int memdom_priv_get(int memdom_id, int ribbon_id){
    struct ribbon_struct *ribbon = NULL;
    struct memdom_struct *memdom = NULL;
    struct mm_struct *mm = current->mm;
    int privs = 0;

    if( ribbon_id > LAST_RIBBON_INDEX || memdom_id > LAST_MEMDOM_INDEX ) {
        printk(KERN_ERR "[%s] Error, out of bound: ribbon %d / memdom %d\n", __func__, ribbon_id, memdom_id);
        return -1;
    }

    mutex_lock(&mm->smv_metadataMutex);
    ribbon = current->mm->ribbon_metadata[ribbon_id];
    memdom = current->mm->memdom_metadata[memdom_id];
    mutex_unlock(&mm->smv_metadataMutex);

    if( !memdom || !ribbon ) {
        printk(KERN_ERR "[%s] memdom %p || ribbon %p not found\n", __func__, memdom, ribbon);
        return -1;
    }       
    if( !ribbon_is_in_memdom(memdom_id, ribbon->ribbon_id) ) {
        printk(KERN_ERR "[%s] ribbon %d is not in memdom %d, please make ribbon join memdom first.\n", __func__, ribbon_id, memdom_id);
        return -1;  
    }
    
    /* TODO: Add privilege check to see if current thread can change the privilege */

    /* Get privilege info */
    mutex_lock(&memdom->memdom_mutex);
    if( test_bit(ribbon_id, memdom->ribbon_bitmapRead) ) {
        privs = privs | MEMDOM_READ;
    }
    if( test_bit(ribbon_id, memdom->ribbon_bitmapWrite) ) {
        privs = privs | MEMDOM_WRITE;
    }
    if( test_bit(ribbon_id, memdom->ribbon_bitmapExecute) ) {
        privs = privs | MEMDOM_EXECUTE;
    }
    if( test_bit(ribbon_id, memdom->ribbon_bitmapAllocate) ) {
        privs = privs | MEMDOM_ALLOCATE;
    }
    mutex_unlock(&memdom->memdom_mutex);

    printk(KERN_INFO "[%s] ribbon %d has privs %x in memdom %d\n", __func__, ribbon_id, privs, memdom_id);
    return privs;
}
EXPORT_SYMBOL(memdom_priv_get);


unsigned long memdom_alloc(int memdom_id, unsigned long sz){

    return 0;
}
EXPORT_SYMBOL(memdom_alloc);

unsigned long memdom_free(unsigned long addr){

    return 0;
}
EXPORT_SYMBOL(memdom_free);

/* Return the memdom id used by the master threadd (global memdom) */
int memdom_main_id(void){
    return MAIN_THREAD;
}
EXPORT_SYMBOL(memdom_main_id);

/* Initialize vma's owner to the main thread, only called by the main thread */
int memdom_claim_all_vmas(int memdom_id){
    struct vm_area_struct *vma;
    struct mm_struct *mm = current->mm;
    int vma_count = 0;

    if( memdom_id > LAST_MEMDOM_INDEX ) {
        printk(KERN_ERR "[%s] Error, out of bound: memdom %d\n", __func__, memdom_id);
        return -1;
    }
    
   	down_write(&mm->mmap_sem);
  	for (vma = mm->mmap; vma; vma = vma->vm_next) {
        vma->memdom_id = MAIN_THREAD;
        vma_count++;
    }
   	up_write(&mm->mmap_sem);

    printk(KERN_INFO "[%s] Initialized %d vmas to be in memdom %d\n", __func__, vma_count, memdom_id);
    return 0;
}

