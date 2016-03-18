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

/* Every newly mm_struct call this function to create a placeholder for ribbons */
struct ribbon_struct *create_ribbon_metadata(void){
    struct ribbon_struct *ribbon = NULL;
    struct mm_struct *mm = current->mm;

    if( !mm ) {
        return NULL;
    }

    /* Create ribbon struct */
    ribbon = allocate_ribbon();
    if( !ribbon ) {
        printk(KERN_ERR "[%s] cannot allocate ribbon metadata\n", __func__);
        return NULL;
    }

    /* Assign ribbon ID */
    ribbon->ribbon_id = atomic_read(&mm->num_ribbons);

    /* Add 1 to existing ribbon count in process's mm */
    atomic_inc(&mm->num_ribbons);

    /* No task running in this ribbon yet */
    atomic_set(&ribbon->ntask, 0);

    /* This ribbon is not in any memory domains yet */
    bitmap_zero(ribbon->memdom_bitmap, MAX_MEMDOM);

    /* No ribbon is allocated yet */
    bitmap_zero(ribbon->ribbon_bitmapInUse, MAX_RIBBON);

    /* Initialize mutex that protects this ribbon */
    mutex_init(&ribbon->ribbon_mutex);    

    return ribbon;
}
void free_ribbon_metadata(struct ribbon_struct *ribbon){
    if( !ribbon ) {
        return;
    }
    kmem_cache_free(ribbon_cachep, (ribbon));
}

int ribbon_create(void){
    int ribbon_id = -1;
    struct mm_struct *mm = current->mm;
    struct ribbon_struct *ribbon_metadata = mm->ribbon_metadata;
    /* Are we having too many ribbons? */
    if( atomic_read(&mm->num_ribbons) == MAX_RIBBON ) {
        printk(KERN_ERR "Too many ribbons, cannot create more.\n");
        return -1;
    }

    /* Find available slot in the bitmap for the new ribbon */
    ribbon_id = find_first_zero_bit(ribbon_metadata->ribbon_bitmapInUse, MAX_RIBBON);
    
    /* Set bit in ribbon bitmap */
    set_bit(ribbon_id, ribbon_metadata->ribbon_bitmapInUse);

    /* Increase total number of ribbon count in mm_struct */
    atomic_inc(&mm->num_ribbons);

    printk(KERN_INFO "Created new ribbon with ID %d, #ribbons: %d / %d\n", 
            ribbon_id, atomic_read(&mm->num_ribbons), MAX_RIBBON);
    return ribbon_id;
}
EXPORT_SYMBOL(ribbon_create);

int ribbon_kill(int ribbon_id){

    struct mm_struct *mm = current->mm;
    struct ribbon_struct *ribbon_metadata = mm->ribbon_metadata;

    /* TODO: check if current task has the permission to delete the ribbon */
    
    /* Clear ribbon_id-th bit in ribbon_bitmapInUse */
    if( test_bit(ribbon_id, ribbon_metadata->ribbon_bitmapInUse) ) {
        clear_bit(ribbon_id, ribbon_metadata->ribbon_bitmapInUse);  
    } else {
        printk(KERN_ERR "Error, trying to delete a ribbon that does not exist: ribbon %d\n", ribbon_id);
        return -1;
    }

    /* TODO: clear all ribbon_bitmap(Read/Write/Execute) bits for this ribbon */

    printk(KERN_INFO "Deleted ribbon with ID %d, #ribbons: %d / %d\n", 
            ribbon_id, atomic_read(&mm->num_ribbons), MAX_RIBBON);
    return 0;
}
EXPORT_SYMBOL(ribbon_kill);

int ribbon_join_memdom(int memdom_id, int ribbon_id){

    return 0;
}
EXPORT_SYMBOL(ribbon_join_memdom);

int ribbon_leave_memdom(int memdom_id, int ribbon_id){

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
