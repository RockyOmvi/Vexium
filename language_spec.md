# 🔥 Language Specification: **Vex**

> *"Code that reads like English. Runs like C. Thinks like AI."*
> **Python-style · OS-first · AI-native · English-like**

---

## 1. Language Philosophy

| Principle | Design |
|-----------|--------|
| **Python-style** | Indentation-based blocks. No curly braces. No semicolons. Clean. |
| **English-like** | `let name be "Alice"`, `display "Hello"`, `give back result` |
| **OS-first** | `use system` — direct memory, ports, interrupts, kernel-level control |
| **AI-native** | `use ai` — tensors, neural nets, training loops, LLM ops built in |
| **Fast** | Bytecode VM with NaN-boxing. Targets Lua-level speed. |
| **Safe by default** | Memory-safe unless you say `unsafe:` for OS hardware work |

---

## 2. Syntax Rules

### 2.1 Indentation = Structure

```vex
# Blocks are defined by indentation (4 spaces), just like Python.
# No curly braces. No semicolons. No noise.

fn greet(name: str) -> str:
    let message be "Hello, {name}!"
    give back message

if age is greater than 18:
    display "Welcome!"
elif age is 18:
    display "Just made it!"
else:
    display "Too young"
```

### 2.2 Core Syntax Summary

```
╔══════════════════════════════════════════════════════════════════════╗
║  RULE                         EXAMPLE                              ║
╠══════════════════════════════════════════════════════════════════════╣
║  Blocks use colon + indent    fn add(a, b):                        ║
║                                   give back a + b                  ║
║                                                                    ║
║  Comments use #               # this is a comment                  ║
║                                                                    ║
║  Strings use " or '           "hello"  'world'  """multiline"""    ║
║                                                                    ║
║  No semicolons                let x be 5                           ║
║                                                                    ║
║  String interpolation         "Hello, {name}! You are {age}."      ║
║                                                                    ║
║  Line continuation            let total be first_value +           ║
║                                   second_value + third_value       ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 3. Complete Keyword List (35 Keywords)

### Core

| Keyword | Purpose | Example |
|---------|---------|---------|
| `let` | Mutable variable | `let x be 10` |
| `be` | Assignment (English) | `let name be "Vex"` |
| `const` | Immutable constant | `const PI be 3.14159` |
| `fn` | Function definition | `fn add(a, b):` |
| `give back` | Return value | `give back x + y` |
| `display` | Print to screen | `display "Hello!"` |
| `pass` | Empty block placeholder | `fn todo(): pass` |

### Control Flow

| Keyword | Purpose | Example |
|---------|---------|---------|
| `if` | Conditional | `if ready:` |
| `elif` | Else-if | `elif x is 0:` |
| `else` | Else branch | `else:` |
| `while` | While loop | `while alive:` |
| `for` | For-each | `for item in list:` |
| `each` | Optional emphasis | `for each item in list:` |
| `repeat` | Repeat N times | `repeat 10 times:` |
| `times` | Loop count | `repeat n times:` |
| `in` | Membership/iteration | `for x in 1 to 10:` |
| `to` | Range end | `for i in 0 to 100:` |
| `by` | Range step | `for i in 0 to 100 by 2:` |
| `break` | Exit loop | `break` |
| `skip` | Next iteration | `skip` |
| `match` | Pattern matching | `match value:` |

### Logic & Values

| Keyword | Purpose | Example |
|---------|---------|---------|
| `and` | Logical AND | `if a and b:` |
| `or` | Logical OR | `if a or b:` |
| `not` | Logical NOT | `if not done:` |
| `is` | Comparison | `if x is 5:` |
| `true` | Boolean true | `let flag be true` |
| `false` | Boolean false | `let flag be false` |
| `nothing` | Null value | `if x is nothing:` |

### Structure & Modules

| Keyword | Purpose | Example |
|---------|---------|---------|
| `use` | Import module | `use ai` |
| `from` | Selective import | `from math use sqrt` |
| `struct` | Data type | `struct Point:` |
| `has` | Struct field | `has x: int` |
| `can` | Struct method | `can move(self):` |
| `task` | Async function | `task fetch(url):` |
| `attempt` | Try block | `attempt:` |
| `otherwise` | Catch block | `otherwise err:` |
| `unsafe` | Low-level OS ops | `unsafe:` |

---

## 4. Types

| Type | Description | Example |
|------|-------------|---------|
| `int` | 64-bit integer | `42`, `0xFF`, `1_000_000` |
| `float` | 64-bit double | `3.14`, `1.0e10` |
| `bool` | Boolean | `true`, `false` |
| `str` | UTF-8 string | `"hello"` |
| `byte` | 8-bit unsigned (OS) | `0xFF` |
| `pointer` | Memory address (OS) | unsafe only |
| `nothing` | Absence of value | `nothing` |
| `[type]` | Array | `[1, 2, 3]` |
| `{k: v}` | Map/dictionary | `{"name": "Alice"}` |
| `(a, b)` | Tuple | `(10, 20)` |
| `type?` | Optional | `str?` = string or nothing |
| `tensor` | Multi-dim array (AI) | `tensor of shape [784, 256]` |

Type inference: types are **optional** — Vex infers them automatically.

```vex
let x be 42              # inferred: int
let name be "hello"       # inferred: str
let pi: float be 3.14     # explicit when you want
```

---

## 5. Operators

### English + Symbol (Both Work)

| English | Symbol | Meaning |
|---------|--------|---------|
| `is` | `==` | Equals |
| `is not` | `!=` | Not equals |
| `is greater than` | `>` | Greater |
| `is less than` | `<` | Less |
| `is at least` | `>=` | Greater or equal |
| `is at most` | `<=` | Less or equal |
| `plus` | `+` | Add |
| `minus` | `-` | Subtract |
| `times` (in expr) | `*` | Multiply |
| `divided by` | `/` | Divide |
| `mod` | `%` | Modulo |
| `to the power of` | `**` | Exponent |

---

## 6. Syntax by Domain

### 6.1 General Programming

```vex
# ── Variables ──
let name be "Vex"
let version be 1.0
const MAX_SIZE be 1024

# ── Functions ──
fn greet(person: str) -> str:
    give back "Hello, {person}!"

display greet("World")

# ── Loops ──
repeat 5 times:
    display "Hello!"

for i in 1 to 10:
    display "Count: {i}"

for each fruit in ["apple", "banana", "cherry"]:
    display "I like {fruit}"

# ── Conditionals ──
let score be 85

if score is greater than 90:
    display "Excellent!"
elif score is greater than 70:
    display "Good job"
else:
    display "Keep trying"

# ── Pattern Matching ──
match status_code:
    200 => display "OK"
    404 => display "Not Found"
    500 => display "Server Error"
    _   => display "Unknown"

# ── Collections ──
let heroes be ["Iron Man", "Thor", "Hulk"]
add "Spider-Man" to heroes
remove "Hulk" from heroes

let user be {
    "name": "Alice",
    "age": 30
}

if "name" exists in user:
    display user["name"]

# ── Higher-order functions ──
let nums be [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

let evens be nums.filter(fn(x): give back x mod 2 is 0)
let doubled be evens.map(fn(x): give back x * 2)
let total be doubled.reduce(0, fn(acc, x): give back acc + x)

display total   # 60
```

### 6.2 Structs — "HAS properties, CAN do things"

```vex
struct Dog:
    has name: str
    has age: int
    has breed: str

    can bark(self):
        display "{self.name} says: Woof!"

    can describe(self) -> str:
        give back "{self.name} is a {self.age}-year-old {self.breed}"

    can is_puppy(self) -> bool:
        give back self.age is less than 2

# Usage
let buddy be Dog("Buddy", 3, "Golden Retriever")
buddy.bark()                        # Buddy says: Woof!
display buddy.describe()            # Buddy is a 3-year-old Golden Retriever

if buddy.is_puppy():
    display "Still a puppy!"
else:
    display "All grown up!"
```

---

### 6.3 🖥️ OS Building (`use system`)

```vex
use system

# ══════════════════════════════════════
#  MEMORY MANAGEMENT
# ══════════════════════════════════════

let buffer be allocate 4096 bytes
write 0xFF to buffer at offset 0
let value be read from buffer at offset 0
free buffer

# ══════════════════════════════════════
#  HARDWARE I/O (requires unsafe)
# ══════════════════════════════════════

unsafe:
    write byte 0x20 to port 0x20         # send EOI to PIC
    let status be read byte from port 0x64

# ══════════════════════════════════════
#  INTERRUPT HANDLERS
# ══════════════════════════════════════

on interrupt 0x21:                        # keyboard IRQ
    let scancode be read byte from port 0x60
    let key be decode_scancode(scancode)
    add key to keyboard_buffer
    send end_of_interrupt to pic

# ══════════════════════════════════════
#  KERNEL DRIVER
# ══════════════════════════════════════

struct VGADriver:
    has buffer: pointer
    has width: int
    has height: int
    has cursor_x: int
    has cursor_y: int

    can write_char(self, ch: char, color: int):
        unsafe:
            let offset be (self.cursor_y * self.width + self.cursor_x) * 2
            write byte ch to self.buffer at offset
            write byte color to self.buffer at offset + 1
        self.cursor_x be self.cursor_x + 1

    can clear_screen(self):
        for i in 0 to self.width * self.height:
            self.write_char(' ', 0x07)
        self.cursor_x be 0
        self.cursor_y be 0

    can print_string(self, text: str, color: int):
        for each ch in text:
            if ch is '\n':
                self.cursor_x be 0
                self.cursor_y be self.cursor_y + 1
            else:
                self.write_char(ch, color)

# ══════════════════════════════════════
#  PROCESS MANAGEMENT
# ══════════════════════════════════════

struct Process:
    has pid: int
    has name: str
    has state: str
    has priority: int
    has memory: pointer

    can create(name: str, entry: fn, priority: int) -> Process:
        let mem be allocate 64 kilobytes
        let p be Process(next_pid(), name, "ready", priority, mem)
        give back p

let shell be Process.create("shell", shell_main, 5)
schedule shell
wait for shell to complete

# ══════════════════════════════════════
#  SIMPLE FILESYSTEM
# ══════════════════════════════════════

struct File:
    has name: str
    has data: [byte]
    has size: int

struct FileSystem:
    has files: {str: File}

    can create_file(self, name: str, content: str):
        let f be File(name, content.to_bytes(), the length of content)
        self.files[name] be f

    can read_file(self, name: str) -> str?:
        if name exists in self.files:
            give back str.from_bytes(self.files[name].data)
        give back nothing

    can list_files(self) -> [str]:
        give back self.files.keys()

# Usage in kernel
let fs be FileSystem({})
fs.create_file("hello.txt", "Hello from VexOS!")
fs.create_file("config.sys", "kernel=vexkernel")

for each filename in fs.list_files():
    display "File: {filename}"
```

---

### 6.4 🤖 AI/ML/LLM Building (`use ai`)

```vex
use ai

# ══════════════════════════════════════
#  TENSORS (NumPy-like but English)
# ══════════════════════════════════════

let weights be tensor of shape [784, 256] filled with random values
let bias be tensor of shape [256] filled with 0.0
let inputs be tensor from [1.0, 2.0, 3.0, 4.0]

# Operations
let output be inputs dot weights plus bias
let activated be apply relu to output
let probabilities be apply softmax to activated

# ══════════════════════════════════════
#  NEURAL NETWORK (Layer-by-Layer)
# ══════════════════════════════════════

let model be neural network:
    layer dense with 784 inputs, 256 outputs, activation is relu
    layer dropout with rate 0.3
    layer dense with 256 inputs, 128 outputs, activation is relu
    layer dense with 128 inputs, 10 outputs, activation is softmax

# ══════════════════════════════════════
#  TRAINING
# ══════════════════════════════════════

let data be load dataset from "mnist/train.csv"
let labels be load labels from "mnist/labels.csv"

train model on data:
    labels are labels
    loss function is cross_entropy
    optimizer is adam with learning rate 0.001
    epochs is 10
    batch size is 32
    display progress every 100 steps

# Evaluate
let accuracy be evaluate model on test_data
display "Accuracy: {accuracy}%"

# Save / Load
save model to "digit_classifier.vex.model"
let loaded_model be load model from "digit_classifier.vex.model"

# ══════════════════════════════════════
#  INFERENCE (Prediction)
# ══════════════════════════════════════

let image be load image from "digit.png"
let prediction be model predict on image
display "Predicted digit: {prediction.argmax()}"

# ══════════════════════════════════════
#  BUILD YOUR OWN LLM
# ══════════════════════════════════════

# Step 1: Tokenizer
struct Tokenizer:
    has vocab: {str: int}
    has reverse: {int: str}
    has next_id: int

    can create() -> Tokenizer:
        give back Tokenizer({}, {}, 0)

    can learn(self, text: str):
        for each word in text.split(" "):
            if word not exists in self.vocab:
                self.vocab[word] be self.next_id
                self.reverse[self.next_id] be word
                self.next_id be self.next_id + 1

    can encode(self, text: str) -> [int]:
        let tokens be []
        for each word in text.split(" "):
            if word exists in self.vocab:
                add self.vocab[word] to tokens
        give back tokens

    can decode(self, tokens: [int]) -> str:
        let words be []
        for each token in tokens:
            add self.reverse[token] to words
        give back words.join(" ")

# Step 2: Transformer Architecture
let transformer be neural network:
    layer embedding with vocab_size 50000, dimension 512
    repeat 6 times:
        layer self_attention with 8 heads, dimension 512
        layer feed_forward with dimension 2048
    layer linear with dimension 50000

# Step 3: Train on text corpus
let corpus be load text from "corpus.txt"
let tokenizer be Tokenizer.create()
tokenizer.learn(corpus)
let tokens be tokenizer.encode(corpus)

train transformer on tokens:
    method is next_token_prediction
    optimizer is adam with learning rate 0.0001
    epochs is 100
    batch size is 64
    context window is 256
    display loss every 10 steps

# Step 4: Generate text
fn generate(prompt: str, max_tokens: int) -> str:
    let input_tokens be tokenizer.encode(prompt)
    repeat max_tokens times:
        let logits be transformer predict on input_tokens
        let next_token be sample from logits with temperature 0.7
        add next_token to input_tokens
        if next_token is tokenizer.eos_token:
            break
    give back tokenizer.decode(input_tokens)

display generate("Once upon a time", 100)

# ══════════════════════════════════════
#  DATA PIPELINE
# ══════════════════════════════════════

let data be load csv from "dataset.csv"
let clean be data:
    .filter(fn(row): give back row["age"] is greater than 0)
    .sort_by("score", descending)
    .take(1000)

let training, testing be split clean into 80% and 20%
display "Training: {the length of training} samples"
display "Testing: {the length of testing} samples"

# ══════════════════════════════════════
#  CUSTOM LOSS FUNCTION
# ══════════════════════════════════════

fn my_loss(predicted: tensor, actual: tensor) -> float:
    let diff be predicted minus actual
    let squared be diff to the power of 2
    give back mean of squared

train model on data:
    loss function is my_loss
    optimizer is sgd with learning rate 0.01
    epochs is 50
```

---

### 6.5 🔧 System Design (`use network` + `use concurrent`)

```vex
use network
use concurrent

# ══════════════════════════════════════
#  HTTP SERVER
# ══════════════════════════════════════

let server be create http server on port 8080

server on route "/hello":
    give back json {"message": "Hello from Vex!"}

server on route "/users/{id}":
    let user be find_user(id)
    if user is nothing:
        give back error 404 with "User not found"
    give back json user

display "Server running on port 8080"
start server

# ══════════════════════════════════════
#  CONCURRENCY (Channels + Tasks)
# ══════════════════════════════════════

let messages be create channel of str

task producer(ch: channel):
    for i in 1 to 10:
        send "message {i}" through ch
    close ch

task consumer(ch: channel):
    for each msg in ch:
        display "Got: {msg}"

run producer(messages) and consumer(messages) concurrently

# ══════════════════════════════════════
#  ASYNC/AWAIT
# ══════════════════════════════════════

task fetch_page(url: str) -> str:
    let response be await request get from url
    give back response.body

task main():
    let pages be await all [
        fetch_page("https://example.com/1"),
        fetch_page("https://example.com/2"),
        fetch_page("https://example.com/3")
    ]
    for each i, page in enumerate(pages):
        display "Page {i}: {the length of page} chars"

# ══════════════════════════════════════
#  TCP SOCKET (OS-level)
# ══════════════════════════════════════

let socket be create tcp socket
bind socket to "0.0.0.0" port 9000
listen on socket

while true:
    let client be accept connection from socket
    task handle(client):
        let data be read from client
        display "Received: {data}"
        write "ACK" to client
        close client
```

---

## 7. Runtime Architecture

```
Source (.vex)
    │
    ▼
┌────────┐   ┌────────┐   ┌──────────────┐   ┌─────────────┐
│ Lexer  │──▶│ Parser │──▶│   Bytecode   │──▶│  Stack VM   │
│        │   │(indent │   │   Compiler   │   │ (NaN-boxed) │
│        │   │ aware) │   │              │   │             │
└────────┘   └────────┘   └──────────────┘   └─────────────┘
                                                    │
                                    ┌───────────────┤
                                    ▼               ▼
                              ┌──────────┐   ┌───────────┐
                              │ Tensor   │   │   FFI     │
                              │ Engine   │   │ (C/CUDA)  │
                              │ (AI ops) │   │ (OS ops)  │
                              └──────────┘   └───────────┘
```

### Speed Techniques

| Technique | Speed Gain | Description |
|-----------|-----------|-------------|
| **NaN-Boxing** | 2x | All values in 8 bytes, no heap alloc for primitives |
| **Computed Goto** | 20% | Direct-threaded VM dispatch |
| **Constant Folding** | ∞ | `2 + 3` compiled as `5` — zero runtime cost |
| **String Interning** | 10x | String equality = pointer comparison |
| **Bytecode Caching** | Instant startup | `.vexc` files skip recompilation |
| **Generational GC** | <5ms pauses | Young objects collected fast |
| **Tensor Backend** | GPU speed | AI ops routed to BLAS/CUDA via FFI |
| **Indent-aware parser** | Faster parse | No brace matching overhead |

---

## 8. Performance Targets

| Benchmark | Vex | Python | Lua | Go |
|-----------|-----|--------|-----|-----|
| Fibonacci(35) | <0.5s | ~3.5s | ~0.3s | ~0.05s |
| Loop 10M | <0.2s | ~1.5s | ~0.1s | ~0.02s |
| Matrix 1K×1K | <1.0s | ~0.5s (NumPy) | N/A | ~0.8s |
| Startup time | <50ms | ~30ms | ~5ms | ~2ms |
| Memory/value | 8 bytes | 28 bytes | 16 bytes | varies |

---

## 9. File Conventions

| Item | Convention | Example |
|------|----------|---------|
| Source files | `.vex` | `main.vex` |
| Compiled | `.vexc` | `main.vexc` |
| Models | `.vex.model` | `classifier.vex.model` |
| Variables | `snake_case` | `user_count` |
| Functions | `snake_case` | `train_model()` |
| Structs | `PascalCase` | `NeuralNetwork` |
| Constants | `UPPER_SNAKE` | `MAX_EPOCHS` |
| Comments | `#` | `# this is a comment` |

## 10. CLI

```bash
vex run main.vex         # Run a program
vex repl                 # Interactive REPL
vex compile main.vex     # Compile to .vexc
vex check main.vex       # Type-check only
vex fmt main.vex         # Auto-format
vex test tests/          # Run tests
vex train model.vex      # Run with AI/GPU support
vex --version            # Version info
```

---

> **Companion docs**: [mvptechdoc.md](./mvptechdoc.md) · [prd.md](./prd.md) · [system_design.md](./system_design.md)
