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

// Command to run a script
dm_error_t dm_cmd_run(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL || argc < 2) {
        fprintf(ctx->output, "Usage: run <filename>\n");
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Get absolute path to the script
    char *abs_path = NULL;
    
    // Get absolute path
    dm_error_t path_err = dm_path_absolute(ctx, argv[1], &abs_path);
    if (path_err != DM_SUCCESS) {
        // Fall back to current directory + relative path
        char *cwd = NULL;
        dm_error_t cwd_err = dm_vfs_get_working_dir(ctx, &cwd);
        if (cwd_err != DM_SUCCESS) {
            fprintf(ctx->output, "Error getting current directory\n");
            return cwd_err;
        }
        
        char *joined_path = NULL;
        dm_error_t join_err = dm_path_join(ctx, cwd, argv[1], &joined_path);
        dm_free(ctx, cwd);
        
        if (join_err != DM_SUCCESS) {
            fprintf(ctx->output, "Error joining paths\n");
            return join_err;
        }
        
        abs_path = joined_path;
    }
    
    if (abs_path == NULL) {
        fprintf(ctx->output, "Error: Memory allocation failed\n");
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Check if file exists
    bool exists = false;
    dm_error_t exists_err = dm_file_exists(ctx, abs_path, &exists);
    if (exists_err != DM_SUCCESS || !exists) {
        fprintf(ctx->output, "Error: File not found: %s\n", argv[1]);
        dm_free(ctx, abs_path);
        return DM_ERROR_FILE_IO;
    }
    
    // Open the file
    dm_file_t *file = NULL;
    dm_error_t open_err = dm_file_open(ctx, abs_path, DM_FILE_READ, &file);
    if (open_err != DM_SUCCESS) {
        fprintf(ctx->output, "Error: Failed to open file: %s\n", argv[1]);
        dm_free(ctx, abs_path);
        return open_err;
    }
    
    // Get file size
    size_t file_size = 0;
    dm_error_t size_err = dm_file_size(ctx, abs_path, &file_size);
    if (size_err != DM_SUCCESS) {
        fprintf(ctx->output, "Error: Failed to get file size: %s\n", argv[1]);
        dm_file_close(ctx, file);
        dm_free(ctx, abs_path);
        return size_err;
    }
    
    // Allocate buffer for file content
    char *content = dm_malloc(ctx, file_size + 1);
    if (content == NULL) {
        fprintf(ctx->output, "Error: Memory allocation failed\n");
        dm_file_close(ctx, file);
        dm_free(ctx, abs_path);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Read file content
    size_t bytes_read = 0;
    dm_error_t read_err = dm_file_read(ctx, file, content, file_size, &bytes_read);
    if (read_err != DM_SUCCESS || bytes_read != file_size) {
        fprintf(ctx->output, "Error: Failed to read file: %s\n", argv[1]);
        dm_free(ctx, content);
        dm_file_close(ctx, file);
        dm_free(ctx, abs_path);
        return read_err;
    }
    
    // Null-terminate the buffer
    content[bytes_read] = '\0';
    
    // Close the file
    dm_file_close(ctx, file);
    
    // Execute the script
    dm_node_t *result = NULL;
    dm_error_t exec_err = dm_execute_source(ctx, content, bytes_read, &result);
    if (exec_err != DM_SUCCESS) {
        fprintf(ctx->output, "Error executing script: %s\n", dm_error_string(exec_err));
        dm_free(ctx, abs_path);
        dm_free(ctx, content);
        return exec_err;
    }
    
    // Free the result if available
    if (result != NULL) {
        dm_node_free(ctx, result);
    }
    
    // Clean up
    dm_free(ctx, abs_path);
    dm_free(ctx, content);
    
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