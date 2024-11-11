# P-Like Language to C Translator

A source-to-source compiler that translates programs written in a Pascal-like language into equivalent C code. The translator supports various language features including procedures, functions, arrays (multi-dimensional), parameter passing modes (in/out/inout), and more.

## Requirements

- C compiler (gcc or clang)
- Make build system
- (Optional) Graphviz for debug visualization

## Building

```bash
# Clone the repository
git clone https://github.com/yourusername/plike-translator
cd plike-translator

# Build the project
make

# Optional: Build with debug features
make debug
```

## Usage

```bash
# Basic usage
./plike input.p output.c

# With array bounds checking enabled
./plike --bounds-check input.p output.c

# With verbose output
./plike --verbose input.p output.c
```

## Language Features

<details>
<summary>Click to expand language features</summary>

### Basic Syntax
```pascal
// Function declaration
function sum(in a: integer, in b: integer): integer
begin
    sum := a + b;
end sum

// Procedure declaration
procedure swap(inout x: integer, inout y: integer)
var temp: integer;
begin 
    temp := x;
    x := y;
    y := temp;
end swap
```

### Array Operations
```pascal
// Fixed-size arrays
var arr: array[10] of integer;

// Range-based arrays
var matrix: array[1..5, 1..5] of real;

// Dynamic arrays
var dynamic: array[n] of integer;
```

### Parameter Passing Modes
- `in`: Pass by value (default)
- `out`: Pass by reference (write-only)
- `inout`: Pass by reference (read/write)

### Control Structures
- `if`/`elseif`/`else`/`endif`
- `while`/`endwhile`
- `for`/`endfor`
- `repeat`/`until`

</details>

## Architecture

<details>
<summary>Click to expand architecture diagrams</summary>

### AST Structure

```mermaid
classDiagram
    class ASTNode {
        +NodeType type
        +SourceLocation loc
        +NodeData data
        +ArrayBoundsData array_bounds
        +ASTNode** children
        +int child_count
        +int child_capacity
    }

    class NodeData {
        <<union>>
        +FunctionData function
        +VariableData variable
        +BinaryOpData binary_op
        +UnaryOpData unary_op
        +ArrayAccessData array_access
        +ParameterData parameter
        +char* value
    }

    class FunctionData {
        +char* name
        +char* return_type
        +ASTNode* params
        +ASTNode* body
        +bool is_procedure
        +bool type_before_name
        +bool is_pointer
        +int pointer_level
    }

    class VariableData {
        +char* name
        +char* type
        +bool is_array
        +bool is_pointer
        +int pointer_level
        +ArrayInfo array_info
        +bool is_param
        +char* param_mode
        +bool is_constant
        +SourceLocation decl_loc
    }

    class Symbol {
        +char* name
        +SymbolKind kind
        +SymbolInfo info
        +Scope* scope
    }

    class Scope {
        +ScopeType type
        +Scope* parent
        +Symbol** symbols
        +int symbol_count
    }

    ASTNode --* NodeData
    NodeData --* FunctionData
    NodeData --* VariableData
    Symbol --o Scope
    Scope --o Symbol
```

### Component Interactions

```mermaid
sequenceDiagram
    participant Main
    participant Config
    participant Lexer
    participant Parser
    participant AST
    participant SymTab
    participant Debug
    participant CodeGen
    participant Logger

    Main->>Config: Initialize configuration
    Main->>Debug: Initialize debug system
    Main->>Lexer: Create lexer
    
    activate Lexer
    Lexer->>Logger: Log initialization
    Lexer-->>Main: Return lexer instance
    deactivate Lexer

    Main->>Parser: Create parser
    activate Parser
    Parser->>SymTab: Create symbol table
    Parser->>Lexer: Request tokens
    loop Parsing
        Parser->>AST: Create nodes
        Parser->>SymTab: Update symbols
        Parser->>Debug: Log parser state
    end
    Parser-->>Main: Return AST
    deactivate Parser

    Main->>CodeGen: Create generator
    activate CodeGen
    CodeGen->>AST: Traverse tree
    CodeGen->>SymTab: Lookup symbols
    CodeGen->>Debug: Log generation
    CodeGen->>Logger: Write output
    deactivate CodeGen

    Main->>Logger: Cleanup
    Main->>Debug: Cleanup
```

### Configuration Impact

```mermaid
flowchart LR
    subgraph Configuration
        direction TB
        assign[Assignment Style<br>:= vs =]
        array[Array Indexing<br>0-based vs 1-based]
        param[Parameter Style<br>Type Declaration]
        op[Operator Style<br>Standard vs Dotted]
        bounds[Array Bounds<br>Checking]
        mixed[Mixed Array<br>Access]
    end

    subgraph "Component Impacts"
        direction TB
        lex[Lexer]
        parse[Parser]
        sym[Symbol Table]
        code[Code Generator]
        debug[Debug System]
    end

    assign -->|Token Recognition| lex
    assign -->|AST Building| parse
    assign -->|Code Output| code
    
    array -->|Symbol Management| sym
    array -->|Bound Checking| code
    
    param -->|Parameter Processing| parse
    param -->|Symbol Creation| sym
    
    op -->|Token Recognition| lex
    op -->|Expression Parsing| parse
    
    bounds -->|Code Generation| code
    bounds -->|Debug Output| debug
    
    mixed -->|Array Access| parse
    mixed -->|Code Generation| code
```

### Translator Architecture

```mermaid
flowchart TB
    subgraph Input/Output
        src[Source Code]
        cfg[Config Files]
        out[C Code Output]
        debug[Debug Output]
        logs[Log Files]
    end

    subgraph "Front End"
        direction TB
        lexer[Lexical Analyzer]
        subgraph "Token Processing"
            tok_stream[Token Stream]
            tok_valid[Token Validation]
        end
        parser[Syntax Parser]
    end

    subgraph "Symbol Management"
        direction TB
        symtab[Symbol Table]
        subgraph "Scope Management"
            global[Global Scope]
            local[Local Scopes]
        end
        param[Parameter Management]
    end

    subgraph "AST Management"
        direction TB
        ast[AST Builder]
        subgraph "Node Types"
            decl[Declarations]
            stmt[Statements]
            expr[Expressions]
        end
        valid[AST Validation]
    end

    subgraph "Code Generation"
        direction TB
        codegen[Code Generator]
        subgraph "Generation Steps"
            preproc[Preprocessing]
            trans[Translation]
            opt[Optimization]
        end
    end

    subgraph "Support Systems"
        direction TB
        debug_sys[Debug System]
        logger[Logger]
        error[Error Handler]
        config[Config Manager]
    end

    src --> lexer
    cfg --> config

    lexer --> tok_stream
    tok_stream --> tok_valid
    tok_valid --> parser
    
    parser --> ast
    parser <--> symtab
    
    symtab --> global
    symtab --> local
    symtab --> param
    
    ast --> decl
    ast --> stmt
    ast --> expr
    ast --> valid

    valid --> codegen
    codegen --> preproc
    preproc --> trans
    trans --> opt
    opt --> out

    config --> lexer
    config --> parser
    config --> codegen
    
    error --> debug
    error --> logs
    
    logger --> logs
    debug_sys --> debug

    lexer -.-> logger
    parser -.-> logger
    codegen -.-> logger
    
    lexer -.-> error
    parser -.-> error
    codegen -.-> error
    
    style lexer fill:#f9f,stroke:#333,stroke-width:2px
    style codegen fill:#bbf,stroke:#333,stroke-width:2px
    style symtab fill:#bfb,stroke:#333,stroke-width:2px
    style ast fill:#fbf,stroke:#333,stroke-width:2px
```

</details>

## Configuration Options

The translator supports various configuration options that affect how the code is processed and generated:

- Assignment style (`:=` vs `=`)
- Array indexing (0-based vs 1-based)
- Parameter style (type declaration location)
- Operator style (standard vs dotted logical operators)
- Array bounds checking
- Mixed array access syntax (`[]` and `()`)

## Contributing

Contributions are welcome! Please read our contributing guidelines before submitting pull requests.

## License

This project is licensed under the MIT License - see the LICENSE file for details.
