//
// Support functions for system calls that involve file descriptors.
//

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
#include "fcntl.h"

struct devsw devsw[NDEV];
struct
{
    struct spinlock lock;
    struct file file[NFILE];
} ftable;

void fileinit(void)
{
    initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.
struct file *
filealloc(void)
{
    struct file *f;

    acquire(&ftable.lock);
    for (f = ftable.file; f < ftable.file + NFILE; f++)
    {
        if (f->ref == 0)
        {
            f->ref = 1;
            release(&ftable.lock);
            return f;
        }
    }
    release(&ftable.lock);
    return 0;
}

// Increment ref count for file f.
struct file *
filedup(struct file *f)
{
    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("filedup");
    f->ref++;
    release(&ftable.lock);
    return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void fileclose(struct file *f)
{
    struct file ff;

    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("fileclose");
    if (--f->ref > 0)
    {
        release(&ftable.lock);
        return;
    }
    ff = *f;
    f->ref = 0;
    f->type = FD_NONE;
    release(&ftable.lock);

    if (ff.type == FD_PIPE)
    {
        pipeclose(ff.pipe, ff.writable);
    }
    else if (ff.type == FD_INODE || ff.type == FD_DEVICE)
    {
        begin_op();
        iput(ff.ip);
        end_op();
    }
}

// Get metadata about file f.
// addr is a user virtual address, pointing to a struct stat.
int filestat(struct file *f, uint64 addr)
{
    struct proc *p = myproc();
    struct stat st;

    if (f->type == FD_INODE || f->type == FD_DEVICE)
    {
        ilock(f->ip);
        stati(f->ip, &st);
        iunlock(f->ip);
        if (copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
            return -1;
        return 0;
    }
    return -1;
}

// Read from file f.
// addr is a user virtual address.
int fileread(struct file *f, uint64 addr, int n)
{
    int r = 0;

    if (f->readable == 0)
        return -1;

    if (f->type == FD_PIPE)
    {
        r = piperead(f->pipe, addr, n);
    }
    else if (f->type == FD_DEVICE)
    {
        if (f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
            return -1;
        r = devsw[f->major].read(1, addr, n);
    }
    else if (f->type == FD_INODE)
    {
        ilock(f->ip);
        if ((r = readi(f->ip, 1, addr, f->off, n)) > 0)
            f->off += r;
        iunlock(f->ip);
    }
    else
    {
        panic("fileread");
    }

    return r;
}

// Write to file f.
// addr is a user virtual address.
int filewrite(struct file *f, uint64 addr, int n)
{
    int r, ret = 0;

    if (f->writable == 0)
        return -1;

    if (f->type == FD_PIPE)
    {
        ret = pipewrite(f->pipe, addr, n);
    }
    else if (f->type == FD_DEVICE)
    {
        if (f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
            return -1;
        ret = devsw[f->major].write(1, addr, n);
    }
    else if (f->type == FD_INODE)
    {
        // write a few blocks at a time to avoid exceeding
        // the maximum log transaction size, including
        // i-node, indirect block, allocation blocks,
        // and 2 blocks of slop for non-aligned writes.
        // this really belongs lower down, since writei()
        // might be writing a device like the console.
        int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * BSIZE;
        int i = 0;
        while (i < n)
        {
            int n1 = n - i;
            if (n1 > max)
                n1 = max;

            begin_op();
            ilock(f->ip);
            if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
                f->off += r;
            iunlock(f->ip);
            end_op();

            if (r != n1)
            {
                // error from writei
                break;
            }
            i += r;
        }
        ret = (i == n ? n : -1);
    }
    else
    {
        panic("filewrite");
    }

    return ret;
}

// 处理mmap引发的缺页中断
int handle_mmap(uint64 va, int scause)
{
    struct proc *p = myproc();
    struct vma *v = p->vma;
    // 查询进程的vma链表
    while (v != 0)
    {
        if (va >= v->start && va < v->end)
            break;
        v = v->next;
    }

    // 没找到
    if (v == 0)
        return -1;

    // 读操作引发的缺页中断，需要vma有读权限才能处理，以下同理
    if (scause == 13 && !(v->permissions & PROT_READ))
        return -2; // unreadable vma
    if (scause == 15 && !(v->permissions & PROT_WRITE))
        return -3; // unwritable vma

    // 将文件内容读取到虚拟内存
    va = PGROUNDDOWN(va);

    // 申请一块物理内存页
    char *mem = kalloc();
    if (mem == 0)
        return -4; // kalloc failed

    memset(mem, 0, PGSIZE);

    // 将物理内存页与va的映射关系写入页表中
    // PROT_NONE       0x0   PTE_V (1L << 0)
    // PROT_READ       0x1   PTE_R (1L << 1)
    // PROT_WRITE      0x2   PTE_W (1L << 2)
    // PROT_EXEC       0x4   PTE_X (1L << 3)
    //                       PTE_U (1L << 4)
    if (mappages(p->pagetable, va, PGSIZE, (uint64)mem, (v->permissions << 1) | PTE_U) != 0)
    {
        kfree(mem);
        return -5; // map page failed
    }

    // 从文件中读取该页的内容
    struct file *f = v->f;
    ilock(f->ip);                                                    // 使用文件前先加锁
    readi(f->ip, 0, (uint64)mem, v->offset + va - v->start, PGSIZE); // 第二参数为0，因为mem是内核的地址
    iunlock(f->ip);                                                  // 使用完成之后释放锁
    return 0;
}

// 将vma中的内容写回到文件中
int write_back(struct vma *v, uint64 addr, int n)
{
    int r, ret = 0;

    if (v->f->writable == 0)
        return -1;

    struct file *f = v->f;

    if (f->type != FD_INODE)
        panic("write_back: only support INODE now");

    int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * BSIZE;
    int i = 0;
    while (i < n)
    {
        int n1 = n - i;
        // 每次最多写max字节的内容？
        if (n1 > max)
            n1 = max;

        begin_op();
        ilock(f->ip);
        if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
            f->off += r;
        iunlock(f->ip);
        end_op();

        if (r != n1)
            break;

        i += r;
    }

    ret = (i == n ? n : -1);

    return ret;
}