# Bytecode Compiler Implementation Guide

> **For developers completing the v2 bytecode compiler (current phase)**

---

## Overview

The bytecode compiler converts an Abstract Syntax Tree (AST) into a linear bytecode sequence that the VM executes. This guide details the compilation process for all language construct types.

---

## Section 1: Compilation Architecture

### Compiler State

```c
typedef struct {
    Chunk* current_chunk;        /* Current chunk being compiled into */
    ClassCompiler* current_class;
    FunctionCompiler* current_function;
    
    Parser* parser;              /* For error reporting */
    
    int depth;                   /* Scope depth (for locals) */
} Compiler;

typedef struct {
    Token name;
    int depth;
    bool is_captured;
} Local;

typedef struct {
    uint8_t index;
    bool is_local;
} Upvalue;

typedef struct {
    Token name;
    Compiler* enclosing;
    
    Local locals[256];
    int local_count;
    
    Upvalue upvalues[256];
    int upvalue_count;
    
    int scope_depth;
} FunctionCompiler;
```

### Output: Chunk (Output Buffer)

```c
typedef struct {
    uint8_t* code;           /* Bytecode instructions */
    Value* constants;        /* Constant pool (numbers, strings, etc.) */
    int* lines;              /* Line numbers for each instruction */
    
    int capacity;
    int count;
    int constant_capacity;
    int constant_count;
} Chunk;
```

---

## Section 2: Compilation Patterns by Node Type

### 2.1 Literals & Constants

#### Integer Literal
```c
// AST: IntegerNode { value: 42 }
// Bytecode output:
static void compile_integer(Compiler* comp, ASTNode* node) {
    Value value = VALUE_INT(node->as.integer.value);
    int const_idx = add_constant(comp->current_chunk, value);
    
    if (const_idx <= 0xFF) {
        emit_byte(comp, OP_CONST);
        emit_byte(comp, (uint8_t)const_idx);
    } else {
        emit_byte(comp, OP_CONST);
        emit_byte(comp, (uint8_t)(const_idx & 0xFF));
        emit_byte(comp, (uint8_t)((const_idx >> 8) & 0xFF));
    }
}

// Bytecode: OP_CONST [idx_lo] [idx_hi?]
// Stack after: [42]
```

#### String Literal
```c
// AST: StringNode { value: "hello" }
static void compile_string(Compiler* comp, ASTNode* node) {
    ObjString* str = copy_string(node->as.string.value, 
                                 node->as.string.length);
    Value value = VALUE_OBJ((Obj*)str);
    int const_idx = add_constant(comp->current_chunk, value);
    
    emit_byte(comp, OP_CONST);
    emit_byte(comp, (uint8_t)(const_idx & 0xFF));
    if (const_idx > 0xFF) {
        emit_byte(comp, (uint8_t)((const_idx >> 8) & 0xFF));
    }
}

// Bytecode: OP_CONST [idx]
// Stack after: ["hello"]
```

#### Boolean Literals
```c
// AST: BoolNode { value: true/false }
static void compile_bool(Compiler* comp, ASTNode* node) {
    if (node->as.bool_val.value) {
        emit_byte(comp, OP_TRUE);   // Push true (no operands)
    } else {
        emit_byte(comp, OP_FALSE);  // Push false (no operands)
    }
}

// Bytecode: OP_TRUE | OP_FALSE
// Stack after: [true] or [false]
```

#### Nothing (Null)
```c
// AST: NothingNode { }
static void compile_nothing(Compiler* comp, ASTNode* node) {
    emit_byte(comp, OP_NOTHING);   // Push nothing
}

// Bytecode: OP_NOTHING
// Stack after: [nothing]
```

---

### 2.2 Variables

#### Variable Declaration
```c
// AST: LetDeclNode { name: "x", initializer: IntegerNode{42} }
static void compile_let_decl(Compiler* comp, ASTNode* node) {
    // Step 1: Compile initializer expression
    compile_expression(comp, node->as.let_decl.initializer);
    
    // Step 2: Determine scope (global or local)
    if (comp->current_function->scope_depth == 0) {
        // Global variable
        ObjString* name = copy_string(node->as.let_decl.name.start,
                                      node->as.let_decl.name.length);
        int const_idx = add_constant(comp->current_chunk, VALUE_OBJ((Obj*)name));
        
        emit_byte(comp, OP_DEFINE_GLOBAL);
        emit_byte(comp, (uint8_t)(const_idx & 0xFF));
        if (const_idx > 0xFF) {
            emit_byte(comp, (uint8_t)((const_idx >> 8) & 0xFF));
        }
    } else {
        // Local variable
        add_local(comp, node->as.let_decl.name);
    }
}

// Global bytecode: OP_DEFINE_GLOBAL [name_idx_lo] [name_idx_hi?]
// Local: no bytecode (stack-based, just track depth)
```

#### Variable Access (Get)
```c
// AST: VariableNode { name: "x" }
static void compile_variable(Compiler* comp, ASTNode* node, bool can_assign) {
    Token name = node->as.variable.name;
    
    // Check if it's a local
    int local_idx = find_local(comp->current_function, &name);
    if (local_idx >= 0) {
        emit_byte(comp, OP_GET_LOCAL);
        emit_byte(comp, (uint8_t)local_idx);
    } else {
        // Must be global
        ObjString* name_str = copy_string(name.start, name.length);
        int const_idx = add_constant(comp->current_chunk, VALUE_OBJ((Obj*)name_str));
        
        emit_byte(comp, OP_GET_GLOBAL);
        emit_byte(comp, (uint8_t)(const_idx & 0xFF));
        if (const_idx > 0xFF) {
            emit_byte(comp, (uint8_t)((const_idx >> 8) & 0xFF));
        }
    }
}

// Bytecode: OP_GET_LOCAL [slot] | OP_GET_GLOBAL [name_idx]
// Stack: [...] → [..., value]
```

#### Variable Assignment
```c
// AST: AssignmentNode { name: "x", value: expr }
static void compile_assignment(Compiler* comp, ASTNode* node) {
    Token name = node->as.assignment.name;
    
    // Step 1: Compile RHS expression
    compile_expression(comp, node->as.assignment.value);
    
    // Step 2: Decide opcode based on scope
    int local_idx = find_local(comp->current_function, &name);
    if (local_idx >= 0) {
        emit_byte(comp, OP_SET_LOCAL);
        emit_byte(comp, (uint8_t)local_idx);
    } else {
        ObjString* name_str = copy_string(name.start, name.length);
        int const_idx = add_constant(comp->current_chunk, VALUE_OBJ((Obj*)name_str));
        
        emit_byte(comp, OP_SET_GLOBAL);
        emit_byte(comp, (uint8_t)(const_idx & 0xFF));
        if (const_idx > 0xFF) {
            emit_byte(comp, (uint8_t)((const_idx >> 8) & 0xFF));
        }
    }
}

// Bytecode: OP_SET_LOCAL [slot] | OP_SET_GLOBAL [name_idx]
// Stack: [value, ...] → [value, ...] (value remains on stack)
```

---

### 2.3 Arithmetic Operations

#### Binary Operations (+, -, *, /, %, **)
```c
// AST: BinaryOpNode { left: expr, op: PLUS, right: expr }
static void compile_binary_op(Compiler* comp, ASTNode* node) {
    // Step 1: Compile left operand
    compile_expression(comp, node->as.binary_op.left);
    
    // Step 2: Compile right operand
    compile_expression(comp, node->as.binary_op.right);
    
    // Step 3: Emit operation opcode
    switch (node->as.binary_op.op.type) {
        case TOKEN_PLUS:
            emit_byte(comp, OP_ADD);
            break;
        case TOKEN_MINUS:
            emit_byte(comp, OP_SUB);
            break;
        case TOKEN_STAR:
            emit_byte(comp, OP_MUL);
            break;
        case TOKEN_SLASH:
            emit_byte(comp, OP_DIV);
            break;
        case TOKEN_PERCENT:
            emit_byte(comp, OP_MOD);
            break;
        case TOKEN_POWER:
            emit_byte(comp, OP_POW);
            break;
        default:
            error(comp->parser, "Unknown binary operator");
    }
}

// Example: 10 + 20
// Bytecode:
//   OP_CONST [0]        # Push 10
//   OP_CONST [1]        # Push 20
//   OP_ADD              # Add top two
// Stack: [10] → [10, 20] → [30]
```

#### Unary Operations (-, !)
```c
// AST: UnaryOpNode { op: MINUS, right: expr }
static void compile_unary_op(Compiler* comp, ASTNode* node) {
    // Step 1: Compile operand
    compile_expression(comp, node->as.unary_op.right);
    
    // Step 2: Emit operation
    switch (node->as.unary_op.op.type) {
        case TOKEN_MINUS:
            emit_byte(comp, OP_NEG);    // Negate
            break;
        case TOKEN_NOT:
            emit_byte(comp, OP_NOT);    // Logical NOT
            break;
        default:
            error(comp->parser, "Unknown unary operator");
    }
}

// Example: -42
// Bytecode:
//   OP_CONST [0]        # Push 42
//   OP_NEG              # Negate
// Stack: [42] → [-42]
```

---

### 2.4 Comparison & Logic

#### Comparison Operations (<, >, <=, >=, ==, !=)
```c
// AST: ComparisonNode { left: expr, op: LT, right: expr }
static void compile_comparison(Compiler* comp, ASTNode* node) {
    compile_expression(comp, node->as.comparison.left);
    compile_expression(comp, node->as.comparison.right);
    
    switch (node->as.comparison.op.type) {
        case TOKEN_LT:
            emit_byte(comp, OP_LT);
            break;
        case TOKEN_GT:
            emit_byte(comp, OP_GT);
            break;
        case TOKEN_LTE:
            emit_byte(comp, OP_LTE);
            break;
        case TOKEN_GTE:
            emit_byte(comp, OP_GTE);
            break;
        case TOKEN_EQ:
            emit_byte(comp, OP_EQ);
            break;
        case TOKEN_NEQ:
            emit_byte(comp, OP_NEQ);
            break;
        default:
            error(comp->parser, "Unknown comparison operator");
    }
}

// Example: 5 < 10
// Bytecode: OP_CONST [0] OP_CONST [1] OP_LT
// Stack: [5, 10] → [true]
```

#### Logical AND / OR
```c
// AST: LogicalOpNode { left: expr, op: AND, right: expr }
static void compile_logical_op(Compiler* comp, ASTNode* node) {
    if (node->as.logical_op.op.type == TOKEN_AND) {
        // x AND y: compile x, jump if false, compile y
        compile_expression(comp, node->as.logical_op.left);
        
        int jump_if_false = emit_jump(comp, OP_JMP_IF_FALSE);
        emit_byte(comp, OP_POP);  // Pop left if true, continue to right
        
        compile_expression(comp, node->as.logical_op.right);
        patch_jump(comp, jump_if_false);
    } else {
        // x OR y: compile x, jump if true, compile y
        compile_expression(comp, node->as.logical_op.left);
        
        int jump_if_true = emit_jump(comp, OP_JMP);
        emit_byte(comp, OP_POP);
        
        compile_expression(comp, node->as.logical_op.right);
        patch_jump(comp, jump_if_true);
    }
}

// Example: true AND false
// Bytecode:
//   OP_TRUE
//   OP_JMP_IF_FALSE [offset=3]   # If false, jump to after OP_POP
//   OP_POP
//   OP_FALSE
// Stack evolution: [true] → [true] (jump skipped) → [] → [false]
```

---

### 2.5 Control Flow

#### If Statement
```c
// AST: IfStmtNode { 
//   condition: expr, 
//   then_branch: stmt,
//   else_branch: stmt? 
// }
static void compile_if_stmt(Compiler* comp, ASTNode* node) {
    // Step 1: Compile condition
    compile_expression(comp, node->as.if_stmt.condition);
    
    // Step 2: Emit conditional jump
    int then_jump = emit_jump(comp, OP_JMP_IF_FALSE);
    emit_byte(comp, OP_POP);  // Pop condition if true
    
    // Step 3: Compile then branch
    compile_statement(comp, node->as.if_stmt.then_branch);
    
    // Step 4: Jump over else (if present)
    int else_jump = emit_jump(comp, OP_JMP);
    
    // Step 5: Patch then_jump to else
    patch_jump(comp, then_jump);
    emit_byte(comp, OP_POP);  // Pop condition if false
    
    // Step 6: Compile else branch (if present)
    if (node->as.if_stmt.else_branch != NULL) {
        compile_statement(comp, node->as.if_stmt.else_branch);
    }
    
    // Step 7: Patch else_jump past else
    patch_jump(comp, else_jump);
}

// Example: if x > 0: display "positive" else: display "negative"
// Bytecode:
//   OP_GET_GLOBAL [x_idx]
//   OP_CONST [0]
//   OP_GT
//   OP_JMP_IF_FALSE [12]         # 12 = offset of else branch
//   OP_POP
//   OP_CONST [str_idx_positive]
//   OP_PRINT
//   OP_JMP [18]                  # Jump past else
//   OP_POP
//   OP_CONST [str_idx_negative]
//   OP_PRINT
```

#### While Loop
```c
// AST: WhileStmtNode { condition: expr, body: stmt }
static void compile_while_stmt(Compiler* comp, ASTNode* node) {
    // Step 1: Mark loop start
    int loop_start = comp->current_chunk->count;
    
    // Step 2: Compile condition
    compile_expression(comp, node->as.while_stmt.condition);
    
    // Step 3: Jump out if false
    int exit_jump = emit_jump(comp, OP_JMP_IF_FALSE);
    emit_byte(comp, OP_POP);
    
    // Step 4: Compile body
    compile_statement(comp, node->as.while_stmt.body);
    
    // Step 5: Loop back
    emit_loop(comp, loop_start);
    
    // Step 6: Patch exit
    patch_jump(comp, exit_jump);
    emit_byte(comp, OP_POP);
}

// Example: while x > 0: x = x - 1
// Bytecode:
//   [0]  OP_GET_GLOBAL [x_idx]
//   [3]  OP_CONST [0]
//   [5]  OP_GT
//   [6]  OP_JMP_IF_FALSE [16]
//   [9]  OP_POP
//   [10] OP_GET_GLOBAL [x_idx]
//   [13] OP_CONST [1]
//   [15] OP_SUB
//   [16] OP_SET_GLOBAL [x_idx]
//   [19] OP_LOOP [-19]            # Jump back to offset 0
//   [22] OP_POP
```

#### For Loop (Range)
```c
// AST: ForStmtNode { 
//   var: "i", 
//   start: expr, 
//   end: expr, 
//   step: expr?,
//   body: stmt 
// }
static void compile_for_stmt(Compiler* comp, ASTNode* node) {
    int loop_depth = ++comp->current_function->scope_depth;
    
    // Step 1: Create loop variable
    add_local(comp, node->as.for_stmt.iter_var);
    
    // Step 2: Compile start expression
    compile_expression(comp, node->as.for_stmt.range_start);
    emit_byte(comp, OP_SET_LOCAL);
    emit_byte(comp, comp->current_function->locals[comp->current_function->local_count - 1].index);
    
    // Step 3: Mark loop start
    int loop_start = comp->current_chunk->count;
    
    // Step 4: Check condition (i < end)
    emit_byte(comp, OP_GET_LOCAL);
    emit_byte(comp, comp->current_function->locals[comp->current_function->local_count - 1].index);
    compile_expression(comp, node->as.for_stmt.range_end);
    emit_byte(comp, OP_LT);
    
    // Step 5: Jump if false
    int exit_jump = emit_jump(comp, OP_JMP_IF_FALSE);
    emit_byte(comp, OP_POP);
    
    // Step 6: Compile body
    compile_statement(comp, node->as.for_stmt.body);
    
    // Step 7: Increment (i = i + step)
    emit_byte(comp, OP_GET_LOCAL);
    emit_byte(comp, comp->current_function->locals[comp->current_function->local_count - 1].index);
    if (node->as.for_stmt.step != NULL) {
        compile_expression(comp, node->as.for_stmt.step);
    } else {
        emit_byte(comp, OP_CONST);
        emit_byte(comp, add_constant(comp->current_chunk, VALUE_INT(1)));
    }
    emit_byte(comp, OP_ADD);
    emit_byte(comp, OP_SET_LOCAL);
    emit_byte(comp, comp->current_function->locals[comp->current_function->local_count - 1].index);
    
    // Step 8: Loop back
    emit_loop(comp, loop_start);
    
    // Step 9: Patch exit
    patch_jump(comp, exit_jump);
    emit_byte(comp, OP_POP);
    
    comp->current_function->scope_depth = loop_depth - 1;
}
```

---

### 2.6 Functions

#### Function Declaration
```c
// AST: FnDeclNode { 
//   name: "greet", 
//   params: ["name"], 
//   body: stmt 
// }
static void compile_fn_decl(Compiler* comp, ASTNode* node) {
    // Step 1: Enter new function scope
    FunctionCompiler fn_compiler;
    init_function_compiler(&fn_compiler, node->as.fn_decl.name);
    fn_compiler.enclosing = comp->current_function;
    comp->current_function = &fn_compiler;
    
    FunctionCompiler* closure = comp->current_function;
    
    // Step 2: Start new chunk for function
    Chunk* current = comp->current_chunk;
    comp->current_chunk = &closure->function->chunk;
    
    // Step 3: Add parameters as locals
    for (int i = 0; i < node->as.fn_decl.param_count; i++) {
        add_local(comp, node->as.fn_decl.params[i]);
    }
    
    // Step 4: Compile body
    if (node->as.fn_decl.body->type == AST_BLOCK) {
        for (int i = 0; i < node->as.fn_decl.body->as.block.stmt_count; i++) {
            compile_statement(comp, node->as.fn_decl.body->as.block.stmts[i]);
        }
    } else {
        compile_statement(comp, node->as.fn_decl.body);
    }
    
    // Step 5: Emit implicit return nothing
    emit_byte(comp, OP_NOTHING);
    emit_byte(comp, OP_RETURN);
    
    // Step 6: Exit function
    comp->current_chunk = current;
    comp->current_function = closure->enclosing;
    
    // Step 7: Define function globally
    ObjString* name = copy_string(node->as.fn_decl.name.start, node->as.fn_decl.name.length);
    int const_idx = add_constant(comp->current_chunk, VALUE_OBJ((Obj*)closure->function));
    
    emit_byte(comp, OP_DEFINE_GLOBAL);
    emit_byte(comp, (uint8_t)(const_idx & 0xFF));
    if (const_idx > 0xFF) {
        emit_byte(comp, (uint8_t)((const_idx >> 8) & 0xFF));
    }
}
```

#### Function Call
```c
// AST: CallNode { callee: expr, args: [expr...] }
static void compile_call(Compiler* comp, ASTNode* node) {
    // Step 1: Compile function expression
    compile_expression(comp, node->as.call.callee);
    
    // Step 2: Compile arguments (left to right)
    for (int i = 0; i < node->as.call.arg_count; i++) {
        compile_expression(comp, node->as.call.args[i]);
    }
    
    // Step 3: Emit call opcode
    emit_byte(comp, OP_CALL);
    emit_byte(comp, node->as.call.arg_count);
}

// Example: greet("Alice")
// Bytecode:
//   OP_GET_GLOBAL [greet_idx]
//   OP_CONST [alice_idx]
//   OP_CALL [1]          # Call with 1 argument
// Stack: [func, "Alice"] → [result]
```

#### Return Statement
```c
// AST: ReturnNode { value: expr? }
static void compile_return(Compiler* comp, ASTNode* node) {
    if (node->as.return_stmt.value != NULL) {
        compile_expression(comp, node->as.return_stmt.value);
    } else {
        emit_byte(comp, OP_NOTHING);
    }
    emit_byte(comp, OP_RETURN);
}

// Bytecode: [expr opcodes] OP_RETURN
// Stack: [..., return_value] → [return_value] (to caller)
```

---

### 2.7 Collections

#### Array Literal
```c
// AST: ArrayNode { elements: [expr...] }
static void compile_array(Compiler* comp, ASTNode* node) {
    // Step 1: Compile each element
    for (int i = 0; i < node->as.array.elem_count; i++) {
        compile_expression(comp, node->as.array.elements[i]);
    }
    
    // Step 2: Emit array creation opcode
    emit_byte(comp, OP_ARRAY);
    emit_byte(comp, (uint8_t)(node->as.array.elem_count & 0xFF));
    if (node->as.array.elem_count > 0xFF) {
        emit_byte(comp, (uint8_t)((node->as.array.elem_count >> 8) & 0xFF));
    }
}

// Example: [1, 2, 3]
// Bytecode:
//   OP_CONST [0]         # Push 1
//   OP_CONST [1]         # Push 2
//   OP_CONST [2]         # Push 3
//   OP_ARRAY [3]         # Create array of 3 elements
// Stack: [1, 2, 3] → [[1, 2, 3]]
```

#### Array Indexing (Get)
```c
// AST: IndexNode { object: expr, index: expr }
static void compile_index_get(Compiler* comp, ASTNode* node) {
    compile_expression(comp, node->as.index.object);
    compile_expression(comp, node->as.index.index);
    emit_byte(comp, OP_INDEX_GET);
}

// Example: arr[0]
// Bytecode: [arr opcodes] [0 opcodes] OP_INDEX_GET
// Stack: [[1,2,3]] → [[1,2,3], 0] → [1]
```

#### Array Indexing (Set)
```c
// AST: IndexAssignNode { object: expr, index: expr, value: expr }
static void compile_index_set(Compiler* comp, ASTNode* node) {
    compile_expression(comp, node->as.index_assign.object);
    compile_expression(comp, node->as.index_assign.index);
    compile_expression(comp, node->as.index_assign.value);
    emit_byte(comp, OP_INDEX_SET);
}

// Example: arr[0] = 99
// Bytecode: [arr] [0] [99] OP_INDEX_SET
// Stack: [[1,2,3], 0, 99] → [[99,2,3]]
```

#### Map Literal
```c
// AST: MapNode { pairs: [(key_expr, value_expr)...] }
static void compile_map(Compiler* comp, ASTNode* node) {
    // Step 1: Compile all key-value pairs
    for (int i = 0; i < node->as.map.pair_count; i++) {
        compile_expression(comp, node->as.map.keys[i]);
        compile_expression(comp, node->as.map.values[i]);
    }
    
    // Step 2: Emit map creation
    emit_byte(comp, OP_MAP);
    emit_byte(comp, (uint8_t)(node->as.map.pair_count & 0xFF));
    if (node->as.map.pair_count > 0xFF) {
        emit_byte(comp, (uint8_t)((node->as.map.pair_count >> 8) & 0xFF));
    }
}

// Example: {"a": 1, "b": 2}
// Bytecode:
//   OP_CONST [a_idx]
//   OP_CONST [1_idx]
//   OP_CONST [b_idx]
//   OP_CONST [2_idx]
//   OP_MAP [2]
// Stack: ["a", 1, "b", 2] → [{"a":1, "b":2}]
```

---

### 2.8 String Operations

#### String Concatenation
```c
// AST: BinaryOpNode { left: str_expr, op: PLUS, right: str_expr }
// Compiled as: OP_ADD (but works on strings too)

// Example: "Hello" + " World"
// Bytecode: OP_CONST ["Hello"] OP_CONST [" World"] OP_ADD
// VM adds string detection at runtime
```

#### String Interpolation
```c
// AST: StringInterpolationNode { 
//   parts: ["Hello, ", name_expr, "!"],
//   exprs: [name_expr]
// }
static void compile_string_interpolation(Compiler* comp, ASTNode* node) {
    // Example: "Hello, {name}!"
    // Compile as: "Hello, " + name + "!"
    
    for (int i = 0; i < node->as.string_interp.part_count; i++) {
        if (i > 0) {
            emit_byte(comp, OP_CONCAT);  // Concatenate with previous
        }
        
        if (node->as.string_interp.parts[i] != NULL) {
            // String literal part
            ObjString* str = copy_string(node->as.string_interp.parts[i],
                                         strlen(node->as.string_interp.parts[i]));
            int const_idx = add_constant(comp->current_chunk, VALUE_OBJ((Obj*)str));
            emit_byte(comp, OP_CONST);
            emit_byte(comp, const_idx);
        } else {
            // Expression to interpolate
            compile_expression(comp, node->as.string_interp.exprs[i]);
            emit_byte(comp, OP_TO_STRING);  // Convert to string
        }
    }
}
```

---

## Section 3: Helper Functions

### Constant Pool Management

```c
int add_constant(Chunk* chunk, Value value) {
    if (chunk->constant_count == chunk->constant_capacity) {
        chunk->constant_capacity = chunk->constant_capacity < 8 
            ? 8 
            : chunk->constant_capacity * 2;
        chunk->constants = realloc(chunk->constants,
            sizeof(Value) * chunk->constant_capacity);
    }
    
    chunk->constants[chunk->constant_count] = value;
    return chunk->constant_count++;
}

Value* get_constant(Chunk* chunk, int index) {
    if (index < 0 || index >= chunk->constant_count) return NULL;
    return &chunk->constants[index];
}
```

### Bytecode Emission

```c
void emit_byte(Compiler* comp, uint8_t byte) {
    if (comp->current_chunk->count == comp->current_chunk->capacity) {
        comp->current_chunk->capacity = comp->current_chunk->capacity < 8
            ? 8
            : comp->current_chunk->capacity * 2;
        comp->current_chunk->code = realloc(comp->current_chunk->code,
            comp->current_chunk->capacity);
        comp->current_chunk->lines = realloc(comp->current_chunk->lines,
            comp->current_chunk->capacity * sizeof(int));
    }
    
    comp->current_chunk->code[comp->current_chunk->count] = byte;
    comp->current_chunk->lines[comp->current_chunk->count] = comp->parser->previous.line;
    comp->current_chunk->count++;
}

int emit_jump(Compiler* comp, uint8_t opcode) {
    emit_byte(comp, opcode);
    emit_byte(comp, 0xFF);  // Placeholder for jump offset
    emit_byte(comp, 0xFF);
    return comp->current_chunk->count - 2;
}

void patch_jump(Compiler* comp, int offset) {
    int jump = comp->current_chunk->count - offset - 2;
    comp->current_chunk->code[offset] = (uint8_t)((jump >> 8) & 0xFF);
    comp->current_chunk->code[offset + 1] = (uint8_t)(jump & 0xFF);
}

void emit_loop(Compiler* comp, int loop_start) {
    emit_byte(comp, OP_LOOP);
    int offset = comp->current_chunk->count - loop_start - 2;
    emit_byte(comp, (uint8_t)((offset >> 8) & 0xFF));
    emit_byte(comp, (uint8_t)(offset & 0xFF));
}
```

### Local Variable Tracking

```c
void add_local(Compiler* comp, Token name) {
    FunctionCompiler* fn = comp->current_function;
    
    if (fn->local_count == 256) {
        error(comp->parser, "Too many local variables in function");
        return;
    }
    
    fn->locals[fn->local_count].name = name;
    fn->locals[fn->local_count].depth = fn->scope_depth;
    fn->locals[fn->local_count].is_captured = false;
    fn->local_count++;
}

int find_local(FunctionCompiler* fn, Token* name) {
    for (int i = fn->local_count - 1; i >= 0; i--) {
        if (identifiers_equal(&fn->locals[i].name, name)) {
            if (fn->locals[i].depth < fn->scope_depth) {
                return -1;  // In parent scope
            }
            return i;
        }
    }
    return -1;  // Not found
}
```

---

## Section 4: Compilation Flow

```c
ObjFunction* compile(ASTNode* program) {
    Parser parser;
    // ... parser setup ...
    
    Compiler compiler;
    init_compiler(&compiler);
    
    // Compile entire program as statements
    for (int i = 0; i < program->as.program.stmt_count; i++) {
        compile_statement(&compiler, program->as.program.stmts[i]);
    }
    
    // Emit final return
    emit_byte(&compiler, OP_NOTHING);
    emit_byte(&compiler, OP_HALT);
    
    return compiler.current_function->function;
}
```

---

## Section 5: Bytecode Validation Checklist

- [ ] All opcodes in language HAVE a compiler case
- [ ] All opcodes emit correct operands (0, 1, or 2 bytes)
- [ ] Jumps are patched correctly
- [ ] Constants are added to pool in correct order
- [ ] Local variables tracked with correct depth
- [ ] Function scope entered/exited properly
- [ ] Stack behavior documented for each opcode
- [ ] No bytecode generated for comments
- [ ] Line numbers tracked for all instructions
- [ ] Error messages include line/column info

---

## Next Steps

1. Add missing node type compilations (structs, async, etc.)
2. Validate all opcodes have compiler support
3. Run test suite: `make test`
4. Compare interpreter vs VM bytecode output
5. Profile OP_CONST usage (add peephole optimization)

---

