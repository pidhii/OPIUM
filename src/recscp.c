#include "opium/opium.h"

#define LOG2_POOL_SIZE 8
#define POOL_SIZE (1 << LOG2_POOL_SIZE)

typedef struct Pool_s Pool;
struct Pool_s {
  OpiRecScp arr[POOL_SIZE];
};

static
cod_vec(Pool*) g_pools;

static
size_t g_pool_cnt;

static
size_t g_cur_offs;

static
OpiRecScp *g_free_scp;

static inline Pool*
pool_new(void)
{
  Pool *pool = malloc(sizeof(Pool));
  for (size_t i = 0; i < POOL_SIZE; ++i)
    pool->arr[i].id = g_pool_cnt++;
  return pool;
}

void
opi_rec_scp_init(void)
{
  g_pool_cnt = 0;
  g_free_scp = NULL;
  g_cur_offs = 0;

  cod_vec_init(g_pools);
  cod_vec_push(g_pools, pool_new());
}

void
opi_rec_scp_cleanup(void)
{
  cod_vec_iter(g_pools, i, x, free(x));
}

OpiRecScp*
opi_rec_scp_alloc(size_t scpsize)
{
  OpiRecScp *ret;

  if (g_free_scp)
  {
    ret = g_free_scp;
    g_free_scp = g_free_scp->next;
  }
  else
  {
    if (opi_unlikely(g_cur_offs == POOL_SIZE))
    {
      Pool *newpool = pool_new();
      cod_vec_push(g_pools, newpool);
      ret = &newpool->arr[0];
      g_cur_offs = 1;
    }
    else
    {
      Pool *curpool = cod_vec_last(g_pools);
      ret = &curpool->arr[g_cur_offs++];
    }
  }

  ret->nodes = malloc(sizeof(OpiRecNode) * scpsize);
  ret->n = scpsize;
  return ret;
}

void
opi_rec_scp_free(OpiRecScp *scp)
{
  free(scp->nodes);
  scp->next = g_free_scp;
  g_free_scp = scp;
}

OpiRecScp*
opi_rec_scp_from_id(uint32_t id)
{
  uint32_t poolid = id & (POOL_SIZE - 1);
  uint32_t cellid = id >> LOG2_POOL_SIZE;
  return &g_pools.data[poolid]->arr[cellid];
}
