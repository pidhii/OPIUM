#include "opium/opium.h"

union cell {
  struct opi_number number;
  struct opi_pair pair;
  struct opi_string string;
};

#define UALLOC_NAME cell
#define UALLOC_TYPE union cell
#include "uniform_allocator.h"

static
struct ualloc_cell g_allocator;

void
opi_allocators_init(void)
{ ualloc_cell_init(&g_allocator); }

void
opi_allocators_cleanup(void)
{ ualloc_cell_destroy(&g_allocator); }

static inline void*
allocate()
{ return ualloc_cell_alloc(&g_allocator); }

void*
opi_alloc_number(void)
{ return allocate(); }

void*
opi_alloc_pair(void)
{ return allocate(); }

void*
opi_alloc_string(void)
{ return allocate(); }

void*
opi_alloc_lazy(void)
{ return allocate(); }

static inline void
delete(void *ptr)
{ ualloc_cell_free(&g_allocator, ptr); }

void
opi_free_number(void* ptr)
{ delete(ptr); }

void
opi_free_pair(void* ptr)
{ delete(ptr); }

void
opi_free_string(void* ptr)
{ delete(ptr); }

void
opi_free_lazy(void* ptr)
{ delete(ptr); }
