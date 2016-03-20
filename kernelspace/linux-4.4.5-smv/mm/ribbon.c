#include <linux/ribbon.h>
#include <linux/memdom.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm_types.h>
#include <linux/sched.h>

/* SLAB cache for ribbon_struct structure  */
static struct kmem_cache *ribbon_cachep;

void ribbon_init(void){
    ribbon_cachep = kmem_cache_create("ribbon_struct",
                                      sizeof(struct ribbon_struct), 0,
                                      SLAB_HWCACHE_ALIGN|SLAB_NOTRACK, NULL);
    if( !ribbon_cachep ) {
        printk(KERN_ERR "[%s] ribbon slab initialization failed...\n", __func__);
    } else{
        printk(KERN_INFO "[%s] ribbon slab initialized\n", __func__);
    }
}

/* Create a ribbon and update metadata */
int ribbon_create(void){
    int ribbon_id = -1;
    struct mm_struct *mm = current->mm;
    struct ribbon_struct *ribbon = NULL;

    /* SMP: protect shared ribbon bitmap */
    mutex_lock(&mm->ribbon_bitmapMutex);

    /* Are we having too many ribbons? */
    if( atomic_read(&mm->num_ribbons) == MAX_RIBBON ) {
        goto err;
    }

    /* Find available slot in the bitmap for the new ribbon */
    ribbon_id = find_first_zero_bit(mm->ribbon_bitmapInUse, MAX_RIBBON);
    if( ribbon_id == MAX_RIBBON ) {
        goto err;        
    }

    /* Create the actual ribbon struct */
    ribbon = allocate_ribbon();
    ribbon->ribbon_id = ribbon_id;
    atomic_set(&ribbon->ntask, 0);
    bitmap_zero(ribbon->memdom_bitmapJoin, MAX_MEMDOM);    
    mutex_init(&ribbon->ribbon_mutex);

    /* Record this new ribbon to mm */
    mm->ribbon_metadata[ribbon_id] = ribbon;

    /* Set bit in mm's ribbon bitmap */
    set_bit(ribbon_id, mm->ribbon_bitmapInUse);

    /* Increase total number of ribbon count in mm_struct */
    atomic_inc(&mm->num_ribbons);

    printk(KERN_INFO "Created new ribbon with ID %d, #ribbons: %d / %d\n", 
            ribbon_id, atomic_read(&mm->num_ribbons), MAX_RIBBON);
    goto out;

err:
    printk(KERN_ERR "Too many ribbons, cannot create more.\n");
    ribbon_id = -1;
out:
    mutex_unlock(&mm->ribbon_bitmapMutex);
    return ribbon_id;
}
EXPORT_SYMBOL(ribbon_create);

int ribbon_kill(int ribbon_id, struct mm_struct *mm){
    struct ribbon_struct *ribbon = NULL; 
    int memdom_id = 0;

    if( ribbon_id >= MAX_RIBBON ) {
        printk(KERN_ERR "[%s] Error, out of bound: ribbon %d\n", __func__, ribbon_id);
        return -1;
    }
    
    /* When user space program calls ribbon_kill, mm_struct is NULL
     * If free_all_ribbons calls this function, it passes the about-to-destroy mm_struct, not current->mm */
    if( !mm ) {
        mm = current->mm;
    }

    /* SMP: protect shared ribbon bitmap */
    mutex_lock(&mm->ribbon_bitmapMutex);
    ribbon = mm->ribbon_metadata[ribbon_id]; 

    /* TODO: check if current task has the permission to delete the ribbon, only master thread can do this */
    
    /* Clear ribbon_id-th bit in mm's ribbon_bitmapInUse */
    if( test_bit(ribbon_id, mm->ribbon_bitmapInUse) ) {
        clear_bit(ribbon_id, mm->ribbon_bitmapInUse);  
    } else {
        printk(KERN_ERR "Error, trying to delete a ribbon that does not exist: ribbon %d, #ribbons: %d\n", ribbon_id, atomic_read(&mm->num_ribbons));
        mutex_unlock(&mm->ribbon_bitmapMutex);
        return -1;
    }

    /* Clear all ribbon_bitmap(Read/Write/Execute/Allocate) bits for this ribbon in all memdoms */  
    memdom_id = find_first_bit(ribbon->memdom_bitmapJoin, MAX_MEMDOM);
    while( memdom_id != MAX_MEMDOM ) {
        ribbon_leave_memdom(memdom_id, ribbon_id, mm); 
        memdom_id = find_first_bit(ribbon->memdom_bitmapJoin, MAX_MEMDOM);
    }
    
    /* Free the actual ribbon struct */
    free_ribbon(ribbon);
    mm->ribbon_metadata[ribbon_id] = NULL;

    /* Decrement ribbon count */
    atomic_dec(&mm->num_ribbons);

    printk(KERN_INFO "Deleted ribbon with ID %d, #ribbons: %d / %d\n", 
            ribbon_id, atomic_read(&mm->num_ribbons), MAX_RIBBON);

    mutex_unlock(&mm->ribbon_bitmapMutex);
    return 0;
}
EXPORT_SYMBOL(ribbon_kill);

/* Free all the ribbons in this mm_struct */
void free_all_ribbons(struct mm_struct *mm){
    int index = 0;
    while( atomic_read(&mm->num_ribbons) > 0 ){
        index = find_first_bit(mm->ribbon_bitmapInUse, MAX_RIBBON);
        printk(KERN_INFO "[%s] killing ribbon %d, remaining #ribbons: %d\n", __func__, index, atomic_read(&mm->num_ribbons));
        ribbon_kill(index, mm);
    }
}

int ribbon_join_memdom(int memdom_id, int ribbon_id){

    return 0;
}
EXPORT_SYMBOL(ribbon_join_memdom);

// Set bit to 0 for R/W/E/A in memdom
int ribbon_leave_memdom(int memdom_id, int ribbon_id, struct mm_struct *mm){
    struct memdom_struct *memdom = NULL;   
    struct ribbon_struct *ribbon = NULL;
    /* mm is not NULL is called by ribbon_kill() */
    if( mm == NULL ) {
        mm = current->mm;
    }

    /* Get the actual memdom and ribbon struct from this mm */
    memdom = mm->memdom_metadata[memdom_id];
    ribbon = mm->ribbon_metadata[ribbon_id];
    if( !memdom || !ribbon ) {
        printk(KERN_ERR "[%s] memdom %p || ribbon %p not found\n", __func__, memdom, ribbon);
        return -1;
    }
    printk(KERN_ERR "[%s] memdom %p, ribbon %p\n", __func__, memdom, ribbon);

    /* Clear ribbon_id-th bit in the bitmap for memdom */
    clear_bit(ribbon_id, memdom->ribbon_bitmapRead);
    clear_bit(ribbon_id, memdom->ribbon_bitmapWrite);
    clear_bit(ribbon_id, memdom->ribbon_bitmapExecute);
    clear_bit(ribbon_id, memdom->ribbon_bitmapAllocate);

    /* Clear memdom_id-th bit in the bitmap for ribbon */
    clear_bit(memdom_id, ribbon->memdom_bitmapJoin);

    return 0;
}
EXPORT_SYMBOL(ribbon_leave_memdom);

int ribbon_is_in_memdom(int memdom_id, int ribbon_id){

    return 0;
}
EXPORT_SYMBOL(ribbon_is_in_memdom);

int ribbon_get_ribbon_id(void){

    return 0;
}
EXPORT_SYMBOL(ribbon_get_ribbon_id);
