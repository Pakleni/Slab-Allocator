#include "BuddyAllocator.h"
#include "slab.h"
#include <string.h>
#include <stdbool.h>
#include <Math.h>

void display(buddy_allocator * buddy) {
	printf("Blocks: %d\n", buddy->n);
	printf("Entries: %d\n", buddy->entries);

	for (int i = 0; i < buddy->entries; i++) {

		printf("%d|", i);

		if (buddy->arr[i] == 0) {
			printf("-> X");
		}
		else {
			buddy_block* curr = (buddy_block*)buddy->arr[i];

			while (curr) {
				int i = get_block_index(buddy, (void**)curr);
				printf( "-> %d", i);
				if (i == -1) printf("FATAL ERROR, block index -1");
				curr = curr->next;
			}
		}

		printf("\n");
	}
	printf("\n");
}

unsigned int size_of(int n) {
	unsigned int entries = floor(log2((double)n)) + 1;

	return (double)sizeof(buddy_allocator) + sizeof(void**) * entries;
}

void factory(void** space, int n, buddy_allocator* buddy) {

	unsigned int entries = floor(log2((double)n)) + 1;

	//acb of array
	void** arr = (void**)(buddy + 1);

	buddy->entries = entries;
	buddy->n = n;
	buddy->arr = arr;
	buddy->space = space;
	
	void** curr = space;
	unsigned int d = n;

	for (int i = 0; i < entries; i++) {
		arr[i] = 0;
	}
	while (d != 0) {
		int ld = floor(log2(d));

		if (arr[ld] == 0) {
			arr[ld] = curr;
			((buddy_block*)arr[ld])->next = nullptr;
		}
		else {
			buddy_block* bcb = (buddy_block*)arr[ld];

			while (bcb->next) bcb = bcb->next;

			bcb->next = (buddy_block*)curr;
			bcb->next->next = nullptr;
		}

		unsigned int size = pow(2, ld);
		d -= size;
		curr = (void**)((char*)curr + BLOCK_SIZE * size);
	}
}

int get_block_index(buddy_allocator* buddy, void** block) {
	int i = ((char*)block - (char*)buddy->space) / BLOCK_SIZE;
	if (i >= 0 && i < buddy->n) {
		return i;
	}
	else {
		return -1;
	}
}

void* buddy_alloc(buddy_allocator * buddy, unsigned short pages) {
	WaitForSingleObject(buddy->myMutex, INFINITE);
	void* temp = nullptr;


	int start = round(log2(pages));
	int i = start;

	if (buddy->arr[i] != nullptr) {
		temp = buddy->arr[i];
		buddy->arr[i] = ((buddy_block*)buddy->arr[i])->next;
		ReleaseMutex(buddy->myMutex);
		return temp;
	}

	++i;

	for (; i < buddy->entries; ++i) {
		if (buddy->arr[i] != nullptr) {
			temp = buddy->arr[i];
			buddy->arr[i] = ((buddy_block*)buddy->arr[i])->next;
			break;
		}
	}

	if (temp == nullptr) {
		ReleaseMutex(buddy->myMutex);
		return 0;
	}

	unsigned int size = pow(2, i);

	i--;

	for (; i >= start; i--) {
		size /= 2;
		void* curr = ((char*)temp + BLOCK_SIZE * size);
		((buddy_block*)curr)->next = buddy->arr[i];

		buddy->arr[i] = curr;
	}

	ReleaseMutex(buddy->myMutex);
	return temp;
}

void buddy_free(buddy_allocator* buddy, void* curr, unsigned int pages) {
	WaitForSingleObject(buddy->myMutex, INFINITE);

	//error not my block
	if ((char*)curr < (char*)buddy->space || (char*)curr >((char*)buddy->space + buddy->n * BLOCK_SIZE)) {
		if (DEBUG) {
			printf("FATAL ERROR NOT IN BUDDY's PAGES: %p\n", curr);
		}
		ReleaseMutex(buddy->myMutex);
		return;
	}

	int i = round(log2(pages));
	unsigned int size = pow(2,i) * BLOCK_SIZE;

	buddy_block** prev = nullptr;
	buddy_block** temp = nullptr;

	bool repeat = true;
	

	while(repeat) {
		prev = nullptr;
		temp = (buddy_block**)&buddy->arr[i];

		bool left = (get_block_index(buddy, (void**)curr) % (int)pow(2, (double)i + 1)) == 0;

		while (*temp && *temp < curr) {
			prev = temp;
			temp = &(*temp)->next;
			if (prev == temp) {
				if (DEBUG) {
					printf("FATAL ERROR DOUBLE FREED\n");
				}
				ReleaseMutex(buddy->myMutex);
				return;
			}
		}

		if (*temp == curr) {
			if (DEBUG) {
				printf("duplicate free: %p\n", curr);
			}
			ReleaseMutex(buddy->myMutex);
			return;
		}

		if (left && *temp == (void*)((char*)curr + size)) {
			if (prev) {
				(*prev)->next = (*temp)->next;
			}
			else {
				buddy->arr[i] = (*temp)->next;
			}
			++i;
			size *= 2;
		}
		else if (!left && prev && *prev == (void*)((char*)curr - size)) {
			curr = (*prev);
			*prev = *temp;
			++i;
			size *= 2;
		}
		else {
			repeat = false;
		}
	}

	((buddy_block*)curr)->next = (*temp);

	if (prev) {
		(*prev)->next = (buddy_block*)curr;
	}
	else {
		buddy->arr[i] = (buddy_block*)curr;
	}

	ReleaseMutex(buddy->myMutex);
}