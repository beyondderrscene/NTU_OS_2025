#include "kernel/types.h"
#include "user/user.h"
#include "user/threads.h"

#define RUN_PRIORITY_RR 1  // 1 = Priority-RR, 0 = HRRN

struct thread_config {
    int id;
    int arrival_time;
    int burst_time;
    int priority;
};

void f(void *arg)
{
    int id = (int)(uint64)arg;
    printf("thread #%d is running\n", id);
    while (1) {}
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(2, "Usage: task4 [HRRN|PRR]\n");
        exit(1);
    }

    int use_priority_rr = 0;
    if (strcmp(argv[1], "PRR") == 0) {
        use_priority_rr = 1;
    } else if (strcmp(argv[1], "HRRN") == 0) {
        use_priority_rr = 0;
    } else {
        fprintf(2, "Unknown scheduling mode: %s\n", argv[1]);
        exit(1);
    }

    struct thread_config threads[] = {
        {1, 0, 7, 1}, 
        {2, 3, 3, 0},
        {3, 1, 10, 2},
    };

    int n = sizeof(threads) / sizeof(struct thread_config);
    for (int i = 0; i < n; i++) {
        struct thread_config tc = threads[i];

        if (use_priority_rr) {
            tc.arrival_time = 0; 
        }

        struct thread *t = thread_create(f, (void *)(uint64)tc.id, 0, tc.burst_time, -1, 1);

        if (use_priority_rr) {
            thread_set_priority(t, tc.priority);
        }

        thread_add_at(t, tc.arrival_time);
    }

    thread_start_threading();
    printf("\nexited\n");
    exit(0);
}