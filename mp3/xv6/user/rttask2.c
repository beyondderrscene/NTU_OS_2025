#include "kernel/types.h"
#include "user/user.h"
#include "user/threads.h"

#define NULL 0

int k = 0;

void f(void *arg)
{
    while (1) {
        k++;
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(2, "Usage: rttask1 [EDF_CBS|DM]\n");
        exit(1);
    }

    if (strcmp(argv[1], "EDF_CBS") == 0) {
        struct thread *t1 = thread_create(f, NULL, 1, 5, 9, 2);
        init_thread_cbs(t1, 5, 1);
        thread_add_at(t1, 0);

        struct thread *t2 = thread_create(f, NULL, 1, 4, 5, 2);
        init_thread_cbs(t2, 4, 1);
        thread_add_at(t2, 3);

    } else if (strcmp(argv[1], "DM") == 0) {
        struct thread *t1 = thread_create(f, NULL, 1, 7, 14, 2);
        thread_add_at(t1, 0);
        
        struct thread *t2 = thread_create(f, NULL, 1, 5, 10, 2);
        thread_add_at(t2, 0);

    } else {
        fprintf(2, "Unknown mode: %s\n", argv[1]);
        exit(1);
    }

    thread_start_threading();
    printf("\nexited\n");
    exit(0);
}

