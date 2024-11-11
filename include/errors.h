#ifndef PLIKE_ERROR_H
#define PLIKE_ERROR_H

#include "lexer.h"
#include "logger.h"
#include "config.h"
#include <stdbool.h>
#include <stdarg.h>

#define verbose_print(f_, ...) //if (g_config.enable_verbose) printf((f_), ##__VA_ARGS__)

#define ERR_ARRAY_BOUNDS "Array index out of bounds"
#define ERR_INVALID_ARRAY "Invalid array access"
#define ERR_DYNAMIC_SIZE "Array size must be constant in this context"

typedef enum {
    ERROR_LEXICAL,
    ERROR_SYNTAX,
    ERROR_SEMANTIC,
    ERROR_TYPE,
    ERROR_INTERNAL
} ErrorType;

typedef enum {
    SEVERITY_WARNING,
    SEVERITY_ERROR,
    SEVERITY_FATAL
} ErrorSeverity;

typedef struct {
    ErrorType type;
    ErrorSeverity severity;
    SourceLocation location;
    char* message;
    char* source_line;
    int error_code;
} Error;

// Error reporting
void error_init(void);
void error_report(ErrorType type, ErrorSeverity severity, 
                 SourceLocation location, const char* format, ...);
void error_at_token(Token* token, const char* format, ...);
void error_at_current(const char* format, ...);

// Error handling
bool error_occurred(void);
int error_count(void);
void error_clear(void);
void error_print_summary(void);

// Utility functions
const char* error_type_string(ErrorType type);
const char* error_severity_string(ErrorSeverity severity);
void error_print_source_line(SourceLocation location);

// Error recovery
void error_synchronize(void);
bool error_panic_mode(void);
void error_begin_panic_mode(void);
void error_end_panic_mode(void);

#endif // PLIKE_ERROR_H