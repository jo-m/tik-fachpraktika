// gcc -Wall -O2 -o linked_list linked_list.c && ./linked_list
/* valgrind ./a.out */

#include <stdio.h>
#include <stdlib.h>

struct elem {
	int pos;
	struct elem *next;
};

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

static void xfree(void *ptr)
{
	if (ptr == NULL)
		panic("Got Null-Pointer!\n");
	free(ptr);
}

static struct elem *init_list(size_t len)
{
	struct elem *first = NULL;
	struct elem *prev = NULL;
	struct elem *head = NULL;
	for(int i = 0; i < len; i++) {
		head = xmalloc(sizeof(struct elem));
		if(NULL == first) {
			first = head;
		}
		if(NULL != prev) {
			prev->next = head;
		}
		head->pos = i + 1;
		prev = head;
	}
	head->next = first;
	return first;
}

static void clean_list(struct elem *head)
{
	struct elem *first = head;
	do {
		struct elem *next = head->next;
		xfree(head);
		head = next;
	} while(head != first);
}

static void traverse_list(struct elem *head, int times)
{
	for(int i = 0; i < times; i++) {
		struct elem *first = head, *tip = head;
		do {
			printf("traverse %d\n", tip->pos);
		} while(first != (tip = tip->next));
	}
}

int main(void)
{
	struct elem *head = NULL;
	size_t len = 10;

	head = init_list(len);
	traverse_list(head, 2);
	clean_list(head);

	return 0;
}
