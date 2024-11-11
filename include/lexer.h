#ifndef PLIKE_LEXER_H
#define PLIKE_LEXER_H

#include <stddef.h>
#include <stdio.h>

typedef enum {
    TOK_EOF = 0,
    
    // Keywords
    TOK_FUNCTION,
    TOK_PROCEDURE,
    TOK_ENDFUNCTION,
    TOK_ENDPROCEDURE,
    TOK_VAR,
    TOK_BEGIN,
    TOK_END,
    TOK_IF,
    TOK_ELSEIF,
    TOK_THEN,
    TOK_ELSE,
    TOK_ENDIF,
    TOK_WHILE,
    TOK_DO,
    TOK_ENDWHILE,
    TOK_FOR,
    TOK_TO,
    TOK_STEP,
    TOK_ENDFOR,
    TOK_REPEAT,
    TOK_UNTIL,
    TOK_RETURN,
    TOK_IN,
    TOK_OUT,
    TOK_INOUT,
    TOK_PRINT,
    TOK_READ,
    
    // Types
    TOK_INTEGER,
    TOK_REAL,
    TOK_LOGICAL,
    TOK_CHARACTER,
    TOK_ARRAY,
    TOK_OF,
    
    // Operators
    TOK_ASSIGN,       // := or =
    TOK_PLUS,         // +
    TOK_MINUS,        // -
    TOK_MULTIPLY,     // *
    TOK_DIVIDE,       // /
    TOK_MOD,          // %
    TOK_NOT,          // !
    TOK_LT,           // <
    TOK_GT,           // >
    TOK_LE,           // <=
    TOK_GE,           // >=
    TOK_EQ,           // ==, eq, .eq., equal, .equal., equals, .equals., = (when it's not assignment)
    TOK_NE,           // !=, ne, .ne., notequal, .notequal., notequals, .notequals.
    TOK_AND,          // &&, .and., and
    TOK_OR,           // ||, .or., or
    TOK_DEREF,        // *
    TOK_ADDR_OF,      // &
    TOK_AT,           // @

    // Bitwise Operators
    TOK_RSHIFT,        // >>
    TOK_LSHIFT,        // <<
    TOK_BITAND,        // &
    TOK_BITOR,         // |
    TOK_BITXOR,        // ^
    TOK_BITNOT,        // ~
    
    // Punctuation
    TOK_LPAREN,         // (
    TOK_RPAREN,         // )
    TOK_LBRACKET,       // [
    TOK_RBRACKET,       // ]
    TOK_COMMA,          // ,
    TOK_COLON,          // :
    TOK_SEMICOLON,      // ;
    TOK_DOT,            // .
    TOK_DOTDOT,         // ..
    TOK_DOTDOTDOT,      // ...
    
    // Values
    TOK_IDENTIFIER,
    TOK_NUMBER,
    TOK_STRING,
    TOK_STRING_LITERAL,
    TOK_TRUE,           // true, .true.
    TOK_FALSE           // false, .false.
} TokenType;

typedef struct {
    int line;
    int column;
    const char* filename;
} SourceLocation;

typedef struct {
    TokenType type;
    char* value;        // Dynamically allocated string value
    SourceLocation loc;
} Token;

typedef struct LexerStruct {
    const char* filename;
    char* source;
    size_t source_length;
    size_t current;
    size_t start;
    int line;
    int column;
    char* line_start;
} Lexer;

typedef struct LexerStruct Lexer;

// Lexer interface
Lexer* lexer_create(const char* filename);
void lexer_destroy(Lexer* lexer);
Token* lexer_next_token(Lexer* lexer);
void token_destroy(Token* token);
const char* token_type_to_string(TokenType type);
void lexer_report_error(Lexer* lexer, const char* message);
SourceLocation token_clone_location(Token* source_token);

#endif // PLIKE_LEXER_H