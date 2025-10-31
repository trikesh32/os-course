#pragma once

struct list2 {
    struct list2 *prev;
    struct list2 *next;
    int rcount;
};

void list2_init(struct list2 *l);

int list2_empty(struct list2 *l);
void list2_acquire(struct list2 *l);

void list2_release(struct list2 *l1, struct list2 *l2);

void list2_add(struct list2 *l1, struct list2 *l2);

void list2_delete(struct list2 *l1, struct list2 *l2);

struct list2* list2_next(struct list2 *l1, struct list2 *l2);

struct list2* list2_begin(struct list2 *l);

void list2_iterate(struct list2 *l1, struct list2 **l2);