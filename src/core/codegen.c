#include "codegen.h"
#include "errors.h"
#include "config.h"
#include "debug.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INDENT 128

static void generate_call(CodeGenerator* gen, ASTNode* node);

static void debug_print_token_type(TokenType type) {
    switch (type) {
        case TOK_PLUS: verbose_print("PLUS"); break;
        case TOK_MINUS: verbose_print("MINUS"); break;
        case TOK_MULTIPLY: verbose_print("MULTIPLY"); break;
        case TOK_DIVIDE: verbose_print("DIVIDE"); break;
        case TOK_RSHIFT: verbose_print("RSHIFT"); break;
        case TOK_LSHIFT: verbose_print("LSHIFT"); break;
        case TOK_BITAND: verbose_print("BITAND"); break;
        case TOK_BITOR: verbose_print("BITOR"); break;
        case TOK_BITXOR: verbose_print("BITXOR"); break;
        case TOK_BITNOT: verbose_print("BITNOT"); break;
        default: verbose_print("TOKEN_%d", type);
    }
}

static void debug_print_node_type(NodeType type) {
    switch (type) {
        case NODE_BINARY_OP: verbose_print("BINARY_OP"); break;
        case NODE_UNARY_OP: verbose_print("UNARY_OP"); break;
        case NODE_NUMBER: verbose_print("NUMBER"); break;
        case NODE_IDENTIFIER: verbose_print("IDENTIFIER"); break;
        default: verbose_print("NODE_%d", type);
    }
}

static void write_indent(CodeGenerator* gen) {
    for (int i = 0; i < gen->indent_level; i++) {
        fprintf(gen->output, "    ");
    }
}

CodeGenerator* codegen_create(FILE* output, SymbolTable* symbols) {
    CodeGenerator* gen = (CodeGenerator*)malloc(sizeof(CodeGenerator));
    if (!gen) return NULL;

    gen->output = output;
    gen->symbols = symbols;
    gen->current_function = NULL;
    gen->indent_level = 0;
    gen->needs_return = false;
    gen->in_expression = false;
    gen->array_context.array_adjustment_needed = false;
    gen->array_context.in_array_access = false;
    gen->array_context.current_dim = 0;

    return gen;
}

void codegen_destroy(CodeGenerator* gen) {
    if (!gen) return;
    free(gen->current_function);
    free(gen);
}

static const char* get_format_specifier(const char* type) {
    if (!type) return "%s";
    
    if (strcmp(type, "integer") == 0) return "%d";
    if (strcmp(type, "logical") == 0) return "%d";
    if (strcmp(type, "real") == 0) return "%f";
    if (strcmp(type, "character") == 0) return "%c";
    if (strstr(type, "array of character") == 0) return "%s";
    
    return "%s"; // default
}

static void generate_print_statement(CodeGenerator* gen, ASTNode* node) {
    if (!node || node->child_count == 0) {
        error_report(ERROR_INTERNAL, SEVERITY_ERROR, 
                    (SourceLocation){0, 0, "internal"},
                    "Invalid print statement node");
        return;
    }

    write_indent(gen);
    fprintf(gen->output, "printf(");

    ASTNode* arg = node->children[0];
    if (arg->type == NODE_STRING) {
        // Escape the string literal for C
        fprintf(gen->output, "\"%s\\n\"", arg->data.value);
    } else {
        // Get the type of the expression
        const char* type = "integer"; // Default to integer
        if (arg->type == NODE_VARIABLE) {
            Symbol* sym = symtable_lookup(gen->symbols, arg->data.variable.name);
            if (sym) {
                type = sym->info.var.type;
            }
        }

        // Output appropriate format specifier
        fprintf(gen->output, "\"");
        fprintf(gen->output, "%s", get_format_specifier(type));
        fprintf(gen->output, "\\n\", ");
        
        // Generate the expression - fix for array access
        if (arg->type == NODE_ARRAY_ACCESS) {
            ASTNode* array = arg->children[0];
            fprintf(gen->output, "%s", array->data.variable.name);
            fprintf(gen->output, "[");
            codegen_generate(gen, arg->children[1]);  // Generate index expression
            fprintf(gen->output, "]");
        } else {
            codegen_generate(gen, arg);
        }
    }

    fprintf(gen->output, ");\n");
}

static void generate_read_statement(CodeGenerator* gen, ASTNode* node) {
    write_indent(gen);
    fprintf(gen->output, "scanf(");

    ASTNode* var = node->children[0];
    Symbol* sym = symtable_lookup(gen->symbols, var->data.variable.name);
    if (!sym) {
        error_report(ERROR_SEMANTIC, SEVERITY_ERROR, var->loc,
                    "Undefined variable in read statement: %s",
                    var->data.variable.name);
        return;
    }

    const char* format = get_format_specifier(sym->info.var.type);
    fprintf(gen->output, "\"%s\", &", format);
    codegen_generate(gen, var);
    fprintf(gen->output, ");\n");
}

static void generate_type(CodeGenerator* gen, const char* type) {
    verbose_print("Generating type: %s\n", type ? type : "null");
    if (!type) {
        fprintf(gen->output, "void");
        return;
    }

    // Count number of array dimensions by counting "array of" occurrences
    const char* array_prefix = "array of ";
    const size_t prefix_len = strlen(array_prefix);
    const char* type_ptr = type;
    int array_dimensions = 0;
    
    // Find base type by skipping through all "array of" prefixes
    while (strncmp(type_ptr, array_prefix, prefix_len) == 0) {
        array_dimensions++;
        type_ptr += prefix_len;
    }
    
    // Now type_ptr points to the base type
    // Generate the base type
    if (strcmp(type_ptr, "integer") == 0) {
        fprintf(gen->output, "int");
    } else if (strcmp(type_ptr, "real") == 0) {
        fprintf(gen->output, "float");
    } else if (strcmp(type_ptr, "logical") == 0) {
        fprintf(gen->output, "bool");
    } else if (strcmp(type_ptr, "character") == 0) {
        fprintf(gen->output, "char");
    } else {
        RecordTypeData* type_sym = symtable_lookup_type(gen->symbols, type_ptr);
        if (type_sym) {
            // For typedef'd types, just use the name
            if (type_sym->is_typedef) {
                fprintf(gen->output, "%s", type_ptr);
            } else {
                // For non-typedef'd records, need to use struct prefix
                fprintf(gen->output, "struct %s", type_ptr);
            }
        } else {
            // If not found, just output the type name (error would have been caught during parsing)
            fprintf(gen->output, "%s", type_ptr);
        }
    }

    // For parameter declarations, we'll add a space before the parameter name
    // The array brackets will be added later in generate_function_declaration
    if (array_dimensions > 0 && !gen->array_context.in_array_declaration) {
        fprintf(gen->output, " ");
    }

    // Store array dimensions for use in function declaration
    gen->array_context.dimensions = array_dimensions;
}


static void generate_function_declaration(CodeGenerator* gen, ASTNode* node) {
    verbose_print("Generating function declaration for: %s\n", node->data.function.name);
    
    // Store function name for implicit return
    free(gen->current_function);
    gen->current_function = strdup(node->data.function.name);
    gen->needs_return = true;
    
    // Generate return type
    if (node->type == NODE_PROCEDURE) {
        fprintf(gen->output, "void");
    } else {
        if (node->data.function.return_type) {
            generate_type(gen, node->data.function.return_type);
            if (node->data.function.is_pointer) {
                for (int i = 0; i < node->data.function.pointer_level; ++i) {
                    fprintf(gen->output, "*");
                }
            }
        } else {
            fprintf(gen->output, "void");
        }
    }
    
    // Function name
    fprintf(gen->output, " %s(", node->data.function.name);
    
    // Parameters
    if (node->data.function.params) {
        bool first = true;
        for (int i = 0; i < node->data.function.params->child_count; i++) {
            ASTNode* param = node->data.function.params->children[i];
            if (!first) fprintf(gen->output, ", ");
            first = false;
            
            // Look up parameter symbol for bounds information
            Symbol* sym = symtable_lookup_parameter(gen->symbols, 
                                                  node->data.function.name, 
                                                  param->data.parameter.name);
            verbose_print("Looking up parameter %s in function %s: %s\n", 
                         param->data.parameter.name,
                         node->data.function.name,
                         sym ? "found" : "not found");
            bool has_bounds = sym && sym->info.var.is_array && sym->info.var.bounds;
            if (sym && sym->info.var.is_array)
                verbose_print("sym has bounds? %d\n", sym->info.var.bounds);
            generate_type(gen, param->data.parameter.type);
            if (sym->info.var.needs_deref && (param->data.parameter.mode == PARAM_MODE_OUT ||
                param->data.parameter.mode == PARAM_MODE_INOUT)) {
                fprintf(gen->output, "*");
            }
            if (param->data.parameter.is_pointer) {
                for (int i = 0; i < param->data.parameter.pointer_level; ++i) {
                    fprintf(gen->output, "*");
                }
            }
            fprintf(gen->output, " %s", param->data.parameter.name);
            
            // Add array brackets based on bounds information
            if (has_bounds) {
                // For remaining dimensions, use the bounds if available
                for (int dim = 0; dim < sym->info.var.dimensions; dim++) {
                    fprintf(gen->output, "[");
                    DimensionBounds* bound = &sym->info.var.bounds->bounds[dim];
                    if (bound->using_range) {
                        // Calculate size from range
                        if (!bound->start.is_constant && !bound->end.is_constant) {
                            // For variable bounds, calculate size
                            fprintf(gen->output, "%s - %s",
                                bound->end.variable_name ? bound->end.variable_name : "0",
                                bound->start.variable_name ? bound->start.variable_name : "0");
                            if ((g_config.array_indexing == ARRAY_ONE_BASED))
                                fprintf(gen->output, " + 1");
                        } else if (bound->start.is_constant && !bound->end.is_constant) {
                            fprintf(gen->output, "%s - %ld",
                                bound->end.variable_name ? bound->end.variable_name : "0",
                                bound->start.constant_value);
                            if ((g_config.array_indexing == ARRAY_ONE_BASED))
                                fprintf(gen->output, " + 1");
                        } else if (!bound->start.is_constant && bound->end.is_constant) {
                            fprintf(gen->output, "%ld - %s",
                                bound->end.constant_value,
                                bound->start.variable_name ? bound->start.variable_name : "0");
                            if ((g_config.array_indexing == ARRAY_ONE_BASED))
                                fprintf(gen->output, " + 1");
                        } else {
                            // For constant bounds, pre-calculate size
                            fprintf(gen->output, "%ld", 
                                bound->end.constant_value - bound->start.constant_value + (g_config.array_indexing == ARRAY_ONE_BASED ? 1 : 0));
                        }
                    } else {
                        // Single size expression
                        if (bound->start.is_constant) {
                            fprintf(gen->output, "%ld", bound->start.constant_value);
                        } else {
                            fprintf(gen->output, "%s", bound->start.variable_name);
                        }
                    }
                    //if (g_config.array_indexing == ARRAY_ONE_BASED)
                    //    fprintf(gen->output, " + 1");
                    fprintf(gen->output, "]");
                }
            } else {
                // If no bounds info but it's an array, add empty brackets
                for (int dim = 0; dim < gen->array_context.dimensions; dim++) {
                    fprintf(gen->output, "[]");
                }
            }
            
            // Reset array dimensions counter
            gen->array_context.dimensions = 0;
        }
    }
    
    fprintf(gen->output, ") {\n");
    gen->indent_level++;

    // Add implicit declaration of function-named variable if it has a return type and not explicitly declared
    if (node->data.function.return_type) {
        write_indent(gen);
        gen->array_context.in_array_declaration = true;
        generate_type(gen, node->data.function.return_type);
        if (node->data.function.is_pointer) {
            for (int i = 0; i < node->data.function.pointer_level; ++i) {
                fprintf(gen->output, "*");
            }
        }
        fprintf(gen->output, " %s;\n", node->data.function.name);
        gen->array_context.in_array_declaration = false;
    }

    // Generate offset variables for range-based parameter arrays
    if (node->data.function.params) {
        for (int i = 0; i < node->data.function.params->child_count; i++) {
            ASTNode* param = node->data.function.params->children[i];
            Symbol* sym = symtable_lookup_parameter(gen->symbols,
                                                  node->data.function.name,
                                                  param->data.parameter.name);
            if (sym && sym->info.var.is_array && sym->info.var.bounds) {
                for (int dim = 0; dim < sym->info.var.dimensions; dim++) {
                    DimensionBounds* bound = &sym->info.var.bounds->bounds[dim];
                    if (bound->using_range) {
                        write_indent(gen);
                        fprintf(gen->output, "const int %s_offset_%d = ", 
                                param->data.parameter.name, dim);
                        
                        // Output the start bound as offset
                        if (bound->start.is_constant) {
                            fprintf(gen->output, "%ld", bound->start.constant_value);
                        } else {
                            fprintf(gen->output, "%s", bound->start.variable_name);
                        }
                        if (g_config.array_indexing == ARRAY_ONE_BASED)
                            fprintf(gen->output, " - 1");   
                        fprintf(gen->output, ";\n");
                    }
                }
            }
        }
    }

    // Generate function body
    codegen_generate(gen, node->data.function.body);

    // Add implicit return if needed
    if (gen->needs_return && node->data.function.return_type) {
        write_indent(gen);
        fprintf(gen->output, "return %s;\n", node->data.function.name);
    }

    gen->indent_level--;
    fprintf(gen->output, "}\n");

    // Clean up
    free(gen->current_function);
    gen->current_function = NULL;
    gen->needs_return = false;
}


static void generate_record_type(CodeGenerator* gen, ASTNode* node) {
    RecordTypeData* record = &node->record_type;
    
    if (record->is_typedef && !record->is_nested) {
        fprintf(gen->output, "typedef ");
    }
    
    fprintf(gen->output, "struct %s {\n", record->name);
    
    gen->indent_level++;

    // Generate fields
    for (int i = 0; i < node->child_count; i++) {
        ASTNode* field = node->children[i];
        write_indent(gen);
        
        if (field->type == NODE_RECORD_FIELD) {
            if (field->children[0] && field->children[0]->type == NODE_RECORD_TYPE) {
                // Mark as nested record before generating
                field->children[0]->record_type.is_nested = true;
                // Nested record
                generate_record_type(gen, field->children[0]);
                fprintf(gen->output, " %s", field->data.variable.name);
            } else {
                // Regular field
                generate_type(gen, field->data.variable.type);
                for (int i = 0; i < field->data.variable.pointer_level; ++i) {
                    fprintf(gen->output, "*");
                }
                fprintf(gen->output, " %s", field->data.variable.name);
                
                // Handle array fields
                if (field->data.variable.is_array && field->data.variable.array_info.bounds) {
                    for (int j = 0; j < field->data.variable.array_info.dimensions; j++) {
                        DimensionBounds* bound = &field->data.variable.array_info.bounds->bounds[j];
                        fprintf(gen->output, "[");
                        if (bound->using_range) {
                            if (bound->end.is_constant && bound->start.is_constant) {
                                fprintf(gen->output, "%ld", 
                                    bound->end.constant_value - bound->start.constant_value + 
                                    (g_config.array_indexing == ARRAY_ONE_BASED ? 1 : 0));
                            } else {
                                // Handle variable bounds
                                if (bound->end.is_constant) {
                                    fprintf(gen->output, "%ld - %s", 
                                        bound->end.constant_value,
                                        bound->start.variable_name);
                                } else if (bound->start.is_constant) {
                                    fprintf(gen->output, "%s - %ld",
                                        bound->end.variable_name,
                                        bound->start.constant_value);
                                } else {
                                    fprintf(gen->output, "%s - %s",
                                        bound->end.variable_name,
                                        bound->start.variable_name);
                                }
                                if (g_config.array_indexing == ARRAY_ONE_BASED)
                                    fprintf(gen->output, " + 1");
                            }
                        } else {
                            if (bound->start.is_constant) {
                                fprintf(gen->output, "%ld", bound->start.constant_value);
                            } else {
                                fprintf(gen->output, "%s", bound->start.variable_name);
                            }
                        }
                        fprintf(gen->output, "]");
                    }
                }
            }
        }
        fprintf(gen->output, ";\n");
    }

    gen->indent_level--;
    write_indent(gen);
    
    // Close the struct definition
    if (record->is_typedef && !record->is_nested) {
        // For top-level typedef, add the type name
        fprintf(gen->output, "} %s;\n", record->name);
    } else {
        // For nested records or var declarations, just close the struct
        fprintf(gen->output, "}");
        // Don't add semicolon for nested records - it will be added by the parent
        if (!record->is_nested) {
            //fprintf(gen->output, ";\n");
        }
    }
}

static void generate_field_access(CodeGenerator* gen, ASTNode* node) {
    codegen_generate(gen, node->children[0]); // Generate record expression
    
    // Check if arrow (->) or dot (.) should be used
    Symbol* sym = NULL;
    if (node->children[0]->type == NODE_IDENTIFIER) {
        sym = symtable_lookup(gen->symbols, node->children[0]->data.value);
    }
    
    fprintf(gen->output, "%s", node->data.value);
}


static void generate_variable_declaration(CodeGenerator* gen, ASTNode* node) {
    if (!node) return;

    verbose_print("Generating variable declaration for %s\n", node->data.variable.name);
    write_indent(gen);

    if (node->child_count && node->children[0] && 
        node->children[0]->type == NODE_RECORD_TYPE) {

        // First generate the record type definition
        generate_record_type(gen, node->children[0]);
        
        // Then generate the variable declaration using the struct type
        write_indent(gen);
        fprintf(gen->output, " %s",
                node->data.variable.name);

        ArrayBoundsData* bounds = node->data.variable.array_info.bounds;
        if (bounds) {
            for (int dim = 0; dim < bounds->dimensions; dim++) {
                fprintf(gen->output, "[");
                
                if (bounds->bounds[dim].using_range) {
                    // Calculate size from range (end - start + 1)
                    fprintf(gen->output, "(");
                    // End bound
                    if (bounds->bounds[dim].end.is_constant) {
                        fprintf(gen->output, "%ld", bounds->bounds[dim].end.constant_value);
                    } else {
                        fprintf(gen->output, "(%s)", bounds->bounds[dim].end.variable_name);
                    }
                    fprintf(gen->output, " - ");
                    // Start bound
                    if (bounds->bounds[dim].start.is_constant) {
                        fprintf(gen->output, "%ld", bounds->bounds[dim].start.constant_value);
                    } else {
                        fprintf(gen->output, "(%s)", bounds->bounds[dim].start.variable_name);
                    }
                    if (g_config.array_indexing == ARRAY_ONE_BASED)
                        fprintf(gen->output, " + 1");
                    fprintf(gen->output, ")");
                } else {
                    // Single size expression
                    if (bounds->bounds[dim].start.is_constant) {
                        fprintf(gen->output, "%ld", 
                            bounds->bounds[dim].start.constant_value + 
                            (g_config.array_indexing == ARRAY_ONE_BASED ? 1 : 0));
                    } else {
                        //if (g_config.array_indexing == ARRAY_ONE_BASED) {
                        //    fprintf(gen->output, "(%s + 1)", bounds->bounds[dim].start.variable_name);
                        //} else {
                            fprintf(gen->output, "%s", bounds->bounds[dim].start.variable_name);
                        //}
                    }
                }
                fprintf(gen->output, "]");
            }
            fprintf(gen->output, ";\n");
        } else if (node->data.variable.array_info.dimensions) {
            for (int i = 0; i < node->data.variable.array_info.dimensions; ++i) {
                fprintf(gen->output, "[]");
            }
            fprintf(gen->output, ";\n");
        } else
            fprintf(gen->output, ";\n");

        //fprintf(gen->output, ";\n");

        // Generate offset variables for range-based arrays in 1-based indexing
        if (g_config.array_indexing == ARRAY_ONE_BASED && bounds) {
            for (int dim = 0; dim < bounds->dimensions; dim++) {
                if (bounds->bounds[dim].using_range) {
                    write_indent(gen);
                    fprintf(gen->output, "const int %s_offset_%d = ", 
                            node->data.variable.name, dim);
                    
                    // Output start bound as offset
                    if (bounds->bounds[dim].start.is_constant) {
                        fprintf(gen->output, "%ld", bounds->bounds[dim].start.constant_value);
                    } else {
                        fprintf(gen->output, "%s", bounds->bounds[dim].start.variable_name);
                    }
                    if (g_config.array_indexing == ARRAY_ONE_BASED)
                        fprintf(gen->output, " - 1");   
                    //fprintf(gen->output, ";\n");
                }
            }
        }
        return;
    }


    // Get base type
    const char* type = node->data.variable.type;
    const char* base_type = type;
    while (strncmp(base_type, "array of ", 9) == 0) {
        base_type = base_type + 9;  // Skip "array of " prefix
    }

    // Convert base type to C type
    if (strcmp(base_type, "integer") == 0) {
        fprintf(gen->output, "int");
    } else if (strcmp(base_type, "real") == 0) {
        fprintf(gen->output, "float");
    } else if (strcmp(base_type, "logical") == 0) {
        fprintf(gen->output, "bool");
    } else if (strcmp(base_type, "character") == 0) {
        fprintf(gen->output, "char");
    } else {
        fprintf(gen->output, "%s", base_type);
    }

    if (node->data.variable.is_pointer) {
        for (int i = 0; i < node->data.variable.pointer_level; ++i) {
            fprintf(gen->output, "*");
        }
    }
    fprintf(gen->output, " %s", node->data.variable.name);

    // Handle array dimensions
    if (node->data.variable.is_array) {
        ArrayBoundsData* bounds = node->data.variable.array_info.bounds;
        if (bounds) {
            // Generate size expressions for each dimension
            for (int dim = 0; dim < bounds->dimensions; dim++) {
                fprintf(gen->output, "[");
                
                // Calculate size based on bounds
                if (bounds->bounds[dim].using_range) {
                    fprintf(gen->output, "(");
                    // End bound
                    if (bounds->bounds[dim].end.is_constant) {
                        fprintf(gen->output, "%ld", bounds->bounds[dim].end.constant_value);
                    } else {
                        fprintf(gen->output, "%s", bounds->bounds[dim].end.variable_name);
                    }
                    fprintf(gen->output, " - ");
                    // Start bound
                    if (bounds->bounds[dim].start.is_constant) {
                        fprintf(gen->output, "%ld", bounds->bounds[dim].start.constant_value);
                    } else {
                        fprintf(gen->output, "%s", bounds->bounds[dim].start.variable_name);
                    }
                    if (g_config.array_indexing == ARRAY_ONE_BASED)
                        fprintf(gen->output, " + 1");
                    fprintf(gen->output, ")");
                } else {
                    // Single size value
                    if (bounds->bounds[dim].start.is_constant) {
                        fprintf(gen->output, "%ld", 
                            bounds->bounds[dim].start.constant_value +
                            (g_config.array_indexing == ARRAY_ONE_BASED ? 1 : 0));
                    } else {
                        //if (g_config.array_indexing == ARRAY_ONE_BASED) {
                        //    fprintf(gen->output, "(%s + 1)", bounds->bounds[dim].start.variable_name);
                        //} else {
                            fprintf(gen->output, "%s", bounds->bounds[dim].start.variable_name);
                        //}
                    }
                }
                fprintf(gen->output, "]");
            }
        }
    }

    fprintf(gen->output, ";\n");

    // Generate offset variables for each dimension using ranges
    if (node->data.variable.is_array) {
        ArrayBoundsData* bounds = node->data.variable.array_info.bounds;
        if (bounds) {
            for (int dim = 0; dim < bounds->dimensions; dim++) {
                if (bounds->bounds[dim].using_range) {
                    write_indent(gen);
                    fprintf(gen->output, "const int %s_offset_%d = ", 
                            node->data.variable.name, dim);
                    if (bounds->bounds[dim].start.is_constant) {
                        fprintf(gen->output, "%ld", bounds->bounds[dim].start.constant_value);
                    } else {
                        fprintf(gen->output, "%s", bounds->bounds[dim].start.variable_name);
                    }
                    if (g_config.array_indexing == ARRAY_ONE_BASED)
                        fprintf(gen->output, " - 1");   
                    fprintf(gen->output, ";\n");
                }
            }
        }
    }
}

// unused maybe remove
static void generate_array_declaration(CodeGenerator* gen, ASTNode* node) {
    verbose_print("Generating array declaration for %s\n", node->data.variable.name);
    write_indent(gen);
    
    gen->array_context.in_array_declaration = true;

    // Get base type from the full array type string
    const char* type = node->data.variable.type;
    const char* base_type = type;
    while (strncmp(base_type, "array of ", 9) == 0) {
        base_type = base_type + 9;  // Skip "array of " prefix
    }

    // Convert base type to C type
    if (strcmp(base_type, "integer") == 0) {
        fprintf(gen->output, "int");
    } else if (strcmp(base_type, "real") == 0) {
        fprintf(gen->output, "float");
    } else if (strcmp(base_type, "logical") == 0) {
        fprintf(gen->output, "bool");
    } else if (strcmp(base_type, "character") == 0) {
        fprintf(gen->output, "char");
    } else {
        fprintf(gen->output, "%s", base_type);
    }

    fprintf(gen->output, " %s", node->data.variable.name);

    // Handle array dimensions
    ArrayBoundsData* bounds = node->data.variable.array_info.bounds;
    if (bounds) {
        verbose_print("Processing array with %d dimensions\n", bounds->dimensions);
        
        for (int dim = 0; dim < bounds->dimensions; dim++) {
            fprintf(gen->output, "[");
            
            if (bounds->bounds[dim].using_range) {
                // Calculate size from range (end - start + 1)
                fprintf(gen->output, "(");
                // End bound
                if (bounds->bounds[dim].end.is_constant) {
                    fprintf(gen->output, "%ld", bounds->bounds[dim].end.constant_value);
                } else {
                    fprintf(gen->output, "(%s)", bounds->bounds[dim].end.variable_name);
                }
                fprintf(gen->output, " - ");
                // Start bound
                if (bounds->bounds[dim].start.is_constant) {
                    fprintf(gen->output, "%ld", bounds->bounds[dim].start.constant_value);
                } else {
                    fprintf(gen->output, "(%s)", bounds->bounds[dim].start.variable_name);
                }
                if (g_config.array_indexing == ARRAY_ONE_BASED)
                    fprintf(gen->output, " + 1");
                fprintf(gen->output, ")");
            } else {
                // Single size expression
                if (bounds->bounds[dim].start.is_constant) {
                    fprintf(gen->output, "%ld", 
                           bounds->bounds[dim].start.constant_value + 
                           (g_config.array_indexing == ARRAY_ONE_BASED ? 1 : 0));
                } else {
                    //if (g_config.array_indexing == ARRAY_ONE_BASED) {
                    //    fprintf(gen->output, "(%s + 1)", bounds->bounds[dim].start.variable_name);
                    //} else {
                        fprintf(gen->output, "%s", bounds->bounds[dim].start.variable_name);
                    //}
                }
            }
            fprintf(gen->output, "]");
        }
    }

    fprintf(gen->output, ";\n");

    // Generate offset variables for range-based arrays in 1-based indexing
    if (g_config.array_indexing == ARRAY_ONE_BASED && bounds) {
        for (int dim = 0; dim < bounds->dimensions; dim++) {
            if (bounds->bounds[dim].using_range) {
                write_indent(gen);
                fprintf(gen->output, "const int %s_offset_%d = ", 
                        node->data.variable.name, dim);
                
                // Output start bound as offset
                if (bounds->bounds[dim].start.is_constant) {
                    fprintf(gen->output, "%ld", bounds->bounds[dim].start.constant_value);
                } else {
                    fprintf(gen->output, "%s", bounds->bounds[dim].start.variable_name);
                }
                if (g_config.array_indexing == ARRAY_ONE_BASED)
                    fprintf(gen->output, " - 1");   
                fprintf(gen->output, ";\n");
            }
        }
    }

    // Generate bounds checking helper function if enabled
    if (g_config.enable_bounds_checking && bounds) {
        write_indent(gen);
        fprintf(gen->output, "static inline void check_%s_bounds(", node->data.variable.name);
        
        // Generate parameter list for each dimension
        for (int dim = 0; dim < bounds->dimensions; dim++) {
            if (dim > 0) fprintf(gen->output, ", ");
            fprintf(gen->output, "int idx%d", dim);
        }
        fprintf(gen->output, ") {\n");
        gen->indent_level++;

        // Generate bounds check for each dimension
        for (int dim = 0; dim < bounds->dimensions; dim++) {
            write_indent(gen);
            fprintf(gen->output, "if (idx%d < ", dim);
            
            if (bounds->bounds[dim].start.is_constant) {
                fprintf(gen->output, "%ld", bounds->bounds[dim].start.constant_value);
            } else {
                fprintf(gen->output, "%s", bounds->bounds[dim].start.variable_name);
            }
            
            fprintf(gen->output, " || idx%d > ", dim);
            
            if (bounds->bounds[dim].end.is_constant) {
                fprintf(gen->output, "%ld", bounds->bounds[dim].end.constant_value);
            } else {
                fprintf(gen->output, "%s", bounds->bounds[dim].end.variable_name);
            }
            
            fprintf(gen->output, ") {\n");
            gen->indent_level++;
            write_indent(gen);
            fprintf(gen->output, "fprintf(stderr, \"Array %s index out of bounds in dimension %d\\n\");\n",
                    node->data.variable.name, dim + 1);
            write_indent(gen);
            fprintf(gen->output, "exit(1);\n");
            gen->indent_level--;
            write_indent(gen);
            fprintf(gen->output, "}\n");
        }

        gen->indent_level--;
        write_indent(gen);
        fprintf(gen->output, "}\n\n");
    }

    gen->array_context.in_array_declaration = false;
}

static void generate_number_literal(CodeGenerator* gen, const char* number) {
    if (!number) return;
    
    // Handle trailing decimal point (e.g., "123." -> "123.0")
    char* decimal_point = strrchr(number, '.');
    if (decimal_point && *(decimal_point + 1) == '\0') {
        // If it ends with a decimal point, append "0"
        fprintf(gen->output, "%s0", number);
        return;
    }
    
    // Handle octal notation (e.g., "0o777" -> "0777")
    if (strlen(number) >= 2 && number[0] == '0' && (number[1] == 'o' || number[1] == 'O')) {
        // Print '0' followed by the rest of the number after 'o'
        fprintf(gen->output, "0%s", number + 2);
        return;
    }
    
    // All other numbers can be written as-is (hex, binary, regular decimals)
    fprintf(gen->output, "%s", number);
}

static void generate_bounds_check(CodeGenerator* gen, ASTNode* node, Symbol* sym) {
    if (!sym->info.var.bounds) return;

    write_indent(gen);
    fprintf(gen->output, "// Bounds checking\n");

    for (int dim = 0; dim < sym->info.var.dimensions; dim++) {
        DimensionBounds* bounds = &sym->info.var.bounds->bounds[dim];
        ASTNode* index = node->children[dim + 1];
        
        write_indent(gen);
        fprintf(gen->output, "if (");
        
        // Generate lower bound check
        if (bounds->start.is_constant) {
            codegen_generate(gen, index);
            fprintf(gen->output, " < %ld", bounds->start.constant_value);
        } else {
            codegen_generate(gen, index);
            fprintf(gen->output, " < %s", bounds->start.variable_name);
        }
        
        fprintf(gen->output, " || ");
        
        // Generate upper bound check
        codegen_generate(gen, index);
        fprintf(gen->output, " > ");
        
        if (bounds->end.is_constant) {
            fprintf(gen->output, "%ld", bounds->end.constant_value);
        } else {
            fprintf(gen->output, "%s", bounds->end.variable_name);
        }
        
        fprintf(gen->output, ") {\n");
        gen->indent_level++;
        write_indent(gen);
        fprintf(gen->output, "fprintf(stderr, \"Array index out of bounds in dimension %d\\n\");\n", dim + 1);
        write_indent(gen);
        fprintf(gen->output, "exit(1);\n");
        gen->indent_level--;
        write_indent(gen);
        fprintf(gen->output, "}\n");
    }
}


static void generate_array_access(CodeGenerator* gen, ASTNode* node) {
    verbose_print("\n=== STARTING ARRAY ACCESS GENERATION ===\n");
    
    debug_codegen_array(gen, node, "starting array access");
    if (!gen || !node || node->type != NODE_ARRAY_ACCESS) {
        verbose_print("Invalid input to generate_array_access\n");
        return;
    }

    // Get the array symbol to check for range-based bounds
    Symbol* sym = NULL;
    const char* array_name = NULL;
    if (node->children[0]->type == NODE_IDENTIFIER) {
        array_name = node->children[0]->data.value;
    } else if (node->children[0]->type == NODE_VARIABLE) {
        array_name = node->children[0]->data.variable.name;
    }
    
    if (array_name) {
        verbose_print("Looking up symbol for array: %s\n", array_name);
        verbose_print("\nDumping symbol table state before lookups:\n");
        symtable_debug_dump_all(gen->symbols);
        
        // Try current scope first (includes local variables)
        sym = symtable_lookup_current_scope(gen->symbols, array_name);
        verbose_print("Current scope lookup result: %s\n", sym ? "found" : "not found");
        
        if (!sym && gen->current_function) {
            // Look up in function's parameters and locals in global scope
            Symbol* func = symtable_lookup(gen->symbols, gen->current_function);
            if (func && (func->kind == SYMBOL_FUNCTION || func->kind == SYMBOL_PROCEDURE)) {
                // Check parameters
                for (int i = 0; i < func->info.func.param_count; i++) {
                    if (strcmp(func->info.func.parameters[i]->name, array_name) == 0) {
                        sym = func->info.func.parameters[i];
                        break;
                    }
                }
                // Check locals if not found in parameters
                if (!sym) {
                    for (int i = 0; i < func->info.func.local_var_count; i++) {
                        if (strcmp(func->info.func.local_variables[i]->name, array_name) == 0) {
                            sym = func->info.func.local_variables[i];
                            break;
                        }
                    }
                }
            }
        }

        debug_codegen_symbol_resolution(gen, 
            node->children[0]->type == NODE_IDENTIFIER ? 
            node->children[0]->data.value : 
            node->children[0]->data.variable.name, 
            sym, "array symbol lookup");
        
        if (sym) {
            verbose_print("Found symbol for array %s with %d dimensions\n", 
                   array_name, sym->info.var.dimensions);
            if (sym->info.var.bounds) {
                verbose_print("Array has bounds information:\n");
                for (int i = 0; i < sym->info.var.dimensions; i++) {
                    verbose_print("  Dimension %d: using_range=%d\n", i,
                           sym->info.var.bounds->bounds[i].using_range);
                }
            }
        }
    }
    // Generate base array access
    if (node->children[0]->type == NODE_ARRAY_ACCESS) {
        verbose_print("Generating nested array access\n");
        generate_array_access(gen, node->children[0]);
    } else {
        verbose_print("Generating base array access\n");
        codegen_generate(gen, node->children[0]);
    }

    // For each dimension (starting from child index 1)
    for (int i = 1; i < node->child_count; i++) {
        fprintf(gen->output, "[");
        verbose_print("Generating index expression %d\n", i-1);

        // Check if this dimension uses range-based indexing
        bool uses_range = false;
        if (sym && sym->info.var.bounds && (i-1) < sym->info.var.dimensions) {
            uses_range = sym->info.var.bounds->bounds[i-1].using_range;
        }

        // Generate index with bounds checking and appropriate adjustments
        if (uses_range) {
            // For range-based arrays, subtract the stored offset
            fprintf(gen->output, "(");
            codegen_generate(gen, node->children[i]);
            if (g_config.array_indexing == ARRAY_ONE_BASED) {
                fprintf(gen->output, " - 1");
            }
            fprintf(gen->output, " - %s_offset_%d)", array_name, i-1);
        } else if (g_config.array_indexing == ARRAY_ONE_BASED) {
            fprintf(gen->output, "(");
            codegen_generate(gen, node->children[i]);
            fprintf(gen->output, " - 1)");
        } else {
            codegen_generate(gen, node->children[i]);
        }

        fprintf(gen->output, "]");
    }

    verbose_print("=== FINISHED ARRAY ACCESS GENERATION ===\n");
}

static void generate_assignment(CodeGenerator* gen, ASTNode* node) {
    verbose_print("\n=== ENTERING GENERATE_ASSIGNMENT ===\n");
    
    if (!gen || !node) {
        verbose_print("ERROR: NULL generator or node\n");
        return;
    }

    if (!node->children || node->child_count < 2) {
        verbose_print("ERROR: Invalid assignment node structure %d\n", node->child_count);
        return;
    }

    write_indent(gen);

    // Debug info about assignment nodes
    verbose_print("Assignment LHS type: %d\n", node->children[0]->type);
    verbose_print("Assignment RHS type: %d\n", node->children[1]->type);

    // Left-hand side
    if (node->children[0]->type == NODE_UNARY_OP && 
        node->children[0]->data.unary_op.op == TOK_DEREF) {
        // For dereferenced pointers, we need to handle multiple levels of dereferencing
        ASTNode* operand = node->children[0]->children[0];
        for (int i = 0; i < node->children[0]->data.unary_op.deref_count; i++) {
            fprintf(gen->output, "*");
        }
        codegen_generate(gen, operand);
    } else if (node->children[0]->type == NODE_ARRAY_ACCESS) {
        verbose_print("LHS is array access\n");
        generate_array_access(gen, node->children[0]);
    } else if (node->children[0]->type == NODE_IDENTIFIER) {
        verbose_print("LHS is identifier: %s\n", node->children[0]->data.value);
        fprintf(gen->output, "%s", node->children[0]->data.value);
    } else if (node->children[0]->type == NODE_VARIABLE) {
        verbose_print("LHS is variable: %s\n", node->children[0]->data.variable.name);
        fprintf(gen->output, "%s", node->children[0]->data.variable.name);
    }

    fprintf(gen->output, " = ");

    // Right-hand side
    bool old_in_expr = gen->in_expression;
    gen->in_expression = true;

    if (node->children[1]->type == NODE_ARRAY_ACCESS) {
        verbose_print("RHS is array access\n");
        generate_array_access(gen, node->children[1]);
    } else if (node->children[1]->type == NODE_CALL) {
        verbose_print("RHS is function call\n");
        generate_call(gen, node->children[1]);
    } else {
        verbose_print("RHS is expression type: %d\n", node->children[1]->type);
        codegen_generate(gen, node->children[1]);
    }

    gen->in_expression = old_in_expr;

    if (!gen->in_expression) {
        fprintf(gen->output, ";\n");
    }
    
    verbose_print("=== EXITING GENERATE_ASSIGNMENT ===\n");
}

static void generate_if_statement(CodeGenerator* gen, ASTNode* node) {
    write_indent(gen);
    fprintf(gen->output, "if (");
    codegen_generate(gen, node->children[0]); // Generate condition
    fprintf(gen->output, ") {\n");
    
    gen->indent_level++;
    codegen_generate(gen, node->children[1]); // Generate "then" block
    gen->indent_level--;
    write_indent(gen);
    
    // Handle else or elseif branch
    if (node->child_count > 2 && node->children[2]) {
        ASTNode* else_node = node->children[2];
        
        if (else_node->type == NODE_IF) {
            // This is an elseif branch
            fprintf(gen->output, "} else if (");
            codegen_generate(gen, else_node->children[0]); // elseif condition
            fprintf(gen->output, ") {\n");
            
            gen->indent_level++;
            codegen_generate(gen, else_node->children[1]); // elseif block
            gen->indent_level--;
            write_indent(gen);
            
            // Continue with any remaining else/elseif branches
            if (else_node->child_count > 2 && else_node->children[2]) {
                fprintf(gen->output, "} else ");
                // Handle next branch without generating a complete new if statement
                ASTNode* next_else = else_node->children[2];
                if (next_else->type == NODE_IF) {
                    fprintf(gen->output, "if (");
                    codegen_generate(gen, next_else->children[0]);
                    fprintf(gen->output, ") {\n");
                    gen->indent_level++;
                    codegen_generate(gen, next_else->children[1]);
                    gen->indent_level--;
                    write_indent(gen);
                    
                    // Recursively handle any remaining branches
                    if (next_else->child_count > 2 && next_else->children[2]) {
                        fprintf(gen->output, "} else ");
                        if (next_else->children[2]->type == NODE_IF) {
                            fprintf(gen->output, "if (");
                            codegen_generate(gen, next_else->children[2]->children[0]);
                            fprintf(gen->output, ") {\n");
                            gen->indent_level++;
                            codegen_generate(gen, next_else->children[2]->children[1]);
                            gen->indent_level--;
                            write_indent(gen);
                        } else {
                            fprintf(gen->output, "{\n");
                            gen->indent_level++;
                            codegen_generate(gen, next_else->children[2]);
                            gen->indent_level--;
                            write_indent(gen);
                        }
                    }
                } else {
                    fprintf(gen->output, "{\n");
                    gen->indent_level++;
                    codegen_generate(gen, next_else);
                    gen->indent_level--;
                    write_indent(gen);
                }
                fprintf(gen->output, "}\n");
                return;
            }
        } else {
            // This is a regular else branch
            fprintf(gen->output, "} else {\n");
            gen->indent_level++;
            codegen_generate(gen, else_node); // else block
            gen->indent_level--;
            write_indent(gen);
        }
    }
    
    fprintf(gen->output, "}\n");
}

static void generate_while_statement(CodeGenerator* gen, ASTNode* node) {
    write_indent(gen);
    fprintf(gen->output, "while (");
    codegen_generate(gen, node->children[0]); // Condition
    fprintf(gen->output, ") {\n");
    
    gen->indent_level++;
    codegen_generate(gen, node->children[1]); // Loop body
    gen->indent_level--;
    
    write_indent(gen);
    fprintf(gen->output, "}\n");
}

static void generate_binary_op(CodeGenerator* gen, ASTNode* node) {
    verbose_print("Generating binary operation:\n");
    verbose_print("  Operator: ");
    debug_print_token_type(node->data.binary_op.op);
    verbose_print("\n  Left operand type: ");
    if (node->children[0]) debug_print_node_type(node->children[0]->type);
    verbose_print("\n  Right operand type: ");
    if (node->children[1]) debug_print_node_type(node->children[1]->type);
    verbose_print("\n");

    bool needs_parens = !gen->in_expression;
    if (needs_parens) fprintf(gen->output, "(");
    
    bool old_in_expr = gen->in_expression;
    gen->in_expression = true;
    
    fprintf(gen->output, "(");
    codegen_generate(gen, node->children[0]);
    
    // Output operator
    const char* op_str = NULL;
    switch (node->data.binary_op.op) {
        // Arithmetic operators
        case TOK_PLUS: op_str = " + "; break;
        case TOK_MINUS: op_str = " - "; break;
        case TOK_MULTIPLY: op_str = " * "; break;
        case TOK_DIVIDE: op_str = " / "; break;
        case TOK_MOD: op_str = " % "; break;

        // Bitwise operators
        case TOK_RSHIFT: op_str = " >> "; break;
        case TOK_LSHIFT: op_str = " << "; break;
        case TOK_BITAND: op_str = " & "; break;
        case TOK_BITOR: op_str = " | "; break;
        case TOK_BITXOR: op_str = " ^ "; break;

        // Logical operators
        case TOK_AND: op_str = " && "; break;
        case TOK_OR: op_str = " || "; break;
        
        // Comparison operators
        case TOK_EQ: op_str = " == "; break;
        case TOK_NE: op_str = " != "; break;
        case TOK_LT: op_str = " < "; break;
        case TOK_LE: op_str = " <= "; break;
        case TOK_GT: op_str = " > "; break;
        case TOK_GE: op_str = " >= "; break;
        default:
            op_str = " /* unknown op */ ";
            verbose_print("  WARNING: Unknown operator type: %d\n", node->data.binary_op.op);
    }
    fprintf(gen->output, "%s", op_str);
    verbose_print("  Writing operator: %s\n", op_str);
    
    codegen_generate(gen, node->children[1]);
    fprintf(gen->output, ")");
    
    gen->in_expression = old_in_expr;
    if (needs_parens) fprintf(gen->output, ")");
}

// Also update the unary operation generation for consistency
static void generate_unary(CodeGenerator* gen, ASTNode* node) {
    if (!node) return;

    bool needs_parens = !gen->in_expression;
    if (needs_parens) fprintf(gen->output, "(");

    switch (node->data.unary_op.op) {
        case TOK_MINUS: fprintf(gen->output, "-"); break;
        case TOK_NOT: fprintf(gen->output, "!"); break;
        case TOK_BITNOT: fprintf(gen->output, "~"); break;
        case TOK_DEREF:
            for (int i = 0; i < node->data.unary_op.deref_count; i++) {
                fprintf(gen->output, "*");
            }
            fprintf(gen->output, "("); 
            break;
        case TOK_ADDR_OF: fprintf(gen->output, "&("); break;
        default: fprintf(gen->output, "/* unknown unary op */");
    }

    bool old_in_expr = gen->in_expression;
    gen->in_expression = true;
    
    codegen_generate(gen, node->children[0]);
    
    // Close parenthesis for pointer operations
    if (node->data.unary_op.op == TOK_DEREF || 
        node->data.unary_op.op == TOK_ADDR_OF) {
        fprintf(gen->output, ")");
    }
    
    gen->in_expression = old_in_expr;
    if (needs_parens) fprintf(gen->output, ")");
}

static void generate_repeat_statement(CodeGenerator* gen, ASTNode* node) {
    if (!node || node->child_count != 2) {
        error_report(ERROR_INTERNAL, SEVERITY_ERROR,
                    (SourceLocation){0, 0, "internal"},
                    "Invalid repeat statement node");
        return;
    }

    write_indent(gen);
    fprintf(gen->output, "do {\n");
    
    gen->indent_level++;
    codegen_generate(gen, node->children[0]); // Generate loop body
    gen->indent_level--;
    
    write_indent(gen);
    fprintf(gen->output, "} while (!(");
    codegen_generate(gen, node->children[1]); // Generate condition
    fprintf(gen->output, "));\n");
}

static void generate_string(CodeGenerator* gen, ASTNode* node) {
    fprintf(gen->output, "\"");
    fprintf(gen->output, node->data.value);
    fprintf(gen->output, "\"");
}

static void generate_for_statement(CodeGenerator* gen, ASTNode* node) {
    verbose_print("Generating for statement\n");
    write_indent(gen);
    
    // Get loop variable name from node data
    const char* var_name = node->data.value;
    
    fprintf(gen->output, "for (");
    
    // Initialize loop variable
    fprintf(gen->output, "%s = ", var_name);
    bool old_in_expr = gen->in_expression;
    gen->in_expression = true;
    codegen_generate(gen, node->children[0]);
    
    // Condition depends on step direction
    fprintf(gen->output, "; %s ", var_name);
    
    // Determine if we have a step value and its direction
    bool has_step = node->children[3] != NULL;
    bool step_is_negative = false;
    
    if (has_step && node->children[3]->type == NODE_NUMBER) {
        // Check if the step is a negative number
        const char* step_value = node->children[3]->data.value;
        step_is_negative = (step_value && step_value[0] == '-');
    }
    
    // Generate appropriate comparison operator based on step direction
    fprintf(gen->output, "%s ", step_is_negative ? ">=" : "<=");
    codegen_generate(gen, node->children[1]);
    
    // Increment or decrement
    fprintf(gen->output, "; %s += ", var_name);
    if (has_step) {
        fprintf(gen->output, "%s", node->children[3]->data.value);
    } else {
        fprintf(gen->output, "1");
    }
    
    gen->in_expression = old_in_expr;
    fprintf(gen->output, ") {\n");
    
    // Generate loop body
    gen->indent_level++;
    codegen_generate(gen, node->children[2]);
    gen->indent_level--;
    
    write_indent(gen);
    fprintf(gen->output, "}\n");
}

static void generate_call(CodeGenerator* gen, ASTNode* node) {
    verbose_print("\n=== ENTERING GENERATE_FUNCTION_CALL ===\n");
    verbose_print("Function name: %s\n", node->data.value);

    // Look up function/procedure symbol to get parameter information
    Symbol* func_sym = symtable_lookup(gen->symbols, node->data.value);
    //if (!func_sym || (func_sym->kind != SYMBOL_FUNCTION && func_sym->kind != SYMBOL_PROCEDURE)) {
    //    verbose_print("Function symbol not found or invalid\n");
    //    return;
    //}

    fprintf(gen->output, "%s(", node->data.value);

    // Generate arguments
    for (int i = 0; i < node->child_count; i++) {
        if (i > 0) fprintf(gen->output, ", ");
        bool needs_address_of = false;
        if (func_sym) {
            if (i < func_sym->info.func.param_count) {
                Symbol* param = func_sym->info.func.parameters[i];
                if (param && param->info.var.needs_deref && !param->info.var.is_array && param->info.var.param_mode &&
                    (strcasecmp(param->info.var.param_mode, "out") == 0 || 
                    strcasecmp(param->info.var.param_mode, "inout") == 0|| 
                    strcasecmp(param->info.var.param_mode, "in/out") == 0)) {
                    needs_address_of = true;
                }
            }
        }

        // Add address-of operator if needed
        if (needs_address_of) {
            fprintf(gen->output, "&");
        }

        verbose_print("Generating argument %d, type: %d\n", i, node->children[i]->type);
        
        if (node->children[i]->type == NODE_ARRAY_ACCESS) {
            verbose_print("Argument is array access\n");
            generate_array_access(gen, node->children[i]);
        } else {
            codegen_generate(gen, node->children[i]);
        }
    }

    fprintf(gen->output, ")");

    // Add semicolon if this is a standalone call (not part of an expression)
    if (!gen->in_expression) {
        fprintf(gen->output, ";\n");
    }

    verbose_print("=== EXITING GENERATE_FUNCTION_CALL ===\n");
}

void codegen_generate(CodeGenerator* gen, ASTNode* node) {
    if (!node) return;
    
    verbose_print("Generating code for node type: %d\n", node->type);
    debug_codegen_state(gen, "starting node generation");

    switch (node->type) {
        case NODE_PROGRAM:
            // Generate standard includes
            fprintf(gen->output, "#include <stdbool.h>\n");
            fprintf(gen->output, "#include <stdio.h>\n\n");
            fprintf(gen->output, "#include <memory.h>\n\n");
            
            // Generate all declarations and definitions
            for (int i = 0; i < node->child_count; i++) {
                codegen_generate(gen, node->children[i]);
                fprintf(gen->output, "\n");
            }
            break;
            
        case NODE_FUNCTION:
        case NODE_PROCEDURE:
            debug_codegen_function(gen, node, "starting function generation");
            generate_function_declaration(gen, node);
            break;
            
        case NODE_VAR_DECL:
            if (node->data.variable.is_array) {
                generate_variable_declaration(gen, node);
            } else {
                generate_variable_declaration(gen, node);
            }
            break;
            
        case NODE_ARRAY_DECL:
            generate_variable_declaration(gen, node);
            break;
            
        case NODE_BLOCK:
            debug_codegen_block(gen, "entering block", gen->indent_level + 1);
            for (int i = 0; i < node->child_count; i++) {
                codegen_generate(gen, node->children[i]);
            }
            debug_codegen_block(gen, "exiting block", gen->indent_level - 1);
            break;
            
        case NODE_ASSIGNMENT:
            generate_assignment(gen, node);
            break;
            
        case NODE_IF:
            generate_if_statement(gen, node);
            break;
            
        case NODE_WHILE:
            generate_while_statement(gen, node);
            break;

        case NODE_FOR:
            generate_for_statement(gen, node);
            break;

        case NODE_REPEAT:
            generate_repeat_statement(gen, node);
            break;

        case NODE_RETURN:
            gen->needs_return = false;  // Explicit return found
            write_indent(gen);
            fprintf(gen->output, "return ");
            if (node->child_count > 0) {
                codegen_generate(gen, node->children[0]);
            } else if (gen->current_function) {
                fprintf(gen->output, "%s", gen->current_function);
            }
            fprintf(gen->output, ";\n");
            break;
            
        case NODE_BINARY_OP:
            debug_codegen_expression(gen, node, "operator expression");
            generate_binary_op(gen, node);
            break;
            
        case NODE_IDENTIFIER:
            if (strcmp(node->data.value, "true") == 0 || 
                strcmp(node->data.value, ".true.") == 0) {
                fprintf(gen->output, "true");
            } else if (strcmp(node->data.value, "false") == 0 || 
                      strcmp(node->data.value, ".false.") == 0) {
                fprintf(gen->output, "false");
            } else {
                fprintf(gen->output, "%s", node->data.value);
            }
            break;

        case NODE_BOOL:
            // Convert true/false to 1/0
            fprintf(gen->output, "%s", 
                strcmp(node->data.value, "true") == 0 || 
                strcmp(node->data.value, ".true.") == 0 ? "true" : "false");
            break;
            
        case NODE_NUMBER:
            generate_number_literal(gen, node->data.value);
            //fprintf(gen->output, "%s", node->data.value);
            break;

        case NODE_VARIABLE:
            fprintf(gen->output, "%s", node->data.variable.name);
            break;

        case NODE_ARRAY_ACCESS:
            ASTNode* root = node;
            while (root->children[0]) {
                root = root->children[0];
            }
            if (root->children[0] && root->children[0]->data.variable.name) {
                debug_codegen_array(gen, root->children[0], "array access");
                fprintf(gen->output, "%s", root->children[0]->data.variable.name);
                fprintf(gen->output, "[");

                if (node->children[1]) {
                    codegen_generate(gen, root->children[1]);
                }
                fprintf(gen->output, "]");
            }
            break;

        case NODE_CALL: {
            if (!gen->in_expression) {
                write_indent(gen);
            }
            generate_call(gen, node);
            break;
        }

        case NODE_PRINT:
            generate_print_statement(gen, node);
            break;

        case NODE_READ:
            generate_read_statement(gen, node);
            break;

        case NODE_UNARY_OP:
            debug_codegen_expression(gen, node, "operator expression");
            verbose_print("Generating unary operation\n");
            if (node->data.unary_op.op == TOK_AT) {
                codegen_generate(gen, node->children[0]);
            } else if (node->data.unary_op.op == TOK_DEREF) {
                fprintf(gen->output, "*");
                codegen_generate(gen, node->children[0]);
            } else {
                generate_unary(gen, node);
            }
            break;
        case NODE_TYPE_DECLARATION:
            generate_record_type(gen, node->children[0]);
            break;
        case NODE_STRING:
            generate_string(gen, node);
            break;
        case NODE_FIELD_ACCESS:
            generate_field_access(gen, node);
            break;
        default:
            error_report(ERROR_INTERNAL, SEVERITY_ERROR,
                        node->loc,
                        "Unsupported node type %d in code generation", node->type);
            break;
    }
    debug_codegen_state(gen, "completed node generation");
}