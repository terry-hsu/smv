#include <linux/ribbon.h>
#include <linux/memdom.h>
#include <linux/module.h>

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


