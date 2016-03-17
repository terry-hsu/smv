#include <linux/ribbon.h>
#include <linux/memdom.h>
#include <linux/module.h>

int ribbon_create(void){
    int ribbon_id = -1;

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
