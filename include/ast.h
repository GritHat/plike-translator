#ifndef PLIKE_AST_H
#define PLIKE_AST_H

#include "lexer.h"
#include <stdbool.h>

#define MAX_ARRAY_DIMENSIONS 10

typedef enum {
    NODE_PROGRAM,
    NODE_FUNCTION,
    NODE_PROCEDURE,
    NODE_PARAMETER,
    NODE_VARIABLE,
    NODE_VAR_DECL,
    NODE_ARRAY_DECL,
    NODE_BLOCK,
    NODE_ASSIGNMENT,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_REPEAT,
    NODE_RETURN,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_ARRAY_ACCESS,
    NODE_IDENTIFIER,
    NODE_NUMBER,
    NODE_BOOL,
    NODE_CALL,
    NODE_PRINT,
    NODE_READ,
    NODE_ARRAY_BOUNDS,
    NODE_STRING,
    NODE_TYPE,
    NODE_PARAMETER_LIST
} NodeType;

typedef enum {
    PARAM_MODE_IN,
    PARAM_MODE_OUT,
    PARAM_MODE_INOUT
} ParameterMode;

typedef struct {
    char* name;
    char* return_type;
    struct ASTNode* params;
    struct ASTNode* body;
    bool is_procedure;
    bool type_before_name;
    bool is_pointer;
    int pointer_level;
} FunctionData;

typedef struct {
    bool using_range;            // Whether bounds use range syntax (e.g., 1..n)
    struct {
        bool is_constant;       // Whether bound is a constant
        union {
            long constant_value;  // For constant bounds
            char* variable_name;  // For variable bounds
        };
    } start, end;              // Start and end bounds for ranges
} DimensionBounds;

typedef struct {
    int dimensions;             // Number of dimensions
    DimensionBounds* bounds;    // Array of bounds, one per dimension
} ArrayBoundsData;

/*typedef struct {
    char* name;
    char* type;
    bool is_array;
    int dimensions;
    struct ASTNode* size_expr;
    bool is_param;
    char* param_mode;  // "in", "out", or "inout"
} VariableData;*/

typedef struct {
    char* name;
    char* type;
    bool is_array;
    bool is_pointer;
    int pointer_level;
    union {
        struct {
            int dimensions;             // Number of dimensions
            ArrayBoundsData* bounds;    // Array bounds info, one per dimension
            bool has_dynamic_size;      // Whether any dimension uses variables
        } array_info;
        struct {
            bool is_reference;          // Whether variable is passed by reference
        } scalar_info;
    };
    bool is_param;
    char* param_mode;          // "in", "out", or "inout"
    bool is_constant;          // For const declarations
    SourceLocation decl_loc;   // Location of declaration
} VariableData;

typedef struct {
    TokenType op;
    struct ASTNode* left;
    struct ASTNode* right;
} BinaryOpData;

typedef struct {
    TokenType op;
    int deref_count;
} UnaryOpData;

typedef struct {
    int dimensions;
} ArrayAccessData;

typedef struct {
    char* name;
    char* type;
    ParameterMode mode;
    bool is_pointer;
    int pointer_level;
} ParameterData;

typedef struct ASTNode {
    NodeType type;
    SourceLocation loc;
    union {
        FunctionData function;
        VariableData variable;
        BinaryOpData binary_op;
        UnaryOpData unary_op;
        ArrayAccessData array_access;
        ParameterData parameter;
        char* value;  // For identifiers and literals   
    } data;
    ArrayBoundsData array_bounds;
    struct ASTNode** children;
    int child_count;
    int child_capacity;
} ASTNode;

// AST functions
ASTNode* ast_create_node(NodeType type);
void ast_destroy_node(ASTNode* node);
void ast_add_child(ASTNode* parent, ASTNode* child);
ASTNode* ast_create_function(const char* name, const char* return_type, bool is_procedure);
ASTNode* ast_create_string(const char* value);
ASTNode* ast_create_variable(const char* name, const char* type, NodeType node_type);
ASTNode* ast_create_binary_op(TokenType op, ASTNode* left, ASTNode* right);
//void ast_print(ASTNode* node, int indent);
bool ast_is_node_type(const ASTNode* node, NodeType type);
const char* ast_node_type_to_string(ASTNode* node);
char* ast_to_string(const ASTNode* node);
void ast_set_location(ASTNode* node, SourceLocation loc);
#endif // PLIKE_AST_H