#include <linux/smv_mm.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/rmap.h>

/* Check whether current fault is a valid smv page fault.
 * Return 1 if it's a valid smv fault, 0 to block access 
 */
int smv_valid_fault(int smv_id, struct vm_area_struct *vma, unsigned long error_code){
    int memdom_id = vma->memdom_id;
    struct mm_struct *mm = current->mm;
    int privs = 0;
    int rv = 0;

    /* Skip checking for smv valid fault if 
     * 1. current task is not using smv 
     * 2. current task is using smv, but page fault triggered by Pthreads (smv_id == -1) 
     */
    if ( !mm->using_smv || (mm->using_smv && current->smv_id == -1) ) {
         return 1;
    }

    /* A fault is valid only if the smv has joined this vma's memdom */
    if ( !smv_is_in_memdom(memdom_id, smv_id) ) {
        printk(KERN_ERR "[%s] smv %d is not in memdom %d\n", __func__, smv_id, memdom_id);
        return 0;
    }

    /* Get this smv's privileges */
    privs = memdom_priv_get(memdom_id, smv_id);

    /* Protection fault */
    if ( error_code & PF_PROT ) {        
    }

    /* Write fault */
    if ( error_code & PF_WRITE ) {
        if ( privs & MEMDOM_WRITE ) {
            rv = 1;
        } else{
            printk(KERN_ERR "[%s] smv %d cannot write memdom %d\n", __func__, smv_id, memdom_id);
            rv = 0; // Try to write a unwrittable address
        }
        
    }
    /* Read fault */ 
    else{
        if ( privs & MEMDOM_READ ) {
            rv = 1;
        } else{
            printk(KERN_ERR "[%s] smv %d cannot read memdom %d\n", __func__, smv_id, memdom_id);
            rv = 0; // Try to read a unreadable address
        }
    }

    /* kernel-/user-mode fault */
    if ( error_code & PF_USER ) {

    }

    /* Use of reserved bit detected */
    if ( error_code & PF_RSVD ) {

    }

    /* Fault was instruction fetch */
    if ( error_code & PF_INSTR ) {

    }

    return rv;    
}

/* Counter statistics helper functions */
static inline void init_rss_vec(int *rss) {
    memset(rss, 0, sizeof(int) * NR_MM_COUNTERS);
}
static inline void add_mm_rss_vec(struct mm_struct *mm, int *rss) {
	int i;

	if (current->mm == mm)
		sync_mm_rss(mm);
	for (i = 0; i < NR_MM_COUNTERS; i++)
		if (rss[i])
			add_mm_counter(mm, i, rss[i]);
}

/* Copy pte of a fault address from src_smv to dst_smv 
 * Return 0 on success, -1 otherwise.
 */
int copy_pgtable_smv(int dst_smv, int src_smv, 
                     unsigned long address, unsigned int flags,
                     struct vm_area_struct *vma){
    
    struct mm_struct *mm = current->mm;
    pgd_t *src_pgd, *dst_pgd;
    pud_t *src_pud, *dst_pud;
    pmd_t *src_pmd, *dst_pmd;
    pte_t *src_pte, *dst_pte;   
    spinlock_t *src_ptl, *dst_ptl;
    struct page *page;
    int rv;
    int rss[NR_MM_COUNTERS];

    /* Don't copy page table to the main thread */
    if ( dst_smv == MAIN_THREAD ) {
        slog(KERN_INFO "[%s] smv %d attempts to overwrite main thread's page table. Skip\n", __func__, src_smv);
        return 0;
    }
    /* Source and destination smvs cannot be the same */
    if ( dst_smv == src_smv ) {
        slog(KERN_INFO "[%s] smv %d attempts to copy its own page table. Skip.\n", __func__, src_smv);
        return 0;
    }
    /* Main thread should not call this function */
    if ( current->smv_id == MAIN_THREAD ) {
        slog(KERN_INFO "[%s] main thread smv %d, skip\n", __func__, current->smv_id);
        return 0;
    }


    /* SMP protection */
    mutex_lock(&mm->smv_metadataMutex);

    /* Source smv:
     * Page walk to obtain the source pte 
     * We should hit each level as __handle_mm_fault has already handled the fault
     */
    src_pgd = pgd_offset_smv(mm, address, src_smv);
    src_pud = pud_offset(src_pgd, address);
    src_pmd = pmd_offset(src_pud, address);
    src_pte = pte_offset_map(src_pmd, address);
    src_ptl = pte_lockptr(mm, src_pmd);
    spin_lock(src_ptl);

    /* Destination smv: 
     * Page walk to obtain the destination pte. 
     * Allocate new entry as needed */
    dst_pgd = pgd_offset_smv(mm, address, dst_smv);
    dst_pud = pud_alloc(mm, dst_pgd, address);
    if ( !dst_pud ) {
        rv = VM_FAULT_OOM;
        printk(KERN_ERR "[%s] Error: !dst_pud, address 0x%16lx\n", __func__, address);
        goto unlock_src;
    }
    dst_pmd = pmd_alloc(mm, dst_pud, address);
    if ( !dst_pmd ) {
        rv = VM_FAULT_OOM;
        printk(KERN_ERR "[%s] Error: !dst_pmd, address 0x%16lx\n", __func__, address);
        goto unlock_src;
    }
    if ( unlikely(pmd_none(*dst_pmd)) &&
         unlikely(__pte_alloc(mm, vma, dst_pmd, address))) {    
         rv = VM_FAULT_OOM;
         printk(KERN_ERR "[%s] Error: pmd_none(*dst_pud) && __pte_alloc() failed, address 0x%16lx\n", __func__, address);
         goto unlock_src;
    }
    dst_pte = pte_offset_map(dst_pmd, address);
    dst_ptl = pte_lockptr(mm, dst_pmd);
    spin_lock_nested(dst_ptl, SINGLE_DEPTH_NESTING);

    /* Skip copying pte if two ptes refer to the same page and specify the same access privileges */
    if ( !pte_same(*src_pte, *dst_pte) ) {
        page = vm_normal_page(vma, address, *src_pte);       
        /* Update data page statistics */
        if ( page ) {
            init_rss_vec(rss);
            get_page(page);
            page_dup_rmap(page);
            if ( PageAnon(page) ) {
                rss[MM_ANONPAGES]++;
            }
            else{
                rss[MM_FILEPAGES]++;
            }
    	    add_mm_rss_vec(mm, rss);
        }
        slog(KERN_INFO "[%s] src_pte 0x%16lx(smv %d) != dst_pte 0x%16lx (smv %d) for addr 0x%16lx\n", __func__, pte_val(*src_pte), src_smv, pte_val(*dst_pte), dst_smv, address);
    } else{
        slog(KERN_INFO "[%s] src_pte (smv %d) == dst_pte (smv %d) for addr 0x%16lx\n", __func__, src_smv, dst_smv, address);
    }

    /* Set the actual value to be the same as the source pgtables for destination  */   
    set_pte_at(mm, address, dst_pte, *src_pte);

    slog(KERN_INFO "[%s] src smv %d: pgd_val:0x%16lx, pud_val:0x%16lx, pmd_val:0x%16lx, pte_val:0x%16lx\n", 
                __func__, src_smv, pgd_val(*src_pgd), pud_val(*src_pud), pmd_val(*src_pmd), pte_val(*src_pte));
    slog(KERN_INFO "[%s] dst smv %d: pgd_val:0x%16lx, pud_val:0x%16lx, pmd_val:0x%16lx, pte_val:0x%16lx\n", 
                __func__, dst_smv, pgd_val(*dst_pgd), pud_val(*dst_pud), pmd_val(*dst_pmd), pte_val(*dst_pte));
  
    spin_unlock(dst_ptl);
    pte_unmap(dst_pte);    

    /* By the time we get here, the page tables are set up correctly */
    rv = 0;

unlock_src:
    spin_unlock(src_ptl);
    pte_unmap(src_pte);

    if ( rv != 0 ) {
        slog(KERN_ERR "[%s] Error: !dst_pud, address 0x%16lx\n", __func__, address);
    } else{
        slog(KERN_INFO "[%s] smv %d copied pte from MAIN_THREAD. addr 0x%16lx, *src_pte 0x%16lx, *dst_pte 0x%16lx\n", 
               __func__, dst_smv, address, pte_val(*src_pte), pte_val(*dst_pte));
    }
	mutex_unlock(&mm->smv_metadataMutex);
    return rv;
}
