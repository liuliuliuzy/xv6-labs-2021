#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        // 2 means stderr in xv6
        fprintf(2, "sleep: no arguments passed.");
        exit(0);
    }

    if (argc > 2)
    {
        fprintf(2, "sleep: to many arguments.");
        exit(0);
    }

    // now begin to sleep
    int sleepCounts = atoi(argv[1]);
    sleep(sleepCounts);
    // exit after sleep
    exit(0);
}