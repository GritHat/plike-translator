#ifndef PLIKE_CONFIG_H
#define PLIKE_CONFIG_H

#include <stdbool.h>

typedef enum {
    ASSIGNMENT_COLON_EQUALS,  // := for assignment
    ASSIGNMENT_EQUALS         // = for assignment
} AssignmentStyle;

typedef enum {
    ARRAY_ZERO_BASED,        // 0-based indexing
    ARRAY_ONE_BASED          // 1-based indexing
} ArrayIndexing;

typedef enum {
    PARAM_STYLE_IN_DECL,     // Type in parameter declaration
    PARAM_STYLE_IN_BODY,     // Type in function body
    PARAM_STYLE_MIXED        // Allow both styles
} ParameterStyle;

typedef enum {
    OP_STYLE_STANDARD,       // and, or, not
    OP_STYLE_DOTTED,        // .and., .or., .not.
    OP_STYLE_MIXED          // Allow both styles
} OperatorStyle;

typedef struct {
    AssignmentStyle assignment_style;
    ArrayIndexing array_indexing;
    ParameterStyle param_style;
    OperatorStyle operator_style;
    bool allow_mixed_array_access;  // Allow both [] and () for array access
    char* input_filename;
    char* output_filename;
    bool enable_verbose;
    bool enable_bounds_checking;
} TranslatorConfig;

// Global configuration instance
extern TranslatorConfig g_config;

// Configuration functions
void config_init(void);
void config_set_defaults(void);
bool config_parse_args(int argc, char** argv);
void config_cleanup(void);

#endif // PLIKE_CONFIG_H