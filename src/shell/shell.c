#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h> // For isatty and fileno
#include "../../include/shell/shell.h"

#define SHELL_INITIAL_BUFFER_SIZE 1024
#define SHELL_MAX_COMMAND_ARGS 64

// Add external declaration for our function
extern dm_error_t dm_register_lang_commands(dm_shell_t *shell);
extern dm_error_t dm_register_fs_commands(dm_shell_t *shell);

// Create a shell
dm_error_t dm_shell_create(dm_context_t *ctx, dm_shell_t **shell) {
    if (ctx == NULL || shell == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    *shell = dm_malloc(ctx, sizeof(dm_shell_t));
    if (*shell == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize shell
    (*shell)->ctx = ctx;
    (*shell)->commands = NULL;
    (*shell)->command_count = 0;
    (*shell)->prompt = strdup("dmkernel> "); // Default prompt
    (*shell)->interactive = isatty(fileno(stdin));
    
    // Allocate input buffer
    (*shell)->buffer_size = SHELL_INITIAL_BUFFER_SIZE;
    (*shell)->input_buffer = dm_malloc(ctx, (*shell)->buffer_size);
    if ((*shell)->input_buffer == NULL) {
        dm_free(ctx, *shell);
        *shell = NULL;
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Register built-in commands
    dm_shell_register_command(*shell, "help", "Display available commands", dm_cmd_help);
    dm_shell_register_command(*shell, "exit", "Exit the shell", dm_cmd_exit);
    dm_shell_register_command(*shell, "version", "Display kernel version", dm_cmd_version);
    dm_shell_register_command(*shell, "exec", "Execute a command string", dm_cmd_exec);
    
    // Register file system commands
    dm_error_t err = dm_register_fs_commands(*shell);
    if (err != DM_SUCCESS) {
        dm_shell_destroy(*shell);
        *shell = NULL;
        return err;
    }
    
    // Register language commands
    err = dm_register_lang_commands(*shell);
    if (err != DM_SUCCESS) {
        dm_shell_destroy(*shell);
        *shell = NULL;
        return err;
    }
    
    return DM_SUCCESS;
}

// Destroy a shell
void dm_shell_destroy(dm_shell_t *shell) {
    if (shell == NULL) {
        return;
    }
    
    // Free commands
    if (shell->commands != NULL) {
        dm_free(shell->ctx, shell->commands);
        shell->commands = NULL;
    }
    
    // Free prompt
    if (shell->prompt != NULL) {
        free(shell->prompt);
        shell->prompt = NULL;
    }
    
    // Free input buffer
    if (shell->input_buffer != NULL) {
        dm_free(shell->ctx, shell->input_buffer);
        shell->input_buffer = NULL;
    }
    
    // Free shell
    dm_free(shell->ctx, shell);
}

// Register a command
dm_error_t dm_shell_register_command(dm_shell_t *shell, const char *name, const char *help, 
                                     dm_error_t (*handler)(dm_context_t *ctx, int argc, char **argv)) {
    if (shell == NULL || name == NULL || handler == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Resize command array
    dm_command_t *new_commands = dm_realloc(shell->ctx, 
                                        shell->commands, 
                                        (shell->command_count + 1) * sizeof(dm_command_t));
    if (new_commands == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    shell->commands = new_commands;
    
    // Add new command
    shell->commands[shell->command_count].name = strdup(name);
    shell->commands[shell->command_count].help = help ? strdup(help) : NULL;
    shell->commands[shell->command_count].handler = handler;
    shell->command_count++;
    
    return DM_SUCCESS;
}

// Split command line into arguments
static int split_args(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;
    bool in_quotes = false;
    char quote_char = 0;
    
    while (*p && argc < max_args) {
        // Skip whitespace
        while (isspace(*p)) {
            p++;
        }
        
        if (*p == '\0') {
            break;
        }
        
        // Start of argument
        argv[argc++] = p;
        
        // Find end of argument
        while (*p && (!isspace(*p) || in_quotes)) {
            if (*p == '\'' || *p == '"') {
                if (!in_quotes) {
                    // Start quotes
                    in_quotes = true;
                    quote_char = *p;
                    // Remove the quote
                    memmove(p, p + 1, strlen(p));
                    continue;
                } else if (*p == quote_char) {
                    // End quotes
                    in_quotes = false;
                    // Remove the quote
                    memmove(p, p + 1, strlen(p));
                    continue;
                }
            }
            p++;
        }
        
        if (*p == '\0') {
            break;
        }
        
        // Null-terminate this argument
        *p++ = '\0';
    }
    
    return argc;
}

// Find a command by name
static dm_command_t* find_command(dm_shell_t *shell, const char *name) {
    for (size_t i = 0; i < shell->command_count; i++) {
        if (strcmp(shell->commands[i].name, name) == 0) {
            return &shell->commands[i];
        }
    }
    return NULL;
}

// Run the shell
dm_error_t dm_shell_run(dm_shell_t *shell) {
    if (shell == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    dm_context_t *ctx = shell->ctx;
    
    // Shell main loop
    while (ctx->running) {
        // Print prompt
        if (shell->interactive) {
            fprintf(ctx->output, "%s", shell->prompt);
            fflush(ctx->output);
        }
        
        // Read line
        if (fgets(shell->input_buffer, shell->buffer_size, ctx->input) == NULL) {
            // EOF or error
            if (shell->interactive) {
                fprintf(ctx->output, "\n");
            }
            break;
        }
        
        // Remove trailing newline
        size_t len = strlen(shell->input_buffer);
        if (len > 0 && shell->input_buffer[len - 1] == '\n') {
            shell->input_buffer[len - 1] = '\0';
        }
        
        // Skip empty lines
        if (shell->input_buffer[0] == '\0') {
            continue;
        }
        
        // Execute the command
        dm_error_t error = dm_shell_execute(shell, shell->input_buffer);
        if (error != DM_SUCCESS && error != DM_ERROR_INVALID_ARGUMENT) {
            fprintf(ctx->error, "Error: %s\n", "Failed to execute command"); // TODO: Better error message
        }
    }
    
    return DM_SUCCESS;
}

// Execute a shell command
dm_error_t dm_shell_execute(dm_shell_t *shell, const char *command) {
    if (shell == NULL || command == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    dm_context_t *ctx = shell->ctx;
    
    // Copy command to avoid modifying the original
    char *cmd_copy = strdup(command);
    if (cmd_copy == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Split command into arguments
    char *argv[SHELL_MAX_COMMAND_ARGS];
    int argc = split_args(cmd_copy, argv, SHELL_MAX_COMMAND_ARGS);
    
    if (argc == 0) {
        free(cmd_copy);
        return DM_SUCCESS; // Nothing to do
    }
    
    // Find command
    dm_command_t *cmd = find_command(shell, argv[0]);
    if (cmd == NULL) {
        // Check if it's a language statement or expression
        // TODO: Handle language expressions
        
        fprintf(ctx->error, "Unknown command: %s\n", argv[0]);
        free(cmd_copy);
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Execute command
    dm_error_t error = cmd->handler(ctx, argc, argv);
    
    free(cmd_copy);
    return error;
}

// Built-in command: help
dm_error_t dm_cmd_help(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Find the shell object (hacky way, in a real implementation we'd pass it properly)
    dm_shell_t *shell = NULL;
    // This assumes we store a pointer to the shell in some context field
    // For now, let's just simulate it by listing hardcoded commands
    
    fprintf(ctx->output, "Available commands:\n");
    fprintf(ctx->output, "  help                 - Display this help message\n");
    fprintf(ctx->output, "  exit                 - Exit the shell\n");
    fprintf(ctx->output, "  version              - Display kernel version\n");
    fprintf(ctx->output, "  run <filename>       - Run a script file\n");
    fprintf(ctx->output, "  exec <code>          - Execute a code snippet\n");
    
    return DM_SUCCESS;
}

// Built-in command: exit
dm_error_t dm_cmd_exit(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    ctx->running = false;
    
    return DM_SUCCESS;
}

// Built-in command: version
dm_error_t dm_cmd_version(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    fprintf(ctx->output, "DMKernel version %d.%d.%d\n", 
            DM_KERNEL_VERSION_MAJOR, 
            DM_KERNEL_VERSION_MINOR, 
            DM_KERNEL_VERSION_PATCH);
    
    return DM_SUCCESS;
}

// Built-in command: exec
dm_error_t dm_cmd_exec(dm_context_t *ctx, int argc, char **argv) {
    if (ctx == NULL || argc < 2) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Combine all remaining arguments into a single code string
    char code[1024] = "";
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            strcat(code, " ");
        }
        strcat(code, argv[i]);
    }
    
    // Print code being executed
    fprintf(ctx->output, "Executing: %s\n", code);
    
    // Execute the code
    dm_error_t err = dm_execute(ctx, code);
    if (err != DM_SUCCESS) {
        fprintf(ctx->error, "Error executing code: %s\n", dm_error_string(err));
        return err;
    }
    
    return DM_SUCCESS;
}