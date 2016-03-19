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

#define MAX_RIBBON 1024
#define MAX_MEMDOM 1024

/* Ribbons struct metadata */
struct ribbon_struct {
    int ribbon_id;
    atomic_t ntask;       // number of tasks running in this ribbon
    DECLARE_BITMAP(memdom_bitmapJoin, MAX_MEMDOM); // Bitmap of memdoms.  set to 1 if this ribbon is in memdom[i], 0 otherwise.
    struct mutex ribbon_mutex;  // lock ribbon struct to prevent race condition   
};

/* Called by init/main.c */
extern void ribbon_init(void);

/* SLAB cache for ribbon_struct structure */
#define allocate_ribbon()         (kmem_cache_alloc(ribbon_cachep, GFP_KERNEL))
#define free_ribbon(ribbon)       (kmem_cache_free(ribbon_cachep, ribbon))

int ribbon_create(void);
int ribbon_kill(int ribbon_id, struct mm_struct *mm);
void free_all_ribbons(struct mm_struct *mm);
int ribbon_join_memdom(int memdom_id, int ribbon_id);
int ribbon_leave_memdom(int memdom_id, int ribbon_id, struct mm_struct *mm);
int ribbon_is_in_memdom(int memdom_id, int ribbon_id);
int ribbon_get_ribbon_id(void);
#endif
