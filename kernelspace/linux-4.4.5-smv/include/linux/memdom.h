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

/* Permission */
#define MEMDOM_MEMBER           0x00000010
#define MEMDOM_INJECT           0x00000008
#define MEMDOM_READ             0x00000004
#define MEMDOM_WRITE            0x00000002
#define MEMDOM_EXECUTE          0x00000001

/* Memory domain struct metadata */
struct memdom_struct {
    int memdom_id;    
    struct mutex memdom_mutex;
    DECLARE_BITMAP(ribbon_bitmapRead, MAX_RIBBON); // Bitmap of ribbon.  Set to 1 if ribbon[i] can read this memdom, 0 otherwise.
    DECLARE_BITMAP(ribbon_bitmapWrite, MAX_RIBBON); // Bitmap of ribbon.  Set to 1 if ribbon[i] can write this memdom, 0 otherwise.
    DECLARE_BITMAP(ribbon_bitmapExecute, MAX_RIBBON); // Bitmap of ribbon.  Set to 1 if ribbon[i] can execute data in this memdom, 0 otherwise.
    DECLARE_BITMAP(ribbon_bitmapAllocate, MAX_RIBBON); // Bitmap of ribbon.  Set to 1 if ribbon[i] can allocate data in this memdom, 0 otherwise.
};

/* Called by init/main.c */
extern void memdom_init(void);

/* SLAB cache for memdom_struct structure */
#define allocate_memdom()   (kmem_cache_alloc(memdom_cachep, GFP_KERNEL))
#define free_memdom(memdom) (kmem_cache_free(memdom_cachep, memdom))

/* Memoey domain functions */
int memdom_create(void);
unsigned long memdom_free(unsigned long addr);
void free_all_memdoms(struct mm_struct *mm);
int memdom_kill(int memdom_id, struct mm_struct *mm);
unsigned long memdom_alloc(int memdom_id, unsigned long sz);
int memdom_priv_add(int memdom_id, int ribbon_id, unsigned long privs);
int memdom_priv_del(int memdom_id, int ribbon_id, unsigned long privs);
int memdom_priv_get(int memdom_id, int ribbon_id);

#endif

