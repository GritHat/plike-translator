#include "ast.h"
#include "symtable.h"
#include "errors.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INITIAL_CHILDREN_CAPACITY 4

// Helper function declarations
static void indent_print(int level);
static void free_node_data(ASTNode* node);
static bool ensure_child_capacity(ASTNode* node);

ASTNode* ast_create_node(NodeType type) {
    debug_ast_node_create(type, "creating base node");
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!node) return NULL;

    node->type = type;
    node->child_count = 0;
    node->children = (ASTNode**)malloc(INITIAL_CHILDREN_CAPACITY * sizeof(ASTNode*));
    
    if (!node->children) {
        free(node);
        return NULL;
    }

    // Initialize all fields to prevent undefined behavior
    memset(&node->data, 0, sizeof(node->data));
    memset(&node->loc, 0, sizeof(node->loc));
    

    debug_ast_node_complete(node, "node creation complete");
    return node;
}

void ast_destroy_node(ASTNode* node) {
    if (!node) return;

    debug_ast_node_destroy(node, "beginning node destruction");

    // Free all children first
    for (int i = 0; i < node->child_count; i++) {
        ast_destroy_node(node->children[i]);
        node->children[i] = NULL;
    }

    free(node->children);
    node->children = NULL;
    node->child_count = 0;
    // Free node-specific data
    free_node_data(node);

    free(node);
}

static void free_node_data(ASTNode* node) {
    switch (node->type) {
        case NODE_FUNCTION:
            free(node->data.function.name);
            free(node->data.function.return_type);
            break;

        case NODE_VARIABLE:
        case NODE_VAR_DECL:
        case NODE_ARRAY_DECL:
            free(node->data.variable.name);
            free(node->data.variable.type);
            if (node->data.variable.is_array) {
                symtable_destroy_bounds(node->data.variable.array_info.bounds);
            }
            break;

        case NODE_IDENTIFIER:
        case NODE_NUMBER:
        case NODE_BOOL:
        case NODE_TYPE:
        case NODE_STRING:
            free(node->data.value);
            break;

        default:
            // Other node types might not need special cleanup
            break;
    }
}

void ast_add_child(ASTNode* parent, ASTNode* child) {
    if (!parent || !child) return;


    debug_ast_add_child(parent, child, "adding child node");

    if (!ensure_child_capacity(parent)) {
        debug_ast_node_destroy(child, "failed to add child - destroying");
        ast_destroy_node(child);
        return;
    }

    parent->children[parent->child_count++] = child;
    debug_ast_node_complete(parent, "child addition complete");
}

static bool ensure_child_capacity(ASTNode* node) {
    if (node->child_count < INITIAL_CHILDREN_CAPACITY) return true;

    size_t new_capacity = node->child_count * 2;
    ASTNode** new_children = (ASTNode**)realloc(node->children, 
                                               new_capacity * sizeof(ASTNode*));
    
    if (!new_children) {
        error_report(ERROR_INTERNAL, SEVERITY_ERROR, node->loc,
                    "Failed to allocate memory for AST node children");
        return false;
    }

    node->children = new_children;
    return true;
}

ASTNode* ast_create_string(const char* value) {
    ASTNode* node = ast_create_node(NODE_STRING);
    if (!node) return NULL;

    if (value) {
        node->data.value = strdup(value);
        if (!node->data.value) {
            ast_destroy_node(node);
            return NULL;
        }
    }

    return node;
}

ASTNode* ast_create_function(const char* name, const char* return_type, bool is_procedure) {
    ASTNode* node = ast_create_node(is_procedure ? NODE_PROCEDURE : NODE_FUNCTION);
    if (!node) return NULL;

    node->data.function.name = strdup(name);
    node->data.function.return_type = return_type ? strdup(return_type) : NULL;
    node->data.function.is_procedure = is_procedure;
    node->data.function.type_before_name = false; // Default to type after name
    
    if (!node->data.function.name || (return_type && !node->data.function.return_type)) {
        ast_destroy_node(node);
        return NULL;
    }

    return node;
}

ASTNode* ast_create_variable(const char* name, const char* type, NodeType node_type) {
    verbose_print("Creating variable node: name=%s, type=%s, node_type=%d\n",
           name ? name : "null",
           type ? type : "null",
           node_type);

    // Validate the node type is a variable-related type
    if (node_type != NODE_VARIABLE && node_type != NODE_VAR_DECL && 
        node_type != NODE_ARRAY_DECL) {
        verbose_print("Invalid node type for variable creation: %d\n", node_type);
        return NULL;
    }

    if (!name) {
        verbose_print("Null name passed to ast_create_variable\n");
        return NULL;
    }

    ASTNode* node = ast_create_node(node_type);
    if (!node) {
        verbose_print("Failed to create base node in ast_create_variable\n");
        return NULL;
    }

    node->data.variable.name = strdup(name);
    node->data.variable.type = type ? strdup(type) : NULL;
    node->data.variable.is_array = (node_type == NODE_ARRAY_DECL);
    node->data.variable.array_info.dimensions = 0;
    node->data.variable.is_param = false;
    
    if (!node->data.variable.name) {
        verbose_print("Failed to duplicate variable name\n");
        ast_destroy_node(node);
        return NULL;
    }

    verbose_print("Successfully created variable node\n");
    return node;
}

ASTNode* ast_create_binary_op(TokenType op, ASTNode* left, ASTNode* right) {
    ASTNode* node = ast_create_node(NODE_BINARY_OP);
    if (!node) return NULL;

    node->data.binary_op.op = op;
    ast_add_child(node, left);
    ast_add_child(node, right);

    return node;
}

void ast_set_location(ASTNode* node, SourceLocation loc) {
    if (!node) return;
    node->loc = loc;
}

// Debug printing functions
/*void ast_print(ASTNode* node, int indent) {
    if (!node) return;

    indent_print(indent);
    printf("%s\n", ast_node_type_to_string(node));

    for (int i = 0; i < node->child_count; i++) {
        ast_print(node->children[i], indent + 2);
    }
}

static void indent_print(int level) {
    for (int i = 0; i < level; i++) {
        printf(" ");
    }
}*/

const char* ast_node_type_to_string(ASTNode* node) {
    static char buffer[256];
    
    switch (node->type) {
        case NODE_PROGRAM:
            return "Program";
        case NODE_FUNCTION:
            snprintf(buffer, sizeof(buffer), "Function: %s -> %s", 
                    node->data.function.name, 
                    node->data.function.return_type ? node->data.function.return_type : "void");
            return buffer;
        case NODE_PROCEDURE:
            snprintf(buffer, sizeof(buffer), "Procedure: %s", node->data.function.name);
            return buffer;
         case NODE_PARAMETER:
            if (node->data.parameter.name) {
                snprintf(buffer, sizeof(buffer), "Parameter: %s: %s (%s)", 
                        node->data.parameter.name,
                        node->data.parameter.type ? node->data.parameter.type : "<type pending>",
                        node->data.parameter.mode == PARAM_MODE_IN ? "in" :
                        node->data.parameter.mode == PARAM_MODE_OUT ? "out" : "inout");
                return buffer;
            }
            return "Parameter";
        case NODE_PARAMETER_LIST:
            return "Parameter List";
        case NODE_VARIABLE:
            snprintf(buffer, sizeof(buffer), "Variable: %s : %s", 
                    node->data.variable.name, node->data.variable.type);
            return buffer;
        case NODE_VAR_DECL:
            snprintf(buffer, sizeof(buffer), "Variable: %s : %s", 
                    node->data.variable.name, node->data.variable.type);
            return buffer;
        case NODE_ARRAY_DECL:
            snprintf(buffer, sizeof(buffer), "Array: %s[%d] : %s", 
                    node->data.variable.name, 
                    node->data.variable.array_info.dimensions, 
                    node->data.variable.type);
            return buffer;
        case NODE_BINARY_OP:
            return "Binary Operation";
        case NODE_UNARY_OP:
            return "Unary Operation";
        case NODE_NUMBER:
            snprintf(buffer, sizeof(buffer), "Number: %s", node->data.value);
            return buffer;
        case NODE_IDENTIFIER:
            snprintf(buffer, sizeof(buffer), "Identifier: %s", node->data.value);
            return buffer;
        case NODE_ASSIGNMENT:
            return "Assignment Statement";
        case NODE_BOOL:
            snprintf(buffer, sizeof(buffer), "Bool: %s", node->data.value);
            return buffer;
        case NODE_ARRAY_ACCESS:
            return "Array Access";
        case NODE_IF:
            return "If Statement";
        case NODE_WHILE:
            return "While Loop";
        case NODE_FOR:
            return "For Loop";
        case NODE_REPEAT:
            return "Repeat Until Loop";
        case NODE_RETURN:
            return "Return";
        case NODE_STRING:
            snprintf(buffer, sizeof(buffer), "String: \"%s\"", 
                    node->data.value ? node->data.value : "");
            return buffer;
        case NODE_PRINT:
            return "Print";
        case NODE_READ:
            return "Read";
        case NODE_TYPE:
            return "Type Specifier";
        case NODE_BLOCK:
            return "Block";
        case NODE_CALL:
            if (node->data.value) {
                static char buffer[256];
                snprintf(buffer, sizeof(buffer), "Call: %s", node->data.value);
                return buffer;
            }
            return "Call";
        default:
            snprintf(buffer, sizeof(buffer),"Unknown Node Type %d", node->type);
            return buffer;
    }
}

// Helper function to quickly check if a node is of a certain type
bool ast_is_node_type(const ASTNode* node, NodeType type) {
    return node && node->type == type;
}

// Get the nth child of a node (with bounds checking)
ASTNode* ast_get_child(const ASTNode* node, int n) {
    if (!node || n < 0 || n >= node->child_count) return NULL;
    return node->children[n];
}

char* ast_to_string(const ASTNode* node) {
    if (!node) return NULL;

    char* result = NULL;
    size_t size = 256;  // Initial buffer size
    result = (char*)malloc(size);
    if (!result) return NULL;
    result[0] = '\0';

    switch (node->type) {
        case NODE_NUMBER:
            strncpy(result, node->data.value, size - 1);
            break;

        case NODE_IDENTIFIER:
            strncpy(result, node->data.value, size - 1);
            break;

        case NODE_BINARY_OP: {
            char* left = ast_to_string(node->children[0]);
            char* right = ast_to_string(node->children[1]);
            if (left && right) {
                const char* op = "";
                switch (node->data.binary_op.op) {
                    case TOK_PLUS: op = "+"; break;
                    case TOK_MINUS: op = "-"; break;
                    case TOK_MULTIPLY: op = "*"; break;
                    case TOK_DIVIDE: op = "/"; break;
                    case TOK_BITAND: op = "&"; break;
                    case TOK_BITOR: op = "|"; break;
                    case TOK_BITXOR: op = "^"; break;
                    case TOK_LSHIFT: op = "<<"; break;
                    case TOK_RSHIFT: op = ">>"; break;
                    case TOK_AND: op = "&&"; break;
                    case TOK_OR: op = "||"; break;
                    case TOK_EQ: op = "=="; break;
                    case TOK_NE: op = "!="; break;
                    case TOK_GT: op = ">"; break;
                    case TOK_GE: op = ">="; break;
                    case TOK_LT: op = "<"; break;
                    case TOK_LE: op = "<="; break;
                    default: op = "?"; break;
                }
                snprintf(result, size, "(%s %s %s)", left, op, right);
            }
            free(left);
            free(right);
            break;
        }

        case NODE_UNARY_OP: {
            char* operand = ast_to_string(node->children[0]);
            if (operand) {
                const char* op = "";
                switch (node->data.unary_op.op) {
                    case TOK_MINUS: op = "-"; break;
                    case TOK_NOT: op = "!"; break;
                    case TOK_BITNOT: op = "~"; break;
                    default: op = "?"; break;
                }
                snprintf(result, size, "%s%s", op, operand);
            }
            free(operand);
            break;
        }

        case NODE_ARRAY_ACCESS: {
            if (node->children[0] && node->children[1]) {
                char* array = ast_to_string(node->children[0]);
                char* index = ast_to_string(node->children[1]);
                if (array && index) {
                    snprintf(result, size, "%s[%s]", array, index);
                }
                free(array);
                free(index);
            }
            break;
        }

        case NODE_CALL: {
            strncpy(result, node->data.value, size - 1);
            strncat(result, "(", size - strlen(result) - 1);
            for (int i = 0; i < node->child_count; i++) {
                if (i > 0) {
                    strncat(result, ", ", size - strlen(result) - 1);
                }
                char* arg = ast_to_string(node->children[i]);
                if (arg) {
                    strncat(result, arg, size - strlen(result) - 1);
                    free(arg);
                }
            }
            strncat(result, ")", size - strlen(result) - 1);
            break;
        }

        default:
            strncpy(result, "<unknown>", size - 1);
            break;
    }

    result[size - 1] = '\0';  // Ensure null termination
    return result;
}