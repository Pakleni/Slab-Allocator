#pragma once
#include "slab.h"
#include "BuddyAllocator.h"
#include <windows.h>

#define BUFFERS 13
#define SLAB_END (unsigned short) -1

//flags
#define ERR_NO_MEM		(1)
#define ERR_NOT_MINE	(1 << 1)
#define ERR_ALR_FREE	(1 << 2)
#define EXPANDING		(1 << 3)
#define TRIED_SHRINK	(1 << 4)
#define NO_SHRINK		(1 << 5)
#define DEAD			(1 << 6)




typedef struct slab_s {
	struct slab_s* next_slab;
	unsigned short first_obj;
	unsigned short obj_num;

	char* s_mem;
}kmem_slab;

struct kmem_cache_s {
	const char* name;
	unsigned int size;

	void (*ctor)(void*);
	void (*dtor)(void*);

	kmem_slab* empty;
	kmem_slab* half;
	kmem_slab* full;

	unsigned short pagesperslab;
	unsigned short pagesforslabheader;
	unsigned char flags;

	HANDLE myMutex;

	unsigned int objperslab;

	unsigned int color;
	unsigned int color_next;

	kmem_cache_t* next;
};

typedef struct {
	buddy_allocator* buddy;
	kmem_cache_t caches;
	kmem_cache_t* buffercaches[BUFFERS];

	kmem_cache_t* cache_chain; //double linked

	HANDLE myMutex;
} slab_allocator;

typedef unsigned short bufctl;

bufctl* bufctls(kmem_slab* slab);

extern slab_allocator* slab_alloc;

kmem_slab* add_slab(kmem_cache_t* cachep);

int slab_free(kmem_cache_t* cachep, kmem_slab** slab, void* objp, int full);

int slab_destroy(kmem_slab* slab, kmem_cache_t* cachep);