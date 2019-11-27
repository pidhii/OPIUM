#include "opium/opium.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

static opi_t
length(void)
{
  opi_t x = opi_pop();
  opi_t ret = opi_num_new(opi_length(x));
  opi_drop(x);
  return ret;
}

static opi_t
strlen_(void)
{
  opi_t str = opi_pop();
  if (opi_unlikely(str->type != opi_string_type)) {
    opi_drop(str);
    return opi_undefined(opi_symbol("type-error"));
  }
  opi_t ret = opi_num_new(opi_string_get_length(str));
  opi_drop(str);
  return ret;
}

static opi_t
substr(void)
{
  if (opi_unlikely(opi_nargs > 3)) {
    while (opi_nargs--)
      opi_drop(opi_pop());
    return opi_undefined(opi_symbol("arity-error"));
  }

  opi_t str = opi_pop();
  opi_t start = opi_pop();
  opi_t end = opi_nargs == 3 ? opi_pop() : NULL;

  if (opi_unlikely(str->type != opi_string_type)) {
    opi_drop(str);
    opi_drop(start);
    if (end)
      opi_drop(end);
    return opi_undefined(opi_symbol("type-error"));
  }

  if (opi_unlikely(start->type != opi_num_type)) {
    opi_drop(str);
    opi_drop(start);
    if (end)
      opi_drop(end);
    return opi_undefined(opi_symbol("type-error"));
  }

  if (end && opi_unlikely(end->type != opi_num_type)) {
    opi_drop(str);
    opi_drop(start);
    opi_drop(end);
    return opi_undefined(opi_symbol("type-error"));
  }

  const char *s = opi_string_get_value(str);
  ssize_t len = opi_string_get_length(str);
  ssize_t from = opi_num_get_value(start);
  ssize_t to = end ? opi_num_get_value(end) : len;

  if (from < 0)
    from = len + from;
  if (to < 0)
    to = len + to;

  if (opi_unlikely(from >= len || from > to)) {
    opi_drop(str);
    opi_drop(start);
    if (end)
      opi_drop(end);
    return opi_undefined(opi_symbol("out-of-range"));
  }

  opi_t ret = opi_string_new_with_len(s + from, to - from);
  opi_drop(str);
  opi_drop(start);
  if (end)
    opi_drop(end);
  return ret;
}

static opi_t
chop(void)
{
  opi_t str = opi_pop();
  if (opi_unlikely(str->type != opi_string_type)) {
    opi_drop(str);
    return opi_undefined(opi_symbol("type-error"));
  }
  const char *s = opi_string_get_value(str);
  size_t len = opi_string_get_length(str);
  if (isspace(s[len - 1])) {
    if (str->rc == 0) {
      *(char*)(s + len - 1) = 0;
      opi_as(str, OpiString).len -= 1;
      return str;
    } else {
      return opi_string_new_with_len(s, len - 1);
    }
  } else {
    return str;
  }
}

static opi_t
chomp(void)
{
  opi_t str = opi_pop();
  if (opi_unlikely(str->type != opi_string_type)) {
    opi_drop(str);
    return opi_undefined(opi_symbol("type-error"));
  }
  const char *s = opi_string_get_value(str);
  size_t len = opi_string_get_length(str);

  size_t newlen = len;
  while (newlen > 0 && isspace(s[newlen - 1]))
    --newlen;

  if (newlen != len) {
    if (str->rc == 0) {
      *(char*)(s + newlen) = 0;
      opi_as(str, OpiString).len = newlen;
      return str;
    } else {
      return opi_string_new_with_len(s, newlen);
    }
  } else {
    return str;
  }
}

static opi_t
ltrim(void)
{
  opi_t str = opi_pop();
  if (opi_unlikely(str->type != opi_string_type)) {
    opi_drop(str);
    return opi_undefined(opi_symbol("type-error"));
  }
  const char *s = opi_string_get_value(str);
  size_t len = opi_string_get_length(str);

  const char *p = s;
  while (isspace(*p))
    p += 1;

  if (p == s) {
    return str;
  } else {
    opi_t ret = opi_string_new_with_len(p, len - (p - s));
    opi_drop(str);
    return ret;
  }
}

static opi_t
strstr_(void)
{
  opi_t str = opi_pop();
  opi_t chr = opi_pop();
  if (opi_unlikely(str->type != opi_string_type
                || chr->type != opi_string_type))
  {
    opi_drop(str);
    opi_drop(chr);
    return opi_undefined(opi_symbol("type-error"));
  }

  char *at = strstr(opi_string_get_value(str), opi_string_get_value(chr));
  opi_drop(chr);

  if (!at) {
    opi_drop(str);
    return opi_false;
  } else {
    opi_t ret = opi_num_new(at - opi_string_get_value(str));
    opi_drop(str);
    return ret;
  }
}

static opi_t
revappend(void)
{
  opi_t l = opi_pop();
  opi_t acc = opi_pop();
  for (opi_t it = l; it->type == opi_pair_type; it = opi_cdr(it))
    acc = opi_cons(opi_car(it), acc);
  opi_drop(l);
  return acc;
}

static opi_t
open_(void)
{
  opi_t path = opi_pop();
  opi_t mode = opi_pop();

  if (opi_unlikely(path->type != opi_string_type
                || mode->type != opi_string_type))
  {
    opi_drop(path);
    opi_drop(mode);
    return opi_undefined(opi_symbol("type-error"));
  }

  FILE *fs = fopen(opi_string_get_value(path), opi_string_get_value(mode));
  opi_drop(path);
  opi_drop(mode);

  if (!fs)
    return opi_undefined(opi_string_new(strerror(errno)));
  else
    return opi_file(fs, fclose);
}

static opi_t
popen_(void)
{
  opi_t cmd = opi_pop();
  opi_t mode = opi_pop();

  if (opi_unlikely(cmd->type != opi_string_type
                || mode->type != opi_string_type))
  {
    opi_drop(cmd);
    opi_drop(mode);
    return opi_undefined(opi_symbol("type-error"));
  }

  FILE *fs = popen(opi_string_get_value(cmd), opi_string_get_value(mode));
  opi_drop(cmd);
  opi_drop(mode);

  if (!fs)
    return opi_undefined(opi_string_new(strerror(errno)));
  else
    return opi_file(fs, pclose);
}

static opi_t
concat(void)
{
  opi_t l = opi_pop();
  size_t len = 0;
  for (opi_t it = l; it->type == opi_pair_type; it = opi_cdr(it)) {
    opi_t s = opi_car(it);
    if (opi_unlikely(s->type != opi_string_type)) {
      opi_drop(l);
      return opi_undefined(opi_symbol("type-error"));
    }
    len += OPI_STRLEN(s);
  }

  char *str = malloc(len + 1);
  char *p = str;
  for (opi_t it = l; it->type == opi_pair_type; it = opi_cdr(it)) {
    opi_t s = opi_car(it);
    size_t slen = OPI_STRLEN(s);
    memcpy(p, OPI_STR(s), slen);
    p += slen;
  }
  *p = 0;

  opi_drop(l);
  return opi_string_drain_with_len(str, len);
}

static opi_t
readline(void)
{
  opi_t file = opi_pop();
  if (opi_unlikely(file->type != opi_file_type)) {
    opi_drop(file);
    return opi_undefined(opi_symbol("type-error"));
  }

  FILE *fs = opi_file_get_value(file);
  char *lineptr = NULL;
  size_t n;

  errno = 0;
  ssize_t nrd = getline(&lineptr, &n, fs);
  int err = errno;
  opi_drop(file);

  if (nrd < 0) {
    free(lineptr);
    if (err)
      return opi_undefined(opi_string_new(strerror(err)));
    else
      return opi_false;
  } else {
    return opi_string_drain_with_len(lineptr, nrd);
  }
}

static opi_t
read(void)
{
  OPI_FN()
  OPI_ARG(file, opi_file_type)
  FILE *fs = opi_file_get_value(file);

  if (opi_nargs == 2) {
    OPI_ARG(size, opi_num_type)
    size_t n = OPI_NUM(size);
    char *buf = malloc(n + 1);
    size_t nrd = fread(buf, 1, n, fs);
    if (nrd == 0) {
      free(buf);
      if (feof(fs))
        OPI_RETURN(opi_false);
      else
        OPI_THROW("i/o-error");
    }
    buf[nrd] = 0;
    OPI_RETURN(opi_string_drain_with_len(buf, nrd));

  } else if (opi_nargs == 1) {
    cod_vec(opi_t) bufs;
    cod_vec_init(bufs);

    while (TRUE) {
      char *buf = malloc(0x400);
      size_t nrd = fread(buf, 1, 0x400 - 1, fs);

      if (nrd == 0) {
        free(buf);
        if (feof(fs)) {
          if(bufs.len == 0) {
            cod_vec_destroy(bufs);
            OPI_RETURN(opi_false);

          } else {
            opi_t l = opi_nil;
            for (int i = bufs.len - 1; i >= 0; --i)
              l = opi_cons(bufs.data[i], l);
            cod_vec_destroy(bufs);
            opi_nargs = 1;
            opi_push(l);
            opi_t ret = concat();
            OPI_RETURN(ret);
          }

        } else {
          for (size_t i = 0; i < bufs.len; ++i)
            opi_drop(bufs.data[i]);
          cod_vec_destroy(bufs);
          OPI_THROW("i/o-error");
        }
      }
      buf[nrd] = 0;
      opi_t s = opi_string_drain_with_len(buf, nrd);
      cod_vec_push(bufs, s);
    }

  } else {
    OPI_THROW("arity-error");
  }
}

static opi_t
match(void)
{
  OPI_FN()
  OPI_ARG(regex, opi_regex_type)
  OPI_ARG(str, opi_string_type)

  const char *subj = OPI_STR(str);
  int ns = opi_regex_exec(regex, subj, OPI_STRLEN(str), 0, 0);
  if (opi_unlikely(ns == 0))
    OPI_THROW("regex-memory-limit");
  else if (opi_unlikely(ns < 0))
    OPI_RETURN(opi_false);

  opi_t l = opi_nil;
  for (int i = (ns - 1)*2; i >= 0; i -= 2) {
    size_t len = opi_ovector[i + 1] - opi_ovector[i];
    opi_t s = opi_string_new_with_len(subj + opi_ovector[i], len);
    l = opi_cons(s, l);
  }
  OPI_RETURN(l);
}

static opi_t
split(void)
{
  OPI_FN()
  OPI_ARG(regex, opi_regex_type)
  OPI_ARG(str, opi_string_type)

  if (opi_regex_get_capture_cout(regex) != 0)
    OPI_THROW("regex-error");

  cod_vec(opi_t) buf;
  cod_vec_init(buf);

  const char *subj = OPI_STR(str);
  int len = OPI_STRLEN(str);
  int offs = 0;
  while (offs < len) {
    int ns = opi_regex_exec(regex, subj, OPI_STRLEN(str), offs, 0);
    if (opi_unlikely(ns == 0)) {
      for (size_t i = 0; i < buf.len; ++i)
        opi_drop(buf.data[i]);
      cod_vec_destroy(buf);
      OPI_THROW("regex-memory-limit");
    } else if (opi_unlikely(ns < 0)) {
      // TODO: can calculate length
      opi_t s = opi_string_new(subj + offs);
      cod_vec_push(buf, s);
      break;
    }
    opi_assert(ns == 1);

    size_t len = opi_ovector[0] - offs;
    opi_t s = opi_string_new_with_len(subj + offs, len);
    cod_vec_push(buf, s);

    offs = opi_ovector[1];
  }

  opi_t l = opi_nil;
  for (int i = buf.len - 1; i >= 0; --i)
    l = opi_cons(buf.data[i], l);
  cod_vec_destroy(buf);
  OPI_RETURN(l);
}

static opi_t
foldl(void)
{
  opi_t f = opi_pop();
  opi_inc_rc(f);

  opi_t acc = opi_pop();
  opi_t l = opi_pop();

  // Optimized for list with zero reference count
  while ((l->type == opi_pair_type) & (l->rc == 0)) {
    opi_t x = opi_car(l);
    opi_t tmp = opi_cdr(l);

    opi_h2w_free(l);
    opi_dec_rc(x);
    opi_dec_rc(tmp);

    l = tmp;

    opi_push(x);
    opi_push(acc);
    acc = opi_apply(f, 2);
    if (opi_unlikely(acc->type == opi_undefined_type)) {
      opi_unref(f);
      return acc;
    }
  }

  // Handle non-zero reference count
  while (l->type == opi_pair_type) {
    opi_t x = opi_car(l);
    l = opi_cdr(l);

    opi_push(x);
    opi_push(acc);
    acc = opi_apply(f, 2);
    if (opi_unlikely(acc->type == opi_undefined_type)) {
      opi_unref(f);
      return acc;
    }
  }

  opi_inc_rc(acc);
  opi_drop(l);
  opi_unref(f);
  opi_dec_rc(acc);
  return acc;
}

static opi_t
revfilter(void)
{
  opi_t f = opi_pop();
  opi_inc_rc(f);

  opi_t l = opi_pop();

  opi_t acc = opi_nil;

  // Optimized for list with zero reference count
  while ((l->type == opi_pair_type) & (l->rc == 0)) {
    opi_t x = opi_car(l);
    opi_t tmp = opi_cdr(l);

    opi_h2w_free(l);
    opi_dec_rc(tmp);

    l = tmp;

    opi_push(x);
    opi_t p = opi_apply(f, 1);
    if (opi_unlikely(p->type == opi_undefined_type)) {
      opi_unref(f);
      opi_drop(acc);
      return p;
    }

    if (p != opi_false) {
      acc = opi_cons(x, acc);
      opi_dec_rc(x);
    } else {
      opi_unref(x);
    }
  }

  // Handle non-zero reference count
  while (l->type == opi_pair_type) {
    opi_t x = opi_car(l);
    l = opi_cdr(l);

    opi_push(x);
    opi_t p = opi_apply(f, 1);
    if (opi_unlikely(p->type == opi_undefined_type)) {
      opi_unref(f);
      opi_drop(l);
      return p;
    }

    if (p != opi_false)
      acc = opi_cons(x, acc);
  }

  opi_drop(l);
  opi_unref(f);
  return acc;
}

int
opium_library(OpiBuilder *bldr)
{
  opi_builder_def_const(bldr, "length", opi_fn("length", length , 1));
  opi_builder_def_const(bldr, "strlen", opi_fn("strlen", strlen_, 1));
  opi_builder_def_const(bldr, "substr", opi_fn("substr", substr ,-3));
  opi_builder_def_const(bldr, "strstr", opi_fn("strstr", strstr_, 2));
  opi_builder_def_const(bldr, "chop"  , opi_fn("chop"  , chop   , 1));
  opi_builder_def_const(bldr, "chomp" , opi_fn("chomp" , chomp  , 1));
  opi_builder_def_const(bldr, "ltrim" , opi_fn("ltrim" , ltrim  , 1));
  opi_builder_def_const(bldr, "concat", opi_fn("concat", concat, 1));

  opi_builder_def_const(bldr, "revappend", opi_fn("revappend", revappend, 2));

  opi_builder_def_const(bldr, "__builtin_open", opi_fn("__builtin_open", open_, 2));
  opi_builder_def_const(bldr, "__builtin_popen", opi_fn("__builtin_popen", popen_, 2));
  opi_builder_def_const(bldr, "read", opi_fn("read", read, -2));
  opi_builder_def_const(bldr, "readline", opi_fn("readline", readline, 1));

  opi_builder_def_const(bldr, "match", opi_fn("match", match, 2));
  opi_builder_def_const(bldr, "split", opi_fn("split", split, 2));

  opi_builder_def_const(bldr, "foldl", opi_fn("foldl", foldl, 3));
  opi_builder_def_const(bldr, "revfilter", opi_fn("revfilter", revfilter, 2));

  return 0;
}
