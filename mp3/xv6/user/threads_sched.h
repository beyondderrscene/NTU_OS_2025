#ifndef THREADS_SCHE_H_
#define THREADS_SCHE_H_

#include "user/list.h"

struct threads_sched_args {
    // the number of ticks since threading starts
    int current_time;
    // the time quantum for each thread (for round-robin scheduling)
    int time_quantum;
    // the linked list containing all the threads available to be run
    struct list_head *run_queue;
    // the linked list containing all the threads that will be available later
    struct list_head *release_queue;
};

struct threads_sched_result {
    // `scheduled_thread_list_member` should point to the `thread_list` member of
    // the scheduled `struct thread` entry
    struct list_head *scheduled_thread_list_member;
    // the number of ticks allocated for this thread to run
    int allocated_time;
};
#ifdef THREAD_SCHEDULER_DEFAULT
struct threads_sched_result schedule_default(struct threads_sched_args args);
#endif
#ifdef THREAD_SCHEDULER_HRRN
struct threads_sched_result schedule_hrrn (struct threads_sched_args args);
#endif
#ifdef THREAD_SCHEDULER_PRIORITY_RR
struct threads_sched_result schedule_priority_rr(struct threads_sched_args args);
#endif
#ifdef THREAD_SCHEDULER_EDF_CBS
struct threads_sched_result schedule_edf_cbs(struct threads_sched_args args);
#endif
#ifdef THREAD_SCHEDULER_DM
struct threads_sched_result schedule_dm(struct threads_sched_args args);
#endif

#endif
