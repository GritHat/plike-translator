#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static FILE* verbose_file = NULL;
static FILE* error_file = NULL;
static int current_indent = 0;
static bool logging_enabled = false;
static VerboseFlags verbose_flags = 0;

#define MAX_INDENT 50
#define INDENT_WIDTH 2

const char* log_component_name[] = {
    [VERBOSE_LEXER] = "[Lexer] ",
    [VERBOSE_PARSER] = "[Parser] ",
    [VERBOSE_AST] = "[AST] ",
    [VERBOSE_SYMBOLS] = "[Symbols] ",
    [VERBOSE_CODEGEN] = "[Codegen] "
};

void logger_init(bool enable_verbose) {
    logging_enabled = enable_verbose;
    
    if (logging_enabled) {
        verbose_file = fopen("verbose.log", "w");
        if (!verbose_file) {
            fprintf(stderr, "Failed to open verbose.log for writing\n");
            logging_enabled = false;
        }
    }
    
    error_file = fopen("logs/error.log", "w");
    if (!error_file) {
        fprintf(stderr, "Failed to open error.log for writing\n");
    }
    
    current_indent = 0;
}

void logger_set_verbose_flags(VerboseFlags flags) {
    verbose_flags = flags;
}

void logger_enable_verbose(VerboseFlags flag) {
    verbose_flags |= flag;
}

void logger_disable_verbose(VerboseFlags flag) {
    verbose_flags &= ~flag;
}

void logger_cleanup(void) {
    if (verbose_file) {
        fclose(verbose_file);
        verbose_file = NULL;
    }
    if (error_file) {
        fclose(error_file);
        error_file = NULL;
    }
}

static void write_indent(FILE* file, int indent) {
    for (int i = 0; i < indent * INDENT_WIDTH && i < MAX_INDENT; i++) {
        fprintf(file, " ");
    }
}

void log_verbose(int indent, VerboseFlags component, const char* format, ...) {
    if (!logging_enabled || !verbose_file || !(verbose_flags & component)) return;

    write_indent(verbose_file, current_indent + indent);
    fprintf(verbose_file, log_component_name[component]);

    va_list args;
    va_start(args, format);
    vfprintf(verbose_file, format, args);
    va_end(args);
    
    // Always end with newline if not present
    if (format[strlen(format) - 1] != '\n') {
        fprintf(verbose_file, "\n");
    }
    
    fflush(verbose_file);
}

void log_verbose_enter(const char* block_name) {
    if (!logging_enabled || !verbose_file) return;
    
    write_indent(verbose_file, current_indent);
    fprintf(verbose_file, "=== Enter: %s ===\n", block_name);
    current_indent++;
    fflush(verbose_file);
}

void log_verbose_exit(const char* block_name) {
    if (!logging_enabled || !verbose_file) return;
    
    if (current_indent > 0) current_indent--;
    write_indent(verbose_file, current_indent);
    fprintf(verbose_file, "=== Exit: %s ===\n", block_name);
    fflush(verbose_file);
}

void log_error(const char* format, ...) {
    va_list args;
    
    // Write to stderr
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    // Write to error log if available
    if (error_file) {
        va_start(args, format);
        vfprintf(error_file, format, args);
        va_end(args);
        fflush(error_file);
    }
}