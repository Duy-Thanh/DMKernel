#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/dmkernel.h"
#include "../../include/shell/shell.h"
#include "../../include/core/filesystem.h"
#include "../../include/lang/exec.h"

// Command: parse <file>
// Parse a file and display the AST
dm_error_t dm_cmd_parse(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL || argc < 2) {
        fprintf(ctx->error, "Usage: parse <file>\n");
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    const char *filename = argv[1];
    
    // Check if file exists
    bool exists = false;
    dm_error_t err = dm_file_exists(ctx, filename, &exists);
    if (err != DM_SUCCESS || !exists) {
        fprintf(ctx->error, "File not found: %s\n", filename);
        return DM_ERROR_FILE_IO;
    }
    
    // Open file
    dm_file_t *file = NULL;
    err = dm_file_open(ctx, filename, DM_FILE_READ, &file);
    if (err != DM_SUCCESS) {
        fprintf(ctx->error, "Failed to open file: %s\n", filename);
        return err;
    }
    
    // Get file size
    size_t file_size = 0;
    err = dm_file_size(ctx, filename, &file_size);
    if (err != DM_SUCCESS) {
        dm_file_close(ctx, file);
        return err;
    }
    
    // Read file into memory
    char *source = dm_malloc(ctx, file_size + 1);
    if (source == NULL) {
        dm_file_close(ctx, file);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    size_t bytes_read = 0;
    err = dm_file_read(ctx, file, source, file_size, &bytes_read);
    if (err != DM_SUCCESS) {
        dm_file_close(ctx, file);
        dm_free(ctx, source);
        return err;
    }
    
    // Null-terminate the source
    source[bytes_read] = '\0';
    
    // Close file
    dm_file_close(ctx, file);
    
    // Initialize parser
    dm_parser_t parser;
    err = dm_parser_init(ctx, &parser, source, bytes_read);
    if (err != DM_SUCCESS) {
        dm_free(ctx, source);
        return err;
    }
    
    // Parse source
    dm_node_t *root = NULL;
    err = dm_parser_parse(&parser, &root);
    if (err != DM_SUCCESS) {
        fprintf(ctx->error, "Parse error: %s\n", parser.error_message);
        dm_free(ctx, source);
        return err;
    }
    
    // Print success
    fprintf(ctx->output, "Successfully parsed %s\n", filename);
    
    // TODO: Pretty-print the AST
    
    // Free AST and source
    dm_node_free(ctx, root);
    dm_free(ctx, source);
    
    return DM_SUCCESS;
}

// Command: compile <source> <output>
// Compile a source file to a bytecode file
dm_error_t dm_cmd_compile(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL || argc < 3) {
        fprintf(ctx->error, "Usage: compile <source> <output>\n");
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    const char *source_file = argv[1];
    const char *output_file = argv[2];
    
    // Check if source file exists
    bool exists = false;
    dm_error_t err = dm_file_exists(ctx, source_file, &exists);
    if (err != DM_SUCCESS || !exists) {
        fprintf(ctx->error, "Source file not found: %s\n", source_file);
        return DM_ERROR_FILE_IO;
    }
    
    // Open source file
    dm_file_t *file = NULL;
    err = dm_file_open(ctx, source_file, DM_FILE_READ, &file);
    if (err != DM_SUCCESS) {
        fprintf(ctx->error, "Failed to open source file: %s\n", source_file);
        return err;
    }
    
    // Get file size
    size_t file_size = 0;
    err = dm_file_size(ctx, source_file, &file_size);
    if (err != DM_SUCCESS) {
        dm_file_close(ctx, file);
        return err;
    }
    
    // Read file into memory
    char *source = dm_malloc(ctx, file_size + 1);
    if (source == NULL) {
        dm_file_close(ctx, file);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    size_t bytes_read = 0;
    err = dm_file_read(ctx, file, source, file_size, &bytes_read);
    if (err != DM_SUCCESS) {
        dm_file_close(ctx, file);
        dm_free(ctx, source);
        return err;
    }
    
    // Null-terminate the source
    source[bytes_read] = '\0';
    
    // Close source file
    dm_file_close(ctx, file);
    
    // Initialize parser
    dm_parser_t parser;
    err = dm_parser_init(ctx, &parser, source, bytes_read);
    if (err != DM_SUCCESS) {
        dm_free(ctx, source);
        return err;
    }
    
    // Parse source
    dm_node_t *root = NULL;
    err = dm_parser_parse(&parser, &root);
    if (err != DM_SUCCESS) {
        fprintf(ctx->error, "Parse error: %s\n", parser.error_message);
        dm_free(ctx, source);
        return err;
    }
    
    // TODO: Generate bytecode from AST
    // For now, we'll just create a simple "bytecode" file with a header
    
    // Open output file
    dm_file_t *output = NULL;
    err = dm_file_open(ctx, output_file, DM_FILE_WRITE | DM_FILE_CREATE | DM_FILE_TRUNCATE, &output);
    if (err != DM_SUCCESS) {
        fprintf(ctx->error, "Failed to open output file: %s\n", output_file);
        dm_node_free(ctx, root);
        dm_free(ctx, source);
        return err;
    }
    
    // Write a simple header
    const char *header = "DMK\0"; // Magic number for our format
    size_t bytes_written = 0;
    err = dm_file_write(ctx, output, header, 4, &bytes_written);
    if (err != DM_SUCCESS || bytes_written != 4) {
        fprintf(ctx->error, "Failed to write to output file\n");
        dm_file_close(ctx, output);
        dm_node_free(ctx, root);
        dm_free(ctx, source);
        return DM_ERROR_FILE_IO;
    }
    
    // Add a version number
    uint16_t version = 1;
    err = dm_file_write(ctx, output, &version, sizeof(version), &bytes_written);
    if (err != DM_SUCCESS || bytes_written != sizeof(version)) {
        fprintf(ctx->error, "Failed to write to output file\n");
        dm_file_close(ctx, output);
        dm_node_free(ctx, root);
        dm_free(ctx, source);
        return DM_ERROR_FILE_IO;
    }
    
    // Close output file
    dm_file_close(ctx, output);
    
    // Free AST and source
    dm_node_free(ctx, root);
    dm_free(ctx, source);
    
    fprintf(ctx->output, "Successfully compiled %s to %s\n", source_file, output_file);
    
    return DM_SUCCESS;
}

// Command: run <file>
// Run a script file
dm_error_t dm_cmd_run_script(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL || argc < 2) {
        fprintf(ctx->error, "Usage: run <file>\n");
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    const char *filename = argv[1];
    
    // Execute the file
    dm_error_t err = dm_execute_file(ctx, filename);
    if (err != DM_SUCCESS) {
        fprintf(ctx->error, "Error executing file: %s\n", dm_error_string(err));
        return err;
    }
    
    return DM_SUCCESS;
}

// Register language commands with the shell
dm_error_t dm_register_lang_commands(dm_shell_t *shell) {
    if (shell == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Register commands
    dm_error_t err;
    
    err = dm_shell_register_command(shell, "parse", "Parse a script file and display the AST", dm_cmd_parse);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    err = dm_shell_register_command(shell, "compile", "Compile a script file to bytecode", dm_cmd_compile);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    err = dm_shell_register_command(shell, "run", "Run a script file", dm_cmd_run_script);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    return DM_SUCCESS;
} 