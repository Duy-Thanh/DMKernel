#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/dmkernel.h"
#include "../../include/lang/exec.h"
#include "../../include/lang/parser.h"
#include "../../include/core/filesystem.h"

// Helper function to create a new node
static dm_node_t *create_result_node(dm_context_t *ctx, dm_node_type_t type) {
    dm_node_t *node = dm_malloc(ctx, sizeof(dm_node_t));
    if (node == NULL) {
        return NULL;
    }
    
    node->type = type;
    return node;
}

// Forward declarations for recursive functions
static dm_error_t eval_literal(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);
static dm_error_t eval_binary(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);
static dm_error_t eval_unary(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);
static dm_error_t eval_variable(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);
static dm_error_t eval_assignment(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);
static dm_error_t eval_block(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);
static dm_error_t eval_if(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);
static dm_error_t eval_while(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);
static dm_error_t eval_function_call(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);
static dm_error_t eval_function_declaration(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);
static dm_error_t eval_return(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);
static dm_error_t eval_program(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);

dm_error_t dm_eval_node(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    if (ctx == NULL || node == NULL || result == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Evaluate based on node type
    switch (node->type) {
        case DM_NODE_LITERAL:
            return eval_literal(ctx, node, result);
            
        case DM_NODE_BINARY_OP:
            return eval_binary(ctx, node, result);
            
        case DM_NODE_UNARY_OP:
            return eval_unary(ctx, node, result);
            
        case DM_NODE_VARIABLE:
            return eval_variable(ctx, node, result);
            
        case DM_NODE_ASSIGNMENT:
            return eval_assignment(ctx, node, result);
            
        case DM_NODE_BLOCK:
            return eval_block(ctx, node, result);
            
        case DM_NODE_IF:
            return eval_if(ctx, node, result);
            
        case DM_NODE_WHILE:
            return eval_while(ctx, node, result);
            
        case DM_NODE_CALL:
            return eval_function_call(ctx, node, result);
            
        case DM_NODE_FUNCTION:
            return eval_function_declaration(ctx, node, result);
            
        case DM_NODE_RETURN:
            return eval_return(ctx, node, result);
            
        case DM_NODE_PROGRAM:
            return eval_program(ctx, node, result);
            
        default:
            return DM_ERROR_INVALID_ARGUMENT;
    }
}

// Evaluate a literal value (just returns a copy of itself)
static dm_error_t eval_literal(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    dm_node_t *res = create_result_node(ctx, DM_NODE_LITERAL);
    if (res == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Copy the literal value
    res->literal.type = node->literal.type;
    
    switch (node->literal.type) {
        case DM_LITERAL_NUMBER:
            res->literal.value.number = node->literal.value.number;
            break;
            
        case DM_LITERAL_STRING:
            res->literal.value.string = dm_strdup(ctx, node->literal.value.string);
            if (res->literal.value.string == NULL) {
                dm_free(ctx, res);
                return DM_ERROR_MEMORY_ALLOCATION;
            }
            break;
            
        case DM_LITERAL_BOOLEAN:
            res->literal.value.boolean = node->literal.value.boolean;
            break;
            
        case DM_LITERAL_NULL:
            // No value to copy
            break;
            
        default:
            dm_free(ctx, res);
            return DM_ERROR_INVALID_ARGUMENT;
    }
    
    *result = res;
    return DM_SUCCESS;
}

// Evaluate a binary expression (arithmetics, comparisons, logical operations)
static dm_error_t eval_binary(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    // Get left and right values
    dm_node_t *left_result = NULL;
    dm_node_t *right_result = NULL;
    dm_error_t err;
    
    err = dm_eval_node(ctx, node->binary.left, &left_result);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    err = dm_eval_node(ctx, node->binary.right, &right_result);
    if (err != DM_SUCCESS) {
        dm_node_free(ctx, left_result);
        return err;
    }
    
    // Create result node
    dm_node_t *res = create_result_node(ctx, DM_NODE_LITERAL);
    if (res == NULL) {
        dm_node_free(ctx, left_result);
        dm_node_free(ctx, right_result);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Only number literals can be used in arithmetic operations
    if ((node->binary.op == DM_OP_ADD || node->binary.op == DM_OP_SUB ||
         node->binary.op == DM_OP_MUL || node->binary.op == DM_OP_DIV) &&
        (left_result->type != DM_NODE_LITERAL || left_result->literal.type != DM_LITERAL_NUMBER ||
         right_result->type != DM_NODE_LITERAL || right_result->literal.type != DM_LITERAL_NUMBER)) {
        dm_node_free(ctx, left_result);
        dm_node_free(ctx, right_result);
        dm_free(ctx, res);
        return DM_ERROR_TYPE_MISMATCH;
    }
    
    // Handle specific operations
    switch (node->binary.op) {
        case DM_OP_ADD:
            res->literal.type = DM_LITERAL_NUMBER;
            res->literal.value.number = left_result->literal.value.number + right_result->literal.value.number;
            break;
            
        case DM_OP_SUB:
            res->literal.type = DM_LITERAL_NUMBER;
            res->literal.value.number = left_result->literal.value.number - right_result->literal.value.number;
            break;
            
        case DM_OP_MUL:
            res->literal.type = DM_LITERAL_NUMBER;
            res->literal.value.number = left_result->literal.value.number * right_result->literal.value.number;
            break;
            
        case DM_OP_DIV:
            // Check for division by zero
            if (right_result->literal.value.number == 0) {
                dm_node_free(ctx, left_result);
                dm_node_free(ctx, right_result);
                dm_free(ctx, res);
                return DM_ERROR_DIVISION_BY_ZERO;
            }
            res->literal.type = DM_LITERAL_NUMBER;
            res->literal.value.number = left_result->literal.value.number / right_result->literal.value.number;
            break;
            
        case DM_OP_EQ:
            res->literal.type = DM_LITERAL_BOOLEAN;
            // Only compare same types
            if (left_result->type != right_result->type || 
                left_result->literal.type != right_result->literal.type) {
                res->literal.value.boolean = false;
            } else {
                switch (left_result->literal.type) {
                    case DM_LITERAL_NUMBER:
                        res->literal.value.boolean = left_result->literal.value.number == right_result->literal.value.number;
                        break;
                    case DM_LITERAL_STRING:
                        res->literal.value.boolean = strcmp(left_result->literal.value.string, right_result->literal.value.string) == 0;
                        break;
                    case DM_LITERAL_BOOLEAN:
                        res->literal.value.boolean = left_result->literal.value.boolean == right_result->literal.value.boolean;
                        break;
                    case DM_LITERAL_NULL:
                        res->literal.value.boolean = true; // null == null is always true
                        break;
                    default:
                        res->literal.value.boolean = false;
                }
            }
            break;
            
        case DM_OP_NEQ:
            res->literal.type = DM_LITERAL_BOOLEAN;
            // Only compare same types
            if (left_result->type != right_result->type || 
                left_result->literal.type != right_result->literal.type) {
                res->literal.value.boolean = true;
            } else {
                switch (left_result->literal.type) {
                    case DM_LITERAL_NUMBER:
                        res->literal.value.boolean = left_result->literal.value.number != right_result->literal.value.number;
                        break;
                    case DM_LITERAL_STRING:
                        res->literal.value.boolean = strcmp(left_result->literal.value.string, right_result->literal.value.string) != 0;
                        break;
                    case DM_LITERAL_BOOLEAN:
                        res->literal.value.boolean = left_result->literal.value.boolean != right_result->literal.value.boolean;
                        break;
                    case DM_LITERAL_NULL:
                        res->literal.value.boolean = false; // null != null is always false
                        break;
                    default:
                        res->literal.value.boolean = true;
                }
            }
            break;
            
        // Add more operations like GT, LT, AND, OR etc.
        // (Abbreviated for now)
            
        default:
            dm_node_free(ctx, left_result);
            dm_node_free(ctx, right_result);
            dm_free(ctx, res);
            return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Free temporary results
    dm_node_free(ctx, left_result);
    dm_node_free(ctx, right_result);
    
    *result = res;
    return DM_SUCCESS;
}

// Unary operations (negation, logical not)
static dm_error_t eval_unary(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    // Get operand value
    dm_node_t *operand_result = NULL;
    dm_error_t err = dm_eval_node(ctx, node->unary.operand, &operand_result);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Create result node
    dm_node_t *res = create_result_node(ctx, DM_NODE_LITERAL);
    if (res == NULL) {
        dm_node_free(ctx, operand_result);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Handle specific operations
    switch (node->unary.op) {
        case DM_OP_NEG:
            // Only numbers can be negated
            if (operand_result->type != DM_NODE_LITERAL || 
                operand_result->literal.type != DM_LITERAL_NUMBER) {
                dm_node_free(ctx, operand_result);
                dm_free(ctx, res);
                return DM_ERROR_TYPE_MISMATCH;
            }
            res->literal.type = DM_LITERAL_NUMBER;
            res->literal.value.number = -operand_result->literal.value.number;
            break;
            
        case DM_OP_NOT:
            // Only booleans can be logically negated
            if (operand_result->type != DM_NODE_LITERAL || 
                operand_result->literal.type != DM_LITERAL_BOOLEAN) {
                dm_node_free(ctx, operand_result);
                dm_free(ctx, res);
                return DM_ERROR_TYPE_MISMATCH;
            }
            res->literal.type = DM_LITERAL_BOOLEAN;
            res->literal.value.boolean = !operand_result->literal.value.boolean;
            break;
            
        default:
            dm_node_free(ctx, operand_result);
            dm_free(ctx, res);
            return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Free temporary result
    dm_node_free(ctx, operand_result);
    
    *result = res;
    return DM_SUCCESS;
}

// Variable reference
static dm_error_t eval_variable(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    // TODO: Implement variable lookup in environment
    // For now, just return a NULL node
    dm_node_t *res = create_result_node(ctx, DM_NODE_LITERAL);
    if (res == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    res->literal.type = DM_LITERAL_NULL;
    *result = res;
    
    return DM_SUCCESS;
}

// Variable assignment
static dm_error_t eval_assignment(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    // TODO: Implement variable assignment in environment
    // For now, just evaluate the value and return it
    
    return dm_eval_node(ctx, node->assignment.value, result);
}

// Block of statements
static dm_error_t eval_block(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    // TODO: Implement statement block with local scope
    // For now, just return NULL
    
    dm_node_t *res = create_result_node(ctx, DM_NODE_LITERAL);
    if (res == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    res->literal.type = DM_LITERAL_NULL;
    *result = res;
    
    return DM_SUCCESS;
}

// If statement
static dm_error_t eval_if(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    // TODO: Implement if statement
    // For now, just return NULL
    
    dm_node_t *res = create_result_node(ctx, DM_NODE_LITERAL);
    if (res == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    res->literal.type = DM_LITERAL_NULL;
    *result = res;
    
    return DM_SUCCESS;
}

// While loop
static dm_error_t eval_while(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    // TODO: Implement while loop
    // For now, just return NULL
    
    dm_node_t *res = create_result_node(ctx, DM_NODE_LITERAL);
    if (res == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    res->literal.type = DM_LITERAL_NULL;
    *result = res;
    
    return DM_SUCCESS;
}

// Function call
static dm_error_t eval_function_call(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    // TODO: Implement function calls
    // For now, just return NULL
    
    dm_node_t *res = create_result_node(ctx, DM_NODE_LITERAL);
    if (res == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    res->literal.type = DM_LITERAL_NULL;
    *result = res;
    
    return DM_SUCCESS;
}

// Function declaration
static dm_error_t eval_function_declaration(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    // TODO: Implement function declaration
    // For now, just return NULL
    
    dm_node_t *res = create_result_node(ctx, DM_NODE_LITERAL);
    if (res == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    res->literal.type = DM_LITERAL_NULL;
    *result = res;
    
    return DM_SUCCESS;
}

// Return statement
static dm_error_t eval_return(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    // TODO: Implement return statement
    // For now, just evaluate the return value
    
    if (node->return_stmt.value) {
        return dm_eval_node(ctx, node->return_stmt.value, result);
    } else {
        dm_node_t *res = create_result_node(ctx, DM_NODE_LITERAL);
        if (res == NULL) {
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        res->literal.type = DM_LITERAL_NULL;
        *result = res;
        
        return DM_SUCCESS;
    }
}

// Program (sequence of statements)
static dm_error_t eval_program(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    dm_node_t *res = NULL;
    dm_error_t err = DM_SUCCESS;
    
    // Execute each statement in the program
    for (size_t i = 0; i < node->program.count; i++) {
        // Free the previous result if it exists
        if (res != NULL) {
            dm_node_free(ctx, res);
            res = NULL;
        }
        
        // Evaluate the current statement
        err = dm_eval_node(ctx, node->program.statements[i], &res);
        if (err != DM_SUCCESS) {
            return err;
        }
    }
    
    // If no statements, return null
    if (res == NULL) {
        res = create_result_node(ctx, DM_NODE_LITERAL);
        if (res == NULL) {
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        res->literal.type = DM_LITERAL_NULL;
    }
    
    *result = res;
    return DM_SUCCESS;
}

// Execute source code
dm_error_t dm_execute_source(dm_context_t *ctx, const char *source, size_t source_len, dm_node_t **result) {
    if (ctx == NULL || source == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Initialize parser
    dm_parser_t parser;
    dm_error_t err = dm_parser_init(ctx, &parser, source, source_len);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Parse the source
    dm_node_t *ast = NULL;
    err = dm_parser_parse(&parser, &ast);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Evaluate the AST
    dm_node_t *eval_result = NULL;
    err = dm_eval_node(ctx, ast, &eval_result);
    
    // Free the AST
    dm_node_free(ctx, ast);
    
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Return the result if requested
    if (result != NULL) {
        *result = eval_result;
    } else {
        dm_node_free(ctx, eval_result);
    }
    
    return DM_SUCCESS;
}

// Convert node to string representation
dm_error_t dm_node_to_string(dm_context_t *ctx, dm_node_t *node, char **str) {
    if (ctx == NULL || node == NULL || str == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Handle based on node type and literal type
    if (node->type == DM_NODE_LITERAL) {
        switch (node->literal.type) {
            case DM_LITERAL_NUMBER: {
                // Convert number to string
                char buffer[64];
                snprintf(buffer, sizeof(buffer), "%f", node->literal.value.number);
                *str = dm_strdup(ctx, buffer);
                if (*str == NULL) {
                    return DM_ERROR_MEMORY_ALLOCATION;
                }
                break;
            }
            
            case DM_LITERAL_STRING:
                // Just copy the string
                *str = dm_strdup(ctx, node->literal.value.string);
                if (*str == NULL) {
                    return DM_ERROR_MEMORY_ALLOCATION;
                }
                break;
                
            case DM_LITERAL_BOOLEAN:
                // Convert boolean to "true" or "false"
                *str = dm_strdup(ctx, node->literal.value.boolean ? "true" : "false");
                if (*str == NULL) {
                    return DM_ERROR_MEMORY_ALLOCATION;
                }
                break;
                
            case DM_LITERAL_NULL:
                // Just return "null"
                *str = dm_strdup(ctx, "null");
                if (*str == NULL) {
                    return DM_ERROR_MEMORY_ALLOCATION;
                }
                break;
                
            default:
                return DM_ERROR_INVALID_ARGUMENT;
        }
    } else {
        // For non-literal nodes, just return a type description
        *str = dm_strdup(ctx, "[non-literal value]");
        if (*str == NULL) {
            return DM_ERROR_MEMORY_ALLOCATION;
        }
    }
    
    return DM_SUCCESS;
} 