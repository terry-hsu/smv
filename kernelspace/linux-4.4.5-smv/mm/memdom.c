#include <linux/ribbon.h>
#include <linux/memdom.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>

/* SLAB cache for ribbon_struct structure  */
static struct kmem_cache *memdom_cachep;

/** void memdom_init(void)
 *  Create slab cache for future memdom_struct allocation This
 *  is called by start_kernel in main.c 
 */
void memdom_init(void){
    memdom_cachep = kmem_cache_create("memdom_struct",
                                      sizeof(struct memdom_struct), 0,
                                      SLAB_HWCACHE_ALIGN | SLAB_NOTRACK, NULL);
    if( !memdom_cachep ) {
        printk(KERN_INFO "[%s] memdom slabs initialization failed...\n", __func__);
    } else{
        printk(KERN_INFO "[%s] memdom slabs initialized\n", __func__);
    }
}

struct memdom_struct *create_memdom_metadata(void){
    struct memdom_struct *memdom = NULL;
    struct mm_struct *mm = current->mm;

    if( !mm ) {
        return NULL;
    }

    /* Create memdom struct */
    memdom = allocate_memdom();
    if( !memdom ) {
        printk(KERN_ERR "[%s] cannot allocate memdom metadata\n", __func__);
        return NULL;
    }

    /* Assign memdom ID */
    memdom->memdom_id = atomic_read(&mm->num_memdoms);

    /* Add 1 to existing memdom count in process's mm */
    atomic_inc(&mm->num_memdoms);

    /* This memdom does not have any ribbons for Read/Write/Execute/Allocate yet */
    bitmap_zero(memdom->ribbon_bitmapRead, MAX_RIBBON);
    bitmap_zero(memdom->ribbon_bitmapWrite, MAX_RIBBON);
    bitmap_zero(memdom->ribbon_bitmapExecute, MAX_RIBBON);
    bitmap_zero(memdom->ribbon_bitmapAllocate, MAX_RIBBON);

    /* No memdom is allocated yet */
    bitmap_zero(memdom->memdom_bitmapInUse, MAX_MEMDOM);

    /* Initialize mutex that protects this ribbon */
    mutex_init(&memdom->memdom_mutex);   

    return NULL;
}

void free_memdom_metadata(struct memdom_struct *memdom){
    if( !memdom ) {        
        return;
    }
    kmem_cache_free(memdom_cachep, (memdom));
}

int memdom_create(void){

    return 0;
}
EXPORT_SYMBOL(memdom_create);

unsigned long memdom_alloc(int memdom_id, unsigned long sz){

    return 0;
}
EXPORT_SYMBOL(memdom_alloc);

unsigned long memdom_free(unsigned long addr){

    return 0;
}
EXPORT_SYMBOL(memdom_free);

int memdom_kill(int memdom_id){

    return 0;
}
EXPORT_SYMBOL(memdom_kill);

int memdom_priv_add(int memdom_id, int ribbon_id, unsigned long privs){

    return 0;
}
EXPORT_SYMBOL(memdom_priv_add);

int memdom_priv_del(int memdom_id, int ribbon_id, unsigned long privs){

    return 0;
}
EXPORT_SYMBOL(memdom_priv_del);

int memdom_priv_get(int memdom_id, int ribbon_id){

    return 0;
}
EXPORT_SYMBOL(memdom_priv_get);


