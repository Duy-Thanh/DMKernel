#ifndef DM_KERNEL_H
#define DM_KERNEL_H

#include "../dmkernel.h"

// Maximum backtrace depth
#define DM_BACKTRACE_MAX_FRAMES 32

// Backtrace structure
typedef struct {
    void *frames[DM_BACKTRACE_MAX_FRAMES];
    int frame_count;
    char **symbols;    // Resolved symbol names (if available)
} dm_backtrace_t;

// Kernel health check result
typedef enum {
    DM_HEALTH_OK = 0,
    DM_HEALTH_WARNING,
    DM_HEALTH_CRITICAL
} dm_health_status_t;

// Kernel panic - called when a fatal error occurs that cannot be recovered from
void dm_kernel_panic(dm_context_t *ctx, const char *file, int line, const char *func, const char *fmt, ...);

// Capture backtrace at current position
void dm_capture_backtrace(dm_backtrace_t *backtrace);

// Free backtrace resources
void dm_free_backtrace(dm_backtrace_t *backtrace);

// System health monitoring
dm_health_status_t dm_check_system_health(dm_context_t *ctx);

// Auto-detect critical system conditions and trigger panic if necessary
void dm_kernel_watchdog(dm_context_t *ctx);

// Register watchdog to run periodically
void dm_register_watchdog(dm_context_t *ctx, int interval_ms);

// Simplified macro to call kernel panic
#define DM_PANIC(ctx, fmt, ...) \
    dm_kernel_panic(ctx, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

// Assert that a condition is true, otherwise trigger kernel panic
#define DM_ASSERT(ctx, cond, fmt, ...) \
    do { \
        if (!(cond)) { \
            dm_kernel_panic(ctx, __FILE__, __LINE__, __func__, \
                           "Assertion failed: %s\n" fmt, #cond, ##__VA_ARGS__); \
        } \
    } while(0)

// Check health and panic if critical
#define DM_CHECK_HEALTH(ctx) \
    do { \
        if (dm_check_system_health(ctx) == DM_HEALTH_CRITICAL) { \
            dm_kernel_panic(ctx, __FILE__, __LINE__, __func__, \
                           "Critical system condition detected by health check"); \
        } \
    } while(0)

#endif /* DM_KERNEL_H */ 