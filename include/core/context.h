#ifndef DM_CONTEXT_H
#define DM_CONTEXT_H

#include "../dmkernel.h"

// Symbol table entry
typedef struct dm_symbol {
    char *name;
    dm_value_t value;
    struct dm_symbol *next;
} dm_symbol_t;

// Symbol table (scope)
typedef struct dm_scope {
    dm_symbol_t **symbols;
    size_t size;
    struct dm_scope *parent;
} dm_scope_t;

// Execution context
struct dm_context {
    // Memory management
    void *memory_impl;
    
    // Current scope
    dm_scope_t *global_scope;
    dm_scope_t *current_scope;
    
    // Error handling
    dm_error_t last_error;
    char error_message[256];
    
    // Input/output
    FILE *input;
    FILE *output;
    FILE *error;
    
    // Runtime state
    bool running;
    int exit_code;
    bool interactive;  // Whether we're in interactive mode
    
    // Command history
    char **history;
    size_t history_size;
    size_t history_capacity;
};

// Context management functions
dm_error_t dm_context_create(dm_context_t **ctx);
void dm_context_destroy(dm_context_t *ctx);
void dm_context_set_error(dm_context_t *ctx, const char *message);

// Scope management
dm_scope_t* dm_scope_create(dm_context_t *ctx, dm_scope_t *parent);
void dm_scope_destroy(dm_context_t *ctx, dm_scope_t *scope);
dm_error_t dm_scope_define(dm_context_t *ctx, dm_scope_t *scope, const char *name, dm_value_t value);
dm_error_t dm_scope_lookup(dm_context_t *ctx, dm_scope_t *scope, const char *name, dm_value_t *value);

// Value management
void dm_value_init(dm_value_t *value);
void dm_value_copy(dm_context_t *ctx, dm_value_t *dest, const dm_value_t *src);
void dm_value_free(dm_context_t *ctx, dm_value_t *value);

#endif /* DM_CONTEXT_H */ 