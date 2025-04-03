#ifndef _DM_LANG_EXEC_H
#define _DM_LANG_EXEC_H

#include "../dmkernel.h"
#include "parser.h"

/**
 * @brief Evaluates an AST node and returns the result
 * 
 * @param ctx The DMKernel context
 * @param node The AST node to evaluate
 * @param result Pointer to store the result (will be allocated)
 * @return dm_error_t Error code
 */
dm_error_t dm_eval_node(dm_context_t *ctx, dm_node_t *node, dm_node_t **result);

/**
 * @brief Executes code from a source string
 * 
 * @param ctx The DMKernel context
 * @param source The source code to execute
 * @param source_len Length of the source code
 * @param result Pointer to store the result (will be allocated, can be NULL if not needed)
 * @return dm_error_t Error code
 */
dm_error_t dm_execute_source(dm_context_t *ctx, const char *source, size_t source_len, dm_node_t **result);

/**
 * @brief Executes code from a file
 * 
 * @param ctx The DMKernel context
 * @param filename The file to execute
 * @return dm_error_t Error code
 */
dm_error_t dm_execute_file(dm_context_t *ctx, const char *filename);

/**
 * @brief Creates a string representation of a value
 * 
 * @param ctx The DMKernel context
 * @param node The node to convert to string
 * @param str Pointer to store the resulting string (will be allocated)
 * @return dm_error_t Error code
 */
dm_error_t dm_node_to_string(dm_context_t *ctx, dm_node_t *node, char **str);

#endif /* _DM_LANG_EXEC_H */ 