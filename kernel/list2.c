#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"
#include "proc.h"
#include "list2.h"

void
list2_init(struct list2 *l)
{
    l->prev = l;
    l->next = l;
    l->rcount = 2;
}

int
list2_empty(struct list2 *l)
{
    return l->next == l && l->prev == l;
}

void 
list2_acquire(struct list2 *l)
{
    l->rcount++;
}

void 
list2_release(struct list2 *l1, struct list2 *l2)
{
    if (l1 == l2)
        return;

    l2->rcount--;

    if (l2->rcount == 0) {
        list2_release(l1, l2->next);
        list2_release(l1, l2->prev);
        bd_free(l2);
    }
}

void
list2_add(struct list2 *l1, struct list2 *l2)
{
    l2->rcount = 2;

    l2->next = l1->next;
    l2->prev = l1;
    l1->next->prev = l2;
    l1->next = l2;
}

void
list2_delete(struct list2 *l1, struct list2 *l2)
{
    l2->prev->next = l2->next;
    l2->next->prev = l2->prev;

    if (l2->next != l1)
        list2_acquire(l2->next);
    if (l2->prev != l1)
        list2_acquire(l2->prev);

    l2->rcount -= 2;

    if (l2->rcount == 0)
        bd_free(l2);
} 

struct list2*
list2_next(struct list2 *l1, struct list2 *l2)
{
    if (l2->next != l1)
        list2_acquire(l2->next);

    return l2->next;
}

struct list2*
list2_begin(struct list2 *l)
{
    return list2_next(l, l);
}

void
list2_iterate(struct list2 *l1, struct list2 **l2)
{
    struct list2 *next = list2_next(l1, *l2);

    list2_release(l1, *l2);

    *l2 = next;
}
