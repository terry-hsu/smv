#include <linux/ribbon.h>
#include <linux/memdom.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/gfp.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <linux/smp.h>
#include <linux/mm.h>
#include <asm-generic/tlb.h>

#define PGALLOC_GFP GFP_KERNEL | __GFP_NOTRACK | __GFP_REPEAT | __GFP_ZERO

/* SLAB cache for ribbon_struct structure  */
static struct kmem_cache *ribbon_cachep;

/// ---------------------------------------------------------------------------------------------  ///
/// ---------------------- Functions exported to user space to manage metadata ------------------  ///
/// ---------------------------------------------------------------------------------------------  ///
/* Telling the kernel that this process will be using the secure memory view model */
int ribbon_main_init(void){
    struct mm_struct *mm = current->mm;
    int ribbon_id = -1;
    int memdom_id = -1;

    if( !mm ) {
        printk(KERN_ERR "[%s] current task does not have mm\n", __func__);
        return -1;
    }
    printk(KERN_INFO "[%s] ------------------ %s pid %d ------------------\n", __func__, current->comm, current->pid);
    /* Mark this mm descriptor as using smv */
    mm->using_smv = 1;

    /* Create a global ribbon and memdom with ID: MAIN_THREAD, and set up metadata */
    ribbon_id = ribbon_create();
    memdom_id = memdom_create();

    /* Make the global ribbon join the global memdom with full privileges */
    ribbon_join_memdom(memdom_id, ribbon_id);
    memdom_priv_add(memdom_id, ribbon_id, MEMDOM_READ | MEMDOM_WRITE | MEMDOM_EXECUTE | MEMDOM_ALLOCATE);    

    /* Initialize mm-related metadata */
    mutex_lock(&mm->smv_metadataMutex);
    mm->pgd_ribbon[MAIN_THREAD] = mm->pgd; // record the main thread's pgd
    mm->page_table_lock_ribbon[MAIN_THREAD] = mm->page_table_lock; // record the main thread's pgtable lock
    current->ribbon_id = MAIN_THREAD;       // main thread is using MAIN_THREAD-th ribbon_id

    /* make all existing vma in memdom_id: MAIN_THREAD */
    memdom_claim_all_vmas(MAIN_THREAD);     

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

    printk(KERN_INFO "[%s] Before ribbon_create mm: %p, nr_pmds: %ld, nr_ptes: %ld\n", 
            __func__, mm, atomic_long_read(&mm->nr_pmds), atomic_long_read(&mm->nr_ptes));
    /* Are we having too many ribbons? */
    if( atomic_read(&mm->num_ribbons) == SMV_ARRAY_SIZE ) {
        goto err;
    }

    /* Find available slot in the bitmap for the new ribbon */
    ribbon_id = find_first_zero_bit(mm->ribbon_bitmapInUse, SMV_ARRAY_SIZE);
    if( ribbon_id == SMV_ARRAY_SIZE ) {
        goto err;        
    }

    /* Create the actual ribbon struct */
    ribbon = allocate_ribbon();
    ribbon->ribbon_id = ribbon_id;
    atomic_set(&ribbon->ntask, 0);
    bitmap_zero(ribbon->memdom_bitmapJoin, SMV_ARRAY_SIZE);    
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
            ribbon_id, atomic_read(&mm->num_ribbons), SMV_ARRAY_SIZE);
    goto out;

err:
    printk(KERN_ERR "Too many ribbons, cannot create more.\n");
    ribbon_id = -1;
out:
    printk(KERN_INFO "[%s] After ribbon_create mm: %p, nr_pmds: %ld, nr_ptes: %ld\n", 
            __func__, mm, atomic_long_read(&mm->nr_pmds), atomic_long_read(&mm->nr_ptes));

    mutex_unlock(&mm->smv_metadataMutex);
    return ribbon_id;
}
EXPORT_SYMBOL(ribbon_create);

int ribbon_kill(int ribbon_id, struct mm_struct *mm){
    struct ribbon_struct *ribbon = NULL; 
    int memdom_id = 0;

    /* Cannot kill global ribbon or ribbons with ID greater than LAST_RIBBON_INDEX */
    if( ribbon_id > LAST_RIBBON_INDEX ) {
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

    printk(KERN_INFO "[%s] killing ribbon metadata %p with ID %d\n", __func__, ribbon, ribbon_id);
    /* TODO: check if current task has the permission to delete the ribbon, only master thread can do this */
    
    /* Clear ribbon_id-th bit in mm's ribbon_bitmapInUse */
    if( test_bit(ribbon_id, mm->ribbon_bitmapInUse) ) {
        clear_bit(ribbon_id, mm->ribbon_bitmapInUse);  
        mutex_unlock(&mm->smv_metadataMutex);
    } else {
        printk(KERN_ERR "Error, trying to delete a ribbon that does not exist: ribbon %d, #ribbons: %d\n", ribbon_id, atomic_read(&mm->num_ribbons));
        mutex_unlock(&mm->smv_metadataMutex);
        return -1;
    }

    /* Clear all ribbon_bitmap(Read/Write/Execute/Allocate) bits for this ribbon in all memdoms */  
    printk(KERN_INFO "[%s] leaving all the joined memdoms\n", __func__);
    do {       
        mutex_lock(&ribbon->ribbon_mutex);
        memdom_id = find_first_bit(ribbon->memdom_bitmapJoin, SMV_ARRAY_SIZE);
        mutex_unlock(&ribbon->ribbon_mutex);
        if( memdom_id != SMV_ARRAY_SIZE ) {
            ribbon_leave_memdom(memdom_id, ribbon_id, mm);
        }
    } while( memdom_id != SMV_ARRAY_SIZE );
    
    /* Free all page tables, then pgd. MAIN_THREAD is using process's original pgd. Will be freed in fork.c */
    if (ribbon_id != MAIN_THREAD) {
        ribbon_free_mmap(mm, ribbon_id);
        pgd_free(mm, mm->pgd_ribbon[ribbon_id]);
    } else{
        printk(KERN_INFO "[%s] skip killing main thread's page tables. Will be done in exit_mmap()\n", __func__);
    }
    
    /* Free the actual ribbon struct */
    free_ribbon(ribbon);
    mm->ribbon_metadata[ribbon_id] = NULL;
    
    /* Decrement ribbon count */
    mutex_lock(&mm->smv_metadataMutex);
    atomic_dec(&mm->num_ribbons);
    mutex_unlock(&mm->smv_metadataMutex);

    printk(KERN_INFO "[%s] Deleted ribbon with ID %d, #ribbons: %d / %d\n", 
            __func__, ribbon_id, atomic_read(&mm->num_ribbons), SMV_ARRAY_SIZE);

    return 0;
}
EXPORT_SYMBOL(ribbon_kill);

/* Free all the ribbons in this mm_struct */
void free_all_ribbons(struct mm_struct *mm){
    int index = 0;
    while( atomic_read(&mm->num_ribbons) > 0 ){
        index = find_first_bit(mm->ribbon_bitmapInUse, SMV_ARRAY_SIZE);
        printk(KERN_INFO "[%s] killing ribbon %d, remaining #ribbons: %d\n", __func__, index, atomic_read(&mm->num_ribbons));
        ribbon_kill(index, mm);
    }
}

// Set memdom_id-th bit for ribbon
int ribbon_join_memdom(int memdom_id, int ribbon_id){
    struct ribbon_struct *ribbon = NULL; 
    struct memdom_struct *memdom = NULL; 
    struct mm_struct *mm = current->mm;

    if( ribbon_id > LAST_RIBBON_INDEX  || memdom_id > LAST_MEMDOM_INDEX) {
        printk(KERN_ERR "[%s] Error, out of bound: ribbon %d, memdom %d\n", __func__, ribbon_id, memdom_id);
        return -1;
    }

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

    if( ribbon_id > LAST_RIBBON_INDEX  || memdom_id > LAST_MEMDOM_INDEX) {
        printk(KERN_ERR "[%s] Error, out of bound: ribbon %d, memdom %d\n", __func__, ribbon_id, memdom_id);
        return -1;
    }

    printk(KERN_ERR "[%s] ribbon %d leaving memdom %d\n", __func__, ribbon_id, memdom_id);

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

    if( ribbon_id > LAST_RIBBON_INDEX  || memdom_id > LAST_MEMDOM_INDEX) {
        printk(KERN_ERR "[%s] Error, out of bound: ribbon %d, memdom %d\n", __func__, ribbon_id, memdom_id);
        return 0;
    }

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

    /* A child ribbon cannot register itself to MAIN_THREAD or a non-existing ribbon */
    if( ribbon_id == MAIN_THREAD || ribbon_id > LAST_RIBBON_INDEX ) {
        printk(KERN_ERR "[%s] Error, out of bound: ribbon %d\n", __func__, ribbon_id);
        return -1;
    }

    /* Tell the kernel we are about to run a new thread in a ribbon */
    mutex_lock(&mm->smv_metadataMutex);
    if( !test_bit(ribbon_id, mm->ribbon_bitmapInUse) ) {
        printk(KERN_ERR "[%s] ribbon %d not found\n", __func__, ribbon_id);
        mutex_unlock(&mm->smv_metadataMutex);
        return -1;
    }
    mm->standby_ribbon_id = ribbon_id;  // Will be reset to MAIN_THREAD when do_fork exits.
    mutex_unlock(&mm->smv_metadataMutex);

    /* Update number of tasks running in the ribbon */
    // TODO: Call atomic_dec when task exits the system
    mutex_lock(&mm->ribbon_metadata[ribbon_id]->ribbon_mutex);
    atomic_inc(&mm->ribbon_metadata[ribbon_id]->ntask); 
    mutex_unlock(&mm->ribbon_metadata[ribbon_id]->ribbon_mutex);

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
        printk(KERN_ERR "[%s] Error: current mm is not using smv model.\n", __func__);
        return NULL;
    }

    /* Allcoate pgd. MAIN_THREAD with ribbon id 0 already has pgd, just record it */
    if( ribbon_id == 0 ) {    
        pgd = mm->pgd;
        mm->page_table_lock_ribbon[ribbon_id] = mm->page_table_lock;
    } else {
        pgd = pgd_alloc(mm); // see implementation in pgtable.c
        if( unlikely(!pgd) ) { 
            printk(KERN_ERR "[%s] failed to allocate new pgd.\n", __func__);
            return NULL;
        }
        /* Init page table lock */
        spin_lock_init(&mm->page_table_lock_ribbon[ribbon_id]);
    }

    /* Assign page table directory to mm_struct for ribbon_id */
    mm->pgd_ribbon[ribbon_id] = pgd;

    printk(KERN_INFO "[%s] ribbon %d pgd %p\n", __func__, ribbon_id, mm->pgd_ribbon[ribbon_id]);
    return pgd;
}

/* Free a pgd for ribbon */
void ribbon_free_pgd(struct mm_struct *mm, int ribbon_id){
    free_page((unsigned long)mm->pgd_ribbon[ribbon_id]);
}

/* Hook for security context switch from one ribbon to another (change secure memory view) 
 */
void switch_ribbon(struct task_struct *prev_tsk, struct task_struct *next_tsk, 
                   struct mm_struct *prev_mm, struct mm_struct *next_mm){

    /* Skip ribbon context switch if the next tasks is not in any ribbons, or if next_mm is NULL */
    if( (next_tsk && next_tsk->ribbon_id == -1) || 
         next_mm == NULL) {
        return;
    }
}

/* See implementation in memory.c */
void ribbon_free_pgtables(struct mmu_gather *tlb, struct vm_area_struct *vma,
                    		unsigned long floor, unsigned long ceiling){
    while (vma) {
		struct vm_area_struct *next = vma->vm_next;
		unsigned long addr = vma->vm_start;
		free_pgd_range(tlb, addr, vma->vm_end, floor, next? next->vm_start: ceiling);
		vma = next;
    }
}

/* Free page tables for a ribbon */
void ribbon_free_mmap(struct mm_struct *mm, int ribbon_id){
    struct vm_area_struct *vma = mm->mmap;
	struct mmu_gather tlb;

    /* Can happen if dup_mmap() received an OOM */
	if (!vma) {
		return;
    }

    /* Leave the chores of cleaning page tables to the main thread when the process exits the system by do_exit() */
    if( ribbon_id == MAIN_THREAD ) {
        return;
    }

    /* Ribbon thread cleans its own page tables information 
     * Question: should we shootdown TLB? 
     */
    else {
        down_write(&mm->mmap_sem);
        printk(KERN_INFO "[%s] Free pgtables for ribbon %d\n", __func__, ribbon_id);
        printk(KERN_INFO "[%s] Before ribbon_free_mmap mm: %p, nr_pmds: %ld, nr_ptes: %ld\n", 
                __func__, mm, atomic_long_read(&mm->nr_pmds), atomic_long_read(&mm->nr_ptes));
        tlb_gather_mmu(&tlb, mm, 0, -1);
        update_hiwater_rss(mm);

        /* Overwrite the ribbon_id to be freed. tlb_gather_mmu set tlb.ribbon_id to be current->ribbon_id.
         * However, this function could be called by the main thread (ribbon_id = 0) when the process 
         * exiting the system to free the page tables for other ribbons (ribbon_id !=0). 
         * So here we need to set the correct ribbon_id for unmap_vmas and ribbon_free_pgtables. */
        tlb.ribbon_id = ribbon_id; 

        /* Do the actual job of freeing page tables */
        unmap_vmas(&tlb, vma, 0, -1);
        ribbon_free_pgtables(&tlb, vma, FIRST_USER_ADDRESS, USER_PGTABLES_CEILING);       

       	tlb_finish_mmu(&tlb, 0, -1);
        printk(KERN_INFO "[%s] After ribbon_free_mmap mm: %p, nr_pmds: %ld, nr_ptes: %ld\n", 
                __func__, mm, atomic_long_read(&mm->nr_pmds), atomic_long_read(&mm->nr_ptes));
        up_write(&mm->mmap_sem);
    }
}
