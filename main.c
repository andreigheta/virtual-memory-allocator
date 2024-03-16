#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "vma.h"

#define DIE(assertion, call_description)				\
	do {								\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",			\
					__FILE__, __LINE__);		\
			perror(call_description);			\
			exit(errno);				        \
		}							\
	} while (0)

#define MAX_STRING_SIZE 64

int main(void)
{
	arena_t *arena = NULL;
	uint64_t size, address;
	int8_t *data = NULL;
	int8_t *permission[MAX_STRING_SIZE];

	while (1) {
		char command[MAX_STRING_SIZE];
		scanf("%s", command);

		if (strcmp(command, "ALLOC_ARENA") == 0) {
			scanf("%ld", &size);
			arena = alloc_arena(size);
		} else if (strcmp(command, "DEALLOC_ARENA") == 0) {
			dealloc_arena(arena);
			break;
		} else if (strcmp(command, "ALLOC_BLOCK") == 0) {
			scanf("%ld%ld", &address, &size);
			alloc_block(arena, address, size);
		} else if (strcmp(command, "FREE_BLOCK") == 0) {
			scanf("%ld", &address);
			free_block(arena, address);
		} else if (strcmp(command, "READ") == 0) {
			scanf("%ld%ld", &address, &size);
			read(arena, address, size);
		} else if (strcmp(command, "WRITE") == 0) {
			scanf("%ld%ld", &address, &size);
			scanf("%*c");
			data = (int8_t *)calloc(sizeof(int8_t), size);
			DIE(!data, "data malloc");
			fread(data, sizeof(int8_t), size, stdin);
			write(arena, address, size, data);
			free(data);
		} else if (strcmp(command, "PMAP") == 0) {
			pmap(arena);
		} else if (strcmp(command, "MPROTECT") == 0) {
			scanf("%ld", &address);
			fgets((char *)permission, MAX_STRING_SIZE, stdin);
			mprotect(arena, address, permission);
		} else {
			printf("Invalid command. Please try again.\n");
		}
	}
	return 0;
}
