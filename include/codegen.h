#ifndef PLIKE_CODEGEN_H
#define PLIKE_CODEGEN_H

#include "ast.h"
#include "symtable.h"
#include <stdio.h>

typedef struct {
    FILE* output;
    SymbolTable* symbols;
    char* current_function;
    int indent_level;
    bool needs_return;
    bool in_expression;
    struct {
        bool array_adjustment_needed;
        bool in_array_access;
        bool in_array_declaration;
        int dimensions;
        int current_dim;
    } array_context;
} CodeGenerator;

// Generator creation/destruction
CodeGenerator* codegen_create(FILE* output, SymbolTable* symbols);
void codegen_destroy(CodeGenerator* gen);

// Main generation functions
void codegen_generate(CodeGenerator* gen, ASTNode* ast);
void codegen_generate_file(const char* filename, ASTNode* ast);

// Individual generation functions
void codegen_function(CodeGenerator* gen, ASTNode* node);
void codegen_variable_declaration(CodeGenerator* gen, ASTNode* node);
void codegen_statement(CodeGenerator* gen, ASTNode* node);
void codegen_expression(CodeGenerator* gen, ASTNode* node);
void codegen_array_access(CodeGenerator* gen, ASTNode* node);

// Context management
void codegen_enter_function(CodeGenerator* gen, const char* name);
void codegen_exit_function(CodeGenerator* gen);
void codegen_indent(CodeGenerator* gen);
void codegen_dedent(CodeGenerator* gen);

// Header and initialization
void codegen_write_headers(CodeGenerator* gen);
void codegen_write_standard_definitions(CodeGenerator* gen);

// Utility functions
void codegen_write(CodeGenerator* gen, const char* format, ...);
void codegen_write_line(CodeGenerator* gen, const char* format, ...);
char* codegen_get_c_type(const char* plike_type);
char* codegen_get_operator(TokenType op);

#endif // PLIKE_CODEGEN_H