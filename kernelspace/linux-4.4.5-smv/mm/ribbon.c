#include <linux/ribbon.h>
#include <linux/memdom.h>
#include <linux/module.h>
#include <linux/slab.h>

/* SLAB cache for ribbon_struct structure  */
struct kmem_cache *ribbon_cachep;

/* Global ribbon count */ 
atomic_t ribbon_count; // TODO: move it to mm.h

void ribbon_init(void){
    ribbon_cachep = kmem_cache_create("ribbon_struct",
                                      sizeof(struct ribbon_struct), 0,
                                      SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_NOTRACK, NULL);
    atomic_set(&ribbon_count, 0);
    printk(KERN_INFO "[%s] ribbon slab initialized\n", __func__);
}

int ribbon_create(void){
    int ribbon_id = -1;
    struct ribbon_struct *ribbon;

    if( atomic_read(&ribbon_count) == INT_MAX -1 ) {
        printk(KERN_ERR "Too many ribbons, cannot create more.\n");
        return -1;
    }
    ribbon = allocate_ribbon();
    atomic_inc(&ribbon_count);
    return ribbon_id;
}
EXPORT_SYMBOL(ribbon_create);

int ribbon_kill(int ribbon_id){

    return 0;
}
EXPORT_SYMBOL(ribbon_kill);

int ribbon_join_memdom(int memdom_id, int ribbon_id){

    return 0;
}
EXPORT_SYMBOL(ribbon_join_memdom);

int ribbon_leave_memdom(int memdom_id, int ribbon_id){

    return 0;
}
EXPORT_SYMBOL(ribbon_leave_memdom);

int ribbon_is_in_memdom(int memdom_id, int ribbon_id){

    return 0;
}
EXPORT_SYMBOL(ribbon_is_in_memdom);

int ribbon_get_ribbon_id(void){

    return 0;
}
EXPORT_SYMBOL(ribbon_get_ribbon_id);
