#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

// 标准输入的内容中，参数之间的分隔符
const char delimitation[] = " \t\r\v";

void doexec(char *command, char **argv)
{
    if (fork() == 0)
    {
        exec(command, argv);
    }
    else
    {
        wait(0);
    }
}

int main(int argc, char *argv[])
{
    char *cmdArgs[MAXARG];
    char *command;
    // 将cmdArgs[]中的字符串地址都初始化为0
    memset(cmdArgs, 0, MAXARG * 8);
    if (argc < 2)
    {
        fprintf(2, "xargs: no arguments\n");
        exit(1);
    }

    if (argc > MAXARG + 1)
    {
        fprintf(2, "xargs: too many arguments\n");
        exit(1);
    }

    // 将xargs的参数存储为要执行的目标指令
    command = argv[1];
    int extraArgStart;
    // src的索引从1开始，dst的索引从0开始
    for (extraArgStart = 1; extraArgStart < argc; extraArgStart++)
    {
        cmdArgs[extraArgStart - 1] = argv[extraArgStart];
    }
    // 复位索引，让extraArgsStart指向cmdArgs中的下一个空单元处，便于在现有目标指令的后面添加从标准输入中读来的内容
    extraArgStart--;

    int extraArgIndex = 0, extraArgInner = 0;
    char tmpChar;

    // 每次读一个字节
    while (read(0, &tmpChar, 1) > 0)
    {
        // 如果遇到分隔字符，则需要移动写入指针到cmdArgs中的下一个单元
        if (strchr(delimitation, tmpChar) != 0)
        {
            // 如果当前已经读入了一部分的内容，则将其看作一个参数进行处理
            if (extraArgInner > 0)
            {
                // 在其末尾添加0字节，以满足C风格字符串的要求
                cmdArgs[extraArgStart + extraArgIndex][extraArgInner++] = 0;
                // 移动Index，使其指向cmdArgs的下一个单元
                extraArgIndex++;
                // Inner复位为0
                extraArgInner = 0;
            }
        }
        // 如果遇到'\n'，需要执行指令
        else if (tmpChar == '\n')
        {
            // 如果已经读入了一部分的内容，处理同上
            if (extraArgInner > 0)
            {
                cmdArgs[extraArgStart + extraArgIndex][extraArgInner++] = 0;
                extraArgIndex++;
            }

            // 将下一个字符串指针置为0，设置参数列表的边界
            if (cmdArgs[extraArgStart + extraArgIndex] != 0)
            {
                free(cmdArgs[extraArgStart + extraArgIndex]);
                cmdArgs[extraArgStart + extraArgIndex] = 0;
            }

            // 执行指令
            doexec(command, cmdArgs);

            // 执行完之后，将局部变量设置回初始值
            extraArgIndex = 0;
            extraArgInner = 0;
        }
        // 其它情况，则将读取的字符写入到当前的字符串中即可
        else
        {
            // 判断是否超出了参数个数的限制，报错然后退出
            if (extraArgStart + extraArgIndex > MAXARG - 1)
            {
                fprintf(2, "xargs: too many args for this line\n");
                break;
            }
            // 写入前检查字符串指针是否为0，如果为0的话需要调用malloc申请空间
            if (cmdArgs[extraArgStart + extraArgIndex] == 0)
            {
                cmdArgs[extraArgStart + extraArgIndex] = malloc(100);
            }
            cmdArgs[extraArgStart + extraArgIndex][extraArgInner++] = tmpChar;
        }
    }

    // 释放之前malloc的空间
    for (int j = extraArgStart; j < MAXARG; j++)
    {
        // 检查并释放之前malloc的空间，不然会造成内存泄露！
        if (cmdArgs[j])
        {
            free(cmdArgs[j]);
        }
    }
    exit(0);
}