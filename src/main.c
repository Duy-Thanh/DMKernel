#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/dmkernel.h"
#include "../include/core/filesystem.h"
#include "../include/core/memory.h"

// Global context
static dm_context_t *g_ctx = NULL;

// Print banner
static void print_banner(FILE *output) {
    fprintf(output, "\n");
    fprintf(output, "----------------------------------------\n");
    fprintf(output, "  DMKernel - Data Mining Runtime v%d.%d.%d\n", 
            DM_KERNEL_VERSION_MAJOR, 
            DM_KERNEL_VERSION_MINOR, 
            DM_KERNEL_VERSION_PATCH);
    fprintf(output, "  Copyright (C) 2025 DMKernel Team.\n");
    fprintf(output, "  All right reserved.\n");
    fprintf(output, "----------------------------------------\n");
    fprintf(output, "\n");
}

// Create some test allocations to showcase memory tracking
static void create_test_allocations(dm_context_t *ctx) {
    // Allocate some memory blocks of various sizes for testing
    void *small = DM_MALLOC(ctx, 1024);       // 1 KB
    void *medium = DM_MALLOC(ctx, 1024*100);  // 100 KB
    void *large = DM_MALLOC(ctx, 1024*1024);  // 1 MB
    
    // Create a string
    char *str = DM_MALLOC(ctx, 256);
    if (str) {
        strcpy(str, "This is a test string");
    }
    
    // Create an array
    int *array = DM_CALLOC(ctx, 1000, sizeof(int));
    if (array) {
        for (int i = 0; i < 1000; i++) {
            array[i] = i;
        }
    }
    
    // We intentionally don't free these allocations
    // This will show up in memory info during kernel panic
    
    // But free one of them to show the difference
    DM_FREE(ctx, medium);
}

// Initialize kernel
dm_error_t dm_init(dm_context_t **ctx) {
    // Create context
    // fprintf(stderr, "Creating context...\n");
    dm_error_t error = dm_context_create(ctx);
    if (error != DM_SUCCESS) {
        fprintf(stderr, "Failed to create context: %s\n", dm_error_string(error));
        return error;
    }
    
    // Initialize filesystem
    // fprintf(stderr, "Initializing filesystem...\n");
    error = dm_fs_init(*ctx);
    if (error != DM_SUCCESS) {
        fprintf(stderr, "Failed to initialize filesystem: %s\n", dm_error_string(error));
        dm_context_destroy(*ctx);
        *ctx = NULL;
        return error;
    }
    
    // Create some test allocations
    create_test_allocations(*ctx);
    
    // TODO: Register primitives
    // dm_register_primitives(*ctx);
    
    return DM_SUCCESS;
}

// Clean up kernel
void dm_cleanup(dm_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    // Clean up filesystem
    dm_fs_cleanup(ctx);
    
    dm_context_destroy(ctx);
}

// Execute code string
dm_error_t dm_execute(dm_context_t *ctx, const char *code) {
    if (ctx == NULL || code == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Execute source code using the parser and evaluator
    dm_node_t *result = NULL;
    dm_error_t err = dm_execute_source(ctx, code, strlen(code), &result);
    
    if (err != DM_SUCCESS) {
        fprintf(ctx->error, "Execution error: %s\n", dm_error_string(err));
        return err;
    }
    
    // Print the result to output if it exists and we're in interactive mode
    if (result != NULL && ctx->interactive) {
        char *str = NULL;
        err = dm_node_to_string(ctx, result, &str);
        
        if (err == DM_SUCCESS && str != NULL) {
            fprintf(ctx->output, "=> %s\n", str);
            DM_FREE(ctx, str);
        }
        
        // Free the result node
        dm_node_free(ctx, result);
    }
    
    return DM_SUCCESS;
}

// Execute script file
dm_error_t dm_execute_file(dm_context_t *ctx, const char *filename) {
    if (ctx == NULL || filename == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
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
    char *code = DM_MALLOC(ctx, file_size + 1);
    if (code == NULL) {
        dm_file_close(ctx, file);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    size_t bytes_read = 0;
    err = dm_file_read(ctx, file, code, file_size, &bytes_read);
    if (err != DM_SUCCESS) {
        dm_file_close(ctx, file);
        DM_FREE(ctx, code);
        return err;
    }
    
    // Null-terminate the code
    code[bytes_read] = '\0';
    
    // Close file
    dm_file_close(ctx, file);
    
    // Execute code
    err = dm_execute(ctx, code);
    
    // Clean up
    DM_FREE(ctx, code);
    
    return err;
}

// Convert error code to string
const char* dm_error_string(dm_error_t error) {
    switch (error) {
        case DM_SUCCESS:
            return "Success";
        case DM_ERROR_MEMORY_ALLOCATION:
            return "Memory allocation failed";
        case DM_ERROR_INVALID_ARGUMENT:
            return "Invalid argument";
        case DM_ERROR_FILE_IO:
            return "File I/O error";
        case DM_ERROR_SYNTAX_ERROR:
            return "Syntax error";
        case DM_ERROR_TYPE_MISMATCH:
            return "Runtime error (type mismatch)";
        default:
            return "Unknown error";
    }
}

#ifndef DMKERNEL_AS_LIBRARY
// Parse command line arguments
static int parse_args(int argc, char **argv, char **script_file) {
    *script_file = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            // Option
            if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                printf("Usage: %s [options] [script.dm]\n", argv[0]);
                printf("Options:\n");
                printf("  -h, --help    Show this help message\n");
                printf("  -v, --version Show version information\n");
                return 0;
            } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
                printf("DMKernel %d.%d.%d\n", 
                       DM_KERNEL_VERSION_MAJOR, 
                       DM_KERNEL_VERSION_MINOR, 
                       DM_KERNEL_VERSION_PATCH);
                return 0;
            } else {
                fprintf(stderr, "Unknown option: %s\n", argv[i]);
                return 1;
            }
        } else {
            // Script file
            *script_file = argv[i];
        }
    }
    
    return 2; // Continue execution
}

int main(int argc, char **argv) {
    int exit_code = 0;
    char *script_file = NULL;
    
    // Parse command line arguments
    int arg_result = parse_args(argc, argv, &script_file);
    if (arg_result <= 1) {
        return arg_result; // 0 for help/version, 1 for error
    }
    
    // Initialize kernel
    dm_error_t error = dm_init(&g_ctx);
    if (error != DM_SUCCESS) {
        fprintf(stderr, "Failed to initialize kernel: %s\n", dm_error_string(error));
        return 1;
    }
    
    // Print banner
    print_banner(g_ctx->output);
    
    if (script_file != NULL) {
        // Execute script file
        error = dm_execute_file(g_ctx, script_file);
        if (error != DM_SUCCESS) {
            fprintf(g_ctx->error, "Error executing script: %s\n", dm_error_string(error));
            exit_code = 1;
        }
    } else {
        // Interactive mode
        dm_shell_t *shell = NULL;
        error = dm_shell_create(g_ctx, &shell);
        if (error != DM_SUCCESS) {
            fprintf(g_ctx->error, "Failed to create shell: %s\n", dm_error_string(error));
            exit_code = 1;
        } else {
            // Run shell
            error = dm_shell_run(shell);
            if (error != DM_SUCCESS) {
                fprintf(g_ctx->error, "Shell error: %s\n", dm_error_string(error));
                exit_code = 1;
            }
            
            // Destroy shell
            dm_shell_destroy(shell);
        }
    }
    
    // Get exit code from context
    exit_code = g_ctx->exit_code;
    
    // Clean up
    dm_cleanup(g_ctx);
    
    return exit_code;
}
#endif // DMKERNEL_AS_LIBRARY