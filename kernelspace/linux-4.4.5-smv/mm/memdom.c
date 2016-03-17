#include <linux/ribbon.h>
#include <linux/memdom.h>
#include <linux/module.h>
#include <linux/slab.h>

/* SLAB cache for ribbon_struct structure  */
struct kmem_cache *memdom_cachep;

/** void memdom_init(void)
 *  Create slab cache for future memdom_struct allocation This
 *  is called by start_kernel in main.c 
 */
void memdom_init(void){
    memdom_cachep = kmem_cache_create("memdom_struct",
                                      sizeof(struct memdom_struct), 0,
                                      SLAB_HWCACHE_ALIGN | SLAB_PANIC | SLAB_NOTRACK, NULL);
    printk(KERN_INFO "[%s] memdom slabs initialized\n", __func__);   
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


