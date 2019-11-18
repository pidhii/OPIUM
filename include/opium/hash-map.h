#ifndef OPIUM_HASH_MAP_H
#define OPIUM_HASH_MAP_H

#include "opium/opium.h"

uint64_t
opi_hash(const char* p, size_t n);

struct opi_hash_map_elt {
  size_t hash;
  opi_t key;
  opi_t val;
};

struct opi_hash_map {
  // must use 32-bit integers to fit in cell (128 bits for data)
  uint32_t size;
  uint32_t cap;
  struct opi_hash_map_elt *data;
};

void
opi_hash_map_init(struct opi_hash_map *map);

void
opi_hash_map_destroy(struct opi_hash_map *map);

int
opi_hash_map_find(struct opi_hash_map *map, opi_t key, size_t hash, struct opi_hash_map_elt* elt);

int
opi_hash_map_find_is(struct opi_hash_map *map, opi_t key, size_t hash, struct opi_hash_map_elt* elt);

void
opi_hash_map_insert(struct opi_hash_map *map, opi_t key, size_t hash, opi_t val, struct opi_hash_map_elt *elt);

static inline size_t
opi_hash_map_next(struct opi_hash_map *map, size_t iter)
{
  for (size_t i = iter + 1; i < map->cap; ++i) {
    if (map->data[i].key != NULL)
      return i;
  }
  return map->cap;
}

static inline size_t
opi_hash_map_begin(struct opi_hash_map *map)
{
  for (size_t i = 0; i < map->cap; ++i) {
    if (map->data[i].key != NULL)
      return i;
  }
  return map->cap;
}

static inline int
opi_hash_map_get(struct opi_hash_map *map, size_t iter, opi_t *key, opi_t *val)
{
  struct opi_hash_map_elt *elt = map->data + iter;
  if (iter >= map->cap)
    return FALSE;
  if (key)
    *key = elt->key;
  if (val)
    *val = elt->val;
  return TRUE;
}

#undef CAP

#endif
