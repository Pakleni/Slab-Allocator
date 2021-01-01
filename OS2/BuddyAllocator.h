#pragma once
#define nullptr  0
#include <windows.h>

/*
space[1..N-2]:
	buddy_blocks
space[N-1]:
	buddy_allocator
	buddy->arr[]
	slab_allocator
	[caches]
*/

#define DEBUG 1
#define DEBUG_MUTEX 0

typedef struct buddy_block_s{
	struct buddy_block_s* next;
} buddy_block;

typedef struct{
	unsigned int entries;
	unsigned int n;
	void** arr;
	void** space;

	HANDLE myMutex;
} buddy_allocator;

void factory(void** space, int n, buddy_allocator* start);

int get_block_index(buddy_allocator*, void** block);

void* buddy_alloc(buddy_allocator*, unsigned short pages);

void buddy_free(buddy_allocator*, void*,unsigned int);

void display(buddy_allocator*);

unsigned int size_of(int n);