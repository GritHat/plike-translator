#ifndef PLIKE_DEBUG_H
#define PLIKE_DEBUG_H

#include "ast.h"
#include "parser.h"
#include "symtable.h"
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Debug flags
typedef enum {
    DEBUG_LEXER = 1 << 0,
    DEBUG_PARSER = 1 << 1,
    DEBUG_AST = 1 << 2,
    DEBUG_SYMBOLS = 1 << 3,
    DEBUG_CODEGEN = 1 << 4,
    DEBUG_ALL = 0xFFFF
} DebugFlags;


extern DebugFlags current_flags;
extern FILE* debug_file;
extern int trace_depth;

// Initialize debug system
void debug_init(void);

// Set debug flags
void debug_set_flags(DebugFlags flags);

// Enable/disable specific debug features
void debug_enable(DebugFlags flag);
void debug_disable(DebugFlags flag);

// Debug output functions
void debug_print_token(Token* token);
void debug_print_ast(ASTNode* node, int indent, bool force);
void debug_print_symbol_table(SymbolTable* table);
void debug_print_symbol_scope(Scope* scope, int indent);
void debug_print_symbol(Symbol* sym, int indent);
void debug_print_codegen_state(const char* context, void* state);

void debug_ast_node_create(NodeType type, const char* context);
void debug_ast_node_destroy(ASTNode* node, const char* context);
void debug_ast_node_transform(ASTNode* from, ASTNode* to, const char* context);
void debug_ast_add_child(ASTNode* parent, ASTNode* child, const char* context);
void debug_ast_node_complete(ASTNode* node, const char* context);

void debug_symbol_create(Symbol* sym, const char* context);
void debug_symbol_destroy(Symbol* sym, const char* context);
void debug_scope_enter(Scope* scope, const char* context);
void debug_scope_exit(Scope* scope, const char* context);
void debug_symbol_lookup(const char* name, Symbol* result, const char* context);
void debug_symbol_bounds_update(Symbol* sym, ArrayBoundsData* bounds, const char* context);
void debug_symbol_table_operation(const char* operation, const char* details);

void debug_codegen_state(CodeGenerator* gen, const char* context);
void debug_codegen_expression(CodeGenerator* gen, ASTNode* expr, const char* context);
void debug_codegen_array(CodeGenerator* gen, ASTNode* array, const char* context);
void debug_codegen_function(CodeGenerator* gen, ASTNode* func, const char* context);
void debug_codegen_block(CodeGenerator* gen, const char* context, int new_level);
void debug_codegen_symbol_resolution(CodeGenerator* gen, const char* name, Symbol* sym, const char* context);

void debug_parser_state(Parser* parser, const char* context);
void debug_parser_state_to(Parser* parser, const char* context, FILE* dest);
void debug_parser_expression(ASTNode* expr, const char* context);
void debug_parser_scope_enter(Parser* parser, const char* scope_type);
void debug_parser_scope_exit(Parser* parser, const char* scope_type);
void debug_parser_rule_start(Parser* parser, const char* rule_name);
void debug_parser_rule_end(Parser* parser, const char* rule_name, ASTNode* result);
void debug_parser_token_consume(Parser* parser, Token* token, const char* expected);
void debug_parser_error_sync(Parser* parser, const char* context);
void debug_print_parser_state(Parser* parser);
void debug_parser_procedure_start(Parser* parser, const char* name);
void debug_parser_function_start(Parser* parser, const char* name);
void debug_parser_parameter_start(Parser* parser);

void debug_token_type(TokenType type, FILE* out);
void debug_lexer_state(Lexer* lexer);
void debug_token_details(Token* token);
const char* debug_get_token_category(TokenType type);

// Parser trace functions
//void debug_trace_enter(const char* function_name);
//void debug_trace_exit(const char* function_name, ASTNode* result);
void debug_print_parser_state_d(Parser* parser);

// Visualization functions
void debug_visualize_ast(ASTNode* node, const char* filename);
void debug_visualize_symbol_table(SymbolTable* table, const char* filename);
void debug_visualize_codegen(CodeGenerator* node, const char* filename);  // For control flow graphs etc.

// Error trace functions
void debug_mark_error_location(SourceLocation loc);
void debug_print_error_context(SourceLocation loc);


#endif // PLIKE_DEBUG_H