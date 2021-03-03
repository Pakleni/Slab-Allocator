#include "SlabAllocator.h"
#include <Math.h>
#include <windows.h>

#define CACHE_MIN_OBJ 8
#define BUFFER_CACHE_MIN_OBJ 1


void debug() {
	display(slab_alloc->buddy);
}

void find_some_memory() {
	
	WaitForSingleObject(slab_alloc->myMutex, INFINITE);
	if (DEBUG_MUTEX) {
		printf("find:: Holding %p\n", slab_alloc);
	}

	kmem_cache_t* curr = slab_alloc->cache_chain;

	if (DEBUG) {
		printf("We' arr findin' some memory\n");
	}

	while (curr) {
		/*if (DEBUG) {
			printf("Shrinking %p\n", curr);
		}*/
		kmem_cache_shrink(curr);
		/*if (DEBUG) {
			printf("Shrunk %p\n", curr);
		}*/

		curr = curr->next;
	}

	if (DEBUG) {
		printf("we' arr done\n");
	}

	ReleaseMutex(slab_alloc->myMutex);
}

void kmem_init(void* space, int block_num) {
	
	unsigned int required_mem = sizeof(slab_allocator) + size_of(block_num - 1);
	unsigned int initial_sys_mem = ceil(((double)required_mem + sizeof(kmem_slab)) / BLOCK_SIZE);
	unsigned int n = block_num - initial_sys_mem;

	//slab allocator
	slab_alloc = (slab_allocator*)((char*)space + n * BLOCK_SIZE);
	slab_alloc->myMutex = CreateMutex(NULL, FALSE, NULL);


	//buddy allocator
	buddy_allocator* buddy = (buddy_allocator*)(slab_alloc + 1);
	factory((void**)space, n, buddy);
	slab_alloc->buddy = buddy;

	buddy->myMutex = CreateMutex(NULL, FALSE, NULL);

	//cache of caches
	kmem_cache_t* caches = &slab_alloc->caches;

	caches->name = nullptr;
	caches->size = sizeof(kmem_cache_t);

	caches->ctor = nullptr;
	caches->dtor = nullptr;

	kmem_slab* slab = (kmem_slab*)((char*)space + n * BLOCK_SIZE + required_mem);

	caches->empty = slab;
	caches->full = nullptr;
	caches->half = nullptr;

	caches->pagesforslabheader = 0;

	double div = ((double)CACHE_MIN_OBJ * (sizeof(kmem_cache_t) + sizeof(bufctl))) / BLOCK_SIZE;

	if (div > 1) {
		caches->pagesperslab = pow(2, ceil(log2(div)));
	}
	else {
		caches->pagesperslab = 1;
	}

	caches->flags = NO_SHRINK;

	caches->myMutex = CreateMutex(NULL, FALSE, NULL);

	unsigned int mem_size = caches->pagesperslab * BLOCK_SIZE - sizeof(kmem_slab);
	unsigned int obj_buf_size = sizeof(kmem_cache_t) + sizeof(bufctl);

	caches->objperslab = mem_size / obj_buf_size;

	caches->color = (mem_size - caches->objperslab * obj_buf_size) / CACHE_L1_LINE_SIZE;
	if (!caches->color) caches->color = 1;


	caches->color_next = 0;

	slab_alloc->cache_chain = caches;
	caches->next = nullptr;

	// cache of caches first slab
	slab->first_obj = 0;
	slab->next_slab = nullptr;

	slab->obj_num = 0;

	unsigned int first_slab_size = initial_sys_mem * BLOCK_SIZE - required_mem - sizeof(kmem_slab);
	unsigned int first_slab_objects = first_slab_size / obj_buf_size;


	slab->s_mem =
		(char*)(slab + 1) //endofslabblock
		+ first_slab_objects * sizeof(bufctl);

	bufctl i;

	for (i = 0; i < first_slab_objects; ++i) {
		bufctls(slab)[i] = i + 1;
	}

	if (i > 0) bufctls(slab)[i - 1] = SLAB_END;

	//small memory buffers
	for (int i = 0; i < BUFFERS; i++) {
		slab_alloc->buffercaches[i] = nullptr;
	}
}

kmem_cache_t* create_cache(const char* name, unsigned int size,
	void (*ctor)(void*),
	void (*dtor)(void*), int min) {

	kmem_cache_t* cache = (kmem_cache_t*)kmem_cache_alloc(&slab_alloc->caches);

	//no memory
	if (cache == nullptr) {
		return 0;
	}

	cache->name = name;
	cache->size = size;

	cache->ctor = ctor;
	cache->dtor = dtor;

	cache->empty = nullptr;
	cache->full = nullptr;
	cache->half = nullptr;
	
	double div = (sizeof(kmem_slab) + (double)min * (size + sizeof(bufctl))) / BLOCK_SIZE;

	if (div > 1) {

		unsigned int pow1 = ceil(pow(2, ceil(log2(div))));
		/*cache->pagesforslabheader = 0;
		cache->pagesperslab = pow1;*/

		double div2 = ((double)min * (size)) / BLOCK_SIZE;

		unsigned int pow3 = ceil(pow(2, ceil(log2(div2))));

		unsigned int objs = (pow3) * BLOCK_SIZE / (size);
		double div1 = (sizeof(kmem_slab) + (double)objs * (sizeof(bufctl))) / BLOCK_SIZE;
		
		unsigned int pow2 = ceil(pow(2, ceil(log2(div1))));
		

		if (pow1 > (pow2 + pow3)) {
			cache->pagesforslabheader = pow2;
			cache->pagesperslab = pow3;
		}
		else {
			cache->pagesforslabheader = 0;
			cache->pagesperslab = pow1;
		}

	}
	else {
		cache->pagesforslabheader = 0;
		cache->pagesperslab = 1;
	}

	cache->flags = 0;

	cache->myMutex = CreateMutex(NULL, FALSE, NULL);

	unsigned int mem_size = cache->pagesperslab * BLOCK_SIZE - (cache->pagesforslabheader == 0) * sizeof(kmem_slab);
	unsigned int obj_buf_size = size + (cache->pagesforslabheader == 0) * sizeof(bufctl);

	cache->objperslab = mem_size / obj_buf_size;

	cache->color = (mem_size - cache->objperslab * obj_buf_size) / CACHE_L1_LINE_SIZE;
	if (!cache->color) cache->color = 1;

	cache->color_next = 0;

	WaitForSingleObject(slab_alloc->myMutex, INFINITE);
	if (DEBUG_MUTEX) {
		printf("create:: Holding %p\n", slab_alloc);
	}

	cache->next = slab_alloc->cache_chain;
	
	slab_alloc->cache_chain = cache;

	ReleaseMutex(slab_alloc->myMutex);

	return cache;
}

kmem_cache_t* kmem_cache_create(const char* name, unsigned int size,
	void (*ctor)(void*),
	void (*dtor)(void*)) {

	kmem_cache_t* cache = create_cache(name, size, ctor, dtor, CACHE_MIN_OBJ);
	
	if (cache)
		add_slab(cache);


	return cache;
}

void* kmem_cache_alloc(kmem_cache_t* cachep) {
	
	if (cachep == nullptr) return nullptr;

	WaitForSingleObject(cachep->myMutex, INFINITE);
	if (DEBUG_MUTEX) {
		printf("alloc:: Holding %p\n", cachep);
	}

	unsigned int size = cachep->size;
	
	kmem_slab* slab = nullptr;

	if (cachep->half) slab = cachep->half;
	else if (cachep->empty) slab = cachep->empty;

	if (slab) {

		void* ret = slab->s_mem + slab->first_obj * cachep->size;

		slab->first_obj = bufctls(slab)[slab->first_obj];

		slab->obj_num++;

		if (slab->obj_num == 1) {
			cachep->empty = slab->next_slab;
			slab->next_slab = cachep->half;
			cachep->half = slab;
		}

		if (slab->first_obj == SLAB_END) {
			cachep->half = slab->next_slab;
			slab->next_slab = cachep->full;
			cachep->full = slab;
		}

		ReleaseMutex(cachep->myMutex);
		return ret;
	}

	cachep->flags |= EXPANDING;

	ReleaseMutex(cachep->myMutex);

	if (add_slab(cachep) == 0) {
		find_some_memory();
		if (add_slab(cachep) == 0) {
			find_some_memory();
			if (add_slab(cachep) == 0) {
				if (DEBUG) {
					cachep->flags |= ERR_NO_MEM;
					printf("kmem_alloc: No memory for slab\n");
				}
				return 0;
			}
		}
	}

	return kmem_cache_alloc(cachep);
}

void kmem_cache_free(kmem_cache_t* cachep, void* objp) {

	if (objp == 0 || cachep == 0) return;
	
	WaitForSingleObject(cachep->myMutex, INFINITE);
	if (DEBUG_MUTEX) {
		printf("free:: Holding %p\n", slab_alloc);
	}

	if (cachep->full && slab_free(cachep, &cachep->full, objp, 1)) {
		ReleaseMutex(cachep->myMutex);
		return;
	}

	if (cachep->half && slab_free(cachep, &cachep->half, objp, 0)) {
		ReleaseMutex(cachep->myMutex);
		return;
	}

	cachep->flags |= ERR_NOT_MINE;

	if (DEBUG) {
		printf("kmem_free: Not in my blocks\n");
	}

	ReleaseMutex(cachep->myMutex);
}

void kmem_cache_destroy(kmem_cache_t* cachep) {

	if (cachep == 0) return;

	WaitForSingleObject(slab_alloc->myMutex, INFINITE);
	if (DEBUG_MUTEX) {
		printf("destroy:: Holding %p\n", slab_alloc);
	}

	if (cachep == slab_alloc->cache_chain) {
		slab_alloc->cache_chain = cachep->next;
	}
	else {
		kmem_cache_t * curr = slab_alloc->cache_chain;

		while (curr->next != cachep) curr = curr->next;
		
		curr->next = cachep->next;
	}

	ReleaseMutex(slab_alloc->myMutex);
	WaitForSingleObject(cachep->myMutex, INFINITE);
	if (DEBUG_MUTEX) {
		printf("destroy:: Holding %p\n", cachep);
	}


	slab_destroy(cachep->full, cachep);
	cachep->full = nullptr;
	slab_destroy(cachep->half, cachep);
	cachep->half = nullptr;
	slab_destroy(cachep->empty, cachep);
	cachep->empty = nullptr;

	cachep->flags = DEAD;
	HANDLE myMutex = cachep->myMutex;
	kmem_cache_free(&slab_alloc->caches, cachep);


	ReleaseMutex(myMutex);
}

int kmem_cache_shrink(kmem_cache_t* cachep) {
	if (cachep == nullptr) return 0;

	if (cachep->flags & NO_SHRINK) return 0;

	int c = 0;

	WaitForSingleObject(cachep->myMutex, INFINITE);
	if (DEBUG_MUTEX) {
		printf("shrink:: Holding %p\n", cachep);
	}

	if (cachep->flags == DEAD) {
		if (DEBUG) {
			printf("DEAD FLAG\n");
		}
		ReleaseMutex(cachep->myMutex);
		return;
	}

	if ((cachep->flags & EXPANDING) == 0 || (cachep->flags & TRIED_SHRINK) == 0) {
		c = slab_destroy(cachep->empty, cachep);

		cachep->empty = nullptr;
	}

	cachep->flags |= TRIED_SHRINK;
	cachep->flags &= ~EXPANDING;

	ReleaseMutex(cachep->myMutex);

	return c;
}

void kmem_cache_info(kmem_cache_t* cachep) {
	if (cachep == 0) return;
	WaitForSingleObject(cachep->myMutex, INFINITE);
	if (DEBUG_MUTEX) {
		printf("info:: Holding %p\n", cachep);
	}


	int c = 0;
	unsigned int objects = 0;

	kmem_slab* curr = cachep->empty;
	while (curr) {
		c++;
		curr = curr->next_slab;
	}
	curr = cachep->half;
	while (curr) {
		c++;
		objects += curr->obj_num;
		curr = curr->next_slab;
	}
	curr = cachep->full;
	while (curr) {
		c++;
		objects += curr->obj_num;
		curr = curr->next_slab;
	}

	float filled = (float)objects / (cachep->objperslab * c) * 100;

	printf(
		"Name:			%s\n"
		"Object size:		%dB\n"
		"Size(in blocks):	%d\n"
		"Number of slabs:	%d\n"
		"Objects:		%d\n"
		"Max Objects per Slab:	%d\n"
		"Filled:			%.2f%c\n\n",
		cachep->name,
		cachep->size,
		c * cachep->pagesperslab,
		c,
		objects,
		cachep->objperslab,
		filled, '%');

	ReleaseMutex(cachep->myMutex);

}

int kmem_cache_error(kmem_cache_t* cachep) {
	if (cachep == nullptr) return nullptr;

	if (cachep->flags == 0) return 0;

	unsigned char temp = cachep->flags;

	cachep->flags &= ~(ERR_NO_MEM + ERR_NOT_MINE + ERR_ALR_FREE);

	if (temp & ERR_NO_MEM) {
		printf("Tried to allocate more memory, didn't have the memory\n");
	}
	else if (temp & ERR_NOT_MINE) {
		printf("Tried to free an object that doesn't belong to this cache\n");
	}
	else if (temp & ERR_ALR_FREE) {
		printf("Tried to free a pointer to memory that was already free\n");
	}

	printf("\n");

	return temp;
}

void* kmalloc(unsigned int size) {
	if (size < pow(2, 5) || size > pow(2, 17)) {
		if (DEBUG) {
			printf("FATAL kmalloc: tried to allocate out of bounds\n");
		}
		return 0;
	}

	int to = ceil(log2(size));
	int i = to - 5;
	
	if (slab_alloc->buffercaches[i] == nullptr) {
		slab_alloc->buffercaches[i] = create_cache("buffer cache", pow(2,to), nullptr, nullptr, BUFFER_CACHE_MIN_OBJ);
	}

	return kmem_cache_alloc(slab_alloc->buffercaches[i]);
}

void kfree(const void* objp) {
	for (int i = 0; i < BUFFERS; i++) {
		if (slab_alloc->buffercaches[i]) {
			slab_alloc->buffercaches[i]->flags = 0;
			kmem_cache_free(slab_alloc->buffercaches[i], objp);

			if ((slab_alloc->buffercaches[i]->flags & ERR_NOT_MINE) == 0) {
				kmem_cache_shrink(slab_alloc->buffercaches[i]);
				break;
			}
		}
	}
}