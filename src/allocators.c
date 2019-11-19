#include "opium/opium.h"

struct cell {
  OpiHeader header;
  uintptr_t w[2];
};

#define UALLOC_NAME cell
#define UALLOC_TYPE struct cell
#include "codeine/ualloc.h"

static
struct cod_ualloc_cell g_allocator;

void*
opi_allocate()
{
  return cod_ualloc_cell_alloc(&g_allocator);
}

void
opi_free(void *ptr)
{
  cod_ualloc_cell_free(&g_allocator, ptr);
}

void
opi_allocators_init(void)
{
  cod_ualloc_cell_init(&g_allocator);
}

void
opi_allocators_cleanup(void)
{
  cod_ualloc_cell_destroy(&g_allocator);
}

