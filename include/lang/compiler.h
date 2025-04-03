#ifndef DM_COMPILER_H
#define DM_COMPILER_H

#include "../dmkernel.h"
#include "../lang/parser.h"

/**
 * Compile source code into executable bytecode
 * 
 * @param ctx The DMKernel context
 * @param source The source code to compile
 * @param source_len The length of the source code
 * @param bytecode Pointer to store the compiled bytecode
 * @param bytecode_len Pointer to store the length of the bytecode
 * @return DM_SUCCESS on success, error code otherwise
 */
dm_error_t dm_compile(dm_context_t *ctx, const char *source, size_t source_len, 
                     void **bytecode, size_t *bytecode_len);

/**
 * Execute compiled bytecode
 * 
 * @param ctx The DMKernel context
 * @param bytecode The compiled bytecode
 * @param bytecode_len The length of the bytecode
 * @param result Pointer to store the result of execution
 * @return DM_SUCCESS on success, error code otherwise
 */
dm_error_t dm_execute_bytecode(dm_context_t *ctx, void *bytecode, size_t bytecode_len, 
                              dm_node_t **result);

/**
 * Free compiled bytecode
 * 
 * @param ctx The DMKernel context
 * @param bytecode The bytecode to free
 * @param bytecode_len The length of the bytecode
 */
void dm_free_bytecode(dm_context_t *ctx, void *bytecode, size_t bytecode_len);

/**
 * Print compiler error with line and column information
 * 
 * @param ctx The DMKernel context
 * @param source The source code
 * @param source_len The length of the source code
 * @param line The line number where the error occurred
 * @param column The column number where the error occurred
 * @param message The error message
 */
void dm_compiler_print_error(dm_context_t *ctx, const char *source, size_t source_len, 
                            size_t line, size_t column, const char *message);

#endif /* DM_COMPILER_H */ 