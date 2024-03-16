#include "vma.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define DIE(assertion, call_description)				\
	do {								\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",			\
					__FILE__, __LINE__);		\
			perror(call_description);			\
			exit(errno);				        \
		}							\
	} while (0)

list_t*
dll_create(size_t data_size)
{
	list_t *list = (list_t *)malloc(sizeof(list_t));
	DIE(!list, "list malloc");
	list->head = NULL;
	list->size = 0;
	list->data_size = data_size;

	return list;
}

dll_node_t*
dll_get_nth_node(list_t *list, size_t n)
{
	if (!list || !list->head)
		return NULL;

	dll_node_t *node = list->head;

	n %= list->size;
	while (n--)
		node = node->next;

	return node;
}

void
dll_add_nth_node(list_t *list, unsigned int n, const void *new_data)
{
	if (!list)
		return;

	if (n > list->size)
		n = list->size;

	dll_node_t *node = (dll_node_t *)malloc(sizeof(dll_node_t));
	node->data = malloc(list->data_size * sizeof(char));
	DIE(!node->data, "node->data malloc");
	memcpy(node->data, new_data, list->data_size);

	if (list->size == 0) {
		node->prev = node;
		node->next = node;
		list->head = node;
		list->size++;

		return;
	}

	if (n == 0) {
		dll_node_t *last_node = list->head->prev;
		node->prev = last_node;
		node->next = list->head;
		list->head->prev = node;
		last_node->next = node;
		list->head = node;
		list->size++;

		return;
	}

	dll_node_t *curr = dll_get_nth_node(list, n);

	node->prev = curr->prev;
	node->next = curr;
	node->prev->next = node;
	node->next->prev = node;

	list->size++;
}

dll_node_t*
dll_remove_nth_node(list_t *list, size_t n)
{
	if (!list || !list->head)
		return NULL;

	if (n >= list->size)
		n = list->size - 1;

	dll_node_t *removed = list->head;

	if (n == 0) {
		list->head = removed->next;
		list->head->prev = removed->prev;
		removed->prev->next = list->head;

		list->size--;

		return removed;
	}

	removed = dll_get_nth_node(list, n);

	removed->prev->next = removed->next;
	removed->next->prev = removed->prev;

	list->size--;

	return removed;
}

unsigned int
dll_get_size(list_t *list)
{
	return list->size;
}

void
dll_free(list_t **pp_list)
{
	dll_node_t *curr = (*pp_list)->head;
	dll_node_t *nextnode;
	(*pp_list)->head = NULL;

	while ((*pp_list)->size != 0) {
		nextnode = curr->next;
		free(curr->data);
		free(curr);
		curr = nextnode;
		((*pp_list)->size)--;
	}

	free(*pp_list);
}

void dll_sort(list_t *list)
{
	if (!list->head)
		return;

	dll_node_t *temp1 = list->head;
	dll_node_t *temp2 = list->head;

	block_t *block;
	do {
		temp2 = temp1->next;

		while (temp2 != list->head) {
			if (((block_t *)temp1->data)->start_address >
			    ((block_t *)temp2->data)->start_address) {
				block = temp1->data;
				temp1->data = temp2->data;
				temp2->data = block;
			}
			temp2 = temp2->next;
		}
		temp1 = temp1->next;
	} while (temp1->next != list->head);
}

arena_t *alloc_arena(const uint64_t size)
{
	arena_t *arena = (arena_t *)malloc(sizeof(arena_t));
	DIE(!arena, "arena malloc");
	arena->arena_size = size;
	arena->alloc_list = dll_create(sizeof(block_t));

	return arena;
}

void dealloc_arena(arena_t *arena)
{
	while (arena->alloc_list->size != 0) {
		dll_node_t *rmv_block =
		dll_remove_nth_node(arena->alloc_list, arena->alloc_list->size);

		while
		(((list_t *)((block_t *)rmv_block->data)->mini_list)->size != 0) {
			dll_node_t *rmv_mini =
			dll_remove_nth_node
			((list_t *)((block_t *)rmv_block->data)->mini_list,
			((list_t *)((block_t *)rmv_block->data)->mini_list)->size);

			free(((mini_t *)rmv_mini->data)->rw_buffer);
			free(rmv_mini->data);
			free(rmv_mini);
		}
		free(((block_t *)rmv_block->data)->mini_list);
		free(rmv_block->data);
		free(rmv_block);
	}

	free(arena->alloc_list);
	free(arena);
}

int overwrite(arena_t *arena, dll_node_t *new)
{
	dll_node_t *curr = arena->alloc_list->head;
	int list_size = arena->alloc_list->size - 1;

	// 1. capatul din stanga al lui curr e mai mic decat capatul din stanga al
	// lui new, dar capatul din dreapta al lui curr e mai mare decat capatul
	// din stanga al lui new
	// 2. capatul din stanga al lui curr e mai mic decat capatul din dreapta al
	// lui new, iar capatul din dreapta al lui curr e mai mare decat capatul
	// din dreapta al lui new
	// 3. curr este cuprins total in new
	while (list_size--) {
		if (((block_t *)curr->data)->start_address <
		    ((block_t *)new->data)->start_address +
			((block_t *)new->data)->size &&
			((block_t *)curr->data)->start_address >=
			((block_t *)new->data)->start_address) {
			printf("This zone was already allocated.\n");
			dll_node_t *removed = dll_remove_nth_node(arena->alloc_list,
													  arena->alloc_list->size);
			free(((mini_t *)((dll_node_t *)((list_t *)((block_t *)
				   removed->data)->mini_list)->head)->data)->rw_buffer);
			free(((dll_node_t *)((list_t *)((block_t *)
			       removed->data)->mini_list)->head)->data);
			free(((list_t *)((block_t *)removed->data)->mini_list)->head);
			free(((block_t *)removed->data)->mini_list);
			free(removed->data);
			free(removed);
			return -1;
		}
		if (((block_t *)curr->data)->start_address +
		    ((block_t *)curr->data)->size >
			((block_t *)new->data)->start_address &&
			((block_t *)curr->data)->start_address +
			((block_t *)curr->data)->size <=
			((block_t *)new->data)->start_address +
			((block_t *)new->data)->size) {
			printf("This zone was already allocated.\n");
			dll_node_t *removed = dll_remove_nth_node(arena->alloc_list,
													  arena->alloc_list->size);
			free(((mini_t *)((dll_node_t *)((list_t *)((block_t *)
				   removed->data)->mini_list)->head)->data)->rw_buffer);
			free(((dll_node_t *)((list_t *)((block_t *)
			       removed->data)->mini_list)->head)->data);
			free(((list_t *)((block_t *)removed->data)->mini_list)->head);
			free(((block_t *)removed->data)->mini_list);
			free(removed->data);
			free(removed);
			return -1;
		}
		if (((block_t *)curr->data)->start_address +
		    ((block_t *)curr->data)->size >=
		    ((block_t *)new->data)->start_address +
			((block_t *)new->data)->size &&
			((block_t *)curr->data)->start_address <=
			((block_t *)new->data)->start_address) {
			printf("This zone was already allocated.\n");
			dll_node_t *removed = dll_remove_nth_node(arena->alloc_list,
													  arena->alloc_list->size);
			free(((mini_t *)((dll_node_t *)((list_t *)((block_t *)
				   removed->data)->mini_list)->head)->data)->rw_buffer);
			free(((dll_node_t *)((list_t *)((block_t *)
			       removed->data)->mini_list)->head)->data);
			free(((list_t *)((block_t *)removed->data)->mini_list)->head);
			free(((block_t *)removed->data)->mini_list);
			free(removed->data);
			free(removed);
			return -1;
		}
		curr = curr->next;
	}
	return 0;
}

void block_in_the_middle(arena_t *arena, dll_node_t *curr, dll_node_t *curr2,
						 dll_node_t *new, int index)
{
	((block_t *)new->data)->start_address =
	((block_t *)curr->data)->start_address;
	((block_t *)new->data)->size += ((block_t *)curr->data)->size +
									((block_t *)curr2->data)->size;

	int cnt = 0;
	while (((list_t *)((block_t *)curr->data)->mini_list)->size) {
		dll_add_nth_node(((block_t *)new->data)->mini_list, cnt,
						 ((list_t *)((block_t *)
						 curr->data)->mini_list)->head->data);
		dll_node_t *removed =
		dll_remove_nth_node(((block_t *)curr->data)->mini_list, 0);
		free(removed->data);
		free(removed);
		cnt++;
	}

	while (((list_t *)((block_t *)curr2->data)->mini_list)->size) {
		dll_add_nth_node(((block_t *)new->data)->mini_list,
						 ((list_t *)((block_t *)
						 new->data)->mini_list)->size,
						 ((list_t *)((block_t *)
						 curr2->data)->mini_list)->head->data);
		dll_node_t *removed =
		dll_remove_nth_node(((block_t *)curr2->data)->mini_list, 0);
		free(removed->data);
		free(removed);
	}

	dll_node_t *rmv_block = dll_remove_nth_node(arena->alloc_list,
												index);
	free(((block_t *)rmv_block->data)->mini_list);
	free(rmv_block->data);
	free(rmv_block);

	dll_node_t *rmv_block2 = dll_remove_nth_node(arena->alloc_list,
												 index);
	free(((block_t *)rmv_block2->data)->mini_list);
	free(rmv_block2->data);
	free(rmv_block2);
}

void block_right(arena_t *arena, dll_node_t *curr, dll_node_t *new, int index)
{
	((block_t *)new->data)->start_address =
	((block_t *)curr->data)->start_address;
	((block_t *)new->data)->size +=
	((block_t *)curr->data)->size;
	int cnt = 0;
	while (((list_t *)((block_t *)curr->data)->mini_list)->size) {
		dll_add_nth_node(((block_t *)new->data)->mini_list, cnt,
						 ((list_t *)((block_t *)
						 curr->data)->mini_list)->head->data);
		dll_node_t *removed =
		dll_remove_nth_node(((block_t *)curr->data)->mini_list, 0);
		free(removed->data);
		free(removed);
		cnt++;
	}

	dll_node_t *rmv_block = dll_remove_nth_node(arena->alloc_list,
												index);
	free(((block_t *)rmv_block->data)->mini_list);
	free(rmv_block->data);
	free(rmv_block);
}

void block_left(arena_t *arena, dll_node_t *curr, dll_node_t *new, int index)
{
	((block_t *)new->data)->size +=
	((block_t *)curr->data)->size;
	while (((list_t *)((block_t *)curr->data)->mini_list)->size) {
		dll_add_nth_node(((block_t *)new->data)->mini_list,
						 ((list_t *)((block_t *)
						 new->data)->mini_list)->size,
						 ((list_t *)((block_t *)
						 curr->data)->mini_list)->head->data);
		dll_node_t *removed =
		dll_remove_nth_node(((block_t *)curr->data)->mini_list, 0);
		free(removed->data);
		free(removed);
	}
	dll_node_t *rmv_block = dll_remove_nth_node(arena->alloc_list,
												index);
	free(((block_t *)rmv_block->data)->mini_list);
	free(rmv_block->data);
	free(rmv_block);
}

void add_block(arena_t *arena, dll_node_t *new)
{
	int list_size = arena->alloc_list->size - 1;
	dll_node_t *curr = arena->alloc_list->head;

	int index = 0;
	while (list_size--) {
		dll_node_t *curr2 = curr->next;
		// noul block se adauga intre 2 blockuri
		if (((block_t *)new->data)->start_address ==
			((block_t *)curr->data)->start_address +
			((block_t *)curr->data)->size &&
			((block_t *)new->data)->start_address +
			((block_t *)new->data)->size ==
			((block_t *)curr2->data)->start_address) {
			//
			block_in_the_middle(arena, curr, curr2, new, index);

			break;
		}

		// noul block se adauga in dreapta
		if (((block_t *)new->data)->start_address ==
			((block_t *)curr->data)->start_address +
			((block_t *)curr->data)->size) {
			//
			block_right(arena, curr, new, index);

			break;
		}
		// noul block se adauga in stanga
		if (((block_t *)new->data)->start_address +
			((block_t *)new->data)->size ==
			((block_t *)curr->data)->start_address) {
			//
			block_left(arena, curr, new, index);

			break;
		}
		curr = curr->next;
		index++;
	}
}

void alloc_block(arena_t *arena, const uint64_t address, const uint64_t size)
{
	if (!arena)
		return;

	if (address >= arena->arena_size) {
		printf("The allocated address is outside the size of arena\n");
		return;
	}

	if (address + size > arena->arena_size) {
		printf("The end address is past the size of the arena\n");
		return;
	}

	block_t *block = malloc(sizeof(block_t));
	DIE(!block, "block malloc");
	block->size = size;
	block->start_address = address;
	block->mini_list = dll_create(sizeof(mini_t));

	mini_t *miniblock = malloc(sizeof(mini_t));
	DIE(!miniblock, "miniblock malloc");
	miniblock->size = size;
	miniblock->start_address = address;
	miniblock->perm = 6;
	miniblock->rw_buffer = malloc(sizeof(char) * size);
	DIE(!miniblock->rw_buffer, "rw_buffer malloc");

	// nodul e adaugat indiferent daca e bine sau nu
	dll_add_nth_node(block->mini_list, ((list_t *)block->mini_list)->size,
					 miniblock);
	free(miniblock);

	dll_add_nth_node(arena->alloc_list, arena->alloc_list->size, block);
	dll_node_t *new = dll_get_nth_node(arena->alloc_list,
									   arena->alloc_list->size - 1);
	free(block);

	if (overwrite(arena, new) == -1)
		return;

	add_block(arena, new);

	dll_sort(arena->alloc_list);
}

void free_in_the_middle(arena_t *arena, dll_node_t *curr, dll_node_t *curr2,
						int indexblock)
{
	dll_node_t *delimiter =
	((list_t *)((block_t *)curr->data)->mini_list)->head;

	block_t *block = malloc(sizeof(block_t));
	DIE(!block, "block malloc");
	block->size = ((mini_t *)curr2->data)->start_address -
				  ((block_t *)curr->data)->start_address;
	block->start_address = ((block_t *)curr->data)->start_address;
	block->mini_list = dll_create(sizeof(mini_t));

	dll_add_nth_node(arena->alloc_list, indexblock, block);
	dll_node_t *new = dll_get_nth_node(arena->alloc_list,
									   indexblock);
	free(block);

	while (delimiter != curr2) {
		dll_node_t *rmv_mini =
		dll_remove_nth_node(((block_t *)curr->data)->mini_list, 0);
		((block_t *)curr->data)->size -= ((mini_t *)rmv_mini->data)->size;

		dll_add_nth_node(((block_t *)new->data)->mini_list,
						 ((list_t *)((block_t *)
						 new->data)->mini_list)->size,
						 rmv_mini->data);
		free(rmv_mini->data);
		free(rmv_mini);

		delimiter = ((list_t *)((block_t *)curr->data)->mini_list)->head;
	}

	delimiter = dll_remove_nth_node(((block_t *)curr->data)->mini_list, 0);
	((block_t *)curr->data)->size -= ((mini_t *)delimiter->data)->size;
	free(((mini_t *)delimiter->data)->rw_buffer);
	free(delimiter->data);
	free(delimiter);

	((block_t *)curr->data)->start_address =
	((mini_t *)((list_t *)((block_t *)
	curr->data)->mini_list)->head->data)->start_address;
}

void free_block(arena_t *arena, const uint64_t address)
{
	if (arena->alloc_list->size == 0) {
		printf("Invalid address for free.\n");
		return;
	}

	dll_node_t *curr = arena->alloc_list->head;

	int list_size = arena->alloc_list->size;
	int indexblock = 0;
	int indexmini = 0;
	int found = 0;
	while (list_size--) {
		if (address >= ((block_t *)curr->data)->start_address &&
		    address < ((block_t *)curr->data)->start_address +
			((block_t *)curr->data)->size)
			break;
		indexblock++;
		curr = curr->next;
	}

	int minilist_size = ((list_t *)((block_t *)curr->data)->mini_list)->size;
	dll_node_t *curr2 = ((list_t *)((block_t *)curr->data)->mini_list)->head;

	while (minilist_size--) {
		if (address == ((mini_t *)curr2->data)->start_address) {
			found = 1;
			break;
		}
		indexmini++;
		curr2 = curr2->next;
	}

	if (found == 0) {
		printf("Invalid address for free.\n");
		return;
	}

	// miniblockul se afla la mijloc
	if (((mini_t *)curr2->data)->start_address >
		((block_t *)curr->data)->start_address &&
		((mini_t *)curr2->data)->start_address +
		((mini_t *)curr2->data)->size <
		((block_t *)curr->data)->start_address +
		((block_t *)curr->data)->size) {
		//
		free_in_the_middle(arena, curr, curr2, indexblock);

		return;
	}
	// miniblockul este chiar primul din block
	if (((mini_t *)curr2->data)->start_address ==
		((block_t *)curr->data)->start_address) {
		//
		((block_t *)curr->data)->start_address +=
		((mini_t *)curr2->data)->size;
		((block_t *)curr->data)->size -= ((mini_t *)curr2->data)->size;
	}
	// miniblockul este ultimul din block
	if (((mini_t *)curr2->data)->start_address +
		((mini_t *)curr2->data)->size ==
		((block_t *)curr->data)->start_address +
		((block_t *)curr->data)->size)
		//
		((block_t *)curr->data)->size -= ((mini_t *)curr2->data)->size;

	dll_node_t *rmv_mini =
	dll_remove_nth_node(((block_t *)curr->data)->mini_list, indexmini);
	free(((mini_t *)rmv_mini->data)->rw_buffer);
	free(rmv_mini->data);
	free(rmv_mini);

	if (((list_t *)((block_t *)curr->data)->mini_list)->size == 0) {
		dll_node_t *rmv_block = dll_remove_nth_node(arena->alloc_list,
													indexblock);
		free(((block_t *)rmv_block->data)->mini_list);
		free(rmv_block->data);
		free(rmv_block);
		return;
	}
}

void read(arena_t *arena, uint64_t address, int64_t size)
{
	dll_node_t *curr = arena->alloc_list->head;
	int list_size = arena->alloc_list->size;

	int found = 0;
	while (list_size--) {
		if (address >= ((block_t *)curr->data)->start_address &&
			address < ((block_t *)curr->data)->start_address +
					  ((block_t *)curr->data)->size) {
			found = 1;
			break;
		}
		curr = curr->next;
	}

	if (found == 0) {
		printf("Invalid address for read.\n");
		return;
	}

	if (((block_t *)curr->data)->start_address +
		((block_t *)curr->data)->size < size + address) {
		size = ((block_t *)curr->data)->start_address +
			   ((block_t *)curr->data)->size - address;
		printf("Warning: size was bigger than the block size. ");
		printf("Reading %ld characters.\n", size);
	}

	int minilist_size = ((list_t *)((block_t *)curr->data)->mini_list)->size;
	curr = ((list_t *)((block_t *)curr->data)->mini_list)->head;

	while (minilist_size--) {
		if (address == ((mini_t *)curr->data)->start_address)
			break;
		curr = curr->next;
	}

	if (((mini_t *)curr->data)->perm != 4 &&
		((mini_t *)curr->data)->perm != 6 &&
		((mini_t *)curr->data)->perm != 7) {
		printf("Invalid permissions for read.\n");
		return;
	}

	int offset = address - ((block_t *)curr->data)->start_address;
	if (address + size > (((mini_t *)curr->data)->start_address +
						  ((mini_t *)curr->data)->size)) {
		printf("%.*s", (int)(((mini_t *)curr->data)->start_address +
							((mini_t *)curr->data)->size - address),
					   (char *)(((mini_t *)curr->data)->rw_buffer + offset));
		size -= ((mini_t *)curr->data)->start_address +
				((mini_t *)curr->data)->size - address;
	} else {
		printf("%.*s", (int)size, (char *)
								(((mini_t *)curr->data)->rw_buffer + offset));
		size = 0;
	}

	while (size > 0) {
		curr = curr->next;
		if (address + size > (((mini_t *)curr->data)->start_address +
							  ((mini_t *)curr->data)->size)) {
			printf("%.*s", (int)(((mini_t *)curr->data)->start_address +
								 ((mini_t *)curr->data)->size),
								  (char *)(((mini_t *)curr->data)->rw_buffer));
			size -= ((mini_t *)curr->data)->start_address +
					((mini_t *)curr->data)->size - address;
		} else {
			printf("%.*s", (int)size, ((char *)
									  ((mini_t *)curr->data)->rw_buffer));
			size = 0;
		}
	}

	printf("\n");
}

void write(arena_t *arena, uint64_t address, int64_t size, int8_t *data)
{
	dll_node_t *curr = arena->alloc_list->head;
	int list_size = arena->alloc_list->size;

	int found = 0;
	while (list_size--) {
		if (address >= ((block_t *)curr->data)->start_address &&
			address < ((block_t *)curr->data)->start_address +
					  ((block_t *)curr->data)->size) {
			found = 1;
			break;
		}
		curr = curr->next;
	}

	if (found == 0) {
		printf("Invalid address for write.\n");
		return;
	}

	if (size + address > ((block_t *)curr->data)->start_address +
						 ((block_t *)curr->data)->size) {
		size = ((block_t *)curr->data)->start_address +
			   ((block_t *)curr->data)->size - address;
		printf("Warning: size was bigger than the block size. ");
		printf("Writing %ld characters.\n", size);
	}

	int minilist_size = ((list_t *)((block_t *)curr->data)->mini_list)->size;
	curr = ((list_t *)((block_t *)curr->data)->mini_list)->head;
	while (minilist_size--) {
		if (address == ((mini_t *)curr->data)->start_address)
			break;
		curr = curr->next;
	}

	if (((mini_t *)curr->data)->perm != 2 &&
		((mini_t *)curr->data)->perm != 6 &&
		((mini_t *)curr->data)->perm != 7) {
		printf("Invalid permissions for write.\n");
		return;
	}

	int offset = address - ((block_t *)curr->data)->start_address;
	char delim = '#';
	memset(((mini_t *)curr->data)->rw_buffer, delim, offset);

	if (address + size > ((mini_t *)curr->data)->start_address +
						 ((mini_t *)curr->data)->size) {
		memcpy(((mini_t *)curr->data)->rw_buffer + offset, data,
			   ((mini_t *)curr->data)->start_address +
			   ((mini_t *)curr->data)->size - address);
		size -= ((mini_t *)curr->data)->start_address +
				((mini_t *)curr->data)->size - address;
		data += ((mini_t *)curr->data)->start_address +
				((mini_t *)curr->data)->size - address;
	} else {
		memcpy(((mini_t *)curr->data)->rw_buffer + offset, data, size);
		size = 0;
		data = 0;
	}

	while (size > 0) {
		curr = curr->next;
		if (address + size > ((mini_t *)curr->data)->start_address +
							 ((mini_t *)curr->data)->size) {
			memcpy(((mini_t *)curr->data)->rw_buffer, data,
				   ((mini_t *)curr->data)->start_address +
				   ((mini_t *)curr->data)->size);
			size -= ((mini_t *)curr->data)->start_address +
				((mini_t *)curr->data)->size - address;
			data += ((mini_t *)curr->data)->start_address +
					((mini_t *)curr->data)->size - address;
		} else {
			memcpy(((mini_t *)curr->data)->rw_buffer, data, size);
			size = 0;
			data = 0;
		}
	}
}

char *perm_string(dll_node_t *curr)
{
	char *perm;
	if (((mini_t *)curr->data)->perm == 7)
		perm = "RWX";

	if (((mini_t *)curr->data)->perm == 6)
		perm = "RW-";

	if (((mini_t *)curr->data)->perm == 5)
		perm = "R-X";

	if (((mini_t *)curr->data)->perm == 4)
		perm = "R--";

	if (((mini_t *)curr->data)->perm == 3)
		perm = "-WX";

	if (((mini_t *)curr->data)->perm == 2)
		perm = "-W-";

	if (((mini_t *)curr->data)->perm == 1)
		perm = "--X";

	if (((mini_t *)curr->data)->perm == 0)
		perm = "---";

	return perm;
}

void pmap(const arena_t *arena)
{
	printf("Total memory: 0x%lX bytes\n", arena->arena_size);
	uint64_t freemem = arena->arena_size;
	dll_node_t *curr = arena->alloc_list->head;
	int size = arena->alloc_list->size;
	while (size--) {
		freemem -= ((block_t *)curr->data)->size;
		curr = curr->next;
	}
	printf("Free memory: 0x%lX bytes\n", freemem);
	printf("Number of allocated blocks: %d\n", arena->alloc_list->size);
	size_t miniblocks = 0;
	size = arena->alloc_list->size;
	curr = arena->alloc_list->head;
	while (size--) {
		miniblocks += ((list_t *)((block_t *)curr->data)->mini_list)->size;
		curr = curr->next;
	}
	printf("Number of allocated miniblocks: %ld\n", miniblocks);
	curr = arena->alloc_list->head;
	int blocksize = arena->alloc_list->size;

	for (int i = 0; i < blocksize; i++) {
		printf("\nBlock %d begin\n", i + 1);
		printf("Zone: 0x%lX - 0x%lX\n",
			   ((block_t *)curr->data)->start_address,
			   ((block_t *)curr->data)->start_address +
			   ((block_t *)curr->data)->size);
		int miniblocksize =
		((list_t *)((block_t *)curr->data)->mini_list)->size;

		dll_node_t *curr2 =
		((list_t *)((block_t *)curr->data)->mini_list)->head;

		for (int j = 0; j < miniblocksize; j++) {
			char *perm = perm_string(curr2);
			printf("Miniblock %d:\t\t0x%lX\t\t-\t\t0x%lX\t\t| %s\n", j + 1,
				   ((mini_t *)curr2->data)->start_address,
				   ((mini_t *)curr2->data)->start_address +
				   ((mini_t *)curr2->data)->size, perm);
			curr2 = curr2->next;
		}
		printf("Block %d end\n", i + 1);
		curr = curr->next;
	}
}

void mprotect(arena_t *arena, uint64_t address, int8_t *permission)
{
	dll_node_t *curr = arena->alloc_list->head;
	int list_size = arena->alloc_list->size;
	int found = 0;
	while (list_size--) {
		if (address >= ((block_t *)curr->data)->start_address &&
			address < ((block_t *)curr->data)->start_address +
					  ((block_t *)curr->data)->size) {
			break;
		}
		curr = curr->next;
	}

	int minilist_size = ((list_t *)((block_t *)curr->data)->mini_list)->size;
	curr = ((list_t *)((block_t *)curr->data)->mini_list)->head;

	while (minilist_size--) {
		if (address == ((mini_t *)curr->data)->start_address) {
			found = 1;
			break;
		}
		curr = curr->next;
	}

	if (found == 0) {
		printf("Invalid address for mprotect.\n");
		return;
	}
	((mini_t *)curr->data)->perm = 0;

	if (strstr((char *)permission, "PROT_READ") != 0)
		((mini_t *)curr->data)->perm += 4;

	if (strstr((char *)permission, "PROT_WRITE") != 0)
		((mini_t *)curr->data)->perm += 2;

	if (strstr((char *)permission, "PROT_EXEC") != 0)
		((mini_t *)curr->data)->perm += 1;

	if (strstr((char *)permission, "PROT_NONE") != 0)
		((mini_t *)curr->data)->perm = 0;
}
