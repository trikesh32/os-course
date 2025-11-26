#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

static int
update_ptes_in_pagetable(pagetable_t pagetable, uint64 old_pa, uint64 new_pa, int level)
{
  int count = 0;
  
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    
    if(pte & PTE_LAZY){
      continue;
    }
    
    if(pte & PTE_V){
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        uint64 child = PTE2PA(pte);
        count += update_ptes_in_pagetable((pagetable_t)child, old_pa, new_pa, level + 1);
      } else {
        uint64 pa = PTE2PA(pte);
        if(pa == old_pa){
          pagetable[i] = PA2PTE(new_pa) | PTE_FLAGS(pte);
          count++;
        }
      }
    }
  }
  
  return count;
}

static int
is_safe_to_move(uint64 pa)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state != UNUSED){
      uint64 kstack_start = p->kstack;
      uint64 kstack_end = p->kstack + PGSIZE;
      if(pa >= kstack_start && pa < kstack_end){
        return 0;
      }
    }
  }
  
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state != UNUSED && p->trapframe != 0){
      uint64 trapframe_pa = (uint64)p->trapframe;
      if(pa == trapframe_pa){
        return 0;
      }
    }
  }
  
  return 1;
}

int
compact_move_page(uint64 old_pa, uint64 new_pa)
{
  struct proc *p;
  int total_updates = 0;
  
  if((old_pa % PGSIZE) != 0 || (new_pa % PGSIZE) != 0)
    return -1;
  
  if(old_pa < KERNBASE || old_pa >= PHYSTOP)
    return -1;
  
  if(new_pa < KERNBASE || new_pa >= PHYSTOP)
    return -1;
  
  if(ref_get((void*)new_pa) != 0)
    return -1;
  
  if(!is_safe_to_move(old_pa))
    return -1;
  
  memmove((void*)new_pa, (void*)old_pa, PGSIZE);
  
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    
    if(p->state != UNUSED && p->pagetable != 0){
      int updates = update_ptes_in_pagetable(p->pagetable, old_pa, new_pa, 0);
      total_updates += updates;
      
      if(updates > 0){
        if(p == myproc()){
          sfence_vma();
        }
      }
    }
    
    release(&p->lock);
  }
  
  total_updates += update_ptes_in_pagetable(kernel_pagetable, old_pa, new_pa, 0);
  
  uint8 old_refs = ref_get((void*)old_pa);
  if(old_refs > 0){
    for(int i = 0; i < old_refs; i++){
      ref_inc((void*)new_pa);
    }
    
    for(int i = 0; i < old_refs; i++){
      ref_dec((void*)old_pa);
    }
    kfree((void*)old_pa);
  }
  
  sfence_vma();
  
  return 0;
}

int
compact_memory(void)
{
  extern char end[];
  uint64 total_pages = (PHYSTOP - KERNBASE) / PGSIZE;
  void *pa_start = (void*)PGROUNDUP((uint64)end + total_pages);
  int pages_moved = 0;
  int max_moves = 10;
  
  for(uint64 free_pa = (uint64)pa_start; free_pa < PHYSTOP && pages_moved < max_moves; free_pa += PGSIZE){
    if(ref_get((void*)free_pa) != 0)
      continue;
    
    for(uint64 used_pa = free_pa + PGSIZE; used_pa < PHYSTOP; used_pa += PGSIZE){
      if(ref_get((void*)used_pa) > 0){
        if(compact_move_page(used_pa, free_pa) == 0){
          pages_moved++;
          break;
        }
      }
    }
  }
  
  return pages_moved;
}

int
compact_get_fragmentation(void)
{
  extern char end[];
  uint64 total_pages = (PHYSTOP - KERNBASE) / PGSIZE;
  void *pa_start = (void*)PGROUNDUP((uint64)end + total_pages);
  int holes = 0;
  int in_hole = 0;
  
  for(uint64 pa = (uint64)pa_start; pa < PHYSTOP; pa += PGSIZE){
    int is_free = (ref_get((void*)pa) == 0);
    
    if(is_free && !in_hole){
      in_hole = 1;
    } else if(!is_free && in_hole){
      holes++;
      in_hole = 0;
    }
  }
  
  return holes;
}