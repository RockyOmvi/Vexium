# Concurrency & Async Implementation Guide

> **For developers implementing task system and async/await in Vex v2**

---

## Overview

Vex v2 provides **true parallelism** (no GIL) with:
- **Tasks** - lightweight async functions
- **Channels** - thread-safe message passing
- **Synchronization primitives** - mutexes, barriers, semaphores
- **No global interpreter lock** - all cores utilized

---

## Task Model

### Task Definition

A task is a lightweight coroutine that runs concurrently:

```vex
# Define an async function with 'async' keyword
async fn fetch_data(url: str) -> str {
    # Simulate async I/O
    let response = http_get(url)
    return response
}

# Or use 'task' keyword (shorthand for async fn)
task download_file(url: str, path: str) {
    let content = http_get(url)
    file_write(path, content)
    display "Downloaded: " + path
}
```

### Task Creation and Spawning

```vex
# Create a task (doesn't start yet)
let t = task {
    display "Running in task"
    sleep(1)
    display "Done"
}

# Spawn task (starts execution)
spawn t

# Spawn and immediately join (wait for completion)
spawn_join(t)

# Create and spawn in one call
spawn async {
    display "Quick task"
}
```

### Async/Await Syntax

```vex
# Mark function as async
async fn fetch_user(id: int) -> {name: str, email: str} {
    let response = await http_get("https://api.example.com/users/" + id)
    let user = parse_json(response)
    return user
}

# Use await to wait for task completion
async fn process_users(ids: [int]) {
    for id in ids {
        let user = await fetch_user(id)
        display user.name
    }
}
```

### Task Handles and Joining

```vex
# Spawn returns a handle
let task_handle = spawn async {
    sleep(2)
    return 42
}

# Wait for task and get result
let result = join(task_handle)  # Blocks until done or error
display result                  # 42

# Non-blocking check
if is_done(task_handle) {
    let result = get_result(task_handle)
    display result
} else {
    display "Still running..."
}

# Wait with timeout
attempt {
    let result = join_timeout(task_handle, 1.0)  # 1 second
} otherwise timeout_err {
    display "Task timed out!"
    cancel(task_handle)
}
```

---

## Channel Communication

### Channel Types

```vex
# Create unbuffered channel (sender blocks until receiver ready)
let ch: channel(int) = channel()

# Create buffered channel (can hold 10 values)
let ch_buffered: channel(str) = channel(capacity=10)

# Create select channel (multiple receivers)
let broadcast: broadcast_channel(str) = broadcast_channel()
```

### Sending and Receiving

```vex
async fn producer(ch: channel(int)) {
    for i in 1 to 10 {
        send(ch, i * 2)          # Send value (blocks if full)
        display "Sent: " + i
    }
    close(ch)                     # Signal no more values
}

async fn consumer(ch: channel(int)) {
    for value in ch {             # Range over channel (blocks at end)
        display "Received: " + value
    }
}

let channel = channel()

# Spawn both producer and consumer concurrently
spawn producer(channel)
spawn consumer(channel)
```

### Select Statement (Multiplexing)

```vex
async fn multiplex(ch1: channel(int), ch2: channel(str)) {
    loop {
        select {
            case ch1 -> value {
                # Received on ch1
                display "Int: " + value
            }
            case ch2 -> text {
                # Received on ch2
                display "Text: " + text
            }
            case timeout(1.0) {
                # No message for 1 second
                display "Timeout!"
            }
        }
    }
}
```

### Non-Blocking Send/Receive

```vex
let ch = channel(capacity=5)

# Try to send without blocking
if try_send(ch, 42) {
    display "Sent successfully"
} else {
    display "Channel full, not sent"
}

# Try to receive without blocking
attempt {
    let value = try_recv(ch)
    display "Received: " + value
} otherwise {
    display "No message available"
}
```

---

## Synchronization Primitives

### Mutex (Mutual Exclusion)

```vex
struct Counter {
    value: int
    lock: mutex
}

async fn increment(counter: mut Counter) {
    # Lock before accessing shared data
    lock(counter.lock)
    try {
        counter.value = counter.value + 1
    } finally {
        unlock(counter.lock)
    }
}

async fn read_counter(counter: Counter) {
    lock(counter.lock)
    try {
        let val = counter.value
        display "Counter: " + val
        return val
    } finally {
        unlock(counter.lock)
    }
}

# Usage
let counter = Counter(value=0, lock=mutex())

# Spawn multiple tasks incrementing same counter
for i in 1 to 10 {
    spawn async {
        increment(counter)
    }
}

sleep(1)  # Wait for all to complete
read_counter(counter)  # Safe to read
```

### Read-Write Lock

```vex
# Multiple readers, exclusive writer
let data: rwlock(str) = rwlock("initial")

async fn reader(name: str, data: rwlock(str)) {
    read_lock(data)
    try {
        let value = read_get(data)
        display name + " reads: " + value
    } finally {
        read_unlock(data)
    }
}

async fn writer(data: mut rwlock(str), new_val: str) {
    write_lock(data)
    try {
        write_set(data, new_val)
        display "Wrote: " + new_val
    } finally {
        write_unlock(data)
    }
}
```

### Semaphore (Counting Semaphore)

```vex
# Only 3 concurrent operations allowed
let sem = semaphore(3)

async fn limited_resource(id: int, sem: semaphore) {
    acquire(sem)           # Decrement count, block if 0
    try {
        display "Resource " + id + " in use"
        sleep(1)
        display "Resource " + id + " released"
    } finally {
        release(sem)       # Increment count
    }
}

# Spawn many tasks (only 3 run at once)
for i in 1 to 20 {
    spawn limited_resource(i, sem)
}
```

### Barrier (Synchronization Point)

```vex
# All 5 tasks must reach barrier before any continue
let barrier = barrier(5)

async fn worker(id: int, b: barrier) {
    display "Task " + id + " working..."
    sleep(random() * 2)     # Random work time
    display "Task " + id + " ready"
    
    # Everyone waits here
    wait(b)
    
    display "Task " + id + " continuing (all done)"
}

for i in 1 to 5 {
    spawn worker(i, barrier)
}
```

### Condition Variable

```vex
struct Queue {
    items: [any]
    lock: mutex
    not_empty: condition
}

async fn enqueue(q: mut Queue, item: any) {
    lock(q.lock)
    try {
        q.items.push(item)
        signal(q.not_empty)  # Wake waiting consumer
    } finally {
        unlock(q.lock)
    }
}

async fn dequeue(q: mut Queue) -> any {
    lock(q.lock)
    try {
        # Wait until something is available
        while q.items.len == 0 {
            wait(q.not_empty, q.lock)  # Releases lock while waiting
        }
        return q.items.pop_front()
    } finally {
        unlock(q.lock)
    }
}
```

---

## Patterns: Common Concurrency Patterns

### Worker Pool

```vex
struct WorkerPool {
    work_queue: channel(task)
    worker_count: int
}

fn create_pool(num_workers: int) -> WorkerPool {
    let pool = WorkerPool(
        work_queue = channel(capacity=100),
        worker_count = num_workers
    )
    
    # Spawn worker tasks
    for i in 1 to num_workers {
        spawn async {
            # Each worker processes tasks from queue
            for work in pool.work_queue {
                attempt {
                    work()  # Execute task
                } otherwise err {
                    display "Worker error: " + err.message
                }
            }
        }
    }
    
    return pool
}

fn submit_work(pool: WorkerPool, work: task) {
    send(pool.work_queue, work)
}

# Usage
let pool = create_pool(4)  # 4 workers

for i in 1 to 100 {
    submit_work(pool, task {
        # Process work item i
        display "Processing " + i
    })
}
```

### Fan-Out / Fan-In

```vex
# Distribute work to multiple tasks, collect results

async fn fork_join(work_items: [any]) -> [any] {
    # Fan-out: create result channel
    let results: channel(any) = channel(capacity=work_items.len)
    
    # Spawn worker for each item
    for item in work_items {
        spawn async {
            let result = process(item)
            send(results, result)
        }
    }
    
    # Fan-in: collect all results
    let collected = []
    for i in 1 to work_items.len {
        let result = recv(results)
        collected.push(result)
    }
    
    return collected
}
```

### Rate Limiting

```vex
async fn rate_limited_requests(urls: [str], rate: float) -> [str] {
    let results = []
    let start = time_now()
    
    for i in 1 to urls.len {
        # Ensure minimum time between requests
        let elapsed = time_now() - start
        let target_time = i / rate
        if elapsed < target_time {
            sleep(target_time - elapsed)
        }
        
        let result = await http_get(urls[i])
        results.push(result)
    }
    
    return results
}
```

### Timeout Pattern

```vex
async fn with_timeout(work: async, max_seconds: float) -> any {
    let result_channel = channel()
    let error_channel = channel()
    
    # Run work in background
    spawn async {
        attempt {
            let result = await work
            send(result_channel, result)
        } otherwise err {
            send(error_channel, err)
        }
    }
    
    # Wait for result or timeout
    select {
        case result_channel -> result {
            return result
        }
        case error_channel -> err {
            throw err
        }
        case timeout(max_seconds) {
            throw TimeoutError("Operation took longer than " + max_seconds + " seconds")
        }
    }
}

# Usage
let data = await with_timeout(fetch_data(), 5.0)  # 5 second timeout
```

---

## Data Types for Concurrency

### Task Handle

```c
typedef struct {
    int id;                       /* Unique task ID */
    TaskStatus status;            /* PENDING, RUNNING, DONE, FAILED */
    VexValue result;              /* Task return value */
    VexError* error;              /* If task failed */
    pthread_t thread;             /* OS thread handle */
    bool is_joined;               /* Has join() been called? */
} TaskHandle;

enum TaskStatus {
    TASK_PENDING,
    TASK_RUNNING,
    TASK_DONE,
    TASK_FAILED,
    TASK_CANCELLED
};
```

### Channel

```c
typedef struct {
    int capacity;                 /* 0 = unbuffered */
    VexValue* buffer;             /* Circular buffer */
    int send_idx, recv_idx;       /* Buffer positions */
    int pending_sends;            /* Tasks waiting to send */
    int pending_recvs;            /* Tasks waiting to receive */
    pthread_mutex_t lock;
    pthread_cond_t not_empty;     /* Signaled when data available */
    pthread_cond_t not_full;      /* Signaled when space available */
    bool is_closed;
} Channel;
```

---

## Memory Safety in Concurrent Code

### Own vs Borrow

```vex
# Own: exclusive ownership (mutable)
async fn modify(mut data: Data) {
    # Can safely modify
    data.value = 42
}

# Borrow: immutable reference (safe to share)
async fn read(data: Data) {
    # Can only read
    let val = data.value
}

# Send data to another task (moves ownership)
let data = Data(value = 10)
spawn async {
    # data is now owned by this task
    modify(data)
}
# Main can no longer access data (compile error)
```

### Shared State with Mutex

```vex
# Wrap shared data in mutex
struct SharedCounter {
    value: int
    lock: mutex
}

async fn increment(counter: mut SharedCounter) {
    lock(counter.lock)
    try {
        # Safe: only we can modify while locked
        counter.value = counter.value + 1
    } finally {
        unlock(counter.lock)
    }
}
```

---

## Implementation Checklist

- [ ] Task AST nodes and parsing
- [ ] `async`/`task` function definitions
- [ ] `await` expression
- [ ] `spawn` / `spawn_join` / task handles
- [ ] Thread pool for task execution
- [ ] Channel creation and send/recv
- [ ] Buffered channels
- [ ] Select statements for multiplexing
- [ ] Mutex / RWLock primitives
- [ ] Semaphore and Barrier
- [ ] Condition variables
- [ ] Task cancellation and cleanup
- [ ] Panic propagation from tasks
- [ ] Goroutine-style scoped concurrency
- [ ] Testing framework for concurrent code
- [ ] Deadlock detection (optional)
- [ ] Race condition detection (optional)

---

## Testing Concurrent Code

```vex
# tests/test_concurrency.vxm

# Test 1: Basic spawn and join
let task_handle = spawn async {
    sleep(0.1)
    return 42
}

let result = join(task_handle)
assert_equal(result, 42)

# Test 2: Channel communication
let ch = channel()

spawn async {
    send(ch, 1)
    send(ch, 2)
    send(ch, 3)
    close(ch)
}

let values = []
for val in ch {
    values.push(val)
}

assert_equal(values, [1, 2, 3])

# Test 3: Mutex protection
let counter = Counter(value=0, lock=mutex())

for i in 1 to 100 {
    spawn async {
        increment(counter)
    }
}

# Wait for all to complete
sleep(1)
assert_equal(counter.value, 100)  # No race condition

display "All concurrency tests passed!"
```

---

## Performance Notes

- **Spawning a task**: ~1-10 microseconds
- **Sending on channel**: ~100-500 nanoseconds (unbuffered)
- **Lock contention**: Scales well up to ~32 cores
- **Memory per task**: ~100-500 bytes (OS thread ~1-2 MB)

---

## Debugging Concurrent Code

### Enable Concurrency Debugging

```bash
$ vex run --debug-concurrency program.vxm
```

Outputs:
- Task creation/destruction
- Channel send/recv
- Lock acquisitions
- Deadlock detection
- Race condition warnings

### Typical Concurrency Bugs

```vex
# ❌ Deadlock: Tasks waiting for each other
let ch = channel()
spawn async {
    let x = recv(ch)      # Waits for message
}
# Main also waits for task
join(task_handle)
# No one sends to channel!

# ✅ Fix: Ensure someone sends
spawn async {
    send(ch, 42)
}

# ❌ Race condition: Unsynchronized access
let counter = 0
spawn async { counter = counter + 1 }
spawn async { counter = counter + 1 }
# Two threads modify counter simultaneously

# ✅ Fix: Use mutex
let counter = Counter(value=0, lock=mutex())
```

---

