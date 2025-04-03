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
    dm_error_t err = DM_SUCCESS;
    
    switch (node->type) {
        case DM_NODE_LITERAL:
            err = eval_literal(ctx, node, result);
            break;
            
        case DM_NODE_BINARY_OP:
            err = eval_binary(ctx, node, result);
            break;
            
        case DM_NODE_UNARY_OP:
            err = eval_unary(ctx, node, result);
            break;
            
        case DM_NODE_VARIABLE:
            err = eval_variable(ctx, node, result);
            break;
            
        case DM_NODE_ASSIGNMENT:
            err = eval_assignment(ctx, node, result);
            break;
            
        case DM_NODE_BLOCK:
            err = eval_block(ctx, node, result);
            break;
            
        case DM_NODE_IF:
            err = eval_if(ctx, node, result);
            break;
            
        case DM_NODE_WHILE:
            err = eval_while(ctx, node, result);
            break;
            
        case DM_NODE_CALL:
            err = eval_function_call(ctx, node, result);
            break;
            
        case DM_NODE_FUNCTION:
            err = eval_function_declaration(ctx, node, result);
            break;
            
        case DM_NODE_RETURN:
            err = eval_return(ctx, node, result);
            break;
            
        case DM_NODE_PROGRAM:
            err = eval_program(ctx, node, result);
            break;
            
        default:
            snprintf(ctx->error_message, sizeof(ctx->error_message), 
                    "Unknown node type: %d", node->type);
            return DM_ERROR_INVALID_ARGUMENT;
    }
    
    return err;
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
    if (ctx == NULL || node == NULL || result == NULL || node->type != DM_NODE_BINARY_OP) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
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
    
    // Handle different types of binary operations
    switch (node->binary.op) {
        case DM_OP_ADD:
        case DM_OP_SUB:
        case DM_OP_MUL:
        case DM_OP_DIV:
        case DM_OP_MOD:
            // Arithmetic operations require numbers
            if (left_result->type != DM_NODE_LITERAL || right_result->type != DM_NODE_LITERAL) {
                snprintf(ctx->error_message, sizeof(ctx->error_message), 
                        "Expected literal values for arithmetic operation");
                dm_node_free(ctx, left_result);
                dm_node_free(ctx, right_result);
                dm_free(ctx, res);
                return DM_ERROR_TYPE_MISMATCH;
            }
            
            // Convert operands to numbers if needed
            double left_num = 0.0, right_num = 0.0;
            bool left_valid = false, right_valid = false;
            
            switch (left_result->literal.type) {
                case DM_LITERAL_NUMBER:
                    left_num = left_result->literal.value.number;
                    left_valid = true;
                    break;
                case DM_LITERAL_BOOLEAN:
                    left_num = left_result->literal.value.boolean ? 1.0 : 0.0;
                    left_valid = true;
                    break;
                default:
                    snprintf(ctx->error_message, sizeof(ctx->error_message), 
                            "Cannot perform arithmetic on non-numeric left operand");
                    break;
            }
            
            switch (right_result->literal.type) {
                case DM_LITERAL_NUMBER:
                    right_num = right_result->literal.value.number;
                    right_valid = true;
                    break;
                case DM_LITERAL_BOOLEAN:
                    right_num = right_result->literal.value.boolean ? 1.0 : 0.0;
                    right_valid = true;
                    break;
                default:
                    snprintf(ctx->error_message, sizeof(ctx->error_message), 
                            "Cannot perform arithmetic on non-numeric right operand");
                    break;
            }
            
            if (!left_valid || !right_valid) {
                dm_node_free(ctx, left_result);
                dm_node_free(ctx, right_result);
                dm_free(ctx, res);
                return DM_ERROR_TYPE_MISMATCH;
            }
            
            // Perform the arithmetic operation
            res->literal.type = DM_LITERAL_NUMBER;
            switch (node->binary.op) {
                case DM_OP_ADD:
                    res->literal.value.number = left_num + right_num;
                    break;
                case DM_OP_SUB:
                    res->literal.value.number = left_num - right_num;
                    break;
                case DM_OP_MUL:
                    res->literal.value.number = left_num * right_num;
                    break;
                case DM_OP_DIV:
                    if (right_num == 0.0) {
                        snprintf(ctx->error_message, sizeof(ctx->error_message), "Division by zero");
                        dm_node_free(ctx, left_result);
                        dm_node_free(ctx, right_result);
                        dm_free(ctx, res);
                        return DM_ERROR_DIVISION_BY_ZERO;
                    }
                    res->literal.value.number = left_num / right_num;
                    break;
                case DM_OP_MOD:
                    if (right_num == 0.0) {
                        snprintf(ctx->error_message, sizeof(ctx->error_message), "Modulo by zero");
                        dm_node_free(ctx, left_result);
                        dm_node_free(ctx, right_result);
                        dm_free(ctx, res);
                        return DM_ERROR_DIVISION_BY_ZERO;
                    }
                    // Use fmod for floating-point modulo
                    res->literal.value.number = fmod(left_num, right_num);
                    break;
                default:
                    // Should never reach here
                    break;
            }
            break;
            
        case DM_OP_EQ:
        case DM_OP_NEQ:
            // Equality operations can compare any types
            res->literal.type = DM_LITERAL_BOOLEAN;
            if (left_result->type != DM_NODE_LITERAL || right_result->type != DM_NODE_LITERAL) {
                // Non-literal values are never equal
                res->literal.value.boolean = (node->binary.op == DM_OP_NEQ);
            } else if (left_result->literal.type != right_result->literal.type) {
                // Different types are never equal
                res->literal.value.boolean = (node->binary.op == DM_OP_NEQ);
            } else {
                // Same types, compare values
                bool equal = false;
                switch (left_result->literal.type) {
                    case DM_LITERAL_NULL:
                        equal = true;  // null always equals null
                        break;
                    case DM_LITERAL_BOOLEAN:
                        equal = left_result->literal.value.boolean == right_result->literal.value.boolean;
                        break;
                    case DM_LITERAL_NUMBER:
                        equal = left_result->literal.value.number == right_result->literal.value.number;
                        break;
                    case DM_LITERAL_STRING:
                        equal = strcmp(left_result->literal.value.string, right_result->literal.value.string) == 0;
                        break;
                }
                res->literal.value.boolean = (node->binary.op == DM_OP_EQ) ? equal : !equal;
            }
            break;
            
        case DM_OP_LT:
        case DM_OP_GT:
        case DM_OP_LTE:
        case DM_OP_GTE:
            // Comparison operations
            res->literal.type = DM_LITERAL_BOOLEAN;
            
            // Operands must be numbers
            if (left_result->type != DM_NODE_LITERAL || right_result->type != DM_NODE_LITERAL ||
                left_result->literal.type != DM_LITERAL_NUMBER || right_result->literal.type != DM_LITERAL_NUMBER) {
                snprintf(ctx->error_message, sizeof(ctx->error_message), 
                        "Expected numeric operands for comparison");
                dm_node_free(ctx, left_result);
                dm_node_free(ctx, right_result);
                dm_free(ctx, res);
                return DM_ERROR_TYPE_MISMATCH;
            }
            
            double left_val = left_result->literal.value.number;
            double right_val = right_result->literal.value.number;
            
            switch (node->binary.op) {
                case DM_OP_LT:
                    res->literal.value.boolean = left_val < right_val;
                    break;
                case DM_OP_GT:
                    res->literal.value.boolean = left_val > right_val;
                    break;
                case DM_OP_LTE:
                    res->literal.value.boolean = left_val <= right_val;
                    break;
                case DM_OP_GTE:
                    res->literal.value.boolean = left_val >= right_val;
                    break;
                default:
                    // Should never reach here
                    break;
            }
            break;
            
        case DM_OP_AND:
        case DM_OP_OR:
            // Logical operations
            res->literal.type = DM_LITERAL_BOOLEAN;
            
            // Convert operands to boolean values
            bool left_bool = false;
            if (left_result->type == DM_NODE_LITERAL) {
                switch (left_result->literal.type) {
                    case DM_LITERAL_BOOLEAN:
                        left_bool = left_result->literal.value.boolean;
                        break;
                    case DM_LITERAL_NUMBER:
                        left_bool = left_result->literal.value.number != 0.0;
                        break;
                    case DM_LITERAL_STRING:
                        left_bool = strlen(left_result->literal.value.string) > 0;
                        break;
                    case DM_LITERAL_NULL:
                        left_bool = false;
                        break;
                }
            } else {
                left_bool = true;  // Non-literal values are truthy
            }
            
            // Short-circuit evaluation for logical operations
            if ((node->binary.op == DM_OP_AND && !left_bool) ||
                (node->binary.op == DM_OP_OR && left_bool)) {
                res->literal.value.boolean = left_bool;
                break;  // Skip right evaluation
            }
            
            bool right_bool = false;
            if (right_result->type == DM_NODE_LITERAL) {
                switch (right_result->literal.type) {
                    case DM_LITERAL_BOOLEAN:
                        right_bool = right_result->literal.value.boolean;
                        break;
                    case DM_LITERAL_NUMBER:
                        right_bool = right_result->literal.value.number != 0.0;
                        break;
                    case DM_LITERAL_STRING:
                        right_bool = strlen(right_result->literal.value.string) > 0;
                        break;
                    case DM_LITERAL_NULL:
                        right_bool = false;
                        break;
                }
            } else {
                right_bool = true;  // Non-literal values are truthy
            }
            
            res->literal.value.boolean = (node->binary.op == DM_OP_AND) ? (left_bool && right_bool) : (left_bool || right_bool);
            break;
            
        default:
            snprintf(ctx->error_message, sizeof(ctx->error_message), 
                    "Unsupported binary operator: %d", node->binary.op);
            dm_node_free(ctx, left_result);
            dm_node_free(ctx, right_result);
            dm_free(ctx, res);
            return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Free the operand results
    dm_node_free(ctx, left_result);
    dm_node_free(ctx, right_result);
    
    // Return the result
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
    if (ctx == NULL || node == NULL || result == NULL || node->type != DM_NODE_VARIABLE) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Look up the variable in the current scope
    dm_value_t value;
    dm_error_t err = dm_scope_lookup(ctx, ctx->current_scope, node->variable.name, &value);
    
    if (err != DM_SUCCESS) {
        // Variable not found
        snprintf(ctx->error_message, sizeof(ctx->error_message), 
                "Undefined variable '%s'", node->variable.name);
        return DM_ERROR_UNDEFINED_VARIABLE;
    }
    
    // Create result node based on the value
    dm_node_t *res = create_result_node(ctx, DM_NODE_LITERAL);
    if (res == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Convert dm_value_t to dm_node_t
    switch (value.type) {
        case DM_TYPE_NULL:
            res->literal.type = DM_LITERAL_NULL;
            break;
            
        case DM_TYPE_BOOLEAN:
            res->literal.type = DM_LITERAL_BOOLEAN;
            res->literal.value.boolean = value.as.boolean;
            break;
            
        case DM_TYPE_INTEGER:
            res->literal.type = DM_LITERAL_NUMBER;
            res->literal.value.number = (double)value.as.integer;
            break;
            
        case DM_TYPE_FLOAT:
            res->literal.type = DM_LITERAL_NUMBER;
            res->literal.value.number = value.as.floating;
            break;
            
        case DM_TYPE_STRING:
            res->literal.type = DM_LITERAL_STRING;
            res->literal.value.string = dm_strdup(ctx, value.as.string.data);
            if (res->literal.value.string == NULL) {
                dm_free(ctx, res);
                return DM_ERROR_MEMORY_ALLOCATION;
            }
            break;
            
        default:
            // For other types, return null for now
            res->literal.type = DM_LITERAL_NULL;
            break;
    }
    
    *result = res;
    return DM_SUCCESS;
}

// Variable assignment
static dm_error_t eval_assignment(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    if (ctx == NULL || node == NULL || result == NULL || node->type != DM_NODE_ASSIGNMENT) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Evaluate the value first
    dm_node_t *value_node = NULL;
    dm_error_t err = dm_eval_node(ctx, node->assignment.value, &value_node);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Convert the node to a dm_value_t
    dm_value_t value;
    dm_value_init(&value);
    
    if (value_node->type == DM_NODE_LITERAL) {
        switch (value_node->literal.type) {
            case DM_LITERAL_NULL:
                value.type = DM_TYPE_NULL;
                break;
                
            case DM_LITERAL_BOOLEAN:
                value.type = DM_TYPE_BOOLEAN;
                value.as.boolean = value_node->literal.value.boolean;
                break;
                
            case DM_LITERAL_NUMBER:
                value.type = DM_TYPE_FLOAT;
                value.as.floating = value_node->literal.value.number;
                break;
                
            case DM_LITERAL_STRING:
                value.type = DM_TYPE_STRING;
                value.as.string.length = strlen(value_node->literal.value.string);
                value.as.string.data = dm_strdup(ctx, value_node->literal.value.string);
                if (value.as.string.data == NULL) {
                    dm_node_free(ctx, value_node);
                    return DM_ERROR_MEMORY_ALLOCATION;
                }
                break;
        }
    } else {
        // For non-literal nodes, store as null for now
        value.type = DM_TYPE_NULL;
    }
    
    // If this is a declaration, define a new variable
    // Otherwise update an existing one
    if (node->assignment.is_declaration) {
        err = dm_scope_define(ctx, ctx->current_scope, node->assignment.name, value);
    } else {
        // First check if the variable exists
        dm_value_t dummy;
        if (dm_scope_lookup(ctx, ctx->current_scope, node->assignment.name, &dummy) != DM_SUCCESS) {
            // Variable doesn't exist
            snprintf(ctx->error_message, sizeof(ctx->error_message), 
                    "Cannot assign to undefined variable '%s'", node->assignment.name);
            
            // Free value if necessary
            if (value.type == DM_TYPE_STRING) {
                dm_free(ctx, value.as.string.data);
            }
            
            dm_node_free(ctx, value_node);
            return DM_ERROR_UNDEFINED_VARIABLE;
        }
        
        // Variable exists, update it
        err = dm_scope_define(ctx, ctx->current_scope, node->assignment.name, value);
    }
    
    // Free the string data if we copied it (scope_define makes its own copy)
    if (value.type == DM_TYPE_STRING) {
        dm_free(ctx, value.as.string.data);
    }
    
    if (err != DM_SUCCESS) {
        dm_node_free(ctx, value_node);
        return err;
    }
    
    // Return the value as the result of the assignment
    *result = value_node;
    return DM_SUCCESS;
}

// Block of statements
static dm_error_t eval_block(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    if (ctx == NULL || node == NULL || result == NULL || node->type != DM_NODE_BLOCK) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Create a new scope for the block
    dm_scope_t *block_scope = dm_scope_create(ctx, ctx->current_scope);
    if (block_scope == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Save previous scope
    dm_scope_t *previous_scope = ctx->current_scope;
    
    // Set the new scope as current
    ctx->current_scope = block_scope;
    
    // Execute all statements in the block
    dm_node_t *stmt_result = NULL;
    dm_error_t err = DM_SUCCESS;
    
    for (size_t i = 0; i < node->block.count; i++) {
        // Free the previous statement result, if any
        if (stmt_result != NULL) {
            dm_node_free(ctx, stmt_result);
            stmt_result = NULL;
        }
        
        // Evaluate the current statement
        err = dm_eval_node(ctx, node->block.statements[i], &stmt_result);
        if (err != DM_SUCCESS) {
            break;
        }
    }
    
    // Save the last result
    *result = stmt_result;
    stmt_result = NULL; // Prevent it from being freed
    
    // Restore the previous scope
    ctx->current_scope = previous_scope;
    
    // Destroy the block scope
    dm_scope_destroy(ctx, block_scope);
    
    // If no statements were executed, return NULL
    if (*result == NULL) {
        *result = create_result_node(ctx, DM_NODE_LITERAL);
        if (*result != NULL) {
            (*result)->literal.type = DM_LITERAL_NULL;
        } else {
            err = DM_ERROR_MEMORY_ALLOCATION;
        }
    }
    
    return err;
}

// If statement
static dm_error_t eval_if(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    if (ctx == NULL || node == NULL || result == NULL || node->type != DM_NODE_IF) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Evaluate the condition
    dm_node_t *condition_result = NULL;
    dm_error_t err = dm_eval_node(ctx, node->if_stmt.condition, &condition_result);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Check if the condition is true
    bool condition_true = false;
    
    if (condition_result->type == DM_NODE_LITERAL) {
        switch (condition_result->literal.type) {
            case DM_LITERAL_BOOLEAN:
                condition_true = condition_result->literal.value.boolean;
                break;
                
            case DM_LITERAL_NUMBER:
                condition_true = condition_result->literal.value.number != 0;
                break;
                
            case DM_LITERAL_STRING:
                condition_true = condition_result->literal.value.string != NULL && 
                                 strlen(condition_result->literal.value.string) > 0;
                break;
                
            case DM_LITERAL_NULL:
                condition_true = false;
                break;
        }
    } else {
        // Non-literal nodes are considered true
        condition_true = true;
    }
    
    // Free condition result
    dm_node_free(ctx, condition_result);
    
    // Execute the appropriate branch
    if (condition_true) {
        // Execute then branch
        return dm_eval_node(ctx, node->if_stmt.then_branch, result);
    } else if (node->if_stmt.else_branch != NULL) {
        // Execute else branch
        return dm_eval_node(ctx, node->if_stmt.else_branch, result);
    } else {
        // No else branch, return NULL
        *result = create_result_node(ctx, DM_NODE_LITERAL);
        if (*result == NULL) {
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        (*result)->literal.type = DM_LITERAL_NULL;
        return DM_SUCCESS;
    }
}

// While loop
static dm_error_t eval_while(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    if (ctx == NULL || node == NULL || result == NULL || node->type != DM_NODE_WHILE) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    dm_node_t *latest_result = NULL;
    dm_error_t err = DM_SUCCESS;
    
    // Loop until condition is false
    while (1) {
        // Evaluate condition
        dm_node_t *condition_result = NULL;
        err = dm_eval_node(ctx, node->while_loop.condition, &condition_result);
        if (err != DM_SUCCESS) {
            break;
        }
        
        // Check if condition is true
        bool condition_true = false;
        
        if (condition_result->type == DM_NODE_LITERAL) {
            switch (condition_result->literal.type) {
                case DM_LITERAL_BOOLEAN:
                    condition_true = condition_result->literal.value.boolean;
                    break;
                    
                case DM_LITERAL_NUMBER:
                    condition_true = condition_result->literal.value.number != 0;
                    break;
                    
                case DM_LITERAL_STRING:
                    condition_true = condition_result->literal.value.string != NULL && 
                                     strlen(condition_result->literal.value.string) > 0;
                    break;
                    
                case DM_LITERAL_NULL:
                    condition_true = false;
                    break;
            }
        } else {
            // Non-literal nodes are considered true
            condition_true = true;
        }
        
        // Free condition result
        dm_node_free(ctx, condition_result);
        
        // Exit loop if condition is false
        if (!condition_true) {
            break;
        }
        
        // Free previous iteration result
        if (latest_result != NULL) {
            dm_node_free(ctx, latest_result);
            latest_result = NULL;
        }
        
        // Execute loop body
        err = dm_eval_node(ctx, node->while_loop.body, &latest_result);
        if (err != DM_SUCCESS) {
            break;
        }
    }
    
    // Return the result of the last iteration or NULL if no iterations
    if (latest_result != NULL) {
        *result = latest_result;
    } else {
        // Return null if loop never executed
        *result = create_result_node(ctx, DM_NODE_LITERAL);
        if (*result == NULL) {
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        (*result)->literal.type = DM_LITERAL_NULL;
    }
    
    return err;
}

// Function call
static dm_error_t eval_function_call(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    if (ctx == NULL || node == NULL || result == NULL || node->type != DM_NODE_CALL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Look up the function in the scope chain
    dm_value_t function_value;
    dm_error_t err = dm_scope_lookup(ctx, ctx->current_scope, node->call.name, &function_value);
    if (err != DM_SUCCESS) {
        // Function not found
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Function '%s' is not defined", node->call.name);
        dm_context_set_error(ctx, error_msg);
        return DM_ERROR_UNDEFINED_VARIABLE;
    }
    
    // Check if it's actually a function
    if (function_value.type != DM_TYPE_FUNCTION) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "'%s' is not a function", node->call.name);
        dm_context_set_error(ctx, error_msg);
        return DM_ERROR_TYPE_MISMATCH;
    }
    
    // Get the function node from the user_data
    dm_node_t *function_node = function_value.as.function.user_data;
    if (function_node == NULL || function_node->type != DM_NODE_FUNCTION) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Invalid function definition for '%s'", node->call.name);
        dm_context_set_error(ctx, error_msg);
        return DM_ERROR_TYPE_MISMATCH;
    }
    
    // Check if argument count matches parameter count
    if (node->call.arg_count != function_node->function.param_count) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                "Function '%s' expects %zu arguments, but got %zu", 
                node->call.name, 
                function_node->function.param_count, 
                node->call.arg_count);
        dm_context_set_error(ctx, error_msg);
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Create a new scope for the function call
    dm_scope_t *function_scope = dm_scope_create(ctx, ctx->current_scope);
    if (function_scope == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Evaluate and bind arguments to parameters
    for (size_t i = 0; i < node->call.arg_count; i++) {
        // Evaluate the argument
        dm_node_t *arg_result = NULL;
        err = dm_eval_node(ctx, node->call.args[i], &arg_result);
        if (err != DM_SUCCESS) {
            dm_scope_destroy(ctx, function_scope);
            return err;
        }
        
        // Convert argument to value
        dm_value_t arg_value;
        dm_value_init(&arg_value);
        
        if (arg_result->type == DM_NODE_LITERAL) {
            // Convert literal to value
            switch (arg_result->literal.type) {
                case DM_LITERAL_NULL:
                    arg_value.type = DM_TYPE_NULL;
                    break;
                    
                case DM_LITERAL_BOOLEAN:
                    arg_value.type = DM_TYPE_BOOLEAN;
                    arg_value.as.boolean = arg_result->literal.value.boolean;
                    break;
                    
                case DM_LITERAL_NUMBER:
                    arg_value.type = DM_TYPE_FLOAT;
                    arg_value.as.floating = arg_result->literal.value.number;
                    break;
                    
                case DM_LITERAL_STRING:
                    arg_value.type = DM_TYPE_STRING;
                    arg_value.as.string.data = strdup(arg_result->literal.value.string);
                    if (arg_value.as.string.data == NULL) {
                        dm_node_free(ctx, arg_result);
                        dm_scope_destroy(ctx, function_scope);
                        return DM_ERROR_MEMORY_ALLOCATION;
                    }
                    arg_value.as.string.length = strlen(arg_value.as.string.data);
                    break;
            }
        } else {
            // Non-literal node, this is an error
            dm_node_free(ctx, arg_result);
            dm_scope_destroy(ctx, function_scope);
            return DM_ERROR_TYPE_MISMATCH;
        }
        
        // Bind argument to parameter
        err = dm_scope_define(ctx, function_scope, function_node->function.params[i], arg_value);
        
        // Clean up any dynamically allocated memory in the value
        dm_value_free(ctx, &arg_value);
        
        // Free the argument result
        dm_node_free(ctx, arg_result);
        
        if (err != DM_SUCCESS) {
            dm_scope_destroy(ctx, function_scope);
            return err;
        }
    }
    
    // Save previous scope
    dm_scope_t *previous_scope = ctx->current_scope;
    
    // Set function scope as current
    ctx->current_scope = function_scope;
    
    // Execute function body
    err = dm_eval_node(ctx, function_node->function.body, result);
    
    // Restore previous scope
    ctx->current_scope = previous_scope;
    
    // Destroy function scope
    dm_scope_destroy(ctx, function_scope);
    
    return err;
}

// Function declaration
static dm_error_t eval_function_declaration(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    if (ctx == NULL || node == NULL || result == NULL || node->type != DM_NODE_FUNCTION) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Store the function in the current scope
    dm_value_t function_value;
    dm_value_init(&function_value);
    
    function_value.type = DM_TYPE_FUNCTION;
    function_value.as.function.func = NULL; // Not a native function
    function_value.as.function.user_data = node; // Store the function node as user data
    
    // Add the function to the current scope
    dm_error_t err = dm_scope_define(ctx, ctx->current_scope, node->function.name, function_value);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Return the function name as result
    *result = create_result_node(ctx, DM_NODE_LITERAL);
    if (*result == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    (*result)->literal.type = DM_LITERAL_STRING;
    (*result)->literal.value.string = strdup(node->function.name);
    if ((*result)->literal.value.string == NULL) {
        dm_node_free(ctx, *result);
        *result = NULL;
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    return DM_SUCCESS;
}

// Return statement
static dm_error_t eval_return(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    if (ctx == NULL || node == NULL || result == NULL || node->type != DM_NODE_RETURN) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // If there's a return value, evaluate it
    if (node->return_stmt.value != NULL) {
        return dm_eval_node(ctx, node->return_stmt.value, result);
    } else {
        // Return null if no value provided
        *result = create_result_node(ctx, DM_NODE_LITERAL);
        if (*result == NULL) {
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        (*result)->literal.type = DM_LITERAL_NULL;
        return DM_SUCCESS;
    }
}

// Execute a program (sequence of statements)
static dm_error_t eval_program(dm_context_t *ctx, dm_node_t *node, dm_node_t **result) {
    if (ctx == NULL || node == NULL || result == NULL || node->type != DM_NODE_PROGRAM) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Execute all statements in sequence, keeping only the last result
    dm_node_t *stmt_result = NULL;
    dm_error_t err = DM_SUCCESS;
    
    for (size_t i = 0; i < node->program.count; i++) {
        // Free previous result, if any
        if (stmt_result != NULL) {
            dm_node_free(ctx, stmt_result);
            stmt_result = NULL;
        }
        
        // Evaluate the current statement
        err = dm_eval_node(ctx, node->program.statements[i], &stmt_result);
        if (err != DM_SUCCESS) {
            // Don't free stmt_result, as it might not be allocated if there was an error
            return err;
        }
        
        // Print the result if it's an expression statement
        if (stmt_result != NULL && node->program.statements[i]->type != DM_NODE_ASSIGNMENT 
            && node->program.statements[i]->type != DM_NODE_FUNCTION) {
            char *result_str = NULL;
            if (dm_node_to_string(ctx, stmt_result, &result_str) == DM_SUCCESS && result_str != NULL) {
                fprintf(ctx->output, "=> %s\n", result_str);
                dm_free(ctx, result_str);
            }
        }
    }
    
    // Return the last statement's result
    *result = stmt_result;
    
    // If no statements were executed, return NULL literal
    if (*result == NULL) {
        *result = create_result_node(ctx, DM_NODE_LITERAL);
        if (*result != NULL) {
            (*result)->literal.type = DM_LITERAL_NULL;
        } else {
            return DM_ERROR_MEMORY_ALLOCATION;
        }
    }
    
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