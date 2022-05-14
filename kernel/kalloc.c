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

// 将引用计数数组加到kmem结构体中，以利用其lock
struct
{
    struct spinlock lock;
    struct run *freelist;
    // 记录每个内存页的引用次数
    uint8 pgrefcnts[(PHYSTOP - KERNBASE) / PGSIZE]; // 128*1024*1024 / 4096 = 32768
} kmem;

// 获取pa对应的物理内存页引用次数
// pa需要4096字节对齐
int getrefcnts(void *pa)
{
    if (((uint64)pa % PGSIZE) != 0 || (uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
        panic("getrefcnts");
    acquire(&kmem.lock);
    int num = kmem.pgrefcnts[(int)((uint64)pa - KERNBASE) / PGSIZE];
    release(&kmem.lock);
    return num;
}

// 将pa对应的物理内存页引用次数加1
// pa需要4096字节对齐
void addrefcnts(void *pa)
{
    if (((uint64)pa % PGSIZE) != 0 || (uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
        panic("addrefcnts");
    acquire(&kmem.lock);
    kmem.pgrefcnts[(int)((uint64)pa - KERNBASE) / PGSIZE]++;
    release(&kmem.lock);
}

void kinit()
{
    // 初始化内存管理器的锁
    initlock(&kmem.lock, "kmem");

    // 首先，将所有的物理内存页的引用次数都设为1
    memset(kmem.pgrefcnts, 1, (PHYSTOP - KERNBASE) / PGSIZE);

    // 然后，对这些内存页调用kfree()，将其加入freelist链表
    freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
    char *p;
    p = (char *)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
        kfree(p);
}

// 将减少引用次数的操作集成在kfree()中
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
    if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    // 获取锁资源
    acquire(&kmem.lock);

    // 将内存页的引用计数减1
    // 如果引用计数减到了0，那么就将内存页视作空闲的内存页，执行free操作，将其插入空闲页链表
    if (--kmem.pgrefcnts[((uint64)pa - KERNBASE) / PGSIZE] == 0)
    {
        // release(&kmem.lock);
        struct run *r;

        // Fill with junk to catch dangling refs.
        memset(pa, 1, PGSIZE);

        r = (struct run *)pa;

        // 送入freelist链表
        r->next = kmem.freelist;
        kmem.freelist = r;
    }

    // 释放锁资源
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
    struct run *r;

    acquire(&kmem.lock);
    r = kmem.freelist;
    if (r)
    {
        // 一块内存页被分配的时候，将其引用计数设为1
        kmem.pgrefcnts[((uint64)r - KERNBASE) / PGSIZE] = 1;
        kmem.freelist = r->next;
    }
    release(&kmem.lock);

    // 如果成功分配了一块物理内存页面
    if (r)
        memset((char *)r, 5, PGSIZE); // fill with junk

    return (void *)r;
}
