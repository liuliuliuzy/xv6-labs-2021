// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

// 双向链表实现LRU缓存
struct
{
    struct spinlock lock;
    struct buf buf[NBUF];

    // Linked list of all buffers, through prev/next.
    // Sorted by how recently the buffer was used.
    // head.next is most recent, head.prev is least.
    // struct buf head;
} bcache;

struct bucket
{
    struct spinlock lock;
    struct buf head; // 双向链表存储
};

#define NBUCKET 13
struct bucket bkts[NBUCKET];

// 哈希函数
int hash(uint blockno)
{
    return blockno % NBUCKET;
}

void binit(void)
{
    struct buf *b;

    initlock(&bcache.lock, "bcache");

    for (int i = 0; i < NBUCKET; i++)
    {
        bkts[i].head.next = &bkts[i].head;
        bkts[i].head.prev = &bkts[i].head;
        initlock(&bkts[i].lock, "cache bucket");
    }

    int serial = 1;
    // 将每个buf送入bucket中
    for (b = bcache.buf; b < bcache.buf + NBUF; b++)
    {
        int idx = hash(serial);

        initsleeplock(&b->lock, "buffer");

        b->next = bkts[idx].head.next;
        b->prev = &bkts[idx].head;
        bkts[idx].head.next->prev = b; // 先修改prev指针
        bkts[idx].head.next = b;       // 再修改next指针

        serial++;
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
    struct buf *b;

    int idx = hash(blockno);

    acquire(&bkts[idx].lock);
    // 从当前bucket中寻找信息完全匹配的buf
    for (b = bkts[idx].head.next; b != &bkts[idx].head; b = b->next)
    {
        if (b->dev == dev && b->blockno == blockno)
        {
            b->refcnt++;
            release(&bkts[idx].lock);

            acquiresleep(&b->lock);
            return b;
        }
    }

    struct buf *replace = 0;
    int minTime = 0x7fffffff;
    // 从当前bucket中寻找空闲的、最近使用时间点离现在最久的buf
    for (b = bkts[idx].head.next; b != &bkts[idx].head; b = b->next)
    {
        // printf("b: %p; b->refcnt: %d; b->timestamp: %d; minTime: %d\n", b, b->refcnt, b->timestamp, minTime);
        if (b->refcnt == 0 && (b->timestamp < minTime))
        {
            replace = b;
            minTime = b->timestamp;
            // printf("    b: %p; b->refcnt: %d; b->timestamp: %d\n", b, b->refcnt, b->timestamp);
        }
    }
    // printf("after first round of searching, replace is %p\n", replace);

    if (replace)
    {
        replace->dev = dev;
        replace->blockno = blockno;
        replace->refcnt = 1;
        replace->valid = 0;
        release(&bkts[idx].lock);

        acquiresleep(&replace->lock);
        return replace;
    }

    // 从bcache全局数组中寻找空闲的、最近使用时间点离现在最久的buf
    acquire(&bcache.lock);
    for (b = bcache.buf; b < bcache.buf + NBUF; b++)
    {
        if (b->refcnt == 0 && b->timestamp < minTime)
        {
            replace = b;
            minTime = b->timestamp;
        }
    }

    if (replace)
    {
        int oldIdx = hash(replace->blockno);
        // 脱链然后插入当前链表

        // 从原链表脱链
        acquire(&bkts[oldIdx].lock);
        replace->next->prev = replace->prev;
        replace->prev->next = replace->next;
        release(&bkts[oldIdx].lock);

        // 插入到当前链表
        replace->next = bkts[idx].head.next;
        replace->prev = &bkts[idx].head;
        bkts[idx].head.next->prev = replace;
        bkts[idx].head.next = replace;

        release(&bcache.lock);

        replace->dev = dev;
        replace->blockno = blockno;
        replace->refcnt = 1;
        replace->valid = 0;
        release(&bkts[idx].lock);

        acquiresleep(&replace->lock);
        return replace;
    }
    else
    {
        release(&bcache.lock);
        panic("bget: no buffers");
    }
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
    struct buf *b;

    b = bget(dev, blockno);
    if (!b->valid)
    {
        virtio_disk_rw(b, 0);
        b->valid = 1;
    }
    return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("brelse");

    releasesleep(&b->lock);

    int idx = hash(b->blockno);

    acquire(&bkts[idx].lock);
    b->refcnt--;
    if (b->refcnt == 0)
        b->timestamp = ticks;
    release(&bkts[idx].lock);
}

void bpin(struct buf *b)
{
    int idx = hash(b->blockno);
    acquire(&bkts[idx].lock);
    b->refcnt++;
    release(&bkts[idx].lock);
}

void bunpin(struct buf *b)
{
    int idx = hash(b->blockno);
    acquire(&bkts[idx].lock);
    b->refcnt--;
    release(&bkts[idx].lock);
}
