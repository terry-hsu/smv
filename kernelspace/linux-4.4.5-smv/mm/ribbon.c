#include <linux/ribbon.h>
#include <linux/memdom.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/gfp.h>
#include <asm/pgalloc.h>

#define PGALLOC_GFP GFP_KERNEL | __GFP_NOTRACK | __GFP_REPEAT | __GFP_ZERO

/* SLAB cache for ribbon_struct structure  */
static struct kmem_cache *ribbon_cachep;

/// ---------------------------------------------------------------------------------------------  ///
/// ---------------------- Functions exported to user space to manage metadata ------------------  ///
/// ---------------------------------------------------------------------------------------------  ///
/* Telling the kernel that this process will be using the secure memory view model */
int ribbon_main_init(void){
    struct mm_struct *mm = current->mm;
    if( !mm ) {
        printk(KERN_ERR "[%s] current task does not have mm\n", __func__);
        return -1;
    }
    mutex_lock(&mm->smv_metadataMutex);
    mm->using_smv = 1;
    mm->pgd_ribbon[MAIN_THREAD] = mm->pgd; // record the main thread's pgd
    mm->page_table_lock_ribbon[MAIN_THREAD] = mm->page_table_lock; // record the main thread's pgtable lock
    mutex_unlock(&mm->smv_metadataMutex);
    return 0;
}
EXPORT_SYMBOL(ribbon_main_init);

/* Create a ribbon and update metadata */
int ribbon_create(void){
    int ribbon_id = -1;
    struct mm_struct *mm = current->mm;
    struct ribbon_struct *ribbon = NULL;

    /* SMP: protect shared ribbon bitmap */
    mutex_lock(&mm->smv_metadataMutex);

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

    /* Assign page table directory */
    ribbon_alloc_pgd(mm, ribbon_id);

    /* Increase total number of ribbon count in mm_struct */
    atomic_inc(&mm->num_ribbons);

    printk(KERN_INFO "Created new ribbon with ID %d, #ribbons: %d / %d\n", 
            ribbon_id, atomic_read(&mm->num_ribbons), MAX_RIBBON);
    goto out;

err:
    printk(KERN_ERR "Too many ribbons, cannot create more.\n");
    ribbon_id = -1;
out:
    mutex_unlock(&mm->smv_metadataMutex);
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
    mutex_lock(&mm->smv_metadataMutex);
    ribbon = mm->ribbon_metadata[ribbon_id]; 

    /* TODO: check if current task has the permission to delete the ribbon, only master thread can do this */
    
    /* Clear ribbon_id-th bit in mm's ribbon_bitmapInUse */
    if( test_bit(ribbon_id, mm->ribbon_bitmapInUse) ) {
        clear_bit(ribbon_id, mm->ribbon_bitmapInUse);  
    } else {
        printk(KERN_ERR "Error, trying to delete a ribbon that does not exist: ribbon %d, #ribbons: %d\n", ribbon_id, atomic_read(&mm->num_ribbons));
        mutex_unlock(&mm->smv_metadataMutex);
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

    /* TODO: Free page tables */

    /* Decrement ribbon count */
    atomic_dec(&mm->num_ribbons);

    printk(KERN_INFO "Deleted ribbon with ID %d, #ribbons: %d / %d\n", 
            ribbon_id, atomic_read(&mm->num_ribbons), MAX_RIBBON);

    mutex_unlock(&mm->smv_metadataMutex);
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

// Set memdom_id-th bit for ribbon
int ribbon_join_memdom(int memdom_id, int ribbon_id){
    struct ribbon_struct *ribbon = NULL; 
    struct memdom_struct *memdom = NULL; 
    struct mm_struct *mm = current->mm;

    mutex_lock(&mm->smv_metadataMutex);
    ribbon = current->mm->ribbon_metadata[ribbon_id];
    memdom = current->mm->memdom_metadata[memdom_id];
    if( !memdom || !ribbon ) {
        printk(KERN_ERR "[%s] memdom %d: %p || ribbon %d: %p not found\n", __func__, memdom_id, memdom, ribbon_id, ribbon);
        mutex_unlock(&mm->smv_metadataMutex);
        return -1;
    }   
    mutex_unlock(&mm->smv_metadataMutex);

    mutex_lock(&ribbon->ribbon_mutex);
    set_bit(memdom_id, ribbon->memdom_bitmapJoin);
    mutex_unlock(&ribbon->ribbon_mutex);

    printk(KERN_INFO "[%s] ribbon id %d joined memdom %d\n", __func__, ribbon_id, memdom_id);
    return 0;
}
EXPORT_SYMBOL(ribbon_join_memdom);

// Clear ribbon_id-th bit for R/W/E/A in memdom
int ribbon_leave_memdom(int memdom_id, int ribbon_id, struct mm_struct *mm){
    struct memdom_struct *memdom = NULL;   
    struct ribbon_struct *ribbon = NULL;
    /* mm is not NULL is called by ribbon_kill() */
    if( mm == NULL ) {
        mm = current->mm;
    }

    /* Get the actual memdom and ribbon struct from this mm */
    mutex_lock(&mm->smv_metadataMutex);
    memdom = mm->memdom_metadata[memdom_id];
    ribbon = mm->ribbon_metadata[ribbon_id];
    mutex_unlock(&mm->smv_metadataMutex);
    if( !memdom || !ribbon ) {
        printk(KERN_ERR "[%s] memdom %p || ribbon %p not found\n", __func__, memdom, ribbon);
        return -1;
    }
    printk(KERN_ERR "[%s] memdom %p, ribbon %p\n", __func__, memdom, ribbon);

    /* Clear ribbon_id-th bit in the bitmap for memdom */
    mutex_lock(&memdom->memdom_mutex);
    clear_bit(ribbon_id, memdom->ribbon_bitmapRead);
    clear_bit(ribbon_id, memdom->ribbon_bitmapWrite);
    clear_bit(ribbon_id, memdom->ribbon_bitmapExecute);
    clear_bit(ribbon_id, memdom->ribbon_bitmapAllocate);
    mutex_unlock(&memdom->memdom_mutex);

    /* Clear memdom_id-th bit in the bitmap for ribbon */
    mutex_lock(&ribbon->ribbon_mutex);
    clear_bit(memdom_id, ribbon->memdom_bitmapJoin);
    mutex_unlock(&ribbon->ribbon_mutex);
    return 0;
}
EXPORT_SYMBOL(ribbon_leave_memdom);

/* Check if the ribbon has joined the memdom, 1 if yes, 0 otherwise */
int ribbon_is_in_memdom(int memdom_id, int ribbon_id){
    struct ribbon_struct *ribbon; 
    struct mm_struct *mm = current->mm;
    int in = 0;    

    mutex_lock(&mm->smv_metadataMutex);
    ribbon = current->mm->ribbon_metadata[ribbon_id];
    mutex_unlock(&mm->smv_metadataMutex);

    if( !ribbon ) {
        printk(KERN_ERR "[%s] ribbon %p not found\n", __func__, ribbon);
        return 0;        
    }
    mutex_lock(&ribbon->ribbon_mutex);
    if( test_bit(memdom_id, ribbon->memdom_bitmapJoin) ) {
        in = 1;
    }
    mutex_unlock(&ribbon->ribbon_mutex);
    return in;
}
EXPORT_SYMBOL(ribbon_is_in_memdom);

int ribbon_get_ribbon_id(void){

    return 0;
}
EXPORT_SYMBOL(ribbon_get_ribbon_id);

/* Put ribbon_id in mm struct for do_fork to use, return -1 if ribbon_id does not exist */
int register_ribbon_thread(int ribbon_id){
    struct mm_struct *mm = current->mm;
    mutex_lock(&mm->smv_metadataMutex);
    if( !test_bit(ribbon_id, mm->ribbon_bitmapInUse) ) {
        printk(KERN_ERR "[%s] ribbon %d not found\n", __func__, ribbon_id);
        return -1;
    }
    mm->standby_ribbon_id = ribbon_id;  // Will be reset to -1 when do_fork exits.
    mutex_unlock(&mm->smv_metadataMutex);
    return 0;
}
EXPORT_SYMBOL(register_ribbon_thread);


/// ---------------------------------------------------------------------------------------------  ///
/// ------------------ Functions called by kernel internally to manage memory space -------------  ///
/// ---------------------------------------------------------------------------------------------  ///
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


/* Allocate a pgd for the new ribbon */
pgd_t *ribbon_alloc_pgd(struct mm_struct *mm, int ribbon_id){
    pgd_t *pgd = NULL;

    if( !mm->using_smv ) {
        printk(KERN_ERR "[%s] current mm is not using smv model.\n", __func__);
        return NULL;
    }

    /* Allcoate pgd */
	pgd = (pgd_t *)__get_free_page(PGALLOC_GFP); // see implementation in pgtable.c
    if( pgd == NULL ) { 
        printk(KERN_ERR "[%s] failed to allocate new pgd.\n", __func__);
        return NULL;
    }

    /* Assign to mm_struct for ribbon_id */
    mm->pgd_ribbon[ribbon_id] = pgd;
    return pgd;
}

/* Free a pgd for ribbon */
void ribbon_free_pgd(struct mm_struct *mm, int ribbon_id){
    free_page((unsigned long)mm->pgd_ribbon[ribbon_id]);
}

/* Security context switch from one ribbon to another (change secure memory view) */
void switch_ribbon(struct task_struct *prev_tsk, struct task_struct *next_tsk, struct mm_struct *next_mm){

    /* 1. idle_task_exit() in core.c could pass NULL prev when taking a core offline.  
     * 2. use_mm() in mmu_context.c could pass NULL when a kernel thread switching mm.
       3. activate_mm() in mmu_context.h could pass NULL prev and next.
       Skip ribbon context switch in these cases.
     */
    if( prev_tsk == NULL ) {
        return;
    }

    /* Skip ribbon context switch if none of the tasks are in any ribbons */
    if( prev_tsk->ribbon_id == -1 && next_tsk->ribbon_id == -1 ) {
        return;
    }

    /* Tell the kernel what page tables the ribbon will be using */
//  next_mm->pgd = next_mm->pgd_ribbon[next_tsk->ribbon_id];
//  next_mm->page_table_lock = next_mm->page_table_lock_ribbon[next_tsk->ribbon_id];

    printk(KERN_INFO "[%s] prev ribbon %d switched to next ribbon %d\n", __func__, prev_tsk->ribbon_id, next_tsk->ribbon_id);       
}
