#ifndef PLIKE_SYMTABLE_H
#define PLIKE_SYMTABLE_H

#include "ast.h"
#include <stdbool.h>

#define HASH_SIZE 211
#define MAX_SCOPE_DEPTH 128

typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_PROCEDURE,
    SYMBOL_PARAMETER,
    SYMBOL_TYPE
} SymbolKind;

typedef enum {
    SCOPE_GLOBAL,
    SCOPE_FUNCTION,
    SCOPE_BLOCK
} ScopeType;

typedef struct {
    char* type;
    bool is_array;
    bool is_pointer;
    int pointer_level;
    ArrayBoundsData* bounds;    // Array bounds for each dimension
    int dimensions;            // Number of dimensions
    bool is_parameter;
    bool needs_type_declaration;
    char* param_mode;         
    bool initialized;
    bool has_dynamic_size;    // Whether any dimension uses variables
    bool needs_deref;
    SourceLocation decl_loc;
} VariableInfo;

typedef struct {
    char* return_type;
    bool is_procedure;
    int param_count;
    struct Symbol** parameters;
    bool has_return_var;    // Whether function name variable is explicitly declared
    struct Symbol** local_variables;
    int local_var_count;
    bool is_pointer;
    int pointer_level;
} FunctionInfo;

typedef struct Symbol {
    char* name;
    SymbolKind kind;
    union {
        VariableInfo var;
        FunctionInfo func;
        RecordTypeData record;
    } info;
    struct Scope* scope;
    struct Symbol* next;    // For hash table chaining
    ASTNode* node;
} Symbol;

typedef struct Scope {
    ScopeType type;
    struct Scope* parent;
    Symbol** symbols;       // Hash table of symbols
    int symbol_count;
    char* function_name;    // For function scopes
} Scope;

typedef struct {
    Scope* current;
    Scope* global;
    int scope_level;
} SymbolTable;

// Symbol table operations
SymbolTable* symtable_create(void);
void symtable_destroy(SymbolTable* table);

void symtable_destroy_bounds(ArrayBoundsData* bounds);
ArrayBoundsData* symtable_clone_bounds(const ArrayBoundsData* bounds);
ArrayBoundsData* symtable_create_bounds(int dimensions);
Symbol* symtable_add_array(SymbolTable* table, const char* name, const char* elem_type, ArrayBoundsData* bounds);

// Scope management
Scope* scope_create(ScopeType type, Scope* parent);
void symtable_enter_scope(SymbolTable* table, ScopeType type);
void symtable_exit_scope(SymbolTable* table);
Scope* symtable_current_scope(SymbolTable* table);

// Symbol management
Symbol* symtable_add_variable(SymbolTable* table, const char* name, const char* type, bool is_array);
Symbol* symtable_add_function(SymbolTable* table, const char* name, const char* return_type, bool is_procedure);
Symbol* symtable_add_parameter(SymbolTable* table, const char* name, const char* type, const char* mode, ASTNode* node, bool needs_deref);
void symtable_update_parameter_bounds_in_global(SymbolTable* table, const char* param_name, ArrayBoundsData* bounds);
void symtable_add_local_to_function(SymbolTable* table, const char* function_name, Symbol* local_var);
Symbol* symtable_add_type(SymbolTable* table, const char* name, ASTNode* record);

// Symbol lookup
Symbol* symtable_lookup(SymbolTable* table, const char* name);
Symbol* symtable_lookup_global(SymbolTable* table, const char* name);
Symbol* symtable_lookup_parameter(SymbolTable* table, const char* function_name, const char* param_name);
Symbol* symtable_lookup_current_scope(SymbolTable* table, const char* name);
RecordTypeData* symtable_lookup_type(SymbolTable* table, const char* name);


// Utility functions
bool symtable_is_type_compatible(const char* type1, const char* type2);
void symtable_print_current_scope(SymbolTable* table);
void symtable_report_error(SymbolTable* table, const char* message);

// Debug functions
void symtable_debug_dump_all(SymbolTable* table);
void symtable_debug_dump_scope(Scope* scope, int level);
void symtable_debug_dump_symbol(Symbol* sym, int level);

#endif // PLIKE_SYMTABLE_H