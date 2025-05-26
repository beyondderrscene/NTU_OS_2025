#include "kernel/types.h"
#include "user/setjmp.h"
#include "user/threads.h"
#include "user/user.h"
#define NULL 0


static struct thread* current_thread = NULL;
static int id = 1;

//the below 2 jmp buffer will be used for main function and thread context switching
static jmp_buf env_st; 
static jmp_buf env_tmp;  
static jmp_buf handler_env_tmp;  // Add this global variable

struct thread *get_current_thread() {
    return current_thread;
}

struct thread *thread_create(void (*f)(void *), void *arg){
    struct thread *t = (struct thread*) malloc(sizeof(struct thread));
    unsigned long new_stack_p;
    unsigned long new_stack;
    new_stack = (unsigned long) malloc(sizeof(unsigned long)*0x100);
    new_stack_p = new_stack +0x100*8-0x2*8;
    t->fp = f; 
    t->arg = arg;
    t->ID  = id;
    t->buf_set = 0;
    t->stack = (void*) new_stack; //points to the beginning of allocated stack memory for the thread.
    t->stack_p = (void*) new_stack_p; //points to the current execution part of the thread.
    id++;

    // Part 2
    t->suspended = 0;
    t->sig_handler[0] = NULL_FUNC;
    t->sig_handler[1] = NULL_FUNC;
    t->signo = -1;
    t->handler_buf_set = 0;

    return t;
}


void thread_add_runqueue(struct thread *t) {
    if (current_thread == NULL) {
        current_thread = t;
        current_thread->next = t;
        current_thread->previous = t;
    } else {
        // Add to end of runqueue
        // ini implement circular doubly linked list yg gaje itu
        t->next = current_thread; // update urusan t pny next
        t->previous = current_thread->previous; // update ekornya linked list
        current_thread->previous->next = t; // ganti next dari ekor linked list
        current_thread->previous = t; // oke, ganti ekor
        
        // Inherit signal handlers from parent thread
        t->sig_handler[0] = current_thread->sig_handler[0]; // jujur gw gtau signal handler ini buat apa
        t->sig_handler[1] = current_thread->sig_handler[1];
    }
}

void thread_yield(void) {
    if(current_thread->signo != -1) {
        int pds = setjmp(current_thread->handler_env);
        if (pds == 0) {
            current_thread->handler_buf_set = 1;
            schedule();
            dispatch();
        } else {
            current_thread->handler_buf_set = 0;
            return;
        }
    }

    int pd = setjmp(current_thread->env);
    if (pd == 0) {
        current_thread->buf_set = 1;
        schedule();
        dispatch();
    } else {
        current_thread->buf_set = 0;
        return;
    }
}

void thread_func() {
    current_thread->fp(current_thread->arg);
    thread_exit();
}

void sig_func() {
    int sig_no = current_thread->signo;
    current_thread->sig_handler[sig_no](sig_no);
    current_thread->signo = -1;

    if(current_thread->buf_set)
        longjmp(current_thread->env, 1);
    else {
        env_tmp->sp = (unsigned long)current_thread->stack_p;
        env_tmp->ra = (unsigned long)thread_func;
        longjmp(env_tmp, 1);
    }
}

void dispatch(void) {
    int sig_no = current_thread->signo;
    if(sig_no != -1) {
        if(current_thread->sig_handler[sig_no] == NULL_FUNC) {
            thread_exit();
            return;
        }

        if(current_thread->handler_buf_set == 0) {
            // Use lower part of existing stack for handler
            env_tmp->sp = (unsigned long)current_thread->stack_p - 0x80;
            env_tmp->ra = (unsigned long)sig_func;
            longjmp(env_tmp, 1);
        } else {
            longjmp(current_thread->handler_env, 1);
        }
    }

    if (current_thread->buf_set == 0) {
        env_tmp->sp = (unsigned long)current_thread->stack_p;
        env_tmp->ra = (unsigned long)thread_func;
        longjmp(env_tmp, 1);
    } else {
        longjmp(current_thread->env, 1);
    }
}

//schedule will follow the rule of FIFO
void schedule(void){
    struct thread *head;
    head = current_thread;
    current_thread = current_thread->next;
    
    //Part 2: TO DO
    // skip yg suspended disini, trus incase infinite loop bikin condition baru
    while(current_thread->suspended && current_thread != head){
        current_thread = current_thread->next;
    }

    // ya basically safety measure sih
    if(current_thread == head && current_thread == head){
        current_thread = head;
    }
}

void thread_exit(void){
    if (current_thread->next != current_thread) {
        //TO DO
        // Remove current thread from the runqueue
        current_thread->previous->next = current_thread->next;
        current_thread->next->previous = current_thread->previous;
        
        // Save next thread before freeing current
        struct thread *next_thread = current_thread->next;
        
        // Free the thread's memory
        void *stack = current_thread->stack;
        free(stack);
        free(current_thread);
        
        // Update current_thread to point to the next thread
        current_thread = next_thread;
        
        // Skip any suspended threads
        struct thread *start = current_thread;
        while (current_thread->suspended) {
            current_thread = current_thread->next;
            if (current_thread == start) {
                // All threads are suspended
                break;
            }
        }
        
        // Dispatch the next thread
        dispatch();
    } else {
        // Last thread is exiting
        free(current_thread->stack);
        free(current_thread);
        current_thread = NULL;
        longjmp(env_st, 1);
    }
}

void thread_start_threading(void){
    //TO DO
    // save context, lgsg run first thread pake dispatch
    if(setjmp(env_st) == 0){
        dispatch();
    }
    return;
}

//PART 2
void thread_register_handler(int signo, void (*handler)(int)){
    if(signo >= 0 && signo <= 1){ // register handler if it gets a valid signo
        current_thread->sig_handler[signo] = handler;
    }
}

void thread_kill(struct thread *t, int signo){
    //TO DO
    // ambil aman
    if(signo >= 0 && signo <= 1 && t->signo == -1){
        t->signo = signo;
    }
}

void thread_suspend(struct thread *t) {
    //TO DO
    t->suspended = 1;
    if(t == current_thread){
        thread_yield(); // save context before suspended
    }
}

void thread_resume(struct thread *t) {
    //TO DO
    t->suspended = 0; // tinggal ganti status yey
}