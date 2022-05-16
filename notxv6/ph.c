#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

#define NBUCKET 5
#define NKEYS 100000

/*
pthread_mutex_t lock;            // declare a lock
pthread_mutex_init(&lock, NULL); // initialize the lock
pthread_mutex_lock(&lock);       // acquire lock
pthread_mutex_unlock(&lock);     // release lock
*/

struct entry
{
    int key;
    int value;
    struct entry *next;
};

struct ht
{
    struct entry *t;
    pthread_mutex_t lock;
};

struct ht table[NBUCKET];
// struct entry *table[NBUCKET]; // 有5条entry链
int keys[NKEYS];
int nthread = 1;

int initlock()
{
    for (int i = 0; i < NBUCKET; i++)
    {
        if (pthread_mutex_init(&table[i].lock, NULL) != 0)
            return -1;
    }
    return 0;
}

double
now()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static void
insert(int key, int value, struct entry **p, struct entry *n)
{
    struct entry *e = malloc(sizeof(struct entry));
    e->key = key;
    e->value = value;

    e->next = n;
    *p = e;
}

static void put(int key, int value)
{
    // 随机对一条链进行操作
    int i = key % NBUCKET;

    // is the key already present?

    // 寻找key
    struct entry *e = 0;
    for (e = table[i].t; e != 0; e = e->next)
    {
        if (e->key == key)
            break;
    }

    // 如果ke存在，则更新value
    if (e)
    {
        // update the existing key.
        e->value = value;
    }
    // 如果key不存在，则插入新的{key, value}
    else
    {
        pthread_mutex_lock(&table[i].lock);
        // the new is new.
        insert(key, value, &table[i].t, table[i].t);
        pthread_mutex_unlock(&table[i].lock);
    }
}

// 查询哈希表
static struct entry *
get(int key)
{
    int i = key % NBUCKET;

    struct entry *e = 0;
    for (e = table[i].t; e != 0; e = e->next)
    {
        if (e->key == key)
            break;
    }

    return e;
}

static void *
put_thread(void *xa)
{
    int n = (int)(long)xa; // thread number
    int b = NKEYS / nthread;

    // 每个put_thread使用一组key
    for (int i = 0; i < b; i++)
    {
        put(keys[b * n + i], n);
    }

    return NULL;
}

static void *
get_thread(void *xa)
{
    int n = (int)(long)xa; // thread number
    int missing = 0;

    // 查找keys[i]是否存在
    for (int i = 0; i < NKEYS; i++)
    {
        struct entry *e = get(keys[i]);
        if (e == 0)
            missing++;
    }
    printf("%d: %d keys missing\n", n, missing);
    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t *tha;
    void *value;
    double t1, t0;

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s nthreads\n", argv[0]);
        exit(-1);
    }
    nthread = atoi(argv[1]);

    // 分配空间存储pthread_t指针
    tha = malloc(sizeof(pthread_t) * nthread);

    // 初始化随机数
    srandom(0);
    assert(NKEYS % nthread == 0);
    for (int i = 0; i < NKEYS; i++)
    {
        // 生成随机的key
        keys[i] = random();
    }

    // 初始化锁
    assert(initlock() == 0);

    //
    // first the puts
    //
    t0 = now();
    for (int i = 0; i < nthread; i++)
    {
        assert(pthread_create(&tha[i], NULL, put_thread, (void *)(long)i) == 0);
    }
    for (int i = 0; i < nthread; i++)
    {
        assert(pthread_join(tha[i], &value) == 0);
    }
    t1 = now();

    printf("%d puts, %.3f seconds, %.0f puts/second\n",
           NKEYS, t1 - t0, NKEYS / (t1 - t0));

    //
    // now the gets
    //
    t0 = now();
    for (int i = 0; i < nthread; i++)
    {
        assert(pthread_create(&tha[i], NULL, get_thread, (void *)(long)i) == 0);
    }
    for (int i = 0; i < nthread; i++)
    {
        assert(pthread_join(tha[i], &value) == 0);
    }
    t1 = now();

    printf("%d gets, %.3f seconds, %.0f gets/second\n",
           NKEYS * nthread, t1 - t0, (NKEYS * nthread) / (t1 - t0));
}
