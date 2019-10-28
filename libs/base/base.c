#include "opium/opium.h"

static opi_t
blob_to_string(void)
{
  opi_t blob = opi_pop();
  opi_t ret;

  const char *s = opi_blob_get_data(blob);
  size_t n = opi_blob_get_size(blob);
  for (const char *p = s; (size_t)(p - s) < n; ++p) {
    if (*p == 0) {
      if (blob->rc == 0) {
        char *str = realloc(opi_blob_drain(blob), p - s + 1);
        ret = opi_string_move2(str, p - s);
      } else {
        ret = opi_string2(s, p - s);
      }
      goto ret;
    }
  }

  if (blob->rc == 0)
    ret = opi_string_move2(opi_blob_drain(blob), n);
  else
    ret = opi_string2(s, n);

ret:
  opi_drop(blob);
  return ret;
}

int
opium_library(struct opi_builder *bldr)
{
  opi_builder_def_const(bldr, "__blob_to_string",
      opi_fn("__blob_to_string", blob_to_string, 1));

  return 0;
}
