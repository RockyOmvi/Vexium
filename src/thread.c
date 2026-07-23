#include "thread.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#ifdef _WIN32
static DWORD WINAPI win32_thread_proc(LPVOID param) {
    VexThread* thread = (VexThread*)param;
    thread->result = thread->func(thread->arg);
    thread->completed = true;
    return 0;
}
#else
static void* posix_thread_proc(void* param) {
    VexThread* thread = (VexThread*)param;
    thread->result = thread->func(thread->arg);
    thread->completed = true;
    return NULL;
}
#endif

VexThread* vex_thread_create(ThreadFunc func, void* arg) {
    VexThread* thread = (VexThread*)malloc(sizeof(VexThread));
    thread->func = func;
    thread->arg = arg;
    thread->result = NULL;
    thread->completed = false;

#ifdef _WIN32
    thread->handle = CreateThread(NULL, 0, win32_thread_proc, thread, 0, NULL);
#else
    pthread_t* tid = (pthread_t*)malloc(sizeof(pthread_t));
    pthread_create(tid, NULL, posix_thread_proc, thread);
    thread->handle = tid;
#endif
    return thread;
}

void vex_thread_join(VexThread* thread) {
    if (!thread || !thread->handle) return;
#ifdef _WIN32
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
#else
    pthread_t* tid = (pthread_t*)thread->handle;
    pthread_join(*tid, NULL);
    free(tid);
#endif
    thread->handle = NULL;
}

void vex_thread_free(VexThread* thread) {
    if (thread) {
        if (thread->handle) vex_thread_join(thread);
        free(thread);
    }
}

/* M:N Work-Stealing Fiber Scheduler */
static void queue_push(WorkStealingQueue* q, Fiber* f) {
    q->tasks[q->tail % WORK_STEAL_CAPACITY] = f;
    q->tail++;
}

static Fiber* queue_pop(WorkStealingQueue* q) {
    if (q->head >= q->tail) return NULL;
    Fiber* f = q->tasks[q->head % WORK_STEAL_CAPACITY];
    q->head++;
    return f;
}

static void* worker_loop(void* arg) {
    WorkStealingQueue* q = (WorkStealingQueue*)arg;
    while (1) {
        Fiber* f = queue_pop(q);
        if (f) {
            f->state = FIBER_RUNNING;
            f->result = f->func(f->arg);
            f->state = FIBER_COMPLETED;
        } else {
            break;
        }
    }
    return NULL;
}

void scheduler_init(FiberScheduler* sched, int num_workers) {
    sched->worker_count = num_workers > 0 ? num_workers : 4;
    sched->workers = (VexThread**)malloc(sizeof(VexThread*) * sched->worker_count);
    sched->queues = (WorkStealingQueue*)malloc(sizeof(WorkStealingQueue) * sched->worker_count);
    sched->running = true;

    for (int i = 0; i < sched->worker_count; i++) {
        sched->queues[i].head = 0;
        sched->queues[i].tail = 0;
        sched->workers[i] = vex_thread_create(worker_loop, &sched->queues[i]);
    }
}

Fiber* scheduler_spawn(FiberScheduler* sched, ThreadFunc func, void* arg) {
    Fiber* f = (Fiber*)malloc(sizeof(Fiber));
    static int fiber_id_counter = 1;
    f->id = fiber_id_counter++;
    f->state = FIBER_READY;
    f->func = func;
    f->arg = arg;
    f->result = NULL;
    f->stack_size = 64 * 1024;
    f->stack = (uint8_t*)malloc(f->stack_size);

    int target_worker = f->id % sched->worker_count;
    queue_push(&sched->queues[target_worker], f);
    return f;
}

void scheduler_yield(void) {
#ifdef _WIN32
    Sleep(0);
#else
    sched_yield();
#endif
}

void scheduler_shutdown(FiberScheduler* sched) {
    if (!sched->running) return;
    for (int i = 0; i < sched->worker_count; i++) {
        vex_thread_join(sched->workers[i]);
        vex_thread_free(sched->workers[i]);
    }
    free(sched->workers);
    free(sched->queues);
    sched->running = false;
}
