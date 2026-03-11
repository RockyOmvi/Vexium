/* ═══════════════════════════════════════════════════════════════════════════════
 *  VEXIUM TASK SYSTEM - Concurrent Execution
 *  Async/await, spawn, and task management
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef VEXIUM_TASK_H
#define VEXIUM_TASK_H

#include "opcodes.h"
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TASK STATES
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    TASK_PENDING,      /* Created but not started */
    TASK_RUNNING,      /* Currently executing */
    TASK_WAITING,      /* Waiting for I/O or channel */
    TASK_COMPLETED,    /* Finished successfully */
    TASK_FAILED        /* Threw an exception */
} TaskState;

/* ═══════════════════════════════════════════════════════════════════════════════
 *  PLATFORM ABSTRACTION
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef _WIN32
typedef CRITICAL_SECTION Mutex;
typedef HANDLE CondVar;
typedef HANDLE Thread;
#define mutex_init(m) InitializeCriticalSection(m)
#define mutex_lock(m) EnterCriticalSection(m)
#define mutex_unlock(m) LeaveCriticalSection(m)
#define mutex_destroy(m) DeleteCriticalSection(m)
#define cond_init(c) (*(c) = CreateEvent(NULL, FALSE, FALSE, NULL))
#define cond_wait(c, m) do { mutex_unlock(m); WaitForSingleObject(*(c), INFINITE); mutex_lock(m); } while(0)
#define cond_signal(c) SetEvent(*(c))
#define cond_destroy(c) CloseHandle(*(c))
#else
typedef pthread_mutex_t Mutex;
typedef pthread_cond_t CondVar;
typedef pthread_t Thread;
#define mutex_init(m) pthread_mutex_init(m, NULL)
#define mutex_lock(m) pthread_mutex_lock(m)
#define mutex_unlock(m) pthread_mutex_unlock(m)
#define mutex_destroy(m) pthread_mutex_destroy(m)
#define cond_init(c) pthread_cond_init(c, NULL)
#define cond_wait(c, m) pthread_cond_wait(c, m)
#define cond_signal(c) pthread_cond_signal(c)
#define cond_destroy(c) pthread_cond_destroy(c)
#endif

/* Forward declarations */
struct Channel;

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TASK STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct Task {
    struct Task* next;           /* For task queue */
    struct Task* prev;
    
    int id;                      /* Unique task ID */
    TaskState state;
    
    /* VM context for this task */
    void* vm;                    /* Each task has its own VM instance (VM*) */
    ObjClosure* closure;         /* The function to execute */
    Value* args;                 /* Spawn-call arguments (copied values) */
    int arg_count;               /* Number of arguments */
    
    /* Result */
    Value result;
    bool has_result;
    
    /* Error handling */
    char* error_msg;
    bool has_error;
    
    /* Synchronization */
    Mutex lock;
    CondVar cond;
    
    /* Waiting state */
    struct Channel* waiting_on;  /* Channel we're waiting on */
    bool is_blocking;            /* If true, task blocks until completion */
} Task;

/* ═══════════════════════════════════════════════════════════════════════════════
 *  CHANNEL STRUCTURE (for task communication)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define CHANNEL_CAPACITY 16

typedef struct Channel {
    Obj obj;                     /* Object header for GC */
    
    Value buffer[CHANNEL_CAPACITY];
    int read_idx;
    int write_idx;
    int count;
    
    /* Waiting lists */
    Task* send_waiters;
    Task* recv_waiters;
    
    /* Synchronization */
    Mutex lock;
    CondVar not_empty;
    CondVar not_full;
    
    bool closed;
} Channel;

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TASK MANAGER
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define MAX_TASKS 1024
#define THREAD_POOL_SIZE 4

typedef struct TaskManager {
    /* Task queue */
    Task* ready_queue;           /* Tasks ready to run */
    Task* all_tasks;             /* All allocated tasks */
    int task_count;
    int next_task_id;
    
    /* Thread pool */
    Thread workers[THREAD_POOL_SIZE];
    bool shutdown;
    
    /* Synchronization */
    Mutex queue_lock;
    CondVar work_available;
    
    /* Main task */
    Task* main_task;
} TaskManager;

/* ═══════════════════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Initialize task system */
void task_system_init(void);
void task_system_shutdown(void);

/* Task creation */
Task* task_create(ObjClosure* closure, bool blocking, Value* args, int argc);
void task_destroy(Task* task);

/* Task lifecycle */
void task_spawn(Task* task);           /* Start task execution */
Value task_await(Task* task);          /* Wait for task completion */
void task_yield(void);                  /* Yield control */

/* Current task */
Task* task_current(void);

/* Channels */
Channel* channel_create(void);
void channel_close(Channel* chan);
bool channel_send(Channel* chan, Value value);
bool channel_receive(Channel* chan, Value* out);

#endif /* VEXIUM_TASK_H */
