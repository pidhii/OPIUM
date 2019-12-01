#ifndef OPIUM_HASH_MAP_H
#define OPIUM_HASH_MAP_H

#include "opium/opium.h"

uint64_t
opi_hash(const char* p, size_t n);

typedef struct OpiHashMapElt_s {
  size_t hash;
  opi_t key;
  opi_t val;
} OpiHashMapElt;

typedef struct OpiHashMap_s {
  size_t size;
  size_t cap;
  OpiHashMapElt *data;
} OpiHashMap;

void
opi_hash_map_init(OpiHashMap *map);

void
opi_hash_map_destroy(OpiHashMap *map);

int
opi_hash_map_find(OpiHashMap *map, opi_t key, size_t hash, OpiHashMapElt* elt);

int
opi_hash_map_find_is(OpiHashMap *map, opi_t key, size_t hash, OpiHashMapElt* elt);

void
opi_hash_map_insert(OpiHashMap *map, opi_t key, size_t hash, opi_t val, OpiHashMapElt *elt);

static inline size_t
opi_hash_map_next(OpiHashMap *map, size_t iter)
{
  for (size_t i = iter + 1; i < map->cap; ++i) {
    if (map->data[i].key != NULL)
      return i;
  }
  return map->cap;
}

static inline size_t
opi_hash_map_begin(OpiHashMap *map)
{
  for (size_t i = 0; i < map->cap; ++i) {
    if (map->data[i].key != NULL)
      return i;
  }
  return map->cap;
}

static inline int
opi_hash_map_get(OpiHashMap *map, size_t iter, opi_t *key, opi_t *val)
{
  OpiHashMapElt *elt = map->data + iter;
  if (iter >= map->cap)
    return FALSE;
  if (key)
    *key = elt->key;
  if (val)
    *val = elt->val;
  return TRUE;
}

#endif
