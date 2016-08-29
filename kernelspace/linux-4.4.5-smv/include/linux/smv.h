#ifndef _LINUX_RIBBON_H
#define _LINUX_RIBBON_H

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
 *  @file       smv.h
 *  @brief      Management of secure memory view metadata.
 *  @author     Terry Ching-Hsiang Hsu  <terryhsu@purdue.edu> 
*/

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/smv_mm.h>

//#define SMV_LOGGING
#ifdef SMV_LOGGING
#define slog(level, fmt, args...) printk(level fmt, ## args...)
#else
#define slog(level, fmt, args...) do{ }while(0)
#endif

/// Ribbons struct metadata ///
struct smv_struct {
    int smv_id;
    atomic_t ntask;       // number of tasks running in this smv
    DECLARE_BITMAP(memdom_bitmapJoin, SMV_ARRAY_SIZE); // Bitmap of memdoms.  set to 1 if this smv is in memdom[i], 0 otherwise.
    struct mutex smv_mutex;  // lock smv struct to prevent race condition   
};

/// --- Functions called by the kernel internally to manage memory space --- ///
struct mmu_gather;
#define allocate_smv()         (kmem_cache_alloc(smv_cachep, GFP_KERNEL)) /* SLAB cache for smv_struct structure */
#define free_smv(smv)       (kmem_cache_free(smv_cachep, smv))
extern void smv_init(void);      /* Called by init/main.c */
pgd_t *smv_alloc_pgd(struct mm_struct *mm, int smv_id);
void smv_free_pgd(struct mm_struct *mm, int smv_id);
void smv_free_pgtables(struct mmu_gather *tlb, struct vm_area_struct *vma,
                    		unsigned long floor, unsigned long ceiling);
void switch_smv(struct task_struct *prev_tsk, struct task_struct *next_tsk, struct mm_struct *prev_mm, struct mm_struct *next_mm);
void smv_free_mmap(struct mm_struct *mm, int smv_id);

/// --- Functions exported to user space to manage metadata --- ///
int smv_main_init(void);
int smv_create(void);
int smv_kill(int smv_id, struct mm_struct *mm);
void free_all_smvs(struct mm_struct *mm);
int smv_join_memdom(int memdom_id, int smv_id);
int smv_leave_memdom(int memdom_id, int smv_id, struct mm_struct *mm);
int smv_is_in_memdom(int memdom_id, int smv_id);
int smv_exists(int smv_id);
int smv_get_smv_id(void);
int register_smv_thread(int smv_id);
#endif
