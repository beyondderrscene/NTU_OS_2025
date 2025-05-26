#include "kernel/types.h"
#include "user/user.h"
#include "user/list.h"
#include "user/threads.h"
#include "user/threads_sched.h"
#include <limits.h>
#define NULL 0

/* default scheduling algorithm */
#ifdef THREAD_SCHEDULER_DEFAULT
struct threads_sched_result schedule_default(struct threads_sched_args args)
{
    struct thread *thread_with_smallest_id = NULL;
    struct thread *th = NULL;
    list_for_each_entry(th, args.run_queue, thread_list) {
        if (thread_with_smallest_id == NULL || th->ID < thread_with_smallest_id->ID)
            thread_with_smallest_id = th;
    }

    struct threads_sched_result r;
    if (thread_with_smallest_id != NULL) {
        r.scheduled_thread_list_member = &thread_with_smallest_id->thread_list;
        r.allocated_time = thread_with_smallest_id->remaining_time;
    } else {
        r.scheduled_thread_list_member = args.run_queue;
        r.allocated_time = 1;
    }

    return r;
}
#endif

/* MP3 Part 1 - Non-Real-Time Scheduling */

// HRRN
#ifdef THREAD_SCHEDULER_HRRN
struct threads_sched_result schedule_hrrn(struct threads_sched_args args)
{
    struct thread *candidate = NULL;
    struct thread *th = NULL;

    list_for_each_entry(th, args.run_queue, thread_list) {
        if(candidate == NULL){
            candidate = th;
            continue;
        }
        // compare response ratio
        int candidate_rr, th_rr;
        candidate_rr = (args.current_time - candidate->arrival_time + candidate->remaining_time) * th->remaining_time;
        th_rr = (args.current_time - th->arrival_time + th->remaining_time) * candidate->remaining_time;
        if(th_rr > candidate_rr){
            candidate = th;
        }
        else if(th_rr == candidate_rr && th->ID < candidate->ID){
            candidate = th;
        }
    }

    struct threads_sched_result r;
    if (candidate) {
        r.scheduled_thread_list_member = &candidate->thread_list;
        r.allocated_time = candidate->remaining_time;
    }
    else {
        r.scheduled_thread_list_member = args.run_queue;
        r.allocated_time = 1;
    }

    return r;
}
#endif

#ifdef THREAD_SCHEDULER_PRIORITY_RR
// priority Round-Robin(RR)
struct threads_sched_result schedule_priority_rr(struct threads_sched_args args) 
{
    static struct curr_state {
        int last_run_time;
        int last_thread;
        int current_priority;
    } state = { -1, -1, -1 };
    
    struct threads_sched_result r;
    // empty runqueue
    if (list_empty(args.run_queue)) {
        r.scheduled_thread_list_member = args.run_queue;
        r.allocated_time = 1;
        return r;
    }
    
    // tiered approach priority classification
    struct {
        int ct;
        struct thread *first;
        struct thread *next_after;
    }ptier[16] = {0};     
    
    int min_p = -1;
    struct thread *th = NULL;
    
    list_for_each_entry(th, args.run_queue, thread_list) {
        int p = th->priority;
        
        if (min_p == -1 || p < min_p) min_p = p;
        
        ptier[p].ct++;
        
        if (ptier[p].first == NULL || th->ID < ptier[p].first->ID) {
            ptier[p].first = th;
        }
        
        if (th->ID > state.last_thread) {
            if (ptier[p].next_after == NULL || 
                th->ID < ptier[p].next_after->ID) {
                ptier[p].next_after = th;
            }
        }
    }
    
    // reset if priority changed/time went backward
    if (state.current_priority != min_p || args.current_time < state.last_run_time) {
        state.last_thread = -1;
        state.current_priority = min_p;
    }
    
    struct thread *candidate = NULL;
    // 1 thread left
    if (ptier[min_p].ct == 1) {
        candidate = ptier[min_p].first;
    }
    else {
        if (state.last_thread == -1) {
            candidate = ptier[min_p].first;
        }
        else if (ptier[min_p].next_after != NULL) {
            candidate = ptier[min_p].next_after;
        }
        else {
            candidate = ptier[min_p].first;
        }
    }
    
    int temp_time;
    if (ptier[min_p].ct == 1) {
        temp_time = candidate->remaining_time;
    }
    else {
        if (candidate->remaining_time < args.time_quantum) {
            temp_time = candidate->remaining_time;
        } else {
            temp_time = args.time_quantum;
        }
    }
    
    state.last_run_time = args.current_time;
    state.last_thread = candidate->ID;
    
    r.scheduled_thread_list_member = &candidate->thread_list;
    r.allocated_time = temp_time;
    
    return r;
}
#endif

/* MP3 Part 2 - Real-Time Scheduling*/

#if defined(THREAD_SCHEDULER_EDF_CBS) || defined(THREAD_SCHEDULER_DM)
static struct thread *__check_deadline_miss(struct list_head *run_queue, int current_time)
{
    struct thread *th = NULL;
    struct thread *thread_missing_deadline = NULL;
    list_for_each_entry(th, run_queue, thread_list) {
        if (th->current_deadline <= current_time) {
            if (thread_missing_deadline == NULL)
                thread_missing_deadline = th;
            else if (th->ID < thread_missing_deadline->ID)
                thread_missing_deadline = th;
        }
    }
    return thread_missing_deadline;
}
#endif

#ifdef THREAD_SCHEDULER_DM
/* Deadline-Monotonic Scheduling */
static int __dm_thread_cmp(struct thread *a, struct thread *b)
{
    // compare a with b (period = deadline)
    // if 1, a win
    // if -1, b win
    if (a->period < b->period) return 1;  // a has higher priority
    else if (a->period > b->period) return -1; // b has higher priority
    else {
        // For threads with the same deadline, use ID as tiebreaker (smaller ID = higher priority)
        if (a->ID < b->ID) return 1;  // a has higher priority
        else return -1; // b has higher priority
    }
}

struct threads_sched_result schedule_dm(struct threads_sched_args args) 
{
    struct threads_sched_result r;

    // printf("current_time = %d\n",args.current_time);

    // struct thread *temp;
    // if(!list_empty(args.run_queue)){
    //     printf("run queue:\n");
    //     list_for_each_entry(temp, args.run_queue, thread_list){
    //         printf("[%d, %d, %d, %d, %d]\n", temp->ID, temp->arrival_time, temp->current_deadline, temp->period, temp->remaining_time);
    //     }
    // }
    // else printf("runqueue empty\n");
    // struct release_queue_entry *tempp;
    // if(!list_empty(args.release_queue)){
    //     printf("release queue:\n");
    //     list_for_each_entry(tempp, args.release_queue, thread_list){
    //         printf("[%d, %d]\n", tempp->thrd->ID, tempp->release_time);
    //     }
    // }
    // else printf("releasequeue empty\n");

    // sleep
    if (list_empty(args.run_queue)) {
        r.scheduled_thread_list_member = args.run_queue;
        
        int time = -1;
        struct release_queue_entry *timeth;
        list_for_each_entry(timeth, args.release_queue, thread_list){
            if(time == -1 && timeth->release_time > args.current_time){
                time = timeth->release_time - args.current_time;
                continue;
            }
            
            if(time > timeth->release_time - args.current_time && timeth->release_time > args.current_time){
                time = timeth->release_time - args.current_time;
            }
        }
        // time = 1;
        r.allocated_time = time;
        return r;
    }

    // check missed deadline
    struct thread *missed_deadline_thread = __check_deadline_miss(args.run_queue, args.current_time);
    if (missed_deadline_thread != NULL) {
        r.scheduled_thread_list_member = &missed_deadline_thread->thread_list;
        r.allocated_time = 0;
        return r;
    }
    
    // find candidate
    struct thread *candidate = NULL;
    struct thread *th;
    // find for highest priority in runqueue
    list_for_each_entry(th, args.run_queue, thread_list){
        if(candidate == NULL || __dm_thread_cmp(candidate, th) < 0){
            candidate = th;
        }
    }

    // find for possible timeslice in releasequeue
    int temp_time = candidate->remaining_time;
    struct release_queue_entry *rth;
    list_for_each_entry(rth, args.release_queue, thread_list){
        th = rth->thrd;
        if(__dm_thread_cmp(candidate, th) < 0 && rth->release_time - args.current_time < temp_time){ // candidate lose priority
            temp_time = rth->release_time - args.current_time;
        }
    }

    r.scheduled_thread_list_member = &candidate->thread_list;
    r.allocated_time = temp_time;

    return r;
}
#endif

#ifdef THREAD_SCHEDULER_EDF_CBS
// EDF with CBS comparation
static int __edf_thread_cmp(struct thread *a, struct thread *b)
{
    // TO DO
    // 1 -> a win
    // 0 -> b win
    if(a->current_deadline < b->current_deadline) return 1;
    else if(a->current_deadline > b->current_deadline) return 0;
    else{
        if(a->ID < b->ID) return 1;
        else return 0;
    }
}
//  EDF_CBS scheduler
struct threads_sched_result schedule_edf_cbs(struct threads_sched_args args)
{
    struct threads_sched_result r;

    // check missed deadline for hard tasks
    struct thread *missed_deadline = __check_deadline_miss(args.run_queue, args.current_time);
    if(missed_deadline != NULL && missed_deadline->cbs.is_hard_rt){
        r.scheduled_thread_list_member = &missed_deadline->thread_list;
        r.allocated_time = 0;
        return r;
    }

    // empty runqueue, find sleep time
    if (list_empty(args.run_queue)) {
        r.scheduled_thread_list_member = args.run_queue;
        
        int time = -1;
        struct release_queue_entry *timeth;
        list_for_each_entry(timeth, args.release_queue, thread_list){
            if(time == -1 && timeth->release_time > args.current_time){
                time = timeth->release_time - args.current_time;
                continue;
            }
            
            if(time > timeth->release_time - args.current_time && timeth->release_time > args.current_time){
                time = timeth->release_time - args.current_time;
            }
        }
        // time = 1;
        r.allocated_time = time;
        return r;
    }

    struct thread *th;
    // toggle throttle: replenish
    list_for_each_entry(th, args.run_queue, thread_list){
        if(!th->cbs.is_hard_rt){
            if(th->cbs.is_throttled && th->current_deadline <= args.current_time){
                th->cbs.is_throttled = 0;
                th->current_deadline = args.current_time + th->period;
                th->cbs.remaining_budget = th->cbs.budget;
            }
        }
    }

    // toggle throttle: throttle
    list_for_each_entry(th, args.run_queue, thread_list){
        if(!th->cbs.is_hard_rt && !th->cbs.is_throttled){
            if(th->cbs.remaining_budget > 0 && th->current_deadline - args.current_time > 0){
                if(th->cbs.remaining_budget * th->period >  th->cbs.budget * (args.current_time-th->current_deadline)){
                    th->current_deadline = args.current_time + th->period;
                    th->cbs.remaining_budget = th->cbs.budget;
                }
            }
            if(th->cbs.remaining_budget <= 0) th->cbs.is_throttled = 1;
        }
    }

    // now choose
    struct thread *candidate = NULL;
    list_for_each_entry(th, args.run_queue, thread_list){
        if(th->cbs.is_throttled) continue;
        if(candidate == NULL || __edf_thread_cmp(th, candidate)) candidate = th;
    }

    // if none selected -> all throttled soft tasks
    struct release_queue_entry *rth;
    int ttime = -1;
    if(candidate == NULL){
        list_for_each_entry(rth, args.release_queue, thread_list){
            if(ttime == -1 || rth->release_time - args.current_time < ttime){
                ttime = rth->release_time - args.current_time;
            }
        }

        list_for_each_entry(th, args.run_queue, thread_list){
            if(!th->cbs.is_throttled && th->current_deadline > args.current_time && th->current_deadline - args.current_time < ttime){
                ttime = th->current_deadline - args.current_time;
            }
        }
        r.scheduled_thread_list_member = args.run_queue;
        r.allocated_time = (ttime == -1)? 1: ttime;
        return r;
    }

    // time allocation
    int temp_time = candidate->remaining_time;
    if(!candidate->cbs.is_hard_rt){
        if(candidate->cbs.remaining_budget <= temp_time) temp_time = candidate->cbs.remaining_budget;
    }

    // timeslice: release queue
    list_for_each_entry(rth, args.release_queue, thread_list){
        th = rth->thrd;
        if(!th->cbs.is_throttled && __edf_thread_cmp(candidate, th) < 1 && rth->release_time - args.current_time < temp_time){
            temp_time = rth->release_time - args.current_time;
        }
    }

    // timeslice: throttled tasks
    list_for_each_entry(th, args.run_queue, thread_list){
        if(th->cbs.is_throttled && th->current_deadline > args.current_time){
            if(th->current_deadline - args.current_time < temp_time){
                temp_time = th->current_deadline - args.current_time;
            }
        }
    }

    r.allocated_time = (temp_time > 0)? temp_time : 1;
    r.scheduled_thread_list_member = &candidate->thread_list;

    // notify the throttle task
    // TO DO

    // first check if there is any thread has missed its current deadline
    // TO DO
    
    // handle the case where run queue is empty
    // TO DO

    return r;
}
#endif