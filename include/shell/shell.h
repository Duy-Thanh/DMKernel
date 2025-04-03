#ifndef DM_SHELL_H
#define DM_SHELL_H

#include "../dmkernel.h"

// Command structure
typedef struct dm_command {
    const char *name;
    const char *help;
    dm_error_t (*handler)(dm_context_t *ctx, int argc, char **argv);
} dm_command_t;

// Shell context
typedef struct dm_shell {
    dm_context_t *ctx;
    dm_command_t *commands;
    size_t command_count;
    char *prompt;
    bool interactive;
    char *input_buffer;
    size_t buffer_size;
} dm_shell_t;

// Shell functions
dm_error_t dm_shell_create(dm_context_t *ctx, dm_shell_t **shell);
void dm_shell_destroy(dm_shell_t *shell);
dm_error_t dm_shell_register_command(dm_shell_t *shell, const char *name, const char *help, 
                                     dm_error_t (*handler)(dm_context_t *ctx, int argc, char **argv));
dm_error_t dm_shell_run(dm_shell_t *shell);
dm_error_t dm_shell_execute(dm_shell_t *shell, const char *command);

// Built-in commands
dm_error_t dm_cmd_help(dm_context_t *ctx, int argc, char **argv);
dm_error_t dm_cmd_exit(dm_context_t *ctx, int argc, char **argv);
dm_error_t dm_cmd_version(dm_context_t *ctx, int argc, char **argv);
dm_error_t dm_cmd_run(dm_context_t *ctx, int argc, char **argv);
dm_error_t dm_cmd_exec(dm_context_t *ctx, int argc, char **argv);

#endif /* DM_SHELL_H */ 