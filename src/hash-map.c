#include "opium/hash-map.h"

extern uint64_t
siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);

uint64_t
opi_hash(const char* p, size_t n)
{
  static uint8_t key[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
  return siphash((const uint8_t*)p, n, key);
}

void
opi_hash_map_init(OpiHashMap *map)
{
  map->cap = 0x100;
  map->size = 0;
  map->data = calloc(map->cap, sizeof(OpiHashMapElt));
}

void
opi_hash_map_destroy(OpiHashMap *map)
{
  opi_t key, val;
  size_t iter = opi_hash_map_begin(map);
  while (opi_hash_map_get(map, iter, &key, &val)) {
    opi_unref(key);
    opi_unref(val);
    iter = opi_hash_map_next(map, iter);
  }
  free(map->data);
}

static OpiHashMapElt*
find(OpiHashMapElt *data, size_t cap, opi_t key, size_t hash)
{
  for (size_t inc = 0, idx = hash & (cap - 1); TRUE; inc += 1, idx += inc) {
    OpiHashMapElt *elt = data + idx;
    if (elt->key == NULL) {
      // Found empty cell.
      return elt;

    } else if (elt->hash == hash && opi_equal(key, elt->key)) {
      // Found cell with matching key:
      return elt;
    }
  }
}

static OpiHashMapElt*
find_is(OpiHashMapElt *data, size_t cap, opi_t key, size_t hash)
{
  for (size_t inc = 0, idx = hash & (cap - 1); TRUE; inc += 1, idx += inc) {
    OpiHashMapElt *elt = data + idx;
    if (elt->key == NULL) {
      // Found empty cell.
      return elt;

    } else if (elt->hash == hash && opi_is(key, elt->key)) {
      // Found cell with matching key:
      return elt;
    }
  }
}

int
opi_hash_map_find(OpiHashMap *map, opi_t key, size_t hash, OpiHashMapElt **elt)
{
  OpiHashMapElt *myelt = find(map->data, map->cap, key, hash);
  if (elt)
    *elt = myelt;
  return myelt->key != NULL;
}

int
opi_hash_map_find_is(OpiHashMap *map, opi_t key, size_t hash, OpiHashMapElt **elt)
{
  OpiHashMapElt *myelt = find_is(map->data, map->cap, key, hash);
  if (elt)
    *elt = myelt;
  return myelt->key != NULL;
}

static void
rehash(OpiHashMap *map, size_t new_cap)
{
  OpiHashMapElt *old_data = map->data;
  size_t old_cap = map->cap;
  OpiHashMapElt *new_data = calloc(new_cap, sizeof(OpiHashMapElt));

  for (size_t i = 0; i < old_cap; ++i) {
    OpiHashMapElt *old_elt = old_data + i;
    if (old_elt->key != NULL) {
      OpiHashMapElt *new_elt = find(new_data, new_cap, old_elt->key, old_elt->hash);
      *new_elt = *old_elt;
    }
  }
  free(old_data);

  map->cap = new_cap;
  map->data = new_data;
}

void
opi_hash_map_insert(OpiHashMap *map, opi_t key, size_t hash, opi_t val, OpiHashMapElt *elt)
{
  if (elt->key == NULL) {
    // Must insert new element => check load factor and rehash if needed.
    map->size += 1;
    if ((map->size * 100) / map->cap > 70) {
      rehash(map, map->cap << 1);
      // repeat search for new data-array
      elt = find(map->data, map->cap, key, hash);
    }

  } else {
    // replace old value with new one
    opi_inc_rc(val);
    opi_unref(elt->val);
    elt->val = val;
    return;
  }

  // insert new element
  opi_inc_rc(elt->key = key);
  opi_inc_rc(elt->val = val);
  elt->hash = hash;
}

