#ifndef OPIUM_LAMBDA_H
#define OPIUM_LAMBDA_H

#include "opium/opium.h"

struct opi_scope {
  size_t rc;
  size_t nlams;
  OpiFn *lams[];
};

void
opi_scope_dropout(struct opi_scope *scp);

struct opi_lambda {
  OpiBytecode *bc;
  struct opi_scope *scp;
  size_t ncaps;
  opi_t caps[];
};

static inline struct opi_lambda *
opi_lambda_allocate(size_t ncaps)
{
  return malloc(sizeof(struct opi_lambda) + sizeof(opi_t) * ncaps);
}

void
opi_lambda_delete(OpiFn *fn);

opi_t
opi_lambda_fn(void);

static inline int
opi_is_lambda(opi_t cell)
{
  return opi_fn_get_handle(cell) == opi_lambda_fn;
}

#endif
