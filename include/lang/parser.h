#ifndef _DM_PARSER_H
#define _DM_PARSER_H

#include "../dmkernel.h"

// Token types
typedef enum {
    DM_TOKEN_EOF,
    DM_TOKEN_IDENTIFIER,
    DM_TOKEN_KEYWORD,
    DM_TOKEN_NUMBER,
    DM_TOKEN_STRING,
    DM_TOKEN_OPERATOR,
    DM_TOKEN_SYMBOL
} dm_token_type_t;

// Token structure
typedef struct {
    dm_token_type_t type;
    char *text;
    size_t length;
    size_t line;
    size_t column;
} dm_token_t;

// Lexer structure
typedef struct {
    dm_context_t *ctx;
    const char *source;
    size_t source_len;
    size_t position;
    size_t line;
    size_t column;
    dm_token_t current;
} dm_lexer_t;

// Parser structure
typedef struct {
    dm_context_t *ctx;
    dm_lexer_t lexer;
    dm_token_t current;
    char error_message[256];
} dm_parser_t;

// AST node types
typedef enum {
    DM_NODE_PROGRAM,
    DM_NODE_LITERAL,
    DM_NODE_BINARY_OP,
    DM_NODE_UNARY_OP,
    DM_NODE_VARIABLE,
    DM_NODE_ASSIGNMENT,
    DM_NODE_BLOCK,
    DM_NODE_IF,
    DM_NODE_WHILE,
    DM_NODE_FOR,
    DM_NODE_CALL,
    DM_NODE_FUNCTION,
    DM_NODE_RETURN,
    DM_NODE_IMPORT
} dm_node_type_t;

// Literal types
typedef enum {
    DM_LITERAL_NUMBER,
    DM_LITERAL_STRING,
    DM_LITERAL_BOOLEAN,
    DM_LITERAL_NULL
} dm_literal_type_t;

// Operator types
typedef enum {
    // Binary operators
    DM_OP_ADD,  // +
    DM_OP_SUB,  // -
    DM_OP_MUL,  // *
    DM_OP_DIV,  // /
    DM_OP_MOD,  // %
    
    // Comparison operators
    DM_OP_EQ,   // ==
    DM_OP_NEQ,  // !=
    DM_OP_LT,   // <
    DM_OP_GT,   // >
    DM_OP_LTE,  // <=
    DM_OP_GTE,  // >=
    
    // Logical operators
    DM_OP_AND,  // &&
    DM_OP_OR,   // ||
    
    // Unary operators
    DM_OP_NEG,  // -
    DM_OP_NOT   // !
} dm_operator_t;

// Forward declaration for dm_node
typedef struct dm_node dm_node_t;

// Node structures for different node types
typedef struct {
    dm_node_t **statements;
    size_t count;
    size_t capacity;
} dm_program_node_t;

typedef struct {
    dm_literal_type_t type;
    union {
        double number;
        char *string;
        bool boolean;
    } value;
} dm_literal_node_t;

typedef struct {
    dm_operator_t op;
    dm_node_t *left;
    dm_node_t *right;
} dm_binary_node_t;

typedef struct {
    dm_operator_t op;
    dm_node_t *operand;
} dm_unary_node_t;

typedef struct {
    char *name;
} dm_variable_node_t;

typedef struct {
    char *name;
    dm_node_t *value;
    bool is_declaration;
} dm_assignment_node_t;

typedef struct {
    dm_node_t **statements;
    size_t count;
    size_t capacity;
} dm_block_node_t;

typedef struct {
    dm_node_t *condition;
    dm_node_t *then_branch;
    dm_node_t *else_branch;
} dm_if_node_t;

typedef struct {
    dm_node_t *condition;
    dm_node_t *body;
} dm_while_node_t;

typedef struct {
    dm_node_t *init;
    dm_node_t *condition;
    dm_node_t *increment;
    dm_node_t *body;
} dm_for_node_t;

typedef struct {
    char *name;
    dm_node_t **args;
    size_t arg_count;
} dm_call_node_t;

typedef struct {
    char *name;
    char **params;
    size_t param_count;
    dm_node_t *body;
} dm_function_node_t;

typedef struct {
    dm_node_t *value;
} dm_return_node_t;

typedef struct {
    char *module;
} dm_import_node_t;

// AST node structure
struct dm_node {
    dm_node_type_t type;
    size_t line;
    size_t column;
    
    union {
        dm_program_node_t program;
        dm_literal_node_t literal;
        dm_binary_node_t binary;
        dm_unary_node_t unary;
        dm_variable_node_t variable;
        dm_assignment_node_t assignment;
        dm_block_node_t block;
        dm_if_node_t if_stmt;
        dm_while_node_t while_loop;
        dm_for_node_t for_loop;
        dm_call_node_t call;
        dm_function_node_t function;
        dm_return_node_t return_stmt;
        dm_import_node_t import;
    };
};

// Lexer functions
dm_error_t dm_lexer_init(dm_context_t *ctx, dm_lexer_t *lexer, const char *source, size_t source_len);
dm_error_t dm_lexer_next_token(dm_lexer_t *lexer, dm_token_t *token);

// Parser functions
dm_error_t dm_parser_init(dm_context_t *ctx, dm_parser_t *parser, const char *source, size_t source_len);
dm_error_t dm_parser_parse(dm_parser_t *parser, dm_node_t **root);
void dm_node_free(dm_context_t *ctx, dm_node_t *node);

// Execution functions
dm_error_t dm_eval_node(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);
dm_error_t dm_execute_source(dm_context_t *ctx, const char *source, size_t source_len, dm_node_t **result);
dm_error_t dm_node_to_string(dm_context_t *ctx, dm_node_t *node, char **str);

// Helper function
char *dm_strdup(dm_context_t *ctx, const char *str);

#endif /* _DM_PARSER_H */ 