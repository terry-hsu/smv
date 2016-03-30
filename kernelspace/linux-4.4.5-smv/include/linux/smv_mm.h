#ifndef _LINUX_SMV_H
#define _LINUX_SMV_H

/*
    This file is part of Secure Memory View (SMV) kernel.

    SMV is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SMV is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SMV.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 *  @file       ribbon.h
 *  @brief      Management of secure memory view metadata.
 *  @author     Terry Ching-Hsiang Hsu  <terryhsu@purdue.edu> 
*/

#define SMV_ARRAY_SIZE 1025 /* Maximum number of ribbons and memdoms allowed in a process */
#define MAIN_THREAD 0 /* Main thread is always using the first index in the metadata array: 0 */
#define LAST_RIBBON_INDEX (SMV_ARRAY_SIZE - 1)
#define LAST_MEMDOM_INDEX (SMV_ARRAY_SIZE - 1)

#include <linux/kernel.h>
#include <linux/mm_types.h>

/*
 * Page fault error code bits:
 *
 *   bit 0 ==	 0: no page found	1: protection fault
 *   bit 1 ==	 0: read access		1: write access
 *   bit 2 ==	 0: kernel-mode access	1: user-mode access
 *   bit 3 ==				1: use of reserved bit detected
 *   bit 4 ==				1: fault was an instruction fetch
 */
enum x86_pf_error_code {

	PF_PROT		=		1 << 0,
	PF_WRITE	=		1 << 1,
	PF_USER		=		1 << 2,
	PF_RSVD		=		1 << 3,
	PF_INSTR	=		1 << 4,
};

/* Called by copy_pte_smv to locate the current pgd */
#define pgd_offset_ribbon(mm, address, ribbon_id) ((mm)->pgd_ribbon[ribbon_id]  + pgd_index((address)))

int smv_valid_fault(int ribbon_id, struct vm_area_struct *vma, unsigned long error_code);
int copy_pgtable_smv(int dst_ribbon, int src_ribbon, 
                     unsigned long addr, unsigned int flags,
                     struct vm_area_struct *vma);
#endif
