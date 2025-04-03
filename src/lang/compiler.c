#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/dmkernel.h"
#include "../../include/lang/parser.h"
#include "../../include/core/memory.h"

// Function to compile the source code into executable bytecode
dm_error_t dm_compile(dm_context_t *ctx, const char *source, size_t source_len, void **bytecode, size_t *bytecode_len) {
    if (ctx == NULL || source == NULL || bytecode == NULL || bytecode_len == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Initialize parser
    dm_parser_t parser;
    dm_error_t err = dm_parser_init(ctx, &parser, source, source_len);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Parse source code to AST
    dm_node_t *ast = NULL;
    err = dm_parser_parse(&parser, &ast);
    if (err != DM_SUCCESS) {
        if (parser.error_message[0] != '\0') {
            fprintf(ctx->error, "Parse error: %s\n", parser.error_message);
        } else {
            fprintf(ctx->error, "Failed to parse the source code\n");
        }
        return err;
    }
    
    // For now, we'll just use the AST directly without bytecode generation
    // In a real compiler, we would translate the AST to bytecode here
    
    // Allocate "bytecode" to store the AST node pointer
    *bytecode = dm_malloc(ctx, sizeof(dm_node_t *));
    if (*bytecode == NULL) {
        dm_node_free(ctx, ast);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Store the AST and set the bytecode length
    *((dm_node_t **)(*bytecode)) = ast;
    *bytecode_len = sizeof(dm_node_t *);
    
    return DM_SUCCESS;
}

// Function to execute the compiled bytecode
dm_error_t dm_execute_bytecode(dm_context_t *ctx, void *bytecode, size_t bytecode_len, dm_node_t **result) {
    if (ctx == NULL || bytecode == NULL || bytecode_len < sizeof(dm_node_t *)) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Extract the AST from the "bytecode"
    dm_node_t *ast = *((dm_node_t **)bytecode);
    
    // Evaluate the AST
    return dm_eval_node(ctx, ast, result);
}

// Function to free compiled bytecode
void dm_free_bytecode(dm_context_t *ctx, void *bytecode, size_t bytecode_len) {
    if (ctx == NULL || bytecode == NULL || bytecode_len < sizeof(dm_node_t *)) {
        return;
    }
    
    // Extract and free the AST
    dm_node_t *ast = *((dm_node_t **)bytecode);
    dm_node_free(ctx, ast);
    
    // Free the bytecode container
    dm_free(ctx, bytecode);
}

// Helper function to print compiler errors
void dm_compiler_print_error(dm_context_t *ctx, const char *source, size_t source_len, 
                            size_t line, size_t column, const char *message) {
    if (ctx == NULL || source == NULL || message == NULL) {
        return;
    }
    
    // Find the start of the line
    size_t line_start = 0;
    size_t current_line = 1;
    
    while (line_start < source_len && current_line < line) {
        if (source[line_start] == '\n') {
            current_line++;
        }
        line_start++;
    }
    
    // Find the end of the line
    size_t line_end = line_start;
    while (line_end < source_len && source[line_end] != '\n') {
        line_end++;
    }
    
    // Extract the line
    size_t line_len = line_end - line_start;
    char *line_str = dm_malloc(ctx, line_len + 1);
    if (line_str == NULL) {
        return;
    }
    
    strncpy(line_str, source + line_start, line_len);
    line_str[line_len] = '\0';
    
    // Print error message
    fprintf(ctx->error, "Error at line %zu, column %zu: %s\n", line, column, message);
    fprintf(ctx->error, "%s\n", line_str);
    
    // Print pointer to the error position
    for (size_t i = 1; i < column && i <= line_len; i++) {
        fprintf(ctx->error, " ");
    }
    fprintf(ctx->error, "^\n");
    
    // Free the line string
    dm_free(ctx, line_str);
} 