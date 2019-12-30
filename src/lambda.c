#include "opium/opium.h"
#include "opium/lambda.h"

extern inline void
opi_lam_destroy(OpiFn *fn)
{
  OpiLambda *lam = fn->data;
  for (size_t i = 0; i < lam->ncaps; ++i) {
    if (lam->caps[i]->rc > 0)
      opi_unref(lam->caps[i]);
  }
}

extern inline void
opi_lam_free(OpiFn *fn)
{
  OpiLambda *lam = fn->data;
  if (lam->ncaps < 2)
    opi_h2w_free(lam);
  else if (lam->ncaps < 6)
    opi_h6w_free(lam);
  else
    free(lam);

  opi_fn_delete(fn);
}

extern inline void
opi_lam_delete(OpiFn *fn)
{
  opi_lam_destroy(fn);
  opi_lam_free(fn);
}

extern inline void
opi_scope_dropout(OpiRecScope *scp)
{
  if (--scp->rc == 0) {
    size_t nnz = 0;
    for (size_t i = 0; i < scp->nrefs; ++i)
      nnz += !!scp->refs[i].val->rc;

    if (nnz == 0) {
      for (size_t i = 0; i < scp->nrefs; ++i)
        scp->refs[i].destroy(scp->refs[i].val);
      for (size_t i = 0; i < scp->nrefs; ++i)
        scp->refs[i].free(scp->refs[i].val);
      free(scp);
    } else {
      scp->rc = nnz;
    }
  }
}

void
opi_lambda_delete(OpiFn *fn)
{
  OpiLambda *lam = fn->data;
  if (lam->scp)
    opi_scope_dropout(lam->scp);
  else
    opi_lam_delete(fn);
}

opi_t
opi_lambda_fn(void)
{
  OpiLambda *lam = opi_current_fn->data;
  return opi_vm(lam->bc);
}

