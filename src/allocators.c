#include "opium/opium.h"

#if defined(OPI_DEBUG_MODE)
#warning Will use malloc for all allocations.
# define ALLOCATOR(n)                                        \
  static                                                     \
  struct cod_ualloc_h##n##w g_allocator_h##n##w;             \
                                                             \
  void*                                                      \
  opi_h##n##w()                                              \
  { return malloc(sizeof(OpiH##n##w)); }                     \
                                                             \
  void                                                       \
  opi_h##n##w_free(void *ptr)                                \
  { free(ptr); }
#else
# define ALLOCATOR(n)                                        \
  static                                                     \
  struct cod_ualloc_h##n##w g_allocator_h##n##w;             \
                                                             \
  void*                                                      \
  opi_h##n##w()                                              \
  { return cod_ualloc_h##n##w_alloc(&g_allocator_h##n##w); } \
                                                             \
  void                                                       \
  opi_h##n##w_free(void *ptr)                                \
  { cod_ualloc_h##n##w_free(&g_allocator_h##n##w, ptr); }
#endif

#define UALLOC_NAME h2w
#define UALLOC_TYPE OpiH2w
#include "codeine/ualloc.h"
ALLOCATOR(2)

#define UALLOC_NAME h3w
#define UALLOC_TYPE OpiH3w
#include "codeine/ualloc.h"
ALLOCATOR(3)

typedef struct Pool_s {
  uintptr_t size;
  opi_t data[];
} Pool;

static inline Pool*
get_pool(opi_t *ptr)
{
  return (Pool*)(ptr - 1);
}

static inline opi_t*
allocate_pool(size_t size)
{
  Pool *pool = malloc(sizeof(Pool) + sizeof(opi_t) * size);
  pool->size = size;
  return pool->data;
}

static inline Pool*
realloc_pool(Pool *pool, size_t new_size)
{
  pool = realloc(pool, sizeof(Pool) + sizeof(opi_t) * new_size);
  pool->size = new_size;
  return pool;
}

static cod_vec(Pool*)
g_pools;

opi_t*
opi_request_pool(size_t size)
{
  if (g_pools.len == 0) {
    return allocate_pool(size);
  } else {
    Pool *pool = cod_vec_pop(g_pools);
    if (pool->size < size)
      pool = realloc_pool(pool, size);
    return pool->data;
  }
}

size_t
opi_get_pool_size(opi_t *ptr)
{
  return get_pool(ptr)->size;
}

void
opi_release_pool(opi_t *ptr)
{
  cod_vec_push(g_pools, get_pool(ptr));
}


void
opi_allocators_init(void)
{
  cod_ualloc_h2w_init(&g_allocator_h2w);
  cod_ualloc_h3w_init(&g_allocator_h3w);
  cod_vec_init(g_pools);
}

void
opi_allocators_cleanup(void)
{
  cod_ualloc_h2w_destroy(&g_allocator_h2w);
  cod_ualloc_h3w_destroy(&g_allocator_h3w);
  for (size_t i = 0; i < g_pools.len; ++i)
    free(g_pools.data[i]);
  cod_vec_destroy(g_pools);
}

