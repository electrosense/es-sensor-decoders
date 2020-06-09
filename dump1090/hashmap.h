
#ifndef HASHMAP_HEADER
#define HASHMAP_HEADER

#include <stdint.h>

typedef struct hashmap_item *Hashmap_Item;

struct hashmap_item {
	Hashmap_Item next;
	uint32_t key;
	void *value;
};

struct hashmap {
	Hashmap_Item * buckets;
	size_t size;
	size_t count;
};

typedef struct hashmap *Hashmap;

Hashmap hashmap_create(size_t);
void * hashmap_get(Hashmap, uint32_t);
void hashmap_set(Hashmap, uint32_t, void *);
void hashmap_each(Hashmap, void fn(void *, uint32_t, int));
void hashmap_delete(Hashmap, uint32_t);
void hashmap_free(Hashmap);

#endif
