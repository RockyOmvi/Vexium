# MVP Technical Documentation: Build Your Own Programming Language, Shell & Operating System

> **Source**: [codecrafters-io/build-your-own-x](https://github.com/codecrafters-io/build-your-own-x)
> **Last Updated**: 2026-03-03
> **Philosophy**: *"What I cannot create, I do not understand" — Richard Feynman*

---

## 1. Project Vision

Build a **complete software stack from scratch** — a custom **Programming Language**, a **Shell** that runs it, and an **Operating System** that hosts it all. Each layer builds on the previous, creating a fully self-contained computing environment.

```
┌─────────────────────────────────────────────┐
│           Custom Programming Language        │   ← Layer 3 (Top)
│   (Lexer → Parser → AST → Interpreter/      │
│    Compiler → Codegen → Runtime)             │
├─────────────────────────────────────────────┤
│              Custom Shell (CLI)              │   ← Layer 2 (Middle)
│   (REPL → Command Parser → Process Mgmt →   │
│    Built-in Commands → Piping/Redirection)   │
├─────────────────────────────────────────────┤
│           Custom Operating System            │   ← Layer 1 (Foundation)
│   (Bootloader → Kernel → Memory Mgmt →      │
│    Process Scheduler → Filesystem → Drivers) │
└─────────────────────────────────────────────┘
```

---

## 2. MVP Scope & Phased Roadmap

### Phase 1: Programming Language (Weeks 1–8)

**Goal**: Create a fully functional interpreted/compiled language with variables, functions, control flow, and a REPL.

#### 2.1 MVP Feature Set

| Feature | Priority | Description |
|---------|----------|-------------|
| Lexer/Tokenizer | P0 | Tokenize source code into tokens (keywords, operators, literals, identifiers) |
| Parser | P0 | Build AST (Abstract Syntax Tree) from token stream |
| Variables & Types | P0 | `int`, `float`, `string`, `bool`, `array` types with dynamic or static typing |
| Control Flow | P0 | `if/else`, `while`, `for` loops |
| Functions | P0 | First-class functions, closures, recursion |
| REPL | P0 | Interactive Read-Eval-Print Loop |
| Error Handling | P1 | Try/catch, meaningful error messages with line numbers |
| Standard Library | P1 | I/O, Math, String manipulation built-ins |
| Garbage Collector | P2 | Mark-and-sweep or reference counting memory management |
| Compiler Backend | P2 | Bytecode compilation or native codegen via LLVM |

#### 2.2 Key Tutorial Resources

| Tutorial | Language | Focus |
|----------|----------|-------|
| [Crafting Interpreters](http://www.craftinginterpreters.com/) | Java/C | **Best comprehensive guide** — tree-walk interpreter + bytecode VM |
| [Build Your Own Lisp](http://www.buildyourownlisp.com/) | C | Learn C by building a Lisp in ~1000 lines |
| [Let's Build A Simple Interpreter](https://ruslanspivak.com/lsbasi-part1/) | Python | Step-by-step Pascal interpreter |
| [The Super Tiny Compiler](https://github.com/jamiebuilds/the-super-tiny-compiler) | JavaScript | Minimal compiler in ~200 lines |
| [How to implement a programming language](http://lisperator.net/pltut/) | JavaScript | Full language implementation tutorial |
| [mal - Make a Lisp](https://github.com/kanaka/mal#mal---make-a-lisp) | Any | Implement Lisp in 80+ languages |
| [Kaleidoscope: LLVM Tutorial](https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/index.html) | C++ | LLVM-based compiler frontend |
| [A journey explaining how to build a compiler](https://github.com/DoctorWkt/acwj) | C | Complete compiler for a subset of C |
| [From Source Code To Machine Code](https://build-your-own.org/compiler/) | Python | Full compiler pipeline |
| [Write Yourself a Scheme in 48 Hours](https://en.wikibooks.org/wiki/Write_Yourself_a_Scheme_in_48_Hours) | Haskell | Scheme interpreter implementation |
| [A Python Interpreter Written in Python](http://aosabook.org/en/500L/a-python-interpreter-written-in-python.html) | Python | Bytecode interpreter internals |
| [C interpreter that interprets itself](https://github.com/lotabout/write-a-C-interpreter) | C | Self-hosting C interpreter |
| [Baby's First Garbage Collector](http://journal.stuffwithstuff.com/2013/12/08/babys-first-garbage-collector/) | C | GC fundamentals |
| [Learning Parser Combinators With Rust](https://bodil.lol/parser-combinators/) | Rust | Functional parsing techniques |
| [Build your own WebAssembly Compiler](https://blog.scottlogic.com/2019/05/17/webassembly-compiler.html) | TypeScript | WASM compilation target |

#### 2.3 Technical Architecture

```
Source Code ("hello.mypl")
        │
        ▼
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│    LEXER     │───▶│    PARSER    │───▶│   AST TREE   │
│  (Tokenizer) │    │  (Grammar)   │    │  (IR Nodes)  │
└──────────────┘    └──────────────┘    └──────────────┘
                                               │
                    ┌──────────────────────────┤
                    │                          │
                    ▼                          ▼
           ┌──────────────┐          ┌──────────────┐
           │ INTERPRETER  │          │   COMPILER   │
           │ (Tree-walk)  │          │  (Bytecode)  │
           └──────────────┘          └──────────────┘
                                           │
                                           ▼
                                    ┌──────────────┐
                                    │      VM      │
                                    │ (Stack-based) │
                                    └──────────────┘
```

#### 2.4 Sample Language Syntax (Target)

```
// Variables
let name: string = "MyLang"
let count: int = 42

// Functions
fn fibonacci(n: int) -> int {
    if n <= 1 {
        return n
    }
    return fibonacci(n - 1) + fibonacci(n - 2)
}

// Control flow
for i in range(0, 10) {
    print(fibonacci(i))
}

// First-class functions
let square = fn(x: int) -> int { return x * x }
print(square(5))  // 25
```

---

### Phase 2: Shell (Weeks 9–12)

**Goal**: Build a UNIX-style shell that can parse and execute commands, manage processes, and interface with the custom language.

#### 2.5 MVP Feature Set

| Feature | Priority | Description |
|---------|----------|-------------|
| Command Parsing | P0 | Tokenize and parse user input into commands + arguments |
| Process Execution | P0 | Fork/exec system calls, process creation |
| Built-in Commands | P0 | `cd`, `pwd`, `echo`, `exit`, `help`, `history` |
| PATH Resolution | P0 | Search PATH directories for executables |
| I/O Redirection | P1 | `>`, `>>`, `<` file redirection |
| Piping | P1 | `cmd1 | cmd2` output piping |
| Environment Variables | P1 | `export`, `$VAR` expansion |
| Signal Handling | P1 | Ctrl+C (SIGINT), Ctrl+Z (SIGTSTP) |
| Job Control | P2 | Background (`&`), `fg`, `bg`, `jobs` |
| Scripting | P2 | Execute `.sh` scripts, integrate custom language |
| Tab Completion | P2 | Auto-complete commands and file paths |
| Prompt Customization | P2 | Configurable PS1/prompt string |

#### 2.6 Key Tutorial Resources

| Tutorial | Language | Focus |
|----------|----------|-------|
| [Write a Shell in C](https://brennan.io/2015/01/16/write-a-shell-in-c/) | C | **Best starter** — clean, complete shell in ~300 lines |
| [Writing a UNIX Shell](https://indradhanush.github.io/blog/writing-a-unix-shell-part-1/) | C | Multi-part deep dive into UNIX shell internals |
| [Build Your Own Shell](https://github.com/tokenrove/build-your-own-shell) | C | Workshop-style with exercises |
| [Let's build a shell!](https://github.com/kamalmarhubi/shell-workshop) | C | Interactive workshop |
| [Write a shell in C](https://danishpraka.sh/posts/write-a-shell/) | C | Concise modern tutorial |
| [Writing a simple shell in Go](https://sj14.gitlab.io/post/2018-07-01-go-unix-shell/) | Go | Shell in Go with goroutines |
| [Build Your Own Shell using Rust](https://www.joshmcguigan.com/blog/build-your-own-shell-rust/) | Rust | Memory-safe shell implementation |

#### 2.7 Shell Architecture

```
User Input: "ls -la | grep .txt > output.txt"
        │
        ▼
┌──────────────┐
│    LEXER     │  → Tokens: [CMD:ls, ARG:-la, PIPE, CMD:grep, ARG:.txt, REDIR:>, ARG:output.txt]
└──────┬───────┘
       ▼
┌──────────────┐
│    PARSER    │  → Command Pipeline: [Cmd(ls, [-la]) | Cmd(grep, [.txt]) > File(output.txt)]
└──────┬───────┘
       ▼
┌──────────────┐
│   EXECUTOR   │  → fork() → exec() → pipe() → dup2() → waitpid()
└──────┬───────┘
       ▼
┌──────────────┐
│   BUILT-INS  │  → cd, pwd, exit, export, history, source
└──────────────┘
```

---

### Phase 3: Operating System (Weeks 13–24)

**Goal**: Build a minimal operating system with bootloader, kernel, memory management, process scheduling, and a basic filesystem.

#### 2.8 MVP Feature Set

| Feature | Priority | Description |
|---------|----------|-------------|
| Bootloader | P0 | x86 bootloader → protected/long mode → load kernel |
| GDT/IDT Setup | P0 | Global Descriptor Table, Interrupt Descriptor Table |
| Screen Output | P0 | VGA text mode driver, `print()` to screen |
| Keyboard Input | P0 | PS/2 keyboard driver, interrupt-based input |
| Memory Management | P0 | Physical + virtual memory, paging, heap allocator |
| Process Scheduler | P1 | Round-robin or priority-based task switching |
| System Calls | P1 | Minimal syscall interface (read, write, exec, exit) |
| Filesystem | P1 | Simple FAT-like or custom filesystem on RAM disk |
| User Mode | P2 | Ring 3 user-space execution |
| ELF Loader | P2 | Load and execute ELF-format binaries |
| Network Stack | P3 | TCP/IP basics (future scope) |

#### 2.9 Key Tutorial Resources

| Tutorial | Language | Focus |
|----------|----------|-------|
| [Writing an OS in Rust](https://os.phil-opp.com/) | Rust | **Best modern guide** — blog-style OS from scratch |
| [Operating Systems: From 0 to 1](https://tuhdo.github.io/os01/) | C | Complete book, theory + practice |
| [The little book about OS development](https://littleosbook.github.io/) | C | Concise x86 OS development guide |
| [How to create an OS from scratch](https://github.com/cfenollosa/os-tutorial) | C | Step-by-step with 23 chapters |
| [Roll your own toy UNIX-clone OS](http://jamesmolloy.co.uk/tutorial_html/) | C | Classic JamesM OS tutorial |
| [Kernel 101 – Let's write a Kernel](https://arjunsreedharan.org/post/82710718100/kernel-101-lets-write-a-kernel) | C | Minimal kernel in ~100 lines |
| [Kernel 201 – Keyboard and Screen](https://arjunsreedharan.org/post/99370248137/kernel-201-lets-write-a-kernel-with-keyboard) | C | Adding device drivers |
| [Writing a Tiny x86 Bootloader](http://joebergeron.io/posts/post_two.html) | Assembly | x86 boot sector fundamentals |
| [Baking Pi – RPi OS Development](http://www.cl.cam.ac.uk/projects/raspberrypi/tutorials/os/index.html) | Assembly | ARM-based OS on Raspberry Pi |
| [Raspberry Pi OS](https://github.com/s-matyukevich/raspberry-pi-os) | C | Linux-like OS on RPi |
| [Linux from Scratch](https://linuxfromscratch.org/lfs) | Any | Build a complete Linux system |
| [OS development for Dummies](https://medium.com/@lduck11007/operating-systems-development-for-dummies-3d4d786e8ac) | C | Beginner-friendly walkthrough |
| [RISC-V Rust OS Tutorial](https://osblog.stephenmarz.com/) | Rust | RISC-V OS in Rust |
| [Write your own OS (C++ Playlist)](https://www.youtube.com/playlist?list=PLHh55M_Kq4OApWScZyPl5HhgsTJS9MZ6M) | C++ | Video series |
| [Writing a Bootloader](http://3zanders.co.uk/2017/10/13/writing-a-bootloader/) | C++ | UEFI bootloader guide |
| [Build a minimal multi-tasking kernel for ARM](https://github.com/jserv/mini-arm-os) | C | ARM multitasking |
| [Nand to Tetris](http://nand2tetris.org/) | Any | **Full stack** — hardware to OS to compiler |

#### 2.10 OS Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      USER SPACE (Ring 3)                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────────────┐  │
│  │  Shell   │  │ Programs │  │  Custom Language Runtime  │  │
│  └────┬─────┘  └────┬─────┘  └────────────┬─────────────┘  │
│       │              │                     │                │
│───────┴──────────────┴─────────────────────┴────────────────│
│                    SYSTEM CALL INTERFACE                     │
│  (read, write, open, close, fork, exec, exit, mmap, ...)   │
│─────────────────────────────────────────────────────────────│
│                      KERNEL SPACE (Ring 0)                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │   Process    │  │    Memory    │  │    Filesystem    │  │
│  │  Scheduler   │  │   Manager    │  │     (FAT/ext)    │  │
│  └──────────────┘  └──────────────┘  └──────────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │   Interrupt  │  │   Device     │  │   System Call    │  │
│  │   Handler    │  │   Drivers    │  │    Dispatcher    │  │
│  └──────────────┘  └──────────────┘  └──────────────────┘  │
│─────────────────────────────────────────────────────────────│
│                    HARDWARE ABSTRACTION LAYER                │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌───────────┐  │
│  │   VGA    │  │ Keyboard │  │   Timer  │  │   Disk    │  │
│  │  Driver  │  │  Driver  │  │  Driver  │  │  Driver   │  │
│  └──────────┘  └──────────┘  └──────────┘  └───────────┘  │
│─────────────────────────────────────────────────────────────│
│                        BOOTLOADER                           │
│  (BIOS/UEFI → Real Mode → Protected Mode → Load Kernel)   │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Technology Stack Recommendation

| Component | Primary Choice | Alternative | Rationale |
|-----------|---------------|-------------|-----------|
| **Language Implementation** | C | Rust / Python | C gives low-level control, massive tutorial ecosystem |
| **Shell Implementation** | C | Rust | Directly interfaces with POSIX system calls |
| **OS Kernel** | C + Assembly | Rust + Assembly | Industry standard for kernel dev; Rust for memory safety |
| **Build System** | Make / CMake | Cargo (Rust) | Cross-compilation, linking control |
| **Emulator/Testing** | QEMU | Bochs / VirtualBox | Fast x86/ARM emulation for OS testing |
| **Debugger** | GDB | LLDB | Kernel-level debugging |
| **Version Control** | Git | — | Mandatory for incremental development |

---

## 4. Development Environment Setup

```bash
# Essential tools
sudo apt install build-essential nasm qemu-system-x86 gdb
sudo apt install grub-pc-bin xorriso mtools  # For ISO creation

# For Rust-based OS (alternative)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
rustup component add rust-src llvm-tools-preview
cargo install bootimage

# Cross-compiler (for OS dev targeting i686)
sudo apt install gcc-multilib
# Or build a cross-compiler targeting i686-elf
```

---

## 5. Milestone Checkpoints

| Milestone | Week | Deliverable | Verification |
|-----------|------|-------------|--------------|
| **M1** | 2 | Lexer tokenizes expressions | Unit tests pass for tokenization |
| **M2** | 4 | Parser builds valid ASTs | AST pretty-printer works |
| **M3** | 6 | Interpreter evaluates expressions + variables | `1 + 2 * 3` → `7` |
| **M4** | 8 | Full language with functions, loops, REPL | Fibonacci computes correctly |
| **M5** | 10 | Shell parses and executes basic commands | `ls`, `cd`, `pwd` work |
| **M6** | 12 | Shell with piping, redirection, scripting | `cat file | grep x > out` works |
| **M7** | 15 | Bootloader + kernel boots in QEMU | "Hello, Kernel!" on screen |
| **M8** | 18 | Memory management + keyboard input | Can type and see characters |
| **M9** | 21 | Process scheduler + basic filesystem | Multiple processes run |
| **M10** | 24 | **Full stack integration**: OS runs shell, shell runs language | End-to-end demo |

---

## 6. Risk Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| OS dev complexity | High | Start with QEMU testing, follow proven tutorials step by step |
| Compiler bugs | Medium | Use comprehensive test suites, compare with reference implementations |
| Cross-compilation issues | Medium | Use Docker for reproducible build environments |
| Scope creep | High | Strict MVP feature gates per phase |
| Debugging kernel panics | High | Use GDB with QEMU stub, serial console logging |

---

## 7. Related Supplementary Projects (from build-your-own-x)

These complement the three core projects:

| Project | Relevance |
|---------|-----------|
| **Build your own VM/Emulator** | Bytecode VM for language, testing OS |
| **Build your own Regex Engine** | Required for language & shell pattern matching |
| **Build your own Git** | Version control understanding |
| **Build your own Memory Allocator** | Core OS component (`malloc()`) |
| **Build your own Network Stack** | Future OS networking |
| **Build your own Database** | Future filesystem/query support |
| **Build your own Text Editor** | Can run on your OS |

---

> **Next**: See [prd.md](./prd.md) for detailed product requirements and [system_design.md](./system_design.md) for architecture deep-dive.
