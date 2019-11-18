#include "opium/opium.h"
#include "opium/lambda.h"

static inline void
lam_destroy(struct opi_fn *fn)
{
  struct opi_lambda *lam = fn->data;
  for (size_t i = 0; i < lam->ncaps; ++i) {
    if (lam->caps[i]->rc > 0)
      opi_unref(lam->caps[i]);
  }
}

static inline void
lam_free(struct opi_fn *fn)
{
  struct opi_lambda *lam = fn->data;
  free(lam);
  opi_fn_delete(fn);
}

static inline void
lam_delete(struct opi_fn *fn)
{
  lam_destroy(fn);
  lam_free(fn);
}

extern inline void
opi_scope_dropout(struct opi_scope *scp)
{
  if (--scp->rc == 0) {
    size_t nnz = 0;
    for (size_t i = 0; i < scp->nlams; ++i)
      nnz += !!scp->lams[i]->header.rc;

    if (nnz == 0) {
      for (size_t i = 0; i < scp->nlams; ++i)
        lam_destroy(scp->lams[i]);
      for (size_t i = 0; i < scp->nlams; ++i)
        lam_free(scp->lams[i]);
      free(scp);
    } else {
      scp->rc = nnz;
    }
  }
}

void
opi_lambda_delete(struct opi_fn *fn)
{
  struct opi_lambda *lam = fn->data;
  if (lam->scp)
    opi_scope_dropout(lam->scp);
  else
    lam_delete(fn);
}

opi_t
opi_lambda_fn(void)
{
  struct opi_lambda *lam = opi_fn_get_data(opi_current_fn);
  return opi_vm(lam->bc);
}

