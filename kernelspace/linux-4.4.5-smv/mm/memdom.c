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
    mutex_lock(&mm->memdom_bitmapMutex);

    /* Are we having too many memdoms? */
    if( atomic_read(&mm->num_memdoms) == MAX_MEMDOM ) {
        goto err;
    }

    /* Find available slot in the bitmap for the new ribbon */
    memdom_id = find_first_zero_bit(mm->memdom_bitmapInUse, MAX_MEMDOM);
    if( memdom_id == MAX_MEMDOM ) {
        goto err;        
    }

    /* Create the actual memdom struct */
    memdom = allocate_memdom();
    memdom->memdom_id = memdom_id;
    bitmap_zero(memdom->ribbon_bitmapRead, MAX_RIBBON);    
    bitmap_zero(memdom->ribbon_bitmapWrite, MAX_RIBBON);    
    bitmap_zero(memdom->ribbon_bitmapExecute, MAX_RIBBON);    
    bitmap_zero(memdom->ribbon_bitmapAllocate, MAX_RIBBON);    
    mutex_init(&memdom->memdom_mutex);

    /* Record this new ribbon to mm */
    mm->memdom_metadata[memdom_id] = memdom;

    /* Set bit in memdom bitmap */
    set_bit(memdom_id, mm->memdom_bitmapInUse);

    /* Increase total number of memdom count in mm_struct */
    atomic_inc(&mm->num_memdoms);

    printk(KERN_INFO "Created new memdom with ID %d, #memdom: %d / %d\n", 
            memdom_id, atomic_read(&mm->num_memdoms), MAX_MEMDOM);
    goto out;

err:
    printk(KERN_ERR "Too many memdoms, cannot create more.\n");
    memdom_id = -1;
out:
    mutex_unlock(&mm->memdom_bitmapMutex);
    return memdom_id;
}
EXPORT_SYMBOL(memdom_create);

/* Find the first (in bit order) ribbon in the memdom. Called by memdom_kill */
int find_first_ribbon(struct memdom_struct *memdom){
    int ribbon_id = 0;

    /* Check read permission */
    ribbon_id = find_first_bit(memdom->ribbon_bitmapRead, MAX_RIBBON);
    if( ribbon_id != MAX_RIBBON ) {
        return ribbon_id;
    }

    /* Check write permission */
    ribbon_id = find_first_bit(memdom->ribbon_bitmapWrite, MAX_RIBBON);
    if( ribbon_id != MAX_RIBBON ) {
        return ribbon_id;
    }

    /* Check allocate permission */
    ribbon_id = find_first_bit(memdom->ribbon_bitmapAllocate, MAX_RIBBON);
    if( ribbon_id != MAX_RIBBON ) {
        return ribbon_id;
    }

    /* Check execute permission */
    ribbon_id = find_first_bit(memdom->ribbon_bitmapExecute, MAX_RIBBON);

    return ribbon_id;
}

/* Free a memory domain metadata and remove it from mm_struct */
int memdom_kill(int memdom_id, struct mm_struct *mm){
    struct memdom_struct *memdom = NULL;
    int ribbon_id = 0;

    if( memdom_id >= MAX_MEMDOM ) {
        printk(KERN_ERR "[%s] Error, out of bound: memdom %d\n", __func__, memdom_id);
        return -1;
    }

    /* When user space program calls memdom_kill, mm_struct is NULL
     * If free_all_memdoms calls this function, it passes the about-to-destroy mm_struct, not current->mm */
    if( !mm ) {
        mm = current->mm;
    }
    
    /* SMP: protect shared memdom bitmap */
    mutex_lock(&mm->memdom_bitmapMutex);
    memdom = mm->memdom_metadata[memdom_id];

    /* TODO: check if current task has the permission to delete the memdom, only master thread can do this */
    
    /* Clear memdom_id-th bit in memdom_bitmapInUse */
    if( test_bit(memdom_id, mm->memdom_bitmapInUse) ) {
        clear_bit(memdom_id, mm->memdom_bitmapInUse);  
    } else {
        printk(KERN_ERR "Error, trying to delete a memdom that does not exist: memdom %d, #memdoms: %d\n", memdom_id, atomic_read(&mm->num_memdoms));
        mutex_unlock(&mm->memdom_bitmapMutex);
        return -1;
    }

    /* Clear all ribbon_bitmapR/W/E/A bits for this memdom in all ribbons */    
    ribbon_id = find_first_ribbon(memdom);
    while( ribbon_id != MAX_RIBBON ) {
        ribbon_leave_memdom(memdom_id, ribbon_id, mm); 
        ribbon_id = find_first_ribbon(memdom);
    }   

    /* Free the actual memdom struct */
    free_memdom(memdom);
    mm->memdom_metadata[memdom_id] = NULL;

    /* Decrement memdom count */
    atomic_dec(&mm->num_memdoms);

    printk(KERN_INFO "Deleted memdom with ID %d, #memdoms: %d / %d\n", 
            memdom_id, atomic_read(&mm->num_memdoms), MAX_MEMDOM);

    mutex_unlock(&mm->memdom_bitmapMutex);
    return 0;

}
EXPORT_SYMBOL(memdom_kill);

/* Free all the memdoms in this mm_struct */
void free_all_memdoms(struct mm_struct *mm){
    int index = 0;
    while( atomic_read(&mm->num_memdoms) > 0 ){
        index = find_first_bit(mm->memdom_bitmapInUse, MAX_MEMDOM);
        printk(KERN_INFO "[%s] killing memdom %d, remaining #memdom: %d\n", __func__, index, atomic_read(&mm->num_memdoms));
        memdom_kill(index, mm);
    }
}

unsigned long memdom_alloc(int memdom_id, unsigned long sz){

    return 0;
}
EXPORT_SYMBOL(memdom_alloc);

unsigned long memdom_free(unsigned long addr){

    return 0;
}
EXPORT_SYMBOL(memdom_free);

int memdom_priv_add(int memdom_id, int ribbon_id, unsigned long privs){

    return 0;
}
EXPORT_SYMBOL(memdom_priv_add);

int memdom_priv_del(int memdom_id, int ribbon_id, unsigned long privs){

    return 0;
}
EXPORT_SYMBOL(memdom_priv_del);

int memdom_priv_get(int memdom_id, int ribbon_id){

    return 0;
}
EXPORT_SYMBOL(memdom_priv_get);


