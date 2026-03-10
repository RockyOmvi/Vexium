# Vexium V2 System Architecture

**Purpose:** Comprehensive system architecture for the recovered V2 implementation  
**Target:** Production-ready, maintainable, extensible design

---

## 1. High-Level Architecture

### Current (Broken) Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         CURRENT (BROKEN) STATE                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Source Code                                                                │
│       │                                                                      │
│       ▼                                                                      │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌────────────┐  │
│   │    Lexer    │───>│   Parser    │───>│  Compiler   │───>│     VM     │  │
│   └─────────────┘    └─────────────┘    └─────────────┘    └────────────┘  │
│        │                    │                   │                 │         │
│        │                    │                   │                 │         │
│   ┌────▼────┐          ┌────▼────┐       ┌─────▼─────┐      ┌───▼────┐     │
│   │  Works  │          │  Works  │       │   Works   │      │ Works  │     │
│   └─────────┘          └─────────┘       └───────────┘      └────────┘     │
│                                                                              │
│   ┌───────────────────────────────────────────────────────────────────┐     │
│   │                    ISOLATED COMPONENTS (UNUSED)                    │     │
│   │  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌─────────────┐  │     │
│   │  │ TypeSystem │  │ TaskSystem │  │ Bytecode   │  │   AI/ML     │  │     │
│   │  │  (orphaned)│  │(C-only)    │  │ Cache      │  │ (missing)   │  │     │
│   │  └────────────┘  └────────────┘  │ (broken)   │  └─────────────┘  │     │
│   │                                   └────────────┘                   │     │
│   └───────────────────────────────────────────────────────────────────┘     │
│                                                                              │
│   PROBLEMS:                                                                 │
│   - Type system never called                                                │
│   - spawn/await not in language                                             │
│   - Cache corrupts strings                                                  │
│   - AI/ML doesn't exist                                                     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Target (Recovered) Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      TARGET (RECOVERED) ARCHITECTURE                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                         LANGUAGE LAYER                                   ││
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ││
│  │  │   let    │  │    fn    │  │  spawn   │  │  await   │  │  defer   │  ││
│  │  │   be     │  │   task   │  │ channel  │  │  select  │  │ unsafe   │  ││
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘  └──────────┘  ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                    │                                         │
│                                    ▼                                         │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                       COMPILER PIPELINE                                  ││
│  │                                                                          ││
│  │   Source ──> Tokens ──> AST ──> TypeCheck ──> Bytecode ──> Execute      ││
│  │              ──────    ────    ──────────    ────────      ───────      ││
│  │              Lexer    Parser   TypeSystem    Compiler        VM         ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                    │                                         │
│                                    ▼                                         │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                      RUNTIME SYSTEM                                      ││
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐ ││
│  │  │     VM      │  │     GC      │  │ Task Scheduler│  │ Channel Manager │ ││
│  │  │  (bytecode) │  │(mark-sweep) │  │ (thread pool) │  │  (select, etc)  │ ││
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────────┘ ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                    │                                         │
│                                    ▼                                         │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                     STANDARD LIBRARY                                     ││
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌────────┐ ││
│  │  │  math   │ │ string  │ │   fs    │ │  http   │ │ tensor  │ │ neural │ ││
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘ └────────┘ ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Component Architecture

### 2.1 Lexer Component

```
┌─────────────────────────────────────────────────────────────────┐
│                         LEXER                                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Input: Source code (char*)                                     │
│  Output: Token stream                                           │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Token Types                                            │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  Literals:     INT, FLOAT, STRING, TRUE, FALSE          │   │
│  │  Keywords:     LET, BE, FN, TASK, SPAWN, AWAIT, etc     │   │
│  │  Operators:    PLUS, MINUS, STAR, SLASH, EQ, NEQ, etc   │   │
│  │  Delimiters:   LPAREN, RPAREN, LBRACE, RBRACE, etc      │   │
│  │  Indentation:  INDENT, DEDENT, NEWLINE                  │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Multi-Word Keywords                                    │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  "give back"  -> TOKEN_GIVE_BACK                        │   │
│  │  "is at least" -> TOKEN_IS_AT_LEAST                     │   │
│  │  "is at most"  -> TOKEN_IS_AT_MOST                      │   │
│  │  "is greater than" -> TOKEN_IS_GREATER                  │   │
│  │  "is less than"    -> TOKEN_IS_LESS                     │   │
│  │  "not exists in"   -> TOKEN_NOT_EXISTS                  │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Parser Component

```
┌─────────────────────────────────────────────────────────────────┐
│                         PARSER                                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Input: Token stream                                            │
│  Output: Abstract Syntax Tree (AST)                             │
│  Algorithm: Recursive Descent with Pratt parsing                │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  AST Node Types                                         │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  Expressions:                                           │   │
│  │    - NODE_INT_LITERAL, NODE_FLOAT_LITERAL              │   │
│  │    - NODE_STRING_LITERAL, NODE_BOOL_LITERAL            │   │
│  │    - NODE_IDENTIFIER, NODE_BINARY_EXPR                 │   │
│  │    - NODE_CALL, NODE_LAMBDA, NODE_SPAWN (NEW)          │   │
│  │    - NODE_AWAIT (NEW), NODE_CHANNEL_RECV (NEW)         │   │
│  │                                                         │   │
│  │  Statements:                                            │   │
│  │    - NODE_LET, NODE_CONST                              │   │
│  │    - NODE_FN, NODE_TASK                                │   │
│  │    - NODE_IF, NODE_WHILE, NODE_FOR                     │   │
│  │    - NODE_ATTEMPT, NODE_THROW                          │   │
│  │    - NODE_DEFER (NEW), NODE_SELECT (NEW)               │   │
│  │    - NODE_CHANNEL_SEND (NEW)                           │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Parser Functions                                       │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  parse_expression()      - Pratt parser entry          │   │
│  │  parse_statement()       - Statement dispatcher        │   │
│  │  parse_fn()              - Function definition         │   │
│  │  parse_task()            - Task definition             │   │
│  │  parse_spawn() (NEW)     - Spawn expression            │   │
│  │  parse_await() (NEW)     - Await expression            │   │
│  │  parse_defer() (NEW)     - Defer statement             │   │
│  │  parse_select() (NEW)    - Select statement            │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.3 Type System Component

```
┌─────────────────────────────────────────────────────────────────┐
│                      TYPE SYSTEM                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Input: AST                                                     │
│  Output: Typed AST + Type Errors                                │
│  Algorithm: Hindley-Milner with Constraint Solving              │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Type Representation                                    │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  TYPE_INT, TYPE_FLOAT, TYPE_BOOL, TYPE_STRING          │   │
│  │  TYPE_ARRAY of Type                                    │   │
│  │  TYPE_MAP of (Type, Type)                              │   │
│  │  TYPE_FUNCTION of (Type[], Type)                       │   │
│  │  TYPE_GENERIC of (name, constraints)                   │   │
│  │  TYPE_VARIABLE of (id, constraints)                    │   │
│  │  TYPE_TENSOR of (shape, dtype, device)                 │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Type Inference Process                                 │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  1. traverse AST                                        │   │
│  │  2. generate constraints                                │   │
│  │  3. unify constraints                                   │   │
│  │  4. apply substitutions                                 │   │
│  │  5. report errors                                       │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Integration Points                                     │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  Compiler: compile() calls type_check()                 │   │
│  │  CLI: vex check uses type_check()                       │   │
│  │  Error Reporting: Line/column info for errors           │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.4 Compiler Component

```
┌─────────────────────────────────────────────────────────────────┐
│                        COMPILER                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Input: AST                                                     │
│  Output: ObjFunction (bytecode + metadata)                      │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Compilation Pipeline                                   │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │                                                         │   │
│  │   AST ──> Type Check ──> Optimize ──> Bytecode         │   │
│  │          (optional)    (optional)                      │   │
│  │                                                         │   │
│  │   New: Type check is now MANDATORY in strict mode      │   │
│  │                                                         │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Bytecode Opcodes                                       │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  Constants: OP_CONST, OP_TRUE, OP_FALSE, OP_NOTHING    │   │
│  │  Arithmetic: OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD    │   │
│  │  Comparison: OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LTE       │   │
│  │  Variables: OP_GET_LOCAL, OP_SET_LOCAL, OP_GET_GLOBAL  │   │
│  │  Functions: OP_CALL, OP_RETURN, OP_CLOSURE             │   │
│  │  Control: OP_JMP, OP_JMP_IF_FALSE, OP_LOOP             │   │
│  │  Concurrency: OP_SPAWN, OP_AWAIT (NEW)                 │   │
│  │  Channels: OP_CHAN_CREATE, OP_CHAN_SEND, OP_CHAN_RECV  │   │
│  │  Defer: OP_DEFER, OP_EXECUTE_DEFERS (NEW)              │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Compilation Context                                    │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  Compiler {                                             │   │
│  │    ObjFunction* function;                               │   │
│  │    Local locals[256];                                   │   │
│  │    int local_count;                                     │   │
│  │    int scope_depth;                                     │   │
│  │    Upvalue upvalues[256];                               │   │
│  │    DeferStack defers;          // NEW                   │   │
│  │    TypeEnv* type_env;          // NEW                   │   │
│  │    bool strict_mode;           // NEW                   │   │
│  │  }                                                      │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.5 VM Component

```
┌─────────────────────────────────────────────────────────────────┐
│                      VIRTUAL MACHINE                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Input: ObjFunction (bytecode)                                  │
│  Output: Execution result (VM_OK or VM_ERROR)                   │
│  Model: Stack-based with call frames                            │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  VM State                                               │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  VM {                                                   │   │
│  │    Value stack[STACK_MAX];                              │   │
│  │    Value* stack_top;                                    │   │
│  │    CallFrame frames[FRAMES_MAX];                        │   │
│  │    int frame_count;                                     │   │
│  │    Obj* objects;              // GC linked list         │   │
│  │    TaskManager* task_mgr;     // NEW: Task system       │   │
│  │    Table globals;                                       │   │
│  │    Table strings;             // String interning       │   │
│  │  }                                                      │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Execution Loop                                         │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  for (;;) {                                             │   │
│  │    OpCode instruction = READ_BYTE();                    │   │
│  │    switch (instruction) {                               │   │
│  │      case OP_ADD: handle_add(vm); break;                │   │
│  │      case OP_SPAWN: handle_spawn(vm); break;      // NEW│   │
│  │      case OP_AWAIT: handle_await(vm); break;      // NEW│   │
│  │      // ...                                             │   │
│  │    }                                                    │   │
│  │  }                                                      │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Concurrency Support (NEW)                              │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  handle_spawn(vm):                                      │   │
│  │    - Pop closure from stack                             │   │
│  │    - Create Task                                        │   │
│  │    - Add to thread pool                                 │   │
│  │    - Push task handle                                   │   │
│  │                                                         │   │
│  │  handle_await(vm):                                      │   │
│  │    - Pop task from stack                                │   │
│  │    - Block until task completes                         │   │
│  │    - Push result                                        │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.6 Task System Component

```
┌─────────────────────────────────────────────────────────────────┐
│                      TASK SYSTEM                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Architecture                                           │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │                                                         │   │
│  │   Main Thread          Thread Pool (4 workers)          │   │
│  │   ───────────          ─────────────────────            │   │
│  │        │                       │                        │   │
│  │        │  spawn task          │                        │   │
│  │        │─────────────────────>│                        │   │
│  │        │                      │ execute                │   │
│  │        │ <────────────────────│                        │   │
│  │        │  await result        │                        │   │
│  │                                                         │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Components                                             │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  TaskManager {                                          │   │
│  │    TaskQueue ready_queue;                               │   │
│  │    Thread workers[THREAD_POOL_SIZE];                    │   │
│  │    Mutex queue_lock;                                    │   │
│  │    CondVar work_available;                              │   │
│  │  }                                                      │   │
│  │                                                         │   │
│  │  Task {                                                 │   │
│  │    int id;                                              │   │
│  │    TaskState state;                                     │   │
│  │    VM* vm;                // Per-task VM instance       │   │
│  │    ObjClosure* closure;   // Code to execute            │   │
│  │    Value result;                                        │   │
│  │    Channel* waiting_on;   // For channel operations     │   │
│  │  }                                                      │   │
│  │                                                         │   │
│  │  Channel {                                              │   │
│  │    Value buffer[CHANNEL_CAPACITY];                      │   │
│  │    int read_idx, write_idx, count;                      │   │
│  │    Task* send_waiters;                                  │   │
│  │    Task* recv_waiters;                                  │   │
│  │    Mutex lock;                                          │   │
│  │  }                                                      │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Language Integration                                   │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  task my_task():              # Task definition        │   │
│  │    # ... body                                           │   │
│  │                                                         │   │
│  │  let t be spawn my_task()     # Spawn task             │   │
│  │  let r be await t             # Wait for result        │   │
│  │                                                         │   │
│  │  let ch be create channel of str                      │   │
│  │  send "msg" to ch                                     │   │
│  │  let msg be receive from ch                           │   │
│  │                                                         │   │
│  │  select:                                                │   │
│  │    case x from ch1: handle(x)                         │   │
│  │    case y from ch2: handle(y)                         │   │
│  │    otherwise: default_action()                        │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Data Flow Diagrams

### 3.1 Compilation Flow

```
┌─────────┐     ┌─────────┐     ┌─────────┐     ┌───────────┐     ┌─────────┐
│  Source │────>│  Lexer  │────>│  Parser │────>│ TypeCheck │────>│  Byte   │
│  (.vxm) │     │         │     │         │     │  (NEW)    │     │  Code   │
└─────────┘     └─────────┘     └─────────┘     └───────────┘     └─────────┘
                                                                      │
                                                                      ▼
                                                               ┌─────────────┐
                                                               │   Cache     │
                                                               │  (.vxmc)    │
                                                               └─────────────┘
```

### 3.2 Execution Flow

```
┌─────────────┐     ┌───────────┐     ┌─────────────┐     ┌─────────────┐
│   Bytecode  │────>│    VM     │────>│    Task     │────>│   Thread    │
│             │     │           │     │   System    │     │    Pool     │
└─────────────┘     └───────────┘     └─────────────┘     └─────────────┘
                           │                                    │
                           ▼                                    ▼
                    ┌─────────────┐                      ┌─────────────┐
                    │      GC     │                      │   Channels  │
                    └─────────────┘                      └─────────────┘
```

### 3.3 Package Manager Flow

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│ vex.toml │────>│ Resolver │────>│ Registry │────>│ Download │
│          │     │(SAT solver)    │   API    │     │ Packages │
└──────────┘     └──────────┘     └──────────┘     └──────────┘
                                                       │
                                                       ▼
┌──────────┐     ┌──────────┐                   ┌──────────┐
│ vex.lock │<────│  Lock    │<──────────────────│  Cache   │
│          │     │ Generator│                   │  (local) │
└──────────┘     └──────────┘                   └──────────┘
```

---

## 4. Module Dependencies

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                         MODULE DEPENDENCY GRAPH                               │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│  ┌──────────┐                                                                 │
│  │  main.c  │                                                                 │
│  └────┬─────┘                                                                 │
│       │                                                                       │
│       ▼                                                                       │
│  ┌────────────────────────────────────────────────────────────────────┐      │
│  │                        Core Pipeline                                │      │
│  │  ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐│      │
│  │  │ lexer  │───>│ parser │───>│  type  │───>│compiler│───>│   vm   ││      │
│  │  └────────┘    └────────┘    │ system │    └────────┘    └────────┘│      │
│  │                              └────────┘                             │      │
│  └────────────────────────────────────────────────────────────────────┘      │
│       │                              │                                        │
│       │                              │                                        │
│       ▼                              ▼                                        │
│  ┌──────────────────┐         ┌──────────────────┐                           │
│  │  Runtime Support │         │  Concurrency     │                           │
│  │  ┌────────────┐  │         │  ┌────────────┐  │                           │
│  │  │     gc     │  │         │  │    task    │  │                           │
│  │  └────────────┘  │         │  └────────────┘  │                           │
│  │  ┌────────────┐  │         │  ┌────────────┐  │                           │
│  │  │bytecode_cache│ │         │  │  channel   │  │                           │
│  │  └────────────┘  │         │  └────────────┘  │                           │
│  └──────────────────┘         └──────────────────┘                           │
│                                                                               │
│  ┌────────────────────────────────────────────────────────────────────┐      │
│  │                      Standard Library                               │      │
│  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐│      │
│  │  │  math  │ │ string │ │   fs   │ │  http  │ │ tensor │ │ neural ││      │
│  │  └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ └────────┘│      │
│  └────────────────────────────────────────────────────────────────────┘      │
│                                                                               │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Extension Points

### 5.1 Adding New Language Features

```c
// 1. Add token (token.h)
TOKEN_MYFEATURE,

// 2. Add lexer keyword (lexer.c)
{"myfeature", 9, TOKEN_MYFEATURE},

// 3. Add AST node (ast.h)
typedef struct {
    ASTNode* body;
} MyFeatureStmt;

// 4. Add parser support (parser.c)
static ASTNode* parse_myfeature(Parser* p);

// 5. Add compiler support (compiler.c)
static void compile_myfeature(Compiler* c, ASTNode* node);

// 6. Add VM opcode (opcodes.h)
OP_MYFEATURE,

// 7. Add VM execution (vm.c)
case OP_MYFEATURE: handle_myfeature(vm); break;
```

### 5.2 Adding Native Modules

```c
// 1. Create module file (src/my_module.c)

static VexValue my_func(VexValue* args, int argc) {
    // Implementation
}

// 2. Register in stdlib.c
void register_my_module() {
    VexModule* mod = vex_module_new("my_module");
    vex_module_add_fn(mod, "my_func", my_func, 2);
    vex_register_module(mod);
}
```

---

## 6. Configuration

### 6.1 vex.toml (Project Configuration)

```toml
[package]
name = "my_project"
version = "1.0.0"
edition = "2026"

[dependencies]
requests = "^2.0"
numpy = ">=1.24, <2.0"

[dependencies.ai]
version = "^1.0"
features = ["gpu", "distributed"]

[profile.release]
opt_level = 3
lto = true

[profile.dev]
opt_level = 0
debug = true
```

### 6.2 vex.lock (Dependency Lock)

```toml
# This file is automatically generated.
# Do not edit manually.
version = 1

[[package]]
name = "my_project"
version = "1.0.0"
dependencies = ["requests 2.31.0", "numpy 1.26.0"]

[[package]]
name = "requests"
version = "2.31.0"
source = "registry+https://vex.pm"
checksum = "sha256:abc123..."

[[package]]
name = "numpy"
version = "1.26.0"
source = "registry+https://vex.pm"
checksum = "sha256:def456..."
```

---

## 7. Error Handling Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    ERROR HANDLING                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Error Types:                                                   │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐            │
│  │  LexError    │ │  ParseError  │ │  TypeError   │            │
│  │  (token)     │ │  (syntax)    │ │  (semantic)  │            │
│  └──────────────┘ └──────────────┘ └──────────────┘            │
│                                                                 │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐            │
│  │ CompileError │ │  RuntimeError│ │  ImportError │            │
│  │  (codegen)   │ │  (execution) │ │  (modules)   │            │
│  └──────────────┘ └──────────────┘ └──────────────┘            │
│                                                                 │
│  Error Format:                                                  │
│  ─────────────────────────────────────────────────────────────  │
│  error[E0001]: Type mismatch                                    │
│    --> src/main.vxm:42:15                                       │
│     │                                                            │
│  42 │ let x: int = "hello"                                      │
│     │               ^^^^^^^ expected `int`, found `string`      │
│     │                                                            │
│     = help: Try converting: let x: int = "hello".to_int()       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 8. Performance Architecture

### 8.1 Optimization Pipeline

```
Source Code
     │
     ▼
┌─────────────────┐
│  Lex & Parse    │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Type Checking  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐     ┌─────────────────┐
│  Optimizations  │────>│ Constant Fold   │
└────────┬────────┘     │ Dead Code Elim  │
         │              │ Inline Caches   │
         │              └─────────────────┘
         ▼
┌─────────────────┐
│  Bytecode Gen   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  VM Execution   │
│  (or JIT)       │
└─────────────────┘
```

### 8.2 JIT Compilation (Future)

```
Hot Loop Detected
       │
       ▼
┌─────────────────┐
│ Trace Recording │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  IR Generation  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Optimization   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Machine Code   │
└─────────────────┘
```

---

## 9. Summary

This architecture provides:

1. **Correctness:** Type checking integrated, bugs fixed
2. **Concurrency:** Full spawn/await/channel support
3. **Tooling:** Package manager, formatter, test runner
4. **Extensibility:** Clear extension points
5. **Performance:** Optimization pipeline, future JIT ready
6. **Maintainability:** Clean module boundaries

**Key Innovation:** Integration-first approach ensures every component is actually usable from the language, not just existing in C code.

---

*Architecture Version: 2.0-Recovery*  
*Designed for: Production deployment*  
*Target completion: 16-20 weeks*
