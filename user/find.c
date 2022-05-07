#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

void find(char *path, char *target)
{
    int fd;
    char buf[512], *p;

    struct dirent de;
    struct stat st;

    // 打开目录文件
    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    // 在buf中存储父级目录
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';

    // 读取目录文件，处理其中记录的各个子项
    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {
        // 对于de.inum == 0的情况，跳过
        if (de.inum == 0)
            continue;

        // 复制文件名，拼接到当前路径之后
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        // 读取file stat信息，判断是文件还是目录
        if (stat(buf, &st) < 0)
        {
            printf("find: cannot stat %s\n", buf);
            continue;
        }

        switch (st.type)
        {
        case T_FILE:
            // 如果是文件，那么匹配文件名与target字符串
            if (strcmp(p, target) == 0)
                printf("%s\n", buf);
            break;
        case T_DIR:
            // 如果是目录，那么递归搜索之
            // 但是要注意，避免重复搜索 "." 与 ".."
            if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
                break;
            // 递归调用find，注意参数的变化
            find(buf, target);
            break;
        default:
            printf("find: unknown type for `%s`\n", buf);
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "find: you need specify the arguments\n");
        exit(-1);
    }
    if (argc < 3)
    {
        find(".", argv[1]);
        exit(0);
    }
    find(argv[1], argv[2]);
    exit(0);
}