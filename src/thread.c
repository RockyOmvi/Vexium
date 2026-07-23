#include "thread.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
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

/* Lock-Free Atomic Queue Operations */
static inline int atomic_fetch_add(volatile int* ptr, int val) {
#ifdef _WIN32
    return InterlockedExchangeAdd((volatile LONG*)ptr, val);
#else
    return __atomic_fetch_add(ptr, val, __ATOMIC_SEQ_CST);
#endif
}

static inline bool atomic_compare_exchange(volatile int* ptr, int* expected, int desired) {
#ifdef _WIN32
    LONG old = InterlockedCompareExchange((volatile LONG*)ptr, desired, *expected);
    if (old == *expected) return true;
    *expected = old;
    return false;
#else
    return __atomic_compare_exchange_n(ptr, expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

static void queue_push(WorkStealingQueue* q, Fiber* f) {
    int t = q->tail;
    q->tasks[t % WORK_STEAL_CAPACITY] = f;
    atomic_fetch_add(&q->tail, 1);
}

static Fiber* queue_pop(WorkStealingQueue* q) {
    int b = q->tail - 1;
    atomic_fetch_add(&q->tail, -1);
    int h = q->head;
    if (h <= b) {
        Fiber* f = q->tasks[b % WORK_STEAL_CAPACITY];
        if (h != b) return f;
        int expected = h;
        if (!atomic_compare_exchange(&q->head, &expected, h + 1)) {
            f = NULL;
        }
        q->tail = h + 1;
        return f;
    } else {
        q->tail = h;
        return NULL;
    }
}

static Fiber* queue_steal(WorkStealingQueue* q) {
    int h = q->head;
    int t = q->tail;
    if (h < t) {
        Fiber* f = q->tasks[h % WORK_STEAL_CAPACITY];
        int expected = h;
        if (atomic_compare_exchange(&q->head, &expected, h + 1)) {
            return f;
        }
    }
    return NULL;
}

typedef struct {
    FiberScheduler* sched;
    int worker_id;
} WorkerArg;

static void* worker_loop(void* arg) {
    WorkerArg* warg = (WorkerArg*)arg;
    FiberScheduler* sched = warg->sched;
    int id = warg->worker_id;
    WorkStealingQueue* my_q = &sched->queues[id];

    while (sched->running) {
        Fiber* f = queue_pop(my_q);
        if (!f) {
            /* Try stealing from sibling worker queues */
            for (int i = 0; i < sched->worker_count; i++) {
                if (i != id) {
                    f = queue_steal(&sched->queues[i]);
                    if (f) break;
                }
            }
        }

        if (f) {
            f->state = FIBER_RUNNING;
            f->result = f->func(f->arg);
            f->state = FIBER_COMPLETED;
        } else {
            scheduler_yield();
        }
    }
    free(warg);
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
        WorkerArg* warg = (WorkerArg*)malloc(sizeof(WorkerArg));
        warg->sched = sched;
        warg->worker_id = i;
        sched->workers[i] = vex_thread_create(worker_loop, warg);
    }
}

Fiber* scheduler_spawn(FiberScheduler* sched, ThreadFunc func, void* arg) {
    Fiber* f = (Fiber*)malloc(sizeof(Fiber));
    static volatile int fiber_id_counter = 1;
    f->id = atomic_fetch_add(&fiber_id_counter, 1);
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
    Sleep(1);
#else
    sched_yield();
#endif
}

void scheduler_shutdown(FiberScheduler* sched) {
    if (!sched->running) return;
    sched->running = false;
    for (int i = 0; i < sched->worker_count; i++) {
        vex_thread_join(sched->workers[i]);
        vex_thread_free(sched->workers[i]);
    }
    free(sched->workers);
    free(sched->queues);
}
