#include <linux/smv_mm.h>
#include <linux/sched.h>

/* Check whether current fault is a valid smv page fault.
 * Return 1 if it's a valid smv fault, 0 to block access 
 */
int smv_valid_fault(int ribbon_id, struct vm_area_struct *vma, unsigned long error_code){
    int memdom_id = vma->memdom_id;
    struct mm_struct *mm = current->mm;
    
    /* Skip checking for smv valid fault if current task is not using smv */
    if ( mm->using_smv ) {
         return 1;
    }

    /* A fault is valid only if the ribbon has joined this vma's memdom */
    if ( !ribbon_is_in_memdom(memdom_id, ribbon_id) ) {
        return 0;
    }

    /* Protection fault */
    if ( error_code & PF_PROT ) {
        
    }

    /* Read/Write fault */
    if ( error_code & PF_WRITE ) {

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

    return 1;    
}

int handle_smv_fault(int dst_ribbon, int src_ribbon, 
                     unsigned long addr, unsigned long error_code,
                     struct vm_area_struct *vma){
    
    

    return 0;
}
