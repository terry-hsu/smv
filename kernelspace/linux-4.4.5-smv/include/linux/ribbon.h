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
 *  @file       ribbon.h
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
struct ribbon_struct {
    int ribbon_id;
    atomic_t ntask;       // number of tasks running in this ribbon
    DECLARE_BITMAP(memdom_bitmapJoin, SMV_ARRAY_SIZE); // Bitmap of memdoms.  set to 1 if this ribbon is in memdom[i], 0 otherwise.
    struct mutex ribbon_mutex;  // lock ribbon struct to prevent race condition   
};

/// --- Functions called by the kernel internally to manage memory space --- ///
struct mmu_gather;
#define allocate_ribbon()         (kmem_cache_alloc(ribbon_cachep, GFP_KERNEL)) /* SLAB cache for ribbon_struct structure */
#define free_ribbon(ribbon)       (kmem_cache_free(ribbon_cachep, ribbon))
extern void ribbon_init(void);      /* Called by init/main.c */
pgd_t *ribbon_alloc_pgd(struct mm_struct *mm, int ribbon_id);
void ribbon_free_pgd(struct mm_struct *mm, int ribbon_id);
void ribbon_free_pgtables(struct mmu_gather *tlb, struct vm_area_struct *vma,
                    		unsigned long floor, unsigned long ceiling);
void switch_ribbon(struct task_struct *prev_tsk, struct task_struct *next_tsk, struct mm_struct *prev_mm, struct mm_struct *next_mm);
void ribbon_free_mmap(struct mm_struct *mm, int ribbon_id);

/// --- Functions exported to user space to manage metadata --- ///
int ribbon_main_init(void);
int ribbon_create(void);
int ribbon_kill(int ribbon_id, struct mm_struct *mm);
void free_all_ribbons(struct mm_struct *mm);
int ribbon_join_memdom(int memdom_id, int ribbon_id);
int ribbon_leave_memdom(int memdom_id, int ribbon_id, struct mm_struct *mm);
int ribbon_is_in_memdom(int memdom_id, int ribbon_id);
int ribbon_exists(int ribbon_id);
int ribbon_get_ribbon_id(void);
int register_ribbon_thread(int ribbon_id);
#endif
