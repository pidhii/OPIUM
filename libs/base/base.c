#include "opium/opium.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>

static opi_t
length(void)
{
  opi_t x = opi_pop();
  opi_t ret = opi_number(opi_length(x));
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
  opi_t ret = opi_number(opi_string_get_length(str));
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

  if (opi_unlikely(start->type != opi_number_type)) {
    opi_drop(str);
    opi_drop(start);
    if (end)
      opi_drop(end);
    return opi_undefined(opi_symbol("type-error"));
  }

  if (end && opi_unlikely(end->type != opi_number_type)) {
    opi_drop(str);
    opi_drop(start);
    opi_drop(end);
    return opi_undefined(opi_symbol("type-error"));
  }

  const char *s = opi_string_get_value(str);
  ssize_t len = opi_string_get_length(str);
  ssize_t from = opi_number_get_value(start);
  ssize_t to = end ? opi_number_get_value(end) : len;

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

  opi_t ret = opi_string2(s + from, to - from);
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
      opi_as(str, struct opi_string).size -= 1;
      return str;
    } else {
      return opi_string2(s, len - 1);
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
      opi_as(str, struct opi_string).size = newlen;
      return str;
    } else {
      return opi_string2(s, newlen);
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
    opi_t ret = opi_string2(p, len - (p - s));
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
    opi_t ret = opi_number(at - opi_string_get_value(str));
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
    return opi_undefined(opi_string(strerror(errno)));
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
    return opi_undefined(opi_string(strerror(errno)));
  else
    return opi_file(fs, pclose);
}

static opi_t
getline_(void)
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
    if (err)
      return opi_undefined(opi_string(strerror(err)));
    else
      return opi_false;
  } else {
    return opi_string_move2(lineptr, nrd);
  }
}

static opi_t
getdelim_(void)
{
  opi_t file = opi_pop();
  opi_t delim = opi_pop();
  if (opi_unlikely(file->type != opi_file_type
                || delim->type != opi_string_type)) {
    opi_drop(file);
    opi_drop(delim);
    return opi_undefined(opi_symbol("type-error"));
  }

  int d;
  if (opi_string_get_length(delim) == 0) {
    d = EOF;
  } else if (opi_string_get_length(delim) == 1) {
    d = opi_string_get_value(delim)[0];
  } else {
    opi_drop(file);
    opi_drop(delim);
    return opi_undefined(opi_symbol("domain-error"));
  }
  opi_drop(delim);

  FILE *fs = opi_file_get_value(file);
  char *lineptr = NULL;
  size_t n;

  errno = 0;
  ssize_t nrd = getdelim(&lineptr, &n, d, fs);
  int err = errno;
  opi_drop(file);

  if (nrd < 0) {
    if (err)
      return opi_undefined(opi_string(strerror(err)));
    else
      return opi_false;
  } else {
    return opi_string_move2(lineptr, nrd);
  }
}

int
opium_library(struct opi_builder *bldr)
{
  opi_builder_def_const(bldr, "length", opi_fn("length", length , 1));
  opi_builder_def_const(bldr, "strlen", opi_fn("strlen", strlen_, 1));
  opi_builder_def_const(bldr, "substr", opi_fn("substr", substr ,-3));
  opi_builder_def_const(bldr, "strstr", opi_fn("strstr", strstr_, 2));
  opi_builder_def_const(bldr, "chop"  , opi_fn("chop"  , chop   , 1));
  opi_builder_def_const(bldr, "chomp" , opi_fn("chomp" , chomp  , 1));
  opi_builder_def_const(bldr, "ltrim" , opi_fn("ltrim" , ltrim  , 1));

  opi_builder_def_const(bldr, "revappend", opi_fn("revappend", revappend, 2));

  opi_builder_def_const(bldr, "__builtin_open", opi_fn("__builtin_open", open_, 2));
  opi_builder_def_const(bldr, "__builtin_popen", opi_fn("__builtin_popen", popen_, 2));
  opi_builder_def_const(bldr, "getline", opi_fn("getline", getline_, 1));
  opi_builder_def_const(bldr, "getdelim", opi_fn("getdelim", getdelim_, 2));

  return 0;
}
