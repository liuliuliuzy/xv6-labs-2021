#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h" //include syscalls

/*
用pipe和fork实现一个线性素数筛选器
线性素数筛选法：https://swtch.com/~rsc/thread/

伪代码：
p = get a number from left neighbor
print p
loop:
    n = get a number from left neighbor
    if (p does not divide n)
        send n to right neighbor

*/

void handle(int pipeline)
{
    if (fork() == 0) // child
    {
        int pNum = 0;
        read(pipeline, &pNum, 4);
        printf("prime %d\n", pNum);

        int p[2];
        pipe(p);
        int nextNum = 0;
        while (1)
        {
            if (read(pipeline, &nextNum, 4) > 0)
            {
                // 过滤掉所有被当前进程的pNum整除的所有数
                if (nextNum % pNum > 0)
                {
                    write(p[1], &nextNum, 4);
                }
            }
            else
            {
                break;
            }
        }
        // if there are no other numbers
        close(p[1]);
        if (nextNum == 0)
        {
            close(p[0]);
            exit(0);
        }
        handle(p[0]);
    }
    else
    {
        close(pipeline);
        // wait child process
        wait(0);
    }
}

int main(int argc, char *argv[])
{
    int p[2];
    pipe(p);
    // feed numbers, stop at 35
    for (int i = 2; i < 35; i++)
    {
        write(p[1], &i, 4);
    }
    // close write side
    close(p[1]);
    // read number from the read side & handle it
    handle(p[0]);
    exit(0);
}