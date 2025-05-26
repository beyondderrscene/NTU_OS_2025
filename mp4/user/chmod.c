#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"
#include "kernel/fcntl.h"

extern int chmod(const char *path, int mode);

// Usage:
// chmod [-R] (+|-)<perm> <path>
// e.g. chmod +rw file
//      chmod -R -w dir
void usage()
{
    fprintf(2, "Usage: chmod [-R] (+|-)(r|w|rw|wr) file_name|dir_name\n");
    exit(1);
}

void apply_chmod(const char *path, int mode)
{
    if (chmod(path, mode) < 0)
    {
        fprintf(2, "chmod: cannot chmod %s\n", path);
    }
}

int main(int argc, char *argv[])
{
    int recursive = 0;
    int mode = 0;
    char *perm_arg, *path;
    int i = 1;

    if (argc < 3 || argc > 4)
        usage();

    if (strcmp(argv[1], "-R") == 0)
    {
        recursive = 1;
        perm_arg = argv[2];
        path = argv[3];
    }
    else
    {
        perm_arg = argv[1];
        path = argv[2];
    }

    if (perm_arg[0] != '+' && perm_arg[0] != '-')
        usage();

    for (i = 1; perm_arg[i]; i++)
    {
        if (perm_arg[i] == 'r')
            mode |= M_READ;
        else if (perm_arg[i] == 'w')
            mode |= M_WRITE;
        else
            usage();
    }

    if (perm_arg[0] == '-')
        mode = M_ALL & (~mode);

    if (!recursive)
    {
        apply_chmod(path, mode);
    }
    else
    {
        // recursively chmod: chmod this dir, then chmod all children
        struct stat st;
        if (stat(path, &st) < 0)
        {
            fprintf(2, "chmod: cannot chmod %s\n", path);
            exit(1);
        }

        if (st.type != T_DIR)
        {
            apply_chmod(path, mode);
            exit(0);
        }

        // apply to the root directory
        apply_chmod(path, mode);

        // open the dir
        int fd = open(path, O_RDONLY);
        if (fd < 0)
        {
            fprintf(2, "chmod: cannot open dir %s\n", path);
            exit(1);
        }

        struct dirent de;
        char buf[512], *p;
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';

        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;

            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            main(recursive ? 4 : 3, (char *[]){"chmod", "-R", perm_arg, buf});
        }

        close(fd);
    }

    exit(0);
}
