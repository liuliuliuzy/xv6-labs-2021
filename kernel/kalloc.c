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

struct run
{
    struct run *next;
};

struct kmem_percpu
{
    struct spinlock lock;
    struct run *freelist;
};

// every CPU holds a freelist
struct kmem_percpu allkmems[NCPU];

void kinit()
{
    char buf[9];
    for (int i = 0; i < NCPU; i++)
    {
        snprintf(buf, 8, "kmem-%d", i);
        initlock(&allkmems[i].lock, buf);
    }
    freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
    char *p;

    p = (char *)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
        kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
    struct run *r;
    int cid = cpuid();

    if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run *)pa;

    acquire(&allkmems[cid].lock);
    r->next = allkmems[cid].freelist;
    allkmems[cid].freelist = r;
    release(&allkmems[cid].lock);
}

void *kalloc_in_other(int cid)
{
    struct run *r; // 定义变量但是不赋值，那么会被初始化为0

    // 查询该CPU的freelist链表
    acquire(&allkmems[cid].lock);
    r = allkmems[cid].freelist;
    if (r)
        allkmems[cid].freelist = r->next;
    release(&allkmems[cid].lock);

    if (r)
        memset((char *)r, 5, PGSIZE); // fill with junk
    return (void *)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
    struct run *r; // 定义变量但是不赋值，那么会被初始化为0
    int cid = cpuid();

    acquire(&allkmems[cid].lock);
    r = allkmems[cid].freelist;
    if (r)
        allkmems[cid].freelist = r->next;
    release(&allkmems[cid].lock);

    if (r)
    {
        memset((char *)r, 5, PGSIZE); // fill with junk
    }
    // 如果当前CPU的freelist为空，则尝试去搜索其它CPU的freelist
    else
    {
        for (int i = 0; i < NCPU; i++)
        {
            if (i != cid)
            {
                r = (struct run *)kalloc_in_other(i);
                if (r)
                    break;
            }
        }
    }
    return (void *)r;
}
