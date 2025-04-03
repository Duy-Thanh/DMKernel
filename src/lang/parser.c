#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../include/lang/parser.h"

// Initialize parser
dm_error_t dm_parser_init(dm_context_t *ctx, dm_parser_t *parser, const char *source, size_t source_len) {
    if (ctx == NULL || parser == NULL || source == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
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
        // Parse string
        size_t len = parser->current.length;
        char *str = dm_malloc(parser->ctx, len + 1);
        if (str == NULL) {
            dm_free(parser->ctx, node);
            return NULL;
        }
        
        // Copy string content
        strncpy(str, parser->current.text, len);
        str[len] = '\0';
        
        node->literal.type = DM_LITERAL_STRING;
        node->literal.value.string = str;
        
        // Consume the string token
        if (consume(parser) != DM_SUCCESS) {
            dm_free(parser->ctx, node->literal.value.string);
            dm_free(parser->ctx, node);
            return NULL;
        }
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

// Parse a statement (dummy implementation for now)
static dm_node_t* parse_statement(dm_parser_t *parser) {
    // For now, just treat all literals as statements
    dm_node_t* expr = parse_expression(parser);
    
    if (expr == NULL) {
        return NULL;
    }
    
    // Expect a semicolon at the end of the statement
    if (!match_symbol(parser, ';')) {
        report_error(parser, "Expected ';' after expression");
        if (expr) dm_node_free(parser->ctx, expr);
        return NULL;
    }
    
    // Consume the semicolon
    if (consume(parser) != DM_SUCCESS) {
        if (expr) dm_node_free(parser->ctx, expr);
        return NULL;
    }
    
    return expr;
}

// Parse an expression
static dm_node_t* parse_expression(dm_parser_t *parser) {
    // Start with lowest precedence
    return parse_binary(parser, 1);
}

// Parse a primary expression (literals, variables, grouping)
static dm_node_t* parse_primary(dm_parser_t *parser) {
    // Try to parse a literal first
    dm_node_t *node = parse_literal(parser);
    if (node != NULL) {
        return node;
    }
    
    // Try to parse a variable
    node = parse_variable(parser);
    if (node != NULL) {
        return node;
    }
    
    // Try to parse a grouped expression
    if (match_symbol(parser, '(')) {
        if (consume(parser) != DM_SUCCESS) {
            return NULL;
        }
        
        dm_node_t *expr = parse_expression(parser);
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
    
    // Nothing matched
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

// Main parse function
dm_error_t dm_parser_parse(dm_parser_t *parser, dm_node_t **root) {
    if (parser == NULL || root == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Parse program
    *root = parse_program(parser);
    
    // Return success if we reached EOF cleanly, or error otherwise
    return (*root != NULL) ? DM_SUCCESS : DM_ERROR_SYNTAX_ERROR;
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