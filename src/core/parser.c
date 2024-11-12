#include "parser.h"
#include "errors.h"
#include "config.h"
#include "debug.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Forward declarations for recursive descent functions
static ASTNode* parse_declaration(Parser* parser);
static ASTNode* parse_identifier(Parser* parser);
static ASTNode* parse_variable(Parser* parser);
static ASTNode* parse_function_declaration(Parser* parser);
static ASTNode* parse_typed_function_declaration(Parser* parser, ASTNode* type, int type_pointer_level);
static ASTNode* parse_procedure_declaration(Parser* parser);
static ASTNode* parse_variable_declaration(Parser* parser);
static ASTNode* parse_statement(Parser* parser);
//static ASTNode* parse_expression(Parser* parser);
static ASTNode* parse_assignment(Parser* parser);
static ASTNode* parse_block(Parser* parser);
static ASTNode* parse_procedure_call(Parser* parser);
static ASTNode* parse_if_statement(Parser* parser);
static ASTNode* parse_while_statement(Parser* parser);
static ASTNode* parse_for_statement(Parser* parser);
static ASTNode* parse_return_statement(Parser* parser);
static ASTNode* parse_array_declaration(Parser* parser);
//static ASTNode* parse_parameter_list(Parser* parser);
static ASTNode* parse_type_specifier(Parser* parser, int* pointer_level);
static ASTNode* parse_parameter(Parser* parser);
//static ASTNode* parse_array_bounds(Parser* parser);
static ASTNode* parse_function_call(Parser* parser, const char* name);
static ASTNode* parse_logical_or(Parser* parser);
static ASTNode* parse_logical_and(Parser* parser);
static ASTNode* parse_equality(Parser* parser);
static ASTNode* parse_bitwise_or(Parser* parser);
static ASTNode* parse_bitwise_xor(Parser* parser);
static ASTNode* parse_bitwise_and(Parser* parser);
static ASTNode* parse_shift(Parser* parser);
static ASTNode* parse_comparison(Parser* parser);
static ASTNode* parse_term(Parser* parser);
static ASTNode* parse_factor(Parser* parser);
static ASTNode* parse_unary(Parser* parser);
static ASTNode* parse_primary(Parser* parser);
static ASTNode* parse_array_access(Parser* parser, ASTNode* array);
static bool parse_dimension_bounds(Parser* parser, DimensionBounds* bounds);
static int parse_array_dimension(Parser* parser);
static ASTNode* parse_record_type(Parser* parser, bool is_typedef);
static ASTNode* parse_record_field(Parser* parser);
static ASTNode* parse_type_declaration(Parser* parser);
static ASTNode* parse_field_access(Parser* parser, ASTNode* record);

static Token* consume_token_with_trace(Parser* parser, const char* context) {
    verbose_print("\n=== CONSUMING TOKEN AT %s ===\n", context);
    verbose_print("Before consumption:\n");
    verbose_print("  Current: type=%d, value='%s', line=%d, col=%d\n",
                 parser->ctx.current->type,
                 parser->ctx.current->value,
                 parser->ctx.current->loc.line,
                 parser->ctx.current->loc.column);
    if (parser->ctx.peek) {
        verbose_print("  Peek: type=%d, value='%s', line=%d, col=%d\n",
                     parser->ctx.peek->type,
                     parser->ctx.peek->value,
                     parser->ctx.peek->loc.line,
                     parser->ctx.peek->loc.column);
    }

    Token* result = parser->ctx.current;
    parser->ctx.current = parser->ctx.peek;
    parser->ctx.peek = lexer_next_token(parser->ctx.lexer);

    verbose_print("After consumption:\n");
    verbose_print("  Current: type=%d, value='%s', line=%d, col=%d\n",
                 parser->ctx.current->type,
                 parser->ctx.current->value,
                 parser->ctx.current->loc.line,
                 parser->ctx.current->loc.column);
    if (parser->ctx.peek) {
        verbose_print("  Peek: type=%d, value='%s', line=%d, col=%d\n",
                     parser->ctx.peek->type,
                     parser->ctx.peek->value,
                     parser->ctx.peek->loc.line,
                     parser->ctx.peek->loc.column);
    }
    verbose_print("=== END CONSUMING TOKEN ===\n\n");

    return result;
}

static void debug_print_parser_state_verb(Parser* parser, const char* context) {
    verbose_print("\n=== PARSER STATE AT %s ===\n", context);
    if (!parser || !parser->ctx.current) {
        verbose_print("Invalid parser state\n");
        return;
    }
    
    verbose_print("Current token: type=%d, value='%s', line=%d, col=%d\n",
                 parser->ctx.current->type,
                 parser->ctx.current->value,
                 parser->ctx.current->loc.line,
                 parser->ctx.current->loc.column);
    
    if (parser->ctx.peek) {
        verbose_print("Peek token: type=%d, value='%s', line=%d, col=%d\n",
                     parser->ctx.peek->type,
                     parser->ctx.peek->value,
                     parser->ctx.peek->loc.line,
                     parser->ctx.peek->loc.column);
    } else {
        verbose_print("Peek token: NULL\n");
    }
    verbose_print("=== END PARSER STATE ===\n\n");
}

static void advance_with_trace(Parser* parser, const char* context) {
    verbose_print("\n=== ADVANCING TOKEN AT %s ===\n", context);
    debug_print_parser_state_verb(parser, "BEFORE ADVANCE");
    
    Token* old_current = parser->ctx.current;
    parser->ctx.current = parser->ctx.peek;
    parser->ctx.peek = lexer_next_token(parser->ctx.lexer);
    
    debug_print_parser_state_verb(parser, "AFTER ADVANCE");
    verbose_print("=== END ADVANCING ===\n\n");
}

static void debug_print_token_info(Token* token, const char* desc) {
    if (!token) {
        verbose_print("%s: NULL token\n", desc);
        return;
    }
    verbose_print("%s: type=%d, value='%s', line=%d, col=%d\n",
           desc,
           token->type,
           token->value ? token->value : "NULL",
           token->loc.line,
           token->loc.column);
}

// Utility functions
static bool is_type_keyword(TokenType type) {
    return type == TOK_INTEGER || type == TOK_REAL ||
           type == TOK_LOGICAL || type == TOK_CHARACTER ||
           type == TOK_ARRAY;
}

static bool is_declaration_start(TokenType type) {
    return type == TOK_FUNCTION || type == TOK_PROCEDURE ||
           type == TOK_VAR;
}

static bool is_statement_start(TokenType type) {
    return type == TOK_IF || type == TOK_WHILE ||
           type == TOK_FOR || type == TOK_RETURN ||
           type == TOK_IDENTIFIER;
}

// Parser creation and destruction
Parser* parser_create(Lexer* lexer) {
    verbose_print("Allocating parser structure...\n");
    Parser* parser = (Parser*)malloc(sizeof(Parser));
    if (!parser) return NULL;

    verbose_print("Initializing parser context...\n");
    parser->ctx.lexer = lexer;
    parser->ctx.prev = NULL;
    parser->ctx.current = NULL;
    parser->ctx.peek = NULL;
    verbose_print("Creating symbol table...\n");
    parser->ctx.symbols = symtable_create();
    parser->ctx.current_function = NULL;
    parser->ctx.in_loop = false;
    parser->ctx.error_count = 0;
    parser->had_error = false;
    parser->panic_mode = false;
 
    verbose_print("Getting initial tokens...\n");    
    // Prime the parser with the first two tokens
    parser->ctx.current = lexer_next_token(lexer);
    if (parser->ctx.current) {
        verbose_print("First token: type=%d\n", parser->ctx.current->type);
    }
    parser->ctx.peek = lexer_next_token(lexer);
    if (parser->ctx.peek) {
        verbose_print("Second token: type=%d\n", parser->ctx.peek->type);
    }

    verbose_print("Parser creation completed\n");
    return parser;
}

void parser_destroy(Parser* parser) {
    if (parser) {
        token_destroy(parser->ctx.prev);
        token_destroy(parser->ctx.current);
        token_destroy(parser->ctx.peek);
        symtable_destroy(parser->ctx.symbols);
        free(parser->ctx.current_function);
        free(parser);
    }
}

// Token handling functions
static void advance(Parser* parser) {
    token_destroy(parser->ctx.prev);
    parser->ctx.prev = parser->ctx.current;
    parser->ctx.current = parser->ctx.peek;
    parser->ctx.peek = lexer_next_token(parser->ctx.lexer);
}

static bool check(Parser* parser, TokenType type) {
    return parser->ctx.current->type == type;
}

static bool match(Parser* parser, TokenType type) {
    debug_print_token_info(parser->ctx.current, "Match checking token");
    verbose_print("Matching against type: %d\n", type);
    if (check(parser, type)) {
        advance(parser);
        return true;
    }
    return false;
}

static Token* consume(Parser* parser, TokenType type, const char* message) {
    verbose_print("Consuming token: expected type=%d, got type=%d, value='%s'\n", 
           type, parser->ctx.current->type, parser->ctx.current->value);
           
    if (check(parser, type)) {
        Token* token = (Token*)malloc(sizeof(Token));
        if (!token) {
            verbose_print("Failed to allocate memory for token\n");
            return NULL;
        }
        
        token->type = parser->ctx.current->type;
        token->value = strdup(parser->ctx.current->value);  // Make sure to copy the entire value
        token->loc = parser->ctx.current->loc;
        
        if (!token->value) {
            verbose_print("Failed to duplicate token value\n");
            free(token);
            return NULL;
        }
        debug_parser_token_consume(parser, token, message);
        verbose_print("Successfully consumed token: %s\n", token->value);
        advance(parser);
        return token;
    }

    parser_error(parser, message);
    return NULL;
}

static void synchronize(Parser* parser) {
    debug_parser_error_sync(parser, "Starting synchronization");
    parser->panic_mode = true;

    while (!check(parser, TOK_EOF)) {
        if (parser->ctx.current->type == TOK_SEMICOLON) {
            advance(parser);
            parser->panic_mode = false;
            debug_parser_error_sync(parser, "Synchronized at semicolon");
            return;
        }

        switch (parser->ctx.current->type) {
            case TOK_FUNCTION:
            case TOK_PROCEDURE:
            case TOK_VAR:
            case TOK_BEGIN:
            case TOK_END:
                parser->panic_mode = false;
                debug_parser_error_sync(parser, "Synchronized at statement boundary");
                return;
            default:
                advance(parser);
        }
    }

    parser->panic_mode = false;
    debug_parser_error_sync(parser, "Synchronized at EOF");
}

void parser_error(Parser* parser, const char* message) {
    if (parser->panic_mode) return;
    
    parser->had_error = true;
    parser->ctx.error_count++;
    
    error_report(ERROR_SYNTAX, SEVERITY_ERROR, 
                parser->ctx.current->loc, "%s", message);
    
    debug_print_error_context(parser->ctx.current->loc);
    debug_print_parser_state_d(parser);
    synchronize(parser);
}

bool parser_sync_to_next_statement(Parser* parser) {
    parser->panic_mode = true;

    while (parser->ctx.current->type != TOK_EOF) {
        if (parser->ctx.current->type == TOK_SEMICOLON) {
            advance(parser);
            parser->panic_mode = false;
            return true;
        }

        switch (parser->ctx.current->type) {
            case TOK_FUNCTION:
            case TOK_PROCEDURE:
            case TOK_VAR:
            case TOK_IF:
            case TOK_WHILE:
            case TOK_FOR:
            case TOK_RETURN:
                parser->panic_mode = false;
                return true;
            default:
                advance(parser);
        }
    }

    parser->panic_mode = false;
    return false;
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

static bool should_auto_dereference(Parser* parser, ASTNode* node) {
    if (!node) return false;

    // Get symbol to check if it's an out parameter
    Symbol* sym = NULL;
    const char* name = NULL;

    if (node->type == NODE_IDENTIFIER) {
        name = node->data.value;
    } else if (node->type == NODE_VARIABLE) {
        name = node->data.variable.name;
    }

    if (name) {
        sym = symtable_lookup(parser->ctx.symbols, name);
        if (sym && sym->info.var.is_parameter && sym->info.var.needs_deref && 
            (strcasecmp(sym->info.var.param_mode, "out") == 0 || 
             strcasecmp(sym->info.var.param_mode, "inout") == 0 || 
             strcasecmp(sym->info.var.param_mode, "in/out") == 0)) {
            return true;
        }
    }

    return false;
}

// Main parsing functions
ASTNode* parser_parse(Parser* parser) {
    verbose_print("Creating program node...\n");
    ASTNode* root = ast_create_node(NODE_PROGRAM);
    if (!root) {
        verbose_print("Failed to create program node\n");
        return NULL;
    }

    verbose_print("Starting to parse declarations...\n");
    while (parser->ctx.current->type != TOK_EOF) {
        verbose_print("Parsing declaration, current token type: %d\n", parser->ctx.current->type);
        ASTNode* decl = parse_declaration(parser);
        if (decl) {
            verbose_print("Successfully parsed declaration\n");
            ast_add_child(root, decl);
        } else if (!parser->panic_mode) {
            verbose_print("Failed to parse declaration, attempting to synchronize\n");
            parser_sync_to_next_statement(parser);
        }
    }

    return root;
}

static ASTNode* parse_typed_function_declaration(Parser* parser, ASTNode* type, int type_pointer_level) {
    debug_parser_rule_start(parser, "parse_typed_function_declaration");
    verbose_print("Parsing function declaration with preceding type\n");
    // Create function node with type already known
    Token* name = consume(parser, TOK_IDENTIFIER, "Expected function name");
    if (!name) {
        debug_parser_rule_end(parser, "parse_typed_function_declaration", NULL);
        return NULL;
    }
    
    debug_parser_function_start(parser, name->value);

    ASTNode* func = ast_create_function(name->value, type->data.value, false);
    if (!func) 
    {
        debug_parser_rule_end(parser, "parse_typed_function_declaration", NULL);
        return NULL;
    }
    ast_set_location(func, name->loc);
    Symbol* func_sym = symtable_add_function(parser->ctx.symbols, name->value, type->data.value, false);
    verbose_print("\nCreated function symbol: %s\n", name->value);

    func_sym->info.func.is_pointer = type_pointer_level > 0;
    func_sym->info.func.pointer_level = type_pointer_level;
    
    // Enter new scope for function
    debug_parser_scope_enter(parser, "Function");
    symtable_enter_scope(parser->ctx.symbols, SCOPE_FUNCTION);
    parser->ctx.symbols->current->function_name = strdup(name->value);
    parser->ctx.current_function = strdup(name->value);
    parser->ctx.is_function = true;


    func->data.function.is_pointer = type_pointer_level > 0;
    func->data.function.pointer_level = type_pointer_level;

    // Parameter list
    debug_parser_state(parser, "Before parameter list");
    if (!match(parser, TOK_LPAREN)) {
        parser_error(parser, "Expected '(' after function name");
        debug_parser_rule_end(parser, "parse_typed_function_declaration", func);
        return func;
    }

    if (!check(parser, TOK_RPAREN)) {
        debug_parser_parameter_start(parser);
        func->data.function.params = parse_parameter_list(parser);
    }

    if (!match(parser, TOK_RPAREN)) {
        parser_error(parser, "Expected ')' after parameters");
        debug_parser_rule_end(parser, "parse_typed_function_declaration", func);
        return func;
    }

    // Create a block node for the function body
    debug_parser_state(parser, "Before function body");
    ASTNode* body = ast_create_node(NODE_BLOCK);
    if (!body) {
        ast_destroy_node(func);
        return NULL;
    }
    ast_set_location(body, parser->ctx.current->loc);
    // Parse variable declarations until we see 'begin'
    while (check(parser, TOK_VAR)) {
        ASTNode* var_decl = parse_variable_declaration(parser);
        if (var_decl) {
            ast_add_child(body, var_decl);
        }
    }

    // Expect 'begin'
    if (!match(parser, TOK_BEGIN)) {
        parser_error(parser, "Expected 'begin' in function body");
        ast_destroy_node(func);
        return NULL;
    }

    // Parse statements until 'end'
    while (!check(parser, TOK_END) && !check(parser, TOK_EOF)) {
        ASTNode* statement = parse_statement(parser);
        if (statement) {
            ast_add_child(body, statement);
        } else if (!parser->panic_mode) {
            parser_sync_to_next_statement(parser);
        }
    }

    // Expect 'end'
    if (!match(parser, TOK_END)) {
        parser_error(parser, "Expected 'end' in function body");
        ast_destroy_node(func);
        return NULL;
    }

    // Attach the body to the function
    func->data.function.body = body;

    // Expect 'endfunction'
    if (!match(parser, TOK_ENDFUNCTION)) {
        parser_error(parser, "Expected 'endfunction'");
        ast_destroy_node(func);
        return NULL;
    }

    // Exit function scope
    debug_parser_scope_exit(parser, "Function");
    symtable_exit_scope(parser->ctx.symbols);
    free(parser->ctx.current_function);
    parser->ctx.current_function = NULL;
    parser->ctx.is_function = false;

    debug_parser_rule_end(parser, "parse_typed_function_declaration", func);
    return func;
}

static ASTNode* parse_declaration(Parser* parser) {
    verbose_print("In parse_declaration, token type: %d\n", parser->ctx.current->type);
    
    // Check for type-before-name function syntax
    if (is_type_keyword(parser->ctx.current->type)) {
        int type_pointer_level = 0;
        ASTNode* type = parse_type_specifier(parser, &type_pointer_level);
        verbose_print("Found type keyword, checking for function declaration\n");
        
        if (match(parser, TOK_FUNCTION)) {
            return parse_typed_function_declaration(parser, type, type_pointer_level);
        } else {
            // TODO: handle global variables here
        }
    }

    // Original function/procedure/var parsing
    if (match(parser, TOK_FUNCTION)) {
        verbose_print("Parsing function declaration\n");
        return parse_function_declaration(parser);
    }
    if (match(parser, TOK_PROCEDURE)) {
        verbose_print("Parsing procedure declaration\n");
        return parse_procedure_declaration(parser);
    }
    if (match(parser, TOK_VAR)) {
        match(parser, TOK_COLON);
        verbose_print("Parsing variable declaration\n");
        return parse_variable_declaration(parser);
    }
    if (match(parser, TOK_TYPE)) {
        match(parser, TOK_COLON);
        verbose_print("Parsing type declaration\n");
        printf("Parsing type declaration\n");
        return parse_type_declaration(parser);
    }


    parser_error(parser, "Expected declaration");
    return NULL;
}

static ASTNode* parse_identifier(Parser* parser) {
    verbose_print("Entering parse_identifier\n");
    debug_print_token_info(parser->ctx.current, "Current token in parse_identifier");

    Token* name = consume(parser, TOK_IDENTIFIER, "Expected identifier");
    if (!name) {
        verbose_print("Failed to consume identifier token\n");
        return NULL;
    }
    verbose_print("Consumed identifier: %s\n", name->value);

    // First look up the symbol to determine its type
    Symbol* symbol = symtable_lookup_parameter(parser->ctx.symbols, parser->ctx.current_function, name->value);
    if (!symbol)
        symbol = symtable_lookup(parser->ctx.symbols, name->value);
    
    // Create base variable node
    ASTNode* base = ast_create_variable(name->value, NULL, NODE_VARIABLE);
    if (!base) {
        verbose_print("Failed to create variable node\n");
        return NULL;
    }
    ast_set_location(base, name->loc);

    // Handle array access - either with [] or with () if enabled
    if (check(parser, TOK_LBRACKET) || 
        (check(parser, TOK_LPAREN) && g_config.allow_mixed_array_access && 
         symbol && (symbol->kind == SYMBOL_VARIABLE || symbol->kind == SYMBOL_PARAMETER) && symbol->info.var.is_array)) {
        verbose_print("Found array access in parse_identifier\n");
        return parse_array_access(parser, base);
    }

    // Handle function calls - when we see () and either:
    // 1. Mixed array access is disabled, or
    // 2. Mixed array access is enabled but the symbol is not an array
    if (check(parser, TOK_LPAREN) && 
        (!g_config.allow_mixed_array_access || 
         !symbol || symbol->kind != SYMBOL_VARIABLE || !symbol->info.var.is_array)) {
        verbose_print("Found function call\n");
        ast_destroy_node(base);  // Clean up the variable node
        return parse_function_call(parser, name->value);
    }

    verbose_print("Created regular variable node\n");
    return base;
}

static ASTNode* parse_print_statement(Parser* parser) {
    verbose_print("Parsing print statement\n");
    //SourceLocation loc = parser->ctx.current->loc;
    Token* print_token = consume(parser, TOK_PRINT, "Expected 'print'");

    ASTNode* print_node = ast_create_node(NODE_PRINT);
    if (!print_node) return NULL;
    ast_set_location(print_node, print_token->loc);

    // Handle optional parentheses
    bool has_parens = match(parser, TOK_LPAREN);

    // Parse the argument - use parse_expression which will handle array access
    verbose_print("Parsing print argument\n");
    ASTNode* arg = parse_expression(parser);
    if (!arg) {
        verbose_print("Failed to parse print argument\n");
        ast_destroy_node(print_node);
        return NULL;
    }

    // Add argument as child of print node
    ast_add_child(print_node, arg);

    if (has_parens) {
        if (!match(parser, TOK_RPAREN)) {
            parser_error(parser, "Expected ')' after print argument");
            ast_destroy_node(print_node);
            return NULL;
        }
    }

    verbose_print("Successfully parsed print statement\n");
    return print_node;
}

static ASTNode* parse_read_statement(Parser* parser) {
    verbose_print("Parsing read statement\n");
    Token* read_token = consume(parser, TOK_READ, "Expected 'read'");
    if (!read_token) return NULL;

    ASTNode* read_node = ast_create_node(NODE_READ);
    if (!read_node) return NULL;
    ast_set_location(read_node, read_token->loc);

    // Handle optional parentheses
    bool has_parens = match(parser, TOK_LPAREN);

    // Parse the variable to read into
    ASTNode* var = parse_variable(parser);
    if (!var) {
        ast_destroy_node(read_node);
        return NULL;
    }
    ast_add_child(read_node, var);

    if (has_parens) {
        consume(parser, TOK_RPAREN, "Expected ')' after read argument");
    }

    return read_node;
}


static ASTNode* parse_variable(Parser* parser) {
    verbose_print("Entering parse_variable\n");
    debug_print_token_info(parser->ctx.current, "Current token in parse_variable");

    if (!parser || !parser->ctx.current) {
        verbose_print("Error: Null parser or current token in parse_variable\n");
        return NULL;
    }
    
    ASTNode* var = parse_identifier(parser);
    verbose_print("After parse_identifier in parse_variable: var is %s\n", var ? "not null" : "null");

    if (!var) {
        verbose_print("parse_identifier returned null in parse_variable\n");
        return NULL;
    }

    // Check for array access if we got a variable
    if (ast_is_node_type(var, NODE_VARIABLE) || ast_is_node_type(var, NODE_IDENTIFIER)) {
        verbose_print("Checking for array access\n");
        debug_print_token_info(parser->ctx.current, "Current token before array access check");
        Symbol* sym = symtable_lookup_parameter(parser->ctx.symbols, parser->ctx.current_function, var->type == NODE_VARIABLE ? var->data.variable.name : var->data.value);
        if (!sym)
            sym = symtable_lookup(parser->ctx.symbols, var->type == NODE_VARIABLE ? var->data.variable.name : var->data.value);
        if (check(parser, TOK_LBRACKET) || (check(parser, TOK_LPAREN) && g_config.allow_mixed_array_access && sym && sym->info.var.is_array)) {
            verbose_print("Found array access operator\n");
            return parse_array_access(parser, var);
        }
    }

    verbose_print("Exiting parse_variable normally\n");
    return var;
}

// Function parsing
static ASTNode* parse_function_declaration(Parser* parser) {
    //debug_trace_enter("parse_function_declaration");
    debug_parser_rule_start(parser, "parse_function_declaration");

    verbose_print("Starting function declaration parse\n");

    Token* name = consume(parser, TOK_IDENTIFIER, "Expected function name");
    if (!name) {
        debug_parser_rule_end(parser, "parse_function_declaration", NULL);
        verbose_print("Failed to get function name\n");
        return NULL;
    }
    verbose_print("Function name: %s\n", name->value);
    debug_parser_function_start(parser, name->value);

    ASTNode* func = ast_create_function(name->value, NULL, false);
    if (!func) {
        debug_parser_rule_end(parser, "parse_function_declaration", NULL);
        verbose_print("Failed to create function node\n");
        return NULL;
    }
    ast_set_location(func, name->loc);
    Symbol* func_sym = symtable_add_function(parser->ctx.symbols, name->value, NULL, true);

    verbose_print("\nCreated function symbol: %s\n", name->value);
    symtable_debug_dump_all(parser->ctx.symbols);
    
    // Enter new scope for function
    debug_parser_scope_enter(parser, "Function");
    symtable_enter_scope(parser->ctx.symbols, SCOPE_FUNCTION);
    parser->ctx.symbols->current->function_name = strdup(name->value);
    parser->ctx.current_function = strdup(name->value);
    parser->ctx.is_function = true;

    verbose_print("Parsing parameter list\n");
    // Parameter list
    debug_parser_state(parser, "Before parameter list");
    if (!match(parser, TOK_LPAREN)) {
        parser_error(parser, "Expected '(' after function name");
        debug_parser_rule_end(parser, "parse_function_declaration", func);
        return func;
    }

    if (!check(parser, TOK_RPAREN)) {
        debug_parser_parameter_start(parser);
        func->data.function.params = parse_parameter_list(parser);
    }

    if (!match(parser, TOK_RPAREN)) {
        parser_error(parser, "Expected ')' after parameters");
        debug_parser_rule_end(parser, "parse_function_declaration", func);
        return func;
    }

    verbose_print("\nAfter parsing parameters:\n");
    symtable_debug_dump_all(parser->ctx.symbols);

    verbose_print("Parsing return type\n");
    // Return type

    debug_parser_state(parser, "Before return type");
    if (match(parser, TOK_COLON)) {
        int pointer_level = 0;
        ASTNode* return_type = parse_type_specifier(parser, &pointer_level);
        if (return_type) {
            func->data.function.return_type = strdup(return_type->data.value);
            func->data.function.is_pointer = pointer_level > 0;
            func->data.function.pointer_level = pointer_level;
            ast_destroy_node(return_type);

            func_sym->info.func.is_pointer = pointer_level > 0;
            func_sym->info.func.pointer_level = pointer_level;
        }
    }

    // Create a block node for the procedure body
    debug_parser_state(parser, "Before function body");
    ASTNode* body = ast_create_node(NODE_BLOCK);
    if (!body) {
        ast_destroy_node(func);
        return NULL;
    }
    ast_set_location(body, parser->ctx.current->loc);
    
    // Parse variable declarations until we see 'begin'
    while (check(parser, TOK_VAR)) {
        ASTNode* var_decl = parse_variable_declaration(parser);
        if (var_decl) {
            ast_add_child(body, var_decl);
        }
    }

    // Expect 'begin'
    if (!match(parser, TOK_BEGIN)) {
        parser_error(parser, "Expected 'begin' in procedure body");
        ast_destroy_node(func);
        return NULL;
    }

    // Parse statements until 'end'
    while (!check(parser, TOK_END) && !check(parser, TOK_EOF)) {
        ASTNode* statement = parse_statement(parser);
        if (statement) {
            ast_add_child(body, statement);
        } else if (!parser->panic_mode) {
            parser_sync_to_next_statement(parser);
        }
    }

    // Expect 'end'
    if (!match(parser, TOK_END)) {
        parser_error(parser, "Expected 'end' in procedure body");
        ast_destroy_node(func);
        return NULL;
    }

    // Attach the body to the procedure
    func->data.function.body = body;

    if (match(parser, TOK_END)) {
        // Check for function name
        Token* end_name = consume(parser, TOK_IDENTIFIER, "Expected function name after end");
        if (!end_name || strcmp(end_name->value, name->value) != 0) {
            parser_error(parser, "Function end name must match function name");
            ast_destroy_node(func);
            return NULL;
        }
    } else if (!match(parser, TOK_ENDFUNCTION)) {
        char* error_message = malloc(strlen(name->value) + strlen("Expected 'endfunction' or 'end %s\n'"));
        sprintf(error_message, "Expected 'endfunction' or 'end %s'\n", name->value);
        parser_error(parser, error_message);
        ast_destroy_node(func);
        free(error_message);
        return NULL;
    }

    // Exit procedure scope
    debug_parser_scope_exit(parser, "Function");
    symtable_exit_scope(parser->ctx.symbols);
    free(parser->ctx.current_function);
    parser->ctx.current_function = NULL;
    parser->ctx.is_function = false;

    debug_parser_rule_end(parser, "parse_function_declaration", func);
    return func;
}

static ASTNode* parse_procedure_declaration(Parser* parser) {
    debug_parser_rule_start(parser, "parse_procedure_declaration");
    verbose_print("Parsing procedure declaration\n");
    Token* name = consume(parser, TOK_IDENTIFIER, "Expected procedure name");
    if (!name) {
        debug_parser_rule_end(parser, "parse_procedure_declaration", NULL);
        return NULL;
    }

    debug_parser_procedure_start(parser, name->value);

    ASTNode* proc = ast_create_function(name->value, NULL, true);
    if (!proc) {
        debug_parser_rule_end(parser, "parse_procedure_declaration", NULL);
        return NULL;
    }
    ast_set_location(proc, name->loc);
    Symbol* func_sym = symtable_add_function(parser->ctx.symbols, name->value, NULL, true);
    verbose_print("\nCreated function symbol: %s\n", name->value);
    symtable_debug_dump_all(parser->ctx.symbols);
    // Enter new scope for procedure
    debug_parser_scope_enter(parser, "Procedure");
    symtable_enter_scope(parser->ctx.symbols, SCOPE_FUNCTION);
    parser->ctx.symbols->current->function_name = strdup(name->value);
    parser->ctx.current_function = strdup(name->value);
    parser->ctx.is_function = false;


    // Parameter list
    debug_parser_state(parser, "Before parameter list");
    if (!match(parser, TOK_LPAREN)) {
        parser_error(parser, "Expected '(' after procedure name");
        debug_parser_rule_end(parser, "parse_procedure_declaration", proc);
        return proc;
    }

    if (!check(parser, TOK_RPAREN)) {
        debug_parser_parameter_start(parser);
        proc->data.function.params = parse_parameter_list(parser);
    }

    if (!match(parser, TOK_RPAREN)) {
        parser_error(parser, "Expected ')' after parameters");
        debug_parser_rule_end(parser, "parse_procedure_declaration", proc);
        return proc;
    }

    verbose_print("\nAfter parsing parameters:\n");
    symtable_debug_dump_all(parser->ctx.symbols);

    // Create a block node for the procedure body
    debug_parser_state(parser, "Before procedure body");
    ASTNode* body = ast_create_node(NODE_BLOCK);
    if (!body) {
        ast_destroy_node(proc);
        return NULL;
    }
    ast_set_location(body, parser->ctx.current->loc);

    // Parse variable declarations until we see 'begin'
    while (check(parser, TOK_VAR)) {
        ASTNode* var_decl = parse_variable_declaration(parser);
        if (var_decl) {
            ast_add_child(body, var_decl);
        }
    }

    // Expect 'begin'
    if (!match(parser, TOK_BEGIN)) {
        parser_error(parser, "Expected 'begin' in procedure body");
        ast_destroy_node(proc);
        return NULL;
    }

    // Parse statements until 'end'
    while (!check(parser, TOK_END) && !check(parser, TOK_EOF)) {
        ASTNode* statement = parse_statement(parser);
        if (statement) {
            ast_add_child(body, statement);
        } else if (!parser->panic_mode) {
            parser_sync_to_next_statement(parser);
        }
    }

    // Expect 'end'
    if (!match(parser, TOK_END)) {
        parser_error(parser, "Expected 'end' in procedure body");
        ast_destroy_node(proc);
        return NULL;
    }

    // Attach the body to the procedure
    proc->data.function.body = body;

    // Expect 'endprocedure'
    if (match(parser, TOK_END)) {
        // Check for procedure name
        Token* end_name = consume(parser, TOK_IDENTIFIER, "Expected procedure name after end");
        if (!end_name || strcmp(end_name->value, name->value) != 0) {
            parser_error(parser, "Procedure end name must match procedure name");
            ast_destroy_node(proc);
            return NULL;
        }
    } else if (!match(parser, TOK_ENDPROCEDURE)) {
        char* error_message = malloc(strlen(name->value) + strlen("Expected 'endfunction' or 'end %s\n'"));
        sprintf(error_message, "Expected 'endprocedure' or 'end %s'\n", name->value);
        parser_error(parser, error_message);
        ast_destroy_node(proc);
        free(error_message);
        return NULL;
    }


    // Exit procedure scope
    debug_parser_scope_exit(parser, "Procedure");
    symtable_exit_scope(parser->ctx.symbols);
    free(parser->ctx.current_function);
    parser->ctx.current_function = NULL;
    parser->ctx.is_function = false;

    debug_parser_rule_end(parser, "parse_procedure_declaration", proc);
    return proc;
}

static ASTNode* parse_bound_expression(Parser* parser) {
    verbose_print("Parsing bound expression\n");
    ASTNode* expr = parse_expression(parser);
    if (!expr) return NULL;
    
    // For now, we only allow:
    // - Simple identifiers (n)
    // - Constants (5)
    // - Binary operations with constant/identifier (+,-,*,/)
    if (expr->type != NODE_NUMBER && 
        expr->type != NODE_IDENTIFIER &&
        (expr->type != NODE_BINARY_OP ||
         (expr->data.binary_op.op != TOK_PLUS &&
          expr->data.binary_op.op != TOK_MINUS &&
          expr->data.binary_op.op != TOK_MULTIPLY &&
          expr->data.binary_op.op != TOK_DIVIDE &&
          expr->data.binary_op.op != TOK_RSHIFT &&
          expr->data.binary_op.op != TOK_LSHIFT &&
          expr->data.binary_op.op != TOK_BITAND &&
          expr->data.binary_op.op != TOK_BITOR &&
          expr->data.binary_op.op != TOK_BITXOR)) &&
        (expr->type != NODE_UNARY_OP ||
         expr->data.unary_op.op != TOK_BITNOT)) {
        parser_error(parser, "Invalid array bound expression");
        ast_destroy_node(expr);
        return NULL;
    }
    
    return expr;
}

static ArrayBoundsData* parse_array_bounds_data(Parser* parser) {
    verbose_print("Parsing array bounds data\n");
    
    // Create bounds structure for a single dimension
    ArrayBoundsData* bounds = symtable_create_bounds(1);
    if (!bounds) return NULL;
    
    // Parse the dimension bounds
    if (!parse_dimension_bounds(parser, &bounds->bounds[0])) {
        symtable_destroy_bounds(bounds);
        return NULL;
    }

    return bounds;
}

// Parse bounds for multiple dimensions
static ArrayBoundsData* parse_array_bounds_for_all_dimensions(Parser* parser, int dimensions) {
    verbose_print("Parsing bounds for %d dimensions\n", dimensions);
    
    if (dimensions <= 0) {
        parser_error(parser, "Invalid number of dimensions");
        return NULL;
    }

    ArrayBoundsData* bounds = symtable_create_bounds(dimensions);
    if (!bounds) return NULL;

    if (check(parser, TOK_RBRACKET)) {
        bounds->bounds[0].using_range = false;
        bounds->bounds[0].start.variable_name = strdup("");
    } else if (!parse_dimension_bounds(parser, &bounds->bounds[0])) {
        symtable_destroy_bounds(bounds);
        return NULL;
    }

    // Parse remaining dimensions if any
    for (int dim = 1; dim < dimensions; dim++) {
        if (!match(parser, TOK_RBRACKET)) {
            parser_error(parser, "Expected ']' after array bounds");
            symtable_destroy_bounds(bounds);
            return NULL;
        }
        
        if (!match(parser, TOK_LBRACKET)) {
            parser_error(parser, "Expected '[' for next dimension");
            symtable_destroy_bounds(bounds);
            return NULL;
        }
        
        if (check(parser, TOK_RBRACKET)) {
            parser_error(parser, "Expected bounds for any dimension after the first");
            return NULL;
        } else if (!parse_dimension_bounds(parser, &bounds->bounds[dim])) {
            symtable_destroy_bounds(bounds);
            return NULL;
        }
    }
    
    return bounds;
}

static ArrayBoundsData* parse_paren_array_bounds_for_all_dimensions(Parser* parser, int dimensions) {
    verbose_print("Parsing bounds for %d dimensions\n", dimensions);
    
    if (dimensions <= 0) {
        parser_error(parser, "Invalid number of dimensions");
        return NULL;
    }

    ArrayBoundsData* bounds = symtable_create_bounds(dimensions);
    if (!bounds) return NULL;
    
    // Parse first dimension
    if (check(parser, TOK_RPAREN)) {
        bounds->bounds[0].using_range = false;
        bounds->bounds[0].start.variable_name = strdup("");
    } else if (!parse_dimension_bounds(parser, &bounds->bounds[0])) {
        symtable_destroy_bounds(bounds);
        return NULL;
    }
    
    // Parse remaining dimensions if any
    for (int dim = 1; dim < dimensions; dim++) {
        if (!match(parser, TOK_RPAREN)) {
            parser_error(parser, "Expected ')' after array bounds");
            symtable_destroy_bounds(bounds);
            return NULL;
        }
        
        if (!match(parser, TOK_LPAREN)) {
            parser_error(parser, "Expected '(' for next dimension");
            symtable_destroy_bounds(bounds);
            return NULL;
        }
        
        if (check(parser, TOK_RPAREN)) {
            parser_error(parser, "Expected bounds for any dimension after the first");
            return NULL;
        } else if (!parse_dimension_bounds(parser, &bounds->bounds[dim])) {
            symtable_destroy_bounds(bounds);
            return NULL;
        }
    }
    
    return bounds;
}

// Helper function to parse bounds for a single dimension
static bool parse_dimension_bounds(Parser* parser, DimensionBounds* bounds) {
    verbose_print("Parsing single dimension bounds\n");
    
    // Parse first expression
    ASTNode* start_expr = parse_bound_expression(parser);
    if (!start_expr) {
        return false;
    }

    // Convert expression to bounds data
    if (start_expr->type == NODE_NUMBER) {
        bounds->start.is_constant = true;
        bounds->start.constant_value = atol(start_expr->data.value);
    } else {
        bounds->start.is_constant = false;
        bounds->start.variable_name = ast_to_string(start_expr);
        if (!bounds->start.variable_name) {
            ast_destroy_node(start_expr);
            return false;
        }
    }
    ast_destroy_node(start_expr);

    // Check for range (..)
    if (match(parser, TOK_DOTDOT)) {
        bounds->using_range = true;
        
        ASTNode* end_expr = parse_bound_expression(parser);
        if (!end_expr) return false;

        if (end_expr->type == NODE_NUMBER) {
            bounds->end.is_constant = true;
            bounds->end.constant_value = atol(end_expr->data.value);
        } else {
            bounds->end.is_constant = false;
            bounds->end.variable_name = ast_to_string(end_expr);
            if (!bounds->end.variable_name) {
                ast_destroy_node(end_expr);
                return false;
            }
        }
        ast_destroy_node(end_expr);
    } else {
        // Single bound - use it as size
        bounds->using_range = false;
        if (bounds->start.is_constant) {
            bounds->end.is_constant = true;
            bounds->end.constant_value = bounds->start.constant_value;
        } else {
            bounds->end.is_constant = false;
            bounds->end.variable_name = strdup(bounds->start.variable_name);
            if (!bounds->end.variable_name) return false;
        }
    }

    return true;
}

// Helper function to combine multiple single-dimension bounds
static ArrayBoundsData* combine_bounds(ArrayBoundsData** bounds_array, int count) {
    if (!bounds_array || count <= 0) return NULL;

    ArrayBoundsData* combined = symtable_create_bounds(count);
    if (!combined) return NULL;

    for (int i = 0; i < count; i++) {
        if (!bounds_array[i] || bounds_array[i]->dimensions != 1) {
            symtable_destroy_bounds(combined);
            return NULL;
        }
        // Copy bounds from each single-dimension array
        combined->bounds[i] = bounds_array[i]->bounds[0];
        // Clear the source bounds to prevent double-free
        // (since we took ownership of the strings)
        bounds_array[i]->bounds[0].start.variable_name = NULL;
        bounds_array[i]->bounds[0].end.variable_name = NULL;
    }

    return combined;
}

// Function to create AST node for array bounds (used when building the AST)
static ASTNode* parse_array_bounds_node(Parser* parser) {
    verbose_print("Parsing array bounds node\n");
    
    ASTNode* bounds_node = ast_create_node(NODE_ARRAY_BOUNDS);
    if (!bounds_node) return NULL;
    ast_set_location(bounds_node, parser->ctx.current->loc);

    // Parse bounds data
    ArrayBoundsData* bounds_data = parse_array_bounds_data(parser);
    if (!bounds_data) {
        ast_destroy_node(bounds_node);
        return NULL;
    }

    // Store bounds data in the node
    bounds_node->array_bounds = *bounds_data;
    free(bounds_data); // Free the temporary structure since we copied its contents

    return bounds_node;
}

static int count_comma_array_dimensions_ahead(Lexer* lexer, size_t var_start, bool use_paren) {
    const char* source = lexer->source;
    size_t pos = var_start;
    char open = use_paren ? '(' : '[';
    char close = use_paren ? ')' : ']';
    // Find first '['
    while (source[pos] && source[pos] != open) {
        pos++;
    }
    
    if (!source[pos]) {
        return 0;
    }
    
    size_t start_pos = pos;
    
    // Find end (colon)
    size_t end_pos = start_pos;
    while (source[end_pos] && source[end_pos] != close) {
        end_pos++;
    }
    
    int dimensions = 0;
    int nesting = 0;
    pos = start_pos;
    
    while (pos < end_pos && source[pos]) {
        char c = source[pos];
        
        if (c == ',') {
            if (dimensions)
                dimensions++;
            else 
                dimensions = 2;
        }
        pos++;
    }
    return dimensions;
}

static int count_array_dimensions_ahead(Lexer* lexer, size_t var_start, bool use_paren) {
    const char* source = lexer->source;
    size_t pos = var_start;
    
    char open = use_paren ? '(' : '[';
    char close = use_paren ? ')' : ']';
    // Find first '['
    while (source[pos] && source[pos] != open) {
        pos++;
    }
    
    if (!source[pos]) {
        return 0;
    }
    
    size_t start_pos = pos;
    
    // Find end (colon)
    size_t end_pos = start_pos;
    bool currently_closed = false;
    while (source[end_pos] && 
          (source[end_pos] != ':' && 
          (source[end_pos] != ',' || !currently_closed))) {
        if (source[end_pos] == close)
            currently_closed = true;
        else if (source[end_pos] == open)
            currently_closed = false;
        end_pos++;
    }
    
    int dimensions = 0;
    int nesting = 0;
    pos = start_pos;
    
    while (pos < end_pos && source[pos]) {
        char c = source[pos];
        
        if (c == open) {
            if (nesting == 0) {
                // Found start of a new dimension
                dimensions++;
            }
            nesting++;
        } else if (c == close) {
            nesting--;
        }
        pos++;
    }
    return dimensions;
}

static int count_array_type_dimensions_ahead(Lexer* lexer, size_t var_start, bool use_paren) {
    const char* source = lexer->source;
    size_t pos = var_start;
    
    
    char open = use_paren ? '(' : '[';
    char close = use_paren ? ')' : ']';
    // Find first '['
    while (source[pos] && source[pos] != open) {
        pos++;
    }
    
    if (!source[pos]) {
        return 0;
    }
    
    size_t start_pos = pos;
    
    // Find end (colon)
    size_t end_pos = start_pos;
    while (source[end_pos] && source[end_pos] != 'o' && source[end_pos + 1] != 'f' && source[end_pos + 2] != ' ') {
        end_pos++;
    }
    
    int dimensions = 0;
    int nesting = 0;
    pos = start_pos;
    
    while (pos < end_pos && source[pos]) {
        char c = source[pos];
        
        if (c == open) {
            if (nesting == 0) {
                // Found start of a new dimension
                dimensions++;
            }
            nesting++;
        } else if (c == close) {
            nesting--;
        }
        pos++;
    }
    return dimensions;
}

static ASTNode* parse_variable_declaration(Parser* parser) {
    verbose_print("Parsing variable declaration\n");
    ASTNode* param_decl = ast_create_node(NODE_BLOCK);
    int param_decl_count = 0;
    ASTNode* declarations = ast_create_node(NODE_BLOCK);
    if (!declarations) return NULL;
    ast_set_location(declarations, parser->ctx.current->loc);

    if (check(parser, TOK_VAR)) {
        advance(parser);
        match(parser, TOK_COLON);
    }
    do {
        int pointer_level = 0;
        
        // Count consecutive pointer operators
        while (check(parser, TOK_MULTIPLY) || check(parser, TOK_DEREF)) {
            verbose_print("Found pointer operator level %d before variable name\n", 
                         pointer_level + 1);
            pointer_level++;
            advance(parser);  // Consume the * token
        }

        size_t var_start = parser->ctx.lexer->start;
        Token* name = consume(parser, TOK_IDENTIFIER, "Expected variable name");
        if (!name) {
            ast_destroy_node(declarations);
            return NULL;
        }

        if (parser->ctx.current_function && parser->ctx.is_function && 
            strcmp(name->value, parser->ctx.current_function) == 0) {
            verbose_print("Skipping declaration of return variable %s\n", name->value);
            
            // We still need to parse and skip the rest of this variable declaration
            // to maintain correct parser state
            if (match(parser, TOK_LBRACKET) || 
                (match(parser, TOK_LPAREN) && g_config.allow_mixed_array_access)) {
                // Skip array bounds
                int bracket_count = 1;
                while (bracket_count > 0 && !check(parser, TOK_EOF)) {
                    if (match(parser, TOK_LBRACKET) || match(parser, TOK_LPAREN)) {
                        bracket_count++;
                    } else if (match(parser, TOK_RBRACKET) || match(parser, TOK_RPAREN)) {
                        bracket_count--;
                    } else {
                        advance(parser);
                    }
                }
            }
            
            // Skip to next variable or type declaration
            if (check(parser, TOK_COMMA)) {
                advance(parser);
                continue;
            }
            
            // Skip type declaration
            if (!match(parser, TOK_COLON)) {
                parser_error(parser, "Expected ':' after variable names");
                ast_destroy_node(declarations);
                return NULL;
            }
            
            // Skip type including array declarations if present
            while (!check(parser, TOK_COMMA) && !check(parser, TOK_SEMICOLON) && 
                   !check(parser, TOK_EOF)) {
                advance(parser);
            }
            
            continue;  // Skip to next variable declaration if there is one
        }
        ASTNode* var = NULL;
        ArrayBoundsData* var_bounds = NULL;
        int total_dimensions = 0;
        bool is_array_decl = false;

        // Parse bounds if present after name
        if (match(parser, TOK_LBRACKET)) {
            is_array_decl = true;
            // Here we need to check all commas within the brackets
            if (parser->ctx.peek->type == TOK_COMMA || count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, false)) {
                int dimension_count = 1;
                TokenType lookahead_type = parser->ctx.peek->type;
                dimension_count = count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, false);
                
                // Create bounds for all dimensions
                var_bounds = symtable_create_bounds(dimension_count);
                if (!var_bounds) {
                    ast_destroy_node(declarations);
                    return NULL;
                }
                // Parse each dimension's bounds
                for (int i = 0; i < dimension_count; i++) {
                    if (i > 0) {
                        if (!match(parser, TOK_COMMA)) {
                            parser_error(parser, "Expected ',' between dimensions");
                            symtable_destroy_bounds(var_bounds);
                            ast_destroy_node(declarations);
                            return NULL;
                        }
                    }
                    
                    if (i == 0 && (check(parser, TOK_RBRACKET) || check(parser, TOK_COMMA))) {
                        var_bounds->bounds[i].using_range = false;
                        var_bounds->bounds[i].start.variable_name = strdup("");
                    } else if ((check(parser, TOK_RBRACKET) || check(parser, TOK_COMMA))) {
                        parser_error(parser, "Expected bound for any dimension after the first one");
                        return NULL;
                    } else if (!parse_dimension_bounds(parser, &var_bounds->bounds[i])) {
                        symtable_destroy_bounds(var_bounds);
                        ast_destroy_node(declarations);
                        return NULL;
                    }
                }
                total_dimensions = dimension_count;
            } else {
                // Count dimensions
                total_dimensions = count_array_dimensions_ahead(parser->ctx.lexer, var_start, false);

                if (total_dimensions <= 0) {
                    ast_destroy_node(declarations);
                    return NULL;
                }
                var_bounds = parse_array_bounds_for_all_dimensions(parser, total_dimensions);
                if (!var_bounds) {
                    ast_destroy_node(declarations);
                    return NULL;
                }
            }
            match(parser, TOK_RBRACKET);
        } else if (g_config.allow_mixed_array_access && match(parser, TOK_LPAREN)) {
            is_array_decl = true;
            // Here we need to check all commas within the parentheses
            if (parser->ctx.peek->type == TOK_COMMA || count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, true)) {
                int dimension_count = 1;
                TokenType lookahead_type = parser->ctx.peek->type;
                dimension_count = count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, true);
                
                // Create bounds for all dimensions
                var_bounds = symtable_create_bounds(dimension_count);
                if (!var_bounds) {
                    ast_destroy_node(declarations);
                    return NULL;
                }
                // Parse each dimension's bounds
                for (int i = 0; i < dimension_count; i++) {
                    if (i > 0) {
                        if (!match(parser, TOK_COMMA)) {
                            parser_error(parser, "Expected ',' between dimensions");
                            symtable_destroy_bounds(var_bounds);
                            ast_destroy_node(declarations);
                            return NULL;
                        }
                    }
                    
                    if (i == 0 && (check(parser, TOK_RPAREN) || check(parser, TOK_COMMA))) {
                        var_bounds->bounds[i].using_range = false;
                        var_bounds->bounds[i].start.variable_name = strdup("");
                    } else if ((check(parser, TOK_RPAREN) || check(parser, TOK_COMMA))) {
                        parser_error(parser, "Expected bound for any dimension after the first one");
                        return NULL;
                    } else if (!parse_dimension_bounds(parser, &var_bounds->bounds[i])) {
                        symtable_destroy_bounds(var_bounds);
                        ast_destroy_node(declarations);
                        return NULL;
                    }
                }
                total_dimensions = dimension_count;
            } else {
                // Count dimensions
                total_dimensions = count_array_dimensions_ahead(parser->ctx.lexer, var_start, true);
                if (total_dimensions <= 0) {
                    ast_destroy_node(declarations);
                    return NULL;
                }
                var_bounds = parse_paren_array_bounds_for_all_dimensions(parser, total_dimensions);
                if (!var_bounds) {
                    ast_destroy_node(declarations);
                    return NULL;
                }
            }
            match(parser, TOK_RPAREN);
        }

        var = ast_create_node((is_array_decl || var_bounds) ? NODE_ARRAY_DECL : NODE_VAR_DECL);
        if (!var) {
            if (var_bounds) symtable_destroy_bounds(var_bounds);
            ast_destroy_node(declarations);
            return NULL;
        }
        ast_set_location(var, name->loc);


        var->data.variable.name = strdup(name->value);
        var->data.variable.is_array = (is_array_decl || var_bounds != NULL);
        var->data.variable.is_pointer = pointer_level > 0;
        var->data.variable.pointer_level = pointer_level;

        if (var_bounds) {
            var->data.variable.array_info.bounds = var_bounds;
            var->data.variable.array_info.dimensions = total_dimensions;
            var->data.variable.array_info.has_dynamic_size = false;
            
            // Check if any dimension has dynamic size
            for (int i = 0; i < total_dimensions; i++) {
                if (!var_bounds->bounds[i].start.is_constant || 
                    !var_bounds->bounds[i].end.is_constant) {
                    var->data.variable.array_info.has_dynamic_size = true;
                    break;
                }
            }
        }

        Symbol* param = NULL;
        if (parser->ctx.current_function) {
            param = symtable_lookup_parameter(parser->ctx.symbols, 
                                           parser->ctx.current_function, 
                                           name->value);
        }

        if (param && param->info.var.needs_type_declaration) {
            // Skip creating a new variable node - we'll update the parameter
            ast_add_child(param_decl, var);
        } else {
            ast_add_child(declarations, var);
        }
    } while (match(parser, TOK_COMMA));

    // Parse type declaration
    if (!match(parser, TOK_COLON)) {
        parser_error(parser, "Expected ':' after variable names");
        ast_destroy_node(declarations);
        return NULL;
    }

    // Parse array keyword if present
    bool is_array = false;
    ArrayBoundsData* type_bounds = NULL;
    int type_dimensions = 0;

    size_t var_start = parser->ctx.lexer->start;
    // Check for dimensional specifier (e.g., "2d array")
    if (parser->ctx.current->type == TOK_IDENTIFIER) {
        // here we need to make sure the dimension is either specified with d (5d) or implicit (check for number of bracket pairs or number of commas [,,,,] or [][][][][])
        //actually we cannot check here since there is no tok_identifier but tok_array directly
        type_dimensions = parse_array_dimension(parser);

    } else if (parser->ctx.current->type == TOK_ARRAY) {
        if (parser->ctx.peek->type == TOK_LBRACKET) {
            // we founds bounds, so we are in the implicit case: if is comma parse commas else count brackets
            type_dimensions = count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, false);
            if (type_dimensions == 1) { // we found no commas
                // check brackets
                type_dimensions = count_array_type_dimensions_ahead(parser->ctx.lexer, var_start, false);
                if (type_dimensions < 1)
                    type_dimensions = 1;
            }
        } else if (g_config.allow_mixed_array_access && parser->ctx.peek->type == TOK_LPAREN) {
            type_dimensions = count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, true);
            if (type_dimensions == 1) { // we found no commas
                // check brackets
                type_dimensions = count_array_type_dimensions_ahead(parser->ctx.lexer, var_start, true);
                if (type_dimensions < 1)
                    type_dimensions = 1;
            }
        }
    }

    if (type_dimensions > 0 || match(parser, TOK_ARRAY)) {
        is_array = true;
        
        if (type_dimensions > 0) {
            if (!match(parser, TOK_ARRAY)) {
                parser_error(parser, "Expected 'array' after dimension specifier");
                ast_destroy_node(declarations);
                return NULL;
            }
        }

        if (match(parser, TOK_LBRACKET)) {
            // check if is comma
            if (parser->ctx.peek->type == TOK_COMMA || count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, false)) {
                //int dimension_count = 1;
                //TokenType lookahead_type = parser->ctx.peek->type;
                //dimension_count = count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start);
                
                // Create bounds for all dimensions
                type_bounds = symtable_create_bounds(type_dimensions);
                if (!type_bounds) {
                    ast_destroy_node(declarations);
                    return NULL;
                }
                // Parse each dimension's bounds
                for (int i = 0; i < type_dimensions; i++) {
                    if (i > 0) {
                        if (!match(parser, TOK_COMMA)) {
                            parser_error(parser, "Expected ',' between dimensions");
                            symtable_destroy_bounds(type_bounds);
                            ast_destroy_node(declarations);
                            return NULL;
                        }
                    }
                    
                    if (i == 0 && (check(parser, TOK_RBRACKET) || check(parser, TOK_COMMA))) {
                        type_bounds->bounds[i].using_range = false;
                        type_bounds->bounds[i].start.variable_name = strdup("");
                    } else if ((check(parser, TOK_RBRACKET) || check(parser, TOK_COMMA))) {
                        parser_error(parser, "Expected bound for any dimension after the first one");
                        return NULL;
                    } else if (!parse_dimension_bounds(parser, &type_bounds->bounds[i])) {
                        symtable_destroy_bounds(type_bounds);
                        ast_destroy_node(declarations);
                        return NULL;
                    }
                }
            } else {
                type_bounds = parse_array_bounds_for_all_dimensions(parser, type_dimensions > 0 ? type_dimensions : 1);
                if (!type_bounds) {
                    ast_destroy_node(declarations);
                    return NULL;
                }
            }
            
            if (!match(parser, TOK_RBRACKET)) {
                parser_error(parser, "Expected ']' after array bounds");
                symtable_destroy_bounds(type_bounds);
                ast_destroy_node(declarations);
                return NULL;
            }
        } else if (g_config.allow_mixed_array_access && match(parser, TOK_LPAREN)) {
            if (parser->ctx.peek->type == TOK_COMMA || count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, true)) {
                //int dimension_count = 1;
                //TokenType lookahead_type = parser->ctx.peek->type;
                //dimension_count = count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start);
                
                // Create bounds for all dimensions
                type_bounds = symtable_create_bounds(type_dimensions);
                if (!type_bounds) {
                    ast_destroy_node(declarations);
                    return NULL;
                }
                // Parse each dimension's bounds
                for (int i = 0; i < type_dimensions; i++) {
                    if (i > 0) {
                        if (!match(parser, TOK_COMMA)) {
                            parser_error(parser, "Expected ',' between dimensions");
                            symtable_destroy_bounds(type_bounds);
                            ast_destroy_node(declarations);
                            return NULL;
                        }
                    }
                    
                    if (i == 0 && (check(parser, TOK_RPAREN) || check(parser, TOK_COMMA))) {
                        type_bounds->bounds[i].using_range = false;
                        type_bounds->bounds[i].start.variable_name = strdup("");
                    } else if ((check(parser, TOK_RPAREN) || check(parser, TOK_COMMA))) {
                        parser_error(parser, "Expected bound for any dimension after the first one");
                        return NULL;
                    } else if (!parse_dimension_bounds(parser, &type_bounds->bounds[i])) {
                        symtable_destroy_bounds(type_bounds);
                        ast_destroy_node(declarations);
                        return NULL;
                    }
                }
            } else {
                type_bounds = parse_paren_array_bounds_for_all_dimensions(parser, type_dimensions > 0 ? type_dimensions : 1);
                if (!type_bounds) {
                    ast_destroy_node(declarations);
                    return NULL;
                }
            }
            
            if (!match(parser, TOK_RPAREN)) {
                parser_error(parser, "Expected ')' after array bounds");
                symtable_destroy_bounds(type_bounds);
                ast_destroy_node(declarations);
                return NULL;
            }
        }

        if (!match(parser, TOK_OF)) {
            parser_error(parser, "Expected 'of' after 'array'");
            if (type_bounds) symtable_destroy_bounds(type_bounds);
            ast_destroy_node(declarations);
            return NULL;
        }
    }

    // Parse base type

    // TODO FIX THIS MESS
    if (match(parser, TOK_RECORD)) {
        ASTNode* var_node = declarations->children[0];
        int num_dimensions = 0;
        if (is_array || var_node->data.variable.is_array) {
            var_node->type = NODE_ARRAY_DECL;
            if (var_node->data.variable.is_array) {
                num_dimensions = var_node->data.variable.array_info.dimensions;
            } else if (type_dimensions > 0) {
                num_dimensions = type_dimensions;
                var_node->data.variable.array_info.dimensions = num_dimensions;
            } else if (type_bounds) {
                num_dimensions = type_bounds->dimensions;
                var_node->data.variable.array_info.dimensions = num_dimensions;
            } else {
                num_dimensions = 1;
                var_node->data.variable.array_info.dimensions = num_dimensions;
            }
        }


        printf("parsing record variable %d\n", num_dimensions);
        ASTNode* record_type = parse_record_type(parser, false);
        if (!record_type) {
            ast_destroy_node(declarations);
            return NULL;
        }
        // Set record name if not already set
        //if (!record_type->record_type.name) {
            record_type->record_type.name = strdup(declarations->children[0]->data.variable.name);
        //}
        ast_add_child(declarations->children[0], record_type);
        
        // Create and register type symbol
        Symbol* type_sym = symtable_add_type(parser->ctx.symbols, 
                                            record_type->record_type.name,
                                            &record_type->record_type);
        if (!type_sym) {
            parser_error(parser, "Failed to register record type");
            ast_destroy_node(declarations);
            return NULL;
        }

        printf("parsing record variable completed\n");
        return declarations;
    }

    int type_pointer_level = 0;
    ASTNode* base_type = parse_type_specifier(parser, &type_pointer_level);
    if (!base_type) {
        if (type_bounds) symtable_destroy_bounds(type_bounds);
        ast_destroy_node(declarations);
        return NULL;
    }

    // Apply type and bounds to all variables
    for (int i = 0; i < declarations->child_count; i++) {
        ASTNode* var_node = declarations->children[i];
        // Build complete type string
        char* full_type = malloc(strlen(base_type->data.value) + 32);
        if (!full_type) {
            if (type_bounds) symtable_destroy_bounds(type_bounds);
            ast_destroy_node(base_type);
            ast_destroy_node(declarations);
            return NULL;
        }
        
        if (is_array || var_node->data.variable.is_array) {
            var_node->type = NODE_ARRAY_DECL;
            int num_dimensions = 0;
            if (var_node->data.variable.is_array) {
                num_dimensions = var_node->data.variable.array_info.dimensions;
            } else if (type_dimensions > 0) {
                num_dimensions = type_dimensions;
            } else if (type_bounds) {
                num_dimensions = type_bounds->dimensions;
            } else {
                num_dimensions = 1;
            }
            
            // Build type string with correct number of "array of" prefixes
            strcpy(full_type, "");
            for (int j = 0; j < num_dimensions; j++) {
                strcat(full_type, "array of ");
            }
            strcat(full_type, base_type->data.value);
        } else {
            strcpy(full_type, base_type->data.value);
        }

        
            
        var_node->data.variable.type = full_type;
        var_node->data.variable.is_pointer = var_node->data.variable.is_pointer || type_pointer_level > 0;
        var_node->data.variable.pointer_level = var_node->data.variable.pointer_level + type_pointer_level;

        // If no bounds were specified with name but we have array type with bounds
        if (is_array && !var_node->data.variable.is_array && type_bounds) {
            var_node->data.variable.is_array = true;
            var_node->data.variable.array_info.bounds = symtable_clone_bounds(type_bounds);
            var_node->data.variable.array_info.dimensions = type_bounds->dimensions;
            var_node->data.variable.array_info.has_dynamic_size = false;
            
            // Check for dynamic size in type bounds
            for (int j = 0; j < type_bounds->dimensions; j++) {
                if (!type_bounds->bounds[j].start.is_constant || 
                    !type_bounds->bounds[j].end.is_constant) {
                    var_node->data.variable.array_info.has_dynamic_size = true;
                    break;
                }
            }
            var_node->type = NODE_ARRAY_DECL;
        }

        // Add to symbol table
        Symbol* sym = NULL;
        if (var_node->data.variable.is_array) {
            // Pass the complete bounds information
            sym = symtable_add_array(parser->ctx.symbols, 
                                var_node->data.variable.name,
                                base_type->data.value,
                                var_node->data.variable.array_info.bounds ? 
                                symtable_clone_bounds(var_node->data.variable.array_info.bounds) :
                                (type_bounds ? symtable_clone_bounds(type_bounds) : NULL));
        } else {
            sym = symtable_add_variable(parser->ctx.symbols,
                                    var_node->data.variable.name,
                                    var_node->data.variable.type,
                                    false);
        }

        if (!sym) {
            parser_error(parser, "Failed to add variable to symbol table");
            // Continue with other declarations
        } else {
            sym->info.var.is_pointer = var_node->data.variable.is_pointer;
            sym->info.var.pointer_level = var_node->data.variable.pointer_level;
        }
    }
    
    for (int i = 0; i < param_decl->child_count; ++i) {
        ASTNode* var_node = param_decl->children[i];
        Symbol* param = NULL;
        char* full_type = malloc(strlen(base_type->data.value) + 32);
        if (is_array || var_node->data.variable.is_array) {
            int num_dimensions = 0;
            if (var_node->data.variable.is_array) {
                num_dimensions = var_node->data.variable.array_info.dimensions;
            } else if (type_dimensions > 0) {
                num_dimensions = type_dimensions;
            } else if (type_bounds) {
                num_dimensions = type_bounds->dimensions;
            } else {
                num_dimensions = 1;
            }
            
            // Build type string with correct number of "array of" prefixes
            strcpy(full_type, "");
            for (int j = 0; j < num_dimensions; j++) {
                strcat(full_type, "array of ");
            }
            strcat(full_type, base_type->data.value);
        } else {
            strcpy(full_type, base_type->data.value);
        }

        if (parser->ctx.current_function) {
            param = symtable_lookup_parameter(parser->ctx.symbols,
                                           parser->ctx.current_function,
                                           var_node->data.variable.name);
        }
        if (param && param->info.var.needs_type_declaration) {
            // Update parameter type 
            param->info.var.type = strdup(full_type);
            param->info.var.is_pointer = var_node->data.variable.is_pointer || type_pointer_level > 0;
            param->info.var.pointer_level = var_node->data.variable.pointer_level + type_pointer_level;
            param->info.var.needs_type_declaration = false;

            if (param->node) {
                param->node->data.variable.type = strdup(full_type);
                param->node->data.parameter.is_pointer = var_node->data.variable.is_pointer || type_pointer_level > 0;
                param->node->data.parameter.pointer_level = var_node->data.variable.pointer_level + type_pointer_level;
            }

            //if (var_node->data.variable.is_array && var_node->data.variable.array_info.bounds) {
            //    param->node->data.variable.is_array = true;
            //    param->node->data.variable.array_info.bounds = symtable_clone_bounds(var_node->data.variable.array_info.bounds);
            //    param->node->data.variable.array_info.dimensions = var_node->data.variable.array_info.dimensions;
            //    param->node->data.variable.array_info.has_dynamic_size = var_node->data.variable.array_info.has_dynamic_size;
            //    param->node->type = NODE_ARRAY_DECL;
            //}

            if (is_array && !var_node->data.variable.is_array && type_bounds) {
                var_node->data.variable.is_array = true;
                var_node->data.variable.array_info.bounds = symtable_clone_bounds(type_bounds);
                var_node->data.variable.array_info.dimensions = type_bounds->dimensions;
                var_node->data.variable.array_info.has_dynamic_size = false;
                
                // Check for dynamic size in type bounds
                for (int j = 0; j < type_bounds->dimensions; j++) {
                    if (!type_bounds->bounds[j].start.is_constant || 
                        !type_bounds->bounds[j].end.is_constant) {
                        var_node->data.variable.array_info.has_dynamic_size = true;
                        break;
                    }
                }
                //var_node->type = NODE_ARRAY_DECL;

                //if (param->node) {
                //    param->node->data.variable.is_array = true;
                //    param->node->data.variable.array_info.bounds = symtable_clone_bounds(type_bounds);
                //    param->node->data.variable.array_info.dimensions = type_bounds->dimensions;
                 //   param->node->data.variable.array_info.has_dynamic_size = false;
                    
                    // Check for dynamic size in type bounds
                    //for (int j = 0; j < type_bounds->dimensions; j++) {
                    //    if (!type_bounds->bounds[j].start.is_constant || 
                    //        !type_bounds->bounds[j].end.is_constant) {
                    //        param->node->data.variable.array_info.has_dynamic_size = true;
                    //        break;
                    //    }
                    //}
                    //param->node->type = NODE_ARRAY_DECL;
                //}
            }
            if (is_array || type_bounds || var_node->data.variable.is_array || var_node->data.variable.array_info.bounds) 
                param->info.var.is_array = true;
            if (is_array || type_bounds || var_node->data.variable.array_info.bounds) {
                ArrayBoundsData* bounds = var_node->data.variable.array_info.bounds ? 
                                    symtable_clone_bounds(var_node->data.variable.array_info.bounds) :
                                    (type_bounds ? symtable_clone_bounds(type_bounds) : NULL);

                param->info.var.needs_deref = false;
                param->info.var.bounds = bounds;
                param->info.var.is_array = true;
                param->info.var.dimensions = bounds ? bounds->dimensions : 1;
                param->info.var.has_dynamic_size = false;

                // Check for dynamic size
                if (bounds) {
                    for (int i = 0; i < bounds->dimensions; i++) {
                        if (!bounds->bounds[i].start.is_constant || 
                            !bounds->bounds[i].end.is_constant) {
                            param->info.var.has_dynamic_size = true;
                        }
                        if (bounds->bounds[i].using_range) {
                        }
                    }
                }
            }
            // Shift remaining children down
            for (int j = i; j < param_decl->child_count - 1; j++) {
                param_decl->children[j] = param_decl->children[j + 1];
            }
            param_decl->child_count--;
            i--; // Process this index again since we shifted everything down
            free(full_type);
        }
    }
    ast_destroy_node(param_decl);
    //if (declarations->child_count == 0 && !declarations->children[0])
    //    ast_destroy_node(declarations);
    // Clean up
    ast_destroy_node(base_type);
    if (type_bounds) {
        symtable_destroy_bounds(type_bounds);
    }

    match(parser, TOK_SEMICOLON);

    return declarations;
}


static bool look_ahead_for_assignment(Parser* parser, int tokens_ahead) {
    // Save current token position
    size_t current = parser->ctx.lexer->current;
    Token* saved_current = parser->ctx.current;
    Token* saved_peek = parser->ctx.peek;
    
    bool result = false;
    
    // Look ahead desired number of tokens
    for (int i = 0; i < tokens_ahead; i++) {
        Token* next = lexer_next_token(parser->ctx.lexer);
        if (next && next->type == TOK_ASSIGN) {
            result = true;
        }
        token_destroy(next);
    }
    
    // Restore parser state
    parser->ctx.lexer->current = current;
    parser->ctx.current = saved_current;
    parser->ctx.peek = saved_peek;
    
    return result;
}

static bool is_start_of_assignment(Parser* parser) {
    verbose_print("Checking if current tokens start an assignment\n");
    
    // Count consecutive dereference operators
    size_t pos = parser->ctx.lexer->current;
    int deref_count = 0;
    
    while (parser->ctx.current->type == TOK_MULTIPLY || 
           parser->ctx.current->type == TOK_DEREF) {
        deref_count++;
        if (!parser->ctx.peek) break;
        parser->ctx.current = parser->ctx.peek;
        parser->ctx.peek = lexer_next_token(parser->ctx.lexer);
    }
    
    // Reset parser position
    parser->ctx.lexer->current = pos;
    
    // Handle patterns:
    // 1. Multiple dereferences followed by identifier and assignment
    if (deref_count > 0 && 
        parser->ctx.current->type == TOK_IDENTIFIER &&
        look_ahead_for_assignment(parser, 1)) {
        return true;
    }
    
    // 2. Simple identifier followed by assignment
    if (parser->ctx.current->type == TOK_IDENTIFIER && 
        parser->ctx.peek && parser->ctx.peek->type == TOK_ASSIGN) {
        return true;
    }
    
    // 3. Dereference operators at start
    if (parser->ctx.current->type == TOK_MULTIPLY || 
        parser->ctx.current->type == TOK_DEREF) {
        return true;
    }
    
    return false;
}

static bool is_dereferenced_assignment(Parser* parser) {
    verbose_print("\n=== CHECKING FOR DEREFERENCED ASSIGNMENT ===\n");
    if (!parser || !parser->ctx.current) {
        verbose_print("Invalid parser or current token\n");
        return false;
    }

    // Save current parser state
    Token* saved_current = parser->ctx.current;
    Token* saved_peek = parser->ctx.peek;
    size_t saved_pos = parser->ctx.lexer->current;
    int deref_count = 0;
    
    verbose_print("Starting token state:\n");
    verbose_print("  Current token: type=%d, value='%s', line=%d, col=%d\n", 
                 parser->ctx.current->type, 
                 parser->ctx.current->value,
                 parser->ctx.current->loc.line,
                 parser->ctx.current->loc.column);
    if (parser->ctx.peek) {
        verbose_print("  Peek token: type=%d, value='%s', line=%d, col=%d\n",
                     parser->ctx.peek->type,
                     parser->ctx.peek->value,
                     parser->ctx.peek->loc.line,
                     parser->ctx.peek->loc.column);
    }
    
    while (parser->ctx.current && 
           (parser->ctx.current->type == TOK_MULTIPLY || 
            parser->ctx.current->type == TOK_DEREF)) {
        deref_count++;
        verbose_print("Found dereference operator %d at line %d, col %d\n", 
                     deref_count,
                     parser->ctx.current->loc.line,
                     parser->ctx.current->loc.column);

        // Advance tokens
        Token* old_current = parser->ctx.current;
        parser->ctx.current = parser->ctx.peek;
        parser->ctx.peek = lexer_next_token(parser->ctx.lexer);
        
        if (parser->ctx.current) {
            verbose_print("Advanced to token: type=%d, value='%s'\n", 
                         parser->ctx.current->type,
                         parser->ctx.current->value);
        } else {
            verbose_print("Advanced to NULL current token\n");
            break;
        }
    }

    bool result = false;
    if (deref_count > 0) {
        verbose_print("After counting %d dereference operators:\n", deref_count);
        if (parser->ctx.current) {
            verbose_print("Current token: type=%d, value='%s'\n",
                         parser->ctx.current->type,
                         parser->ctx.current->value);
            
            if (parser->ctx.current->type == TOK_IDENTIFIER) {
                Symbol* sym = symtable_lookup(parser->ctx.symbols, parser->ctx.current->value);
                if (sym) {
                    verbose_print("Found symbol '%s' with pointer level %d\n",
                                sym->name, sym->info.var.pointer_level);
                    if (deref_count <= sym->info.var.pointer_level) {
                        if (parser->ctx.peek && parser->ctx.peek->type == TOK_ASSIGN) {
                            verbose_print("Found valid assignment pattern\n");
                            result = true;
                        }
                    }
                }
            }
        }
    }

    // Restore parser state
    parser->ctx.current = saved_current;
    parser->ctx.peek = saved_peek;
    parser->ctx.lexer->current = saved_pos;

    verbose_print("=== END DEREFERENCED ASSIGNMENT CHECK (result: %s) ===\n\n",
                 result ? "true" : "false");
    return result;
}

static ASTNode* parse_repeat_statement(Parser* parser) {
    verbose_print("Parsing repeat statement\n");
    Token* repeat_token = consume(parser, TOK_REPEAT, "Expected 'repeat'");
    
    ASTNode* repeat = ast_create_node(NODE_REPEAT);
    if (!repeat) return NULL;
    ast_set_location(repeat, repeat_token->loc);

    // Create a block node for the loop body
    ASTNode* body = ast_create_node(NODE_BLOCK);
    if (!body) {
        ast_destroy_node(repeat);
        return NULL;
    }
    ast_set_location(body, parser->ctx.current->loc);

    // Parse statements until 'until'
    while (!check(parser, TOK_UNTIL) && !check(parser, TOK_EOF)) {
        ASTNode* statement = parse_statement(parser);
        if (statement) {
            ast_add_child(body, statement);
        } else if (!parser->panic_mode) {
            ast_destroy_node(body);
            ast_destroy_node(repeat);
            return NULL;
        }
    }

    // Expect 'until'
    if (!match(parser, TOK_UNTIL)) {
        parser_error(parser, "Expected 'until' after repeat block");
        ast_destroy_node(body);
        ast_destroy_node(repeat);
        return NULL;
    }

    // Parse condition
    ASTNode* condition = parse_expression(parser);
    if (!condition) {
        ast_destroy_node(body);
        ast_destroy_node(repeat);
        return NULL;
    }

    // Add body and condition as children
    ast_add_child(repeat, body);
    ast_add_child(repeat, condition);

    return repeat;
}


static ASTNode* parse_statement(Parser* parser) {
    verbose_print("\n=== PARSING STATEMENT ===\n");
    verbose_print("Token state at start of parse_statement:\n");
    ASTNode* stmt;
    if (parser->ctx.current) {
        verbose_print("  Current token: type=%d, value='%s', line=%d, col=%d\n",
                     parser->ctx.current->type,
                     parser->ctx.current->value,
                     parser->ctx.current->loc.line,
                     parser->ctx.current->loc.column);
    } else {
        verbose_print("  Current token: NULL\n");
    }
    
    if (parser->ctx.peek) {
        verbose_print("  Peek token: type=%d, value='%s', line=%d, col=%d\n",
                     parser->ctx.peek->type,
                     parser->ctx.peek->value,
                     parser->ctx.peek->loc.line,
                     parser->ctx.peek->loc.column);
    } else {
        verbose_print("  Peek token: NULL\n");
    }

    if (parser->ctx.current->type == TOK_AT) {
        verbose_print("Found @ operator at start of statement\n");
        stmt = parse_assignment(parser);
        goto parsed;
    }

    if (parser->ctx.current && 
        (parser->ctx.current->type == TOK_MULTIPLY || 
         parser->ctx.current->type == TOK_DEREF)) {
        verbose_print("Found potential dereference operator, checking for assignment\n");
        if (is_dereferenced_assignment(parser)) {
            verbose_print("Confirmed as dereferenced assignment, parsing assignment\n");
            stmt = parse_assignment(parser);
            goto parsed;
        }
        verbose_print("Not a valid dereferenced assignment\n");
    }

    switch (parser->ctx.current->type) {
        case TOK_IF:
            verbose_print("Parsing if statement\n");
            stmt = parse_if_statement(parser);
            goto parsed;
        case TOK_WHILE:
            verbose_print("Parsing while statement\n");
            stmt = parse_while_statement(parser);
            goto parsed;
        case TOK_FOR:
            verbose_print("Parsing for statement\n");
            stmt = parse_for_statement(parser);
            goto parsed;
        case TOK_RETURN:
            verbose_print("Parsing return statement\n");
            stmt = parse_return_statement(parser);
            goto parsed;
        case TOK_BEGIN:
            verbose_print("Parsing block\n");
            stmt = parse_block(parser);
            goto parsed;
        case TOK_IDENTIFIER:
            // Look ahead to determine statement type
            Token* peek = parser->ctx.peek;
            Token* current = parser->ctx.current;
            
            // Look up symbol to check if it's an array
            Symbol* sym = symtable_lookup_parameter(parser->ctx.symbols, parser->ctx.current_function, current->value);

            if (!sym)
                sym = symtable_lookup(parser->ctx.symbols, current->value);
            
            // Handle array access with parentheses
            if (peek->type == TOK_LPAREN && g_config.allow_mixed_array_access && 
                sym && (sym->kind == SYMBOL_VARIABLE || sym->kind == SYMBOL_PARAMETER) && sym->info.var.is_array) {
                stmt = parse_assignment(parser);
                goto parsed;
            }
            
            // Handle regular array access
            if (peek->type == TOK_LBRACKET || peek->type == TOK_ASSIGN) {
                stmt = parse_assignment(parser);
                goto parsed;
            }

            // If it's not an array access or assignment, treat as procedure call
            if (peek->type == TOK_LPAREN) {
                stmt = parse_procedure_call(parser);
                goto parsed;
            }
            
            // Default to parsing as assignment if we see := later
            size_t i = parser->ctx.lexer->current;
            while (i < parser->ctx.lexer->source_length) {
                if (parser->ctx.lexer->source[i] == ':' && 
                    i + 1 < parser->ctx.lexer->source_length && 
                    parser->ctx.lexer->source[i + 1] == '=') {
                    stmt = parse_assignment(parser);
                    goto parsed;
                }
                if (parser->ctx.lexer->source[i] == ';' || 
                    parser->ctx.lexer->source[i] == '\n') {
                    break;
                }
                i++;
            }
            stmt = parse_procedure_call(parser);
            goto parsed;
        case TOK_VAR:
            stmt = parse_variable_declaration(parser);
            goto parsed;
        case TOK_PRINT:
            stmt = parse_print_statement(parser);
            goto parsed;
        case TOK_READ:
            stmt = parse_read_statement(parser);
            goto parsed;
        case TOK_REPEAT:
            stmt = parse_repeat_statement(parser);
            goto parsed;
        default:
            verbose_print("Unexpected token type %d in statement\n", parser->ctx.current->type);
            parser_error(parser, "Expected statement");
            return NULL;
    }
    verbose_print("End of statement parsing\n");
    verbose_print("=== END PARSING STATEMENT ===\n\n");
parsed:
    match(parser, TOK_SEMICOLON);
    return stmt;
}

static ASTNode* parse_block(Parser* parser) {
    verbose_print("Starting block parse\n");
    Token* begin_token = consume(parser, TOK_BEGIN, "Expected 'begin'");
    
    ASTNode* block = ast_create_node(NODE_BLOCK);
    if (!block) {
        verbose_print("Failed to create block node\n");
        return NULL;
    }
    ast_set_location(block, begin_token->loc);

    verbose_print("Entering block scope\n");
    symtable_enter_scope(parser->ctx.symbols, SCOPE_BLOCK);

    verbose_print("Parsing block statements\n");
    // Parse declarations and statements until 'end'
    while (!check(parser, TOK_END) && !check(parser, TOK_EOF)) {
        verbose_print("\nParsing next statement in block.\n");
        verbose_print("Current token: type=%d, value='%s', line=%d, col=%d\n",
                     parser->ctx.current->type,
                     parser->ctx.current->value,
                     parser->ctx.current->loc.line,
                     parser->ctx.current->loc.column);
        ASTNode* node = NULL;
        if (parser->ctx.current->type == TOK_MULTIPLY) {
            verbose_print("Found multiply/dereference at start of statement\n");
        }
        
        debug_print_token_info(parser->ctx.current, "Current token in block");
        // First try parsing a declaration
        if (is_declaration_start(parser->ctx.current->type)) {
            node = parse_declaration(parser);
        } else {
            node = parse_statement(parser);
        }

        if (node) {
            verbose_print("Adding statement to block\n");
            ast_add_child(block, node);
        } else if (!parser->panic_mode) {
            verbose_print("Statement parse failed, synchronizing\n");
            parser_sync_to_next_statement(parser);
        }
    }

    verbose_print("Exiting block\n");
    consume(parser, TOK_END, "Expected 'end'");
    symtable_exit_scope(parser->ctx.symbols);

    return block;
}

static ASTNode* parse_procedure_call(Parser* parser) {
    verbose_print("In parse_procedure_call\n");
    debug_print_token_info(parser->ctx.current, "Procedure call start token");
    Token* name = consume(parser, TOK_IDENTIFIER, "Expected procedure name");
    if (!name) return NULL;

    ASTNode* call = ast_create_node(NODE_CALL);
    if (!call) return NULL;
    ast_set_location(call, name->loc);

    call->data.value = strdup(name->value);

    // Parameter list
    if (!match(parser, TOK_LPAREN)) {
        parser_error(parser, "Expected '(' after procedure name");
        ast_destroy_node(call);
        return NULL;
    }

    // Parse arguments
    if (!check(parser, TOK_RPAREN)) {
        do {
            ASTNode* arg = parse_expression(parser);
            if (!arg) {
                ast_destroy_node(call);
                return NULL;
            }
            ast_add_child(call, arg);
        } while (match(parser, TOK_COMMA));
    }

    if (!match(parser, TOK_RPAREN)) {
        parser_error(parser, "Expected ')' after procedure arguments");
        ast_destroy_node(call);
        return NULL;
    }

    return call;
}

static ASTNode* parse_if_statement(Parser* parser) {
    verbose_print("Parsing if statement\n");
    Token* if_token = consume(parser, TOK_IF, "Expected 'if'");
    
    ASTNode* if_node = ast_create_node(NODE_IF);
    if (!if_node) return NULL;
    ast_set_location(if_node, if_token->loc);
    
    if_node->child_count = 2;
    // Parse condition
    if_node->children[0] = parse_expression(parser);
    if (!if_node->children[0]) {
        ast_destroy_node(if_node);
        return NULL;
    }
    
    if (!match(parser, TOK_THEN)) {
        parser_error(parser, "Expected 'then' after if condition");
        ast_destroy_node(if_node);
        return NULL;
    }

    // Create a block node for the "then" branch
    ASTNode* then_block = ast_create_node(NODE_BLOCK);
    if (!then_block) {
        ast_destroy_node(if_node);
        return NULL;
    }
    ast_set_location(then_block, parser->ctx.prev->loc);

    // Parse statements until 'else' or 'endif'
     while (!check(parser, TOK_ELSE) && !check(parser, TOK_ELSEIF) && 
           !check(parser, TOK_ENDIF) && !check(parser, TOK_EOF)) {
        ASTNode* statement = parse_statement(parser);
        if (statement) {
            ast_add_child(then_block, statement);
        } else if (!parser->panic_mode) {
            ast_destroy_node(then_block);
            ast_destroy_node(if_node);
            return NULL;
        }
    }

    // Attach the then block
    if_node->children[1] = then_block;

    ASTNode* current_if = if_node;
    
    while (match(parser, TOK_ELSEIF)) {
        // Create a new if node for the elseif branch
        ASTNode* elseif_node = ast_create_node(NODE_IF);
        if (!elseif_node) {
            ast_destroy_node(if_node);
            return NULL;
        }
        ast_set_location(elseif_node, parser->ctx.prev->loc);


        // Parse elseif condition
        elseif_node->children[0] = parse_expression(parser);
        if (!elseif_node->children[0]) {
            ast_destroy_node(elseif_node);
            ast_destroy_node(if_node);
            return NULL;
        }

        if (!match(parser, TOK_THEN)) {
            parser_error(parser, "Expected 'then' after elseif condition");
            ast_destroy_node(elseif_node);
            ast_destroy_node(if_node);
            return NULL;
        }

        // Create a block for the elseif branch
        ASTNode* elseif_block = ast_create_node(NODE_BLOCK);
        if (!elseif_block) {
            ast_destroy_node(elseif_node);
            ast_destroy_node(if_node);
            return NULL;    }
        ast_set_location(elseif_block, parser->ctx.prev->loc);


        // Parse statements for this elseif branch
        while (!check(parser, TOK_ELSE) && !check(parser, TOK_ELSEIF) && 
               !check(parser, TOK_ENDIF) && !check(parser, TOK_EOF)) {
            ASTNode* statement = parse_statement(parser);
            if (statement) {
                ast_add_child(elseif_block, statement);
            } else if (!parser->panic_mode) {
                ast_destroy_node(elseif_block);
                ast_destroy_node(elseif_node);
                ast_destroy_node(if_node);
                return NULL;
            }
        }

        elseif_node->children[1] = elseif_block;
        current_if->children[2] = elseif_node;  // Add as else branch of current if
        current_if->child_count = 3;
        current_if = elseif_node;               // Move to new if node for next iteration
    }

    // Handle optional else branch
    if (match(parser, TOK_ELSE)) {
        current_if->child_count = 3;
        // Create a block node for the "else" branch
        ASTNode* else_block = ast_create_node(NODE_BLOCK);
        if (!else_block) {
            ast_destroy_node(if_node);
            return NULL;
        }
        ast_set_location(else_block, parser->ctx.prev->loc);



        if (check(parser, TOK_IF)) {
            ASTNode* nested_if = parse_if_statement(parser);
            ast_add_child(else_block, nested_if);
        }

        // Parse statements until 'endif'
        while (!check(parser, TOK_ENDIF) && !check(parser, TOK_EOF)) {
            ASTNode* statement = parse_statement(parser);
            if (statement) {
                ast_add_child(else_block, statement);
            } else if (!parser->panic_mode) {
                ast_destroy_node(else_block);
                ast_destroy_node(if_node);
                return NULL;
            }
        }

        // Attach the else block
        current_if->children[2] = else_block;
    }

    // Expect 'endif'
    if (!match(parser, TOK_ENDIF)) {
        parser_error(parser, "Expected 'endif'");
        ast_destroy_node(if_node);
        return NULL;
    }

    return if_node;
}

static ASTNode* parse_while_statement(Parser* parser) {
    Token* while_token = consume(parser, TOK_WHILE, "Expected 'while'");
    
    ASTNode* while_node = ast_create_node(NODE_WHILE);
    if (!while_node) return NULL;
    ast_set_location(while_node, while_token->loc);


    while_node->child_count = 2;

    // Condition
    while_node->children[0] = parse_expression(parser);
    if (!while_node->children[0]) {
        ast_destroy_node(while_node);
        return NULL;
    }
    
    if (!match(parser, TOK_DO)) {
        parser_error(parser, "Expected 'do' after while condition");
        ast_destroy_node(while_node);
        return NULL;
    }

    // Create a block node for the loop body
    ASTNode* body = ast_create_node(NODE_BLOCK);
    if (!body) {
        ast_destroy_node(while_node);
        return NULL;
    }
    ast_set_location(body, parser->ctx.prev->loc);

    // Parse statements until endwhile
    while (!check(parser, TOK_ENDWHILE) && !check(parser, TOK_EOF)) {
        ASTNode* statement = parse_statement(parser);
        if (statement) {
            ast_add_child(body, statement);
        } else if (!parser->panic_mode) {
            ast_destroy_node(body);
            ast_destroy_node(while_node);
            return NULL;
        }
    }

    if (!match(parser, TOK_ENDWHILE)) {
        parser_error(parser, "Expected 'endwhile'");
        ast_destroy_node(body);
        ast_destroy_node(while_node);
        return NULL;
    }

    while_node->children[1] = body;
    return while_node;
}

static ASTNode* parse_for_statement(Parser* parser) {
    Token* for_token  = consume(parser, TOK_FOR, "Expected 'for'");
    
    ASTNode* for_node = ast_create_node(NODE_FOR);
    if (!for_node) return NULL;
    ast_set_location(for_node, for_token->loc);

    int child_count = 3;
    // Loop variable
    Token* var = consume(parser, TOK_IDENTIFIER, "Expected loop variable name");
    if (!var) {
        ast_destroy_node(for_node);
        return NULL;
    }

    for_node->data.value = strdup(var->value);

    // Initial value
    consume(parser, TOK_ASSIGN, "Expected assignment operator after loop variable");
    for_node->children[0] = parse_expression(parser);
    if (!for_node->children[0]) {
        ast_destroy_node(for_node);
        return NULL;
    }

    consume(parser, TOK_TO, "Expected 'to' after initial value");

    // End value
    for_node->children[1] = parse_expression(parser);
    if (!for_node->children[1]) {
        ast_destroy_node(for_node);
        return NULL;
    }

    // Optional step value
    ASTNode* step_value = NULL;
    if (match(parser, TOK_STEP)) {
        verbose_print("Parsing step value for FOR loop\n");
        
        // Check for negative step value
        bool is_negative = match(parser, TOK_MINUS);

        step_value = parse_expression(parser);
        if (!step_value) {
            ast_destroy_node(for_node);
            return NULL;
        }
        // Create a proper number node for the step value
        ASTNode* step_node = ast_create_node(NODE_NUMBER);
        if (!step_node) {
            ast_destroy_node(step_value);
            ast_destroy_node(for_node);
            return NULL;
        }
        ast_set_location(step_node, parser->ctx.current->loc);
        if (is_negative) {
            char* negative_value = malloc(strlen(step_value->data.value) + 1);
            negative_value[0] = '-';
            strcat(negative_value, step_value->data.value);
            step_value->data.value = negative_value;
        }
        step_node->data.value = strdup(step_value->data.value);
        ast_destroy_node(step_value);
        for_node->children[3] = step_node;
        verbose_print("Step value node created: type=%d, value=%s\n", 
                     step_node->type, 
                     step_node->data.value);
        ++child_count;
    }

    Token* do_token = consume(parser, TOK_DO, "Expected 'do' after loop bounds");

    // Track that we're in a loop for break/continue
    bool outer_loop = parser->ctx.in_loop;
    parser->ctx.in_loop = true;

    // Loop body
    /*for_node->children[2] = parse_statement(parser);
    if (!for_node->children[2]) {
        ast_destroy_node(for_node);
        parser->ctx.in_loop = outer_loop;
        return NULL;
    }

    parser->ctx.in_loop = outer_loop;

    if (!match(parser, TOK_ENDFOR)) {
        parser_error(parser, "Expected 'endfor'");
        ast_destroy_node(for_node);
        return NULL;
    }*/
    // Create a block node for the loop body
    ASTNode* body = ast_create_node(NODE_BLOCK);
    if (!body) {
        ast_destroy_node(for_node);
        parser->ctx.in_loop = outer_loop;
        return NULL;
    }
    ast_set_location(body, do_token->loc);


    // Parse statements until endfor
    while (!check(parser, TOK_ENDFOR) && !check(parser, TOK_EOF)) {
        ASTNode* statement = parse_statement(parser);
        if (statement) {
            ast_add_child(body, statement);
        } else if (!parser->panic_mode) {
            ast_destroy_node(body);
            ast_destroy_node(for_node);
            parser->ctx.in_loop = outer_loop;
            return NULL;
        }
    }

    if (!match(parser, TOK_ENDFOR)) {
        parser_error(parser, "Expected 'endfor'");
        ast_destroy_node(body);
        ast_destroy_node(for_node);
        parser->ctx.in_loop = outer_loop;
        return NULL;
    }
    // Attach the body block to the for node
    for_node->children[2] = body;
    for_node->child_count = child_count;

    parser->ctx.in_loop = outer_loop;


    //for_node->child_count = child_count;
    return for_node;
}

static ASTNode* parse_return_statement(Parser* parser) {
    Token* return_token = consume(parser, TOK_RETURN, "Expected 'return'");
    
    ASTNode* return_node = ast_create_node(NODE_RETURN);
    if (!return_node) return NULL;
    ast_set_location(return_node, return_token->loc);

    // Optional return value
    if (!check(parser, TOK_SEMICOLON) && !check(parser, TOK_END)) {
        return_node->children[0] = parse_expression(parser);
    }

    return return_node;
}

static ASTNode* parse_left_hand_side(Parser* parser) {
    verbose_print("\n=== PARSING LEFT HAND SIDE ===\n");
    debug_print_token_info(parser->ctx.current, "Current token at start of LHS");
    
    bool skip_deref = match(parser, TOK_AT);

    // Count consecutive dereference operators
    int deref_count = 0;
    while (check(parser, TOK_MULTIPLY) || check(parser, TOK_DEREF)) {
        verbose_print("Found dereference operator %d\n", deref_count + 1);
        deref_count++;
        debug_print_token_info(parser->ctx.current, "Current token after consuming *");
        advance(parser);
    }

    // Parse the base variable
    Token* start = parser->ctx.current;
    verbose_print("Parsing base variable after %d dereference operators\n", deref_count);
    ASTNode* var = parse_variable(parser);
    if (!var) {
        verbose_print("Failed to parse base variable\n");
        parser_error(parser, "Expected variable name on left hand side of assignment");
        return NULL;
    }

    if (!skip_deref && (var->type == NODE_IDENTIFIER || var->type == NODE_VARIABLE)) {
        const char* name = var->type == NODE_IDENTIFIER ? 
                          var->data.value : var->data.variable.name;
                          
        Symbol* sym = symtable_lookup(parser->ctx.symbols, name);
        
        if (sym && sym->info.var.is_parameter && sym->info.var.needs_deref && 
            (strcasecmp(sym->info.var.param_mode, "out") == 0 || 
             strcasecmp(sym->info.var.param_mode, "inout") == 0 || 
             strcasecmp(sym->info.var.param_mode, "in/out") == 0)) {
            
            // Create implicit dereference node
            ASTNode* deref = ast_create_node(NODE_UNARY_OP);
            if (!deref) {
                ast_destroy_node(var);
                return NULL;
            }
            ast_set_location(deref, parser->ctx.current->loc);
            deref->data.unary_op.op = TOK_DEREF;
            deref->data.unary_op.deref_count = 1;
            ast_add_child(deref, var);
            var = deref;
        }
    }

    // If we had dereference operators, create the unary node
    if (deref_count > 0) {
        verbose_print("Creating unary node for %d dereference operators\n", deref_count);
        Symbol* sym = NULL;
        const char* var_name = NULL;
        
        if (var->type == NODE_IDENTIFIER) {
            var_name = var->data.value;
            verbose_print("Variable is identifier: %s\n", var_name);
        } else if (var->type == NODE_VARIABLE) {
            var_name = var->data.variable.name;
            verbose_print("Variable is variable node: %s\n", var_name);
        }
        
        if (var_name) {
            verbose_print("Looking up symbol: %s\n", var_name);
            sym = symtable_lookup(parser->ctx.symbols, var_name);
            if (sym) {
                verbose_print("Found symbol with pointer level %d\n", sym->info.var.pointer_level);
                if (deref_count > sym->info.var.pointer_level) {
                    verbose_print("Too many dereference operators for pointer level\n");
                    parser_error(parser, "Too many dereference operators");
                    ast_destroy_node(var);
                    return NULL;
                }
            } else {
                verbose_print("Symbol not found in symbol table\n");
            }
        }

        ASTNode* deref = ast_create_node(NODE_UNARY_OP);
        if (!deref) {
            verbose_print("Failed to create unary node\n");
            ast_destroy_node(var);
            return NULL;
        }
        ast_set_location(deref, parser->ctx.current->loc);

        deref->data.unary_op.op = TOK_DEREF;
        deref->data.unary_op.deref_count = deref_count;
        ast_add_child(deref, var);
        verbose_print("Successfully created dereference node with %d levels\n", deref_count);
        
        verbose_print("=== END PARSING LEFT HAND SIDE ===\n\n");
        return deref;
    }

    verbose_print("=== END PARSING LEFT HAND SIDE ===\n\n");
    return var;
}

static ASTNode* parse_assignment(Parser* parser) {
    verbose_print("\n=== PARSING ASSIGNMENT ===\n");
    debug_print_parser_state_verb(parser, "START OF ASSIGNMENT");
    debug_print_token_info(parser->ctx.current, "Assignment start token");
    // Save the start of the left side for error reporting
    Token* start = parser->ctx.current;
    
    ASTNode* left = parse_left_hand_side(parser);
    if (!left) return NULL;

    debug_print_parser_state_verb(parser, "AFTER LEFT HAND SIDE");

    // Check for proper assignment operator based on config
    TokenType assign_op = g_config.assignment_style == ASSIGNMENT_EQUALS ? TOK_EQ : TOK_ASSIGN;
    SourceLocation assign_loc = token_clone_location(parser->ctx.current);
    
    if (!match(parser, assign_op)) {
        parser_error(parser, "Expected assignment operator");
        ast_destroy_node(left);
        return NULL;
    }

    debug_print_parser_state_verb(parser, "AFTER ASSIGNMENT OPERATOR");

    ASTNode* right = parse_expression(parser);
    if (!right) {
        ast_destroy_node(left);
        return NULL;
    }

    debug_print_parser_state_verb(parser, "AFTER RIGHT HAND SIDE");

    ASTNode* assign = ast_create_node(NODE_ASSIGNMENT);
    if (!assign) {
        ast_destroy_node(left);
        ast_destroy_node(right);
        return NULL;
    }
    ast_set_location(assign, assign_loc);
    

    assign->children[0] = left;
    assign->children[1] = right;
    assign->child_count = 2;

    debug_print_parser_state_verb(parser, "END OF ASSIGNMENT");
    verbose_print("=== END PARSING ASSIGNMENT ===\n\n");
    return assign;
}

// The precedence order from highest to lowest will be:
// 1. Primary (literals, parentheses)
// 2. Unary (!, ~, -)  
// 3. Multiplicative (*, /)
// 4. Additive (+, -)
// 5. Shift (<<, >>)
// 6. Bitwise AND (&)
// 7. Bitwise XOR (^)
// 8. Bitwise OR (|)
// 9. Logical AND (&&)
// 10. Logical OR (||)
// 11. Equality (==, !=)

ASTNode* parse_expression(Parser* parser) {
    debug_parser_rule_start(parser, "parse_expression");
    verbose_print("\n=== PARSING EXPRESSION ===\n");
    debug_print_parser_state_verb(parser, "START OF EXPRESSION");
    ASTNode* expr = parse_logical_or(parser);

    if (parser->ctx.current && 
        (parser->ctx.current->type == TOK_ASSIGN || 
         parser->ctx.current->type == TOK_SEMICOLON)) {
        verbose_print("Stopping expression parse at statement boundary\n");
    }

    debug_print_parser_state_verb(parser, "END OF EXPRESSION");
    debug_parser_expression(expr, "Expression result");
    debug_parser_rule_end(parser, "parse_expression", expr);
    verbose_print("=== END PARSING EXPRESSION ===\n\n");
    return expr;
}

static ASTNode* parse_logical_or(Parser* parser) {
    verbose_print("Parsing logical OR\n");
    ASTNode* expr = parse_logical_and(parser);
    if (!expr) return NULL;

    while (check(parser, TOK_OR)) {
        TokenType op = parser->ctx.current->type;
        SourceLocation or_loc = token_clone_location(parser->ctx.current);
        advance(parser);

        ASTNode* right = parse_logical_and(parser);
        if (!right) {
            ast_destroy_node(expr);
            return NULL;
        }

        ASTNode* new_expr = ast_create_node(NODE_BINARY_OP);
        if (!new_expr) {
            ast_destroy_node(expr);
            ast_destroy_node(right);
            return NULL;
        }
        ast_set_location(new_expr, or_loc);

        new_expr->data.binary_op.op = op;
        ast_add_child(new_expr, expr);
        ast_add_child(new_expr, right);
        expr = new_expr;
    }

    return expr;
}

static ASTNode* parse_logical_and(Parser* parser) {
    verbose_print("Parsing logical AND\n");
    ASTNode* expr = parse_bitwise_or(parser);  // Chain to bitwise OR
    if (!expr) return NULL;

    while (check(parser, TOK_AND)) {
        TokenType op = parser->ctx.current->type;
        SourceLocation and_loc = token_clone_location(parser->ctx.current);
        advance(parser);

        ASTNode* right = parse_bitwise_or(parser);  // Chain to bitwise OR
        if (!right) {
            ast_destroy_node(expr);
            return NULL;
        }

        ASTNode* new_expr = ast_create_node(NODE_BINARY_OP);
        if (!new_expr) {
            ast_destroy_node(expr);
            ast_destroy_node(right);
            return NULL;
        }
        ast_set_location(new_expr, and_loc);

        new_expr->data.binary_op.op = op;
        ast_add_child(new_expr, expr);
        ast_add_child(new_expr, right);
        expr = new_expr;
    }

    return expr;
}

static ASTNode* parse_bitwise_or(Parser* parser) {
    verbose_print("Parsing bitwise OR\n");
    // First get left operand from next precedence level
    ASTNode* expr = parse_bitwise_xor(parser);
    if (!expr) return NULL;

    // Then look for OR operators
    while (check(parser, TOK_BITOR)) {
        TokenType op = parser->ctx.current->type;
        SourceLocation bitor_loc = token_clone_location(parser->ctx.current);
        advance(parser);

        ASTNode* right = parse_bitwise_xor(parser);
        if (!right) {
            ast_destroy_node(expr);
            return NULL;
        }

        ASTNode* new_expr = ast_create_node(NODE_BINARY_OP);
        if (!new_expr) {
            ast_destroy_node(expr);
            ast_destroy_node(right);
            return NULL;
        }
        ast_set_location(new_expr, bitor_loc);

        new_expr->data.binary_op.op = op;
        ast_add_child(new_expr, expr);
        ast_add_child(new_expr, right);
        expr = new_expr;
    }

    return expr;
}

static ASTNode* parse_bitwise_xor(Parser* parser) {
    verbose_print("Parsing bitwise XOR\n");
    ASTNode* expr = parse_bitwise_and(parser);
    if (!expr) return NULL;

    while (check(parser, TOK_BITXOR)) {
        TokenType op = parser->ctx.current->type;
        SourceLocation bitxor_loc = token_clone_location(parser->ctx.current);
        advance(parser);

        ASTNode* right = parse_bitwise_and(parser);
        if (!right) {
            ast_destroy_node(expr);
            return NULL;
        }

        ASTNode* new_expr = ast_create_node(NODE_BINARY_OP);
        if (!new_expr) {
            ast_destroy_node(expr);
            ast_destroy_node(right);
            return NULL;
        }
        ast_set_location(new_expr, bitxor_loc);

        new_expr->data.binary_op.op = op;
        ast_add_child(new_expr, expr);
        ast_add_child(new_expr, right);
        expr = new_expr;
    }

    return expr;
}

static ASTNode* parse_bitwise_and(Parser* parser) {
    verbose_print("Parsing bitwise AND\n");
    ASTNode* expr = parse_shift(parser);
    if (!expr) return NULL;

    while (check(parser, TOK_BITAND)) {
        TokenType op = parser->ctx.current->type;
        SourceLocation bitand_loc = token_clone_location(parser->ctx.current);
        advance(parser);

        ASTNode* right = parse_shift(parser);
        if (!right) {
            ast_destroy_node(expr);
            return NULL;
        }

        ASTNode* new_expr = ast_create_node(NODE_BINARY_OP);
        if (!new_expr) {
            ast_destroy_node(expr);
            ast_destroy_node(right);
            return NULL;
        }
        ast_set_location(new_expr, bitand_loc);

        new_expr->data.binary_op.op = op;
        ast_add_child(new_expr, expr);
        ast_add_child(new_expr, right);
        expr = new_expr;
    }

    return expr;
}

static ASTNode* parse_shift(Parser* parser) {
    verbose_print("Parsing shift\n");
    ASTNode* expr = parse_equality(parser);
    if (!expr) return NULL;

    while (check(parser, TOK_LSHIFT) || check(parser, TOK_RSHIFT)) {
        TokenType op = parser->ctx.current->type;
        SourceLocation shift_loc = token_clone_location(parser->ctx.current);
        advance(parser);

        ASTNode* right = parse_equality(parser);
        if (!right) {
            ast_destroy_node(expr);
            return NULL;
        }

        ASTNode* new_expr = ast_create_node(NODE_BINARY_OP);
        if (!new_expr) {
            ast_destroy_node(expr);
            ast_destroy_node(right);
            return NULL;
        }
        ast_set_location(new_expr, shift_loc);

        new_expr->data.binary_op.op = op;
        ast_add_child(new_expr, expr);
        ast_add_child(new_expr, right);
        expr = new_expr;
    }

    return expr;
}

static ASTNode* parse_equality(Parser* parser) {
    verbose_print("Parsing equality\n");
    ASTNode* expr = parse_comparison(parser);
    if (!expr) return NULL;

    while (check(parser, TOK_EQ) || check(parser, TOK_NE)) {
        TokenType op = parser->ctx.current->type;
        SourceLocation equality_loc = token_clone_location(parser->ctx.current);
        advance(parser);

        ASTNode* right = parse_comparison(parser);
        if (!right) {
            ast_destroy_node(expr);
            return NULL;
        }

        ASTNode* new_expr = ast_create_node(NODE_BINARY_OP);
        if (!new_expr) {
            ast_destroy_node(expr);
            ast_destroy_node(right);
            return NULL;
        }
        ast_set_location(new_expr, equality_loc);

        new_expr->data.binary_op.op = op;
        ast_add_child(new_expr, expr);
        ast_add_child(new_expr, right);
        expr = new_expr;
    }

    return expr;
}

static ASTNode* parse_comparison(Parser* parser) {
    verbose_print("Parsing comparison\n");
    ASTNode* expr = parse_term(parser);
    if (!expr) return NULL;

    while (check(parser, TOK_LT) || check(parser, TOK_GT) ||
           check(parser, TOK_LE) || check(parser, TOK_GE)) {
        TokenType op = parser->ctx.current->type;
        SourceLocation comparison_loc = token_clone_location(parser->ctx.current);
        advance(parser);

        ASTNode* right = parse_term(parser);
        if (!right) {
            ast_destroy_node(expr);
            return NULL;
        }

        ASTNode* new_expr = ast_create_node(NODE_BINARY_OP);
        if (!new_expr) {
            ast_destroy_node(expr);
            ast_destroy_node(right);
            return NULL;
        }
        ast_set_location(new_expr, comparison_loc);

        new_expr->data.binary_op.op = op;
        ast_add_child(new_expr, expr);
        ast_add_child(new_expr, right);
        expr = new_expr;
    }

    return expr;
}

static ASTNode* parse_term(Parser* parser) {
    verbose_print("Parsing term\n");
    int start_line = parser->ctx.current->loc.line;
    ASTNode* expr = parse_factor(parser);
    if (!expr) return NULL;

    while (check(parser, TOK_PLUS) || check(parser, TOK_MINUS)) {
        if (start_line != parser->ctx.current->loc.line) {
            verbose_print("Detected new line in factor, stopping\n");
            break;
        }
        TokenType op = parser->ctx.current->type;
        SourceLocation sum_loc = token_clone_location(parser->ctx.current);
        advance(parser);

        ASTNode* right = parse_factor(parser);
        if (!right) {
            ast_destroy_node(expr);
            return NULL;
        }

        ASTNode* new_expr = ast_create_node(NODE_BINARY_OP);
        if (!new_expr) {
            ast_destroy_node(expr);
            ast_destroy_node(right);
            return NULL;
        }
        ast_set_location(new_expr, sum_loc);

        new_expr->data.binary_op.op = op;
        ast_add_child(new_expr, expr);
        ast_add_child(new_expr, right);
        expr = new_expr;
    }

    return expr;
}

static ASTNode* parse_factor(Parser* parser) {
    verbose_print("\n=== PARSING FACTOR ===\n");
    debug_print_parser_state_verb(parser, "START OF FACTOR");
    //Token* start_token = parser->ctx.current;
    int start_line = parser->ctx.current->loc.line;
    ASTNode* expr = parse_unary(parser);
    if (!expr) return NULL;

    while (check(parser, TOK_MULTIPLY) || check(parser, TOK_DIVIDE) || check(parser, TOK_MOD)) {
        if (start_line != parser->ctx.current->loc.line) {
            verbose_print("Detected new line in factor, stopping\n");
            break;
        }
        TokenType op = parser->ctx.current->type;
        //advance(parser);
        SourceLocation op_loc = token_clone_location(parser->ctx.current);
        advance(parser);
        //consume_token_with_trace(parser, "FACTOR OPERATOR");

        ASTNode* right = parse_unary(parser);
        if (!right) {
            ast_destroy_node(expr);
            return NULL;
        }

        ASTNode* new_expr = ast_create_node(NODE_BINARY_OP);
        if (!new_expr) {
            ast_destroy_node(expr);
            ast_destroy_node(right);
            return NULL;
        }
        ast_set_location(new_expr, op_loc);

        new_expr->data.binary_op.op = op;
        ast_add_child(new_expr, expr);
        ast_add_child(new_expr, right);
        expr = new_expr;
    }

    debug_print_parser_state_verb(parser, "END OF FACTOR");

    return expr;
}

static ASTNode* parse_unary(Parser* parser) {
    
    verbose_print("\n=== PARSING UNARY ===\n");
    debug_print_parser_state_verb(parser, "START OF UNARY");
    
    Token* start_token = parser->ctx.current;
    if (check(parser, TOK_MULTIPLY) || check(parser, TOK_DEREF)) {
        verbose_print("Found dereference operator\n");
        int deref_count = 0;
        SourceLocation deref_loc = token_clone_location(parser->ctx.current);
        
        // Count consecutive dereference operators
        while (check(parser, TOK_MULTIPLY) || check(parser, TOK_DEREF)) {
            verbose_print("Found dereference operator %d\n", deref_count);
            consume_token_with_trace(parser, "DEREFERENCE OPERATOR");
            deref_count++;
            //advance(parser);
        }
        
        verbose_print("Parsing operand after %d dereference operators\n", deref_count);
        ASTNode* operand = parse_primary(parser);
        if (!operand) {
            verbose_print("Failed to parse operand after dereference\n");
            return NULL;
        }

        // Look up symbol to validate dereferencing
        Symbol* sym = NULL;
        if (operand->type == NODE_IDENTIFIER) {
            sym = symtable_lookup(parser->ctx.symbols, operand->data.value);
        } else if (operand->type == NODE_VARIABLE) {
            sym = symtable_lookup(parser->ctx.symbols, operand->data.variable.name);
        }

        if (sym) {
            verbose_print("Found symbol %s with pointer level %d\n", 
                         sym->name, sym->info.var.pointer_level);
            if (deref_count > sym->info.var.pointer_level) {
                parser_error(parser, "Too many dereference operators");
                ast_destroy_node(operand);
                return NULL;
            }
        }

        // Create unary node for dereferencing
        ASTNode* node = ast_create_node(NODE_UNARY_OP);
        if (!node) {
            ast_destroy_node(operand);
            return NULL;
        }
        ast_set_location(node, deref_loc);

        node->data.unary_op.op = TOK_DEREF;
        node->data.unary_op.deref_count = deref_count;
        ast_add_child(node, operand);
        verbose_print("Created dereference node with %d levels\n", deref_count);
        return node;
    }
    else if (check(parser, TOK_ADDR_OF)) {
        SourceLocation addrof_loc = token_clone_location(parser->ctx.current);
        advance(parser);
        ASTNode* operand = parse_unary(parser);
        if (!operand) return NULL;

        ASTNode* node = ast_create_node(NODE_UNARY_OP);
        if (!node) {
            ast_destroy_node(operand);
            return NULL;
        }
        ast_set_location(node, addrof_loc);

        node->data.unary_op.op = TOK_ADDR_OF;
        node->data.unary_op.deref_count = 0;
        ast_add_child(node, operand);
        return node;
    }

    else if (check(parser, TOK_MINUS) || check(parser, TOK_NOT) || 
        check(parser, TOK_BITNOT)) {
        TokenType op = parser->ctx.current->type;
        SourceLocation op_loc = token_clone_location(parser->ctx.current);
        advance(parser);
        
        ASTNode* operand = parse_unary(parser);  // Recursively parse the operand
        if (!operand) return NULL;

        ASTNode* node = ast_create_node(NODE_UNARY_OP);
        if (!node) {
            ast_destroy_node(operand);
            return NULL;
        }
        ast_set_location(node, op_loc);

        node->data.unary_op.op = op;
        ast_add_child(node, operand);
        return node;
    }

    return parse_primary(parser);
}

static ASTNode* parse_primary(Parser* parser) {
    verbose_print("\n=== PARSING PRIMARY ===\n");
    debug_print_parser_state_verb(parser, "START OF PRIMARY");
    
    if (match(parser, TOK_AT)) {
        verbose_print("Found @ operator\n");
        SourceLocation at_loc = token_clone_location(parser->ctx.prev);
        ASTNode* expr = parse_primary(parser);
        if (!expr) return NULL;

        ASTNode* no_deref = ast_create_node(NODE_UNARY_OP);
        if (!no_deref) {
            ast_destroy_node(expr);
            return NULL;
        }
        ast_set_location(no_deref, at_loc);
        no_deref->data.unary_op.op = TOK_AT;
        no_deref->data.unary_op.deref_count = 0;
        expr->data.unary_op.op = TOK_AT;
        ast_add_child(no_deref, expr);
        return no_deref;
    }

    if (check(parser, TOK_NUMBER)) {
        Token* number = consume(parser, TOK_NUMBER, "Expected number");
        SourceLocation number_loc = token_clone_location(number);
        ASTNode* node = ast_create_node(NODE_NUMBER);
        if (!node) return NULL;
        ast_set_location(node, number_loc);
        node->data.value = strdup(number->value);
        return node;
    }

    if (check(parser, TOK_TRUE) || check(parser, TOK_FALSE)) {
        Token* bool_token = consume(parser, parser->ctx.current->type, "Expected boolean value");
        if (!bool_token) return NULL;
        SourceLocation bool_loc = token_clone_location(bool_token);

        ASTNode* node = ast_create_node(NODE_BOOL);
        if (!node) return NULL;
        ast_set_location(node, bool_loc);

        // Store standardized value
        node->data.value = strdup(bool_token->type == TOK_TRUE ? "1" : "0");
        return node;
    }

    if (check(parser, TOK_IDENTIFIER)) {
        Token* name = consume(parser, TOK_IDENTIFIER, "Expected identifier");
        ASTNode* node;
        SourceLocation identifier_loc = token_clone_location(name);

        // Check if it's an array access with either () or []
        
        Symbol* symbol = symtable_lookup_parameter(parser->ctx.symbols, parser->ctx.current_function, name->value);
        if (!symbol)
            symbol = symtable_lookup(parser->ctx.symbols, name->value);
        if (check(parser, TOK_LBRACKET) || 
        (check(parser, TOK_LPAREN) && g_config.allow_mixed_array_access && 
         symbol && (symbol->kind == SYMBOL_VARIABLE || symbol->kind == SYMBOL_PARAMETER) && symbol->info.var.is_array)) 
        {
            node = ast_create_node(NODE_IDENTIFIER);
            if (!node) return NULL;
            ast_set_location(node, identifier_loc);
            node->data.value = strdup(name->value);
            node = parse_array_access(parser, node);
        }
        // Check if it's a function call
        else if (check(parser, TOK_LPAREN)) {
            node = parse_function_call(parser, name->value);
        } else {
            node = ast_create_node(NODE_IDENTIFIER);
            ast_set_location(node, identifier_loc);
            if (!node) return NULL;
            ast_set_location(node, identifier_loc);
            node->data.value = strdup(name->value);
            if (symbol && symbol->info.var.is_parameter && symbol->info.var.needs_deref &&
                (strcasecmp(symbol->info.var.param_mode, "out") == 0 || 
                strcasecmp(symbol->info.var.param_mode, "inout") == 0 || 
                strcasecmp(symbol->info.var.param_mode, "in/out") == 0)) {
                ASTNode* deref = ast_create_node(NODE_UNARY_OP);
                if (!deref) {
                    ast_destroy_node(node);
                    return NULL;
                }
                ast_set_location(deref, identifier_loc);
                deref->data.unary_op.op = TOK_DEREF;
                deref->data.unary_op.deref_count = 1;
                ast_add_child(deref, node);
                return deref;
            }
        }

        return node;
    }

    if (match(parser, TOK_LPAREN)) {
        ASTNode* expr = parse_expression(parser);
        consume(parser, TOK_RPAREN, "Expected ')'");
        return expr;
    }

    parser_error(parser, "Expected expression");
    return NULL;
}

static ASTNode* parse_array_access(Parser* parser, ASTNode* array) {
    verbose_print("\nEntering parse_array_access\n");
    if (!parser || !array) {
        verbose_print("NULL parser or array\n");
        return NULL;
    }
    verbose_print("Initial array node type: %d\n", array->type);
    ASTNode* current = array;
    
    while ((check(parser, TOK_LBRACKET) || 
            (g_config.allow_mixed_array_access && check(parser, TOK_LPAREN)))) {
        SourceLocation array_access_loc = token_clone_location(parser->ctx.current);
        bool using_parens = check(parser, TOK_LPAREN);
        TokenType open_token = using_parens ? TOK_LPAREN : TOK_LBRACKET;
        TokenType close_token = using_parens ? TOK_RPAREN : TOK_RBRACKET;

        verbose_print("Found array access with %s\n", using_parens ? "parentheses" : "brackets");

        if (!match(parser, open_token)) {
            verbose_print("Failed to match opening token\n");
            if (current != array) {
                ast_destroy_node(current);
            }
            return array;
        }

        // Create new array access node
        ASTNode* access = ast_create_node(NODE_ARRAY_ACCESS);
        if (!access) {
            verbose_print("Failed to create array access node\n");
            if (current != array) {
                ast_destroy_node(current);
            }
            return NULL;
        }
        ast_set_location(access, array_access_loc);

        verbose_print("Created new array access node\n");

        // Link previous expression as array base
        ast_add_child(access, current);
        verbose_print("Added base node as first child\n");

        // Parse first index expression
        ASTNode* index = parse_expression(parser);
        if (!index) {
            verbose_print("Failed to parse index expression\n");
            ast_destroy_node(access);
            if (current != array) {
                ast_destroy_node(current);
            }
            return NULL;
        }
        ast_add_child(access, index);
        verbose_print("Added first index expression\n");

        // Handle comma-separated indices
        while (match(parser, TOK_COMMA)) {
            verbose_print("Found comma-separated index\n");
            index = parse_expression(parser);
            if (!index) {
                verbose_print("Failed to parse additional index expression\n");
                ast_destroy_node(access);
                if (current != array) {
                    ast_destroy_node(current);
                }
                return NULL;
            }
            ast_add_child(access, index);
            access->data.array_access.dimensions++;
            verbose_print("Added additional index expression\n");
        }

        if (!match(parser, close_token)) {
            verbose_print("Failed to match closing token\n");
            parser_error(parser, using_parens ? "Expected ')'" : "Expected ']'");
            ast_destroy_node(access);
            if (current != array) {
                ast_destroy_node(current);
            }
            return NULL;
        }

        access->data.array_access.dimensions = access->child_count - 1; // Subtract 1 for the array base
        current = access;
        verbose_print("Successfully created array access with %d dimensions\n", 
               access->data.array_access.dimensions);
    }
    verbose_print("Exiting parse_array_access, returning node type: %d\n", current->type);
    return current;
}

static ASTNode* parse_function_call(Parser* parser, const char* name) {
    ASTNode* call = ast_create_node(NODE_CALL);
    if (!call) return NULL;
    ast_set_location(call, parser->ctx.prev->loc);

    call->data.value = strdup(name);
    
    consume(parser, TOK_LPAREN, "Expected '(' after function name");

    // Parse arguments
    if (!check(parser, TOK_RPAREN)) {
        do {
            ASTNode* arg = parse_expression(parser);
            if (!arg) {
                ast_destroy_node(call);
                return NULL;
            }
            ast_add_child(call, arg);
        } while (match(parser, TOK_COMMA));
    }

    consume(parser, TOK_RPAREN, "Expected ')' after arguments");

    return call;
}

static int parse_array_dimension(Parser* parser) {
    verbose_print("Parsing array dimension\n");
    
    // Get the current token value
    const char* value = parser->ctx.current->value;
    if (!value) return 0;
    
    // Skip any whitespace
    while (*value && isspace(*value)) value++;
    
    // Try to parse a number
    char* endptr;
    long dim = strtol(value, &endptr, 10);
    
    // Check if we found a valid number and it's followed by 'd' or 'D'
    if (endptr != value && (*endptr == 'd' || *endptr == 'D') && dim > 0) {
        verbose_print("Found dimension: %ld\n", dim);
        advance(parser);  // Consume the dimension token
        return (int)dim;
    }
    return 0;  // Not a dimension specifier
}

static ArrayBoundsData* create_multidim_bounds(int dimensions) {
    ArrayBoundsData* bounds = symtable_create_bounds(dimensions);
    if (!bounds) return NULL;
    bounds->dimensions = dimensions;
    return bounds;
}

static ASTNode* parse_type_specifier(Parser* parser, int* pointer_level) {
    verbose_print("Parsing type specifier\n");

    ASTNode* type = ast_create_node(NODE_TYPE);
    if (!type) return NULL;
    ast_set_location(type, parser->ctx.current->loc);

    // Build type string for potentially nested array types
    char type_str[1024] = "";
    size_t type_len = 0;
    const char* array_prefix = "array of ";
    size_t prefix_len = strlen(array_prefix);

    // Check for dimensional specifier (e.g., "2d", "3d")
    int dimensions = parse_array_dimension(parser);
    size_t var_start = parser->ctx.lexer->start;
    if (dimensions > 0) {
        verbose_print("Found %dd array\n", dimensions);
        
        if (!match(parser, TOK_ARRAY)) {
            parser_error(parser, "Expected 'array' after dimension specifier");
            ast_destroy_node(type);
            return NULL;
        }

        if (!match(parser, TOK_OF)) {
            parser_error(parser, "Expected 'of' after 'array'");
            ast_destroy_node(type);
            return NULL;
        }

        // Add appropriate number of "array of " prefixes for each dimension
        for (int i = 0; i < dimensions; i++) {
            if (type_len + prefix_len >= sizeof(type_str) - 1) {
                parser_error(parser, "Type string too long");
                ast_destroy_node(type);
                return NULL;
            }
            strcat(type_str, array_prefix);
            type_len += prefix_len;
        }
    } else {
        // Count array dimensions from multiple brackets
        /*int bracket_dimensions = count_array_type_dimensions_ahead(parser->ctx.lexer, var_start);
        for (int i = 0; i < bracket_dimensions; ++i) {
            if (type_len + prefix_len >= sizeof(type_str) - 1) {
                parser_error(parser, "Type string too long");
                ast_destroy_node(type);
                return NULL;
            }
            strcat(type_str, array_prefix);
            type_len += prefix_len;
        }*/
        //dimensions = bracket_dimensions;
    }

    // Get base type
    const char* base_type;
    if (match(parser, TOK_INTEGER)) {
        base_type = "integer";
    } else if (match(parser, TOK_REAL)) {
        base_type = "real";
    } else if (match(parser, TOK_LOGICAL)) {
        base_type = "logical";
    } else if (match(parser, TOK_CHARACTER)) {
        base_type = "character";
    } else {
        printf("looking for type %s in global scope\n", parser->ctx.current->value);
        const char* type_name = parser->ctx.current->value;
        RecordTypeData* type_sym = symtable_lookup_type(parser->ctx.symbols, type_name);
        printf("ptr %d\n", type_sym);
        if (type_sym) {
            printf("found type in global scope\n");
            verbose_print("Found user-defined type: %s\n", type_name);
            base_type = type_name;
            advance(parser); // Consume the type name
        } else {
            printf("type not found\n");
            ast_destroy_node(type);
            return NULL;
        }
    }

    // Check for buffer overflow
    if (type_len + strlen(base_type) >= sizeof(type_str) - 1) {
        parser_error(parser, "Type string too long");
        ast_destroy_node(type);
        return NULL;
    }

    // Add base type to type string
    strcat(type_str, base_type);
    // Store complete type string and dimensions
    type->data.value = strdup(type_str);
    while (match(parser, TOK_MULTIPLY) || match(parser, TOK_DEREF)) {
        *pointer_level += 1;
    }

    if (dimensions)
        type->array_bounds.dimensions = dimensions;

    
    return type;
}

ASTNode* parse_parameter_list(Parser* parser) {
    //debug_trace_enter("parse_parameter_list");

    ASTNode* params = ast_create_node(NODE_PARAMETER_LIST);
    if (!params) return NULL;
    ast_set_location(params, parser->ctx.current->loc);

    if (!check(parser, TOK_RPAREN)) {
        do {
            ASTNode* param = parse_parameter(parser);
            if (!param) {
                ast_destroy_node(params);
                return NULL;
            }
            ast_add_child(params, param);
        } while (match(parser, TOK_COMMA) || match(parser, TOK_SEMICOLON));
    }
    //debug_trace_exit("parse_parameter_list", params);
    return params;
}

static ASTNode* parse_parameter(Parser* parser) {
    //debug_trace_enter("parse_parameter");
    verbose_print("Parsing parameter\n");
    
    ASTNode* param = ast_create_node(NODE_PARAMETER);
    if (!param) return NULL;
    ast_set_location(param, parser->ctx.current->loc);

    // Parse parameter mode (in, out, inout)
    if (match(parser, TOK_IN)) {
        param->data.parameter.mode = PARAM_MODE_IN;
        match(parser, TOK_COLON);
    } else if (match(parser, TOK_OUT)) {
        param->data.parameter.mode = PARAM_MODE_OUT;
        match(parser, TOK_COLON);
    } else if (match(parser, TOK_INOUT)) {
        param->data.parameter.mode = PARAM_MODE_INOUT;
        match(parser, TOK_COLON);
    } else {
        param->data.parameter.mode = PARAM_MODE_IN; // Default to IN
    }

    // Parse parameter name

    int pointer_level = 0;
        
    // Count consecutive pointer operators
    while (check(parser, TOK_MULTIPLY) || check(parser, TOK_DEREF)) {
        verbose_print("Found pointer operator level %d before parameter name\n", 
                    pointer_level + 1);
        pointer_level++;
        advance(parser);  // Consume the * token
    }


    size_t var_start = parser->ctx.lexer->start;

    Token* name = consume(parser, TOK_IDENTIFIER, "Expected parameter name");

    if (!name) {
        ast_destroy_node(param);
        return NULL;
    }
    param->data.parameter.name = strdup(name->value);

    ArrayBoundsData* bounds = NULL;
    int total_dimensions = 0;
    bool has_brackets = false;
    // Parse array bounds that appear after the parameter name
    if (match(parser, TOK_LBRACKET)) {
        has_brackets = true;
        // Check if we have comma-separated dimensions like [n,m]
        if (parser->ctx.peek->type == TOK_COMMA || count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, false)) {  //TODO do the same for LBRACKET and for type LBRACKET and for for var decl
            
            int dimension_count = 1;
            TokenType lookahead_type = parser->ctx.peek->type;
            dimension_count = count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, false);
            
            // Create bounds for all dimensions
            bounds = symtable_create_bounds(dimension_count);
            if (!bounds) {
                ast_destroy_node(param);
                return NULL;
            }
            // Parse each dimension's bounds
            for (int i = 0; i < dimension_count; i++) {
                if (i > 0) {
                    if (!match(parser, TOK_COMMA)) {
                        parser_error(parser, "Expected ',' between dimensions");
                        symtable_destroy_bounds(bounds);
                        ast_destroy_node(param);
                        return NULL;
                    }
                }
                if (i == 0 && (check(parser, TOK_RBRACKET) || check(parser, TOK_COMMA))) {
                    bounds->bounds[i].using_range = false;
                    bounds->bounds[i].start.variable_name = strdup("");
                } else if ((check(parser, TOK_RBRACKET) || check(parser, TOK_COMMA))) {
                    parser_error(parser, "Expected bound for any dimension after the first one");
                    return NULL;
                } else if (!parse_dimension_bounds(parser, &bounds->bounds[i])) {
                    symtable_destroy_bounds(bounds);
                    ast_destroy_node(param);
                    return NULL;
                }
            }
            total_dimensions = dimension_count;
            
        } else {
            // Single dimension bounds like [n] or [1..n]

            total_dimensions = count_array_dimensions_ahead(parser->ctx.lexer, var_start, false);
            if (total_dimensions <= 0) {
                ast_destroy_node(param);
                return NULL;
            }
            bounds = parse_array_bounds_for_all_dimensions(parser, total_dimensions);

            if (!bounds) {
                ast_destroy_node(param);
                return NULL;
            }
        }
        if (!match(parser, TOK_RBRACKET)) {
            parser_error(parser, "Expected ']' after array bounds");
            symtable_destroy_bounds(bounds);
            ast_destroy_node(param);
            return NULL;
        }
    } else if (g_config.allow_mixed_array_access && match(parser, TOK_LPAREN)) {
        has_brackets = true;
        // Check if we have comma-separated dimensions like [n,m]
        if (parser->ctx.peek->type == TOK_COMMA || count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, true)) {
            int dimension_count = 1;
            TokenType lookahead_type = parser->ctx.peek->type;
            dimension_count = count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, true);
            
            // Create bounds for all dimensions
            bounds = symtable_create_bounds(dimension_count);
            if (!bounds) {
                ast_destroy_node(param);
                return NULL;
            }
            // Parse each dimension's bounds
            for (int i = 0; i < dimension_count; i++) {
                if (i > 0) {
                    if (!match(parser, TOK_COMMA)) {
                        parser_error(parser, "Expected ',' between dimensions");
                        symtable_destroy_bounds(bounds);
                        ast_destroy_node(param);
                        return NULL;
                    }
                }
                if (i == 0 && (check(parser, TOK_RPAREN) || check(parser, TOK_COMMA))) {
                    bounds->bounds[i].using_range = false;
                    bounds->bounds[i].start.variable_name = strdup("");
                } else if (check(parser, TOK_RPAREN) || check(parser, TOK_COMMA)) {
                    parser_error(parser, "Expected bound for any dimension after the first one");
                    return NULL;
                } else if (!parse_dimension_bounds(parser, &bounds->bounds[i])) {
                    symtable_destroy_bounds(bounds);
                    ast_destroy_node(param);
                    return NULL;
                }
            }
            total_dimensions = dimension_count;
            
        } else {
            // Single dimension bounds like [n] or [1..n]
            total_dimensions = count_array_dimensions_ahead(parser->ctx.lexer, var_start, true);
            if (total_dimensions <= 0) {
                ast_destroy_node(param);
                return NULL;
            }
            bounds = parse_paren_array_bounds_for_all_dimensions(parser, total_dimensions);
            if (!bounds) {
                ast_destroy_node(param);
                return NULL;
            }
        }
        if (!match(parser, TOK_RPAREN)) {
            parser_error(parser, "Expected ')' after array bounds");
            symtable_destroy_bounds(bounds);
            ast_destroy_node(param);
            return NULL;
        }
    }
    
    // Parse type declaration
    if (!match(parser, TOK_COLON) && g_config.param_style == PARAM_STYLE_IN_DECL) {
        parser_error(parser, "Expected ':' after parameter name");
        if (bounds) symtable_destroy_bounds(bounds);
        ast_destroy_node(param);
        return NULL;
    }

    // Handle dimensional array types (e.g., "2d array")
    bool is_array = false;
    ArrayBoundsData* type_bounds = NULL;
    int type_dimensions = 0;



    if (parser->ctx.current->type == TOK_IDENTIFIER) {
        type_dimensions = parse_array_dimension(parser);
    } else if (parser->ctx.current->type == TOK_ARRAY) {
        if (parser->ctx.peek->type == TOK_LBRACKET) {
            // we founds bounds, so we are in the implicit case: if is comma parse commas else count brackets
            size_t var_start = parser->ctx.lexer->start;
            type_dimensions = count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, false);
            if (type_dimensions == 1) { // we found no commas
                // check brackets
                type_dimensions = count_array_type_dimensions_ahead(parser->ctx.lexer, var_start, false);
                if (type_dimensions < 1)
                    type_dimensions = 1;
            }
        } else if (g_config.allow_mixed_array_access && parser->ctx.peek->type == TOK_LPAREN) {
             // we founds bounds, so we are in the implicit case: if is comma parse commas else count brackets
            size_t var_start = parser->ctx.lexer->start;
            type_dimensions = count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, true);
            if (type_dimensions == 1) { // we found no commas
                // check brackets
                type_dimensions = count_array_type_dimensions_ahead(parser->ctx.lexer, var_start, true);
                if (type_dimensions < 1)
                    type_dimensions = 1;
            }
        }
    }

    if (type_dimensions > 0 || match(parser, TOK_ARRAY)) {
        is_array = true;
        if (type_dimensions > 0) {
            if (!match(parser, TOK_ARRAY)) {
                if (bounds) symtable_destroy_bounds(bounds);
                ast_destroy_node(param);
                return NULL;
            }
        }

        // Handle array bounds in type declaration
        if (match(parser, TOK_LBRACKET)) {
            if (parser->ctx.peek->type == TOK_COMMA || count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, false)) {
                // Create bounds for all dimensions
                type_bounds = symtable_create_bounds(type_dimensions);
                if (!type_bounds) {
                    ast_destroy_node(param);
                    return NULL;
                }
                // Parse each dimension's bounds
                for (int i = 0; i < type_dimensions; i++) {
                    if (i > 0) {
                        if (!match(parser, TOK_COMMA)) {
                            parser_error(parser, "Expected ',' between dimensions");
                            symtable_destroy_bounds(type_bounds);
                            ast_destroy_node(param);
                            return NULL;
                        }
                    }
                    if (i == 0 && (check(parser, TOK_RBRACKET) || check(parser, TOK_COMMA))) {
                        type_bounds->bounds[i].using_range = false;
                        type_bounds->bounds[i].start.variable_name = strdup("");
                    } else if ((check(parser, TOK_RBRACKET) || check(parser, TOK_COMMA))) {
                        parser_error(parser, "Expected bound for any dimension after the first one");
                        return NULL;
                    } else if (!parse_dimension_bounds(parser, &type_bounds->bounds[i])) {
                        symtable_destroy_bounds(type_bounds);
                        ast_destroy_node(param);
                        return NULL;
                    }
                }
            } else {
                type_bounds = parse_array_bounds_for_all_dimensions(parser, 
                    type_dimensions > 0 ? type_dimensions : 1);
                if (!type_bounds) {
                    if (bounds) symtable_destroy_bounds(bounds);
                    ast_destroy_node(param);
                    return NULL;
                }
            }
            if (!match(parser, TOK_RBRACKET)) {
                parser_error(parser, "Expected ']' after array bounds");
                symtable_destroy_bounds(type_bounds);
                if (bounds) symtable_destroy_bounds(bounds);
                ast_destroy_node(param);
                return NULL;
            }
        } else if (match(parser, TOK_LPAREN)) {
            if (parser->ctx.peek->type == TOK_COMMA || count_comma_array_dimensions_ahead(parser->ctx.lexer, var_start, true)) {
                // Create bounds for all dimensions
                type_bounds = symtable_create_bounds(type_dimensions);
                if (!type_bounds) {
                    ast_destroy_node(param);
                    return NULL;
                }
                // Parse each dimension's bounds
                for (int i = 0; i < type_dimensions; i++) {
                    if (i > 0) {
                        if (!match(parser, TOK_COMMA)) {
                            parser_error(parser, "Expected ',' between dimensions");
                            symtable_destroy_bounds(type_bounds);
                            ast_destroy_node(param);
                            return NULL;
                        }
                    }
                    if (i == 0 && (check(parser, TOK_RPAREN) || check(parser, TOK_COMMA))) {
                        type_bounds->bounds[i].using_range = false;
                        type_bounds->bounds[i].start.variable_name = strdup("");
                    } else if ((check(parser, TOK_RPAREN) || check(parser, TOK_COMMA))) {
                        parser_error(parser, "Expected bound for any dimension after the first one");
                        return NULL;
                    } else if (!parse_dimension_bounds(parser, &type_bounds->bounds[i])) {
                        symtable_destroy_bounds(type_bounds);
                        ast_destroy_node(param);
                        return NULL;
                    }
                }
            } else {
                type_bounds = parse_paren_array_bounds_for_all_dimensions(parser, 
                    type_dimensions > 0 ? type_dimensions : 1);
                if (!type_bounds) {
                    if (bounds) symtable_destroy_bounds(bounds);
                    ast_destroy_node(param);
                    return NULL;
                }
            }
            if (!match(parser, TOK_RPAREN)) {
                parser_error(parser, "Expected ')' after array bounds");
                symtable_destroy_bounds(type_bounds);
                if (bounds) symtable_destroy_bounds(bounds);
                ast_destroy_node(param);
                return NULL;
            }
        }

        if (!match(parser, TOK_OF)) {
            parser_error(parser, "Expected 'of' after array declaration");
            if (type_bounds) symtable_destroy_bounds(type_bounds);
            if (bounds) symtable_destroy_bounds(bounds);
            ast_destroy_node(param);
            return NULL;
        }
    }

    // Check for dimension mismatch
    if (has_brackets && type_dimensions > 0 && type_dimensions != total_dimensions) {
        parser_error(parser, "Dimension mismatch between array bounds and type");
        if (type_bounds) symtable_destroy_bounds(type_bounds);
        if (bounds) symtable_destroy_bounds(bounds);
        ast_destroy_node(param);
        return NULL;
    }

    // Parse base type
    int type_pointer_level = 0;
    ASTNode* base_type = parse_type_specifier(parser, &type_pointer_level);
    if (!base_type && g_config.param_style == PARAM_STYLE_IN_DECL) {
        if (type_bounds) symtable_destroy_bounds(type_bounds);
        if (bounds) symtable_destroy_bounds(bounds);
        ast_destroy_node(param);
        return NULL;
    }

    // Build complete type string
    int array_dimensions = 0;
    if (is_array) {
        array_dimensions = type_dimensions > 0 ? type_dimensions : 1;
    }
    if (has_brackets && total_dimensions > array_dimensions) {
        array_dimensions = total_dimensions;
    }

    if (base_type) {
        char* full_type = malloc(strlen(base_type->data.value) + array_dimensions * 32);
        if (!full_type) {
            ast_destroy_node(base_type);
            if (type_bounds) symtable_destroy_bounds(type_bounds);
            if (bounds) symtable_destroy_bounds(bounds);
            ast_destroy_node(param);
            return NULL;
        }

        strcpy(full_type, "");
        for (int i = 0; i < array_dimensions; i++) {
            strcat(full_type, "array of ");
        }
        strcat(full_type, base_type->data.value);
        param->data.parameter.type = full_type;
        param->data.parameter.pointer_level = type_pointer_level + pointer_level;
        param->data.parameter.is_pointer = type_pointer_level + pointer_level > 0;
    }
    // Add to symbol table
    Symbol* sym = symtable_add_parameter(parser->ctx.symbols, 
                                       param->data.parameter.name,
                                       param->data.parameter.type,
                                       param->data.parameter.mode == PARAM_MODE_IN ? "in" :
                                       param->data.parameter.mode == PARAM_MODE_OUT ? "out" : "inout",
                                       param,
                                       !(is_array || has_brackets));
    if (sym) {
        sym->info.var.is_pointer = type_pointer_level + pointer_level > 0;
        sym->info.var.pointer_level = type_pointer_level + pointer_level;
        if (is_array || has_brackets) {
            sym->info.var.is_array = true;
            sym->info.var.dimensions = array_dimensions;
            sym->info.var.needs_deref = false;
            
            // Use bounds from either the name or type declaration
            if (bounds) {
                sym->info.var.bounds = symtable_clone_bounds(bounds);
            } else if (type_bounds) {
                sym->info.var.bounds = symtable_clone_bounds(type_bounds);
            }

            
            // Check for dynamic sizes
            ArrayBoundsData* used_bounds = bounds ? bounds : type_bounds;
            if (used_bounds) {
                sym->info.var.has_dynamic_size = false;
                for (int i = 0; i < used_bounds->dimensions; i++) {
                    if (!used_bounds->bounds[i].start.is_constant || 
                        !used_bounds->bounds[i].end.is_constant) {
                        sym->info.var.has_dynamic_size = true;
                        break;
                    }
                }
                // Debug print
                debug_print_bounds("Added parameter", sym);
                symtable_update_parameter_bounds_in_global(parser->ctx.symbols, sym->name, used_bounds);
            }
        }
    }

    // Clean up
    ast_destroy_node(base_type);
    if (type_bounds) symtable_destroy_bounds(type_bounds);
    if (bounds) symtable_destroy_bounds(bounds);

    //debug_trace_exit("parse_parameter", param);
    return param;
}

static ASTNode* parse_record_type(Parser* parser, bool is_typedef) {
    printf("parsing record type\n");
    ASTNode* record = ast_create_node(NODE_RECORD_TYPE);
    if (!record) return NULL;

    record->record_type.is_typedef = is_typedef;
    record->record_type.is_nested = false; // Will be set to true by parent if nested
    record->record_type.fields = NULL;
    record->record_type.field_count = 0;
    record->record_type.parent = NULL;

    // For non-typedef records in var declarations, create a temporary type name
    if (!is_typedef) {
        static int anon_record_count = 0;
        char temp_name[32];
        snprintf(temp_name, sizeof(temp_name), "record_%d", anon_record_count++);
        record->record_type.name = strdup(temp_name);
    }

    // Parse fields until 'end'
    while (!check(parser, TOK_END)) {
        ASTNode* field = parse_record_field(parser);
        if (!field) {
            ast_destroy_node(record);
            return NULL;
        }
        
        // If this field contains a nested record, set up the parent-child relationship
        if (field->children[0] && field->children[0]->type == NODE_RECORD_TYPE) {
            field->children[0]->record_type.parent = &record->record_type;
            field->children[0]->record_type.is_nested = true;
            
            // Create name for nested record if not provided
            //if (!field->children[0]->record_type.name) {
            //    field->children[0]->record_type.name = strdup(field->data.variable.name);
            //}
        }
        
        ast_add_child(record, field);
    }

    consume(parser, TOK_END, "Expected 'end' after record fields");
    return record;
}

static ASTNode* parse_record_field(Parser* parser) {
    // Get field name
    Token* name = consume(parser, TOK_IDENTIFIER, "Expected field name");
    if (!name) return NULL;

    if (!match(parser, TOK_COLON)) {
        parser_error(parser, "Expected ':' after field name");
        return NULL;
    }

    ASTNode* field = ast_create_node(NODE_RECORD_FIELD);
    if (!field) return NULL;

    field->data.variable.name = strdup(name->value);

    printf("parsing nested %s\n", name->value);

    // Check if field is a nested record
    if (match(parser, TOK_RECORD)) {
        ASTNode* nested_record = parse_record_type(parser, false);
        if (!nested_record) {
            ast_destroy_node(field);
            return NULL;
        }
        // Set the nested record's name to be the same as the field
        //if (nested_record->record_type.name == NULL) {
            nested_record->record_type.name = strdup(name->value);
        //}
        nested_record->record_type.is_nested = true;
        ast_add_child(field, nested_record);
        return field;
    }

    // Not a record, try parsing as a type specifier
    int pointer_level = 0;
    ASTNode* type = parse_type_specifier(parser, &pointer_level);
    if (!type) {
        ast_destroy_node(field);
        return NULL;
    }

    field->data.variable.type = strdup(type->data.value);
    field->data.variable.is_pointer = pointer_level > 0;
    field->data.variable.pointer_level = pointer_level;
    ast_destroy_node(type);

    return field;
}

static ASTNode* parse_type_declaration(Parser* parser) {
    //consume(parser, TOK_TYPE, "Expected 'type'");
    
    Token* name = consume(parser, TOK_IDENTIFIER, "Expected type name");
    if (!name) return NULL;

    if (!match(parser, TOK_COLON)) {
        parser_error(parser, "Expected ':' after type name");
        return NULL;
    }

    if (!match(parser, TOK_RECORD)) {
        parser_error(parser, "Expected 'record' after ':'");
        return NULL;
    }

    ASTNode* record = parse_record_type(parser, true);
    if (!record) return NULL;

    record->record_type.name = strdup(name->value);
    symtable_add_type(parser->ctx.symbols, name->value, &record->record_type);

    ASTNode* type_decl = ast_create_node(NODE_TYPE_DECLARATION);
    if (!type_decl) {
        ast_destroy_node(record);
        return NULL;
    }

    ast_add_child(type_decl, record);
    return type_decl;
}