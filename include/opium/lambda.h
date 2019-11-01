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

static inline struct opi_lambda *
opi_lambda_allocate(size_t ncaps)
{
  /*switch (ncaps) {*/
    /*case 0:*/
    /*case 1:*/
      /*return opi_allocate_h2w();*/
    /*case 2:*/
      /*return opi_allocate_h3w();*/
    /*case 3:*/
      /*return opi_allocate_h4w();*/
    /*default:*/
      /*return malloc(sizeof(struct opi_lambda) + sizeof(opi_t) * ncaps);*/
  /*}*/
  if (opi_likely(ncaps & 18446744073709551612UL))
    return malloc(sizeof(struct opi_lambda) + sizeof(opi_t) * ncaps);
  else
    return opi_allocate_h4w();
}

void
opi_lambda_delete(struct opi_fn *fn);

opi_t
opi_lambda_fn(void);

static inline int
opi_is_lambda(opi_t cell)
{ return opi_fn_get_handle(cell) == opi_lambda_fn; }

#endif
