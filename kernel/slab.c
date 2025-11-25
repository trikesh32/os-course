#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"
#include "fs.h"
#include "file.h"


#define SLAB_SIZE 4096
#define MAX_OBJECTS_COUNT_PER_SLAB 256
#define META_DATA_SIZE 48

struct slab {
    struct slab* next_slab;
    short object_size;
    short count;
    void* free_list;
};
void* get_address(struct slab* base, int idx);

struct slab* proc_cache;
struct slab* file_cache;

struct slab* create_new_slab(unsigned long size){
    struct slab* res = kalloc();
    res->next_slab = 0;
    res->object_size = size;
    res->count = 0;
    short max_object_count = (SLAB_SIZE - META_DATA_SIZE) / res->object_size;
    void* ptr = &(res->free_list);
    for (short i=0; i<max_object_count; ++i){
        *(void**)ptr = get_address(res, i);
        ptr = *(void**)ptr;
    }
    *(void**)ptr = 0;
    return res;
}

void slab_init(){
    proc_cache = create_new_slab(sizeof(struct proc));
    file_cache = create_new_slab(sizeof(struct file));
    if (!proc_cache || !file_cache) {
        panic("slab: initialization failed");
    }
}

void* get_address(struct slab* base, int idx){
    return (void*) base + META_DATA_SIZE + base->object_size * idx;
}
int get_idx(struct slab* base, void* ptr){
    return (int)((unsigned long)ptr - (unsigned long)base - META_DATA_SIZE) / base->object_size;
}

void* slab_malloc(char string){
    struct slab* p;
    if (string == 'p'){
        p = proc_cache;
    } 
    else if (string == 'f'){
        p = file_cache;
    }
    else
        return (void*)0;
    while (p->next_slab != (void*)0 && p->free_list == (void*) 0)
        p = p->next_slab;
    if (p->next_slab == (void*)0 && p->free_list == (void*) 0){
        struct slab* new_slab = create_new_slab(string == 'p' ? sizeof(struct proc) : sizeof(struct file));
        p->next_slab = new_slab;
        new_slab->count+=1;
        void* temp = new_slab->free_list;
        new_slab->free_list = *(void**)(new_slab->free_list);
        return temp;
    }
    if (p->free_list != (void*)0){
        void* temp = p->free_list;
        p->free_list = *(void**)(p->free_list);
        p->count += 1;
        return temp;
    }
    return (void*)0;
}

void slab_free(void* ptr, char string){
    struct slab* p;
    if (string == 'p'){
        p = proc_cache;
    } 
    else if (string == 'f'){
        p = file_cache;
    }
    else
        return;
    while (p != (void*)0 && (ptr < (void*)p + META_DATA_SIZE || ptr >= (void*)p + SLAB_SIZE))
        p = p->next_slab;
    if (p == (void*) 0)
        return;
    p->count -= 1;
    void* temp = p->free_list;
    p->free_list = ptr;
    *(void**)(p->free_list) = temp;
    if (p->count == 0 && p != proc_cache && p != file_cache){
        struct slab* s = string == 'p' ? proc_cache : file_cache;
        while (s->next_slab != p)
            s = s->next_slab;
        s->next_slab = p->next_slab;
        kfree(p);
    }

}