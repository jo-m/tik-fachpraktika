#include <stdio.h>

// gcc fibonacci.c -o fibonacci && ./fibonacci

int fibonacci(n) {
  if(n < 2) {
    return n;
  }

  return fibonacci(n - 1) + fibonacci(n - 2);
}

int main() {
  printf("%d: %d\n", 10, fibonacci(10));
}
