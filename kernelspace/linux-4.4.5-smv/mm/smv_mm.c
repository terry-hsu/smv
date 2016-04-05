#include <linux/smv_mm.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/rmap.h>

/* Check whether current fault is a valid smv page fault.
 * Return 1 if it's a valid smv fault, 0 to block access 
 */
int smv_valid_fault(int ribbon_id, struct vm_area_struct *vma, unsigned long error_code){
    int memdom_id = vma->memdom_id;
    struct mm_struct *mm = current->mm;
    int privs = 0;
    int rv = 0;

    /* Skip checking for smv valid fault if 
     * 1. current task is not using smv 
     * 2. current task is using smv, but page fault triggered by Pthreads (ribbon_id == -1) 
     */
    if ( !mm->using_smv || (mm->using_smv && current->ribbon_id == -1) ) {
         return 1;
    }

    /* A fault is valid only if the ribbon has joined this vma's memdom */
    if ( !ribbon_is_in_memdom(memdom_id, ribbon_id) ) {
        printk(KERN_ERR "[%s] ribbon %d is not in memdom %d\n", __func__, ribbon_id, memdom_id);
        return 0;
    }

    /* Get this ribbon's privileges */
    privs = memdom_priv_get(memdom_id, ribbon_id);

    /* Protection fault */
    if ( error_code & PF_PROT ) {        
    }

    /* Write fault */
    if ( error_code & PF_WRITE ) {
        if ( privs & MEMDOM_WRITE ) {
            rv = 1;
        } else{
            printk(KERN_ERR "[%s] ribbon %d cannot write memdom %d\n", __func__, ribbon_id, memdom_id);
            rv = 0; // Try to write a unwrittable address
        }
        
    }
    /* Read fault */ 
    else{
        if ( privs & MEMDOM_READ ) {
            rv = 1;
        } else{
            printk(KERN_ERR "[%s] ribbon %d cannot read memdom %d\n", __func__, ribbon_id, memdom_id);
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

/* Copy pte of a fault address from src_ribbon to dst_ribbon 
 * Return 0 on success, -1 otherwise.
 */
int copy_pgtable_smv(int dst_ribbon, int src_ribbon, 
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
    if ( dst_ribbon == MAIN_THREAD ) {
        slog(KERN_INFO "[%s] ribbon %d attempts to overwrite main thread's page table. Skip\n", __func__, src_ribbon);
        return 0;
    }
    /* Source and destination ribbons cannot be the same */
    if ( dst_ribbon == src_ribbon ) {
        slog(KERN_INFO "[%s] ribbon %d attempts to copy its own page table. Skip.\n", __func__, src_ribbon);
        return 0;
    }
    /* Main thread should not call this function */
    if ( current->ribbon_id == MAIN_THREAD ) {
        slog(KERN_INFO "[%s] main thread ribbon %d, skip\n", __func__, current->ribbon_id);
        return 0;
    }


    /* SMP protection */
    mutex_lock(&mm->smv_metadataMutex);

    /* Source ribbon:
     * Page walk to obtain the source pte 
     * We should hit each level as __handle_mm_fault has already handled the fault
     */
    src_pgd = pgd_offset_ribbon(mm, address, src_ribbon);
    src_pud = pud_offset(src_pgd, address);
    src_pmd = pmd_offset(src_pud, address);
    src_pte = pte_offset_map(src_pmd, address);
    src_ptl = pte_lockptr(mm, src_pmd);
    spin_lock(src_ptl);

    /* Destination ribbon: 
     * Page walk to obtain the destination pte. 
     * Allocate new entry as needed */
    dst_pgd = pgd_offset_ribbon(mm, address, dst_ribbon);
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
        slog(KERN_INFO "[%s] src_pte 0x%16lx(ribbon %d) != dst_pte 0x%16lx (ribbon %d) for addr 0x%16lx\n", __func__, pte_val(*src_pte), src_ribbon, pte_val(*dst_pte), dst_ribbon, address);
    } else{
        slog(KERN_INFO "[%s] src_pte (ribbon %d) == dst_pte (ribbon %d) for addr 0x%16lx\n", __func__, src_ribbon, dst_ribbon, address);
    }

    /* Set the actual value to be the same as the source pgtables for destination  */   
    set_pte_at(mm, address, dst_pte, *src_pte);

    slog(KERN_INFO "[%s] src ribbon %d: pgd_val:0x%16lx, pud_val:0x%16lx, pmd_val:0x%16lx, pte_val:0x%16lx\n", 
                __func__, src_ribbon, pgd_val(*src_pgd), pud_val(*src_pud), pmd_val(*src_pmd), pte_val(*src_pte));
    slog(KERN_INFO "[%s] dst ribbon %d: pgd_val:0x%16lx, pud_val:0x%16lx, pmd_val:0x%16lx, pte_val:0x%16lx\n", 
                __func__, dst_ribbon, pgd_val(*dst_pgd), pud_val(*dst_pud), pmd_val(*dst_pmd), pte_val(*dst_pte));
  
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
        slog(KERN_INFO "[%s] ribbon %d copied pte from MAIN_THREAD. addr 0x%16lx, *src_pte 0x%16lx, *dst_pte 0x%16lx\n", 
               __func__, dst_ribbon, address, pte_val(*src_pte), pte_val(*dst_pte));
    }
	mutex_unlock(&mm->smv_metadataMutex);
    return rv;
}
