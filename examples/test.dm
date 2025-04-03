// Basic test script for DMKernel

// Basic arithmetic
10 + 5; // Should be 15

// String operations
"Hello, " + "world!"; // Should concatenate

// Variable assignment
let x = 42;
x; // Should output 42

// Variable update
x = x + 8;
x; // Should be 50

// Block with local scope
{
    let y = 100;
    y + 1; // Should be 101
    
    // Nested block
    {
        let z = 200;
        y + z; // Should be 300
    }
}

// If statement
if (x > 40) {
    "x is greater than 40";
} else {
    "x is not greater than 40";
}

// If-else statement with block
if (x < 30) {
    "x is less than 30";
} else {
    let message = "x is not less than 30";
    message; // Should output this message
}

// While loop test
let i = 0;
let sum = 0;
while (i < 5) {
    sum = sum + i;
    i = i + 1;
}
sum; // Should be 10 (0+1+2+3+4)

// Function definition
function add(a, b) {
    let result = a + b;
    return result;
}

// Function call
add(10, 20); // Should be 30

// Function with multiple operations
function fibonacci(n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

// Call fibonacci
fibonacci(6); // Should be 8 (fibonacci sequence: 0,1,1,2,3,5,8)

// Final expression
"Test completed successfully!"; 