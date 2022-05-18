struct buf
{
    int valid; // has data been read from disk?
    int disk;  // does disk "own" buf?
    // 磁盘信息
    uint dev;     // 哪一块？
    uint blockno; // block号？
    struct sleeplock lock;
    uint refcnt;
    struct buf *prev; // LRU cache list
    struct buf *next;
    uchar data[BSIZE];
};
