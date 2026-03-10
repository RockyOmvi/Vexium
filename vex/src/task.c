/* ═══════════════════════════════════════════════════════════════════════════════
 *  VEXIUM TASK SYSTEM - Implementation
 *  Async/await, spawn, and task management
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define _POSIX_C_SOURCE 200809L  /* For strdup on POSIX systems */

#include "task.h"
#include "vm.h"
#include <stdlib.h>
#include <string.h>

/* strdup is not C99, provide fallback for Windows */
#ifdef _WIN32
static char* task_strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* copy = (char*)malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}
#define strdup task_strdup
#endif

/* Thread-local storage - use static variable for single-threaded fallback */
#if defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#elif defined(__MINGW32__)
/* MinGW doesn't fully support thread-local, use pthread for compatibility */
#define THREAD_LOCAL
#else
#define THREAD_LOCAL
#endif

static THREAD_LOCAL Task* g_current_task = NULL;
#define get_current_task() g_current_task
#define set_current_task(t) g_current_task = (t)

/* Global task manager */
static TaskManager g_task_manager;

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TASK MANAGER
 * ═══════════════════════════════════════════════════════════════════════════════ */

void task_system_init(void) {
    memset(&g_task_manager, 0, sizeof(TaskManager));
    mutex_init(&g_task_manager.queue_lock);
    cond_init(&g_task_manager.work_available);
    g_task_manager.next_task_id = 1;
}

void task_system_shutdown(void) {
    g_task_manager.shutdown = true;
    cond_signal(&g_task_manager.work_available);
    
    /* Wait for all tasks to complete */
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (g_task_manager.workers[i]) {
            WaitForSingleObject(g_task_manager.workers[i], INFINITE);
            CloseHandle(g_task_manager.workers[i]);
        }
    }
    
    /* Clean up all tasks */
    Task* task = g_task_manager.all_tasks;
    while (task) {
        Task* next = task->next;
        task_destroy(task);
        task = next;
    }
    
    mutex_destroy(&g_task_manager.queue_lock);
    cond_destroy(&g_task_manager.work_available);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TASK CREATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

Task* task_create(ObjClosure* closure, bool blocking) {
    Task* task = (Task*)calloc(1, sizeof(Task));
    if (!task) return NULL;
    
    task->id = g_task_manager.next_task_id++;
    task->state = TASK_PENDING;
    task->closure = closure;
    task->is_blocking = blocking;
    
    mutex_init(&task->lock);
    cond_init(&task->cond);
    
    /* Create VM for this task - vm_new allocates and initializes */
    task->vm = vm_new();
    if (!task->vm) {
        free(task);
        return NULL;
    }
    
    return task;
}

void task_destroy(Task* task) {
    if (!task) return;
    
    if (task->vm) {
        vm_free((VM*)task->vm);
    }
    
    if (task->error_msg) {
        free(task->error_msg);
    }
    
    mutex_destroy(&task->lock);
    cond_destroy(&task->cond);
    
    free(task);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TASK QUEUE MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void task_enqueue(Task* task) {
    mutex_lock(&g_task_manager.queue_lock);
    
    /* Add to ready queue */
    task->next = g_task_manager.ready_queue;
    if (g_task_manager.ready_queue) {
        g_task_manager.ready_queue->prev = task;
    }
    g_task_manager.ready_queue = task;
    
    /* Add to all tasks list */
    task->prev = NULL;
    Task* old_head = g_task_manager.all_tasks;
    g_task_manager.all_tasks = task;
    task->next = old_head;
    if (old_head) {
        old_head->prev = task;
    }
    
    g_task_manager.task_count++;
    
    cond_signal(&g_task_manager.work_available);
    mutex_unlock(&g_task_manager.queue_lock);
}

static Task* task_dequeue(void) {
    mutex_lock(&g_task_manager.queue_lock);
    
    Task* task = NULL;
    while (!task && !g_task_manager.shutdown) {
        /* Find first ready task */
        Task* current = g_task_manager.ready_queue;
        while (current) {
            if (current->state == TASK_PENDING || current->state == TASK_RUNNING) {
                task = current;
                break;
            }
            current = current->next;
        }
        
        if (!task) {
            cond_wait(&g_task_manager.work_available, &g_task_manager.queue_lock);
        }
    }
    
    mutex_unlock(&g_task_manager.queue_lock);
    return task;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TASK EXECUTION
 * ═══════════════════════════════════════════════════════════════════════════════ */

static DWORD WINAPI task_worker(LPVOID arg) {
    (void)arg;
    
    while (!g_task_manager.shutdown) {
        Task* task = task_dequeue();
        if (!task || g_task_manager.shutdown) break;
        
        /* Set as current task for this thread */
        set_current_task(task);
        
        mutex_lock(&task->lock);
        task->state = TASK_RUNNING;
        mutex_unlock(&task->lock);
        
        /* Execute the task - vm_run expects ObjFunction*, get it from closure */
        VMResult result = vm_run((VM*)task->vm, task->closure->function);
        
        mutex_lock(&task->lock);
        if (result == VM_OK) {
            task->state = TASK_COMPLETED;
            task->has_result = true;
            task->result = val_nothing_v();
        } else {
            task->state = TASK_FAILED;
            task->has_error = true;
            task->error_msg = strdup("Task execution failed");
            if (!task->error_msg) {
                task->error_msg = (char*)malloc(1);
                if (task->error_msg) task->error_msg[0] = '\0';
            }
        }
        
        /* Signal any waiters */
        cond_signal(&task->cond);
        mutex_unlock(&task->lock);
        
        set_current_task(NULL);
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════════════════════════ */

void task_spawn(Task* task) {
    if (!task) return;
    
    /* Start worker threads if not already running */
    static bool workers_started = false;
    if (!workers_started) {
        for (int i = 0; i < THREAD_POOL_SIZE; i++) {
            g_task_manager.workers[i] = CreateThread(
                NULL, 0, task_worker, NULL, 0, NULL);
        }
        workers_started = true;
    }
    
    task_enqueue(task);
}

Value task_await(Task* task) {
    if (!task) return val_nothing_v();
    
    mutex_lock(&task->lock);
    
    /* Wait for task to complete */
    while (task->state != TASK_COMPLETED && task->state != TASK_FAILED) {
        cond_wait(&task->cond, &task->lock);
    }
    
    Value result = task->has_result ? task->result : val_nothing_v();
    
    mutex_unlock(&task->lock);
    
    return result;
}

Task* task_current(void) {
    return get_current_task();
}

void task_yield(void) {
    /* In a cooperative system, this would yield to scheduler */
    /* For now, just sleep briefly */
    Sleep(0);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  CHANNEL IMPLEMENTATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

Channel* channel_create(void) {
    Channel* chan = (Channel*)calloc(1, sizeof(Channel));
    if (!chan) return NULL;
    
    chan->obj.type = OBJ_CHANNEL;
    chan->read_idx = 0;
    chan->write_idx = 0;
    chan->count = 0;
    chan->closed = false;
    
    mutex_init(&chan->lock);
    cond_init(&chan->not_empty);
    cond_init(&chan->not_full);
    
    return chan;
}

void channel_close(Channel* chan) {
    if (!chan) return;
    
    mutex_lock(&chan->lock);
    chan->closed = true;
    cond_signal(&chan->not_empty);
    cond_signal(&chan->not_full);
    mutex_unlock(&chan->lock);
}

bool channel_send(Channel* chan, Value value) {
    if (!chan) return false;
    
    mutex_lock(&chan->lock);
    
    /* Wait until space available or closed */
    while (chan->count >= CHANNEL_CAPACITY && !chan->closed) {
        cond_wait(&chan->not_full, &chan->lock);
    }
    
    if (chan->closed) {
        mutex_unlock(&chan->lock);
        return false;
    }
    
    /* Add to buffer */
    chan->buffer[chan->write_idx] = value;
    chan->write_idx = (chan->write_idx + 1) % CHANNEL_CAPACITY;
    chan->count++;
    
    cond_signal(&chan->not_empty);
    mutex_unlock(&chan->lock);
    
    return true;
}

bool channel_receive(Channel* chan, Value* out) {
    if (!chan || !out) return false;
    
    mutex_lock(&chan->lock);
    
    /* Wait until data available or closed */
    while (chan->count == 0 && !chan->closed) {
        cond_wait(&chan->not_empty, &chan->lock);
    }
    
    if (chan->count == 0 && chan->closed) {
        mutex_unlock(&chan->lock);
        return false;
    }
    
    /* Remove from buffer */
    *out = chan->buffer[chan->read_idx];
    chan->read_idx = (chan->read_idx + 1) % CHANNEL_CAPACITY;
    chan->count--;
    
    cond_signal(&chan->not_full);
    mutex_unlock(&chan->lock);
    
    return true;
}
