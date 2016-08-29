#include <linux/smv.h>
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

/* SLAB cache for smv_struct structure  */
static struct kmem_cache *smv_cachep;

/// ---------------------------------------------------------------------------------------------  ///
/// ---------------------- Functions exported to user space to manage metadata ------------------  ///
/// ---------------------------------------------------------------------------------------------  ///
/* Telling the kernel that this process will be using the secure memory view model */
int smv_main_init(void){
    struct mm_struct *mm = current->mm;
    int smv_id = -1;
    int memdom_id = -1;

    if( !mm ) {
        printk(KERN_ERR "[%s] current task does not have mm\n", __func__);
        return -1;
    }
    printk(KERN_INFO "[%s] ------------------ %s pid %d ------------------\n", __func__, current->comm, current->pid);
    /* Mark this mm descriptor as using smv */
    mm->using_smv = 1;

    /* Create a global smv and memdom with ID: MAIN_THREAD, and set up metadata */
    smv_id = smv_create();
    memdom_id = memdom_create();

    /* Make the global smv join the global memdom with full privileges */
    smv_join_memdom(memdom_id, smv_id);
    memdom_priv_add(memdom_id, smv_id, MEMDOM_READ | MEMDOM_WRITE | MEMDOM_EXECUTE | MEMDOM_ALLOCATE);    

    /* Initialize mm-related metadata */
    mutex_lock(&mm->smv_metadataMutex);
    mm->pgd_smv[MAIN_THREAD] = mm->pgd; // record the main thread's pgd
    mm->page_table_lock_smv[MAIN_THREAD] = mm->page_table_lock; // record the main thread's pgtable lock
    current->smv_id = MAIN_THREAD;       // main thread is using MAIN_THREAD-th (0) smv_id
    current->mmap_memdom_id = MAIN_THREAD;  // main thread is using MAIN_THREAD-th (0) as mmap_id

    /* make all existing vma in memdom_id: MAIN_THREAD */
    memdom_claim_all_vmas(MAIN_THREAD);     

    mutex_unlock(&mm->smv_metadataMutex);
    return 0;
}
EXPORT_SYMBOL(smv_main_init);

/* Create a smv and update metadata */
int smv_create(void){
    int smv_id = -1;
    struct mm_struct *mm = current->mm;
    struct smv_struct *smv = NULL;

    /* SMP: protect shared smv bitmap */    
    mutex_lock(&mm->smv_metadataMutex);

    slog(KERN_INFO "[%s] Before smv_create mm: %p, nr_pmds: %ld, nr_ptes: %ld\n", 
            __func__, mm, atomic_long_read(&mm->nr_pmds), atomic_long_read(&mm->nr_ptes));
    /* Are we having too many smvs? */
    if( atomic_read(&mm->num_smvs) == SMV_ARRAY_SIZE ) {
        goto err;
    }

    /* Find available slot in the bitmap for the new smv */
    smv_id = find_first_zero_bit(mm->smv_bitmapInUse, SMV_ARRAY_SIZE);
    if( smv_id == SMV_ARRAY_SIZE ) {
        goto err;        
    }

    /* Create the actual smv struct */
    smv = allocate_smv();
    smv->smv_id = smv_id;
    atomic_set(&smv->ntask, 0);
    bitmap_zero(smv->memdom_bitmapJoin, SMV_ARRAY_SIZE);    
    mutex_init(&smv->smv_mutex);

    /* Record this new smv to mm */
    mm->smv_metadata[smv_id] = smv;

    /* Set bit in mm's smv bitmap */
    set_bit(smv_id, mm->smv_bitmapInUse);

    /* Assign page table directory */
    smv_alloc_pgd(mm, smv_id);
    
    /* Increase total number of smv count in mm_struct */
    atomic_inc(&mm->num_smvs);

    slog(KERN_INFO "Created new smv with ID %d, #smvs: %d / %d\n", 
            smv_id, atomic_read(&mm->num_smvs), SMV_ARRAY_SIZE);
    goto out;

err:
    printk(KERN_ERR "Too many smvs, cannot create more.\n");
    smv_id = -1;
out:
    slog(KERN_INFO "[%s] After smv_create mm: %p, nr_pmds: %ld, nr_ptes: %ld\n", 
            __func__, mm, atomic_long_read(&mm->nr_pmds), atomic_long_read(&mm->nr_ptes));

    mutex_unlock(&mm->smv_metadataMutex);
    return smv_id;
}
EXPORT_SYMBOL(smv_create);

int smv_kill(int smv_id, struct mm_struct *mm){
    struct smv_struct *smv = NULL; 
    int memdom_id = 0;

    /* Cannot kill global smv or smvs with ID greater than LAST_RIBBON_INDEX */
    if( smv_id > LAST_RIBBON_INDEX ) {
        printk(KERN_ERR "[%s] Error, out of bound: smv %d\n", __func__, smv_id);
        return -1;
    }
    
    /* When user space program calls smv_kill, mm_struct is NULL
     * If free_all_smvs calls this function, it passes the about-to-destroy mm_struct, not current->mm */
    if( !mm ) {
        mm = current->mm;        
    }

    /* SMP: protect shared smv bitmap */
    mutex_lock(&mm->smv_metadataMutex);
    smv = mm->smv_metadata[smv_id]; 

    slog(KERN_INFO "[%s] killing smv metadata %p with ID %d\n", __func__, smv, smv_id);
    /* TODO: check if current task has the permission to delete the smv, only master thread can do this */
    
    /* Clear smv_id-th bit in mm's smv_bitmapInUse */
    if( test_bit(smv_id, mm->smv_bitmapInUse) ) {
        clear_bit(smv_id, mm->smv_bitmapInUse);  
        mutex_unlock(&mm->smv_metadataMutex);
    } else {
        printk(KERN_ERR "Error, trying to delete a smv that does not exist: smv %d, #smvs: %d\n", smv_id, atomic_read(&mm->num_smvs));
        mutex_unlock(&mm->smv_metadataMutex);
        return -1;
    }

    /* Clear all smv_bitmap(Read/Write/Execute/Allocate) bits for this smv in all memdoms */  
    slog(KERN_INFO "[%s] leaving all the joined memdoms\n", __func__);
    do {       
        mutex_lock(&smv->smv_mutex);
        memdom_id = find_first_bit(smv->memdom_bitmapJoin, SMV_ARRAY_SIZE);
        mutex_unlock(&smv->smv_mutex);
        if( memdom_id != SMV_ARRAY_SIZE ) {
            smv_leave_memdom(memdom_id, smv_id, mm);
        }
    } while( memdom_id != SMV_ARRAY_SIZE );
    
    /* Free all page tables, then pgd. MAIN_THREAD is using process's original pgd. Will be freed in fork.c */
    if (smv_id != MAIN_THREAD) {
        down_write(&mm->mmap_sem);
        smv_free_mmap(mm, smv_id);
        pgd_free(mm, mm->pgd_smv[smv_id]);
        up_write(&mm->mmap_sem);
    } else{
        slog(KERN_INFO "[%s] skip killing main thread's page tables. Will be done in exit_mmap()\n", __func__);
    }
    
    /* Free the actual smv struct */
    free_smv(smv);
    
    /* Decrement smv count */
    mutex_lock(&mm->smv_metadataMutex);
    mm->smv_metadata[smv_id] = NULL;
    atomic_dec(&mm->num_smvs);
    mutex_unlock(&mm->smv_metadataMutex);

    slog(KERN_INFO "[%s] Deleted smv with ID %d, #smvs: %d / %d\n", 
            __func__, smv_id, atomic_read(&mm->num_smvs), SMV_ARRAY_SIZE);

    return 0;
}
EXPORT_SYMBOL(smv_kill);

/* Free all the smvs in this mm_struct */
void free_all_smvs(struct mm_struct *mm){
    int index = 0;
    while( atomic_read(&mm->num_smvs) > 0 ){
        index = find_first_bit(mm->smv_bitmapInUse, SMV_ARRAY_SIZE);
        slog(KERN_INFO "[%s] killing smv %d, remaining #smvs: %d\n", __func__, index, atomic_read(&mm->num_smvs));
        smv_kill(index, mm);
    }
}

// Set memdom_id-th bit for smv
int smv_join_memdom(int memdom_id, int smv_id){
    struct smv_struct *smv = NULL; 
    struct memdom_struct *memdom = NULL; 
    struct mm_struct *mm = current->mm;

    if( smv_id > LAST_RIBBON_INDEX  || memdom_id > LAST_MEMDOM_INDEX) {
        printk(KERN_ERR "[%s] Error, out of bound: smv %d, memdom %d\n", __func__, smv_id, memdom_id);
        return -1;
    }

    mutex_lock(&mm->smv_metadataMutex);
    smv = current->mm->smv_metadata[smv_id];
    memdom = current->mm->memdom_metadata[memdom_id];
    if( !memdom || !smv ) {
        printk(KERN_ERR "[%s] memdom %d: %p || smv %d: %p not found\n", __func__, memdom_id, memdom, smv_id, smv);
        mutex_unlock(&mm->smv_metadataMutex);
        return -1;
    }   
    mutex_unlock(&mm->smv_metadataMutex);

    mutex_lock(&smv->smv_mutex);
    set_bit(memdom_id, smv->memdom_bitmapJoin);
    mutex_unlock(&smv->smv_mutex);

    slog(KERN_INFO "[%s] smv id %d joined memdom %d\n", __func__, smv_id, memdom_id);
    return 0;
}
EXPORT_SYMBOL(smv_join_memdom);

// Clear smv_id-th bit for R/W/E/A in memdom
int smv_leave_memdom(int memdom_id, int smv_id, struct mm_struct *mm){
    struct memdom_struct *memdom = NULL;   
    struct smv_struct *smv = NULL;

    if( smv_id > LAST_RIBBON_INDEX  || memdom_id > LAST_MEMDOM_INDEX) {
        printk(KERN_ERR "[%s] Error, out of bound: smv %d, memdom %d\n", __func__, smv_id, memdom_id);
        return -1;
    }

    slog(KERN_INFO "[%s] smv %d leaving memdom %d\n", __func__, smv_id, memdom_id);

    /* mm is not NULL is called by smv_kill() */
    if( mm == NULL ) {
        mm = current->mm;
    }

    /* Get the actual memdom and smv struct from this mm */
    mutex_lock(&mm->smv_metadataMutex);
    memdom = mm->memdom_metadata[memdom_id];
    smv = mm->smv_metadata[smv_id];
    mutex_unlock(&mm->smv_metadataMutex);
    if( !memdom || !smv ) {
        printk(KERN_ERR "[%s] memdom %p || smv %p not found\n", __func__, memdom, smv);
        return -1;
    }
    slog(KERN_INFO "[%s] memdom %p, smv %p\n", __func__, memdom, smv);

    /* Clear smv_id-th bit in the bitmap for memdom */
    mutex_lock(&memdom->memdom_mutex);
    clear_bit(smv_id, memdom->smv_bitmapRead);
    clear_bit(smv_id, memdom->smv_bitmapWrite);
    clear_bit(smv_id, memdom->smv_bitmapExecute);
    clear_bit(smv_id, memdom->smv_bitmapAllocate);
    mutex_unlock(&memdom->memdom_mutex);

    /* Clear memdom_id-th bit in the bitmap for smv */
    mutex_lock(&smv->smv_mutex);
    clear_bit(memdom_id, smv->memdom_bitmapJoin);
    mutex_unlock(&smv->smv_mutex);
    return 0;
}
EXPORT_SYMBOL(smv_leave_memdom);

/* Check if the smv has joined the memdom, 1 if yes, 0 otherwise */
int smv_is_in_memdom(int memdom_id, int smv_id){
    struct smv_struct *smv; 
    struct mm_struct *mm = current->mm;
    int in = 0;    

    if( smv_id > LAST_RIBBON_INDEX  || memdom_id > LAST_MEMDOM_INDEX) {
        printk(KERN_ERR "[%s] Error, out of bound: smv %d, memdom %d\n", __func__, smv_id, memdom_id);
        return 0;
    }

    mutex_lock(&mm->smv_metadataMutex);
    smv = current->mm->smv_metadata[smv_id];
    mutex_unlock(&mm->smv_metadataMutex);

    if( !smv ) {
        printk(KERN_ERR "[%s] smv %p not found\n", __func__, smv);
        return 0;        
    }
    mutex_lock(&smv->smv_mutex);
    if( test_bit(memdom_id, smv->memdom_bitmapJoin) ) {
        in = 1;
    }
    mutex_unlock(&smv->smv_mutex);
    return in;
}
EXPORT_SYMBOL(smv_is_in_memdom);

/* Check if a smv exists, 1 if yes, 0 otherwise */
int smv_exists(int smv_id){
    struct smv_struct *smv = NULL; 
    struct mm_struct *mm = current->mm;

    if( smv_id > LAST_RIBBON_INDEX ) {
        printk(KERN_ERR "[%s] Error, out of bound: smv %d\n", __func__, smv_id);
        return 0;
    }
    
    /* TODO: add privilege checks */

    mutex_lock(&mm->smv_metadataMutex);
    smv = current->mm->smv_metadata[smv_id];
    mutex_unlock(&mm->smv_metadataMutex);

    if( !smv ) {
        printk(KERN_ERR "[%s] smv %p does not exist.\n", __func__, smv);
        return 0;        
    }
    return 1;
}
EXPORT_SYMBOL(smv_exists);

int smv_get_smv_id(void){    
    return current->smv_id;
}
EXPORT_SYMBOL(smv_get_smv_id);

/* Put smv_id in mm struct for do_fork to use, return -1 if smv_id does not exist */
int register_smv_thread(int smv_id){
    struct mm_struct *mm = current->mm;

    /* A child smv cannot register itself to MAIN_THREAD or a non-existing smv */
    if( smv_id == MAIN_THREAD || smv_id > LAST_RIBBON_INDEX ) {
        printk(KERN_ERR "[%s] Error, out of bound: smv %d\n", __func__, smv_id);
        return -1;
    }

    /* Tell the kernel we are about to run a new thread in a smv */
    mutex_lock(&mm->smv_metadataMutex);
    if( !test_bit(smv_id, mm->smv_bitmapInUse) ) {
        printk(KERN_ERR "[%s] smv %d not found\n", __func__, smv_id);
        mutex_unlock(&mm->smv_metadataMutex);
        return -1;
    }
    mm->standby_smv_id = smv_id;  // Will be reset to MAIN_THREAD when do_fork exits.
    mutex_unlock(&mm->smv_metadataMutex);

    /* Update number of tasks running in the smv */
    // TODO: Call atomic_dec when task exits the system
    mutex_lock(&mm->smv_metadata[smv_id]->smv_mutex);
    atomic_inc(&mm->smv_metadata[smv_id]->ntask); 
    mutex_unlock(&mm->smv_metadata[smv_id]->smv_mutex);

    return 0;
}
EXPORT_SYMBOL(register_smv_thread);


/// ---------------------------------------------------------------------------------------------  ///
/// ------------------ Functions called by kernel internally to manage memory space -------------  ///
/// ---------------------------------------------------------------------------------------------  ///
void smv_init(void){
    smv_cachep = kmem_cache_create("smv_struct",
                                      sizeof(struct smv_struct), 0,
                                      SLAB_HWCACHE_ALIGN|SLAB_NOTRACK, NULL);
    if( !smv_cachep ) {
        printk(KERN_ERR "[%s] smv slab initialization failed...\n", __func__);
    } else{
        printk(KERN_INFO "[%s] smv slab initialized\n", __func__);
    }
}


/* Allocate a pgd for the new smv */
pgd_t *smv_alloc_pgd(struct mm_struct *mm, int smv_id){
    pgd_t *pgd = NULL;

    if( !mm->using_smv ) {
        printk(KERN_ERR "[%s] Error: current mm is not using smv model.\n", __func__);
        return NULL;
    }

    /* Allcoate pgd. MAIN_THREAD with smv id 0 already has pgd, just record it */
    if( smv_id == 0 ) {    
        pgd = mm->pgd;
        mm->page_table_lock_smv[smv_id] = mm->page_table_lock;
    } else {
        pgd = pgd_alloc(mm); // see implementation in pgtable.c
        if( unlikely(!pgd) ) { 
            printk(KERN_ERR "[%s] failed to allocate new pgd.\n", __func__);
            return NULL;
        }
        /* Init page table lock */
        spin_lock_init(&mm->page_table_lock_smv[smv_id]);
    }

    /* Assign page table directory to mm_struct for smv_id */
    mm->pgd_smv[smv_id] = pgd;

    slog(KERN_INFO "[%s] smv %d pgd %p\n", __func__, smv_id, mm->pgd_smv[smv_id]);
    return pgd;
}

/* Free a pgd for smv */
void smv_free_pgd(struct mm_struct *mm, int smv_id){
    free_page((unsigned long)mm->pgd_smv[smv_id]);
}

/* Hook for security context switch from one smv to another (change secure memory view) 
 */
void switch_smv(struct task_struct *prev_tsk, struct task_struct *next_tsk, 
                   struct mm_struct *prev_mm, struct mm_struct *next_mm){

    /* Skip smv context switch if the next tasks is not in any smvs, or if next_mm is NULL */
    if( (next_tsk && next_tsk->smv_id == -1) || 
         next_mm == NULL) {
        return;
    }
}

/* See implementation in memory.c */
void smv_free_pgtables(struct mmu_gather *tlb, struct vm_area_struct *vma,
                    		unsigned long floor, unsigned long ceiling){
    while (vma) {
		struct vm_area_struct *next = vma->vm_next;
		unsigned long addr = vma->vm_start;
		free_pgd_range(tlb, addr, vma->vm_end, floor, next? next->vm_start: ceiling);
		vma = next;
    }
}

/* Free page tables for a smv 
 * Caller must hold the mm semaphore */
void smv_free_mmap(struct mm_struct *mm, int smv_id){
    struct vm_area_struct *vma = mm->mmap;
	struct mmu_gather tlb;

    /* Can happen if dup_mmap() received an OOM */
	if (!vma) {
		return;
    }

    /* Leave the chores of cleaning page tables to the main thread when the process exits the system by do_exit() */
    if( smv_id == MAIN_THREAD ) {
        return;
    }

    /* Ribbon thread cleans its own page tables information 
     * Question: should we shootdown TLB? 
     */
    else {
        slog(KERN_INFO "[%s] Free pgtables for smv %d\n", __func__, smv_id);
        slog(KERN_INFO "[%s] Before smv_free_mmap mm: %p, nr_pmds: %ld, nr_ptes: %ld\n", 
                __func__, mm, atomic_long_read(&mm->nr_pmds), atomic_long_read(&mm->nr_ptes));
        tlb_gather_mmu(&tlb, mm, 0, -1);
        update_hiwater_rss(mm);

        /* Overwrite the smv_id to be freed. tlb_gather_mmu set tlb.smv_id to be current->smv_id.
         * However, this function could be called by the main thread (smv_id = 0) when the process 
         * exiting the system to free the page tables for other smvs (smv_id !=0). 
         * So here we need to set the correct smv_id for unmap_vmas and smv_free_pgtables. */
        tlb.smv_id = smv_id; 

        /* Do the actual job of freeing page tables */
        unmap_vmas(&tlb, vma, 0, -1);
        smv_free_pgtables(&tlb, vma, FIRST_USER_ADDRESS, USER_PGTABLES_CEILING);       

       	tlb_finish_mmu(&tlb, 0, -1);
        slog(KERN_INFO "[%s] After smv_free_mmap mm: %p, nr_pmds: %ld, nr_ptes: %ld\n", 
                __func__, mm, atomic_long_read(&mm->nr_pmds), atomic_long_read(&mm->nr_ptes));
    }
}
