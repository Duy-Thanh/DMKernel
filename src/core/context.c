#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For isatty
#include "../../include/core/context.h"
#include "../../include/core/memory.h"

// Hash function for symbol table
static size_t hash_string(const char *str, size_t size) {
    size_t hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash % size;
}

// Create a new execution context
dm_error_t dm_context_create(dm_context_t **ctx) {
    if (ctx == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    *ctx = malloc(sizeof(dm_context_t));
    if (*ctx == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    memset(*ctx, 0, sizeof(dm_context_t));
    
    // Set up I/O
    (*ctx)->input = stdin;
    (*ctx)->output = stdout;
    (*ctx)->error = stderr;
    
    // Initialize memory tracking
    dm_error_t error = dm_memory_init(*ctx);
    if (error != DM_SUCCESS) {
        free(*ctx);
        *ctx = NULL;
        return error;
    }
    
    // Set up global scope
    (*ctx)->global_scope = dm_scope_create(*ctx, NULL);
    if ((*ctx)->global_scope == NULL) {
        dm_memory_cleanup(*ctx);
        free(*ctx);
        *ctx = NULL;
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    (*ctx)->current_scope = (*ctx)->global_scope;
    
    // Set up running state
    (*ctx)->running = true;
    (*ctx)->exit_code = 0;
    (*ctx)->interactive = isatty(fileno(stdin));  // Set interactive mode based on whether stdin is a TTY
    
    return DM_SUCCESS;
}

// Destroy an execution context
void dm_context_destroy(dm_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    // Free scopes
    if (ctx->global_scope != NULL) {
        dm_scope_destroy(ctx, ctx->global_scope);
    }
    
    // Free command history
    if (ctx->history != NULL) {
        for (size_t i = 0; i < ctx->history_size; i++) {
            free(ctx->history[i]);
        }
        free(ctx->history);
    }
    
    // Cleanup memory tracking last (after all other resources are freed)
    dm_memory_cleanup(ctx);
    
    // Free context itself
    free(ctx);
}

// Create a new scope
dm_scope_t* dm_scope_create(dm_context_t *ctx, dm_scope_t *parent) {
    if (ctx == NULL) {
        return NULL;
    }
    
    dm_scope_t *scope = dm_malloc(ctx, sizeof(dm_scope_t));
    if (scope == NULL) {
        return NULL;
    }
    
    // Initialize symbol table with 64 buckets
    const size_t table_size = 64;
    scope->symbols = dm_calloc(ctx, table_size, sizeof(dm_symbol_t*));
    if (scope->symbols == NULL) {
        dm_free(ctx, scope);
        return NULL;
    }
    
    scope->size = table_size;
    scope->parent = parent;
    
    return scope;
}

// Destroy a scope
void dm_scope_destroy(dm_context_t *ctx, dm_scope_t *scope) {
    if (ctx == NULL || scope == NULL) {
        return;
    }
    
    // Free all symbols in the table
    for (size_t i = 0; i < scope->size; i++) {
        dm_symbol_t *symbol = scope->symbols[i];
        while (symbol != NULL) {
            dm_symbol_t *next = symbol->next;
            
            // Free symbol name
            dm_free(ctx, symbol->name);
            
            // Free symbol value
            dm_value_free(ctx, &symbol->value);
            
            // Free symbol struct
            dm_free(ctx, symbol);
            
            symbol = next;
        }
    }
    
    // Free symbol table
    dm_free(ctx, scope->symbols);
    
    // Free scope struct
    dm_free(ctx, scope);
}

// Define a symbol in a scope
dm_error_t dm_scope_define(dm_context_t *ctx, dm_scope_t *scope, const char *name, dm_value_t value) {
    if (ctx == NULL || scope == NULL || name == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Calculate hash bucket
    size_t hash = hash_string(name, scope->size);
    
    // Check if symbol already exists
    dm_symbol_t *symbol = scope->symbols[hash];
    while (symbol != NULL) {
        if (strcmp(symbol->name, name) == 0) {
            // Symbol exists, free old value and update
            dm_value_free(ctx, &symbol->value);
            dm_value_copy(ctx, &symbol->value, &value);
            return DM_SUCCESS;
        }
        symbol = symbol->next;
    }
    
    // Create new symbol
    symbol = dm_malloc(ctx, sizeof(dm_symbol_t));
    if (symbol == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Copy name
    symbol->name = dm_malloc(ctx, strlen(name) + 1);
    if (symbol->name == NULL) {
        dm_free(ctx, symbol);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    strcpy(symbol->name, name);
    
    // Copy value
    dm_value_init(&symbol->value);
    dm_value_copy(ctx, &symbol->value, &value);
    
    // Insert at head of bucket
    symbol->next = scope->symbols[hash];
    scope->symbols[hash] = symbol;
    
    return DM_SUCCESS;
}

// Look up a symbol in a scope and its parents
dm_error_t dm_scope_lookup(dm_context_t *ctx, dm_scope_t *scope, const char *name, dm_value_t *value) {
    if (ctx == NULL || scope == NULL || name == NULL || value == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Start at current scope
    dm_scope_t *current = scope;
    
    while (current != NULL) {
        // Calculate hash bucket
        size_t hash = hash_string(name, current->size);
        
        // Search in this scope
        dm_symbol_t *symbol = current->symbols[hash];
        while (symbol != NULL) {
            if (strcmp(symbol->name, name) == 0) {
                // Found symbol, copy value
                dm_value_copy(ctx, value, &symbol->value);
                return DM_SUCCESS;
            }
            symbol = symbol->next;
        }
        
        // Not found in this scope, check parent
        current = current->parent;
    }
    
    // Symbol not found
    return DM_ERROR_INVALID_ARGUMENT;
}

// Initialize a value to null
void dm_value_init(dm_value_t *value) {
    if (value == NULL) {
        return;
    }
    
    value->type = DM_TYPE_NULL;
    memset(&value->as, 0, sizeof(value->as));
}

// Copy a value
void dm_value_copy(dm_context_t *ctx, dm_value_t *dest, const dm_value_t *src) {
    if (ctx == NULL || dest == NULL || src == NULL) {
        return;
    }
    
    // Free any existing value in dest
    dm_value_free(ctx, dest);
    
    // Copy type
    dest->type = src->type;
    
    // Copy contents based on type
    switch (src->type) {
        case DM_TYPE_NULL:
            // Nothing to copy
            break;
            
        case DM_TYPE_BOOLEAN:
            dest->as.boolean = src->as.boolean;
            break;
            
        case DM_TYPE_INTEGER:
            dest->as.integer = src->as.integer;
            break;
            
        case DM_TYPE_FLOAT:
            dest->as.floating = src->as.floating;
            break;
            
        case DM_TYPE_STRING:
            // Copy string
            dest->as.string.length = src->as.string.length;
            dest->as.string.data = dm_malloc(ctx, src->as.string.length + 1);
            if (dest->as.string.data != NULL) {
                memcpy(dest->as.string.data, src->as.string.data, src->as.string.length + 1);
            }
            break;
            
        case DM_TYPE_ARRAY:
            // Copy array
            dest->as.array.length = src->as.array.length;
            dest->as.array.capacity = src->as.array.length;
            
            if (src->as.array.length > 0) {
                dest->as.array.items = dm_malloc(ctx, sizeof(dm_value_t) * src->as.array.length);
                if (dest->as.array.items != NULL) {
                    for (size_t i = 0; i < src->as.array.length; i++) {
                        dm_value_init(&dest->as.array.items[i]);
                        dm_value_copy(ctx, &dest->as.array.items[i], &src->as.array.items[i]);
                    }
                }
            } else {
                dest->as.array.items = NULL;
            }
            break;
            
        case DM_TYPE_MATRIX:
            // Copy matrix (shallow copy for now)
            dest->as.matrix = src->as.matrix;
            break;
            
        case DM_TYPE_OBJECT:
        case DM_TYPE_FUNCTION:
            // Simple copy for now
            dest->as = src->as;
            break;
    }
}

// Free a value's resources
void dm_value_free(dm_context_t *ctx, dm_value_t *value) {
    if (ctx == NULL || value == NULL) {
        return;
    }
    
    // Free resources based on type
    switch (value->type) {
        case DM_TYPE_NULL:
        case DM_TYPE_BOOLEAN:
        case DM_TYPE_INTEGER:
        case DM_TYPE_FLOAT:
            // Nothing to free
            break;
            
        case DM_TYPE_STRING:
            // Free string data
            if (value->as.string.data != NULL) {
                dm_free(ctx, value->as.string.data);
                value->as.string.data = NULL;
            }
            break;
            
        case DM_TYPE_ARRAY:
            // Free array items
            if (value->as.array.items != NULL) {
                for (size_t i = 0; i < value->as.array.length; i++) {
                    dm_value_free(ctx, &value->as.array.items[i]);
                }
                dm_free(ctx, value->as.array.items);
                value->as.array.items = NULL;
            }
            value->as.array.length = 0;
            value->as.array.capacity = 0;
            break;
            
        case DM_TYPE_MATRIX:
            // Free matrix data
            if (value->as.matrix.data != NULL) {
                dm_matrix_free(ctx, value->as.matrix.data);
                value->as.matrix.data = NULL;
            }
            break;
            
        case DM_TYPE_OBJECT:
            // TODO: Implement object freeing
            break;
            
        case DM_TYPE_FUNCTION:
            // Nothing to free for primitive functions
            break;
    }
    
    // Reset to NULL type
    value->type = DM_TYPE_NULL;
}

// Set error message in context
void dm_context_set_error(dm_context_t *ctx, const char *message) {
    if (ctx == NULL || message == NULL) {
        return;
    }
    
    // Copy the message with truncation if needed
    strncpy(ctx->error_message, message, sizeof(ctx->error_message) - 1);
    ctx->error_message[sizeof(ctx->error_message) - 1] = '\0';
} 