#include "opium/opium.h"

#define CELL(n)               \
  struct h##n##w {            \
    struct opi_header header; \
    uintptr_t w[n];           \
  };

#define ALLOCATOR(n)                                     \
  static                                                 \
  struct ualloc_h##n##w g_allocator_h##n##w;             \
                                                         \
  void*                                                  \
  opi_allocate_h##n##w()                                 \
  { return ualloc_h##n##w_alloc(&g_allocator_h##n##w); } \
                                                         \
  void                                                   \
  opi_free_h##n##w(void *ptr)                            \
  { ualloc_h##n##w_free(&g_allocator_h##n##w, ptr); }

CELL(2)
#define UALLOC_NAME h2w
#define UALLOC_TYPE struct h2w
#include "uniform_allocator.h"
ALLOCATOR(2)

CELL(3)
#define UALLOC_NAME h3w
#define UALLOC_TYPE struct h3w
#include "uniform_allocator.h"
ALLOCATOR(3)

CELL(4)
#define UALLOC_NAME h4w
#define UALLOC_TYPE struct h4w
#include "uniform_allocator.h"
ALLOCATOR(4)

void
opi_allocators_init(void)
{
  ualloc_h2w_init(&g_allocator_h2w);
  ualloc_h3w_init(&g_allocator_h3w);
  ualloc_h4w_init(&g_allocator_h4w);
}

void
opi_allocators_cleanup(void)
{
  ualloc_h2w_destroy(&g_allocator_h2w);
  ualloc_h3w_destroy(&g_allocator_h3w);
  ualloc_h4w_destroy(&g_allocator_h4w);
}

