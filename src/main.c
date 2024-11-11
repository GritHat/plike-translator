#include "config.h"
#include "lexer.h"
#include "parser.h"
#include "symtable.h"
#include "codegen.h"
#include "errors.h"
#include "debug.h"
#include "logger.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

static void print_usage(const char* program_name) {
    fprintf(stderr, "Usage: %s [options] input_file [output_file]\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -a, --assignment=STYLE    Set assignment style (colon-equals|equals)\n");
    fprintf(stderr, "  -i, --indexing=STYLE      Set array indexing style (zero|one)\n");
    fprintf(stderr, "  -p, --params=STYLE        Set parameter style (decl|body|mixed)\n");
    fprintf(stderr, "  -o, --operators=STYLE     Set operator style (standard|dotted|mixed)\n");
    fprintf(stderr, "  -m, --mixed-arrays        Allow mixed array access ([] and ())\n");
    fprintf(stderr, "  -d, --debug=FLAGS         Set debug flags (lexer,parser,ast,symbols,codegen,all)\n");
    fprintf(stderr, "  -v, --verbose             Enable verbose output\n");
    fprintf(stderr, "  -h, --help                Display this help message\n");
}

static DebugFlags parse_debug_flags(const char* flags_str) {
    DebugFlags flags = 0;
    char* flags_copy = strdup(flags_str);
    char* token = strtok(flags_copy, ",");
    
    while (token) {
        if (strcasecmp(token, "lexer") == 0) flags |= DEBUG_LEXER;
        else if (strcasecmp(token, "parser") == 0) flags |= DEBUG_PARSER;
        else if (strcasecmp(token, "ast") == 0) flags |= DEBUG_AST;
        else if (strcasecmp(token, "symbols") == 0) flags |= DEBUG_SYMBOLS;
        else if (strcasecmp(token, "codegen") == 0) flags |= DEBUG_CODEGEN;
        else if (strcasecmp(token, "all") == 0) flags |= DEBUG_ALL;
        token = strtok(NULL, ",");
    }
    
    free(flags_copy);
    return flags;
}

int main(int argc, char** argv) {
    // Initialize configuration with defaults
    config_init();
    // Define long options
    static struct option long_options[] = {
        {"assignment", required_argument, 0, 'a'},
        {"indexing", required_argument, 0, 'i'},
        {"params", required_argument, 0, 'p'},
        {"operators", required_argument, 0, 'o'},
        {"mixed-arrays", no_argument, 0, 'm'},
        {"debug", required_argument, 0, 'd'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    // Parse command line arguments
    if (!config_parse_args(argc, argv)) {
        print_usage(argv[0]);
        return 1;
    }

    // Initialize debug system with default flags
    debug_init();

    debug_set_flags(DEBUG_ALL);
    // Initialize logger
    logger_init(g_config.enable_verbose);

    verbose_print("Creating lexer for file: %s\n", g_config.input_filename);
    // Create lexer
    Lexer* lexer = lexer_create(g_config.input_filename);
    if (!lexer) {
        fprintf(stderr, "Failed to create lexer\n");
        return 1;
    }

    verbose_print("Creating parser...\n");
    // Create parser
    Parser* parser = parser_create(lexer);
    if (!parser) {
        fprintf(stderr, "Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    verbose_print("Starting parsing...\n");
    // Parse input file
    debug_visualize_symbol_table(parser->ctx.symbols, "visualize/symbols_initial.dot");
    ASTNode* ast = parser_parse(parser);
    debug_visualize_symbol_table(parser->ctx.symbols, "visualize/symbols_post_parse.dot");

    verbose_print("Checking for errors...\n");
    // Check for errors before proceeding
    if (error_count() > 0) {
        fprintf(stderr, "Compilation failed with %d errors\n", error_count());
        if (ast) ast_destroy_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return 1;
    }

    if (!ast) {
        fprintf(stderr, "Parsing failed\n");
        parser_destroy(parser);
        lexer_destroy(lexer);
        return 1;
    }

    verbose_print("Parsing successful, visualizing AST...\n");
    debug_print_ast(ast, 0, false);
    debug_visualize_ast(ast, "visualize/ast.dot");

    verbose_print("Opening output file: %s\n", g_config.output_filename);
    // Open output file
    FILE* output = fopen(g_config.output_filename, "w");
    if (!output) {
        fprintf(stderr, "Failed to open output file: %s\n", g_config.output_filename);
        ast_destroy_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return 1;
    }

    verbose_print("Creating code generator...\n");
    // Create code generator
    CodeGenerator* codegen = codegen_create(output, parser->ctx.symbols);
    if (!codegen) {
        fprintf(stderr, "Failed to create code generator\n");
        fclose(output);
        ast_destroy_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return 1;
    }

    verbose_print("Generating code...\n");
    // Generate code
    codegen_generate(codegen, ast);

    verbose_print("Cleanup...\n");
    // Clean up
    fclose(output);
    codegen_destroy(codegen);
    ast_destroy_node(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);


    printf("Compilation completed. Output written to %s\n", g_config.output_filename);
    if (error_count() > 0) {
        printf("Compilation completed with %d errors\n", error_count());
        return 1;
    }
    
    config_cleanup();
    logger_cleanup();

    return 0;
}