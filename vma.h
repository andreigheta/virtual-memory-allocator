#pragma once
#include <inttypes.h>
#include <stddef.h>

typedef struct dll_node_t dll_node_t;
struct dll_node_t {
	void *data;
	dll_node_t *prev, *next;
};

typedef struct {
	dll_node_t *head;
	unsigned int data_size;
	unsigned int size;
} list_t;

typedef struct {
	uint64_t start_address;
	size_t size;
	void *mini_list;
} block_t;

typedef struct {
	uint64_t start_address;
	size_t size;
	uint8_t perm;
	void *rw_buffer;
} mini_t;

typedef struct {
	uint64_t arena_size;
	list_t *alloc_list;
} arena_t;

list_t *dll_create(size_t data_size);
dll_node_t *dll_get_nth_node(list_t *list, size_t n);
void dll_add_nth_node(list_t *list, unsigned int n, const void *new_data);
dll_node_t *dll_remove_nth_node(list_t *list, size_t n);
unsigned int dll_get_size(list_t *list);
void dll_free(list_t **pp_list);

arena_t *alloc_arena(const uint64_t size);
void dealloc_arena(arena_t *arena);

int overwrite(arena_t *arena, dll_node_t *new);
void block_in_the_middle(arena_t *arena, dll_node_t *curr, dll_node_t *curr2,
						 dll_node_t *new, int index);
void block_right(arena_t *arena, dll_node_t *curr, dll_node_t *new, int index);
void block_left(arena_t *arena, dll_node_t *curr, dll_node_t *new, int index);
void add_block(arena_t *arena, dll_node_t *new);
void alloc_block(arena_t *arena, const uint64_t address, const uint64_t size);

void free_in_the_middle(arena_t *arena, dll_node_t *curr, dll_node_t *curr2,
						int indexblock);
void free_block(arena_t *arena, const uint64_t address);

void read(arena_t *arena, uint64_t address, int64_t size);
void write(arena_t *arena, uint64_t address, int64_t size, int8_t *data);
void pmap(const arena_t *arena);

char *perm_string(dll_node_t *curr);
void mprotect(arena_t *arena, uint64_t address, int8_t *permission);
