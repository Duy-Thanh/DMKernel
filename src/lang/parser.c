#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../include/lang/parser.h"
#include "../../include/core/debug.h"

// Override debug macros locally to disable them
#undef DM_DEBUG_ERROR_PRINT
#undef DM_DEBUG_WARN_PRINT
#undef DM_DEBUG_INFO_PRINT
#undef DM_DEBUG_VERBOSE_PRINT
#define DM_DEBUG_ERROR_PRINT(...)
#define DM_DEBUG_WARN_PRINT(...)
#define DM_DEBUG_INFO_PRINT(...)
#define DM_DEBUG_VERBOSE_PRINT(...)

// Initialize parser
dm_error_t dm_parser_init(dm_context_t *ctx, dm_parser_t *parser, const char *source, size_t source_len) {
    if (ctx == NULL || parser == NULL || source == NULL) {
        DM_DEBUG_ERROR_PRINT("Invalid arguments to dm_parser_init\n");
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    DM_DEBUG_INFO_PRINT("Initializing parser with source length: %zu\n", source_len);
    
    // Initialize parser
    parser->ctx = ctx;
    strncpy(parser->error_message, "", sizeof(parser->error_message));
    
    // Initialize lexer
    return dm_lexer_init(ctx, &parser->lexer, source, source_len);
}

// Helper function for error reporting
static void report_error(dm_parser_t *parser, const char *message) {
    strncpy(parser->error_message, message, sizeof(parser->error_message) - 1);
    parser->error_message[sizeof(parser->error_message) - 1] = '\0';
}

// Check if current token matches the given type
static bool match(dm_parser_t *parser, dm_token_type_t type) {
    return parser->current.type == type;
}

// Check if current token is a specific keyword
static bool match_keyword(dm_parser_t *parser, const char *keyword) {
    if (parser->current.type != DM_TOKEN_KEYWORD) {
        return false;
    }
    
    return (strncmp(parser->current.text, keyword, parser->current.length) == 0 &&
            strlen(keyword) == parser->current.length);
}

// Check if current token is a specific symbol
static bool match_symbol(dm_parser_t *parser, char symbol) {
    if (parser->current.type != DM_TOKEN_SYMBOL) {
        return false;
    }
    
    return parser->current.text[0] == symbol && parser->current.length == 1;
}

// Check if current token is a specific token type (use this instead of match in updated code)
static bool match_token_type(dm_parser_t *parser, dm_token_type_t type) {
    return parser->current.type == type;
}

// Consume current token and move to next
static dm_error_t consume(dm_parser_t *parser) {
    return dm_lexer_next_token(&parser->lexer, &parser->current);
}

// Consume and check token type
static dm_error_t consume_type(dm_parser_t *parser, dm_token_type_t type, const char *error_message) {
    if (parser->current.type != type) {
        report_error(parser, error_message);
        return DM_ERROR_SYNTAX_ERROR;
    }
    
    return consume(parser);
}

// Consume and check keyword
static dm_error_t consume_keyword(dm_parser_t *parser, const char *keyword, const char *error_message) {
    if (!match_keyword(parser, keyword)) {
        report_error(parser, error_message);
        return DM_ERROR_SYNTAX_ERROR;
    }
    
    return consume(parser);
}

// Consume and check symbol
static dm_error_t consume_symbol(dm_parser_t *parser, char symbol, const char *error_message) {
    if (!match_symbol(parser, symbol)) {
        report_error(parser, error_message);
        return DM_ERROR_SYNTAX_ERROR;
    }
    
    return consume(parser);
}

// Create a node of the given type
static dm_node_t* create_node(dm_context_t *ctx, dm_node_type_t type) {
    dm_node_t *node = dm_malloc(ctx, sizeof(dm_node_t));
    if (node == NULL) {
        return NULL;
    }
    
    // Initialize node
    node->type = type;
    
    // Clear the union
    memset(&node->program, 0, sizeof(node->program));
    
    return node;
}

// Forward declarations
static dm_node_t* parse_expression(dm_parser_t *parser);
static dm_node_t* parse_statement(dm_parser_t *parser);
static dm_node_t* parse_block(dm_parser_t *parser);
static dm_node_t* parse_primary(dm_parser_t *parser);
static dm_node_t* parse_assignment(dm_parser_t *parser);
static dm_node_t* parse_binary(dm_parser_t *parser, int precedence);
static dm_node_t* parse_unary(dm_parser_t *parser);
static dm_node_t* parse_if(dm_parser_t *parser);
static dm_node_t* parse_while(dm_parser_t *parser);
static dm_node_t* parse_function(dm_parser_t *parser);
static dm_node_t* parse_return(dm_parser_t *parser);

// Parse a program (multiple statements)
static dm_node_t* parse_program(dm_parser_t *parser) {
    dm_node_t *node = create_node(parser->ctx, DM_NODE_PROGRAM);
    if (node == NULL) {
        return NULL;
    }
    
    // Allocate memory for statements
    size_t capacity = 8; // Initial capacity
    node->program.statements = dm_malloc(parser->ctx, capacity * sizeof(dm_node_t*));
    if (node->program.statements == NULL) {
        dm_free(parser->ctx, node);
        return NULL;
    }
    
    node->program.count = 0;
    node->program.capacity = capacity;
    
    // Get first token
    dm_error_t err = consume(parser);
    if (err != DM_SUCCESS) {
        dm_free(parser->ctx, node->program.statements);
        dm_free(parser->ctx, node);
        return NULL;
    }
    
    // Parse statements until end of file
    while (parser->current.type != DM_TOKEN_EOF) {
        dm_node_t *stmt = parse_statement(parser);
        if (stmt == NULL) {
            // Error occurred
            for (size_t i = 0; i < node->program.count; i++) {
                dm_node_free(parser->ctx, node->program.statements[i]);
            }
            dm_free(parser->ctx, node->program.statements);
            dm_free(parser->ctx, node);
            return NULL;
        }
        
        // Resize statements array if necessary
        if (node->program.count >= node->program.capacity) {
            size_t new_capacity = node->program.capacity * 2;
            dm_node_t **new_stmts = dm_realloc(parser->ctx, node->program.statements,
                                             new_capacity * sizeof(dm_node_t*));
            
            if (new_stmts == NULL) {
                // Allocation failed, clean up
                for (size_t i = 0; i < node->program.count; i++) {
                    dm_node_free(parser->ctx, node->program.statements[i]);
                }
                dm_free(parser->ctx, node->program.statements);
                dm_free(parser->ctx, node);
                return NULL;
            }
            
            node->program.statements = new_stmts;
            node->program.capacity = new_capacity;
        }
        
        node->program.statements[node->program.count++] = stmt;
    }
    
    return node;
}

// Parse a literal value
static dm_node_t* parse_literal(dm_parser_t *parser) {
    dm_node_t *node = create_node(parser->ctx, DM_NODE_LITERAL);
    if (node == NULL) {
        return NULL;
    }
    
    if (match(parser, DM_TOKEN_NUMBER)) {
        // Parse number
        char *tmp = dm_malloc(parser->ctx, parser->current.length + 1);
        if (tmp == NULL) {
            dm_free(parser->ctx, node);
            return NULL;
        }
        
        // Copy and null-terminate the number string
        strncpy(tmp, parser->current.text, parser->current.length);
        tmp[parser->current.length] = '\0';
        
        // Check if it's a floating-point number
        bool is_float = false;
        for (size_t i = 0; i < parser->current.length; i++) {
            if (tmp[i] == '.') {
                is_float = true;
                break;
            }
        }
        
        if (is_float) {
            // Float value
            node->literal.type = DM_LITERAL_NUMBER;
            node->literal.value.number = atof(tmp);
        } else {
            // Integer value (convert to double for uniformity)
            node->literal.type = DM_LITERAL_NUMBER;
            node->literal.value.number = (double)atoll(tmp);
        }
        
        // Free temporary string
        dm_free(parser->ctx, tmp);
        
        // Consume the number token
        if (consume(parser) != DM_SUCCESS) {
            dm_free(parser->ctx, node);
            return NULL;
        }
    } else if (match(parser, DM_TOKEN_STRING)) {
        // Remove quotes from string literal
        size_t len = parser->current.length - 2;  // subtract 2 for quotes
        char* value = dm_malloc(parser->ctx, len + 1);
        if (value == NULL) {
            return NULL;
        }
        
        strncpy(value, parser->current.text + 1, len);
        value[len] = '\0';
        
        dm_node_t* node = create_node(parser->ctx, DM_NODE_LITERAL);
        if (node == NULL) {
            dm_free(parser->ctx, value);
            return NULL;
        }
        
        node->literal.type = DM_LITERAL_STRING;
        node->literal.value.string = value;
        
        consume(parser);
        return node;
    } else if (match_keyword(parser, "true")) {
        node->literal.type = DM_LITERAL_BOOLEAN;
        node->literal.value.boolean = true;
        
        // Consume the 'true' token
        if (consume(parser) != DM_SUCCESS) {
            dm_free(parser->ctx, node);
            return NULL;
        }
    } else if (match_keyword(parser, "false")) {
        node->literal.type = DM_LITERAL_BOOLEAN;
        node->literal.value.boolean = false;
        
        // Consume the 'false' token
        if (consume(parser) != DM_SUCCESS) {
            dm_free(parser->ctx, node);
            return NULL;
        }
    } else if (match_keyword(parser, "null")) {
        node->literal.type = DM_LITERAL_NULL;
        
        // Consume the 'null' token
        if (consume(parser) != DM_SUCCESS) {
            dm_free(parser->ctx, node);
            return NULL;
        }
    } else {
        dm_free(parser->ctx, node);
        return NULL;
    }
    
    return node;
}

// Parse a variable
static dm_node_t* parse_variable(dm_parser_t *parser) {
    if (!match(parser, DM_TOKEN_IDENTIFIER)) {
        return NULL;
    }
    
    dm_node_t *node = create_node(parser->ctx, DM_NODE_VARIABLE);
    if (node == NULL) {
        return NULL;
    }
    
    // Copy variable name
    size_t name_len = parser->current.length;
    char *name = dm_malloc(parser->ctx, name_len + 1);
    if (name == NULL) {
        dm_free(parser->ctx, node);
        return NULL;
    }
    
    strncpy(name, parser->current.text, name_len);
    name[name_len] = '\0';
    
    node->variable.name = name;
    
    // Consume the identifier token
    if (consume(parser) != DM_SUCCESS) {
        dm_free(parser->ctx, node->variable.name);
        dm_free(parser->ctx, node);
        return NULL;
    }
    
    return node;
}

// Parse a statement
static dm_node_t* parse_statement(dm_parser_t *parser) {
    if (parser == NULL) {
        return NULL;
    }
    
    // Check for variable declaration
    if (match_keyword(parser, "let") || match_keyword(parser, "var") || match_keyword(parser, "const")) {
        return parse_assignment(parser);
    }
    
    // Check for function definition
    if (match_keyword(parser, "function")) {
        return parse_function(parser);
    }
    
    // Check for return statement
    if (match_keyword(parser, "return")) {
        return parse_return(parser);
    }
    
    // Check for if statement
    if (match_keyword(parser, "if")) {
        return parse_if(parser);
    }
    
    // Check for while loop
    if (match_keyword(parser, "while")) {
        return parse_while(parser);
    }
    
    // Check for block statement
    if (match_symbol(parser, '{')) {
        return parse_block(parser);
    }
    
    // Check for non-declaration assignment (identifier followed by equals)
    if (match(parser, DM_TOKEN_IDENTIFIER)) {
        // Save the identifier for possible assignment
        char *name = dm_malloc(parser->ctx, parser->current.length + 1);
        if (name == NULL) {
            return NULL;
        }
        
        strncpy(name, parser->current.text, parser->current.length);
        name[parser->current.length] = '\0';
        
        // Consume identifier
        if (consume(parser) != DM_SUCCESS) {
            dm_free(parser->ctx, name);
            return NULL;
        }
        
        // Check if this is an assignment (=)
        if (match(parser, DM_TOKEN_OPERATOR) && parser->current.length == 1 && parser->current.text[0] == '=') {
            // Create assignment node
            dm_node_t *node = create_node(parser->ctx, DM_NODE_ASSIGNMENT);
            if (node == NULL) {
                dm_free(parser->ctx, name);
                return NULL;
            }
            
            node->assignment.name = name;
            node->assignment.is_declaration = false;
            
            // Consume the equals sign
            if (consume(parser) != DM_SUCCESS) {
                dm_free(parser->ctx, name);
                dm_free(parser->ctx, node);
                return NULL;
            }
            
            // Parse the right-hand expression
            dm_node_t *value = parse_expression(parser);
            if (value == NULL) {
                dm_free(parser->ctx, name);
                dm_free(parser->ctx, node);
                return NULL;
            }
            
            node->assignment.value = value;
            
            // Expect semicolon after assignment
            if (!match_symbol(parser, ';')) {
                report_error(parser, "Expected ';' after assignment");
                dm_free(parser->ctx, name);
                dm_node_free(parser->ctx, value);
                dm_free(parser->ctx, node);
                return NULL;
            }
            
            // Consume the semicolon
            if (consume(parser) != DM_SUCCESS) {
                dm_free(parser->ctx, name);
                dm_node_free(parser->ctx, value);
                dm_free(parser->ctx, node);
                return NULL;
            }
            
            return node;
        }
        
        // Not an assignment, must be a variable reference
        // Create a variable node
        dm_node_t *var_node = create_node(parser->ctx, DM_NODE_VARIABLE);
        if (var_node == NULL) {
            dm_free(parser->ctx, name);
            return NULL;
        }
        
        // Set the variable name
        var_node->variable.name = name;
        
        // Expect semicolon after expression statement
        if (!match_symbol(parser, ';')) {
            report_error(parser, "Expected ';' after expression");
            dm_free(parser->ctx, name);
            dm_free(parser->ctx, var_node);
            return NULL;
        }
        
        // Consume the semicolon
        if (consume(parser) != DM_SUCCESS) {
            dm_free(parser->ctx, name);
            dm_free(parser->ctx, var_node);
            return NULL;
        }
        
        return var_node;
    }
    
    // Expression statement
    dm_node_t *expr = parse_expression(parser);
    if (expr == NULL) {
        return NULL;
    }
    
    // Expect semicolon after expression statement
    if (!match_symbol(parser, ';')) {
        report_error(parser, "Expected ';' after expression");
        dm_node_free(parser->ctx, expr);
        return NULL;
    }
    
    // Consume the semicolon
    if (consume(parser) != DM_SUCCESS) {
        dm_node_free(parser->ctx, expr);
        return NULL;
    }
    
    return expr;
}

// Parse a variable assignment
static dm_node_t* parse_assignment(dm_parser_t *parser) {
    DM_DEBUG_INFO_PRINT("Parsing assignment...\n");
    
    bool is_declaration = false;
    bool is_const = false;
    
    // Check if this is a declaration (let/var/const)
    if (match_keyword(parser, "let") || match_keyword(parser, "var")) {
        is_declaration = true;
        DM_DEBUG_VERBOSE_PRINT("  Found variable declaration keyword\n");
        if (consume(parser) != DM_SUCCESS) {
            DM_DEBUG_ERROR_PRINT("  Error consuming declaration keyword\n");
            return NULL;
        }
    } else if (match_keyword(parser, "const")) {
        is_declaration = true;
        is_const = true;
        DM_DEBUG_VERBOSE_PRINT("  Found const declaration keyword\n");
        if (consume(parser) != DM_SUCCESS) {
            DM_DEBUG_ERROR_PRINT("  Error consuming const keyword\n");
            return NULL;
        }
    }
    
    // Variable name
    if (!match(parser, DM_TOKEN_IDENTIFIER)) {
        DM_DEBUG_ERROR_PRINT("  Expected identifier but found: %.*s (type: %d)\n", 
                (int)parser->current.length, parser->current.text, parser->current.type);
        report_error(parser, "Expected variable name");
        return NULL;
    }
    
    DM_DEBUG_VERBOSE_PRINT("  Found identifier: %.*s\n", (int)parser->current.length, parser->current.text);
    
    // Store the variable name
    char *name = dm_malloc(parser->ctx, parser->current.length + 1);
    if (name == NULL) {
        DM_DEBUG_ERROR_PRINT("  Memory allocation failed for variable name\n");
        return NULL;
    }
    
    strncpy(name, parser->current.text, parser->current.length);
    name[parser->current.length] = '\0';
    
    if (consume(parser) != DM_SUCCESS) {
        DM_DEBUG_ERROR_PRINT("  Error consuming identifier\n");
        dm_free(parser->ctx, name);
        return NULL;
    }
    
    // Expect equals sign (= operator)
    if (!(match(parser, DM_TOKEN_OPERATOR) && parser->current.length == 1 && parser->current.text[0] == '=')) {
        DM_DEBUG_ERROR_PRINT("  Expected '=' operator but found: %.*s\n", 
                (int)parser->current.length, parser->current.text);
        report_error(parser, "Expected '=' in assignment");
        dm_free(parser->ctx, name);
        return NULL;
    }
    
    if (consume(parser) != DM_SUCCESS) {
        DM_DEBUG_ERROR_PRINT("  Error consuming '='\n");
        dm_free(parser->ctx, name);
        return NULL;
    }
    
    // Parse the expression value
    dm_node_t *value = parse_expression(parser);
    if (value == NULL) {
        DM_DEBUG_ERROR_PRINT("  Failed to parse expression\n");
        dm_free(parser->ctx, name);
        return NULL;
    }
    
    // Expect semicolon
    if (!match_symbol(parser, ';')) {
        DM_DEBUG_ERROR_PRINT("  Expected ';' but found: %.*s\n", 
                (int)parser->current.length, parser->current.text);
        report_error(parser, "Expected ';' after assignment");
        dm_free(parser->ctx, name);
        dm_node_free(parser->ctx, value);
        return NULL;
    }
    
    if (consume(parser) != DM_SUCCESS) {
        DM_DEBUG_ERROR_PRINT("  Error consuming ';'\n");
        dm_free(parser->ctx, name);
        dm_node_free(parser->ctx, value);
        return NULL;
    }
    
    // Create assignment node
    dm_node_t *node = create_node(parser->ctx, DM_NODE_ASSIGNMENT);
    if (node == NULL) {
        DM_DEBUG_ERROR_PRINT("  Memory allocation failed for assignment node\n");
        dm_free(parser->ctx, name);
        dm_node_free(parser->ctx, value);
        return NULL;
    }
    
    node->assignment.name = name;
    node->assignment.value = value;
    node->assignment.is_declaration = is_declaration;
    
    DM_DEBUG_INFO_PRINT("Assignment parsing completed successfully.\n");
    return node;
}

// Parse an expression
static dm_node_t* parse_expression(dm_parser_t *parser) {
    // Start with lowest precedence
    return parse_binary(parser, 1);
}

// Parse a primary expression (literals, variables, grouping)
static dm_node_t* parse_primary(dm_parser_t *parser) {
    if (parser == NULL) {
        return NULL;
    }
    
    // Parse literals
    if (match(parser, DM_TOKEN_NUMBER)) {
        double value = atof(parser->current.text);
        
        dm_node_t* node = create_node(parser->ctx, DM_NODE_LITERAL);
        if (node == NULL) {
            return NULL;
        }
        
        node->literal.type = DM_LITERAL_NUMBER;
        node->literal.value.number = value;
        
        consume(parser);
        return node;
    }
    
    if (match(parser, DM_TOKEN_STRING)) {
        // Remove quotes from string literal
        size_t len = parser->current.length - 2;  // subtract 2 for quotes
        char* value = dm_malloc(parser->ctx, len + 1);
        if (value == NULL) {
            return NULL;
        }
        
        strncpy(value, parser->current.text + 1, len);
        value[len] = '\0';
        
        dm_node_t* node = create_node(parser->ctx, DM_NODE_LITERAL);
        if (node == NULL) {
            dm_free(parser->ctx, value);
            return NULL;
        }
        
        node->literal.type = DM_LITERAL_STRING;
        node->literal.value.string = value;
        
        consume(parser);
        return node;
    }
    
    if (match_keyword(parser, "true") || match_keyword(parser, "false")) {
        bool value = strcmp(parser->current.text, "true") == 0;
        
        dm_node_t* node = create_node(parser->ctx, DM_NODE_LITERAL);
        if (node == NULL) {
            return NULL;
        }
        
        node->literal.type = DM_LITERAL_BOOLEAN;
        node->literal.value.boolean = value;
        
        consume(parser);
        return node;
    }
    
    if (match_keyword(parser, "null")) {
        dm_node_t* node = create_node(parser->ctx, DM_NODE_LITERAL);
        if (node == NULL) {
            return NULL;
        }
        
        node->literal.type = DM_LITERAL_NULL;
        
        consume(parser);
        return node;
    }
    
    // Parse variable reference or function call
    if (match(parser, DM_TOKEN_IDENTIFIER)) {
        // Get the identifier name
        size_t name_len = parser->current.length;
        char* name = dm_malloc(parser->ctx, name_len + 1);
        if (name == NULL) {
            return NULL;
        }
        
        // Copy the identifier text
        strncpy(name, parser->current.text, name_len);
        name[name_len] = '\0';
        
        // Consume the identifier
        if (consume(parser) != DM_SUCCESS) {
            dm_free(parser->ctx, name);
            return NULL;
        }
        
        // Check if this is a function call (identifier followed by parenthesis)
        if (match_symbol(parser, '(')) {
            // This is a function call
            if (consume(parser) != DM_SUCCESS) {
                dm_free(parser->ctx, name);
                return NULL;
            }
            
            // Parse arguments
            dm_node_t** arguments = NULL;
            size_t argument_count = 0;
            size_t argument_capacity = 0;
            
            // Parse argument list
            while (!match_symbol(parser, ')')) {
                // Check for comma between arguments
                if (argument_count > 0) {
                    if (!match_symbol(parser, ',')) {
                        report_error(parser, "Expected ',' between arguments");
                        dm_free(parser->ctx, name);
                        for (size_t i = 0; i < argument_count; i++) {
                            dm_node_free(parser->ctx, arguments[i]);
                        }
                        dm_free(parser->ctx, arguments);
                        return NULL;
                    }
                    
                    if (consume(parser) != DM_SUCCESS) {
                        dm_free(parser->ctx, name);
                        for (size_t i = 0; i < argument_count; i++) {
                            dm_node_free(parser->ctx, arguments[i]);
                        }
                        dm_free(parser->ctx, arguments);
                        return NULL;
                    }
                }
                
                // Parse argument expression
                dm_node_t* argument = parse_expression(parser);
                if (argument == NULL) {
                    report_error(parser, "Expected expression for argument");
                    dm_free(parser->ctx, name);
                    for (size_t i = 0; i < argument_count; i++) {
                        dm_node_free(parser->ctx, arguments[i]);
                    }
                    dm_free(parser->ctx, arguments);
                    return NULL;
                }
                
                // Allocate or resize arguments array if needed
                if (argument_count >= argument_capacity) {
                    size_t new_capacity = argument_capacity == 0 ? 4 : argument_capacity * 2;
                    dm_node_t** new_arguments = dm_realloc(parser->ctx, arguments, new_capacity * sizeof(dm_node_t*));
                    if (new_arguments == NULL) {
                        report_error(parser, "Failed to allocate memory for arguments");
                        dm_free(parser->ctx, name);
                        dm_node_free(parser->ctx, argument);
                        for (size_t i = 0; i < argument_count; i++) {
                            dm_node_free(parser->ctx, arguments[i]);
                        }
                        dm_free(parser->ctx, arguments);
                        return NULL;
                    }
                    arguments = new_arguments;
                    argument_capacity = new_capacity;
                }
                
                // Store argument
                arguments[argument_count] = argument;
                argument_count++;
            }
            
            // Consume closing parenthesis
            if (consume(parser) != DM_SUCCESS) {
                dm_free(parser->ctx, name);
                for (size_t i = 0; i < argument_count; i++) {
                    dm_node_free(parser->ctx, arguments[i]);
                }
                dm_free(parser->ctx, arguments);
                return NULL;
            }
            
            // Create call node
            dm_node_t* node = create_node(parser->ctx, DM_NODE_CALL);
            if (node == NULL) {
                dm_free(parser->ctx, name);
                for (size_t i = 0; i < argument_count; i++) {
                    dm_node_free(parser->ctx, arguments[i]);
                }
                dm_free(parser->ctx, arguments);
                return NULL;
            }
            
            node->call.name = name;
            node->call.args = arguments;
            node->call.arg_count = argument_count;
            
            return node;
        } else {
            // This is a variable reference
            dm_node_t* node = create_node(parser->ctx, DM_NODE_VARIABLE);
            if (node == NULL) {
                dm_free(parser->ctx, name);
                return NULL;
            }
            
            node->variable.name = name;
            return node;
        }
    }
    
    // Parse grouped expression (parentheses)
    if (match_symbol(parser, '(')) {
        if (consume(parser) != DM_SUCCESS) {
            return NULL;
        }
        
        dm_node_t* expr = parse_expression(parser);
        if (expr == NULL) {
            return NULL;
        }
        
        if (!match_symbol(parser, ')')) {
            report_error(parser, "Expected ')' after expression");
            dm_node_free(parser->ctx, expr);
            return NULL;
        }
        
        if (consume(parser) != DM_SUCCESS) {
            dm_node_free(parser->ctx, expr);
            return NULL;
        }
        
        return expr;
    }
    
    report_error(parser, "Expected expression");
    return NULL;
}

// Get operator precedence
static int get_binary_precedence(char op) {
    switch (op) {
        case '+':
        case '-':
            return 1;
        case '*':
        case '/':
        case '%':
            return 2;
        default:
            return 0;
    }
}

// Get operator type
static dm_operator_t get_binary_operator(char op) {
    switch (op) {
        case '+': return DM_OP_ADD;
        case '-': return DM_OP_SUB;
        case '*': return DM_OP_MUL;
        case '/': return DM_OP_DIV;
        case '%': return DM_OP_MOD;
        default:  return (dm_operator_t)-1;
    }
}

// Parse a binary expression with precedence climbing
static dm_node_t* parse_binary(dm_parser_t *parser, int precedence) {
    dm_node_t *left = parse_unary(parser);
    if (left == NULL) {
        return NULL;
    }
    
    while (match(parser, DM_TOKEN_OPERATOR) && 
           get_binary_precedence(parser->current.text[0]) >= precedence) {
        
        // Get operator
        char op_char = parser->current.text[0];
        dm_operator_t op = get_binary_operator(op_char);
        int next_precedence = get_binary_precedence(op_char) + 1;
        
        // Consume operator
        if (consume(parser) != DM_SUCCESS) {
            dm_node_free(parser->ctx, left);
            return NULL;
        }
        
        // Parse right operand with higher precedence
        dm_node_t *right = parse_binary(parser, next_precedence);
        if (right == NULL) {
            dm_node_free(parser->ctx, left);
            return NULL;
        }
        
        // Create binary node
        dm_node_t *binary = create_node(parser->ctx, DM_NODE_BINARY_OP);
        if (binary == NULL) {
            dm_node_free(parser->ctx, left);
            dm_node_free(parser->ctx, right);
            return NULL;
        }
        
        binary->binary.op = op;
        binary->binary.left = left;
        binary->binary.right = right;
        
        left = binary;
    }
    
    return left;
}

// Parse a unary expression
static dm_node_t* parse_unary(dm_parser_t *parser) {
    // Check for unary operators
    if ((match(parser, DM_TOKEN_OPERATOR) && parser->current.text[0] == '-') || 
        (match(parser, DM_TOKEN_OPERATOR) && parser->current.text[0] == '!')) {
        dm_operator_t op = parser->current.text[0] == '-' ? DM_OP_NEG : DM_OP_NOT;
        
        // Consume operator
        if (consume(parser) != DM_SUCCESS) {
            return NULL;
        }
        
        // Parse operand
        dm_node_t *operand = parse_unary(parser);
        if (operand == NULL) {
            return NULL;
        }
        
        // Create unary node
        dm_node_t *unary = create_node(parser->ctx, DM_NODE_UNARY_OP);
        if (unary == NULL) {
            dm_node_free(parser->ctx, operand);
            return NULL;
        }
        
        unary->unary.op = op;
        unary->unary.operand = operand;
        
        return unary;
    }
    
    // Not a unary expression, try primary
    return parse_primary(parser);
}

// Parse a block of statements
static dm_node_t* parse_block(dm_parser_t *parser) {
    // Expect '{'
    if (!match_symbol(parser, '{')) {
        report_error(parser, "Expected '{' to begin block");
        return NULL;
    }
    
    // Consume '{'
    if (consume(parser) != DM_SUCCESS) {
        return NULL;
    }
    
    // Create block node
    dm_node_t *node = create_node(parser->ctx, DM_NODE_BLOCK);
    if (node == NULL) {
        return NULL;
    }
    
    // Allocate initial capacity for statements
    size_t capacity = 8;
    node->block.statements = dm_malloc(parser->ctx, capacity * sizeof(dm_node_t*));
    if (node->block.statements == NULL) {
        dm_free(parser->ctx, node);
        return NULL;
    }
    
    node->block.count = 0;
    node->block.capacity = capacity;
    
    // Parse statements until we hit '}'
    while (!match_symbol(parser, '}')) {
        // Check for EOF
        if (match(parser, DM_TOKEN_EOF)) {
            report_error(parser, "Unexpected end of file, expected '}'");
            
            // Free statements and node
            for (size_t i = 0; i < node->block.count; i++) {
                dm_node_free(parser->ctx, node->block.statements[i]);
            }
            dm_free(parser->ctx, node->block.statements);
            dm_free(parser->ctx, node);
            
            return NULL;
        }
        
        // Parse a statement
        dm_node_t *stmt = parse_statement(parser);
        if (stmt == NULL) {
            // Error occurred, free statements and node
            for (size_t i = 0; i < node->block.count; i++) {
                dm_node_free(parser->ctx, node->block.statements[i]);
            }
            dm_free(parser->ctx, node->block.statements);
            dm_free(parser->ctx, node);
            
            return NULL;
        }
        
        // Resize statements array if necessary
        if (node->block.count >= node->block.capacity) {
            size_t new_capacity = node->block.capacity * 2;
            dm_node_t **new_stmts = dm_realloc(parser->ctx, node->block.statements,
                                             new_capacity * sizeof(dm_node_t*));
            
            if (new_stmts == NULL) {
                // Allocation failed, clean up
                for (size_t i = 0; i < node->block.count; i++) {
                    dm_node_free(parser->ctx, node->block.statements[i]);
                }
                dm_free(parser->ctx, node->block.statements);
                dm_free(parser->ctx, node);
                return NULL;
            }
            
            node->block.statements = new_stmts;
            node->block.capacity = new_capacity;
        }
        
        // Add statement to block
        node->block.statements[node->block.count++] = stmt;
    }
    
    // Consume the '}'
    if (consume(parser) != DM_SUCCESS) {
        // Error occurred, free statements and node
        for (size_t i = 0; i < node->block.count; i++) {
            dm_node_free(parser->ctx, node->block.statements[i]);
        }
        dm_free(parser->ctx, node->block.statements);
        dm_free(parser->ctx, node);
        
        return NULL;
    }
    
    return node;
}

// Parse an if statement
static dm_node_t* parse_if(dm_parser_t *parser) {
    // Expect 'if'
    if (!match_keyword(parser, "if")) {
        report_error(parser, "Expected 'if'");
        return NULL;
    }
    
    // Consume 'if'
    if (consume(parser) != DM_SUCCESS) {
        return NULL;
    }
    
    // Expect '('
    if (!match_symbol(parser, '(')) {
        report_error(parser, "Expected '(' after 'if'");
        return NULL;
    }
    
    // Consume '('
    if (consume(parser) != DM_SUCCESS) {
        return NULL;
    }
    
    // Parse condition
    dm_node_t *condition = parse_expression(parser);
    if (condition == NULL) {
        return NULL;
    }
    
    // Expect ')'
    if (!match_symbol(parser, ')')) {
        report_error(parser, "Expected ')' after condition");
        dm_node_free(parser->ctx, condition);
        return NULL;
    }
    
    // Consume ')'
    if (consume(parser) != DM_SUCCESS) {
        dm_node_free(parser->ctx, condition);
        return NULL;
    }
    
    // Parse then branch
    dm_node_t *then_branch = parse_statement(parser);
    if (then_branch == NULL) {
        dm_node_free(parser->ctx, condition);
        return NULL;
    }
    
    // Check for 'else'
    dm_node_t *else_branch = NULL;
    if (match_keyword(parser, "else")) {
        // Consume 'else'
        if (consume(parser) != DM_SUCCESS) {
            dm_node_free(parser->ctx, condition);
            dm_node_free(parser->ctx, then_branch);
            return NULL;
        }
        
        // Parse else branch
        else_branch = parse_statement(parser);
        if (else_branch == NULL) {
            dm_node_free(parser->ctx, condition);
            dm_node_free(parser->ctx, then_branch);
            return NULL;
        }
    }
    
    // Create if node
    dm_node_t *node = create_node(parser->ctx, DM_NODE_IF);
    if (node == NULL) {
        dm_node_free(parser->ctx, condition);
        dm_node_free(parser->ctx, then_branch);
        if (else_branch != NULL) {
            dm_node_free(parser->ctx, else_branch);
        }
        return NULL;
    }
    
    node->if_stmt.condition = condition;
    node->if_stmt.then_branch = then_branch;
    node->if_stmt.else_branch = else_branch;
    
    return node;
}

// Parse a while loop
static dm_node_t* parse_while(dm_parser_t *parser) {
    if (parser == NULL) {
        return NULL;
    }
    
    // Expect opening parenthesis
    if (!match_symbol(parser, '(')) {
        report_error(parser, "Expected '(' after 'while'");
        return NULL;
    }
    
    if (consume(parser) != DM_SUCCESS) {
        return NULL;
    }
    
    // Parse condition
    dm_node_t *condition = parse_expression(parser);
    if (condition == NULL) {
        report_error(parser, "Expected condition in while loop");
        return NULL;
    }
    
    // Expect closing parenthesis
    if (!match_symbol(parser, ')')) {
        report_error(parser, "Expected ')' after while condition");
        dm_node_free(parser->ctx, condition);
        return NULL;
    }
    
    if (consume(parser) != DM_SUCCESS) {
        dm_node_free(parser->ctx, condition);
        return NULL;
    }
    
    // Parse body
    dm_node_t *body = parse_statement(parser);
    if (body == NULL) {
        dm_node_free(parser->ctx, condition);
        report_error(parser, "Expected body for while loop");
        return NULL;
    }
    
    // Create while node
    dm_node_t *node = create_node(parser->ctx, DM_NODE_WHILE);
    if (node == NULL) {
        dm_node_free(parser->ctx, condition);
        dm_node_free(parser->ctx, body);
        return NULL;
    }
    
    node->while_loop.condition = condition;
    node->while_loop.body = body;
    
    return node;
}

// Parse a function
static dm_node_t* parse_function(dm_parser_t *parser) {
    if (parser == NULL) {
        return NULL;
    }
    
    // Get function name (identifier)
    if (!match(parser, DM_TOKEN_IDENTIFIER)) {
        report_error(parser, "Expected function name after 'function' keyword");
        return NULL;
    }
    
    char *name = strdup(parser->current.text);
    if (name == NULL) {
        report_error(parser, "Failed to allocate memory for function name");
        return NULL;
    }
    
    // Consume function name
    if (consume(parser) != DM_SUCCESS) {
        free(name);
        return NULL;
    }
    
    // Expect opening parenthesis for parameters
    if (!match_symbol(parser, '(')) {
        report_error(parser, "Expected '(' after function name");
        free(name);
        return NULL;
    }
    
    if (consume(parser) != DM_SUCCESS) {
        free(name);
        return NULL;
    }
    
    // Parse parameters
    char **parameters = NULL;
    size_t parameter_count = 0;
    size_t parameter_capacity = 0;
    
    // Parse parameter list
    while (!match_symbol(parser, ')')) {
        // Check for comma between parameters
        if (parameter_count > 0) {
            if (!match_symbol(parser, ',')) {
                report_error(parser, "Expected ',' between parameters");
                free(name);
                for (size_t i = 0; i < parameter_count; i++) {
                    free(parameters[i]);
                }
                free(parameters);
                return NULL;
            }
            
            if (consume(parser) != DM_SUCCESS) {
                free(name);
                for (size_t i = 0; i < parameter_count; i++) {
                    free(parameters[i]);
                }
                free(parameters);
                return NULL;
            }
        }
        
        // Get parameter name (identifier)
        if (!match(parser, DM_TOKEN_IDENTIFIER)) {
            report_error(parser, "Expected parameter name");
            free(name);
            for (size_t i = 0; i < parameter_count; i++) {
                free(parameters[i]);
            }
            free(parameters);
            return NULL;
        }
        
        // Allocate or resize parameters array if needed
        if (parameter_count >= parameter_capacity) {
            size_t new_capacity = parameter_capacity == 0 ? 4 : parameter_capacity * 2;
            char **new_parameters = realloc(parameters, new_capacity * sizeof(char *));
            if (new_parameters == NULL) {
                report_error(parser, "Failed to allocate memory for parameters");
                free(name);
                for (size_t i = 0; i < parameter_count; i++) {
                    free(parameters[i]);
                }
                free(parameters);
                return NULL;
            }
            parameters = new_parameters;
            parameter_capacity = new_capacity;
        }
        
        // Store parameter name
        parameters[parameter_count] = strdup(parser->current.text);
        if (parameters[parameter_count] == NULL) {
            report_error(parser, "Failed to allocate memory for parameter name");
            free(name);
            for (size_t i = 0; i < parameter_count; i++) {
                free(parameters[i]);
            }
            free(parameters);
            return NULL;
        }
        parameter_count++;
        
        // Consume parameter name
        if (consume(parser) != DM_SUCCESS) {
            free(name);
            for (size_t i = 0; i < parameter_count; i++) {
                free(parameters[i]);
            }
            free(parameters);
            return NULL;
        }
    }
    
    // Consume closing parenthesis
    if (consume(parser) != DM_SUCCESS) {
        free(name);
        for (size_t i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        return NULL;
    }
    
    // Parse function body
    dm_node_t *body = parse_statement(parser);
    if (body == NULL) {
        report_error(parser, "Expected function body");
        free(name);
        for (size_t i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        return NULL;
    }
    
    // Create function node
    dm_node_t *node = create_node(parser->ctx, DM_NODE_FUNCTION);
    if (node == NULL) {
        free(name);
        for (size_t i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        dm_node_free(parser->ctx, body);
        return NULL;
    }
    
    node->function.name = name;
    node->function.params = parameters;
    node->function.param_count = parameter_count;
    node->function.body = body;
    
    return node;
}

// Parse a return statement
static dm_node_t* parse_return(dm_parser_t *parser) {
    if (parser == NULL) {
        return NULL;
    }
    
    // Consume the 'return' keyword
    if (consume(parser) != DM_SUCCESS) {
        return NULL;
    }
    
    // Return statement can have an optional expression
    dm_node_t *value = NULL;
    
    // Check if there's an expression or just a semicolon
    if (!match_symbol(parser, ';')) {
        // Parse the return value
        value = parse_expression(parser);
        if (value == NULL) {
            report_error(parser, "Expected expression after 'return'");
            return NULL;
        }
    }
    
    // Expect semicolon
    if (!match_symbol(parser, ';')) {
        report_error(parser, "Expected ';' after return statement");
        if (value != NULL) {
            dm_node_free(parser->ctx, value);
        }
        return NULL;
    }
    
    if (consume(parser) != DM_SUCCESS) {
        if (value != NULL) {
            dm_node_free(parser->ctx, value);
        }
        return NULL;
    }
    
    // Create return node
    dm_node_t *node = create_node(parser->ctx, DM_NODE_RETURN);
    if (node == NULL) {
        if (value != NULL) {
            dm_node_free(parser->ctx, value);
        }
        return NULL;
    }
    
    node->return_stmt.value = value;
    
    return node;
}

// Main parse function
dm_error_t dm_parser_parse(dm_parser_t *parser, dm_node_t **result) {
    if (parser == NULL || result == NULL) {
        DM_DEBUG_ERROR_PRINT("Invalid arguments to dm_parser_parse\n");
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    DM_DEBUG_INFO_PRINT("Starting to parse...\n");
    
    // Parse the program
    dm_node_t *program = parse_program(parser);
    
    if (program == NULL) {
        DM_DEBUG_ERROR_PRINT("Parse error: %s\n", parser->error_message);
        return DM_ERROR_SYNTAX_ERROR;
    }
    
    // Set the result
    *result = program;
    
    DM_DEBUG_INFO_PRINT("Parsing completed successfully.\n");
    return DM_SUCCESS;
}

// Free a node and its children
void dm_node_free(dm_context_t *ctx, dm_node_t *node) {
    if (ctx == NULL || node == NULL) {
        return;
    }
    
    // Free children based on node type
    switch (node->type) {
        case DM_NODE_PROGRAM:
            // Free all statements
            for (size_t i = 0; i < node->program.count; i++) {
                dm_node_free(ctx, node->program.statements[i]);
            }
            dm_free(ctx, node->program.statements);
            break;
            
        case DM_NODE_BLOCK:
            // Free all statements
            for (size_t i = 0; i < node->block.count; i++) {
                dm_node_free(ctx, node->block.statements[i]);
            }
            dm_free(ctx, node->block.statements);
            break;
            
        case DM_NODE_VARIABLE:
            // Free variable name
            dm_free(ctx, node->variable.name);
            break;
            
        case DM_NODE_ASSIGNMENT:
            // Free target and value
            dm_free(ctx, node->assignment.name);
            dm_node_free(ctx, node->assignment.value);
            break;
            
        case DM_NODE_BINARY_OP:
            // Free operands
            dm_node_free(ctx, node->binary.left);
            dm_node_free(ctx, node->binary.right);
            break;
            
        case DM_NODE_UNARY_OP:
            // Free operand
            dm_node_free(ctx, node->unary.operand);
            break;
            
        case DM_NODE_CALL:
            // Free callee and arguments
            dm_free(ctx, node->call.name);
            for (size_t i = 0; i < node->call.arg_count; i++) {
                dm_node_free(ctx, node->call.args[i]);
            }
            dm_free(ctx, node->call.args);
            break;
            
        case DM_NODE_IF:
            // Free condition and branches
            dm_node_free(ctx, node->if_stmt.condition);
            dm_node_free(ctx, node->if_stmt.then_branch);
            if (node->if_stmt.else_branch != NULL) {
                dm_node_free(ctx, node->if_stmt.else_branch);
            }
            break;
            
        case DM_NODE_WHILE:
            // Free condition and body
            dm_node_free(ctx, node->while_loop.condition);
            dm_node_free(ctx, node->while_loop.body);
            break;
            
        case DM_NODE_FOR:
            // Free init, condition, increment, and body
            if (node->for_loop.init != NULL) {
                dm_node_free(ctx, node->for_loop.init);
            }
            if (node->for_loop.condition != NULL) {
                dm_node_free(ctx, node->for_loop.condition);
            }
            if (node->for_loop.increment != NULL) {
                dm_node_free(ctx, node->for_loop.increment);
            }
            dm_node_free(ctx, node->for_loop.body);
            break;
            
        case DM_NODE_FUNCTION:
            // Free name, parameters, and body
            dm_free(ctx, node->function.name);
            for (size_t i = 0; i < node->function.param_count; i++) {
                dm_free(ctx, node->function.params[i]);
            }
            dm_free(ctx, node->function.params);
            dm_node_free(ctx, node->function.body);
            break;
            
        case DM_NODE_RETURN:
            // Free value
            if (node->return_stmt.value != NULL) {
                dm_node_free(ctx, node->return_stmt.value);
            }
            break;
            
        case DM_NODE_IMPORT:
            // Free module name
            dm_free(ctx, node->import.module);
            break;
            
        case DM_NODE_LITERAL:
            // Free string if needed
            if (node->literal.type == DM_LITERAL_STRING && node->literal.value.string != NULL) {
                dm_free(ctx, node->literal.value.string);
            }
            break;
    }
    
    // Free the node itself
    dm_free(ctx, node);
} 