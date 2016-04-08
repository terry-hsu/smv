#ifndef _LINUX_MEMDOM_H
#define _LINUX_MEMDOM_H

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
 *  @file       memdom.h
 *  @brief      Management of memory domain metadata.
 *  @author     Terry Ching-Hsiang Hsu  <terryhsu@purdue.edu> 
*/

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/smv_mm.h>

/* Permission */
#define MEMDOM_READ             0x00000001
#define MEMDOM_WRITE            0x00000002
#define MEMDOM_EXECUTE          0x00000004
#define MEMDOM_ALLOCATE         0x00000008

/// Memdom struct metadata ///
struct memdom_struct {
    int memdom_id;    
    struct mutex memdom_mutex;
    DECLARE_BITMAP(ribbon_bitmapRead, SMV_ARRAY_SIZE); // Bitmap of ribbon.  Set to 1 if ribbon[i] can read this memdom, 0 otherwise.
    DECLARE_BITMAP(ribbon_bitmapWrite, SMV_ARRAY_SIZE); // Bitmap of ribbon.  Set to 1 if ribbon[i] can write this memdom, 0 otherwise.
    DECLARE_BITMAP(ribbon_bitmapExecute, SMV_ARRAY_SIZE); // Bitmap of ribbon.  Set to 1 if ribbon[i] can execute data in this memdom, 0 otherwise.
    DECLARE_BITMAP(ribbon_bitmapAllocate, SMV_ARRAY_SIZE); // Bitmap of ribbon.  Set to 1 if ribbon[i] can allocate data in this memdom, 0 otherwise.
};

/// --- Functions called by the kernel internally to manage memory space --- ///
#define allocate_memdom()   (kmem_cache_alloc(memdom_cachep, GFP_KERNEL))
#define free_memdom(memdom) (kmem_cache_free(memdom_cachep, memdom))
extern void memdom_init(void);
int memdom_claim_all_vmas(int memdom_id);

/// --- Functions exported to user space to manage metadata --- ///
int memdom_create(void);
void free_all_memdoms(struct mm_struct *mm);
int memdom_kill(int memdom_id, struct mm_struct *mm);
int memdom_priv_add(int memdom_id, int ribbon_id, int privs);
int memdom_priv_del(int memdom_id, int ribbon_id, int privs);
int memdom_priv_get(int memdom_id, int ribbon_id);
int memdom_mmap_register(int memdom_id);
unsigned long memdom_munmap(unsigned long addr);
int memdom_main_id(void);

#endif

