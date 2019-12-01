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
/*#define UALLOC_POOL_SIZE 0x1000*/
#include "codeine/ualloc.h"
ALLOCATOR(2)

#define UALLOC_NAME h6w
#define UALLOC_TYPE OpiH6w
#define UALLOC_POOL_SIZE 0x40
#include "codeine/ualloc.h"
ALLOCATOR(6)

void
opi_allocators_init(void)
{
  cod_ualloc_h2w_init(&g_allocator_h2w);
  cod_ualloc_h6w_init(&g_allocator_h6w);
}

void
opi_allocators_cleanup(void)
{
  cod_ualloc_h2w_destroy(&g_allocator_h2w);
  cod_ualloc_h6w_destroy(&g_allocator_h6w);
}

