#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define MAX_ERROR_MESSAGE 1024
#define MAX_ERRORS 100

static struct {
    int count;
    bool panic_mode;
    Error errors[MAX_ERRORS];
    char* current_file;
    char** source_lines;
    int source_line_count;
} error_state = {0};

void error_init(void) {
    memset(&error_state, 0, sizeof(error_state));
}

static void store_error(ErrorType type, ErrorSeverity severity, 
                       SourceLocation location, const char* message) {
    if (error_state.count >= MAX_ERRORS) {
        log_error("Too many errors. Aborting.\n");
        exit(1);
    }

    Error* error = &error_state.errors[error_state.count++];
    error->type = type;
    error->severity = severity;
    error->location = location;
    error->message = strdup(message);
    error->error_code = error_state.count;

    // Store source line if available
    if (error_state.source_lines && location.line > 0 && 
        location.line <= error_state.source_line_count) {
        error->source_line = strdup(error_state.source_lines[location.line - 1]);
    } else {
        error->source_line = NULL;
    }
}

void error_report(ErrorType type, ErrorSeverity severity, 
                 SourceLocation location, const char* format, ...) {
    char message[MAX_ERROR_MESSAGE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    store_error(type, severity, location, message);

    // Print error immediately
    log_error("%s:%d:%d: %s: %s\n",
            location.filename ? location.filename : "<unknown>",
            location.line,
            location.column,
            error_severity_string(severity),
            message);

    // Print source line and error indicator if available
    if (error_state.source_lines && location.line > 0 && 
        location.line <= error_state.source_line_count) {
        log_error("%s\n", error_state.source_lines[location.line - 1]);
        for (int i = 0; i < location.column - 1; i++) {
            log_error(" ");
        }
        log_error("^\n");
    }

    if (severity == SEVERITY_FATAL) {
        error_print_summary();
        exit(1);
    }
}

void error_at_token(Token* token, const char* format, ...) {
    char message[MAX_ERROR_MESSAGE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    error_report(ERROR_SYNTAX, SEVERITY_ERROR, token->loc, message);
}

void error_at_current(const char* format, ...) {
    char message[MAX_ERROR_MESSAGE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    log_error("Error: %s\n", message);
}

bool error_occurred(void) {
    return error_state.count > 0;
}

int error_count(void) {
    return error_state.count;
}

void error_clear(void) {
    for (int i = 0; i < error_state.count; i++) {
        free(error_state.errors[i].message);
        free(error_state.errors[i].source_line);
    }
    error_state.count = 0;
    error_state.panic_mode = false;
}

void error_print_summary(void) {
    if (error_state.count == 0) {
        return;
    }

    log_error("\nError Summary:\n");
    log_error("-------------\n");
    log_error("Total errors: %d\n\n", error_state.count);

    int warnings = 0, errors = 0, fatals = 0;
    for (int i = 0; i < error_state.count; i++) {
        switch (error_state.errors[i].severity) {
            case SEVERITY_WARNING: warnings++; break;
            case SEVERITY_ERROR: errors++; break;
            case SEVERITY_FATAL: fatals++; break;
        }
    }

    log_error("Warnings: %d\n", warnings);
    log_error("Errors: %d\n", errors);
    log_error("Fatal errors: %d\n", fatals);
}

const char* error_type_string(ErrorType type) {
    switch (type) {
        case ERROR_LEXICAL: return "Lexical error";
        case ERROR_SYNTAX: return "Syntax error";
        case ERROR_SEMANTIC: return "Semantic error";
        case ERROR_TYPE: return "Type error";
        case ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* error_severity_string(ErrorSeverity severity) {
    switch (severity) {
        case SEVERITY_WARNING: return "Warning";
        case SEVERITY_ERROR: return "Error";
        case SEVERITY_FATAL: return "Fatal error";
        default: return "Unknown severity";
    }
}

void error_begin_panic_mode(void) {
    error_state.panic_mode = true;
}

void error_end_panic_mode(void) {
    error_state.panic_mode = false;
}

bool error_panic_mode(void) {
    return error_state.panic_mode;
}

void error_synchronize(void) {
    error_begin_panic_mode();
    // Parser will implement specific synchronization logic
}