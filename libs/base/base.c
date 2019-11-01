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

static opi_t
default_next(void)
{
  opi_drop(opi_pop());
  return opi_undefined(opi_symbol("unimplemented_trait"));
}

static opi_t
id(void)
{
  return opi_pop();
}

static opi_t
flush_lazy(void)
{
  opi_t lazy = opi_pop();
  if (opi_unlikely(lazy->type != opi_lazy_type))
    return lazy;
  opi_t ret = opi_lazy_get_value(lazy);
  opi_inc_rc(ret);
  opi_drop(lazy);
  opi_dec_rc(ret);
  return ret;
}

static opi_t
string_length(void)
{
  opi_t str = opi_pop();
  opi_t ret = opi_number(opi_string_get_length(str));
  opi_drop(str);
  return ret;
}

static opi_t
array_length(void)
{
  opi_t arr = opi_pop();
  opi_t ret = opi_number(opi_array_get_length(arr));
  opi_drop(arr);
  return ret;
}

int
opium_library(struct opi_builder *bldr)
{
  opi_builder_def_const(bldr, "__blob_to_string",
      opi_fn("__blob_to_string", blob_to_string, 1));
  opi_builder_def_const(bldr, "__string_length",
      opi_fn("__string_length", string_length, 1));
  opi_builder_def_const(bldr, "__array_length",
      opi_fn("__array_length", array_length, 1));

  struct opi_trait *next_trait = opi_trait(opi_fn("next", default_next, 1));
  opi_trait_impl(next_trait, opi_null_type, opi_fn("nil_next", id, 1));
  opi_trait_impl(next_trait, opi_pair_type, opi_fn("pair_next", id, 1));
  opi_trait_impl(next_trait, opi_lazy_type, opi_fn("lazy_next", flush_lazy, 1));
  opi_t next_generic = opi_trait_into_generic(next_trait, "next");
  opi_builder_add_trait(bldr, "next", next_trait);
  opi_builder_def_const(bldr, "next", next_generic);
  return 0;
}
