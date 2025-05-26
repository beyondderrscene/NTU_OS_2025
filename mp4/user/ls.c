#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

char *fmtname(char *path)
{
    static char buf[DIRSIZ + 1];
    char *p;

    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    // Return blank-padded name.
    if (strlen(p) >= DIRSIZ)
        return p;
    memmove(buf, p, strlen(p));
    memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
    return buf;
}

/* TODO: Access Control & Symbolic Link */
void ls(char *path)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, O_NOACCESS)) < 0)
    {
        fprintf(2, "ls: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_FILE:
    case T_SYMLINK:
    {
        // Check if symlink points to a directory
        struct stat target_st;
        int tfd = open(path, 0); // follow the symlink
        if (tfd >= 0)
        {
            if (fstat(tfd, &target_st) >= 0 && target_st.type == T_DIR)
            {
                close(tfd);
                close(fd);
                ls(path); // recurse into directory
                return;
            }
            close(tfd);
        }

        // If not a directory, print symlink or file info
        if (st.type == T_SYMLINK)
        {
            char target[128];
            int slfd = open(path, O_NOACCESS); // open symlink itself
            if (slfd >= 0)
            {
                int n = read(slfd, target, sizeof(target) - 1);
                if (n >= 0)
                {
                    target[n] = '\0';
                    printf("%s -> %s %d %d %d\n", fmtname(path), target, st.type, st.ino, st.size);
                }
                else
                {
                    printf("%s [read error] %d %d %d\n", fmtname(path), st.type, st.ino, st.size);
                }
                close(slfd);
            }
            else
            {
                printf("%s [broken symlink] %d %d %d\n", fmtname(path), st.type, st.ino, st.size);
            }
        }
        else
        {
            printf("%s %d %d %d\n", fmtname(path), st.type, st.ino, st.size);
        }
        break;
    }

    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            printf("ls: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0)
            {
                printf("ls: cannot stat %s\n", buf);
                continue;
            }

            if (st.type == T_SYMLINK)
            {
                char target[128];
                int tfd = open(buf, 0);
                if (tfd >= 0)
                {
                    int n = read(tfd, target, sizeof(target) - 1);
                    target[n] = 0;
                    close(tfd);
                    printf("%s -> %s %d %d %d\n", fmtname(buf), target, st.type, st.ino, st.size);
                }
                else
                {
                    printf("%s [broken symlink] %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
                }
            }
            else
            {
                printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
            }
        }
        break;
    }

    close(fd);
}

int main(int argc, char *argv[])
{
    int i;

    if (argc < 2)
    {
        ls(".");
        exit(0);
    }
    for (i = 1; i < argc; i++)
        ls(argv[i]);
    exit(0);
}
