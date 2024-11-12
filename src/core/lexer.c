#include "lexer.h"
#include "errors.h"
#include "config.h"
#include "utils.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_BUFFER_SIZE 128
#define MAX_IDENTIFIER_LENGTH 255
#define MAX_NUMBER_LENGTH 64

const Keyword keywords_mixed[] = {
    {"function", TOK_FUNCTION},
    {"procedure", TOK_PROCEDURE},
    {"endfunction", TOK_ENDFUNCTION},
    {"endprocedure", TOK_ENDPROCEDURE},
    {"var", TOK_VAR},
    {"begin", TOK_BEGIN},
    {"end", TOK_END},
    {"if", TOK_IF},
    {"then", TOK_THEN},
    {"else", TOK_ELSE},
    {"elseif", TOK_ELSEIF},
    {"endif", TOK_ENDIF},
    {"while", TOK_WHILE},
    {"do", TOK_DO},
    {"endwhile", TOK_ENDWHILE},
    {"for", TOK_FOR},
    {"to", TOK_TO},
    {"step", TOK_STEP},
    {"endfor", TOK_ENDFOR},
    {"return", TOK_RETURN},
    {"repeat", TOK_REPEAT},
    {"until", TOK_UNTIL},
    {"in", TOK_IN},
    {"out", TOK_OUT},
    {"inout", TOK_INOUT},
    {"in/out", TOK_INOUT},
    {"print", TOK_PRINT},
    {"read", TOK_READ},
    {"integer", TOK_INTEGER},
    {"real", TOK_REAL},
    {"logical", TOK_LOGICAL},
    {"character", TOK_CHARACTER},
    {"array", TOK_ARRAY},
    {"of", TOK_OF},
    {"type", TOK_TYPE},
    {"record", TOK_RECORD},
    {"and", TOK_AND},
    {".and.", TOK_AND},
    {"or", TOK_OR},
    {".or.", TOK_OR},
    {"not", TOK_NOT},
    {".not.", TOK_NOT},
    {"eq", TOK_EQ},
    {".eq.", TOK_EQ},
    {"equal", TOK_EQ},
    {".equal.", TOK_EQ},
    {"equals", TOK_EQ},
    {".equals.", TOK_EQ},
    {"ne", TOK_NE},
    {".ne.", TOK_NE},
    {"notequal", TOK_NE},
    {".notequal.", TOK_NE},
    {"notequals", TOK_NE},
    {".notequals.", TOK_NE},
    {"true", TOK_TRUE},
    {".true.", TOK_TRUE},
    {"false", TOK_FALSE},
    {".false.", TOK_FALSE},
    {"mod", TOK_MOD},
    {".mod.", TOK_MOD},
    {NULL, TOK_EOF}
};

const Keyword keywords_standard[] = {
    {"function", TOK_FUNCTION},
    {"procedure", TOK_PROCEDURE},
    {"endfunction", TOK_ENDFUNCTION},
    {"endprocedure", TOK_ENDPROCEDURE},
    {"var", TOK_VAR},
    {"begin", TOK_BEGIN},
    {"end", TOK_END},
    {"if", TOK_IF},
    {"then", TOK_THEN},
    {"else", TOK_ELSE},
    {"elseif", TOK_ELSEIF},
    {"endif", TOK_ENDIF},
    {"while", TOK_WHILE},
    {"do", TOK_DO},
    {"endwhile", TOK_ENDWHILE},
    {"for", TOK_FOR},
    {"to", TOK_TO},
    {"step", TOK_STEP},
    {"endfor", TOK_ENDFOR},
    {"return", TOK_RETURN},
    {"repeat", TOK_REPEAT},
    {"until", TOK_UNTIL},
    {"in", TOK_IN},
    {"out", TOK_OUT},
    {"inout", TOK_INOUT},
    {"in/out", TOK_INOUT},
    {"print", TOK_PRINT},
    {"read", TOK_READ},
    {"integer", TOK_INTEGER},
    {"real", TOK_REAL},
    {"logical", TOK_LOGICAL},
    {"character", TOK_CHARACTER},
    {"array", TOK_ARRAY},
    {"of", TOK_OF},
    {"type", TOK_TYPE},
    {"record", TOK_RECORD},
    {"and", TOK_AND},
    {"or", TOK_OR},
    {"not", TOK_NOT},
    {"eq", TOK_EQ},
    {"equal", TOK_EQ},
    {"equals", TOK_EQ},
    {"ne", TOK_NE},
    {"notequal", TOK_NE},
    {"notequals", TOK_NE},
    {"true", TOK_TRUE},
    {"false", TOK_FALSE},
    {"mod", TOK_MOD},
    {NULL, TOK_EOF}
};

const Keyword keywords_dotted[] = {
    {"function", TOK_FUNCTION},
    {"procedure", TOK_PROCEDURE},
    {"endfunction", TOK_ENDFUNCTION},
    {"endprocedure", TOK_ENDPROCEDURE},
    {"var", TOK_VAR},
    {"begin", TOK_BEGIN},
    {"end", TOK_END},
    {"if", TOK_IF},
    {"then", TOK_THEN},
    {"else", TOK_ELSE},
    {"elseif", TOK_ELSEIF},
    {"endif", TOK_ENDIF},
    {"while", TOK_WHILE},
    {"do", TOK_DO},
    {"endwhile", TOK_ENDWHILE},
    {"for", TOK_FOR},
    {"to", TOK_TO},
    {"step", TOK_STEP},
    {"endfor", TOK_ENDFOR},
    {"return", TOK_RETURN},
    {"repeat", TOK_REPEAT},
    {"until", TOK_UNTIL},
    {"in", TOK_IN},
    {"out", TOK_OUT},
    {"inout", TOK_INOUT},
    {"in/out", TOK_INOUT},
    {"print", TOK_PRINT},
    {"read", TOK_READ},
    {"integer", TOK_INTEGER},
    {"real", TOK_REAL},
    {"logical", TOK_LOGICAL},
    {"character", TOK_CHARACTER},
    {"array", TOK_ARRAY},
    {"of", TOK_OF},
    {"type", TOK_TYPE},
    {"record", TOK_RECORD},
    {".and.", TOK_AND},
    {".or.", TOK_OR},
    {".not.", TOK_NOT},
    {".eq.", TOK_EQ},
    {".equal.", TOK_EQ},
    {".equals.", TOK_EQ},
    {".ne.", TOK_NE},
    {".notequal.", TOK_NE},
    {".notequals.", TOK_NE},
    {".true.", TOK_TRUE},
    {".false.", TOK_FALSE},
    {".mod.", TOK_MOD},
    {NULL, TOK_EOF}
};

const Keyword* keywords = keywords_mixed;

// Helper function declarations
static bool is_at_end(Lexer* lexer);
static char advance(Lexer* lexer);
static char peek(Lexer* lexer);
static char peek_next(Lexer* lexer);
static bool match(Lexer* lexer, char expected);
static Token* make_token(Lexer* lexer, TokenType type);
static Token* error_token(Lexer* lexer, const char* message);
static bool is_alpha(char c);
static bool is_digit(char c);
static void skip_whitespace(Lexer* lexer);
static TokenType check_keyword(Lexer* lexer, int start, int length, const char* rest, TokenType type);
static TokenType identifier_type(Lexer* lexer);
static Token* scan_token(Lexer* lexer);

// Initialize lexer with source file
Lexer* lexer_create(const char* filename) {
    if (current_flags & DEBUG_LEXER) {
        fprintf(debug_file, "=== Creating Lexer ===\n");
        fprintf(debug_file, "Input file: %s\n", filename);
    }
    
    verbose_print("Opening file: %s\n", filename);
    FILE* file = fopen(filename, "r");
    if (!file) {
        error_report(ERROR_INTERNAL, SEVERITY_FATAL, 
                    (SourceLocation){0, 0, filename},
                    "Could not open file '%s'", filename);
        return NULL;
    }

    verbose_print("Getting file size...\n");
    // Get file size
    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    verbose_print("Allocating buffer of size %zu\n", size);
    // Allocate buffer and read file
    char* source = (char*)malloc(size + 1);
    if (!source) {
        fclose(file);
        error_report(ERROR_INTERNAL, SEVERITY_FATAL,
                    (SourceLocation){0, 0, filename},
                    "Out of memory");
        return NULL;
    }

    verbose_print("Reading file content...\n");
    size_t bytes_read = fread(source, 1, size, file);
    fclose(file);

    if (bytes_read < size) {
        free(source);
        error_report(ERROR_INTERNAL, SEVERITY_FATAL,
                    (SourceLocation){0, 0, filename},
                    "Could not read file '%s'", filename);
        return NULL;
    }

    source[bytes_read] = '\0';

    verbose_print("Creating lexer structure...\n");
    Lexer* lexer = (Lexer*)malloc(sizeof(Lexer));
    if (!lexer) {
        free(source);
        error_report(ERROR_INTERNAL, SEVERITY_FATAL,
                    (SourceLocation){0, 0, filename},
                    "Out of memory");
        return NULL;
    }

    verbose_print("Initializing lexer fields...\n");
    lexer->filename = filename;
    lexer->source = source;
    lexer->source_length = bytes_read;
    lexer->current = 0;
    lexer->start = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->line_start = source;

    verbose_print("Lexer creation completed\n");
    if (current_flags & DEBUG_LEXER) {
        fprintf(debug_file, "Lexer created successfully\n");
        fprintf(debug_file, "Source length: %zu bytes\n", lexer->source_length);
        fprintf(debug_file, "\n");
    }
    return lexer;
}

void lexer_destroy(Lexer* lexer) {
    if (current_flags & DEBUG_LEXER) {
        fprintf(debug_file, "=== Destroying Lexer ===\n");
        fprintf(debug_file, "Total lines processed: %d\n", lexer->line);
    }
    
    if (lexer) {
        free(lexer->source);
        free(lexer);
    }

    if (current_flags & DEBUG_LEXER) {
        fprintf(debug_file, "Lexer destroyed successfully\n\n");
    }
}

void token_destroy(Token* token) {
    if (token) {
        free(token->value);
        free(token);
    }
}

// Lexer helper functions
static bool is_at_end(Lexer* lexer) {
    return lexer->current >= lexer->source_length;
}

static void lexer_rewind(Lexer* lexer, int current, int column) {
    lexer->current = current;
    lexer->column = column;
}

static char advance(Lexer* lexer) {
    lexer->current++;
    lexer->column++;
    return lexer->source[lexer->current - 1];
}

static char peek(Lexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->source[lexer->current];
}

static char peek_next(Lexer* lexer) {
    if (lexer->current + 1 >= lexer->source_length) return '\0';
    return lexer->source[lexer->current + 1];
}

static bool match(Lexer* lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (lexer->source[lexer->current] != expected) return false;
    lexer->current++;
    lexer->column++;
    return true;
}

SourceLocation token_clone_location(Token* source_token) {
    SourceLocation clone;
    clone.column = source_token->loc.column;
    clone.line = source_token->loc.line;
    clone.filename = source_token->loc.filename;
    return clone;
}

static Token* make_token(Lexer* lexer, TokenType type) {
    Token* token = (Token*)malloc(sizeof(Token));
    if (!token) return NULL;

    size_t length = lexer->current - lexer->start;
    token->value = (char*)malloc(length + 1);
    if (!token->value) {
        free(token);
        return NULL;
    }

    memcpy(token->value, &lexer->source[lexer->start], length);
    token->value[length] = '\0';
    token->type = type;
    token->loc.line = lexer->line;
    token->loc.column = lexer->column - length;
    token->loc.filename = lexer->filename;

    // Map operator symbols to their token types
    if (type == TOK_LT) token->value = strdup("<");
    else if (type == TOK_GT) token->value = strdup(">");
    else if (type == TOK_LE) token->value = strdup("<=");
    else if (type == TOK_GE) token->value = strdup(">=");
    else if (type == TOK_EQ) token->value = strdup("==");
    else if (type == TOK_NE) token->value = strdup("!=");

    return token;
}

static Token* error_token(Lexer* lexer, const char* message) {
    Token* token = (Token*)malloc(sizeof(Token));
    if (!token) return NULL;

    token->value = strdup(message);
    token->type = TOK_EOF;  // Use EOF for error tokens
    token->loc.line = lexer->line;
    token->loc.column = lexer->column;
    token->loc.filename = lexer->filename;

    return token;
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_dimension_specifier(const char* text) {
    // Check if the text matches the pattern of a number followed by 'd' or 'D'
    char* endptr;
    long num = strtol(text, &endptr, 10);
    
    // Valid if we found a number > 0 and it's followed by 'd' or 'D' with nothing after
    return num > 0 && (*endptr == 'd' || *endptr == 'D') && *(endptr + 1) == '\0';
}

static void skip_whitespace(Lexer* lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
            case '\n':
                lexer->line++;
                lexer->column = 1;
                lexer->line_start = &lexer->source[lexer->current + 1];
                advance(lexer);
                break;
            case '/':
                // Check if this is part of "in/out"
                if (lexer->current >= 2 && 
                    strncmp(&lexer->source[lexer->current - 2], "in", 2) == 0 &&
                    lexer->current + 3 < lexer->source_length &&
                    strncmp(&lexer->source[lexer->current + 1], "out", 3) == 0) {
                    return;  // Don't skip - this is part of "in/out"
                }
                
                // Handle comments
                if (peek_next(lexer) == '/') {
                    while (peek(lexer) != '\n' && !is_at_end(lexer)) advance(lexer);
                } else if (peek_next(lexer) == '*') {
                    advance(lexer);
                    advance(lexer);
                    while (!is_at_end(lexer) && 
                           !(peek(lexer) == '*' && peek_next(lexer) == '/')) {
                        if (peek(lexer) == '\n') {
                            lexer->line++;
                            lexer->column = 1;
                            lexer->line_start = &lexer->source[lexer->current + 1];
                        }
                        advance(lexer);
                    }
                    if (!is_at_end(lexer)) {
                        advance(lexer);
                        advance(lexer);
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static TokenType identifier_type(Lexer* lexer) {
    size_t length = lexer->current - lexer->start;
    if (length == 6 && strncmp(&lexer->source[lexer->start], "in/out", 6) == 0) {
        return TOK_INOUT;
    }
    char identifier[MAX_IDENTIFIER_LENGTH + 1];
    if (length > MAX_IDENTIFIER_LENGTH) length = MAX_IDENTIFIER_LENGTH;
    strncpy(identifier, &lexer->source[lexer->start], length);
    identifier[length] = '\0';

    // First check for dimension specifiers
    if (is_dimension_specifier(identifier)) {
        verbose_print("Found dimension specifier: %s\n", identifier);
        return TOK_IDENTIFIER;  // Treat dimension specifiers as identifiers
    }
    
    // Check for keywords
    /*for (const Keyword* k = keywords; k->text != NULL; k++) {
        size_t len = strlen(k->text);
        if (lexer->current - lexer->start == len &&
            strncmp(&lexer->source[lexer->start], k->text, len) == 0) {
            verbose_print("Matched keyword: %s -> token type: %d\n", k->text, k->type);
            return k->type;
        }
    }*/
    for (const Keyword* k = keywords; k->text != NULL; k++) {
        if (strcasecmp(identifier, k->text) == 0) {
            return k->type;
        }
    }

    //char identifier[MAX_IDENTIFIER_LENGTH + 1];
    size_t len = lexer->current - lexer->start;
    if (len > MAX_IDENTIFIER_LENGTH) len = MAX_IDENTIFIER_LENGTH;
    strncpy(identifier, &lexer->source[lexer->start], len);
    identifier[len] = '\0';

    /*if (strcasecmp(identifier, "eq") == 0 ||
        strcasecmp(identifier, ".eq.") == 0 ||
        strcasecmp(identifier, "equal") == 0 ||
        strcasecmp(identifier, ".equal.") == 0 ||
        strcasecmp(identifier, "equals") == 0 ||
        strcasecmp(identifier, ".equals.") == 0) {
        return TOK_EQ;
    }*/

    verbose_print("No keyword match, treating as identifier: %s\n", identifier);

    return TOK_IDENTIFIER;
}

static bool is_hex_digit(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static bool is_binary_digit(char c) {
    return c == '0' || c == '1';
}

static bool is_octal_digit(char c) {
    return c >= '0' && c <= '7';
}

static Token* scan_number(Lexer* lexer) {
    bool is_float = false;
    const char* start = &lexer->source[lexer->start];

    // Check for hex, octal, or binary prefix
    if (start[0] == '0' && lexer->start + 1 < lexer->source_length) {
        char prefix = tolower(start[1]);
        if (prefix == 'x') {  // Hexadecimal
            lexer->current += 2;  // Skip '0x'
            while (is_hex_digit(peek(lexer))) {
                advance(lexer);
            }
            return make_token(lexer, TOK_NUMBER);
        }
        else if (prefix == 'o') {  // Octal
            lexer->current += 2;  // Skip '0o'
            while (is_octal_digit(peek(lexer))) {
                advance(lexer);
            }
            return make_token(lexer, TOK_NUMBER);
        }
        else if (prefix == 'b') {  // Binary
            lexer->current += 2;  // Skip '0b'
            while (is_binary_digit(peek(lexer))) {
                advance(lexer);
            }
            return make_token(lexer, TOK_NUMBER);
        }
    }

    // Regular decimal number or float
    while (is_digit(peek(lexer))) {
        advance(lexer);
    }

    int current = lexer->current;
    int column = lexer->column;
    // Look for fractional part
    if (peek(lexer) == '.') {
        is_float = true;
        advance(lexer);  // Consume the '.'
        
        // Handle the case of just a trailing dot (e.g., "123.")
        if (!is_digit(peek(lexer))) {
            if (is_alpha(peek(lexer)) || peek(lexer) == '.') {
                lexer_rewind(lexer, current, column);
                return make_token(lexer, TOK_NUMBER);
            }
        } else {
            // Consume fractional digits
            while (is_digit(peek(lexer))) {
                advance(lexer);
            }
        }
    }

    // Look for float suffix 'f'
    if (peek(lexer) == 'f' || peek(lexer) == 'F') {
        is_float = true;
        advance(lexer);
    }

    return make_token(lexer, TOK_NUMBER);
}

// Add string literal support in lexer.c
static Token* scan_string(Lexer* lexer) {    
    lexer->start = lexer->current;
    while (peek(lexer) != '"' && !is_at_end(lexer)) {
        if (peek(lexer) == '\n') {
            error_report(ERROR_LEXICAL, SEVERITY_ERROR,
                        (SourceLocation){lexer->line, lexer->column, lexer->filename},
                        "Unterminated string");
            return NULL;
        }
        advance(lexer);
    }

    if (is_at_end(lexer)) {
        error_report(ERROR_LEXICAL, SEVERITY_ERROR,
                    (SourceLocation){lexer->line, lexer->column, lexer->filename},
                    "Unterminated string");
        return NULL;
    }

    // Create token before consuming closing quote
    Token* token = make_token(lexer, TOK_STRING_LITERAL);
    
    // Skip the closing quote
    advance(lexer);
    return token;
}

Token* lexer_next_token(Lexer* lexer) {
    if (current_flags & DEBUG_LEXER) {
        fprintf(debug_file, "=== Starting Token Scan ===\n");
        debug_lexer_state(lexer);
    }

    Token* token = NULL;

    skip_whitespace(lexer);
    lexer->start = lexer->current;

    if (is_at_end(lexer)) {
        if (current_flags & DEBUG_LEXER) {
            fprintf(debug_file, "Reached end of file\n");
        }
        token = make_token(lexer, TOK_EOF);
        goto scanned;
    }

    char c = advance(lexer);
    //printf("next c %c\n", c);
    if (c == '"') {
        return scan_string(lexer);
    }

    if (is_digit(c)) {
        if (!is_at_end(lexer) && (peek(lexer) == 'd' || peek(lexer) == 'D')) {
            advance(lexer); // Consume the 'd' or 'D'
            token = make_token(lexer, TOK_IDENTIFIER);
            goto scanned;
        }
        token = scan_number(lexer);
        goto scanned;
    }

    /*
    // Check for dimensional array specifier (e.g., "2D", "3d")
    if (is_digit(c)) {
        // Look ahead for 'd' or 'D'
        if (!is_at_end(lexer) && (peek(lexer) == 'd' || peek(lexer) == 'D')) {
            advance(lexer); // Consume the 'd' or 'D'
            return make_token(lexer, TOK_IDENTIFIER);
        }
        
        // Handle regular numbers
        while (is_digit(peek(lexer))) advance(lexer);
        if (peek(lexer) == '.' && is_digit(peek_next(lexer))) {
            advance(lexer); // Consume the "."
            while (is_digit(peek(lexer))) advance(lexer);
        }
        return make_token(lexer, TOK_NUMBER);
    }*/

    // Identifiers
    if (is_alpha(c)) {
        size_t word_start = lexer->start;
        while (is_alpha(peek(lexer)) || is_digit(peek(lexer))) {
            advance(lexer);
        }

         // Get the full identifier
        size_t length = lexer->current - word_start;
        
        // Check for "in" explicitly
        if (length == 2 && strncmp(&lexer->source[word_start], "in", 2) == 0) {
            // Peek ahead for "/out"
            char next = peek(lexer);
            if (next == '/') {
                advance(lexer);  // consume '/'
                if (lexer->current + 2 < lexer->source_length &&
                    strncmp(&lexer->source[lexer->current], "out", 3) == 0) {
                    lexer->current += 3;  // consume 'out'
                    token = make_token(lexer, TOK_INOUT);
                    goto scanned;
                }
                // If not followed by "out", back up
                lexer->current--;
            }
        }
        token = make_token(lexer, identifier_type(lexer));
        goto scanned;
    }

    // Special handling for assignment and equality based on config
    if (c == ':' && peek(lexer) == '=') {
        advance(lexer);
        token = make_token(lexer, TOK_ASSIGN);
        goto scanned;
    }

    if (c == '=') {
        if (peek(lexer) == '=') {
            advance(lexer);
            token = make_token(lexer, TOK_EQ);
            goto scanned;
        } else {
            if (g_config.assignment_style == ASSIGNMENT_EQUALS) {
                token = make_token(lexer, TOK_ASSIGN);
                goto scanned;
            } else {
                token = make_token(lexer, TOK_EQ);
                goto scanned;
            }
        }
    }

    if (c == '*') {
        // Save current position to peek ahead
        size_t saved_pos = lexer->current;
        int saved_col = lexer->column;
        
        // Skip any whitespace
        char next = peek(lexer);
        while (isspace(next)) {
            advance(lexer);
            next = peek(lexer);
        }

        // If followed immediately by identifier or opening paren, it's likely dereference
        if (is_alpha(next) || next == '(') {
            // Look behind for operators or opening delimiters
            bool preceded_by_op = false;
            for (size_t i = lexer->start - 1; i > 0; i--) {
                char prev = lexer->source[i];
                if (!isspace(prev)) {
                    preceded_by_op = (prev == '=' || prev == '(' || prev == ',' ||
                                    prev == '+' || prev == '-' || prev == '*' ||
                                    prev == '/' || prev == '&' || prev == '|' ||
                                    prev == '^' || prev == '<' || prev == '>' ||
                                    prev == '!');
                    break;
                }
            }
            
            if (preceded_by_op) {
                token = make_token(lexer, TOK_DEREF);
                goto scanned;
            }
        }
        
        // Restore position if we didn't find a clear dereference case
        lexer->current = saved_pos;
        lexer->column = saved_col;
        token = make_token(lexer, TOK_MULTIPLY);
        goto scanned;
    }

    if (c == '&') {
        if (match(lexer, '&')) return make_token(lexer, TOK_AND);
        // Save current position to peek ahead
        size_t saved_pos = lexer->current;
        int saved_col = lexer->column;
        
        // Skip any whitespace
        char next = peek(lexer);
        while (isspace(next)) {
            advance(lexer);
            next = peek(lexer);
        }

        // If followed immediately by identifier or opening paren, it's likely dereference
        if (is_alpha(next) || next == '(') {
            // Look behind for operators or opening delimiters
            bool preceded_by_op = false;
            for (size_t i = lexer->start - 1; i > 0; i--) {
                char prev = lexer->source[i];
                if (!isspace(prev)) {
                    preceded_by_op = (prev == '=' || prev == '(' || prev == ',' ||
                                    prev == '+' || prev == '-' || prev == '*' ||
                                    prev == '/' || prev == '&' || prev == '|' ||
                                    prev == '^' || prev == '<' || prev == '>' ||
                                    prev == '!');
                    break;
                }
            }
            
            if (preceded_by_op) {
                token = make_token(lexer, TOK_ADDR_OF);
                goto scanned;
            }
        }
        
        // Restore position if we didn't find a clear dereference case
        //lexer->current = saved_pos;
        //lexer->column = saved_col;
        lexer_rewind(lexer, saved_pos, saved_col);
        token = make_token(lexer, TOK_BITAND);
        goto scanned;
    }


    // Multi-character operators
    switch (c) {
        case '(': token = make_token(lexer, TOK_LPAREN); goto scanned;
        case ')': token = make_token(lexer, TOK_RPAREN); goto scanned;
        case '[': token = make_token(lexer, TOK_LBRACKET); goto scanned;
        case ']': token = make_token(lexer, TOK_RBRACKET); goto scanned;
        case ',': token = make_token(lexer, TOK_COMMA); goto scanned;
        case ':': token = make_token(lexer, TOK_COLON); goto scanned;
        case ';': token = make_token(lexer, TOK_SEMICOLON); goto scanned;
        case '.':
            // Look ahead to check if this might be a dotted operator
            size_t pos = lexer->current;
            char buffer[32] = ".";  // Start with the dot
            int buf_pos = 1;
            
            // Read characters until we find another dot or space
            while (pos < lexer->source_length && 
                buf_pos < sizeof(buffer) - 1 && 
                (isalpha(lexer->source[pos]) || lexer->source[pos] == '.')) {
                buffer[buf_pos++] = lexer->source[pos++];
            }
            buffer[buf_pos] = '\0';
            
            // Check if we found a dotted operator
            for (const Keyword* k = keywords; k->text != NULL; k++) {
                if (strcasecmp(buffer, k->text) == 0) {
                    // Found a dotted operator, consume all the characters
                    lexer->current = pos;
                    lexer->column += (buf_pos - 1);
                    token = make_token(lexer, k->type);
                    if (token) {
                        token->value = strdup(buffer);
                    }
                    goto scanned;
                }
            }
            
            // Not a dotted operator, handle as normal dot
            if (match(lexer, '.')) {
                if (match(lexer, '.')) {
                    token = make_token(lexer, TOK_DOTDOTDOT);
                    goto scanned;
                }
                token = make_token(lexer, TOK_DOTDOT);
                goto scanned;
            }
            token = make_token(lexer, TOK_DOT);
            goto scanned;
        case '+': token = make_token(lexer, TOK_PLUS); goto scanned;
        case '-': if (match(lexer, '>')) { token = make_token(lexer, TOK_ARROW); goto scanned; }
            token = make_token(lexer, TOK_MINUS); goto scanned;
        //case '*': return make_token(lexer, TOK_MULTIPLY);
        case '/': token = make_token(lexer, TOK_DIVIDE); goto scanned;
        case '<':
            if (match(lexer, '=')) { token = make_token(lexer, TOK_LE); goto scanned; }
            if (match(lexer, '<')) { token = make_token(lexer, TOK_LSHIFT); goto scanned; }
            token = make_token(lexer, TOK_LT); goto scanned;
        case '>':
            if (match(lexer, '=')) { token = make_token(lexer, TOK_GE); goto scanned; }
            if (match(lexer, '>')) { token = make_token(lexer, TOK_RSHIFT); goto scanned; }
            token = make_token(lexer, TOK_GT); goto scanned;
        case '!':
            if (match(lexer, '=')) { token = make_token(lexer, TOK_NE); goto scanned; }
            token = make_token(lexer, TOK_NOT); goto scanned;
        //case '&':
        //    if (match(lexer, '&')) return make_token(lexer, TOK_AND);
        //    return make_token(lexer, TOK_BITAND);
        case '|':
            if (match(lexer, '|')) { token = make_token(lexer, TOK_OR); goto scanned; }
            token = make_token(lexer, TOK_BITOR); goto scanned;
        case '^': token = make_token(lexer, TOK_BITXOR); goto scanned;
        case '~': token = make_token(lexer, TOK_BITNOT); goto scanned;
        case '@': token = make_token(lexer, TOK_AT); goto scanned;
        case '%': token = make_token(lexer, TOK_MOD); goto scanned;
    }

    return error_token(lexer, "Unexpected character");
scanned:
    if (current_flags & DEBUG_LEXER) {
        fprintf(debug_file, "=== Token Scanned ===\n");
        debug_token_details(token);
        fprintf(debug_file, "\n");
    }
    return token;
}

