#ifndef VEXIUM_THREAD_H
#define VEXIUM_THREAD_H

#include "common.h"

typedef void* (*ThreadFunc)(void* arg);

typedef struct {
    void* handle;
    ThreadFunc func;
    void* arg;
    void* result;
    bool completed;
} VexThread;

VexThread* vex_thread_create(ThreadFunc func, void* arg);
void vex_thread_join(VexThread* thread);
void vex_thread_free(VexThread* thread);

/* M:N Work-Stealing Fiber Task Scheduler */
typedef enum {
    FIBER_READY,
    FIBER_RUNNING,
    FIBER_SUSPENDED,
    FIBER_COMPLETED
} FiberState;

typedef struct Fiber Fiber;

struct Fiber {
    int id;
    FiberState state;
    ThreadFunc func;
    void* arg;
    void* result;
    uint8_t* stack;
    size_t stack_size;
};

#define WORK_STEAL_CAPACITY 256

typedef struct {
    Fiber* tasks[WORK_STEAL_CAPACITY];
    volatile int head;
    volatile int tail;
} WorkStealingQueue;

typedef struct {
    VexThread** workers;
    int worker_count;
    WorkStealingQueue* queues;
    bool running;
} FiberScheduler;

void scheduler_init(FiberScheduler* sched, int num_workers);
Fiber* scheduler_spawn(FiberScheduler* sched, ThreadFunc func, void* arg);
void scheduler_yield(void);
void scheduler_shutdown(FiberScheduler* sched);

#endif
