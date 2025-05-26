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
        struct thread *t1 = thread_create(f, NULL, 1, 10, 20, 2);
        init_thread_cbs(t1, 10, 1);
        thread_add_at(t1, 0);

        struct thread *t2 = thread_create(f, NULL, 1, 8, 15, 2);
        init_thread_cbs(t2, 8, 1);
        thread_add_at(t2, 5);

        struct thread *t3 = thread_create(f, NULL, 1, 15, 20, 2);
        init_thread_cbs(t3, 15, 0);
        thread_add_at(t3, 12);

    } else if (strcmp(argv[1], "DM") == 0) {
        struct thread *t1 = thread_create(f, NULL, 1, 1, 10, 2);
        thread_add_at(t1, 3);

        struct thread *t2 = thread_create(f, NULL, 1, 2, 10, 2);
        thread_add_at(t2, 2);

        struct thread *t3 = thread_create(f, NULL, 1, 3, 10, 2);
        thread_add_at(t3, 1);

        struct thread *t4 = thread_create(f, NULL, 1, 4, 10, 2);
        thread_add_at(t4, 0);

        thread_start_threading();
        printf("\nexited\n");
        exit(0);
        }

    thread_start_threading();
    printf("\nexited\n");
    exit(0);
}