#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/dmkernel.h"

// Function to deliberately cause a segmentation fault
static void trigger_segfault(void) {
    printf("Triggering segmentation fault...\n");
    int *ptr = NULL;
    *ptr = 42; // This will cause a segmentation fault
}

// Function to consume memory
static void* consume_memory(size_t amount) {
    printf("Allocating %zu bytes of memory...\n", amount);
    return malloc(amount);
}

// Nested functions to demonstrate backtrace
static void function_level_3(dm_context_t *ctx) {
    printf("In function_level_3, about to panic\n");
    DM_PANIC(ctx, "Test panic from nested function call");
    printf("This line should never be reached\n");
}

static void function_level_2(dm_context_t *ctx) {
    printf("In function_level_2, calling function_level_3\n");
    function_level_3(ctx);
}

static void function_level_1(dm_context_t *ctx) {
    printf("In function_level_1, calling function_level_2\n");
    function_level_2(ctx);
}

// Test watchdog
static void test_watchdog(dm_context_t *ctx) {
    printf("Testing system health monitoring and watchdog...\n");
    
    // Register the kernel watchdog
    dm_register_watchdog(ctx, 1000); // 1 second interval
    
    // Check system health
    dm_health_status_t health = dm_check_system_health(ctx);
    printf("Current system health: %d\n", health);
    
    // Allocate memory directly using the DMKernel memory allocator
    void *memory1 = DM_MALLOC(ctx, 10 * 1024 * 1024); // 10MB
    printf("Allocated 10MB with DM_MALLOC\n");
    
    // Check health status again
    health = dm_check_system_health(ctx);
    printf("System health after allocation: %d (0=OK, 1=WARNING, 2=CRITICAL)\n", health);
    
    if (health != DM_HEALTH_CRITICAL) {
        // Allocate even more memory if needed
        void *memory2 = DM_MALLOC(ctx, 5 * 1024 * 1024); // 5MB more
        printf("Allocated additional 5MB with DM_MALLOC\n");
    }
    
    // Trigger watchdog check
    printf("Triggering watchdog check...\n");
    dm_kernel_watchdog(ctx);
    
    printf("This line should never be reached if watchdog detects critical condition\n");
}

// Test backtrace
static void test_backtrace(dm_context_t *ctx) {
    printf("Testing backtrace with nested function calls...\n");
    function_level_1(ctx);
    printf("This line should never be reached\n");
}

// Test signal handling
static void test_signal_handling(void) {
    printf("Testing signal handling with segmentation fault...\n");
    sleep(1); // Give a moment to read the message
    trigger_segfault();
    printf("This line should never be reached\n");
}

int main(int argc, char **argv) {
    printf("DMKernel Watchdog and Backtrace Test\n");
    printf("====================================\n\n");
    
    // Create context
    dm_context_t *ctx = NULL;
    dm_error_t err = dm_init(&ctx);
    if (err != DM_SUCCESS) {
        fprintf(stderr, "Failed to initialize DMKernel: %d\n", err);
        return 1;
    }
    
    printf("Choose a test to run:\n");
    printf("1. Manual kernel panic with backtrace\n");
    printf("2. Automatic panic through system health check\n");
    printf("3. Signal handler panic (segfault)\n");
    printf("Enter choice (1-3): ");
    
    int choice = 0;
    if (scanf("%d", &choice) != 1) {
        choice = 1; // Default
    }
    
    // Clear input buffer
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    
    printf("\n");
    
    switch (choice) {
        case 2:
            test_watchdog(ctx);
            break;
        case 3:
            // Register signal handlers
            dm_register_watchdog(ctx, 1000);
            test_signal_handling();
            break;
        case 1:
        default:
            test_backtrace(ctx);
            break;
    }
    
    // This should never be reached due to panic
    dm_cleanup(ctx);
    printf("Test completed successfully\n");
    
    return 0;
} 