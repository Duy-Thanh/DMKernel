#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/dmkernel.h"

/**
 * @brief Duplicate a string using kernel memory allocation.
 * 
 * @param ctx The DMKernel context
 * @param str The string to duplicate
 * @return A newly allocated string or NULL if allocation failed
 */
char *dm_strdup(dm_context_t *ctx, const char *str) {
    if (ctx == NULL || str == NULL) {
        return NULL;
    }
    
    size_t len = strlen(str) + 1;
    char *copy = dm_malloc(ctx, len);
    if (copy == NULL) {
        return NULL;
    }
    
    memcpy(copy, str, len);
    return copy;
} 