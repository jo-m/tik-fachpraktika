#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

// gcc -w -o args args.c && ./args 2 3

void err_exit(int c) {
  printf("Invalid or not enough arguments received (Error code: %d)\n", errno);
  puts("    Usage: ./args a b");
  exit(1);
}

int main(int argc, char const **argv) {
  long a,b;
  if(argc < 3) err_exit(1);
  a = strtol(argv[1], NULL, 10);
  if(0 != errno) err_exit(2);
  b = strtol(argv[2], NULL, 10);
  if(0 != errno) err_exit(3);

  printf("a = %d; b = %d; a * b = %d\n", a, b, a * b);

  return 0;
}
