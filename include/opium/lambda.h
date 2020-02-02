#ifndef OPIUM_LAMBDA_H
#define OPIUM_LAMBDA_H

#include "opium/opium.h"

typedef struct OpiRecRef_s {
  opi_t val;
  void (*destroy)(void*);
  void (*free)(void*);
} OpiRecRef;

typedef struct OpiRecScope_s {
  size_t rc;
  size_t nrefs;
  OpiRecRef refs[];
} OpiRecScope;

static inline OpiRecScope*
opi_rec_scope(size_t nrefs)
{
  OpiRecScope *scp = malloc(sizeof(OpiRecScope) + sizeof(OpiRecRef) * nrefs);
  scp->nrefs = nrefs;
  return scp;
}

static inline void
opi_rec_scope_finalize(OpiRecScope *scp)
{
  for (size_t i = 0; i < scp->nrefs; ++i)
    scp->refs[i].val->rc = 1;
  scp->rc = scp->nrefs;
}

static inline void
opi_rec_scope_set(OpiRecScope *scp, size_t iref, opi_t val,
    void (*destroy)(void*), void (*free)(void*))
{
  OpiRecRef *ref = scp->refs + iref;
  ref->val = val;
  ref->destroy = destroy;
  ref->free = free;
}

void
opi_scope_dropout(OpiRecScope *scp);

typedef struct {
  OpiBytecode *bc;
  OpiRecScope *scp;
  size_t ncaps;
  opi_t caps[];
} OpiLambda;

static inline OpiLambda*
opi_lambda_allocate(size_t ncaps)
{
  if (ncaps < 2)
    return opi_h2w();
  if (ncaps < 6)
    return opi_h6w();
  return malloc(sizeof(OpiLambda) + sizeof(opi_t) * ncaps);
}

void
opi_lam_free(OpiFn *fn);

void
opi_lam_destroy(OpiFn *fn);

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
