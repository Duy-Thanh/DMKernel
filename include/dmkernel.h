#ifndef DMKERNEL_H
#define DMKERNEL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

// Version information
#define DM_KERNEL_VERSION_MAJOR 0
#define DM_KERNEL_VERSION_MINOR 1
#define DM_KERNEL_VERSION_PATCH 0

// Error types
typedef enum dm_error_t {
    DM_SUCCESS = 0,
    DM_ERROR_INVALID_ARGUMENT,
    DM_ERROR_MEMORY_ALLOCATION,
    DM_ERROR_FILE_IO,
    DM_ERROR_NOT_FOUND,
    DM_ERROR_ALREADY_EXISTS,
    DM_ERROR_PERMISSION_DENIED,
    DM_ERROR_NOT_SUPPORTED,
    DM_ERROR_BUFFER_OVERFLOW,
    DM_ERROR_TIMEOUT,
    DM_ERROR_BUSY,
    DM_ERROR_WOULD_BLOCK,
    DM_ERROR_INTERRUPTED,
    DM_ERROR_DIVISION_BY_ZERO,
    DM_ERROR_TYPE_MISMATCH,
    DM_ERROR_SYNTAX_ERROR,
    DM_ERROR_UNDEFINED_VARIABLE,
    DM_ERROR_INDEX_OUT_OF_BOUNDS,
    DM_ERROR_STACK_OVERFLOW,
    DM_ERROR_MAX_ERROR
} dm_error_t;

// Forward declarations
typedef struct dm_context dm_context_t;
typedef struct dm_value dm_value_t;
typedef struct dm_object dm_object_t;

// Value types (moved here from context.h)
typedef enum {
    DM_TYPE_NULL,
    DM_TYPE_BOOLEAN,
    DM_TYPE_INTEGER,
    DM_TYPE_FLOAT,
    DM_TYPE_STRING,
    DM_TYPE_ARRAY,
    DM_TYPE_MATRIX,
    DM_TYPE_OBJECT,
    DM_TYPE_FUNCTION
} dm_value_type_t;

// Data value structure (moved here from context.h)
struct dm_value {
    dm_value_type_t type;
    union {
        bool boolean;
        int64_t integer;
        double floating;
        struct {
            char *data;
            size_t length;
        } string;
        struct {
            dm_value_t *items;
            size_t length;
            size_t capacity;
        } array;
        struct {
            void *data;
            size_t rows;
            size_t cols;
            dm_value_type_t elem_type;
        } matrix;
        dm_object_t *object;
        struct {
            dm_error_t (*func)(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
            void *user_data;
        } function;
    } as;
};

// Function pointer types
typedef dm_error_t (*dm_primitive_func_t)(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);

// Include other component headers
#include "core/memory.h"
#include "core/context.h"
#include "core/utils.h"
#include "core/kernel.h"
#include "shell/shell.h"
#include "lang/parser.h"
#include "primitives/primitives.h"

// Public API functions
dm_error_t dm_init(dm_context_t **ctx);
void dm_cleanup(dm_context_t *ctx);
dm_error_t dm_execute(dm_context_t *ctx, const char *code);
dm_error_t dm_execute_file(dm_context_t *ctx, const char *filename);
const char *dm_error_string(dm_error_t error);

#endif /* DMKERNEL_H */ 