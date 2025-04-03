#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/core/memory.h"

// Memory pool structure
struct dm_memory_pool {
    dm_context_t *ctx;
    size_t block_size;
    void **blocks;
    size_t block_count;
    void *current_block;
    size_t current_offset;
    size_t items_per_block;
};

// Memory tracking structure
typedef struct dm_memory_tracker {
    // Allocation statistics
    size_t total_allocations;
    size_t active_allocations;
    size_t total_bytes_allocated;
    size_t current_bytes_allocated;
    size_t peak_bytes_allocated;
    
    // Allocation tracking
    dm_memory_allocation_t *allocations;
    size_t allocations_size;
    size_t allocations_capacity;
} dm_memory_tracker_t;

// Initialize memory tracker
static dm_memory_tracker_t* create_memory_tracker() {
    dm_memory_tracker_t *tracker = malloc(sizeof(dm_memory_tracker_t));
    if (tracker == NULL) {
        return NULL;
    }
    
    memset(tracker, 0, sizeof(dm_memory_tracker_t));
    
    // Initial capacity for allocation tracking
    tracker->allocations_capacity = 1024;
    tracker->allocations = malloc(tracker->allocations_capacity * sizeof(dm_memory_allocation_t));
    if (tracker->allocations == NULL) {
        free(tracker);
        return NULL;
    }
    
    return tracker;
}

// Clean up memory tracker
static void destroy_memory_tracker(dm_memory_tracker_t *tracker) {
    if (tracker == NULL) {
        return;
    }
    
    if (tracker->allocations != NULL) {
        free(tracker->allocations);
    }
    
    free(tracker);
}

// Track an allocation
static void track_allocation(dm_memory_tracker_t *tracker, void *ptr, size_t size, const char *file, int line) {
    if (tracker == NULL || ptr == NULL) {
        return;
    }
    
    // Resize allocation array if needed
    if (tracker->allocations_size >= tracker->allocations_capacity) {
        size_t new_capacity = tracker->allocations_capacity * 2;
        dm_memory_allocation_t *new_allocations = realloc(tracker->allocations, 
            new_capacity * sizeof(dm_memory_allocation_t));
        
        if (new_allocations == NULL) {
            // Can't resize, just record stats without tracking this allocation
            tracker->total_allocations++;
            tracker->active_allocations++;
            tracker->total_bytes_allocated += size;
            tracker->current_bytes_allocated += size;
            
            if (tracker->current_bytes_allocated > tracker->peak_bytes_allocated) {
                tracker->peak_bytes_allocated = tracker->current_bytes_allocated;
            }
            return;
        }
        
        tracker->allocations = new_allocations;
        tracker->allocations_capacity = new_capacity;
    }
    
    // Record allocation
    dm_memory_allocation_t *alloc = &tracker->allocations[tracker->allocations_size++];
    alloc->ptr = ptr;
    alloc->size = size;
    alloc->file = file;
    alloc->line = line;
    
    // Update stats
    tracker->total_allocations++;
    tracker->active_allocations++;
    tracker->total_bytes_allocated += size;
    tracker->current_bytes_allocated += size;
    
    if (tracker->current_bytes_allocated > tracker->peak_bytes_allocated) {
        tracker->peak_bytes_allocated = tracker->current_bytes_allocated;
    }
}

// Find allocation index by pointer
static int find_allocation(dm_memory_tracker_t *tracker, void *ptr) {
    if (tracker == NULL || ptr == NULL) {
        return -1;
    }
    
    for (size_t i = 0; i < tracker->allocations_size; i++) {
        if (tracker->allocations[i].ptr == ptr) {
            return (int)i;
        }
    }
    
    return -1;
}

// Remove allocation tracking
static size_t untrack_allocation(dm_memory_tracker_t *tracker, void *ptr) {
    if (tracker == NULL || ptr == NULL) {
        return 0;
    }
    
    int index = find_allocation(tracker, ptr);
    if (index < 0) {
        return 0;
    }
    
    size_t size = tracker->allocations[index].size;
    
    // Remove by swapping with the last element
    if ((size_t)index < tracker->allocations_size - 1) {
        tracker->allocations[index] = tracker->allocations[tracker->allocations_size - 1];
    }
    tracker->allocations_size--;
    
    // Update stats
    tracker->active_allocations--;
    tracker->current_bytes_allocated -= size;
    
    return size;
}

// Allocate memory
void* dm_malloc(dm_context_t *ctx, size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    void *ptr = malloc(size);
    if (ptr == NULL) {
        // Out of memory
        return NULL;
    }
    
    return ptr;
}

// Debug version with file/line tracking
void* dm_malloc_debug(dm_context_t *ctx, size_t size, const char *file, int line) {
    void *ptr = dm_malloc(ctx, size);
    
    if (ptr != NULL && ctx != NULL && ctx->memory_impl != NULL) {
        track_allocation((dm_memory_tracker_t*)ctx->memory_impl, ptr, size, file, line);
    }
    
    return ptr;
}

// Allocate zeroed memory
void* dm_calloc(dm_context_t *ctx, size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    
    // Check for multiplication overflow
    if (size > SIZE_MAX / nmemb) {
        return NULL;
    }
    
    void *ptr = calloc(nmemb, size);
    if (ptr == NULL) {
        // Out of memory
        return NULL;
    }
    
    return ptr;
}

// Debug version with file/line tracking
void* dm_calloc_debug(dm_context_t *ctx, size_t nmemb, size_t size, const char *file, int line) {
    void *ptr = dm_calloc(ctx, nmemb, size);
    
    if (ptr != NULL && ctx != NULL && ctx->memory_impl != NULL) {
        track_allocation((dm_memory_tracker_t*)ctx->memory_impl, ptr, nmemb * size, file, line);
    }
    
    return ptr;
}

// Reallocate memory
void* dm_realloc(dm_context_t *ctx, void *ptr, size_t size) {
    if (ptr == NULL) {
        return dm_malloc(ctx, size);
    }
    
    if (size == 0) {
        dm_free(ctx, ptr);
        return NULL;
    }
    
    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL) {
        // Out of memory, but original pointer is still valid
        return NULL;
    }
    
    return new_ptr;
}

// Debug version with file/line tracking
void* dm_realloc_debug(dm_context_t *ctx, void *ptr, size_t size, const char *file, int line) {
    // For realloc, we need to untrack the old allocation
    size_t old_size = 0;
    if (ptr != NULL && ctx != NULL && ctx->memory_impl != NULL) {
        old_size = untrack_allocation((dm_memory_tracker_t*)ctx->memory_impl, ptr);
    }
    
    void *new_ptr = dm_realloc(ctx, ptr, size);
    
    // Track the new allocation
    if (new_ptr != NULL && ctx != NULL && ctx->memory_impl != NULL) {
        track_allocation((dm_memory_tracker_t*)ctx->memory_impl, new_ptr, size, file, line);
    }
    
    return new_ptr;
}

// Free memory
void dm_free(dm_context_t *ctx, void *ptr) {
    if (ptr == NULL) {
        return;
    }
    
    // Update tracking before freeing
    if (ctx != NULL && ctx->memory_impl != NULL) {
        untrack_allocation((dm_memory_tracker_t*)ctx->memory_impl, ptr);
    }
    
    free(ptr);
}

// Initialize memory subsystem
dm_error_t dm_memory_init(dm_context_t *ctx) {
    if (ctx == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Create memory tracker
    dm_memory_tracker_t *tracker = create_memory_tracker();
    if (tracker == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    ctx->memory_impl = tracker;
    return DM_SUCCESS;
}

// Clean up memory subsystem
void dm_memory_cleanup(dm_context_t *ctx) {
    if (ctx == NULL || ctx->memory_impl == NULL) {
        return;
    }
    
    dm_memory_tracker_t *tracker = (dm_memory_tracker_t*)ctx->memory_impl;
    
    // Check for leaks
    if (tracker->active_allocations > 0) {
        fprintf(stderr, "WARNING: %zu memory leaks detected (%zu bytes not freed)\n",
                tracker->active_allocations, tracker->current_bytes_allocated);
        
        // Print leak details (up to 10)
        size_t leak_count = 0;
        for (size_t i = 0; i < tracker->allocations_size && leak_count < 10; i++) {
            fprintf(stderr, "  Leak: %zu bytes at %p (allocated in %s:%d)\n",
                   tracker->allocations[i].size,
                   tracker->allocations[i].ptr,
                   tracker->allocations[i].file ? tracker->allocations[i].file : "unknown",
                   tracker->allocations[i].line);
            leak_count++;
        }
    }
    
    destroy_memory_tracker(tracker);
    ctx->memory_impl = NULL;
}

// Get memory statistics
dm_error_t dm_memory_get_stats(dm_context_t *ctx, dm_memory_stats_t *stats) {
    if (ctx == NULL || stats == NULL || ctx->memory_impl == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    dm_memory_tracker_t *tracker = (dm_memory_tracker_t*)ctx->memory_impl;
    
    // Copy basic stats
    stats->total_allocations = tracker->total_allocations;
    stats->active_allocations = tracker->active_allocations;
    stats->total_bytes_allocated = tracker->total_bytes_allocated;
    stats->current_bytes_allocated = tracker->current_bytes_allocated;
    stats->peak_bytes_allocated = tracker->peak_bytes_allocated;
    
    // Find largest allocations
    stats->num_largest_allocations = 0;
    
    // Simple bubble sort to find largest allocations (limited by MAX_LARGEST=10)
    const size_t MAX_LARGEST = sizeof(stats->largest_allocations) / sizeof(stats->largest_allocations[0]);
    
    if (tracker->allocations_size > 0) {
        // Copy allocations to temporary array for sorting
        dm_memory_allocation_t sorted[MAX_LARGEST];
        memset(sorted, 0, sizeof(sorted));
        
        // Fill with first MAX_LARGEST allocations
        size_t count = (tracker->allocations_size < MAX_LARGEST) ? 
                        tracker->allocations_size : MAX_LARGEST;
        
        for (size_t i = 0; i < count; i++) {
            sorted[i] = tracker->allocations[i];
        }
        
        // Sort first MAX_LARGEST elements
        for (size_t i = 0; i < count; i++) {
            for (size_t j = i + 1; j < count; j++) {
                if (sorted[j].size > sorted[i].size) {
                    dm_memory_allocation_t temp = sorted[i];
                    sorted[i] = sorted[j];
                    sorted[j] = temp;
                }
            }
        }
        
        // Update with rest of allocations if any are larger
        for (size_t i = count; i < tracker->allocations_size; i++) {
            if (tracker->allocations[i].size > sorted[count-1].size) {
                sorted[count-1] = tracker->allocations[i];
                
                // Re-sort
                for (size_t j = count - 1; j > 0; j--) {
                    if (sorted[j].size > sorted[j-1].size) {
                        dm_memory_allocation_t temp = sorted[j];
                        sorted[j] = sorted[j-1];
                        sorted[j-1] = temp;
                    } else {
                        break;
                    }
                }
            }
        }
        
        // Copy back to stats
        for (size_t i = 0; i < count; i++) {
            stats->largest_allocations[i] = sorted[i];
        }
        stats->num_largest_allocations = count;
    }
    
    return DM_SUCCESS;
}

// Create a memory pool
dm_memory_pool_t* dm_pool_create(dm_context_t *ctx, size_t block_size) {
    dm_memory_pool_t *pool = (dm_memory_pool_t*)dm_malloc(ctx, sizeof(dm_memory_pool_t));
    if (pool == NULL) {
        return NULL;
    }

    pool->ctx = ctx;
    pool->block_size = block_size;
    pool->items_per_block = 4096 / block_size; // Use 4KB blocks by default
    if (pool->items_per_block < 8) {
        pool->items_per_block = 8; // Ensure at least 8 items per block
    }

    pool->blocks = NULL;
    pool->block_count = 0;
    pool->current_block = NULL;
    pool->current_offset = 0;

    return pool;
}

// Allocate from memory pool
void* dm_pool_alloc(dm_memory_pool_t *pool) {
    if (pool == NULL) {
        return NULL;
    }

    // If we need a new block
    if (pool->current_block == NULL || 
        pool->current_offset >= pool->items_per_block * pool->block_size) {
        // Allocate new block
        void *block = dm_malloc(pool->ctx, pool->items_per_block * pool->block_size);
        if (block == NULL) {
            return NULL;
        }

        // Add to blocks array
        void **new_blocks = dm_realloc(pool->ctx, pool->blocks, 
                                    (pool->block_count + 1) * sizeof(void*));
        if (new_blocks == NULL) {
            dm_free(pool->ctx, block);
            return NULL;
        }

        pool->blocks = new_blocks;
        pool->blocks[pool->block_count++] = block;
        pool->current_block = block;
        pool->current_offset = 0;
    }

    // Allocate from current block
    void *ptr = (char*)pool->current_block + pool->current_offset;
    pool->current_offset += pool->block_size;

    return ptr;
}

// Reset memory pool (keep blocks, just reset offset)
void dm_pool_reset(dm_memory_pool_t *pool) {
    if (pool == NULL) {
        return;
    }

    if (pool->block_count > 0) {
        pool->current_block = pool->blocks[0];
        pool->current_offset = 0;
    }
}

// Destroy memory pool
void dm_pool_destroy(dm_memory_pool_t *pool) {
    if (pool == NULL) {
        return;
    }

    // Free all blocks
    for (size_t i = 0; i < pool->block_count; i++) {
        dm_free(pool->ctx, pool->blocks[i]);
    }

    // Free blocks array
    dm_free(pool->ctx, pool->blocks);

    // Free pool structure
    dm_free(pool->ctx, pool);
}

// Aligned matrix allocation for SIMD operations
void* dm_matrix_alloc(dm_context_t *ctx, size_t rows, size_t cols, size_t elem_size) {
    if (rows == 0 || cols == 0 || elem_size == 0) {
        return NULL;
    }
    
    // Check for multiplication overflow
    if (cols > SIZE_MAX / rows || 
        elem_size > SIZE_MAX / (rows * cols)) {
        return NULL;
    }
    
    size_t size = rows * cols * elem_size;
    return dm_malloc(ctx, size);
}

// Free matrix
void dm_matrix_free(dm_context_t *ctx, void *matrix) {
    dm_free(ctx, matrix);
} 