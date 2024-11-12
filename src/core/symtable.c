#include "symtable.h"
#include "errors.h"
#include "config.h"
#include "ast.h"
#include "utils.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Hash function for symbol names
static unsigned int hash(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_SIZE;
}

static char* safe_strdup(const char* str) {
    if (!str) return NULL;
    char* dup = strdup(str);
    if (!dup) {
        error_report(ERROR_INTERNAL, SEVERITY_ERROR, 
                    (SourceLocation){0, 0, "internal"},
                    "Failed to allocate memory for string duplication");
    }
    return dup;
}

// Create a new symbol
static Symbol* symbol_create(const char* name, SymbolKind kind) {
    Symbol* symbol = (Symbol*)malloc(sizeof(Symbol));
    if (!symbol) return NULL;

    symbol->name = safe_strdup(name);
    if (!symbol->name) {
        free(symbol);
        return NULL;
    }

    symbol->kind = kind;
    symbol->scope = NULL;
    symbol->next = NULL;
    memset(&symbol->info, 0, sizeof(symbol->info));

    // Initialize union based on kind
    if (kind == SYMBOL_FUNCTION || kind == SYMBOL_PROCEDURE) {
        symbol->info.func.return_type = NULL;
        symbol->info.func.param_count = 0;
        symbol->info.func.parameters = NULL;
        symbol->info.func.local_var_count = 0;
        symbol->info.func.local_variables = NULL;
    } else {
        symbol->info.var.type = NULL;
        symbol->info.var.is_array = false;
        symbol->info.var.bounds = NULL;
        symbol->info.var.dimensions = 0;
        symbol->info.var.param_mode = NULL;
    }

    return symbol;
}

static void symbol_destroy(Symbol* symbol) {
    if (!symbol) return;
    
    free(symbol->name);
    
    if (symbol->kind == SYMBOL_FUNCTION) {
        free(symbol->info.func.return_type);
        for (int i = 0; i < symbol->info.func.param_count; i++) {
            symbol_destroy(symbol->info.func.parameters[i]);
        }
        free(symbol->info.func.parameters);
    } else if (symbol->kind == SYMBOL_VARIABLE) {
        free(symbol->info.var.type);
    }
    
    free(symbol);
}

// Create new scope
Scope* scope_create(ScopeType type, Scope* parent) {
    Scope* scope = (Scope*)malloc(sizeof(Scope));
    if (!scope) return NULL;

    scope->type = type;
    scope->parent = parent;
    scope->symbols = (Symbol**)calloc(HASH_SIZE, sizeof(Symbol*));
    scope->symbol_count = 0;
    scope->function_name = NULL;

    if (!scope->symbols) {
        free(scope);
        return NULL;
    }

    return scope;
}

static void scope_destroy(Scope* scope) {
    if (!scope) return;

    // Free all symbols in this scope
    for (int i = 0; i < HASH_SIZE; i++) {
        Symbol* current = scope->symbols[i];
        while (current) {
            Symbol* next = current->next;
            debug_symbol_destroy(current, "destroying scope and symbols");
            symbol_destroy(current);
            current = next;
        }
    }

    free(scope->symbols);
    free(scope->function_name);
    free(scope);
}

// Symbol table operations
SymbolTable* symtable_create(void) {
    SymbolTable* table = (SymbolTable*)malloc(sizeof(SymbolTable));
    if (!table) return NULL;

    table->global = scope_create(SCOPE_GLOBAL, NULL);
    if (!table->global) {
        free(table);
        return NULL;
    }

    table->current = table->global;
    table->scope_level = 0;

    return table;
}

void symtable_destroy(SymbolTable* table) {
    if (!table) return;

    // Start from the current scope and move up to global
    while (table->current && table->current != table->global) {
        Scope* parent = table->current->parent;
        scope_destroy(table->current);
        table->current = parent;
    }

    scope_destroy(table->global);
    free(table);
}

void symtable_enter_scope(SymbolTable* table, ScopeType type) {
    if (!table || table->scope_level >= MAX_SCOPE_DEPTH) {
        debug_symbol_table_operation("Enter Scope Failed", "Maximum depth exceeded or invalid table");
        error_report(ERROR_INTERNAL, SEVERITY_ERROR, 
                    (SourceLocation){0, 0, "internal"},
                    "Maximum scope depth exceeded");
        return;
    }

    Scope* new_scope = scope_create(type, table->current);
    if (!new_scope) {
        debug_symbol_table_operation("Enter Scope Failed", "Failed to create new scope");
        error_report(ERROR_INTERNAL, SEVERITY_ERROR, 
                    (SourceLocation){0, 0, "internal"},
                    "Failed to create new scope");
        return;
    }

    debug_scope_enter(new_scope, "entering new scope");
    table->current = new_scope;
    table->scope_level++;
}

void symtable_exit_scope(SymbolTable* table) {
    if (!table || !table->current || table->current == table->global) {
        debug_symbol_table_operation("Exit Scope Failed", "Cannot exit global scope or invalid state");
        error_report(ERROR_INTERNAL, SEVERITY_ERROR, 
                    (SourceLocation){0, 0, "internal"},
                    "Cannot exit global scope");
        return;
    }

    debug_scope_exit(table->current, "exiting current scope");

    Scope* old_scope = table->current;
    table->current = old_scope->parent;
    table->scope_level--;
    //scope_destroy(old_scope);
}

static void deep_copy_var_info(VariableInfo* dest, const VariableInfo* src) {
    if (!dest || !src) return;
    
    dest->type = src->type ? strdup(src->type) : NULL;
    dest->is_array = src->is_array;
    dest->dimensions = src->dimensions;
    dest->is_parameter = src->is_parameter;
    dest->param_mode = src->param_mode ? strdup(src->param_mode) : NULL;
    dest->initialized = src->initialized;
    dest->has_dynamic_size = src->has_dynamic_size;
    dest->decl_loc = src->decl_loc;
    
    if (src->is_array && src->bounds) {
        dest->dimensions = src->dimensions;
        dest->bounds = symtable_clone_bounds(src->bounds);
        verbose_print("Deep copied bounds for parameter with %d dimensions\n", 
                     src->dimensions);
        if (dest->bounds) {
            for (int i = 0; i < src->dimensions; i++) {
                DimensionBounds* src_bound = &src->bounds->bounds[i];
                DimensionBounds* dest_bound = &dest->bounds->bounds[i];
                verbose_print("Dimension %d: range=%s ", i, 
                            src_bound->using_range ? "yes" : "no");
                
                if (src_bound->start.is_constant) {
                    verbose_print("start=constant(%ld) ", src_bound->start.constant_value);
                } else {
                    verbose_print("start=variable(%s) ", 
                                src_bound->start.variable_name ? 
                                src_bound->start.variable_name : "NULL");
                }
                
                if (src_bound->end.is_constant) {
                    verbose_print("end=constant(%ld)\n", src_bound->end.constant_value);
                } else {
                    verbose_print("end=variable(%s)\n", 
                                src_bound->end.variable_name ? 
                                src_bound->end.variable_name : "NULL");
                }
            }
        }
    } else {
        dest->bounds = NULL;
    }
}

Symbol* symtable_add_variable(SymbolTable* table, const char* name, const char* type, bool is_array) {
    debug_symbol_table_operation("Add Variable", name);
    if (!table || !name || !type) return NULL;

    // Check if variable already exists in current scope
    unsigned int h = hash(name);
    Symbol* existing = table->current->symbols[h];
    while (existing) {
        if (strcmp(existing->name, name) == 0) {            
            debug_symbol_table_operation("Variable Already Exists", name);
            error_report(ERROR_SEMANTIC, SEVERITY_ERROR,
                        (SourceLocation){0, 0, "internal"},
                        "Variable '%s' already declared in current scope", name);
            return NULL;
        }
        existing = existing->next;
    }

    Symbol* symbol = symbol_create(name, SYMBOL_VARIABLE);
    if (!symbol) return NULL;

    symbol->info.var.type = strdup(type);
    symbol->info.var.is_array = is_array;
    symbol->info.var.dimensions = 0;
    symbol->info.var.is_parameter = false;
    symbol->info.var.initialized = false;

    // Add to current scope
    symbol->scope = table->current;
    symbol->next = table->current->symbols[h];
    
    table->current->symbols[h] = symbol;
    table->current->symbol_count++;

    if (table->current->type == SCOPE_FUNCTION || 
        (table->current->type == SCOPE_BLOCK && table->current->function_name)) {
        const char* func_name = table->current->function_name;
        symtable_add_local_to_function(table, func_name, symbol);
    }
    debug_symbol_create(symbol, "adding new variable to current scope");
    debug_symbol_table_operation("Variable Added Successfully", name);

    return symbol;
}

Symbol* symtable_add_function(SymbolTable* table, const char* name, 
                            const char* return_type, bool is_procedure) {
    if (!table || !name) return NULL;

    // Functions can only be added to global scope
    if (table->current != table->global) {
        error_report(ERROR_SEMANTIC, SEVERITY_ERROR,
                    (SourceLocation){0, 0, "internal"},
                    "Functions can only be declared in global scope");
        return NULL;
    }

    // Check if function already exists
    unsigned int h = hash(name);
    Symbol* existing = table->global->symbols[h];
    while (existing) {
        if (strcmp(existing->name, name) == 0) {
            error_report(ERROR_SEMANTIC, SEVERITY_ERROR,
                        (SourceLocation){0, 0, "internal"},
                        "Function '%s' already declared", name);
            return NULL;
        }
        existing = existing->next;
    }

    Symbol* symbol = symbol_create(name, SYMBOL_FUNCTION);
    if (!symbol) return NULL;

    symbol->info.func.return_type = return_type ? strdup(return_type) : NULL;
    symbol->info.func.is_procedure = is_procedure;
    symbol->info.func.param_count = 0;
    symbol->info.func.parameters = NULL;
    symbol->info.func.has_return_var = false;

    // Add to global scope
    symbol->scope = table->global;
    symbol->next = table->global->symbols[h];
    table->global->symbols[h] = symbol;
    table->global->symbol_count++;
    debug_symbol_create(symbol, "adding new function to global scope");
    debug_symbol_table_operation("Variable Added Successfully", name);

    return symbol;
}

ArrayBoundsData* symtable_create_bounds(int dimensions) {
    ArrayBoundsData* bounds = (ArrayBoundsData*)malloc(sizeof(ArrayBoundsData));
    if (!bounds) return NULL;
    
    bounds->dimensions = dimensions;
    bounds->bounds = (DimensionBounds*)calloc(dimensions, sizeof(DimensionBounds));
    if (!bounds->bounds) {
        free(bounds);
        return NULL;
    }
    
    // Initialize each dimension
    for (int i = 0; i < dimensions; i++) {
        bounds->bounds[i].using_range = false;
        bounds->bounds[i].start.is_constant = false;
        bounds->bounds[i].end.is_constant = false;
    }
    
    return bounds;
}

void symtable_destroy_bounds(ArrayBoundsData* bounds) {
    if (!bounds) return;
    
    if (bounds->bounds) {
        for (int i = 0; i < bounds->dimensions; i++) {
            if (!bounds->bounds[i].start.is_constant) {
                free(bounds->bounds[i].start.variable_name);
            }
            if (!bounds->bounds[i].end.is_constant) {
                free(bounds->bounds[i].end.variable_name);
            }
        }
        free(bounds->bounds);
    }
    free(bounds);
}

ArrayBoundsData* symtable_clone_bounds(const ArrayBoundsData* src) {
    if (!src) return NULL;

    ArrayBoundsData* bounds = symtable_create_bounds(src->dimensions);
    if (!bounds) return NULL;

    for (int i = 0; i < src->dimensions; i++) {
        bounds->bounds[i].using_range = src->bounds[i].using_range;

        // Clone start bound
        bounds->bounds[i].start.is_constant = src->bounds[i].start.is_constant;
        if (src->bounds[i].start.is_constant) {
            bounds->bounds[i].start.constant_value = src->bounds[i].start.constant_value;
        } else if (src->bounds[i].start.variable_name) {
            bounds->bounds[i].start.variable_name = strdup(src->bounds[i].start.variable_name);
            if (!bounds->bounds[i].start.variable_name) {
                symtable_destroy_bounds(bounds);
                return NULL;
            }
        }

        // Clone end bound
        bounds->bounds[i].end.is_constant = src->bounds[i].end.is_constant;
        if (src->bounds[i].end.is_constant) {
            bounds->bounds[i].end.constant_value = src->bounds[i].end.constant_value;
        } else if (src->bounds[i].end.variable_name) {
            bounds->bounds[i].end.variable_name = strdup(src->bounds[i].end.variable_name);
            if (!bounds->bounds[i].end.variable_name) {
                symtable_destroy_bounds(bounds);
                return NULL;
            }
        }
    }

    return bounds;
}

Symbol* symtable_add_array(SymbolTable* table, const char* name, const char* elem_type, ArrayBoundsData* bounds) {
    verbose_print("\n=== ADDING ARRAY TO SYMBOL TABLE ===\n");
    verbose_print("Array name: %s\n", name);
    verbose_print("Element type: %s\n", elem_type);
    
    if (!table || !name || !elem_type) {
        verbose_print("Invalid input parameters\n");
        return NULL;
    }

    // Check if variable already exists in current scope
    unsigned int h = hash(name);
    Symbol* existing = table->current->symbols[h];
    while (existing) {
        if (strcmp(existing->name, name) == 0) {
            error_report(ERROR_SEMANTIC, SEVERITY_ERROR,
                        (SourceLocation){0, 0, "internal"},
                        "Variable '%s' already declared in current scope", name);
            return NULL;
        }
        existing = existing->next;
    }

    // Create the symbol
    Symbol* symbol = symbol_create(name, SYMBOL_VARIABLE);
    if (!symbol) {
        verbose_print("Failed to create symbol\n");
        return NULL;
    }

    // Build full type string
    char* full_type = malloc(strlen(elem_type) + 32);
    if (!full_type) {
        symbol_destroy(symbol);
        return NULL;
    }

    int dim_count = bounds ? bounds->dimensions : 1;
    strcpy(full_type, "");
    for (int i = 0; i < dim_count; i++) {
        strcat(full_type, "array of ");
    }
    strcat(full_type, elem_type);

    // Initialize array info
    symbol->info.var.type = full_type;
    symbol->info.var.is_array = true;
    symbol->info.var.bounds = bounds ? symtable_clone_bounds(bounds) : NULL;
    symbol->info.var.dimensions = bounds ? bounds->dimensions : 1;
    symbol->info.var.has_dynamic_size = false;

    // Check for dynamic size
    if (bounds) {
        verbose_print("Array has %d dimensions\n", bounds->dimensions);
        for (int i = 0; i < bounds->dimensions; i++) {
            if (!bounds->bounds[i].start.is_constant || 
                !bounds->bounds[i].end.is_constant) {
                symbol->info.var.has_dynamic_size = true;
                verbose_print("Dimension %d has dynamic size\n", i);
            }
            if (bounds->bounds[i].using_range) {
                verbose_print("Dimension %d uses range\n", i);
            }
        }
    }

    // Add to current scope
    symbol->scope = table->current;
    symbol->next = table->current->symbols[h];
    table->current->symbols[h] = symbol;
    table->current->symbol_count++;

    verbose_print("Successfully added array to symbol table\n");
    verbose_print("Current scope: %s\n", 
                 table->current == table->global ? "global" : "local");
    verbose_print("=== FINISHED ADDING ARRAY TO SYMBOL TABLE ===\n\n");

    if (table->current->type == SCOPE_FUNCTION || 
        (table->current->type == SCOPE_BLOCK && table->current->function_name)) {
        const char* func_name = table->current->function_name;
        symtable_add_local_to_function(table, func_name, symbol);
    }

    debug_symbol_create(symbol, "adding new array to current scope");
    debug_symbol_table_operation("Variable Added Successfully", name);

    return symbol;
}

static void debug_print_bounds(const char* context, Symbol* sym) {
    if (!sym) {
        verbose_print("%s: NULL symbol\n", context);
        return;
    }

    verbose_print("%s: Symbol '%s' ", context, sym->name);
    if (!sym->info.var.is_array) {
        verbose_print("is not an array\n");
        return;
    }

    verbose_print("is array with %d dimensions\n", sym->info.var.dimensions);
    
    if (!sym->info.var.bounds) {
        verbose_print("  No bounds information stored\n");
        return;
    }

    for (int i = 0; i < sym->info.var.dimensions; i++) {
        DimensionBounds* bound = &sym->info.var.bounds->bounds[i];
        verbose_print("  Dimension %d:\n", i);
        verbose_print("    Using range: %s\n", bound->using_range ? "yes" : "no");
        
        // Print start bound
        verbose_print("    Start bound: ");
        if (bound->start.is_constant) {
            verbose_print("constant %ld\n", bound->start.constant_value);
        } else {
            verbose_print("variable %s\n", 
                bound->start.variable_name ? bound->start.variable_name : "NULL");
        }
        
        // Print end bound
        verbose_print("    End bound: ");
        if (bound->end.is_constant) {
            verbose_print("constant %ld\n", bound->end.constant_value);
        } else {
            verbose_print("variable %s\n", 
                bound->end.variable_name ? bound->end.variable_name : "NULL");
        }
    }
}

void symtable_update_parameter_bounds_in_global(SymbolTable* table, const char* param_name, ArrayBoundsData* bounds) {
    if (!table || !param_name || !bounds || !table->current->function_name) {
        return;
    }

    verbose_print("Updating bounds for parameter %s in function %s\n", 
                 param_name, table->current->function_name);

    // Find function in global scope
    unsigned int h = hash(table->current->function_name);
    Symbol* func = table->global->symbols[h];
    while (func) {
        if (strcmp(func->name, table->current->function_name) == 0) {
            // Find parameter in function's parameter list
            for (int i = 0; i < func->info.func.param_count; i++) {
                Symbol* param = func->info.func.parameters[i];
                if (strcmp(param->name, param_name) == 0) {
                    // Update bounds in global copy
                    if (param->info.var.bounds) {
                        symtable_destroy_bounds(param->info.var.bounds);
                    }
                    param->info.var.bounds = symtable_clone_bounds(bounds);
                    param->info.var.dimensions = bounds->dimensions;
                    verbose_print("Successfully updated bounds for parameter %s in global scope\n", param_name);
                    debug_symbol_bounds_update(param, bounds, "updating symbol array bounds");
                    return;
                }
            }
            break;
        }
        func = func->next;
    }
}

void symtable_add_local_to_function(SymbolTable* table, const char* function_name, Symbol* local_var) {
    if (!table || !function_name || !local_var) return;

    verbose_print("Adding local variable %s to function %s\n", 
                 local_var->name, function_name);

    // Find function in global scope
    unsigned int h = hash(function_name);
    Symbol* func = table->global->symbols[h];
    while (func) {
        if (strcmp(func->name, function_name) == 0 && 
            (func->kind == SYMBOL_FUNCTION || func->kind == SYMBOL_PROCEDURE)) {
            // Create new local variables array or extend existing one
            Symbol** new_locals = realloc(func->info.func.local_variables,
                                        (func->info.func.local_var_count + 1) * sizeof(Symbol*));
            if (new_locals) {
                func->info.func.local_variables = new_locals;
                
                // Create a copy of the local variable
                Symbol* local_copy = symbol_create(local_var->name, local_var->kind);
                if (local_copy) {
                    // Copy all variable info
                    memcpy(&local_copy->info, &local_var->info, sizeof(local_var->info));
                    if (local_var->info.var.is_array && local_var->info.var.bounds) {
                        local_copy->info.var.bounds = symtable_clone_bounds(local_var->info.var.bounds);
                    }
                    local_copy->scope = local_var->scope;
                    
                    // Store in function's local variables list
                    func->info.func.local_variables[func->info.func.local_var_count++] = local_copy;
                    verbose_print("Successfully added local variable to function\n");
                    
                    // Debug print bounds if it's an array
                    if (local_copy->info.var.is_array && local_copy->info.var.bounds) {
                        debug_print_bounds("Local variable copy", local_copy);
                    }
                }
            }
            break;
        }
        func = func->next;
    }

    debug_symbol_create(local_var, "adding new local variable to function in global scope");
    debug_symbol_table_operation("Variable Added Successfully", local_var->name);
}


Symbol* symtable_add_parameter(SymbolTable* table, const char* name, const char* type, const char* mode, ASTNode* node, bool needs_deref) {
    verbose_print("Adding parameter: %s (type: %s, mode: %s)\n", name, type, mode);
    verbose_print("Current scope type: %s\n", 
                 table->current->type == SCOPE_FUNCTION ? "FUNCTION" :
                 table->current->type == SCOPE_GLOBAL ? "GLOBAL" : "OTHER");
    verbose_print("Current function: %s\n", 
                 table->current->function_name ? table->current->function_name : "NULL");
    Symbol* param = symbol_create(name, SYMBOL_PARAMETER);
    if (!param) return NULL;

    param->node = node;
    param->info.var.needs_type_declaration = (type == NULL);

    if (type)
        param->info.var.type = strdup(type);
    param->info.var.is_parameter = true;
    param->info.var.param_mode = strdup(mode);
    
    bool is_array = false;
    // Set array flag if type starts with "array of"
    if (type && strstr(type, "array of") == type) {
        param->info.var.is_array = true;
        is_array = true;
        // Count dimensions
        int dimensions = 0;
        const char* ptr = type;
        while (strstr(ptr, "array of") == ptr) {
            dimensions++;
            ptr += 9;  // Length of "array of "
        }
        param->info.var.dimensions = dimensions;
    }

    if (is_array || !needs_deref) {
        param->info.var.needs_deref = false;
    } else if (strcasecmp(mode, "out") == 0 || strcasecmp(mode, "inout") == 0) {
        param->info.var.needs_deref = true;
    }

    // Add to current scope (for local use)
    unsigned int h = hash(name);
    param->scope = table->current;
    param->next = table->current->symbols[h];
    table->current->symbols[h] = param;

    // Store reference to the parameter for later update
    Symbol* global_param_copy = NULL;
    
    // Also add to function's parameter list
    if (table->current->function_name) {
        verbose_print("Looking for function %s in global scope\n", table->current->function_name);
        h = hash(table->current->function_name);
        Symbol* func = table->global->symbols[h];
        while (func) {
            if (strcmp(func->name, table->current->function_name) == 0 &&
                (func->kind == SYMBOL_FUNCTION || func->kind == SYMBOL_PROCEDURE)) {
                verbose_print("Found function, current param count: %d\n", func->info.func.param_count);
                
                // Create new parameter array or extend existing one
                Symbol** new_params = realloc(func->info.func.parameters, (func->info.func.param_count + 1) * sizeof(Symbol*));
                if (new_params) {
                    func->info.func.parameters = new_params;
                    // Store a copy of the parameter in the function's parameter list
                    Symbol* global_param_copy = symbol_create(name, SYMBOL_PARAMETER);
                    if (global_param_copy) {
                        deep_copy_var_info(&global_param_copy->info.var, &param->info.var);
                        if (type)
                            global_param_copy->info.var.type = strdup(type);
                        global_param_copy->scope = param->scope;
                        global_param_copy->node = node;
                        global_param_copy->info.var.needs_type_declaration = (type == NULL);
                        
                        if (is_array || !needs_deref) {
                            global_param_copy->info.var.needs_deref = false;
                        } else if (strcasecmp(mode, "out") == 0 || strcasecmp(mode, "inout") == 0) {
                            global_param_copy->info.var.needs_deref = true;
                        }

                        // Store in function's parameter list
                        func->info.func.parameters[func->info.func.param_count++] = global_param_copy;
                        verbose_print("Added parameter to function's parameter list (new count: %d)\n",
                                    func->info.func.param_count);
                        
                        // Debug bounds information
                        if (global_param_copy->info.var.is_array && global_param_copy->info.var.bounds) {
                            verbose_print("Parameter copy has bounds with %d dimensions\n",
                                        global_param_copy->info.var.dimensions);
                        }
                    }
                }
                break;
            }
            func = func->next;
        }
    }

    debug_symbol_create(param, "adding new parameter to local scope and to function in global scope");
    debug_symbol_table_operation("Variable Added Successfully", param->name);
    return param;
}

Symbol* symtable_lookup_parameter(SymbolTable* table, const char* function_name, const char* param_name) {
    if (!table || !function_name || !param_name) return NULL;
    verbose_print("Looking up parameter %s in function %s\n", param_name, function_name);

    // Find function in global scope
    unsigned int h = hash(function_name);
    Symbol* func = table->global->symbols[h];
    while (func) {
        if (strcmp(func->name, function_name) == 0 && 
            (func->kind == SYMBOL_FUNCTION || func->kind == SYMBOL_PROCEDURE)) {
            verbose_print("Found function %s with %d parameters\n", 
                         function_name, func->info.func.param_count);
            
            // Search through function parameters
            for (int i = 0; i < func->info.func.param_count; i++) {
                Symbol* param = func->info.func.parameters[i];
                if (param && strcmp(param->name, param_name) == 0) {
                    verbose_print("Found parameter %s\n", param_name);
                    return param;
                }
            }
            verbose_print("Parameter %s not found in function's parameter list\n", param_name);
            break;
        }
        func = func->next;
    }
    verbose_print("Function %s not found in global scope\n", function_name);
    return NULL;
}

Symbol* symtable_lookup(SymbolTable* table, const char* name) {

    verbose_print("Looking up symbol %s in global scope\n", name);
    if (!table || !name) {
        debug_symbol_table_operation("Lookup Failed", "Invalid parameters");
        return NULL;
    }

    unsigned int h = hash(name);
    Scope* scope = table->current;

    // Search through all scopes starting from current
    while (scope) {
        Symbol* symbol = scope->symbols[h];
        while (symbol) {
            if (strcmp(symbol->name, name) == 0) {
                debug_symbol_lookup(name, symbol, "found in scope");
                return symbol;
            }
            symbol = symbol->next;
        }
        scope = scope->parent;
    }

    verbose_print("symbol %s not found in global scope\n", name);

    debug_symbol_lookup(name, NULL, "symbol not found");
    return NULL;
}

Symbol* symtable_lookup_global(SymbolTable* table, const char* name) {
    if (!table || !name || !table->global) return NULL;
    
    verbose_print("Looking up symbol %s in global scope only\n", name);
    unsigned int h = hash(name);
    Symbol* symbol = table->global->symbols[h];
    
    while (symbol) {
        if (strcmp(symbol->name, name) == 0) {
            return symbol;
        }
        symbol = symbol->next;
    }
    return NULL;
}

Symbol* symtable_lookup_current_scope(SymbolTable* table, const char* name) {
    if (!table || !name) return NULL;

    unsigned int h = hash(name);
    Symbol* symbol = table->current->symbols[h];
    while (symbol) {
        if (strcmp(symbol->name, name) == 0) {
            return symbol;
        }
        symbol = symbol->next;
    }

    return NULL;
}

bool symtable_is_type_compatible(const char* type1, const char* type2) {
    if (!type1 || !type2) return false;
    if (strcmp(type1, type2) == 0) return true;

    // Special cases for type compatibility
    if (strcmp(type1, "real") == 0 && strcmp(type2, "integer") == 0) return true;
    if (strcmp(type1, "integer") == 0 && strcmp(type2, "real") == 0) return true;

    return false;
}

void symtable_print_current_scope(SymbolTable* table) {
    if (!table || !table->current) return;

    verbose_print("Current Scope (level %d):\n", table->scope_level);
    for (int i = 0; i < HASH_SIZE; i++) {
        Symbol* symbol = table->current->symbols[i];
        while (symbol) {
            verbose_print("  %s: ", symbol->name);
            switch (symbol->kind) {
                case SYMBOL_VARIABLE:
                    verbose_print("Variable (type: %s%s)\n", 
                           symbol->info.var.type,
                           symbol->info.var.is_array ? "[]" : "");
                    break;
                case SYMBOL_FUNCTION:
                    verbose_print("Function (returns: %s)\n",
                           symbol->info.func.return_type ?
                           symbol->info.func.return_type : "void");
                    break;
                default:
                    verbose_print("Unknown symbol kind\n");
            }
            symbol = symbol->next;
        }
    }
}

void symtable_debug_dump_symbol(Symbol* sym, int level) {
    if (!sym) return;
    
    for (int i = 0; i < level; i++) verbose_print("  ");
    
    verbose_print("Symbol: %s\n", sym->name);
    for (int i = 0; i < level; i++) verbose_print("  ");
    verbose_print("  Kind: %s\n", 
           sym->kind == SYMBOL_FUNCTION ? "FUNCTION" :
           sym->kind == SYMBOL_PROCEDURE ? "PROCEDURE" :
           sym->kind == SYMBOL_VARIABLE ? "VARIABLE" :
           sym->kind == SYMBOL_PARAMETER ? "PARAMETER" : "UNKNOWN");
           
    if (sym->kind == SYMBOL_FUNCTION || sym->kind == SYMBOL_PROCEDURE) {
        for (int i = 0; i < level; i++) verbose_print("  ");
        verbose_print("  Parameter count: %d\n", sym->info.func.param_count);
        for (int i = 0; i < sym->info.func.param_count; i++) {
            symtable_debug_dump_symbol(sym->info.func.parameters[i], level + 2);
        }
    }
    
    if (sym->kind == SYMBOL_VARIABLE || sym->kind == SYMBOL_PARAMETER) {
        for (int i = 0; i < level; i++) verbose_print("  ");
        verbose_print("  Type: %s\n", sym->info.var.type);
        for (int i = 0; i < level; i++) verbose_print("  ");
        verbose_print("  Is Array: %s\n", sym->info.var.is_array ? "yes" : "no");
        if (sym->info.var.is_array && sym->info.var.bounds) {
            for (int i = 0; i < level; i++) verbose_print("  ");
            verbose_print("  Dimensions: %d\n", sym->info.var.dimensions);
            for (int dim = 0; dim < sym->info.var.dimensions; dim++) {
                for (int i = 0; i < level; i++) verbose_print("  ");
                verbose_print("    Dimension %d:\n", dim);
                DimensionBounds* bound = &sym->info.var.bounds->bounds[dim];
                for (int i = 0; i < level; i++) verbose_print("  ");
                verbose_print("      Range: %s\n", bound->using_range ? "yes" : "no");
                for (int i = 0; i < level; i++) verbose_print("  ");
                verbose_print("      Start: %s%s\n", 
                       bound->start.is_constant ? "constant " : "variable ",
                       bound->start.is_constant ? 
                           (char[]){bound->start.constant_value + '0', 0} : 
                           bound->start.variable_name);
                for (int i = 0; i < level; i++) verbose_print("  ");
                verbose_print("      End: %s%s\n", 
                       bound->end.is_constant ? "constant " : "variable ",
                       bound->end.is_constant ? 
                           (char[]){bound->end.constant_value + '0', 0} : 
                           bound->end.variable_name);
            }
        }
    }
}

void record_add_field(ASTNode* child, RecordTypeData* record_data) {
    size_t new_capacity = (record_data->field_count + 1) * 2;
    RecordField** new_fields = (RecordField**)realloc(record_data->fields, new_capacity * sizeof(RecordField*));
    if (!new_fields) {
        error_report(ERROR_INTERNAL, SEVERITY_ERROR, child->loc,
                    "Failed to allocate memory for record fields");
        return;
    }
    record_data->fields = new_fields;

    record_data->fields[record_data->field_count] = malloc(sizeof(RecordField));
    record_data->fields[record_data->field_count]->record_type = malloc(sizeof(RecordTypeData));
    *record_data->fields[record_data->field_count]->record_type = child->record_type;
    if (child->data.value)
        record_data->fields[record_data->field_count]->record_type->name = child->data.value;
    record_data->field_count++;
}

void recursive_add_field(ASTNode* parent, RecordTypeData* record_data) {
    for (int i = 0; i < parent->child_count; ++i) {
        record_add_field(parent->children[i], record_data);
        recursive_add_field(parent->children[i], record_data->fields[i]->record_type);
    }
}

Symbol* symtable_add_type(SymbolTable* table, const char* name, ASTNode* record) {
    if (!table || !name || !record) return NULL;

    RecordTypeData* record_type = &record->record_type;
    Symbol* symbol = symbol_create(name, SYMBOL_TYPE);
    if (!symbol) return NULL;

    symbol->info.record = *record_type;
    recursive_add_field(record, &symbol->info.record);
    
    // Add to current scope
    unsigned int h = hash(name);
    symbol->scope = table->current;
    symbol->next = table->current->symbols[h];
    table->current->symbols[h] = symbol;
    table->current->symbol_count++;

    return symbol;
}

RecordTypeData* symtable_lookup_type(SymbolTable* table, const char* name) {
    Symbol* sym = symtable_lookup(table, name);
    if (sym && sym->kind == SYMBOL_TYPE) {
        return &sym->info.record;
    }
    return NULL;
}

void symtable_debug_dump_scope(Scope* scope, int level) {
    if (!scope) return;
    
    for (int i = 0; i < level; i++) verbose_print("  ");
    verbose_print("Scope: %s\n", 
           scope->type == SCOPE_GLOBAL ? "GLOBAL" :
           scope->type == SCOPE_FUNCTION ? "FUNCTION" :
           scope->type == SCOPE_BLOCK ? "BLOCK" : "UNKNOWN");
    
    if (scope->function_name) {
        for (int i = 0; i < level; i++) verbose_print("  ");
        verbose_print("Function name: %s\n", scope->function_name);
    }
    
    for (int i = 0; i < HASH_SIZE; i++) {
        Symbol* sym = scope->symbols[i];
        while (sym) {
            symtable_debug_dump_symbol(sym, level + 1);
            sym = sym->next;
        }
    }
}

void symtable_debug_dump_all(SymbolTable* table) {
    if (!table) return;
    
    verbose_print("\n=== SYMBOL TABLE DUMP ===\n");
    verbose_print("Current scope level: %d\n\n", table->scope_level);
    
    // Dump global scope
    verbose_print("=== GLOBAL SCOPE ===\n");
    symtable_debug_dump_scope(table->global, 0);
    
    // Dump current scope if different from global
    if (table->current != table->global) {
        verbose_print("\n=== CURRENT SCOPE ===\n");
        symtable_debug_dump_scope(table->current, 0);
    }
    verbose_print("\n=== END SYMBOL TABLE DUMP ===\n\n");
}