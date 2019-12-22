#include "opium/opium.h"
#include <pthread.h>
#include <sched.h>

#define cas __sync_bool_compare_and_swap

typedef struct Buf_s {
  opi_t *ptr;
  size_t len;
  size_t cap;
} Buf;

static void
buf_init(Buf *buf)
{
  buf->cap = 0x100;
  buf->len = 0;
  buf->ptr = malloc(sizeof(opi_t) * buf->cap);
}

static void
buf_destroy(Buf *buf)
{ free(buf->ptr); }

static void
buf_push(Buf *buf, opi_t x)
{
  if (opi_unlikely(buf->len == buf->cap)) {
    buf->cap <<= 1;
    buf->ptr = realloc(buf->ptr, sizeof(opi_t) * buf->cap);
  }
  buf->ptr[buf->len++] = x;
}

static void
buf_clear(Buf *buf)
{ buf->len = 0; }

static Buf g_bufs[2];
static Buf g_ref_buffer;

static Buf *g_local_buffer;
static Buf *g_thread_buffer;
static Buf *g_old_buffer;

void
opi_gc_init(void)
{
  buf_init(g_bufs + 0);
  buf_init(g_bufs + 1);
  buf_init(&g_ref_buffer);

  g_local_buffer = g_bufs + 0;
  g_old_buffer = g_bufs + 1;
  g_thread_buffer = NULL;
}

/*
 * IF THREAD_BUFFER is NULL THEN
 *   1. READ OLD_BUFFER (later set as a new LOCAL_BUFFER)
 *   2. MOVE LOCAL_BUFFER -> THREAD_BUFFER
 */
void
opi_gc_push(opi_t x)
{
  buf_push(g_local_buffer, x);
  if (g_thread_buffer == NULL) {
      opi_debug("new thread-buffer is available\n");
    /* Read OLD_BUFFER before assigning THREAD_BUFFER, since in may result in
     * reassigning to OLD_BUFFER by the thread before we actully read it. */
    Buf *tmp = g_local_buffer;
    __sync_synchronize();
    /* Thread-buffer is absent, so we move LOCAL_BUFFER into THREAD_BUFFER. */
    g_thread_buffer = g_local_buffer;
    __sync_synchronize();
    /* Once THREAD_BUFFER is set to NULL, that buffer is actually moved to
     * OLD_BUFFER, so we fetch it to use as a new LOCAL_BUFFER. */
    g_local_buffer = tmp;
  }
}

void
opi_gc_flush_local_buffer()
{
  if (g_local_buffer->len > 0) {
    opi_debug("flush local buffer\n");
    while (g_thread_buffer != NULL);
    g_thread_buffer = g_local_buffer;
    g_local_buffer = g_old_buffer;
  }
}

/*
 * Fetch new thread-buffer.
 *
 * Note: previous thread-buffer must be moved into local-buffer before fetching
 * new thrad-buffer.
 */
static Buf*
thread_fetch_new_buffer()
{
  Buf *buf;
  g_thread_buffer = NULL;
  opi_debug("fetching new thread-buffer\n");
  while (TRUE) {
    if ((buf = g_thread_buffer)) {
      opi_debug("new thread-buffer is read\n");
      return buf;
    }
    sched_yield();
  }
}

/*
 * 1. For each container object, set gc_refs equal to the reference count.
 */
static void
thread_init_gc_refs(void)
{
  for (size_t i = 0; i < g_ref_buffer.len; ++i) {
    OpiRef *ref = OPI_REF(g_ref_buffer.ptr[i]);
    ref->gc_refs = ref->header.rc - 1; // one reference from gc itself.
    ref->gc_mark = 0;
  }
}

/*
 * 2. For each container object, find which container objects it references and
 *    decrement the referenced container's gc_refs field.
 */
static void
thread_scan_refs_aux(OpiRefVec *xs, int i0)
{
  int nxs = xs->vec.len - i0;
  if (nxs == 0)
    return;

  int len0 = xs->vec.len;
  for (int ix = i0; ix < len0; ++ix) {
    if (xs->vec.data[ix]->type == opi_ref_type) {
      OpiRef *ref = OPI_REF(xs->vec.data[ix]);

      // don't count same reference two times
      if (ref->gc_mark)
        continue;

      ref->gc_refs -= 1;
      ref->gc_mark = 1;
    }

    opi_get_refs(xs->vec.data[ix], xs);
  }

  i0 += nxs;
  thread_scan_refs_aux(xs, i0);
}

