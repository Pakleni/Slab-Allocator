#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "test.h"

#define BLOCK_NUMBER (10)

#define shared_size (7)

#define nullptr 0

void construct2(void* data) {
	static int i = 0;
	*((int*)data) = i;
	++i;
}


#define size 10

int main2() {

	void* space = malloc(BLOCK_SIZE * BLOCK_NUMBER);

	kmem_init(space, BLOCK_NUMBER);

	kmem_cache_t* shared = kmem_cache_create("shared object1", sizeof(int), construct2, NULL);
	
	int* arr[size];


	for (int i = 0; i < size; i++) {

		arr[i] = (int*)kmem_cache_alloc(shared);
		
		if (arr[i] == nullptr) {
			free(space);
			printf("NULL");
			return 1;
		}
	}

	debug();

	kmem_cache_info(shared);

	for (int i = 0; i < size; i++) {
		printf("%d ", *arr[i]);
		kmem_cache_free(shared, arr[i]);
	}

	printf("\n\n");

	kmem_cache_t* shared2 = kmem_cache_create("shared object2", sizeof(int), construct2, NULL);


	for (int i = 0; i < size; i++) {
		arr[i] = (int*)kmem_cache_alloc(shared2);

		if (arr[i] == nullptr) {
			free(space);
			printf("NULL");
			return 1;
		}
	}

	kmem_cache_info(shared2);


	for (int i = 0; i < size; i++) {
		printf("%d ", *arr[i]);
		kmem_cache_free(shared2, arr[i]);
	}

	printf("\n\n");

	kmem_cache_info(shared2);

	debug();

	kmem_cache_error(shared);
	kmem_cache_error(shared2);

	kmem_cache_shrink(shared);
	kmem_cache_shrink(shared2);

	for (int i = 0; i < size; i++) {
		arr[i] = (int*)kmem_cache_alloc(shared2);

		if (arr[i] == nullptr) {
			free(space);
			printf("NULL");
			return 1;
		}
	}
	kmem_cache_error(shared2);


	debug();


	kmem_cache_destroy(shared2);
	kmem_cache_destroy(shared);

	debug();

	free(space);
	return 0;
}