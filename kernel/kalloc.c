// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint8 *refs;
  void *pa_start;
  void *pa_end;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  kmem.refs = (uint8*)end;
  uint64 total_pages = (PHYSTOP - KERNBASE) / PGSIZE;
  kmem.pa_start = (void*)PGROUNDUP((uint64)end + total_pages);
  kmem.pa_end = (void*)PHYSTOP;
  memset(kmem.refs, 0, total_pages);
  
  freerange(kmem.pa_start, kmem.pa_end);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

uint8 ref_inc(void* pa){
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < (char*)kmem.pa_start || (uint64)pa >= PHYSTOP)
    panic("ref_inc: bad pa");
  
  acquire(&kmem.lock);
  uint64 index = ((uint64)pa - (uint64)kmem.pa_start) / PGSIZE;
  uint8 new_ref = ++kmem.refs[index];
  release(&kmem.lock);
  return new_ref;
}

uint8 ref_dec(void* pa){
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < (char*)kmem.pa_start || (uint64)pa >= PHYSTOP)
    panic("ref_dec: bad pa");
  
  acquire(&kmem.lock);
  uint64 index = ((uint64)pa - (uint64)kmem.pa_start) / PGSIZE;
  uint8 new_ref = --kmem.refs[index];
  release(&kmem.lock);
  return new_ref;
}

uint8 ref_get(void* pa){
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < (char*)kmem.pa_start || (uint64)pa >= PHYSTOP)
    panic("ref_get: bad pa");
  uint64 index = ((uint64)pa - (uint64)kmem.pa_start) / PGSIZE;
  uint8 new_ref = kmem.refs[index];
  return new_ref;
}



// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < (char*)kmem.pa_start || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  
  uint64 index = ((uint64)pa - (uint64)kmem.pa_start) / PGSIZE;
  if(kmem.refs[index] != 0) {
    release(&kmem.lock);
    return;
  }
  memset(pa, 1, PGSIZE);
  r = (struct run*)pa;
  r->next = kmem.freelist;
  kmem.freelist = r;
  
  release(&kmem.lock);
}

void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    uint64 index = ((uint64)r - (uint64)kmem.pa_start) / PGSIZE;
    kmem.refs[index] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
    
  return (void*)r;
}
