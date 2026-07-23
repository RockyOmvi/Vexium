# ⚡ Vexium Programming Language (v3.0 / v4.0 Enterprise)

> **"Every feature in Vexium exists because another language made a developer cry."**

Vexium is a high-performance, next-generation programming language designed to unify **AI/Data Science, Native OS Windowing, WebAssembly, Lock-Free Multi-Threading, and Distributed Systems** into a single, cohesive developer stack.

---

## 🌟 Why Vexium? (The Single-Stack Advantage)

Before Vexium, developers were forced to juggle four different languages and ecosystems:
* **Python** for AI/Data Science (but crippled by the GIL & 100x slower execution).
* **C++** for GPU performance & game engines (but plagued by manual memory segmentation faults & build configuration hell).
* **TypeScript / JS** for Web Frontends & WASM targets.
* **Go / Java** for Backend Microservices & Concurrency.

**With Vexium, you write 100% of your stack in ONE unified language:**

```
                          ┌────────────────────────┐
                          │    VEXIUM SINGLE STACK │
                          └───────────┬────────────┘
                                      │
         ┌──────────────────┬─────────┴────────┬──────────────────┐
         │                  │                  │                  │
  🤖 AI Models       🌐 Web Server       🎮 GPU Engine       📊 Big Data
 (use ai / gpu)       (use web/db)       (use gui/window)   (use dataframe)
```

---

## ⚡ Comparison Matrix: Vexium vs. Legacy Languages

| Domain | 🐍 Python | ⚡ C++ | ☕ Java | 🦀 Rust | ⚡ VEXIUM |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Speed** | ❌ 100x Slower | 🟢 Fast | 🟡 Medium (JVM Warmup) | 🟢 Fast | 🟢 **Native C / x86_64 JIT Speed** |
| **Multi-Core** | ❌ Blocked by GIL | 🟢 Native Threads | 🟢 JVM Threads | 🟢 Native Threads | 🟢 **NO GIL (M:N Lock-Free Fibers)** |
| **Memory** | ❌ 28-byte int bloat | ⚠️ Manual / Bug Prone | ❌ Large JVM Footprint | 🟢 Borrow Checker | 🟢 **NaN-Boxing (< 15MB RAM)** |
| **AI Tensors**| 🟢 PyTorch / PyPI | ⚠️ Complex Wrappers | ❌ Weak | ⚠️ Rigid Graphs | 🟢 **First-Class INT4/FP8 Tensors** |
| **Packaging**| ⚠️ Dependency Hell | ❌ CMake / Make Hell | ⚠️ Maven / Gradle | 🟢 Cargo | 🟢 **VexPI (`vex add <package>`)** |
| **WASM Target**| ❌ Slow Emscripten | ⚠️ Heavy Toolchain | ❌ Non-Standard | 🟢 Wasm-Bindgen | 🟢 **Built-in LEB128 WASM Emitter** |

---

## 📦 VexPI (Vexium Package Index) & Serverless Toolchain

Vexium includes an enterprise-grade package manager (**VexPI**) equivalent to PyPI (`pip`), Cargo (`crates.io`), and npm.

```bash
# 1. Initialize a new project manifest (vex.json)
vex init

# 2. Add a package via GitHub CDN, Git URL, or Local Path (Sub-second resolution!)
vex add vex-torch
vex add https://github.com/user/vex-repo.git
vex add ./my-local-module

# 3. Restores exact package checksums from vex.lock
vex install

# 4. Search global package registry
vex search gpu

# 5. Package release tarball & publish to VexPI Registry
vex publish
```

### Manifest (`vex.json`) & Lockfile (`vex.lock`)

```json
{
  "name": "vex-app",
  "version": "1.0.0",
  "dependencies": {
    "vex-torch": "^1.0.0"
  }
}
```

---

## 🔬 Core Engine Architecture

1. **NaN-Boxing Primitive Representation ([`src/value.h`](file:///d:/Vexium/src/value.h))**: IEEE 754 Quiet NaN bitmask (`0x7ff8000000000000`) encoding 64-bit doubles, 32-bit tagged integers, booleans, nil, and object pointers in unboxed 64-bit values (`Value64`).
2. **x86_64 JIT Lowering Engine ([`src/jit.c`](file:///d:/Vexium/src/jit.c))**: Dynamic machine code generation emitting REX/ModRM/SIB bytes, relative 32-bit jump patch offsets, and RWX memory allocation (`VirtualProtect`/`mprotect`).
3. **WebAssembly Binary Emitter ([`src/wasm_emitter.c`](file:///d:/Vexium/src/wasm_emitter.c))**: Signed/unsigned LEB128 variable-length integer encoders emitting WASM binary module sections (Type 1, Function 3, Memory 5, Export 7, Code 10).
4. **Multi-Vendor GPU Driver ([`src/gpu_driver.c`](file:///d:/Vexium/src/gpu_driver.c))**: Dynamic DLL/SO runtime resolution (`LoadLibraryA`/`dlopen`) for CUDA (`nvcuda.dll`), OpenCL (`OpenCL.dll`), and Vulkan (`vulkan-1.dll`) with host CPU fallback.
5. **Lock-Free Fiber Task Scheduler ([`src/thread.c`](file:///d:/Vexium/src/thread.c))**: Chase-Lev work-stealing queue using hardware atomic Compare-And-Swap (CAS) instructions.
6. **Card-Table Generational Garbage Collector ([`src/gc.c`](file:///d:/Vexium/src/gc.c))**: 512-byte card table tracking and write barrier instrumentation (`gc_write_barrier`, `gc_mark_dirty_card`).
7. **Embedded WAL Storage Engine ([`src/stdlib_db.c`](file:///d:/Vexium/src/stdlib_db.c))**: Embedded key-value persistence log (`vexium.wal`) supporting `kv_set`, `kv_get`, and `kv_delete`.
8. **Interactive Win32 Desktop Windowing ([`src/stdlib.c`](file:///d:/Vexium/src/stdlib.c))**: Native OS window registration (`RegisterClassExA`, `CreateWindowExA`), Win32 controls (`EDIT`, `BUTTON`, `LISTBOX`), and modal event pump (`GetMessageA`).
9. **LSP & DAP Protocol Servers ([`src/lsp.c`](file:///d:/Vexium/src/lsp.c), [`src/disasm.c`](file:///d:/Vexium/src/disasm.c))**: JSON-RPC 2.0 stdio Language Server (symbol indexer, autocomplete, diagnostics) and Debug Adapter Protocol debugger.

---

## 🗺️ VexPI Package Author Roadmap (Community Opportunities)

While Vexium's core stdlib includes built-in modules (`use ai`, `use db`, `use net`, `use web`, `use json`, `use path`, `use dataframe`, `use gui`), community package authors can build and publish packages across these domains:

* **🤖 AI & Deep Learning**: SAFETENSORS / GGUF binary model weight loaders, custom autograd layers.
* **📊 Data Science & DataFrames**: Apache Arrow columnar memory layouts, statistical solvers.
* **👁️ Computer Vision**: Image decoders (PNG/JPEG/WEBP) & spatial convolution filters.
* **🗣️ NLP & LLM Tools**: BPE tokenizers & vector database connectors (ChromaDB, Pinecone).
* **🌐 Web Automation**: Headless browser automation control wire protocols.
* **🎮 Game Development**: 3D GLTF mesh loaders & 2D/3D physics collision solvers.

---

## 🛠️ Quickstart: Building & Running From Source

### 1. Compile the Standalone Vexium Engine (`vexium.exe`)

```bash
# Clone the repository
git clone https://github.com/RockyOmvi/Vexium.git
cd Vexium

# Compile using standard GCC (C99)
gcc -std=c99 -O2 -o vexium.exe src/*.c -lm -lws2_32 -lgdi32 -lwininet
```

### 2. Run the Feature Verification Test Suite

```bash
.\vexium.exe test examples\test_v3_upgrades.vxm
```

### 3. Run the Native Win32 Interactive Desktop Application

```bash
.\vexium.exe run desktop_App\native_gui_app.vxm
```

### 4. Compile a Source File directly to WebAssembly (`.wasm`)

```bash
.\vexium.exe wasm examples\test_v3_upgrades.vxm
```

---

## 📜 License

The Vexium Programming Language Core is open-source under the **MIT License**.
