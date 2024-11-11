#include "debug.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DebugFlags current_flags = 0;
FILE* debug_file = NULL;
FILE* lexer_debug_file = NULL;
FILE* parser_debug_file = NULL;
FILE* ast_debug_file = NULL;
FILE* symbol_debug_file = NULL;
FILE* codegen_debug_file = NULL;

int trace_depth = 0;

static int ast_node_id = 0;
static int symbol_node_id = 0;
static int codegen_node_id = 0;

void debug_init(void) {
    debug_file = fopen("logs/debug.log", "w");
    lexer_debug_file = fopen("logs/lexer_debug.log", "w");
    parser_debug_file = fopen("logs/parser_debug.log", "w");
    ast_debug_file = fopen("logs/ast_debug.log", "w");
    symbol_debug_file = fopen("logs/symbol_debug.log", "w");
    codegen_debug_file = fopen("logs/codegen_debug.log", "w");
    if (!debug_file) {
        fprintf(stderr, "Warning: Could not open debug.log\n");
        debug_file = stderr;
    }
    if (!lexer_debug_file) {
        fprintf(stderr, "Warning: Could not open lexer_debug.log\n");
    }
    if (!parser_debug_file) {
        fprintf(stderr, "Warning: Could not open parser_debug.log\n");
    }
    if (!ast_debug_file) {
        fprintf(stderr, "Warning: Could not open ast_debug.log\n");
    }
    if (!symbol_debug_file) {
        fprintf(stderr, "Warning: Could not open symbol_debug.log\n");
    }
    if (!codegen_debug_file) {
        fprintf(stderr, "Warning: Could not open codegen_debug.log\n");
    }
    
}

void debug_set_flags(DebugFlags flags) {
    current_flags = flags;
}

void debug_enable(DebugFlags flag) {
    current_flags |= flag;
}

void debug_disable(DebugFlags flag) {
    current_flags &= ~flag;
}

static void print_indent_to(int indent, FILE* dest) {
    for (int i = 0; i < indent; i++) {
        fprintf(dest, "  ");
    }
}

static void print_indent(int indent) {
    /*for (int i = 0; i < indent; i++) {
        fprintf(debug_file, "  ");
    }*/
   print_indent_to(indent, debug_file);
}

void debug_print_token_to(Token* token, FILE* dest) {
    if (!(current_flags & DEBUG_LEXER)) return;

    fprintf(dest, "Token{type=%s, value='%s', line=%d, col=%d}\n",
            token_type_to_string(token->type),
            token->value ? token->value : "NULL",
            token->loc.line,
            token->loc.column);
}

void debug_print_token(Token* token) {
    /*if (!(current_flags & DEBUG_LEXER)) return;

    fprintf(debug_file, "Token{type=%s, value='%s', line=%d, col=%d}\n",
            token_type_to_string(token->type),
            token->value ? token->value : "NULL",
            token->loc.line,
            token->loc.column);*/
    debug_print_token_to(token, debug_file);
}

const char* ast_node_type_to_string_detailed(NodeType type) {
    switch (type) {
        case NODE_PROGRAM:
            return "Program";
        case NODE_FUNCTION:
            return "Function Declaration";
        case NODE_PROCEDURE:
            return "Procedure Declaration";
        case NODE_PARAMETER:
            return "Parameter Declaration";
        case NODE_PARAMETER_LIST:
            return "Parameter List";
        case NODE_VARIABLE:
            return "Variable Reference";
        case NODE_VAR_DECL:
            return "Variable Declaration";
        case NODE_ARRAY_DECL:
            return "Array Declaration";
        case NODE_BLOCK:
            return "Code Block";
        case NODE_ASSIGNMENT:
            return "Assignment Statement";
        case NODE_IF:
            return "If Statement";
        case NODE_WHILE:
            return "While Loop";
        case NODE_FOR:
            return "For Loop";
        case NODE_REPEAT:
            return "Repeat Loop";
        case NODE_RETURN:
            return "Return Statement";
        case NODE_BINARY_OP:
            return "Binary Operation";
        case NODE_UNARY_OP:
            return "Unary Operation";
        case NODE_ARRAY_ACCESS:
            return "Array Access";
        case NODE_IDENTIFIER:
            return "Identifier";
        case NODE_NUMBER:
            return "Number Literal";
        case NODE_STRING:
            return "String Literal";
        case NODE_BOOL:
            return "Boolean Literal";
        case NODE_CALL:
            return "Function/Procedure Call";
        case NODE_PRINT:
            return "Print Statement";
        case NODE_READ:
            return "Read Statement";
        case NODE_TYPE:
            return "Type Specifier";
        default:
            return "Unknown Node Type";
    }
}

void debug_print_ast_to(ASTNode* node, int indent, bool force, FILE* dest) {
    if (!node || (!(current_flags & DEBUG_AST) && !force)) return;

    print_indent_to(indent, dest);
    fprintf(dest, "Node {\n");
    indent++;

    print_indent_to(indent, dest);
    fprintf(dest, "Type: %s\n", ast_node_type_to_string_detailed(node->type));

    // Print node-specific information
    switch (node->type) {
        case NODE_FUNCTION:
        case NODE_PROCEDURE:
            print_indent_to(indent, dest);
            fprintf(dest, "Name: %s\n", node->data.function.name);
            if (node->data.function.return_type) {
                print_indent_to(indent, dest);
                fprintf(dest, "Return Type: %s\n", node->data.function.return_type);
            }
            if (node->data.function.is_pointer) {
                print_indent_to(indent, dest);
                fprintf(dest, "Pointer Level: %d\n", node->data.function.pointer_level);
            }
            break;
        case NODE_VARIABLE:
        case NODE_VAR_DECL:
        case NODE_ARRAY_DECL:
            print_indent_to(indent, dest);
            fprintf(dest, "Name: %s\n", node->data.variable.name);
            print_indent_to(indent, dest);
            fprintf(dest, "Type: %s\n", node->data.variable.type ? node->data.variable.type : "<pending>");
            if (node->data.variable.is_array) {
                print_indent_to(indent, dest);
                fprintf(dest, "Dimensions: %d\n", node->data.variable.array_info.dimensions);
                if (node->data.variable.array_info.bounds) {
                    print_indent_to(indent, dest);
                    fprintf(dest, "Has Bounds Information: yes\n");
                }
            }
            if (node->data.variable.is_pointer) {
                print_indent_to(indent, dest);
                fprintf(dest, "Pointer Level: %d\n", node->data.variable.pointer_level);
            }
            break;
        case NODE_BINARY_OP:
            print_indent_to(indent, dest);
            fprintf(dest, "Operator: %s\n", token_type_to_string(node->data.binary_op.op));
            break;
        case NODE_UNARY_OP:
            print_indent_to(indent, dest);
            fprintf(dest, "Operator: %s\n", token_type_to_string(node->data.unary_op.op));
            if (node->data.unary_op.op == TOK_DEREF) {
                print_indent_to(indent, dest);
                fprintf(dest, "Dereference Count: %d\n", node->data.unary_op.deref_count);
            }
            break;
        case NODE_ARRAY_ACCESS:
            print_indent_to(indent, dest);
            fprintf(dest, "Dimensions: %d\n", node->data.array_access.dimensions);
            break;
        case NODE_NUMBER:
        case NODE_IDENTIFIER:
        case NODE_STRING:
            print_indent_to(indent, dest);
            fprintf(dest, "Value: %s\n", node->data.value);
            break;
    }
    print_indent_to(indent, dest);
    fprintf(dest, "Location: %d:%d\n", node->loc.line, node->loc.column);

    // Print children count and recursively print children
    if (node->child_count > 0) {
        print_indent_to(indent, dest);
        fprintf(dest, "Children (%d):\n", node->child_count);
        for (int i = 0; i < node->child_count; i++) {
            debug_print_ast(node->children[i], indent + 1, force);
        }
    }

    indent--;
    print_indent_to(indent, dest);
    fprintf(dest, "}\n");
}

void debug_print_ast(ASTNode* node, int indent, bool force) {
    /*if (!node || (!(current_flags & DEBUG_AST) && !force)) return;

    print_indent(indent);
    fprintf(debug_file, "Node {\n");
    indent++;

    print_indent(indent);
    fprintf(debug_file, "Type: %s\n", ast_node_type_to_string_detailed(node->type));

    // Print node-specific information
    switch (node->type) {
        case NODE_FUNCTION:
        case NODE_PROCEDURE:
            print_indent(indent);
            fprintf(debug_file, "Name: %s\n", node->data.function.name);
            if (node->data.function.return_type) {
                print_indent(indent);
                fprintf(debug_file, "Return Type: %s\n", node->data.function.return_type);
            }
            if (node->data.function.is_pointer) {
                print_indent(indent);
                fprintf(debug_file, "Pointer Level: %d\n", node->data.function.pointer_level);
            }
            break;
        case NODE_VARIABLE:
        case NODE_VAR_DECL:
        case NODE_ARRAY_DECL:
            print_indent(indent);
            fprintf(debug_file, "Name: %s\n", node->data.variable.name);
            print_indent(indent);
            fprintf(debug_file, "Type: %s\n", node->data.variable.type ? node->data.variable.type : "<pending>");
            if (node->data.variable.is_array) {
                print_indent(indent);
                fprintf(debug_file, "Dimensions: %d\n", node->data.variable.array_info.dimensions);
                if (node->data.variable.array_info.bounds) {
                    print_indent(indent);
                    fprintf(debug_file, "Has Bounds Information: yes\n");
                }
            }
            if (node->data.variable.is_pointer) {
                print_indent(indent);
                fprintf(debug_file, "Pointer Level: %d\n", node->data.variable.pointer_level);
            }
            break;
        case NODE_BINARY_OP:
            print_indent(indent);
            fprintf(debug_file, "Operator: %s\n", token_type_to_string(node->data.binary_op.op));
            break;
        case NODE_UNARY_OP:
            print_indent(indent);
            fprintf(debug_file, "Operator: %s\n", token_type_to_string(node->data.unary_op.op));
            if (node->data.unary_op.op == TOK_DEREF) {
                print_indent(indent);
                fprintf(debug_file, "Dereference Count: %d\n", node->data.unary_op.deref_count);
            }
            break;
        case NODE_ARRAY_ACCESS:
            print_indent(indent);
            fprintf(debug_file, "Dimensions: %d\n", node->data.array_access.dimensions);
            break;
        case NODE_NUMBER:
        case NODE_IDENTIFIER:
        case NODE_STRING:
            print_indent(indent);
            fprintf(debug_file, "Value: %s\n", node->data.value);
            break;
    }
    print_indent(indent);
    fprintf(debug_file, "Location: %d:%d\n", node->loc.line, node->loc.column);

    // Print children count and recursively print children
    if (node->child_count > 0) {
        print_indent(indent);
        fprintf(debug_file, "Children (%d):\n", node->child_count);
        for (int i = 0; i < node->child_count; i++) {
            debug_print_ast(node->children[i], indent + 1, force);
        }
    }

    indent--;
    print_indent(indent);
    fprintf(debug_file, "}\n");*/
    debug_print_ast_to(node, indent, force, debug_file);
}

/*void debug_trace_enter(const char* function_name) {
    if (!(current_flags & DEBUG_PARSER)) return;

    print_indent(trace_depth);
    fprintf(debug_file, "-> %s\n", function_name);
    trace_depth++;
}

void debug_trace_exit(const char* function_name, ASTNode* result) {
    if (!(current_flags & DEBUG_PARSER)) return;

    trace_depth--;
    print_indent(trace_depth);
    fprintf(debug_file, "<- %s: %s\n", function_name,
            result ? ast_node_type_to_string(result) : "NULL");
}*/

void debug_print_error_context(SourceLocation loc) {
    FILE* source = fopen(loc.filename, "r");
    if (!source) return;

    char line[1024];
    int current_line = 1;

    // Print a few lines before the error
    while (current_line <= loc.line && fgets(line, sizeof(line), source)) {
        if (current_line >= loc.line - 2) {
            fprintf(debug_file, "%4d | %s", current_line, line);
        }
        current_line++;
    }

    // Print error indicator
    fprintf(debug_file, "     | ");
    for (int i = 1; i < loc.column; i++) {
        fprintf(debug_file, " ");
    }
    fprintf(debug_file, "^\n");

    fclose(source);
}

// More detailed symbol table debugging
void debug_print_symbol_table(SymbolTable* table) {
    if (!(current_flags & DEBUG_SYMBOLS)) return;
    
    fprintf(debug_file, "=== Symbol Table ===\n");
    if (table->current == table->global) {
        fprintf(debug_file, "Current Scope: Global\n");
    } else {
        fprintf(debug_file, "Current Scope: Local (Level %d)\n", table->scope_level);
    }
    
    debug_print_symbol_scope(table->global, 0);
}

void debug_print_symbol_scope(Scope* scope, int indent) {
    if (!scope) return;
    
    for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
    fprintf(debug_file, "Scope Type: %s\n", 
            scope->type == SCOPE_GLOBAL ? "Global" :
            scope->type == SCOPE_FUNCTION ? "Function" : "Block");
    
    // Print all symbols in this scope
    for (int i = 0; i < HASH_SIZE; i++) {
        for (Symbol* sym = scope->symbols[i]; sym; sym = sym->next) {
            debug_print_symbol(sym, indent + 1);
        }
    }
}

void debug_print_symbol_to(Symbol* sym, int indent, FILE* dest) {
    if (!sym) return;
    
    // Print indentation
    for (int i = 0; i < indent; i++) {
        fprintf(dest, "  ");
    }
    
    fprintf(dest, "Symbol '%s':\n", sym->name);
    
    // Indent details
    indent++;
    for (int i = 0; i < indent; i++) {
        fprintf(dest, "  ");
    }
    
    // Print kind-specific information
    switch (sym->kind) {
        case SYMBOL_FUNCTION:
        case SYMBOL_PROCEDURE:
            fprintf(dest, "Kind: %s\n", sym->kind == SYMBOL_FUNCTION ? "Function" : "Procedure");
            for (int i = 0; i < indent; i++) fprintf(dest, "  ");
            fprintf(dest, "Return Type: %s\n", 
                    sym->info.func.return_type ? sym->info.func.return_type : "void");
            for (int i = 0; i < indent; i++) fprintf(dest, "  ");
            fprintf(dest, "Parameter Count: %d\n", sym->info.func.param_count);
            
            // Print parameters
            if (sym->info.func.param_count > 0) {
                for (int i = 0; i < indent; i++) fprintf(dest, "  ");
                fprintf(dest, "Parameters:\n");
                for (int i = 0; i < sym->info.func.param_count; i++) {
                    debug_print_symbol(sym->info.func.parameters[i], indent + 1);
                }
            }
            
            // Print local variables
            if (sym->info.func.local_var_count > 0) {
                for (int i = 0; i < indent; i++) fprintf(dest, "  ");
                fprintf(dest, "Local Variables:\n");
                for (int i = 0; i < sym->info.func.local_var_count; i++) {
                    debug_print_symbol(sym->info.func.local_variables[i], indent + 1);
                }
            }
            break;
            
        case SYMBOL_VARIABLE:
            fprintf(dest, "Kind: Variable\n");
            for (int i = 0; i < indent; i++) fprintf(dest, "  ");
            fprintf(dest, "Type: %s\n", sym->info.var.type);
            
            if (sym->info.var.is_pointer) {
                for (int i = 0; i < indent; i++) fprintf(dest, "  ");
                fprintf(dest, "Pointer Level: %d\n", sym->info.var.pointer_level);
            }
            
            if (sym->info.var.is_array) {
                for (int i = 0; i < indent; i++) fprintf(dest, "  ");
                fprintf(dest, "Array Dimensions: %d\n", sym->info.var.dimensions);
                
                if (sym->info.var.bounds) {
                    for (int dim = 0; dim < sym->info.var.dimensions; dim++) {
                        for (int i = 0; i < indent; i++) fprintf(dest, "  ");
                        fprintf(dest, "Dimension %d:\n", dim + 1);
                        
                        DimensionBounds* bound = &sym->info.var.bounds->bounds[dim];
                        indent++;
                        
                        // Print start bound
                        for (int i = 0; i < indent; i++) fprintf(dest, "  ");
                        if (bound->start.is_constant) {
                            fprintf(dest, "Start: constant %ld\n", bound->start.constant_value);
                        } else {
                            fprintf(dest, "Start: variable %s\n", 
                                   bound->start.variable_name ? bound->start.variable_name : "<null>");
                        }
                        
                        // Print end bound
                        for (int i = 0; i < indent; i++) fprintf(dest, "  ");
                        if (bound->end.is_constant) {
                            fprintf(dest, "End: constant %ld\n", bound->end.constant_value);
                        } else {
                            fprintf(dest, "End: variable %s\n", 
                                   bound->end.variable_name ? bound->end.variable_name : "<null>");
                        }
                        
                        // Print range info
                        for (int i = 0; i < indent; i++) fprintf(dest, "  ");
                        fprintf(dest, "Uses Range: %s\n", bound->using_range ? "yes" : "no");
                        
                        indent--;
                    }
                }
            }
            
            if (sym->info.var.is_parameter) {
                for (int i = 0; i < indent; i++) fprintf(dest, "  ");
                fprintf(dest, "Parameter Mode: %s\n", sym->info.var.param_mode);
            }
            break;
            
        case SYMBOL_PARAMETER:
            fprintf(dest, "Kind: Parameter\n");
            for (int i = 0; i < indent; i++) fprintf(dest, "  ");
            fprintf(dest, "Type: %s\n", sym->info.var.type);
            for (int i = 0; i < indent; i++) fprintf(dest, "  ");
            fprintf(dest, "Mode: %s\n", sym->info.var.param_mode);
            if (sym->info.var.needs_type_declaration) {
                for (int i = 0; i < indent; i++) fprintf(dest, "  ");
                fprintf(dest, "Needs Type Declaration: yes\n");
            }
            break;
            
        case SYMBOL_TYPE:
            fprintf(dest, "Kind: Type\n");
            break;
            
        default:
            fprintf(dest, "Kind: Unknown\n");
    }
    
    // Print scope information
    if (sym->scope) {
        for (int i = 0; i < indent; i++) fprintf(dest, "  ");
        fprintf(dest, "Scope: %s%s\n", 
                sym->scope->type == SCOPE_GLOBAL ? "Global" :
                sym->scope->type == SCOPE_FUNCTION ? "Function" : "Block",
                sym->scope->function_name ? 
                    sym->scope->function_name : "");
    }
}

void debug_print_symbol(Symbol* sym, int indent) {
    if (!sym) return;
    
    // Print indentation
    for (int i = 0; i < indent; i++) {
        fprintf(debug_file, "  ");
    }
    
    fprintf(debug_file, "Symbol '%s':\n", sym->name);
    
    // Indent details
    indent++;
    for (int i = 0; i < indent; i++) {
        fprintf(debug_file, "  ");
    }
    
    // Print kind-specific information
    switch (sym->kind) {
        case SYMBOL_FUNCTION:
        case SYMBOL_PROCEDURE:
            fprintf(debug_file, "Kind: %s\n", sym->kind == SYMBOL_FUNCTION ? "Function" : "Procedure");
            for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
            fprintf(debug_file, "Return Type: %s\n", 
                    sym->info.func.return_type ? sym->info.func.return_type : "void");
            for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
            fprintf(debug_file, "Parameter Count: %d\n", sym->info.func.param_count);
            
            // Print parameters
            if (sym->info.func.param_count > 0) {
                for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
                fprintf(debug_file, "Parameters:\n");
                for (int i = 0; i < sym->info.func.param_count; i++) {
                    debug_print_symbol(sym->info.func.parameters[i], indent + 1);
                }
            }
            
            // Print local variables
            if (sym->info.func.local_var_count > 0) {
                for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
                fprintf(debug_file, "Local Variables:\n");
                for (int i = 0; i < sym->info.func.local_var_count; i++) {
                    debug_print_symbol(sym->info.func.local_variables[i], indent + 1);
                }
            }
            break;
            
        case SYMBOL_VARIABLE:
            fprintf(debug_file, "Kind: Variable\n");
            for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
            fprintf(debug_file, "Type: %s\n", sym->info.var.type);
            
            if (sym->info.var.is_pointer) {
                for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
                fprintf(debug_file, "Pointer Level: %d\n", sym->info.var.pointer_level);
            }
            
            if (sym->info.var.is_array) {
                for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
                fprintf(debug_file, "Array Dimensions: %d\n", sym->info.var.dimensions);
                
                if (sym->info.var.bounds) {
                    for (int dim = 0; dim < sym->info.var.dimensions; dim++) {
                        for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
                        fprintf(debug_file, "Dimension %d:\n", dim + 1);
                        
                        DimensionBounds* bound = &sym->info.var.bounds->bounds[dim];
                        indent++;
                        
                        // Print start bound
                        for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
                        if (bound->start.is_constant) {
                            fprintf(debug_file, "Start: constant %ld\n", bound->start.constant_value);
                        } else {
                            fprintf(debug_file, "Start: variable %s\n", 
                                   bound->start.variable_name ? bound->start.variable_name : "<null>");
                        }
                        
                        // Print end bound
                        for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
                        if (bound->end.is_constant) {
                            fprintf(debug_file, "End: constant %ld\n", bound->end.constant_value);
                        } else {
                            fprintf(debug_file, "End: variable %s\n", 
                                   bound->end.variable_name ? bound->end.variable_name : "<null>");
                        }
                        
                        // Print range info
                        for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
                        fprintf(debug_file, "Uses Range: %s\n", bound->using_range ? "yes" : "no");
                        
                        indent--;
                    }
                }
            }
            
            if (sym->info.var.is_parameter) {
                for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
                fprintf(debug_file, "Parameter Mode: %s\n", sym->info.var.param_mode);
            }
            break;
            
        case SYMBOL_PARAMETER:
            fprintf(debug_file, "Kind: Parameter\n");
            for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
            fprintf(debug_file, "Type: %s\n", sym->info.var.type);
            for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
            fprintf(debug_file, "Mode: %s\n", sym->info.var.param_mode);
            if (sym->info.var.needs_type_declaration) {
                for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
                fprintf(debug_file, "Needs Type Declaration: yes\n");
            }
            break;
            
        case SYMBOL_TYPE:
            fprintf(debug_file, "Kind: Type\n");
            break;
            
        default:
            fprintf(debug_file, "Kind: Unknown\n");
    }
    
    // Print scope information
    if (sym->scope) {
        for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
        fprintf(debug_file, "Scope: %s%s\n", 
                sym->scope->type == SCOPE_GLOBAL ? "Global" :
                sym->scope->type == SCOPE_FUNCTION ? "Function" : "Block",
                sym->scope->function_name ? 
                    sym->scope->function_name : "");
    }
}

// Detailed code generation debugging
void debug_print_codegen_state(const char* context, void* state) {
    if (!(current_flags & DEBUG_CODEGEN)) return;
    
    fprintf(debug_file, "=== CodeGen State: %s ===\n", context);
    // Print relevant codegen state information
    CodeGenerator* gen = (CodeGenerator*)state;
    fprintf(debug_file, "Current Function: %s\n", 
            gen->current_function ? gen->current_function : "<none>");
    fprintf(debug_file, "Indent Level: %d\n", gen->indent_level);
    fprintf(debug_file, "In Expression: %s\n", gen->in_expression ? "yes" : "no");
    // Add more state information as needed
}


static const char* debug_get_symbol_kind_str(SymbolKind kind) {
    switch (kind) {
        case SYMBOL_FUNCTION: return "Function";
        case SYMBOL_PROCEDURE: return "Procedure";
        case SYMBOL_VARIABLE: return "Variable";
        case SYMBOL_PARAMETER: return "Parameter";
        case SYMBOL_TYPE: return "Type";
        default: return "Unknown";
    }
}

static const char* debug_get_scope_type_str(ScopeType type) {
    switch (type) {
        case SCOPE_GLOBAL: return "Global";
        case SCOPE_FUNCTION: return "Function";
        case SCOPE_BLOCK: return "Block";
        default: return "Unknown";
    }
}

static void debug_print_array_bounds_to(ArrayBoundsData* bounds, int indent, FILE* dest) {
    if (!bounds) {
        for (int i = 0; i < indent; i++) fprintf(dest, "  ");
        fprintf(dest, "<no bounds information>\n");
        return;
    }

    for (int dim = 0; dim < bounds->dimensions; dim++) {
        for (int i = 0; i < indent; i++) fprintf(dest, "  ");
        fprintf(dest, "Dimension %d:\n", dim + 1);
        
        DimensionBounds* bound = &bounds->bounds[dim];
        indent++;
        
        for (int i = 0; i < indent; i++) fprintf(dest, "  ");
        fprintf(dest, "Range: %s\n", bound->using_range ? "yes" : "no");
        
        for (int i = 0; i < indent; i++) fprintf(dest, "  ");
        if (bound->start.is_constant) {
            fprintf(dest, "Start: constant %ld\n", bound->start.constant_value);
        } else {
            fprintf(dest, "Start: variable %s\n", 
                   bound->start.variable_name ? bound->start.variable_name : "<null>");
        }
        
        for (int i = 0; i < indent; i++) fprintf(dest, "  ");
        if (bound->end.is_constant) {
            fprintf(dest, "End: constant %ld\n", bound->end.constant_value);
        } else {
            fprintf(dest, "End: variable %s\n", 
                   bound->end.variable_name ? bound->end.variable_name : "<null>");
        }
        
        indent--;
    }
}

static void debug_print_array_bounds(ArrayBoundsData* bounds, int indent) {
    /*if (!bounds) {
        for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
        fprintf(debug_file, "<no bounds information>\n");
        return;
    }

    for (int dim = 0; dim < bounds->dimensions; dim++) {
        for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
        fprintf(debug_file, "Dimension %d:\n", dim + 1);
        
        DimensionBounds* bound = &bounds->bounds[dim];
        indent++;
        
        for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
        fprintf(debug_file, "Range: %s\n", bound->using_range ? "yes" : "no");
        
        for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
        if (bound->start.is_constant) {
            fprintf(debug_file, "Start: constant %ld\n", bound->start.constant_value);
        } else {
            fprintf(debug_file, "Start: variable %s\n", 
                   bound->start.variable_name ? bound->start.variable_name : "<null>");
        }
        
        for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
        if (bound->end.is_constant) {
            fprintf(debug_file, "End: constant %ld\n", bound->end.constant_value);
        } else {
            fprintf(debug_file, "End: variable %s\n", 
                   bound->end.variable_name ? bound->end.variable_name : "<null>");
        }
        
        indent--;
    }*/
   debug_print_array_bounds_to(bounds, indent, debug_file);
}

static const char* get_node_color(NodeType type) {
    switch (type) {
        case NODE_PROGRAM: return "lightblue";
        case NODE_FUNCTION:
        case NODE_PROCEDURE: return "lightgreen";
        case NODE_PARAMETER:
        case NODE_PARAMETER_LIST: return "lightpink";
        case NODE_VAR_DECL:
        case NODE_ARRAY_DECL: return "lightyellow";
        case NODE_BINARY_OP:
        case NODE_UNARY_OP: return "lightgray";
        case NODE_IF:
        case NODE_WHILE:
        case NODE_FOR:
        case NODE_REPEAT: return "lightcyan";
        default: return "white";
    }
}

static const char* get_symbol_color(Symbol* sym) {
    switch (sym->kind) {
        case SYMBOL_FUNCTION:
        case SYMBOL_PROCEDURE:
            return "lightblue";
        case SYMBOL_VARIABLE:
            return sym->info.var.is_array ? "lightgreen" : "lightyellow";
        case SYMBOL_PARAMETER:
            return "lightpink";
        case SYMBOL_TYPE:
            return "lightgray";
        default:
            return "white";
    }
}

static void generate_symbol_dot(FILE* dot, Symbol* sym, int scope_id) {
    if (!sym) return;
    
    int sym_id = ++symbol_node_id;
    
    // Build label with symbol details
    fprintf(dot, "  node%d [label=\"{%s|", sym_id, sym->name);
    
    switch (sym->kind) {
        case SYMBOL_FUNCTION:
        case SYMBOL_PROCEDURE:
            fprintf(dot, "Kind: %s\\l", sym->kind == SYMBOL_FUNCTION ? "Function" : "Procedure");
            fprintf(dot, "Return: %s\\l", 
                   sym->info.func.return_type ? sym->info.func.return_type : "void");
            fprintf(dot, "Params: %d\\l", sym->info.func.param_count);
            break;
            
        case SYMBOL_VARIABLE:
            fprintf(dot, "Kind: Variable\\l");
            fprintf(dot, "Type: %s\\l", sym->info.var.type);
            if (sym->info.var.is_array) {
                fprintf(dot, "Dimensions: %d\\l", sym->info.var.dimensions);
                if (sym->info.var.bounds) {
                    for (int i = 0; i < sym->info.var.dimensions; i++) {
                        DimensionBounds* bound = &sym->info.var.bounds->bounds[i];
                        if (bound->using_range) {
                            fprintf(dot, "Dim%d: [%s..%s]\\l", i+1,
                                   bound->start.is_constant ? 
                                   "const" : bound->start.variable_name,
                                   bound->end.is_constant ? 
                                   "const" : bound->end.variable_name);
                        } else {
                            fprintf(dot, "Dim%d: [%s]\\l", i+1,
                                   bound->start.is_constant ? 
                                   "const" : bound->start.variable_name);
                        }
                    }
                }
            }
            if (sym->info.var.is_parameter) {
                fprintf(dot, "Mode: %s\\l", sym->info.var.param_mode);
            }
            break;
            
        case SYMBOL_PARAMETER:
            fprintf(dot, "Kind: Parameter\\l");
            fprintf(dot, "Type: %s\\l", sym->info.var.type);
            fprintf(dot, "Mode: %s\\l", sym->info.var.param_mode);
            break;
            
        case SYMBOL_TYPE:
            fprintf(dot, "Kind: Type\\l");
            break;
    }
    
    fprintf(dot, "}\" style=filled fillcolor=\"%s\"];\n", get_symbol_color(sym));
    
    // Link to scope
    fprintf(dot, "  scope%d -> node%d;\n", scope_id, sym_id);
    
    // If it's a function, create subgraph for parameters and local variables
    if ((sym->kind == SYMBOL_FUNCTION || sym->kind == SYMBOL_PROCEDURE) && 
        (sym->info.func.param_count > 0 || sym->info.func.local_var_count > 0)) {
        
        int func_scope_id = ++symbol_node_id;
        fprintf(dot, "  subgraph cluster_%d {\n", func_scope_id);
        fprintf(dot, "    label=\"%s scope\";\n", sym->name);
        fprintf(dot, "    style=rounded;\n");
        
        // Generate nodes for parameters
        for (int i = 0; i < sym->info.func.param_count; i++) {
            generate_symbol_dot(dot, sym->info.func.parameters[i], func_scope_id);
        }
        
        // Generate nodes for local variables
        for (int i = 0; i < sym->info.func.local_var_count; i++) {
            generate_symbol_dot(dot, sym->info.func.local_variables[i], func_scope_id);
        }
        
        fprintf(dot, "  }\n");
        
        // Link function node to its scope
        fprintf(dot, "  node%d -> scope%d [style=dotted];\n", sym_id, func_scope_id);
    }
}

static void generate_scope_dot(FILE* dot, Scope* scope, int parent_id) {
    if (!scope) return;
    
    int scope_id = ++symbol_node_id;
    
    // Create scope node
    fprintf(dot, "  scope%d [label=\"%s Scope%s\" shape=box style=rounded];\n",
           scope_id,
           scope->type == SCOPE_GLOBAL ? "Global" :
           scope->type == SCOPE_FUNCTION ? "Function" : "Block",
           scope->function_name ? scope->function_name : "");
    
    // Link to parent scope if any
    if (parent_id > 0) {
        fprintf(dot, "  scope%d -> scope%d [style=dashed];\n", parent_id, scope_id);
    }
    
    // Generate symbols in this scope
    for (int i = 0; i < HASH_SIZE; i++) {
        for (Symbol* sym = scope->symbols[i]; sym; sym = sym->next) {
            generate_symbol_dot(dot, sym, scope_id);
        }
    }
}

void generate_symbol_table_dot(FILE* dot, SymbolTable* table) {
    if (!dot || !table) return;
    
    symbol_node_id = 0;  // Reset node counter
    
    fprintf(dot, "  // Symbol Table Structure\n");
    fprintf(dot, "  node [shape=record];\n");
    fprintf(dot, "  rankdir=LR;\n");
    
    // Generate global scope
    generate_scope_dot(dot, table->global, 0);
    
    // If current scope is different from global, generate it too
    if (table->current != table->global) {
        generate_scope_dot(dot, table->current, symbol_node_id);
    }
}

static void generate_dot_node(FILE* dot, ASTNode* node, int parent_id) {
    if (!node) return;
    
    int current_id = ++ast_node_id;
    
    // Node label with all the details
    fprintf(dot, "  node%d [label=\"%s", current_id, ast_node_type_to_string_detailed(node->type));
    
    // Add node-specific information to label
    switch (node->type) {
        case NODE_FUNCTION:
        case NODE_PROCEDURE:
            fprintf(dot, "\\n%s", node->data.function.name);
            if (node->data.function.return_type) {
                fprintf(dot, "\\nReturns: %s", node->data.function.return_type);
            }
            if (node->data.function.is_pointer) {
                fprintf(dot, "\\nPointer Level: %d", node->data.function.pointer_level);
            }
            break;
            
        case NODE_VARIABLE:
        case NODE_VAR_DECL:
        case NODE_ARRAY_DECL:
            if (node->type == NODE_ARRAY_DECL)
            fprintf(dot, "\\n%s: %s", node->data.variable.name, 
                    node->data.variable.type ? node->data.variable.type : "<pending>");
            if (node->data.variable.is_array) {
                fprintf(dot, "\\nDimensions: %d", node->data.variable.array_info.dimensions);
            }
            if (node->data.variable.is_pointer) {
                fprintf(dot, "\\nPointer Level: %d", node->data.variable.pointer_level);
            }
            break;
            
        case NODE_PARAMETER:
            fprintf(dot, "\\n%s: %s", node->data.parameter.name,
                    node->data.parameter.type ? node->data.parameter.type : "<pending>");
            fprintf(dot, "\\nMode: %s", 
                    node->data.parameter.mode == PARAM_MODE_IN ? "in" :
                    node->data.parameter.mode == PARAM_MODE_OUT ? "out" : "inout");
            break;
            
        case NODE_BINARY_OP:
            fprintf(dot, "\\nOperator: %s", token_type_to_string(node->data.binary_op.op));
            break;
            
        case NODE_UNARY_OP:
            fprintf(dot, "\\nOperator: %s", token_type_to_string(node->data.unary_op.op));
            if (node->data.unary_op.op == TOK_DEREF) {
                fprintf(dot, "\\nDereference Count: %d", node->data.unary_op.deref_count);
            }
            break;
            
        case NODE_NUMBER:
        case NODE_IDENTIFIER:
        case NODE_STRING:
            fprintf(dot, "\\n%s", node->data.value);
            break;
            
        case NODE_ARRAY_ACCESS:
            fprintf(dot, "\\nDimensions: %d", node->data.array_access.dimensions);
            break;
        case NODE_BLOCK:
            if (node->child_count == 0 && !node->data.function.body) {
                fprintf(dot, "\\nParameter Body Declaration");
            }
            break;
    }

    // Add location information if available
    if (node->loc.line > 0) {
        fprintf(dot, "\\nLine: %d, Col: %d", node->loc.line, node->loc.column);
    }
    
    // Close label and add style attributes
    fprintf(dot, "\", style=filled, fillcolor=\"%s\"", get_node_color(node->type));
    
    // Add shape based on node type
    if (node->type == NODE_PROGRAM) {
        fprintf(dot, ", shape=doubleoctagon");
    } else if (node->type == NODE_FUNCTION || node->type == NODE_PROCEDURE) {
        fprintf(dot, ", shape=box");
    } else if (node->type == NODE_BLOCK) {
        fprintf(dot, ", shape=box3d");
    } else if (node->type == NODE_IF || node->type == NODE_WHILE || 
               node->type == NODE_FOR || node->type == NODE_REPEAT) {
        fprintf(dot, ", shape=diamond");
    } else if (node->type == NODE_BINARY_OP || node->type == NODE_UNARY_OP) {
        fprintf(dot, ", shape=circle");
    } else {
        fprintf(dot, ", shape=box");
    }

    // Close node definition
    fprintf(dot, "];\n");
    
    // Edge from parent if this isn't the root
    if (parent_id > 0) {
        // Add edge style based on relationship
        if (node->type == NODE_PARAMETER) {
            fprintf(dot, "  node%d -> node%d [style=dotted];\n", parent_id, current_id);
        } else if (node->type == NODE_BLOCK) {
            fprintf(dot, "  node%d -> node%d [style=bold];\n", parent_id, current_id);
        } else {
            fprintf(dot, "  node%d -> node%d;\n", parent_id, current_id);
        }
    }

    if ((node->type == NODE_FUNCTION || node->type == NODE_PROCEDURE) && node->data.function.params) {
        for (int i = 0; i < node->data.function.params->child_count; i++) {
            generate_dot_node(dot, node->data.function.params->children[i], current_id);
        }
    }
    
    for (int i = 0; i < node->child_count; i++) {
        generate_dot_node(dot, node->children[i], current_id);
    }
    
    if (node  && node->type != NODE_ARRAY_DECL && node->data.function.body)
        generate_dot_node(dot, node->data.function.body, current_id);
    
}


void debug_visualize_ast(ASTNode* node, const char* filename) {
    if (!(current_flags & DEBUG_AST)) return;

    FILE* dot = fopen(filename, "w");
    if (!dot) return;

    fprintf(dot, "digraph AST {\n");
    fprintf(dot, "  // Graph attributes\n");
    fprintf(dot, "  graph [rankdir=TB, splines=ortho, nodesep=0.8, ranksep=1.0];\n");
    fprintf(dot, "  node [fontname=\"Arial\", fontsize=10, shape=box, style=filled];\n");
    fprintf(dot, "  edge [fontname=\"Arial\", fontsize=8];\n\n");

    ast_node_id = 0;  // Reset node counter
    generate_dot_node(dot, node, 0);

    fprintf(dot, "}\n");
    fclose(dot);

    // Generate visual representation using graphviz
    char command[256];
    snprintf(command, sizeof(command), "dot -Tpng %s -o %s.png", filename, filename);
    system(command);
}

static void generate_codegen_dot(FILE* dot, CodeGenerator* gen) {
    // Reset counter for this visualization
    codegen_node_id = 0;

    // Create main graph attributes
    fprintf(dot, "  rankdir=TB;\n");  // Top to bottom layout
    fprintf(dot, "  node [shape=record, style=filled];\n");
    
    // Create root node for code generator state
    fprintf(dot, "  state%d [label=\"{CodeGen State|", ++codegen_node_id);
    fprintf(dot, "Current Function: %s\\l", 
            gen->current_function ? gen->current_function : "<none>");
    fprintf(dot, "Indent Level: %d\\l", gen->indent_level);
    fprintf(dot, "In Expression: %s\\l", gen->in_expression ? "yes" : "no");
    fprintf(dot, "Needs Return: %s\\l", gen->needs_return ? "yes" : "no");
    fprintf(dot, "}\" fillcolor=lightblue];\n");

    // Create node for array context
    int array_node = ++codegen_node_id;
    fprintf(dot, "  state%d [label=\"{Array Context|", array_node);
    fprintf(dot, "Adjustment Needed: %s\\l",
            gen->array_context.array_adjustment_needed ? "yes" : "no");
    fprintf(dot, "In Array Access: %s\\l",
            gen->array_context.in_array_access ? "yes" : "no");
    fprintf(dot, "In Array Declaration: %s\\l",
            gen->array_context.in_array_declaration ? "yes" : "no");
    fprintf(dot, "Dimensions: %d\\l", gen->array_context.dimensions);
    fprintf(dot, "Current Dimension: %d\\l", gen->array_context.current_dim);
    fprintf(dot, "}\" fillcolor=lightgreen];\n");

    // Link array context to state
    fprintf(dot, "  state1 -> state%d;\n", array_node);

    // Add symbol table integration node if available
    if (gen->symbols) {
        int symbols_node = ++codegen_node_id;
        fprintf(dot, "  state%d [label=\"{Symbol Table|", symbols_node);
        fprintf(dot, "Active\\l}\" fillcolor=lightyellow];\n");
        fprintf(dot, "  state1 -> state%d;\n", symbols_node);
    }

    // Add output file status
    if (gen->output) {
        int output_node = ++codegen_node_id;
        fprintf(dot, "  state%d [label=\"{Output|Active}\" fillcolor=lightpink];\n",
                output_node);
        fprintf(dot, "  state1 -> state%d;\n", output_node);
    }
}

void debug_visualize_codegen(CodeGenerator* gen, const char* context) {
    if (!(current_flags & DEBUG_CODEGEN)) return;

    // Create filename with context
    char filename[256];
    snprintf(filename, sizeof(filename), "codegen_%s.dot", 
             context ? context : "state");

    FILE* dot = fopen(filename, "w");
    if (!dot) return;

    fprintf(dot, "digraph CodeGen {\n");
    generate_codegen_dot(dot, gen);
    fprintf(dot, "}\n");
    fclose(dot);

    // Generate visualization
    char command[512];
    snprintf(command, sizeof(command), "dot -Tpng %s -o %s.png", 
             filename, filename);
    system(command);

    // Also update the debug log
    if (debug_file) {
        fprintf(debug_file, "\n=== CodeGen State: %s ===\n", context);
        debug_print_codegen_state(context, gen);
        fprintf(debug_file, "=== Generated visualization: %s.png ===\n\n", 
                filename);
    }
}

// Symbol Table Visualization
void debug_visualize_symbol_table(SymbolTable* table, const char* filename) {
    if (!(current_flags & DEBUG_SYMBOLS)) return;

    FILE* dot = fopen(filename, "w");
    if (!dot) return;

    fprintf(dot, "digraph SymbolTable {\n");
    fprintf(dot, "  node [shape=record];\n");
    
    // Generate DOT format for symbol table
    generate_symbol_table_dot(dot, table);
    
    fprintf(dot, "}\n");
    fclose(dot);

    char command[256];
    snprintf(command, sizeof(command), "dot -Tpng %s -o %s.png", filename, filename);
    system(command);
}

void debug_print_parser_state_d(Parser* parser) {
    if (!(current_flags & DEBUG_PARSER)) return;

    fprintf(debug_file, "Parser State:\n");
    fprintf(debug_file, "  Current Token: ");
    debug_print_token(parser->ctx.current);
    fprintf(debug_file, "  Next Token: ");
    debug_print_token(parser->ctx.peek);
    fprintf(debug_file, "  In Function: %s\n",
            parser->ctx.current_function ? parser->ctx.current_function : "<none>");
    fprintf(debug_file, "  In Loop: %s\n",
            parser->ctx.in_loop ? "yes" : "no");
    fprintf(debug_file, "  Error Count: %d\n",
            parser->ctx.error_count);

    if (parser_debug_file) {
        fprintf(parser_debug_file, "Parser State:\n");
        fprintf(parser_debug_file, "  Current Token: ");
        debug_print_token_to(parser->ctx.current, parser_debug_file);
        fprintf(parser_debug_file, "  Next Token: ");
        debug_print_token_to(parser->ctx.peek, parser_debug_file);
        fprintf(parser_debug_file, "  In Function: %s\n",
                parser->ctx.current_function ? parser->ctx.current_function : "<none>");
        fprintf(parser_debug_file, "  In Loop: %s\n",
                parser->ctx.in_loop ? "yes" : "no");
        fprintf(parser_debug_file, "  Error Count: %d\n",
                parser->ctx.error_count);
    }
}

void debug_ast_node_create(NodeType type, const char* context) {
    if (!(current_flags & DEBUG_AST)) return;
    
    fprintf(debug_file, "\n=== Creating AST Node ===\n");
    fprintf(debug_file, "  Type: %s\n", ast_node_type_to_string_detailed(type));
    fprintf(debug_file, "  Context: %s\n", context);

    if (ast_debug_file) {
        fprintf(ast_debug_file, "\n=== Creating AST Node ===\n");
        fprintf(ast_debug_file, "  Type: %s\n", ast_node_type_to_string_detailed(type));
        fprintf(ast_debug_file, "  Context: %s\n", context);
    }
}

void debug_ast_node_destroy(ASTNode* node, const char* context) {
    if (!(current_flags & DEBUG_AST)) return;
    
    fprintf(debug_file, "\n=== Destroying AST Node ===\n");
    fprintf(debug_file, "  Type: %s\n", ast_node_type_to_string_detailed(node->type));
    fprintf(debug_file, "  Context: %s\n", context);
    fprintf(debug_file, "  Node Details:\n");
    debug_print_ast(node, 2, false);

    if (ast_debug_file) {
        fprintf(ast_debug_file, "\n=== Destroying AST Node ===\n");
        fprintf(ast_debug_file, "  Type: %s\n", ast_node_type_to_string_detailed(node->type));
        fprintf(ast_debug_file, "  Context: %s\n", context);
        fprintf(ast_debug_file, "  Node Details:\n");
        debug_print_ast_to(node, 2, false, ast_debug_file);
    }
}

void debug_ast_add_child(ASTNode* parent, ASTNode* child, const char* context) {
    if (!(current_flags & DEBUG_AST)) return;
    
    fprintf(debug_file, "\n=== Adding Child Node ===\n");
    fprintf(debug_file, "  Parent: %s\n", ast_node_type_to_string_detailed(parent->type));
    fprintf(debug_file, "  Child: %s\n", ast_node_type_to_string_detailed(child->type));
    fprintf(debug_file, "  Context: %s\n", context);
    fprintf(debug_file, "  New Child Count: %d\n", parent->child_count + 1);

    if (ast_debug_file) {
        fprintf(ast_debug_file, "\n=== Adding Child Node ===\n");
        fprintf(ast_debug_file, "  Parent: %s\n", ast_node_type_to_string_detailed(parent->type));
        fprintf(ast_debug_file, "  Child: %s\n", ast_node_type_to_string_detailed(child->type));
        fprintf(ast_debug_file, "  Context: %s\n", context);
        fprintf(ast_debug_file, "  New Child Count: %d\n", parent->child_count + 1);
    }
}

void debug_ast_node_complete(ASTNode* node, const char* context) {
    if (!(current_flags & DEBUG_AST)) return;
    
    fprintf(debug_file, "\n=== Completed AST Node ===\n");
    fprintf(debug_file, "  Context: %s\n", context);
    fprintf(debug_file, "  Node Structure:\n");
    debug_print_ast(node, 2, false);

    if (ast_debug_file) {
        fprintf(ast_debug_file, "\n=== Completed AST Node ===\n");
        fprintf(ast_debug_file, "  Context: %s\n", context);
        fprintf(ast_debug_file, "  Node Structure:\n");
        debug_print_ast_to(node, 2, false, ast_debug_file);
    }
}

void debug_symbol_create(Symbol* sym, const char* context) {
    if (!(current_flags & DEBUG_SYMBOLS)) return;
    
    fprintf(debug_file, "\n=== Creating Symbol: %s ===\n", sym->name);
    fprintf(debug_file, "  Context: %s\n", context);
    fprintf(debug_file, "  Kind: %s\n", debug_get_symbol_kind_str(sym->kind));
    
    switch (sym->kind) {
        case SYMBOL_FUNCTION:
        case SYMBOL_PROCEDURE:
            fprintf(debug_file, "  Return Type: %s\n", 
                    sym->info.func.return_type ? sym->info.func.return_type : "<none>");
            fprintf(debug_file, "  Parameters: %d\n", sym->info.func.param_count);
            if (sym->info.func.is_pointer) {
                fprintf(debug_file, "  Pointer Level: %d\n", sym->info.func.pointer_level);
            }
            break;
            
        case SYMBOL_VARIABLE:
            fprintf(debug_file, "  Type: %s\n", sym->info.var.type ? sym->info.var.type : "<pending>");
            if (sym->info.var.is_array) {
                fprintf(debug_file, "  Array Info:\n");
                fprintf(debug_file, "    Dimensions: %d\n", sym->info.var.dimensions);
                debug_print_array_bounds(sym->info.var.bounds, 2);
            }
            if (sym->info.var.is_pointer) {
                fprintf(debug_file, "  Pointer Level: %d\n", sym->info.var.pointer_level);
            }
            if (sym->info.var.is_parameter) {
                fprintf(debug_file, "  Parameter Mode: %s\n", sym->info.var.param_mode);
            }
            break;
            
        case SYMBOL_PARAMETER:
            fprintf(debug_file, "  Type: %s\n", sym->info.var.type ? sym->info.var.type : "<pending>");
            fprintf(debug_file, "  Mode: %s\n", sym->info.var.param_mode);
            fprintf(debug_file, "  Needs Type Declaration: %s\n", 
                    sym->info.var.needs_type_declaration ? "yes" : "no");
            break;
    }
    fprintf(debug_file, "\n");

    if (symbol_debug_file) {
        if (!(current_flags & DEBUG_SYMBOLS)) return;
    
        fprintf(symbol_debug_file, "\n=== Creating Symbol: %s ===\n", sym->name);
        fprintf(symbol_debug_file, "  Context: %s\n", context);
        fprintf(symbol_debug_file, "  Kind: %s\n", debug_get_symbol_kind_str(sym->kind));
        
        switch (sym->kind) {
            case SYMBOL_FUNCTION:
            case SYMBOL_PROCEDURE:
                fprintf(symbol_debug_file, "  Return Type: %s\n", 
                        sym->info.func.return_type ? sym->info.func.return_type : "<none>");
                fprintf(symbol_debug_file, "  Parameters: %d\n", sym->info.func.param_count);
                if (sym->info.func.is_pointer) {
                    fprintf(symbol_debug_file, "  Pointer Level: %d\n", sym->info.func.pointer_level);
                }
                break;
                
            case SYMBOL_VARIABLE:
                fprintf(symbol_debug_file, "  Type: %s\n", sym->info.var.type ? sym->info.var.type : "<pending>");
                if (sym->info.var.is_array) {
                    fprintf(symbol_debug_file, "  Array Info:\n");
                    fprintf(symbol_debug_file, "    Dimensions: %d\n", sym->info.var.dimensions);
                    debug_print_array_bounds_to(sym->info.var.bounds, 2, symbol_debug_file);
                }
                if (sym->info.var.is_pointer) {
                    fprintf(symbol_debug_file, "  Pointer Level: %d\n", sym->info.var.pointer_level);
                }
                if (sym->info.var.is_parameter) {
                    fprintf(symbol_debug_file, "  Parameter Mode: %s\n", sym->info.var.param_mode);
                }
                break;
                
            case SYMBOL_PARAMETER:
                fprintf(symbol_debug_file, "  Type: %s\n", sym->info.var.type ? sym->info.var.type : "<pending>");
                fprintf(symbol_debug_file, "  Mode: %s\n", sym->info.var.param_mode);
                fprintf(symbol_debug_file, "  Needs Type Declaration: %s\n", 
                        sym->info.var.needs_type_declaration ? "yes" : "no");
                break;
        }
        fprintf(symbol_debug_file, "\n");
    }
}

void debug_symbol_destroy(Symbol* sym, const char* context) {
    if (!(current_flags & DEBUG_SYMBOLS)) return;
    
    fprintf(debug_file, "\n=== Destroying Symbol: %s ===\n", sym->name);
    fprintf(debug_file, "  Context: %s\n", context);
    fprintf(debug_file, "  Kind: %s\n", debug_get_symbol_kind_str(sym->kind));
    fprintf(debug_file, "\n");

    if (symbol_debug_file) {
        fprintf(symbol_debug_file, "\n=== Destroying Symbol: %s ===\n", sym->name);
        fprintf(symbol_debug_file, "  Context: %s\n", context);
        fprintf(symbol_debug_file, "  Kind: %s\n", debug_get_symbol_kind_str(sym->kind));
        fprintf(symbol_debug_file, "\n");
    }
}

void debug_scope_enter(Scope* scope, const char* context) {
    if (!(current_flags & DEBUG_SYMBOLS)) return;
    
    fprintf(debug_file, "\n>>> Entering Scope <<<\n");
    fprintf(debug_file, "  Type: %s\n", debug_get_scope_type_str(scope->type));
    if (scope->function_name) {
        fprintf(debug_file, "  Function: %s\n", scope->function_name);
    }
    fprintf(debug_file, "  Context: %s\n", context);
    fprintf(debug_file, "  Parent Scope: %s\n", 
            scope->parent ? debug_get_scope_type_str(scope->parent->type) : "<none>");
    fprintf(debug_file, "\n");

    if (symbol_debug_file) {
        fprintf(symbol_debug_file, "\n>>> Entering Scope <<<\n");
        fprintf(symbol_debug_file, "  Type: %s\n", debug_get_scope_type_str(scope->type));
        if (scope->function_name) {
            fprintf(symbol_debug_file, "  Function: %s\n", scope->function_name);
        }
        fprintf(symbol_debug_file, "  Context: %s\n", context);
        fprintf(symbol_debug_file, "  Parent Scope: %s\n", 
                scope->parent ? debug_get_scope_type_str(scope->parent->type) : "<none>");
        fprintf(symbol_debug_file, "\n");
    }    
}

void debug_scope_exit(Scope* scope, const char* context) {
    if (!(current_flags & DEBUG_SYMBOLS)) return;
    
    fprintf(debug_file, "\n<<< Exiting Scope <<<\n");
    fprintf(debug_file, "  Type: %s\n", debug_get_scope_type_str(scope->type));
    if (scope->function_name) {
        fprintf(debug_file, "  Function: %s\n", scope->function_name);
    }
    fprintf(debug_file, "  Context: %s\n", context);
    fprintf(debug_file, "  Symbol Count: %d\n", scope->symbol_count);
    fprintf(debug_file, "\n");

    if (symbol_debug_file) {
        fprintf(symbol_debug_file, "\n<<< Exiting Scope <<<\n");
        fprintf(symbol_debug_file, "  Type: %s\n", debug_get_scope_type_str(scope->type));
        if (scope->function_name) {
            fprintf(symbol_debug_file, "  Function: %s\n", scope->function_name);
        }
        fprintf(symbol_debug_file, "  Context: %s\n", context);
        fprintf(symbol_debug_file, "  Symbol Count: %d\n", scope->symbol_count);
        fprintf(symbol_debug_file, "\n");
    }
}

void debug_symbol_lookup(const char* name, Symbol* result, const char* context) {
    if (!(current_flags & DEBUG_SYMBOLS)) return;
    
    fprintf(debug_file, "\n=== Symbol Lookup: %s ===\n", name);
    fprintf(debug_file, "  Context: %s\n", context);
    fprintf(debug_file, "  Result: %s\n", result ? "found" : "not found");
    if (result) {
        fprintf(debug_file, "  Found in scope: %s\n", 
                debug_get_scope_type_str(result->scope->type));
        fprintf(debug_file, "  Symbol details:\n");
        debug_print_symbol(result, 2);
    }
    fprintf(debug_file, "\n");

    if (symbol_debug_file) {
        fprintf(symbol_debug_file, "\n=== Symbol Lookup: %s ===\n", name);
        fprintf(symbol_debug_file, "  Context: %s\n", context);
        fprintf(symbol_debug_file, "  Result: %s\n", result ? "found" : "not found");
        if (result) {
            fprintf(symbol_debug_file, "  Found in scope: %s\n", 
                    debug_get_scope_type_str(result->scope->type));
            fprintf(symbol_debug_file, "  Symbol details:\n");
            debug_print_symbol_to(result, 2, symbol_debug_file);
        }
        fprintf(symbol_debug_file, "\n");
    }
}

void debug_symbol_bounds_update(Symbol* sym, ArrayBoundsData* bounds, const char* context) {
    if (!(current_flags & DEBUG_SYMBOLS)) return;
    
    fprintf(debug_file, "\n=== Updating Array Bounds: %s ===\n", sym->name);
    fprintf(debug_file, "  Context: %s\n", context);
    fprintf(debug_file, "  Old bounds:\n");
    debug_print_array_bounds(sym->info.var.bounds, 2);
    fprintf(debug_file, "  New bounds:\n");
    debug_print_array_bounds(bounds, 2);
    fprintf(debug_file, "\n");

    if (symbol_debug_file) {
        fprintf(symbol_debug_file, "\n=== Updating Array Bounds: %s ===\n", sym->name);
        fprintf(symbol_debug_file, "  Context: %s\n", context);
        fprintf(symbol_debug_file, "  Old bounds:\n");
        debug_print_array_bounds_to(sym->info.var.bounds, 2, symbol_debug_file);
        fprintf(symbol_debug_file, "  New bounds:\n");
        debug_print_array_bounds_to(bounds, 2, symbol_debug_file);
        fprintf(symbol_debug_file, "\n");
    }
}

void debug_symbol_table_operation(const char* operation, const char* details) {
    if (!(current_flags & DEBUG_SYMBOLS)) return;
    
    fprintf(debug_file, "\n=== Symbol Table Operation ===\n");
    fprintf(debug_file, "  Operation: %s\n", operation);
    fprintf(debug_file, "  Details: %s\n", details);
    fprintf(debug_file, "\n");

    if (symbol_debug_file) {
        fprintf(symbol_debug_file, "\n=== Symbol Table Operation ===\n");
        fprintf(symbol_debug_file, "  Operation: %s\n", operation);
        fprintf(symbol_debug_file, "  Details: %s\n", details);
        fprintf(symbol_debug_file, "\n");
    }
}

void debug_codegen_state(CodeGenerator* gen, const char* context) {
    if (!(current_flags & DEBUG_CODEGEN)) return;
    
    fprintf(debug_file, "\n=== CodeGen State: %s ===\n", context);
    fprintf(debug_file, "  Current Function: %s\n", 
            gen->current_function ? gen->current_function : "<none>");
    fprintf(debug_file, "  Indent Level: %d\n", gen->indent_level);
    fprintf(debug_file, "  In Expression: %s\n", gen->in_expression ? "yes" : "no");
    fprintf(debug_file, "  Needs Return: %s\n", gen->needs_return ? "yes" : "no");
    
    fprintf(debug_file, "  Array Context:\n");
    fprintf(debug_file, "    Adjustment Needed: %s\n", 
            gen->array_context.array_adjustment_needed ? "yes" : "no");
    fprintf(debug_file, "    In Array Access: %s\n", 
            gen->array_context.in_array_access ? "yes" : "no");
    fprintf(debug_file, "    In Array Declaration: %s\n", 
            gen->array_context.in_array_declaration ? "yes" : "no");
    fprintf(debug_file, "    Dimensions: %d\n", gen->array_context.dimensions);
    fprintf(debug_file, "    Current Dimension: %d\n", gen->array_context.current_dim);
    fprintf(debug_file, "\n");

    if (codegen_debug_file) {
        fprintf(codegen_debug_file, "\n=== CodeGen State: %s ===\n", context);
        fprintf(codegen_debug_file, "  Current Function: %s\n", 
                gen->current_function ? gen->current_function : "<none>");
        fprintf(codegen_debug_file, "  Indent Level: %d\n", gen->indent_level);
        fprintf(codegen_debug_file, "  In Expression: %s\n", gen->in_expression ? "yes" : "no");
        fprintf(codegen_debug_file, "  Needs Return: %s\n", gen->needs_return ? "yes" : "no");
        
        fprintf(codegen_debug_file, "  Array Context:\n");
        fprintf(codegen_debug_file, "    Adjustment Needed: %s\n", 
                gen->array_context.array_adjustment_needed ? "yes" : "no");
        fprintf(codegen_debug_file, "    In Array Access: %s\n", 
                gen->array_context.in_array_access ? "yes" : "no");
        fprintf(codegen_debug_file, "    In Array Declaration: %s\n", 
                gen->array_context.in_array_declaration ? "yes" : "no");
        fprintf(codegen_debug_file, "    Dimensions: %d\n", gen->array_context.dimensions);
        fprintf(codegen_debug_file, "    Current Dimension: %d\n", gen->array_context.current_dim);
        fprintf(codegen_debug_file, "\n");
    }
}

void debug_codegen_expression(CodeGenerator* gen, ASTNode* expr, const char* context) {
    if (!(current_flags & DEBUG_CODEGEN)) return;
    
    fprintf(debug_file, "\n=== Generating Expression: %s ===\n", context);
    fprintf(debug_file, "  Expression Type: %s\n", ast_node_type_to_string_detailed(expr->type));
    
    switch (expr->type) {
        case NODE_BINARY_OP:
            fprintf(debug_file, "  Operator: %s\n", token_type_to_string(expr->data.binary_op.op));
            break;
        case NODE_UNARY_OP:
            fprintf(debug_file, "  Operator: %s\n", token_type_to_string(expr->data.unary_op.op));
            if (expr->data.unary_op.op == TOK_DEREF) {
                fprintf(debug_file, "  Dereference Count: %d\n", expr->data.unary_op.deref_count);
            }
            break;
        case NODE_ARRAY_ACCESS:
            fprintf(debug_file, "  Array Dimensions: %d\n", expr->data.array_access.dimensions);
            fprintf(debug_file, "  Current Context:\n");
            fprintf(debug_file, "    One-Based: %s\n", 
                    g_config.array_indexing == ARRAY_ONE_BASED ? "yes" : "no");
            fprintf(debug_file, "    Needs Adjustment: %s\n",
                    gen->array_context.array_adjustment_needed ? "yes" : "no");
            break;
    }
    fprintf(debug_file, "\n");

    if (codegen_debug_file) {
        fprintf(codegen_debug_file, "\n=== Generating Expression: %s ===\n", context);
        fprintf(codegen_debug_file, "  Expression Type: %s\n", ast_node_type_to_string_detailed(expr->type));
        
        switch (expr->type) {
            case NODE_BINARY_OP:
                fprintf(codegen_debug_file, "  Operator: %s\n", token_type_to_string(expr->data.binary_op.op));
                break;
            case NODE_UNARY_OP:
                fprintf(codegen_debug_file, "  Operator: %s\n", token_type_to_string(expr->data.unary_op.op));
                if (expr->data.unary_op.op == TOK_DEREF) {
                    fprintf(codegen_debug_file, "  Dereference Count: %d\n", expr->data.unary_op.deref_count);
                }
                break;
            case NODE_ARRAY_ACCESS:
                fprintf(codegen_debug_file, "  Array Dimensions: %d\n", expr->data.array_access.dimensions);
                fprintf(codegen_debug_file, "  Current Context:\n");
                fprintf(codegen_debug_file, "    One-Based: %s\n", 
                        g_config.array_indexing == ARRAY_ONE_BASED ? "yes" : "no");
                fprintf(codegen_debug_file, "    Needs Adjustment: %s\n",
                        gen->array_context.array_adjustment_needed ? "yes" : "no");
                break;
        }
        fprintf(codegen_debug_file, "\n");
    }
}

void debug_codegen_array(CodeGenerator* gen, ASTNode* array, const char* context) {
    if (!(current_flags & DEBUG_CODEGEN)) return;
    
    fprintf(debug_file, "\n=== Generating Array Access: %s ===\n", context);
    
    Symbol* sym = NULL;
    if (array->type == NODE_IDENTIFIER) {
        sym = symtable_lookup(gen->symbols, array->data.value);
    } else if (array->type == NODE_VARIABLE) {
        sym = symtable_lookup(gen->symbols, array->data.variable.name);
    }
    
    fprintf(debug_file, "  Array Name: %s\n", 
            sym ? sym->name : "unknown");
    fprintf(debug_file, "  Symbol Found: %s\n", sym ? "yes" : "no");
    
    if (sym && sym->info.var.bounds) {
        fprintf(debug_file, "  Array Bounds:\n");
        for (int i = 0; i < sym->info.var.dimensions; i++) {
            DimensionBounds* bound = &sym->info.var.bounds->bounds[i];
            fprintf(debug_file, "    Dimension %d:\n", i + 1);
            fprintf(debug_file, "      Uses Range: %s\n", bound->using_range ? "yes" : "no");
            if (bound->start.is_constant) {
                fprintf(debug_file, "      Start: constant %ld\n", bound->start.constant_value);
            } else {
                fprintf(debug_file, "      Start: variable %s\n", 
                        bound->start.variable_name ? bound->start.variable_name : "<null>");
            }
            if (bound->end.is_constant) {
                fprintf(debug_file, "      End: constant %ld\n", bound->end.constant_value);
            } else {
                fprintf(debug_file, "      End: variable %s\n",
                        bound->end.variable_name ? bound->end.variable_name : "<null>");
            }
        }
    }
    fprintf(debug_file, "\n");

    if (codegen_debug_file) {
        fprintf(codegen_debug_file, "\n=== Generating Array Access: %s ===\n", context);
    
        if (array->type == NODE_IDENTIFIER) {
            sym = symtable_lookup(gen->symbols, array->data.value);
        } else if (array->type == NODE_VARIABLE) {
            sym = symtable_lookup(gen->symbols, array->data.variable.name);
        }
        
        fprintf(codegen_debug_file, "  Array Name: %s\n", 
                sym ? sym->name : "unknown");
        fprintf(codegen_debug_file, "  Symbol Found: %s\n", sym ? "yes" : "no");
        
        if (sym && sym->info.var.bounds) {
            fprintf(codegen_debug_file, "  Array Bounds:\n");
            for (int i = 0; i < sym->info.var.dimensions; i++) {
                DimensionBounds* bound = &sym->info.var.bounds->bounds[i];
                fprintf(codegen_debug_file, "    Dimension %d:\n", i + 1);
                fprintf(codegen_debug_file, "      Uses Range: %s\n", bound->using_range ? "yes" : "no");
                if (bound->start.is_constant) {
                    fprintf(codegen_debug_file, "      Start: constant %ld\n", bound->start.constant_value);
                } else {
                    fprintf(codegen_debug_file, "      Start: variable %s\n", 
                            bound->start.variable_name ? bound->start.variable_name : "<null>");
                }
                if (bound->end.is_constant) {
                    fprintf(codegen_debug_file, "      End: constant %ld\n", bound->end.constant_value);
                } else {
                    fprintf(codegen_debug_file, "      End: variable %s\n",
                            bound->end.variable_name ? bound->end.variable_name : "<null>");
                }
            }
        }
        fprintf(codegen_debug_file, "\n");
    }
}

void debug_codegen_function(CodeGenerator* gen, ASTNode* func, const char* context) {
    if (!(current_flags & DEBUG_CODEGEN)) return;
    
    fprintf(debug_file, "\n=== Generating Function: %s ===\n", context);
    fprintf(debug_file, "  Name: %s\n", func->data.function.name);
    fprintf(debug_file, "  Type: %s\n", 
            func->type == NODE_FUNCTION ? "Function" : "Procedure");
    fprintf(debug_file, "  Return Type: %s\n", 
            func->data.function.return_type ? func->data.function.return_type : "void");
    
    if (func->data.function.params) {
        fprintf(debug_file, "  Parameters:\n");
        for (int i = 0; i < func->data.function.params->child_count; i++) {
            ASTNode* param = func->data.function.params->children[i];
            fprintf(debug_file, "    %d: %s (%s)\n", i + 1,
                    param->data.parameter.name,
                    param->data.parameter.mode == PARAM_MODE_IN ? "in" :
                    param->data.parameter.mode == PARAM_MODE_OUT ? "out" : "inout");
        }
    }
    fprintf(debug_file, "\n");

    if (codegen_debug_file) {
        fprintf(codegen_debug_file, "\n=== Generating Function: %s ===\n", context);
        fprintf(codegen_debug_file, "  Name: %s\n", func->data.function.name);
        fprintf(codegen_debug_file, "  Type: %s\n", 
                func->type == NODE_FUNCTION ? "Function" : "Procedure");
        fprintf(codegen_debug_file, "  Return Type: %s\n", 
                func->data.function.return_type ? func->data.function.return_type : "void");
        
        if (func->data.function.params) {
            fprintf(codegen_debug_file, "  Parameters:\n");
            for (int i = 0; i < func->data.function.params->child_count; i++) {
                ASTNode* param = func->data.function.params->children[i];
                fprintf(codegen_debug_file, "    %d: %s (%s)\n", i + 1,
                        param->data.parameter.name,
                        param->data.parameter.mode == PARAM_MODE_IN ? "in" :
                        param->data.parameter.mode == PARAM_MODE_OUT ? "out" : "inout");
            }
        }
        fprintf(codegen_debug_file, "\n");
    }
}

void debug_codegen_block(CodeGenerator* gen, const char* context, int new_level) {
    if (!(current_flags & DEBUG_CODEGEN)) return;
    
    fprintf(debug_file, "\n=== Generating Block: %s ===\n", context);
    fprintf(debug_file, "  Previous Indent: %d\n", gen->indent_level);
    fprintf(debug_file, "  New Indent: %d\n", new_level);
    fprintf(debug_file, "  In Function: %s\n", 
            gen->current_function ? gen->current_function : "<none>");
    fprintf(debug_file, "\n");

    if (codegen_debug_file) {
        fprintf(codegen_debug_file, "\n=== Generating Block: %s ===\n", context);
        fprintf(codegen_debug_file, "  Previous Indent: %d\n", gen->indent_level);
        fprintf(codegen_debug_file, "  New Indent: %d\n", new_level);
        fprintf(codegen_debug_file, "  In Function: %s\n", 
                gen->current_function ? gen->current_function : "<none>");
        fprintf(codegen_debug_file, "\n");
    }
}

void debug_codegen_symbol_resolution(CodeGenerator* gen, const char* name, Symbol* sym, const char* context) {
    if (!(current_flags & DEBUG_CODEGEN)) return;
    
    fprintf(debug_file, "\n=== Symbol Resolution: %s ===\n", context);
    fprintf(debug_file, "  Name: %s\n", name);
    fprintf(debug_file, "  Found: %s\n", sym ? "yes" : "no");
    
    if (sym) {
        fprintf(debug_file, "  Kind: %s\n", 
                sym->kind == SYMBOL_FUNCTION ? "Function" :
                sym->kind == SYMBOL_PROCEDURE ? "Procedure" :
                sym->kind == SYMBOL_VARIABLE ? "Variable" :
                sym->kind == SYMBOL_PARAMETER ? "Parameter" : "Unknown");
        
        if (sym->kind == SYMBOL_VARIABLE || sym->kind == SYMBOL_PARAMETER) {
            fprintf(debug_file, "  Type: %s\n", sym->info.var.type);
            if (sym->info.var.is_array) {
                fprintf(debug_file, "  Is Array: yes\n");
                fprintf(debug_file, "  Dimensions: %d\n", sym->info.var.dimensions);
            }
            if (sym->info.var.is_parameter) {
                fprintf(debug_file, "  Parameter Mode: %s\n", sym->info.var.param_mode);
            }
        }
    }
    fprintf(debug_file, "\n");

    if (codegen_debug_file) {
        fprintf(codegen_debug_file, "\n=== Symbol Resolution: %s ===\n", context);
        fprintf(codegen_debug_file, "  Name: %s\n", name);
        fprintf(codegen_debug_file, "  Found: %s\n", sym ? "yes" : "no");
        
        if (sym) {
            fprintf(codegen_debug_file, "  Kind: %s\n", 
                    sym->kind == SYMBOL_FUNCTION ? "Function" :
                    sym->kind == SYMBOL_PROCEDURE ? "Procedure" :
                    sym->kind == SYMBOL_VARIABLE ? "Variable" :
                    sym->kind == SYMBOL_PARAMETER ? "Parameter" : "Unknown");
            
            if (sym->kind == SYMBOL_VARIABLE || sym->kind == SYMBOL_PARAMETER) {
                fprintf(codegen_debug_file, "  Type: %s\n", sym->info.var.type);
                if (sym->info.var.is_array) {
                    fprintf(codegen_debug_file, "  Is Array: yes\n");
                    fprintf(codegen_debug_file, "  Dimensions: %d\n", sym->info.var.dimensions);
                }
                if (sym->info.var.is_parameter) {
                    fprintf(codegen_debug_file, "  Parameter Mode: %s\n", sym->info.var.param_mode);
                }
            }
        }
        fprintf(codegen_debug_file, "\n");
    }
}

/*static void print_parser_context(Parser* parser, const char* context, int indent) {
    if (!parser || !debug_file) return;

    for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
    fprintf(debug_file, "=== Parser Context: %s ===\n", context);

    if (parser->ctx.current) {
        for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
        fprintf(debug_file, "Current Token: ");
        debug_print_token(parser->ctx.current);
    }

    if (parser->ctx.peek) {
        for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
        fprintf(debug_file, "Next Token: ");
        debug_print_token(parser->ctx.peek);
    }

    for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
    fprintf(debug_file, "In Function: %s\n", 
            parser->ctx.current_function ? parser->ctx.current_function : "<none>");
    
    for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
    fprintf(debug_file, "Scope Level: %d\n", 
            parser->ctx.symbols ? parser->ctx.symbols->scope_level : -1);
    
    for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
    fprintf(debug_file, "Error Count: %d\n", parser->ctx.error_count);
    
    for (int i = 0; i < indent; i++) fprintf(debug_file, "  ");
    fprintf(debug_file, "Error Recovery: %s\n", 
            parser->panic_mode ? "active" : "inactive");
}*/

void debug_parser_token_consume(Parser* parser, Token* token, const char* expected) {
    if (!(current_flags & DEBUG_PARSER)) return;
    
    fprintf(debug_file, "  Token Consumer:\n");
    fprintf(debug_file, "    Expected: %s\n", expected);
    fprintf(debug_file, "    Got: %s (type=%s, line=%d, col=%d)\n",
            token->value ? token->value : "<null>",
            token_type_to_string(token->type),
            token->loc.line,
            token->loc.column);

    if (parser_debug_file) {
        fprintf(parser_debug_file, "  Token Consumer:\n");
        fprintf(parser_debug_file, "    Expected: %s\n", expected);
        fprintf(parser_debug_file, "    Got: %s (type=%s, line=%d, col=%d)\n",
                token->value ? token->value : "<null>",
                token_type_to_string(token->type),
                token->loc.line,
                token->loc.column);
    }
}

void debug_parser_rule_start(Parser* parser, const char* rule_name) {
    if (!(current_flags & DEBUG_PARSER)) return;
    
    fprintf(debug_file, "\n=== Begin Rule: %s ===\n", rule_name);
    debug_parser_state(parser, "Rule Entry");

    if (parser_debug_file) {
        fprintf(parser_debug_file, "\n=== Begin Rule: %s ===\n", rule_name);
        debug_parser_state_to(parser, "Rule Entry", parser_debug_file);
    }
}

void debug_parser_rule_end(Parser* parser, const char* rule_name, ASTNode* result) {
    if (!(current_flags & DEBUG_PARSER)) return;
    
    fprintf(debug_file, "\n=== End Rule: %s ===\n", rule_name);
    if (result) {
        fprintf(debug_file, "  Result Type: %s\n", ast_node_type_to_string(result));
        switch (result->type) {
            case NODE_NUMBER:
            case NODE_IDENTIFIER:
            case NODE_STRING:
                fprintf(debug_file, "  Value: %s\n", result->data.value);
                break;
            case NODE_PARAMETER:
                fprintf(debug_file, "  Parameter: %s: %s (%s)\n",
                        result->data.parameter.name,
                        result->data.parameter.type ? result->data.parameter.type : "<type pending>",
                        result->data.parameter.mode == PARAM_MODE_IN ? "in" :
                        result->data.parameter.mode == PARAM_MODE_OUT ? "out" : "inout");
                break;
            case NODE_PARAMETER_LIST:
                fprintf(debug_file, "  Parameters: %d\n", result->child_count);
                for (int i = 0; i < result->child_count; i++) {
                    debug_print_ast(result->children[i], 2, true);
                }
                break;
            default:
                fprintf(debug_file, "  Details:\n");
                debug_print_ast(result, 2, true);
        }
    } else {
        fprintf(debug_file, "  Result: <null>\n");
    }
    fprintf(debug_file, "\n");

    if (parser_debug_file) {
        fprintf(parser_debug_file, "\n=== End Rule: %s ===\n", rule_name);
        if (result) {
            fprintf(parser_debug_file, "  Result Type: %s\n", ast_node_type_to_string(result));
            switch (result->type) {
                case NODE_NUMBER:
                case NODE_IDENTIFIER:
                case NODE_STRING:
                    fprintf(parser_debug_file, "  Value: %s\n", result->data.value);
                    break;
                case NODE_PARAMETER:
                    fprintf(parser_debug_file, "  Parameter: %s: %s (%s)\n",
                            result->data.parameter.name,
                            result->data.parameter.type ? result->data.parameter.type : "<type pending>",
                            result->data.parameter.mode == PARAM_MODE_IN ? "in" :
                            result->data.parameter.mode == PARAM_MODE_OUT ? "out" : "inout");
                    break;
                case NODE_PARAMETER_LIST:
                    fprintf(parser_debug_file, "  Parameters: %d\n", result->child_count);
                    for (int i = 0; i < result->child_count; i++) {
                        debug_print_ast_to(result->children[i], 2, true, parser_debug_file);
                    }
                    break;
                default:
                    fprintf(parser_debug_file, "  Details:\n");
                    debug_print_ast_to(result, 2, true, parser_debug_file);
            }
        } else {
            fprintf(parser_debug_file, "  Result: <null>\n");
        }
        fprintf(parser_debug_file, "\n");
    }
}

void debug_parser_expression(ASTNode* expr, const char* context) {
    if (!(current_flags & DEBUG_PARSER)) return;
    
    fprintf(debug_file, "\n=== Expression Parse: %s ===\n", context);
    if (expr) {
        fprintf(debug_file, "  Result Type: %s\n", ast_node_type_to_string(expr));
        fprintf(debug_file, "  Value: ");
        switch (expr->type) {
            case NODE_NUMBER:
                fprintf(debug_file, "%s\n", expr->data.value);
                break;
            case NODE_IDENTIFIER:
                fprintf(debug_file, "%s\n", expr->data.value);
                break;
            case NODE_BINARY_OP:
                fprintf(debug_file, "Binary Operation (%s)\n", 
                        token_type_to_string(expr->data.binary_op.op));
                break;
            case NODE_UNARY_OP:
                fprintf(debug_file, "Unary Operation (%s)\n", 
                        token_type_to_string(expr->data.unary_op.op));
                break;
            case NODE_ARRAY_ACCESS:
                fprintf(debug_file, "Array Access (dimensions: %d)\n", 
                        expr->data.array_access.dimensions);
                break;
            default:
                fprintf(debug_file, "<unhandled expression type>\n");
        }
        if (expr->child_count > 0) {
            fprintf(debug_file, "  Children:\n");
            for (int i = 0; i < expr->child_count; i++) {
                debug_print_ast(expr->children[i], 2, true);
            }
        }
    } else {
        fprintf(debug_file, "  Result: <no expression>\n");
    }

    if (parser_debug_file) {
        fprintf(parser_debug_file, "\n=== Expression Parse: %s ===\n", context);
        if (expr) {
            fprintf(parser_debug_file, "  Result Type: %s\n", ast_node_type_to_string(expr));
            fprintf(parser_debug_file, "  Value: ");
            switch (expr->type) {
                case NODE_NUMBER:
                    fprintf(parser_debug_file, "%s\n", expr->data.value);
                    break;
                case NODE_IDENTIFIER:
                    fprintf(parser_debug_file, "%s\n", expr->data.value);
                    break;
                case NODE_BINARY_OP:
                    fprintf(parser_debug_file, "Binary Operation (%s)\n", 
                            token_type_to_string(expr->data.binary_op.op));
                    break;
                case NODE_UNARY_OP:
                    fprintf(parser_debug_file, "Unary Operation (%s)\n", 
                            token_type_to_string(expr->data.unary_op.op));
                    break;
                case NODE_ARRAY_ACCESS:
                    fprintf(parser_debug_file, "Array Access (dimensions: %d)\n", 
                            expr->data.array_access.dimensions);
                    break;
                default:
                    fprintf(parser_debug_file, "<unhandled expression type>\n");
            }
            if (expr->child_count > 0) {
                fprintf(parser_debug_file, "  Children:\n");
                for (int i = 0; i < expr->child_count; i++) {
                    debug_print_ast_to(expr->children[i], 2, true, parser_debug_file);
                }
            }
        } else {
            fprintf(parser_debug_file, "  Result: <no expression>\n");
        }
    }
}

void debug_parser_state_to(Parser* parser, const char* context, FILE* dest) {
    if (!(current_flags & DEBUG_PARSER)) return;
    
    fprintf(dest, "  Parser State (%s):\n", context);
    fprintf(dest, "    Current: ");
    if (parser->ctx.current) {
        fprintf(dest, "%s '%s' at %d:%d\n",
                token_type_to_string(parser->ctx.current->type),
                parser->ctx.current->value ? parser->ctx.current->value : "<null>",
                parser->ctx.current->loc.line,
                parser->ctx.current->loc.column);
    } else {
        fprintf(dest, "<null>\n");
    }
    
    fprintf(dest, "    Next: ");
    if (parser->ctx.peek) {
        fprintf(dest, "%s '%s' at %d:%d\n",
                token_type_to_string(parser->ctx.peek->type),
                parser->ctx.peek->value ? parser->ctx.peek->value : "<null>",
                parser->ctx.peek->loc.line,
                parser->ctx.peek->loc.column);
    } else {
        fprintf(dest, "<null>\n");
    }
    
    fprintf(dest, "    Function: %s\n",
            parser->ctx.current_function ? parser->ctx.current_function : "<none>");
    fprintf(dest, "    Scope Level: %d\n",
            parser->ctx.symbols ? parser->ctx.symbols->scope_level : -1);
    fprintf(dest, "    Error Count: %d\n", parser->ctx.error_count);
    fprintf(dest, "    Error Recovery: %s\n",
            parser->panic_mode ? "active" : "inactive");
}

void debug_parser_state(Parser* parser, const char* context) {
    /*if (!(current_flags & DEBUG_PARSER)) return;
    
    fprintf(debug_file, "  Parser State (%s):\n", context);
    fprintf(debug_file, "    Current: ");
    if (parser->ctx.current) {
        fprintf(debug_file, "%s '%s' at %d:%d\n",
                token_type_to_string(parser->ctx.current->type),
                parser->ctx.current->value ? parser->ctx.current->value : "<null>",
                parser->ctx.current->loc.line,
                parser->ctx.current->loc.column);
    } else {
        fprintf(debug_file, "<null>\n");
    }
    
    fprintf(debug_file, "    Next: ");
    if (parser->ctx.peek) {
        fprintf(debug_file, "%s '%s' at %d:%d\n",
                token_type_to_string(parser->ctx.peek->type),
                parser->ctx.peek->value ? parser->ctx.peek->value : "<null>",
                parser->ctx.peek->loc.line,
                parser->ctx.peek->loc.column);
    } else {
        fprintf(debug_file, "<null>\n");
    }
    
    fprintf(debug_file, "    Function: %s\n",
            parser->ctx.current_function ? parser->ctx.current_function : "<none>");
    fprintf(debug_file, "    Scope Level: %d\n",
            parser->ctx.symbols ? parser->ctx.symbols->scope_level : -1);
    fprintf(debug_file, "    Error Count: %d\n", parser->ctx.error_count);
    fprintf(debug_file, "    Error Recovery: %s\n",
            parser->panic_mode ? "active" : "inactive");*/
    debug_parser_state_to(parser, context, debug_file);
}

void debug_parser_scope_enter(Parser* parser, const char* scope_type) {
    if (!(current_flags & DEBUG_PARSER)) return;
    
    fprintf(debug_file, "\n>>> Entering %s Scope >>>\n", scope_type);
    debug_parser_state(parser, "scope entry");

    if (parser_debug_file) {
        fprintf(parser_debug_file, "\n>>> Entering %s Scope >>>\n", scope_type);
        debug_parser_state_to(parser, "scope entry", parser_debug_file);
    }
}

void debug_parser_scope_exit(Parser* parser, const char* scope_type) {
    if (!(current_flags & DEBUG_PARSER)) return;
    
    fprintf(debug_file, "\n<<< Exiting %s Scope <<<\n", scope_type);
    debug_parser_state(parser, "scope exit");

    if (parser_debug_file) {
        fprintf(parser_debug_file, "\n<<< Exiting %s Scope <<<\n", scope_type);
        debug_parser_state_to(parser, "scope exit", parser_debug_file);
    }
}

void debug_parser_error_sync(Parser* parser, const char* context) {
    if (!(current_flags & DEBUG_PARSER)) return;
    
    fprintf(debug_file, "\n!!! Error Recovery: %s !!!\n", context);
    debug_parser_state(parser, "error recovery");
    fprintf(debug_file, "  Synchronizing to next statement...\n\n");

    if (parser_debug_file) {
        fprintf(parser_debug_file, "\n!!! Error Recovery: %s !!!\n", context);
        debug_parser_state_to(parser, "error recovery", parser_debug_file);
        fprintf(parser_debug_file, "  Synchronizing to next statement...\n\n");
    }
}

void debug_parser_procedure_start(Parser* parser, const char* name) {
    if (!(current_flags & DEBUG_PARSER)) return;
    
    fprintf(debug_file, "\n=== Begin Procedure Declaration: %s ===\n", name);
    debug_parser_state(parser, "procedure declaration start");

    if (parser_debug_file) {
        fprintf(parser_debug_file, "\n=== Begin Procedure Declaration: %s ===\n", name);
        debug_parser_state_to(parser, "procedure declaration start", parser_debug_file);
    }
}

void debug_parser_function_start(Parser* parser, const char* name) {
    if (!(current_flags & DEBUG_PARSER)) return;
    
    fprintf(debug_file, "\n=== Begin Function Declaration: %s ===\n", name);
    debug_parser_state(parser, "function declaration start");

    if (parser_debug_file) {
        fprintf(parser_debug_file, "\n=== Begin Function Declaration: %s ===\n", name);
        debug_parser_state_to(parser, "function declaration start", parser_debug_file);
    }
}

void debug_parser_parameter_start(Parser* parser) {
    if (!(current_flags & DEBUG_PARSER)) return;

    fprintf(debug_file, "\n=== Begin Parameter List ===\n");
    debug_parser_state(parser, "parameter list start");

    if (parser_debug_file) {
        fprintf(parser_debug_file, "\n=== Begin Parameter List ===\n");
        debug_parser_state_to(parser, "parameter list start", parser_debug_file);
    }
}

const char* token_type_to_string(TokenType type) {
    switch (type) {
        // Special
        case TOK_EOF: return "EOF";
        
        // Keywords
        case TOK_FUNCTION: return "FUNCTION";
        case TOK_PROCEDURE: return "PROCEDURE";
        case TOK_ENDFUNCTION: return "ENDFUNCTION";
        case TOK_ENDPROCEDURE: return "ENDPROCEDURE";
        case TOK_VAR: return "VAR";
        case TOK_BEGIN: return "BEGIN";
        case TOK_END: return "END";
        case TOK_IF: return "IF";
        case TOK_ELSEIF: return "ELSEIF";
        case TOK_THEN: return "THEN";
        case TOK_ELSE: return "ELSE";
        case TOK_ENDIF: return "ENDIF";
        case TOK_WHILE: return "WHILE";
        case TOK_DO: return "DO";
        case TOK_ENDWHILE: return "ENDWHILE";
        case TOK_FOR: return "FOR";
        case TOK_TO: return "TO";
        case TOK_STEP: return "STEP";
        case TOK_ENDFOR: return "ENDFOR";
        case TOK_REPEAT: return "REPEAT";
        case TOK_UNTIL: return "UNTIL";
        case TOK_RETURN: return "RETURN";
        case TOK_IN: return "IN";
        case TOK_OUT: return "OUT";
        case TOK_INOUT: return "INOUT";
        case TOK_PRINT: return "PRINT";
        case TOK_READ: return "READ";
        
        // Types
        case TOK_INTEGER: return "INTEGER";
        case TOK_REAL: return "REAL";
        case TOK_LOGICAL: return "LOGICAL";
        case TOK_CHARACTER: return "CHARACTER";
        case TOK_ARRAY: return "ARRAY";
        case TOK_OF: return "OF";
        
        // Operators
        case TOK_ASSIGN: return "ASSIGN";
        case TOK_PLUS: return "PLUS";
        case TOK_MINUS: return "MINUS";
        case TOK_MULTIPLY: return "MULTIPLY";
        case TOK_DIVIDE: return "DIVIDE";
        case TOK_MOD: return "MOD";
        case TOK_NOT: return "NOT";
        case TOK_LT: return "LT";
        case TOK_GT: return "GT";
        case TOK_LE: return "LE";
        case TOK_GE: return "GE";
        case TOK_EQ: return "EQ";
        case TOK_NE: return "NE";
        case TOK_AND: return "AND";
        case TOK_OR: return "OR";
        case TOK_DEREF: return "DEREF";
        case TOK_ADDR_OF: return "ADDR_OF";
        case TOK_AT: return "AT";
        
        // Bitwise Operators
        case TOK_RSHIFT: return "RSHIFT";
        case TOK_LSHIFT: return "LSHIFT";
        case TOK_BITAND: return "BITAND";
        case TOK_BITOR: return "BITOR";
        case TOK_BITXOR: return "BITXOR";
        case TOK_BITNOT: return "BITNOT";
        
        // Punctuation
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_LBRACKET: return "LBRACKET";
        case TOK_RBRACKET: return "RBRACKET";
        case TOK_COMMA: return "COMMA";
        case TOK_COLON: return "COLON";
        case TOK_SEMICOLON: return "SEMICOLON";
        case TOK_DOT: return "DOT";
        case TOK_DOTDOT: return "DOTDOT";
        case TOK_DOTDOTDOT: return "DOTDOTDOT";
        
        // Values
        case TOK_IDENTIFIER: return "IDENTIFIER";
        case TOK_NUMBER: return "NUMBER";
        case TOK_STRING: return "STRING";
        case TOK_STRING_LITERAL: return "STRING_LITERAL";
        case TOK_TRUE: return "TRUE";
        case TOK_FALSE: return "FALSE";
        
        default: return "UNKNOWN";
    }
}

void debug_token_type(TokenType type, FILE* out) {
    if (!(current_flags & DEBUG_LEXER)) return;
    
    fprintf(out, "%s (%d): %s\n", token_type_to_string(type), type, 
           debug_get_token_category(type));
}

void debug_lexer_state(Lexer* lexer) {
    if (!(current_flags & DEBUG_LEXER)) return;
    
    fprintf(debug_file, "Lexer State:\n");
    fprintf(debug_file, "  Position: %zu/%zu\n", lexer->current, lexer->source_length);
    fprintf(debug_file, "  Line: %d, Column: %d\n", lexer->line, lexer->column);
    
    // Show context around current position
    if (lexer->current < lexer->source_length) {
        fprintf(debug_file, "  Current char: '%c' (0x%02X)\n", 
               lexer->source[lexer->current],
               (unsigned char)lexer->source[lexer->current]);
        
        // Show a few characters of context
        fprintf(debug_file, "  Context: \"");
        for (int i = -2; i <= 2; i++) {
            size_t pos = lexer->current + i;
            if (pos < lexer->source_length && pos >= 0) {
                char c = lexer->source[pos];
                if (c == '\n') fprintf(debug_file, "\\n");
                else if (c == '\t') fprintf(debug_file, "\\t");
                else fprintf(debug_file, "%c", c);
            }
        }
        fprintf(debug_file, "\"\n");
    }

    if (lexer_debug_file) {
        fprintf(lexer_debug_file, "Lexer State:\n");
        fprintf(lexer_debug_file, "  Position: %zu/%zu\n", lexer->current, lexer->source_length);
        fprintf(lexer_debug_file, "  Line: %d, Column: %d\n", lexer->line, lexer->column);
        
        // Show context around current position
        if (lexer->current < lexer->source_length) {
            fprintf(lexer_debug_file, "  Current char: '%c' (0x%02X)\n", 
                lexer->source[lexer->current],
                (unsigned char)lexer->source[lexer->current]);
            
            // Show a few characters of context
            fprintf(lexer_debug_file, "  Context: \"");
            for (int i = -2; i <= 2; i++) {
                size_t pos = lexer->current + i;
                if (pos < lexer->source_length && pos >= 0) {
                    char c = lexer->source[pos];
                    if (c == '\n') fprintf(lexer_debug_file, "\\n");
                    else if (c == '\t') fprintf(lexer_debug_file, "\\t");
                    else fprintf(lexer_debug_file, "%c", c);
                }
            }
            fprintf(lexer_debug_file, "\"\n");
        }
    }
}

void debug_token_details(Token* token) {
    if (!token || !(current_flags & DEBUG_LEXER)) return;
    
    fprintf(debug_file, "Token {\n");
    fprintf(debug_file, "  Type: ");
    debug_token_type(token->type, debug_file);
    fprintf(debug_file, "  Value: %s\n", token->value ? token->value : "<null>");
    fprintf(debug_file, "  Location: %s:%d:%d\n", 
           token->loc.filename ? token->loc.filename : "<unknown>",
           token->loc.line, token->loc.column);
    fprintf(debug_file, "}\n");

    if (lexer_debug_file) {
        fprintf(lexer_debug_file, "Token {\n");
        fprintf(lexer_debug_file, "  Type: ");
        debug_token_type(token->type, lexer_debug_file);
        fprintf(lexer_debug_file, "  Value: %s\n", token->value ? token->value : "<null>");
        fprintf(lexer_debug_file, "  Location: %s:%d:%d\n", 
            token->loc.filename ? token->loc.filename : "<unknown>",
            token->loc.line, token->loc.column);
        fprintf(lexer_debug_file, "}\n");
    }
}

const char* debug_get_token_category(TokenType type) {
    switch (type) {
        case TOK_EOF:
            return "End of File";
            
        case TOK_FUNCTION:
        case TOK_PROCEDURE:
        case TOK_VAR:
        case TOK_BEGIN:
        case TOK_END:
        case TOK_IF:
        case TOK_THEN:
        case TOK_ELSE:
        case TOK_WHILE:
        case TOK_DO:
        case TOK_FOR:
        case TOK_TO:
        case TOK_RETURN:
            return "Keyword";
            
        case TOK_INTEGER:
        case TOK_REAL:
        case TOK_LOGICAL:
        case TOK_CHARACTER:
        case TOK_ARRAY:
            return "Type";
            
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_MULTIPLY:
        case TOK_DIVIDE:
        case TOK_MOD:
        case TOK_NOT:
        case TOK_AND:
        case TOK_OR:
        case TOK_BITAND:
        case TOK_BITOR:
        case TOK_BITXOR:
        case TOK_BITNOT:
        case TOK_RSHIFT:
        case TOK_LSHIFT:
            return "Operator";
            
        case TOK_ASSIGN:
        case TOK_EQ:
        case TOK_NE:
        case TOK_LT:
        case TOK_GT:
        case TOK_LE:
        case TOK_GE:
            return "Comparison/Assignment";
            
        case TOK_LPAREN:
        case TOK_RPAREN:
        case TOK_LBRACKET:
        case TOK_RBRACKET:
        case TOK_COMMA:
        case TOK_COLON:
        case TOK_SEMICOLON:
        case TOK_DOT:
        case TOK_DOTDOT:
            return "Punctuation";
            
        case TOK_IDENTIFIER:
            return "Identifier";
            
        case TOK_NUMBER:
            return "Literal (Number)";
            
        case TOK_STRING:
        case TOK_STRING_LITERAL:
            return "Literal (String)";
            
        case TOK_TRUE:
        case TOK_FALSE:
            return "Literal (Boolean)";
            
        default:
            return "Unknown";
    }
}