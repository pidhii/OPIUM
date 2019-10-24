#ifndef OPIUM_LAMBDA_H
#define OPIUM_LAMBDA_H

#include "opium/opium.h"

struct opi_scope {
  size_t rc;
  size_t nlams;
  struct opi_fn *lams[];
};

void
opi_scope_dropout(struct opi_scope *scp);

struct opi_lambda {
  struct opi_bytecode *bc;
  struct opi_scope *scp;
  size_t ncaps;
  opi_t caps[];
};

void
opi_lambda_delete(struct opi_fn *fn);

opi_t
opi_lambda_fn(void);

static inline int
opi_is_lambda(opi_t cell)
{ return opi_fn_get_handle(cell) == opi_lambda_fn; }

#endif
