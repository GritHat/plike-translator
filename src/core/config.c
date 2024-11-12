#include "config.h"
#include "errors.h"
#include "debug.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

// Global configuration instance
TranslatorConfig g_config;

// Default configuration values
static const TranslatorConfig DEFAULT_CONFIG = {
    .assignment_style = ASSIGNMENT_COLON_EQUALS,
    .array_indexing = ARRAY_ONE_BASED,
    .param_style = PARAM_STYLE_MIXED,
    .operator_style = OP_STYLE_MIXED,
    .allow_mixed_array_access = true,
    .input_filename = NULL,
    .output_filename = NULL,
    .enable_verbose = false
};

void config_init(void) {
    memset(&g_config, 0, sizeof(TranslatorConfig));
    config_set_defaults();
}

void config_set_defaults(void) {
    g_config = DEFAULT_CONFIG;
    keywords = keywords_mixed;
}

void config_cleanup(void) {
    free(g_config.input_filename);
    free(g_config.output_filename);
    g_config.input_filename = NULL;
    g_config.output_filename = NULL;
}

static void print_usage(const char* program_name) {
    fprintf(stderr, "Usage: %s [options] input_file [output_file]\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -a, --assignment=STYLE    Set assignment style (colon-equals|equals)\n");
    fprintf(stderr, "  -i, --indexing=STYLE      Set array indexing style (zero|one)\n");
    fprintf(stderr, "  -p, --params=STYLE        Set parameter style (decl|body|mixed)\n");
    fprintf(stderr, "  -o, --operators=STYLE     Set operator style (standard|dotted|mixed)\n");
    fprintf(stderr, "  -m, --mixed-arrays=STYLE  Allow mixed array access ([] and ()) (true|false)\n");
    fprintf(stderr, "  -d, --debug=FLAGS         Set debug flags (lexer,parser,ast,symbols,codegen,all)\n");
    fprintf(stderr, "  -h, --help                Display this help message\n");
}

static bool parse_assignment_style(const char* style) {
    if (strcmp(style, "colon-equals") == 0) {
        g_config.assignment_style = ASSIGNMENT_COLON_EQUALS;
    } else if (strcmp(style, "equals") == 0) {
        g_config.assignment_style = ASSIGNMENT_EQUALS;
    } else {
        return false;
    }
    return true;
}

static bool parse_array_indexing(const char* style) {
    if (strcmp(style, "zero") == 0) {
        g_config.array_indexing = ARRAY_ZERO_BASED;
    } else if (strcmp(style, "one") == 0) {
        g_config.array_indexing = ARRAY_ONE_BASED;
    } else {
        return false;
    }
    return true;
}

static bool parse_param_style(const char* style) {
    if (strcmp(style, "decl") == 0) {
        g_config.param_style = PARAM_STYLE_IN_DECL;
    } else if (strcmp(style, "body") == 0) {
        g_config.param_style = PARAM_STYLE_IN_BODY;
    } else if (strcmp(style, "mixed") == 0) {
        g_config.param_style = PARAM_STYLE_MIXED;
    } else {
        return false;
    }
    return true;
}

static bool parse_operator_style(const char* style) {
    if (strcmp(style, "standard") == 0) {
        g_config.operator_style = OP_STYLE_STANDARD;
        keywords = keywords_standard;
    } else if (strcmp(style, "dotted") == 0) {
        g_config.operator_style = OP_STYLE_DOTTED;
        keywords = keywords_dotted;
    } else if (strcmp(style, "mixed") == 0) {
        g_config.operator_style = OP_STYLE_MIXED;
        keywords = keywords_mixed;
    } else {
        return false;
    }
    return true;
}

static bool parse_debug_flags(const char* style) {
    char* flags_copy = strdup(style);
    char* token = strtok(flags_copy, ",");
    DebugFlags debug_flags = 0;
    debug_set_flags(debug_flags);
    
    while (token) {
        if (strcasecmp(token, "lexer") == 0) { debug_flags |= DEBUG_LEXER; }
        else if (strcasecmp(token, "parser") == 0) { debug_flags |= DEBUG_PARSER; }
        else if (strcasecmp(token, "ast") == 0) { debug_flags |= DEBUG_AST; }
        else if (strcasecmp(token, "symbols") == 0) { debug_flags |= DEBUG_SYMBOLS; }
        else if (strcasecmp(token, "codegen") == 0) { debug_flags |= DEBUG_CODEGEN; }
        else if (strcasecmp(token, "all") == 0) { debug_flags |= DEBUG_ALL; }
        else {
            fprintf(stderr, "Invalid debug flag: (%s)\n", token);
            free(flags_copy);
            return false;
        }
        token = strtok(NULL, ",");
    }
    free(flags_copy);
    debug_set_flags(debug_flags);
    return true;
}

bool config_parse_args(int argc, char** argv) {
     static struct option long_options[] = {
        {"assignment", required_argument, 0, 'a'},
        {"indexing", required_argument, 0, 'i'},
        {"params", required_argument, 0, 'p'},
        {"operators", required_argument, 0, 'o'},
        {"mixed-arrays", required_argument, 0, 'm'},
        {"debug", required_argument, 0, 'd'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "a:i:p:o:d:mh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'a':
                if (!parse_assignment_style(optarg)) {
                    fprintf(stderr, "Invalid assignment style: %s\n", optarg);
                    return false;
                }
                break;

            case 'i':
                if (!parse_array_indexing(optarg)) {
                    fprintf(stderr, "Invalid array indexing style: %s\n", optarg);
                    return false;
                }
                break;

            case 'p':
                if (!parse_param_style(optarg)) {
                    fprintf(stderr, "Invalid parameter style: %s\n", optarg);
                    return false;
                }
                break;

            case 'o':
                if (!parse_operator_style(optarg)) {
                    fprintf(stderr, "Invalid operator style: %s\n", optarg);
                    return false;
                }
                break;

            case 'd':
                if (!parse_debug_flags(optarg)) {
                    fprintf(stderr, "Invalid debug flag style: %s\n", optarg);
                    return false;
                }
                break;

            case 'm':
                g_config.allow_mixed_array_access = true;
                break;

            case 'h':
                print_usage(argv[0]);
                exit(0);

            default:
                print_usage(argv[0]);
                return false;
        }
    }

    // Handle input and output files
    if (optind < argc) {
        g_config.input_filename = strdup(argv[optind++]);
    } else {
        fprintf(stderr, "Error: Input file required\n");
        print_usage(argv[0]);
        return false;
    }

    if (optind < argc) {
        //char* buffer = malloc(strlen(argv[optind]) + 7);
        //sprintf(buffer, "%s%s", "output/", argv[optind]);
        //g_config.output_filename = strdup(buffer);
        //free(buffer);
        g_config.output_filename = strdup(argv[optind]);
    }

    return true;
}