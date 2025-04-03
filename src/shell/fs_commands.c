#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/dmkernel.h"
#include "../../include/shell/shell.h"
#include "../../include/core/filesystem.h"

// Command: ls [path]
// List directory contents
dm_error_t dm_cmd_ls(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    const char *path = argc > 1 ? argv[1] : ".";
    
    // Open directory
    dm_dir_t *dir = NULL;
    dm_error_t err = dm_dir_open(ctx, path, &dir);
    if (err != DM_SUCCESS) {
        fprintf(ctx->error, "Failed to open directory: %s\n", path);
        return err;
    }
    
    // Read directory entries
    dm_dir_entry_t *entry = NULL;
    while ((err = dm_dir_read(ctx, dir, &entry)) == DM_SUCCESS && entry != NULL) {
        char type_char = '?';
        
        switch (entry->type) {
            case DM_FILE_TYPE_REGULAR:
                type_char = '-';
                break;
            case DM_FILE_TYPE_DIRECTORY:
                type_char = 'd';
                break;
            case DM_FILE_TYPE_SPECIAL:
                type_char = 's';
                break;
            default:
                type_char = '?';
                break;
        }
        
        fprintf(ctx->output, "%c %8zu %s\n", type_char, entry->size, entry->name);
    }
    
    // Close directory
    dm_dir_close(ctx, dir);
    
    return DM_SUCCESS;
}

// Command: cd <path>
// Change working directory
dm_error_t dm_cmd_cd(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL || argc < 2) {
        fprintf(ctx->error, "Usage: cd <path>\n");
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    const char *path = argv[1];
    
    // Check if directory exists before changing to it
    bool exists = false;
    dm_error_t err = dm_dir_exists(ctx, path, &exists);
    if (err != DM_SUCCESS) {
        fprintf(ctx->error, "Error checking directory: %s\n", dm_error_string(err));
        return err;
    }
    
    if (!exists) {
        fprintf(ctx->error, "Directory not found: %s\n", path);
        return DM_ERROR_FILE_IO;
    }
    
    // Change directory
    err = dm_vfs_set_working_dir(ctx, path);
    if (err != DM_SUCCESS) {
        fprintf(ctx->error, "Failed to change directory: %s (%s)\n", 
                path, dm_error_string(err));
        return err;
    }
    
    return DM_SUCCESS;
}

// Command: pwd
// Print working directory
dm_error_t dm_cmd_pwd(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Get working directory
    char *cwd = NULL;
    dm_error_t err = dm_vfs_get_working_dir(ctx, &cwd);
    if (err != DM_SUCCESS) {
        fprintf(ctx->error, "Failed to get working directory\n");
        return err;
    }
    
    // Print working directory
    fprintf(ctx->output, "%s\n", cwd);
    
    // Free memory
    dm_free(ctx, cwd);
    
    return DM_SUCCESS;
}

// Command: cat <file>
// Display file contents
dm_error_t dm_cmd_cat(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL || argc < 2) {
        fprintf(ctx->error, "Usage: cat <file>\n");
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
    
    // Read and display file contents
    char buffer[1024];
    size_t bytes_read = 0;
    
    while ((err = dm_file_read(ctx, file, buffer, sizeof(buffer) - 1, &bytes_read)) == DM_SUCCESS && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        fprintf(ctx->output, "%s", buffer);
    }
    
    // Add newline if needed
    if (bytes_read > 0 && buffer[bytes_read - 1] != '\n') {
        fprintf(ctx->output, "\n");
    }
    
    // Close file
    dm_file_close(ctx, file);
    
    return DM_SUCCESS;
}

// Command: mkdir <path>
// Create a directory
dm_error_t dm_cmd_mkdir(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL || argc < 2) {
        fprintf(ctx->error, "Usage: mkdir <path>\n");
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    const char *path = argv[1];
    
    // Create directory
    dm_error_t err = dm_dir_create(ctx, path);
    if (err != DM_SUCCESS) {
        fprintf(ctx->error, "Failed to create directory: %s\n", path);
        return err;
    }
    
    return DM_SUCCESS;
}

// Command: rm <path>
// Remove a file or directory
dm_error_t dm_cmd_rm(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL || argc < 2) {
        fprintf(ctx->error, "Usage: rm <path>\n");
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    const char *path = argv[1];
    
    // Check if path exists
    bool exists = false;
    dm_error_t err = dm_file_exists(ctx, path, &exists);
    if (err != DM_SUCCESS || !exists) {
        // Try as directory
        err = dm_dir_exists(ctx, path, &exists);
        if (err != DM_SUCCESS || !exists) {
            fprintf(ctx->error, "Path not found: %s\n", path);
            return DM_ERROR_FILE_IO;
        }
        
        // Remove directory
        err = dm_dir_delete(ctx, path);
        if (err != DM_SUCCESS) {
            fprintf(ctx->error, "Failed to remove directory: %s\n", path);
            return err;
        }
    } else {
        // Remove file
        err = dm_file_delete(ctx, path);
        if (err != DM_SUCCESS) {
            fprintf(ctx->error, "Failed to remove file: %s\n", path);
            return err;
        }
    }
    
    return DM_SUCCESS;
}

// Register file system commands with the shell
dm_error_t dm_register_fs_commands(dm_shell_t *shell) {
    if (shell == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Register commands
    dm_error_t err;
    
    err = dm_shell_register_command(shell, "ls", "List directory contents", dm_cmd_ls);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    err = dm_shell_register_command(shell, "cd", "Change working directory", dm_cmd_cd);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    err = dm_shell_register_command(shell, "pwd", "Print working directory", dm_cmd_pwd);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    err = dm_shell_register_command(shell, "cat", "Display file contents", dm_cmd_cat);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    err = dm_shell_register_command(shell, "mkdir", "Create a directory", dm_cmd_mkdir);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    err = dm_shell_register_command(shell, "rm", "Remove a file or directory", dm_cmd_rm);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    return DM_SUCCESS;
} 