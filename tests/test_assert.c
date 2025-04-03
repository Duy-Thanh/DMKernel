#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/dmkernel.h"

// Test function that should fail an assertion
void test_assertion_failure(dm_context_t *ctx) {
    int x = 10;
    int y = 0;
    
    // This will trigger a kernel panic through assertion
    DM_ASSERT(ctx, y != 0, "Division by zero would occur! x=%d, y=%d", x, y);
    
    // This code should never be reached
    int result = x / y;
    printf("Result: %d\n", result);
}

// Test function with a valid assertion
void test_assertion_success(dm_context_t *ctx) {
    int x = 10;
    int y = 2;
    
    // This assertion should pass
    DM_ASSERT(ctx, y != 0, "Division by zero would occur! x=%d, y=%d", x, y);
    
    // This code should be reached
    int result = x / y;
    printf("Result: %d\n", result);
}

int main(int argc, char **argv) {
    dm_context_t *ctx = NULL;
    
    // Initialize kernel
    dm_error_t error = dm_init(&ctx);
    if (error != DM_SUCCESS) {
        fprintf(stderr, "Failed to initialize kernel: %s\n", dm_error_string(error));
        return 1;
    }
    
    printf("DMKernel initialized successfully.\n");
    
    // Test successful assertion
    printf("\nTesting successful assertion...\n");
    test_assertion_success(ctx);
    
    // Test failed assertion - will trigger kernel panic
    printf("\nPress Enter to test failed assertion (will trigger kernel panic)...\n");
    getchar();
    test_assertion_failure(ctx);
    
    // This code should never be reached
    printf("This should never be printed\n");
    
    // Clean up
    dm_cleanup(ctx);
    
    return 0;
} 