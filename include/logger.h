// logger.h
#ifndef PLIKE_LOGGER_H
#define PLIKE_LOGGER_H

#include <stdbool.h>

typedef enum {
    VERBOSE_LEXER = 1 << 0,
    VERBOSE_PARSER = 1 << 1,
    VERBOSE_AST = 1 << 2,
    VERBOSE_SYMBOLS = 1 << 3,
    VERBOSE_CODEGEN = 1 << 4,
    VERBOSE_ALL = 0xFFFF
} VerboseFlags;

// Initialize logging system
void logger_init(bool enable_verbose);

// Set verbose flags
void logger_set_verbose_flags(VerboseFlags flags);
void logger_enable_verbose(VerboseFlags flag);
void logger_disable_verbose(VerboseFlags flag);

// Clean up logging system
void logger_cleanup(void);

// Verbose logging functions with flag checks
void log_verbose_lexer(int indent, const char* format, ...);
void log_verbose_parser(int indent, const char* format, ...);
void log_verbose_ast(int indent, const char* format, ...);
void log_verbose_symbols(int indent, const char* format, ...);
void log_verbose_codegen(int indent, const char* format, ...);

// Block logging with flag checks
void log_verbose_enter_block(VerboseFlags flag, const char* block_name);
void log_verbose_exit_block(VerboseFlags flag, const char* block_name);

// Error logging (always enabled)
void log_error(const char* format, ...);

#endif // PLIKE_LOGGER_H
