#include <stdio.h>
#include <stdlib.h>

// gcc -w -o error error.c && ./error

int *f(int a) {
  int *b = malloc(sizeof(int));
  *b = 2 * a;
  return b;
}

int main(void) {
  int *p4, *p8;
  p4 = f(4);
  p8 = f(8);
  printf("p4: %i / p8: %i\n", *p4, *p8);
}
