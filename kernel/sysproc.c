#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
    int n;
    if (argint(0, &n) < 0)
        return -1;
    exit(n);
    return 0; // not reached
}

uint64
sys_getpid(void)
{
    return myproc()->pid;
}

uint64
sys_fork(void)
{
    return fork();
}

uint64
sys_wait(void)
{
    uint64 p;
    if (argaddr(0, &p) < 0)
        return -1;
    return wait(p);
}

uint64
sys_sbrk(void)
{
    int addr;
    int n;

    if (argint(0, &n) < 0)
        return -1;

    addr = myproc()->sz;
    if (growproc(n) < 0)
        return -1;
    return addr;
}

uint64
sys_sleep(void)
{
    int n;
    uint ticks0;

    if (argint(0, &n) < 0)
        return -1;
    acquire(&tickslock);
    ticks0 = ticks;
    while (ticks - ticks0 < n)
    {
        if (myproc()->killed)
        {
            release(&tickslock);
            return -1;
        }
        sleep(&ticks, &tickslock);
    }
    release(&tickslock);
    return 0;
}

#ifdef LAB_PGTBL
int sys_pgaccess(void)
{
    // lab pgtbl: your code here.

    // fetch parameters
    // three parameters: address of first page, number of pages to read, user address to write consult results
    uint64 va, ua;
    int npages;
    if (argaddr(0, &va) < 0)
        return -1;
    if (argint(1, &npages) < 0)
        return -1;
    if (argaddr(2, &ua) < 0)
        return -1;

    // printf("%p - %d - %p\n", va, npages, ua);

    // upper limit for npages is 512
    if (npages > 512)
    {
        panic("too many pages to check");
    }

    // read page table
    struct proc *p = myproc();
    pagetable_t pagetable;
    pte_t *pte;
    char resbuf[64] = {0};

    // check pagetable at first
    // vmprint(p->pagetable);

    int level, i;
    for (i = 0; i < npages; i++)
    {
        // set temporary variable before every inner circulation
        pagetable = p->pagetable;
        for (level = 2; level > 0; level--)
        {
            // get pte
            pte = &pagetable[PX(level, va)];
            if (*pte & PTE_V)
            {
                pagetable = (pagetable_t)PTE2PA(*pte);
            }
            else
            {
                // search failed, so break
                break;
            }
        }
        // if found corresponding PTE
        if (level == 0)
        {
            pte = &pagetable[PX(0, va)];
            // printf("found PTE: %p\n", *pte);
            // if not accessed
            if (*pte & PTE_A)
            {
                *pte &= (~PTE_A);
                resbuf[(i >> 3)] |= (1 << (i % 8));
            }
        }
        // renew va
        va += PGSIZE;
    }

    // check result buf in kernel
    // for (int j = 0; j < NBITS2BYTES(npages); j++)
    // {
    //     printf("%d: %d\n", j, resbuf[j]);
    // }

    // copy the result from kernel to user
    if (copyout(p->pagetable, ua, resbuf, NBITS2BYTES(npages)) < 0)
        return -1;

    return 0;
}
#endif

uint64
sys_kill(void)
{
    int pid;

    if (argint(0, &pid) < 0)
        return -1;
    return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
    uint xticks;

    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);
    return xticks;
}
