/* gcc -Wall -O2 substr.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void panic(const char *serror)
{
	printf("%s", serror);
	exit(1);
}

static void *xmalloc(size_t size)
{
	void *ptr;
	if (size == 0)
		panic("Size is 0!\n");
	ptr = malloc(size);
	if (!ptr)
		panic("No mem left!\n");
	return ptr;
}

static char *substring(const char *str, off_t pos, size_t len)
{
	/* ... */
}

int main(int argc, char **argv)
{
	char *foo = "Nicht\n";
	char *bar = substring(foo, 2, 3);
	printf("%s", bar);
	free(bar);
	return 0;
}
