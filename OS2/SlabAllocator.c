#include "SlabAllocator.h"

slab_allocator* slab_alloc = nullptr;

void init_slab(kmem_slab* slab, kmem_cache_t* cachep) {

	bufctl i;

	for (i = 0; i < cachep->objperslab; ++i) {
		bufctls(slab)[i] = i + 1;

		if (cachep->ctor)
			cachep->ctor(slab->s_mem + i * cachep->size);

	}

	if (i > 0) bufctls(slab)[i - 1] = SLAB_END;

}

kmem_slab* add_slab(kmem_cache_t* cachep) {
	kmem_slab* slab;
	if (cachep->pagesforslabheader > 0) {
		slab = (kmem_slab*)buddy_alloc(slab_alloc->buddy, cachep->pagesforslabheader);
	}
	else {
		slab = (kmem_slab*)buddy_alloc(slab_alloc->buddy, cachep->pagesperslab);
	}

	if (!slab) {
		return 0;
	}

	WaitForSingleObject(cachep->myMutex, INFINITE);
	if (DEBUG_MUTEX) {
		printf("add_slab:: Holding %p\n", slab_alloc);
	}

	slab->first_obj = 0;
	slab->next_slab = cachep->empty;
	cachep->empty = slab;

	slab->obj_num = 0;

	if (cachep->pagesforslabheader > 0) {

		slab->s_mem = (kmem_slab*)buddy_alloc(slab_alloc->buddy, cachep->pagesperslab);

		if (slab->s_mem == 0) {
			buddy_free(slab_alloc->buddy, slab, cachep->pagesforslabheader);
			ReleaseMutex(cachep->myMutex);
			return 0;
		}
	}
	else {
		slab->s_mem =
			(char*)(slab + 1) //endofslabblock
			+ cachep->objperslab * sizeof(bufctl) //bufctl
			+ cachep->color_next * CACHE_L1_LINE_SIZE; //l1 offset
	}

	cachep->color_next = (cachep->color_next + 1) % cachep->color;

	init_slab(slab, cachep);

	ReleaseMutex(cachep->myMutex);
	return slab;
}

int slab_free(kmem_cache_t* cachep, kmem_slab** slab, void* objp, int full) {
	unsigned int size = cachep->size;

	while (*slab) {
		if (((char*)objp >= (char*)(*slab)->s_mem) &&
			((char*)objp < ((char*)(*slab)->s_mem + cachep->objperslab * size))) {

			bufctl i = (*slab)->first_obj;
			while (i != SLAB_END) {

				void* curr = ((*slab)->s_mem + i * size);

				if (curr == objp) {
					cachep->flags |= ERR_ALR_FREE;
					return 1;
				}

				i = bufctls(*slab)[i];
			}

			i = ((int)objp - (int)(*slab)->s_mem) / size;

			bufctls(*slab)[i] = (*slab)->first_obj;

			(*slab)->first_obj = i;
			(*slab)->obj_num--;

			kmem_slab* temp;

			if (full) {
				temp = *slab;
				*slab = temp->next_slab;
				temp->next_slab = cachep->half;
				cachep->half = temp;
				slab = &cachep->half;
			}

			if ((*slab)->obj_num == 0) {
				temp = *slab;
				*slab = temp->next_slab;
				temp->next_slab = cachep->empty;
				cachep->empty = temp;
			}

			return 1;
		}

		slab = &(*slab)->next_slab;
	}

	return 0;
}

int slab_destroy(kmem_slab* slab, kmem_cache_t* cachep) {

	int c = 0;

	while (slab) {

		c++;

		if (cachep->dtor) {
			for (bufctl i = 0; i < cachep->objperslab; ++i) {
				cachep->dtor(slab->s_mem + i * cachep->size);
			}
		}

		kmem_slab* temp = slab->next_slab;

		if (cachep->pagesforslabheader > 0) {
			buddy_free(slab_alloc->buddy, slab->s_mem, cachep->pagesperslab);
			buddy_free(slab_alloc->buddy, slab, cachep->pagesforslabheader);
		}
		else {
			buddy_free(slab_alloc->buddy, slab, cachep->pagesperslab);
		}

		slab = temp;
	}

	return c;
}

unsigned short* bufctls(kmem_slab* slab) {
	return (slab + 1);
}