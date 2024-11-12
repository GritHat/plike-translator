#ifndef PLIKE_PARSER_H
#define PLIKE_PARSER_H

#include "lexer.h"
#include "ast.h"
#include "symtable.h"

//typedef struct ParserStruct Parser;

typedef struct {
    Token* current;          // Current token
    Token* prev;          // Current token
    Token* peek;            // Look-ahead token
    Lexer* lexer;          // Lexer instance
    SymbolTable* symbols;   // Symbol table
    char* current_record;
    char* current_function; // Name of function being parsed
    bool is_function;
    bool in_loop;          // Track if we're inside a loop
    int error_count;       // Number of parsing errors
} ParserContext;

typedef struct {
    ParserContext ctx;
    bool had_error;
    bool panic_mode;
} Parser;

// Parser creation/destruction
Parser* parser_create(Lexer* lexer);
void parser_destroy(Parser* parser);

// Main parsing functions
ASTNode* parser_parse(Parser* parser);
ASTNode* parser_parse_file(const char* filename);

// Individual parsing functions
ASTNode* parse_function(Parser* parser);
ASTNode* parse_procedure(Parser* parser);
ASTNode* parse_expression(Parser* parser);
ASTNode* parse_parameter_list(Parser* parser);

// Error handling
void parser_error(Parser* parser, const char* message);
bool parser_sync_to_next_statement(Parser* parser);

// Utility functions
bool parser_match(Parser* parser, TokenType type);
bool parser_check(Parser* parser, TokenType type);
Token* parser_advance(Parser* parser);
Token* parser_peek(Parser* parser);
bool parser_at_end(Parser* parser);

#endif // PLIKE_PARSER_H