#ifndef DM_MEMORY_H
#define DM_MEMORY_H

#include <stddef.h>
#include "../dmkernel.h"

// Memory allocation tracking for each block
typedef struct {
    void *ptr;
    size_t size;
    const char *file;
    int line;
} dm_memory_allocation_t;

// Memory statistics structure
typedef struct {
    size_t total_allocations;      // Total number of allocations made
    size_t active_allocations;     // Current number of active allocations
    size_t total_bytes_allocated;  // Total bytes allocated over lifetime
    size_t current_bytes_allocated; // Current bytes in use
    size_t peak_bytes_allocated;   // Peak memory usage
    
    // Largest allocations for debugging
    dm_memory_allocation_t largest_allocations[10];
    size_t num_largest_allocations;
} dm_memory_stats_t;

// Memory subsystem initialization and cleanup
dm_error_t dm_memory_init(dm_context_t *ctx);
void dm_memory_cleanup(dm_context_t *ctx);

// Memory allocation functions optimized for data mining workloads
void* dm_malloc(dm_context_t *ctx, size_t size);
void* dm_calloc(dm_context_t *ctx, size_t nmemb, size_t size);
void* dm_realloc(dm_context_t *ctx, void *ptr, size_t size);
void dm_free(dm_context_t *ctx, void *ptr);

// Enhanced memory allocation with file and line tracking
#define DM_MALLOC(ctx, size) dm_malloc_debug(ctx, size, __FILE__, __LINE__)
#define DM_CALLOC(ctx, nmemb, size) dm_calloc_debug(ctx, nmemb, size, __FILE__, __LINE__)
#define DM_REALLOC(ctx, ptr, size) dm_realloc_debug(ctx, ptr, size, __FILE__, __LINE__)
#define DM_FREE(ctx, ptr) dm_free(ctx, ptr)

// Debug versions with source location tracking
void* dm_malloc_debug(dm_context_t *ctx, size_t size, const char *file, int line);
void* dm_calloc_debug(dm_context_t *ctx, size_t nmemb, size_t size, const char *file, int line);
void* dm_realloc_debug(dm_context_t *ctx, void *ptr, size_t size, const char *file, int line);

// Memory statistics
dm_error_t dm_memory_get_stats(dm_context_t *ctx, dm_memory_stats_t *stats);

// Memory pool for efficient allocation of many small objects
typedef struct dm_memory_pool dm_memory_pool_t;

dm_memory_pool_t* dm_pool_create(dm_context_t *ctx, size_t block_size);
void* dm_pool_alloc(dm_memory_pool_t *pool);
void dm_pool_reset(dm_memory_pool_t *pool);
void dm_pool_destroy(dm_memory_pool_t *pool);

// Matrix memory allocation (aligned for SIMD operations)
void* dm_matrix_alloc(dm_context_t *ctx, size_t rows, size_t cols, size_t elem_size);
void dm_matrix_free(dm_context_t *ctx, void *matrix);

#endif /* DM_MEMORY_H */ 