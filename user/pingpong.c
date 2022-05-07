#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    // need no arguments except the program name "pingpong"
    if (argc > 1)
    {
        fprintf(2, "pingpong: you need no arguments to execute this program (pingpong)\n");
        exit(0);
    }

    // p[0]用于读，p[1]用于写
    int p[2];
    // create pipe
    // 系统调用
    pipe(p);

    if (fork() == 0)
    { // child process
        char receivedByte;
        read(p[0], &receivedByte, 1);
        fprintf(1, "%d: received ping\n", getpid());
        write(p[1], &receivedByte, 1);
        exit(0);
    }
    else
    { // parent process
        char byteToSend = 'y';
        write(p[1], &byteToSend, 1);
        read(p[0], &byteToSend, 1);
        fprintf(1, "%d: received pong\n", getpid());
        exit(0);
    }
    exit(0);
}