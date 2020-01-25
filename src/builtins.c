#include "opium/opium.h"

#include <math.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

static opi_t
is_(void)
{
  opi_t x = opi_pop();
  opi_t y = opi_pop();
  opi_t ret = opi_is(x, y) ? opi_true : opi_false;
  opi_drop(x);
  opi_drop(y);
  return ret;
}

static opi_t
eq_(void)
{
  opi_t x = opi_pop();
  opi_t y = opi_pop();
  opi_t ret = opi_eq(x, y) ? opi_true : opi_false;
  opi_drop(x);
  opi_drop(y);
  return ret;
}

static opi_t
equal_(void)
{
  opi_t x = opi_pop();
  opi_t y = opi_pop();
  opi_t ret = opi_equal(x, y) ? opi_true : opi_false;
  opi_drop(x);
  opi_drop(y);
  return ret;
}

static opi_t
car_(void)
{
  opi_t x = opi_pop();
  if (opi_unlikely(x->type != opi_pair_type)) {
    opi_drop(x);
    return opi_undefined(opi_symbol("type-error"));
  }
  opi_t ret = opi_car(x);
  opi_inc_rc(ret);
  opi_drop(x);
  opi_dec_rc(ret);
  return ret;
}

static opi_t
cdr_(void)
{
  opi_t x = opi_pop();
  if (opi_unlikely(x->type != opi_pair_type)) {
    opi_drop(x);
    return opi_undefined(opi_symbol("type-error"));
  }
  opi_t ret = opi_cdr(x);
  opi_inc_rc(ret);
  opi_drop(x);
  opi_dec_rc(ret);
  return ret;
}

static opi_t
null_(void)
{
  opi_t x = opi_pop();
  opi_t ret = x == opi_nil ? opi_true : opi_false;
  opi_drop(x);
  return ret;
}

static opi_t
print(void)
{
  int isfirst = TRUE;
  int nargs = opi_nargs;
  while (nargs--) {
    opi_t x = opi_pop();
    if (!isfirst)
      putc(' ', stdout);
    opi_display(x, stdout);
    opi_drop(x);
    isfirst = FALSE;
  }
  putc('\n', stdout);
  return opi_nil;
}

static opi_t
format_aux(const char *fmt, size_t offs, opi_t port, size_t nargs)
{
  FILE *fs = opi_file_get_value(port);
  size_t iarg = 0;
  opi_t x;
  for (const char *p = fmt; *p; ++p) {
    if (*p == '%') {
      switch (*++p) {
        case '%':
          fputc('%', fs);
          break;

        case 'w':
          if (opi_unlikely(iarg > nargs))
            return opi_undefined(opi_symbol("out-of-range"));
          x = opi_get(offs + iarg++);
          opi_write(x, fs);
          opi_drop(x);
          break;

        case 'd':
          if (opi_unlikely(iarg > nargs))
            return opi_undefined(opi_symbol("out-of-range"));
          x = opi_get(offs + iarg++);
          opi_display(x, fs);
          opi_drop(x);
          break;

        default:
          return opi_undefined(opi_symbol("format-error"));
      }
    } else {
      putc(*p, fs);
    }
  }
  return NULL;
}

static opi_t
fprintf_(void)
{
  opi_t err = NULL;
  size_t nargs = opi_nargs;

  // lock all arguments
  for (size_t i = 1; i <= nargs; ++i)
    opi_inc_rc(opi_get(i));

  opi_t port = opi_get(1);
  opi_t fmt = opi_get(2);
  if (opi_unlikely(port->type != opi_file_type)) {
    err = opi_undefined(opi_symbol("type-error"));
    goto error;
  }
  if (opi_unlikely(fmt->type != opi_str_type)) {
    err = opi_undefined(opi_symbol("type-error"));
    goto error;
  }

  if ((err = format_aux(OPI_STR(fmt)->str, 3, port, nargs - 2)))
    goto error;

  err = opi_nil;

error:
  while (nargs--)
    opi_unref(opi_pop());
  return err;
}

static opi_t
printf_(void)
{
  opi_push(opi_stdout);
  opi_nargs += 1;
  return fprintf_();
}

static opi_t
format(void)
{
  opi_t err = NULL;
  size_t nargs = opi_nargs;

  // lock all arguments
  for (size_t i = 1; i <= nargs; ++i)
    opi_inc_rc(opi_get(i));

  char *ptr;
  size_t size;
  FILE *fs = open_memstream(&ptr, &size);
  opi_t port = opi_file(fs, fclose);
  opi_inc_rc(port);

  opi_t fmt = opi_get(1);
  if (opi_unlikely(fmt->type != opi_str_type)) {
    err = opi_undefined(opi_symbol("type-error"));
    goto error;
  }

  if ((err = format_aux(OPI_STR(fmt)->str, 2, port, nargs - 1)))
    goto error;

  opi_unref(port);
  err = opi_str_new(ptr);
  free(ptr);
  goto end;

error:
  opi_unref(port);
  free(ptr);
end:
  while (nargs--)
    opi_unref(opi_pop());
  return err;
}

static opi_t
newline_(void)
{
  opi_t p = opi_nargs == 1 ? opi_pop() : opi_stdout;
  putc('\n', opi_file_get_value(p));
  opi_drop(p);
  return opi_nil;
}

static opi_t
undefined_(void)
{
  return opi_undefined(opi_nil);
}

static opi_t
error_(void)
{
  return opi_undefined(opi_pop());
}

static opi_t
not_(void)
{
  opi_t x = opi_pop();
  opi_t ret = x == opi_false ? opi_true : opi_false;
  opi_drop(x);
  return ret;
}

static opi_t
die(void)
{
  opi_t x = opi_pop();
  opi_error("\x1b[38;5;196;1mdie\x1b[0m ");
  opi_display(x, OPI_ERROR);
  putc('\n', OPI_ERROR);
  exit(EXIT_FAILURE);
}

static opi_t
id(void)
{
  return opi_pop();
}

static opi_t
List(void)
{
  opi_t acc = opi_nil;
  for (size_t i = opi_nargs; i > 0; --i)
    acc = opi_cons(opi_get(i), acc);
  opi_popn(opi_nargs);
  return acc;
}

static opi_t
Table(void)
{
  opi_t l = opi_pop();
  opi_t tab = opi_table(l, FALSE);
  opi_drop(l);
  return tab;
}

static opi_t
table_ref(void)
{
  opi_t tab = opi_pop();
  opi_inc_rc(tab);
  opi_t key = opi_pop();
  opi_inc_rc(key);

  if (opi_unlikely(tab->type != opi_table_type)) {
    opi_unref(tab);
    opi_unref(key);
    return opi_undefined(opi_symbol("type-error"));
  }

  opi_t err = NULL;
  opi_t x = opi_table_at(tab, key, &err);
  opi_t ret = x ? opi_cdr(x) : err;

  opi_inc_rc(ret);
  opi_unref(tab);
  opi_unref(key);
  opi_dec_rc(ret);
  return ret;
}

static opi_t
pairs(void)
{
  opi_t tab = opi_pop();
  if (opi_unlikely(tab->type != opi_table_type)) {
    opi_drop(tab);
    return opi_undefined(opi_symbol("type-error"));
  }
  opi_t ret = opi_table_pairs(tab);
  opi_inc_rc(ret);
  opi_drop(tab);
  opi_dec_rc(ret);
  return ret;
}

static opi_t
str_at(void)
{
  opi_t str = opi_pop();
  opi_inc_rc(str);
  opi_t idx = opi_pop();
  opi_inc_rc(idx);

  if (opi_unlikely(str->type != opi_str_type)) {
    opi_unref(str);
    opi_unref(idx);
    return opi_undefined(opi_symbol("type-error"));
  }

  if (opi_unlikely(idx->type != opi_num_type)) {
    opi_unref(str);
    opi_unref(idx);
    return opi_undefined(opi_symbol("type-error"));
  }

  size_t k = opi_num_get_value(idx);
  if (opi_unlikely(k >= OPI_STR(str)->len)) {
    opi_unref(str);
    opi_unref(idx);
    return opi_undefined(opi_symbol("out-of-range"));
  }

  opi_t ret = opi_str_from_char(OPI_STR(str)->str[k]);
  opi_unref(str);
  opi_unref(idx);
  return ret;
}

static opi_t
apply(void)
{
  opi_t f = opi_pop();
  opi_t l = opi_pop();
  opi_write(l, OPI_DEBUG); putc('\n', OPI_DEBUG);

  if (f->type != opi_fn_type) {
    opi_drop(f);
    opi_drop(l);
    return opi_undefined(opi_symbol("type-error"));
  }

  size_t nargs = opi_length(l);

  opi_sp += nargs;
  size_t iarg = 1;
  for (opi_t it = l; it->type == opi_pair_type; it = opi_cdr(it))
    opi_sp[-iarg++] = opi_car(it);
  opi_debug("apply %zu args\n", nargs);
  opi_t ret = opi_apply(f, nargs);

  opi_inc_rc(ret);
  opi_drop(f);
  opi_drop(l);
  opi_dec_rc(ret);

  return ret;
}

static opi_t
lazy(void)
{
  opi_t x = opi_pop();
  if (opi_unlikely(x->type != opi_fn_type)) {
    opi_drop(x);
    return opi_undefined(opi_symbol("type-error"));
  }
  if (opi_unlikely(!opi_test_arity(opi_fn_get_arity(x), 0))) {
    opi_drop(x);
    return opi_undefined(opi_symbol("arity-error"));
  }
  return opi_lazy(x);
}

static opi_t
force(void)
{
  opi_t lazy = opi_pop();
  if (opi_unlikely(lazy->type != opi_lazy_type)) {
    return lazy;
    /*opi_drop(lazy);*/
    /*return opi_undefined(opi_symbol("type-error"));*/
  }
  opi_inc_rc(lazy);
  opi_t ret = opi_lazy_get_value(lazy);
  opi_inc_rc(ret);
  opi_unref(lazy);
  opi_dec_rc(ret);
  return ret;
}

struct compose_data { opi_t f, g; };

static void
compose_delete(OpiFn *fn)
{
  struct compose_data *data = fn->data;
  opi_unref(data->f);
  opi_unref(data->g);
  free(data);
  opi_fn_delete(fn);
}

static opi_t
compose_aux(void)
{
  struct compose_data *data = opi_current_fn->data;
  opi_t tmp = opi_apply(data->g, opi_nargs);
  if (opi_unlikely(tmp->type == opi_undefined_type))
    return tmp;
  opi_push(tmp);
  return opi_apply(data->f, 1);
}

static opi_t
compose(void)
{
  opi_t f = opi_pop();
  opi_t g = opi_pop();
  struct compose_data *data = malloc(sizeof(struct compose_data));
  opi_inc_rc(data->f = f);
  opi_inc_rc(data->g = g);
  opi_t aux = opi_fn_new(compose_aux, 1);
  opi_fn_set_data(aux, data, compose_delete);
  return aux;
}

static opi_t
concat(void)
{
  OPI_BEGIN_FN()
  OPI_ARG(s1, opi_str_type)
  OPI_ARG(s2, opi_str_type)
  size_t l1 = OPI_STR(s1)->len;
  size_t l2 = OPI_STR(s2)->len;
  char *buf = malloc(l1 + l2 + 1);
  memcpy(buf, OPI_STR(s1)->str, l1);
  memcpy(buf + l1, OPI_STR(s2)->str, l2);
  buf[l1 + l2] = '\0';

  opi_unref(s1);
  opi_unref(s2);
  return opi_str_drain_with_len(buf, l1 + l2);
}

struct vaarg_data {
  opi_t f;
  size_t nmin;
};

static void
vaarg_delete(OpiFn *fn)
{
  struct vaarg_data *data = fn->data;
  opi_unref(data->f);
  free(data);
  opi_fn_delete(fn);
}

static opi_t
vaarg_aux(void)
{
  struct vaarg_data *data = opi_current_fn->data;

  if (opi_unlikely(opi_nargs < data->nmin)) {
    while (opi_nargs--)
      opi_drop(opi_pop());
    return opi_undefined(opi_symbol("arity-error"));
  }

  opi_t args[data->nmin + 1];
  for (size_t i = 0; i < data->nmin; ++i, --opi_nargs)
    args[i] = opi_pop();
  args[data->nmin] = List();

  for (int i = data->nmin; i >= 0; --i)
    opi_push(args[i]);

  return opi_fn_apply(data->f, data->nmin + 1);
}

static opi_t
vaarg(void)
{
  opi_t nmin = opi_pop();
  opi_t f = opi_pop();

  if (opi_unlikely(nmin->type != opi_num_type || f->type != opi_fn_type)) {
    opi_drop(nmin);
    opi_drop(f);
    return opi_undefined(opi_symbol("type-error"));
  }

  size_t ari = opi_num_get_value(nmin);
  opi_drop(nmin);

  if (opi_unlikely(!opi_test_arity(opi_fn_get_arity(f), ari + 1))) {
    opi_drop(f);
    return opi_undefined(opi_symbol("airty-error"));
  }

  struct vaarg_data *data = malloc(sizeof(struct vaarg_data));
  opi_inc_rc(data->f = f);
  data->nmin = ari;

  opi_t f_va = opi_fn_new(vaarg_aux, -(ari + 1));
  opi_fn_set_data(f_va, data, vaarg_delete);
  return f_va;
}

static opi_t
system_(void)
{
  opi_t cmd = opi_pop();
  if (opi_unlikely(cmd->type != opi_str_type)) {
    opi_drop(cmd);
    return opi_undefined(opi_symbol("type-error"));
  }
  int err = system(OPI_STR(cmd)->str);
  opi_drop(cmd);
  return opi_num_new(err);
}

static opi_t
shell(void)
{
  opi_t cmd = opi_pop();
  if (opi_unlikely(cmd->type != opi_str_type)) {
    opi_drop(cmd);
    return opi_undefined(opi_symbol("type-error"));
  }

  FILE *fs = popen(OPI_STR(cmd)->str, "r");
  opi_drop(cmd);

  if (!fs)
    return opi_undefined(opi_str_new(strerror(errno)));

  cod_vec(char) buf;
  cod_vec_init(buf);

  errno = 0;
  while (TRUE) {
    int c = fgetc(fs);
    if (opi_unlikely(c == EOF)) {
      if (errno) {
        pclose(fs);
        cod_vec_destroy(buf);
        return opi_undefined(opi_str_new(strerror(errno)));

      } else {
        errno = 0;
        int err = pclose(fs);
        if (err || errno) {
          cod_vec_destroy(buf);
          if (err)
            return opi_undefined(opi_symbol("shell-error"));
          else
            return opi_undefined(opi_str_new(strerror(errno)));
        }
        if (buf.len > 0 && buf.data[buf.len - 1] == '\n')
          buf.data[buf.len - 1] = 0;
        else
          cod_vec_push(buf, '\0');
        return opi_str_drain_with_len(buf.data, buf.len - 1);
      }
    }
    cod_vec_push(buf, c);
  }
}

static opi_t
exit_(void)
{
  opi_t err = opi_pop();
  if (err->type != opi_num_type) {
    opi_drop(err);
    return opi_undefined(opi_symbol("type-error"));
  }
  long double e = opi_num_get_value(err);
  opi_drop(err);
  if (e < 0 || e > 255) {
    opi_drop(err);
    return opi_undefined(opi_symbol("domain-error"));
  }
  exit(e);
}

static opi_t
regex(void)
{
  opi_t pattern = opi_pop();
  opi_assert(pattern->type == opi_str_type);

  const char *err;
  opi_t regex = opi_regex_new(OPI_STR(pattern)->str, 0, &err);
  if (regex == NULL) {
    opi_error("%s\n", err);
    abort();
  }
  return regex;
}

typedef cod_vec(char) string_t;

static int
replace(int n, const char *src, const char *p, string_t* out)
{
  for (; *p; ++p) {
    if (*p == '\\') {
      ++p;

      if (*p == 0) {
        cod_vec_push(*out, '\\');
        return 0;
      }

      if (isdigit(*p)) {
        char *endptr;
        int i = strtoul(p, &endptr, 10);
        if (i >= n)
          return -1;

        int idx = i*2;
        for (int i = opi_ovector[idx]; i < opi_ovector[idx + 1]; ++i)
          cod_vec_push(*out, src[i]);

        p = endptr - 1;
        continue;
      }

      cod_vec_push(*out, '\\');
      if (*p != '\\') {
        cod_vec_push(*out, '\\');
        cod_vec_push(*out, *p);
      }
      continue;
    }

    cod_vec_push(*out, *p);
  }

  return 0;
}

static opi_t
search_replace(void)
{
  OPI_BEGIN_FN()
  OPI_ARG(re, opi_regex_type)
  OPI_ARG(pat_, opi_str_type);
  OPI_ARG(opt_, opi_str_type);
  OPI_ARG(str_, opi_str_type);

  const char *pat = OPI_STR(pat_)->str;
  const char *str = OPI_STR(str_)->str;
  int len = OPI_STR(str_)->len;
  const char *opt = OPI_STR(opt_)->str;

  int g = !!strchr(opt, 'g');

  string_t out;
  cod_vec_init(out);

  int offs = 0;
  while (offs < len) {
    int n = opi_regex_exec(re, str, len, offs, 0);

    if (n < 0)
      break;
    opi_assert(n != 0);

    int start = opi_ovector[0];
    int end = opi_ovector[1];

    for (int i = offs; i < start; ++i)
      cod_vec_push(out, str[i]);
    if (replace(n, str, pat, &out)) {
      cod_vec_destroy(out);
      OPI_THROW("regex-error");
    }
    offs = end;

    if (!g)
      break;
  }

  while (offs < len)
    cod_vec_push(out, str[offs++]);
  cod_vec_push(out, 0);
  OPI_RETURN(opi_str_drain_with_len(out.data, out.len));
}

static opi_t
number(void)
{
  OPI_BEGIN_FN()
  OPI_ARG(str, opi_str_type)
  char *endptr;
  long double num = strtold(OPI_STR(str)->str, &endptr);
  if (endptr == OPI_STR(str)->str)
    OPI_THROW("format-error");
  OPI_RETURN(opi_num_new(num));
}

static
OPI_DEF(power,
  opi_arg(x, opi_num_type)
  opi_arg(y, opi_num_type)
  opi_t ret = opi_num_new(powl(OPI_NUM(x)->val, OPI_NUM(y)->val));
  opi_unref(x);
  opi_unref(y);
  return ret;
)

static opi_t
addressof(void)
{
  opi_t x = opi_pop();
  opi_t ret = opi_num_new((uintptr_t)x);
  opi_drop(x);
  return ret;
}

void
opi_builtins(OpiBuilder *bldr)
{
  opi_builder_def_const(bldr, "^", opi_fn_new(power, 2));
  opi_builder_def_const(bldr, ".", opi_fn_new(compose, 2));
  opi_builder_def_const(bldr, "++", opi_fn_new(concat, 2));
  opi_builder_def_const(bldr, "car", opi_fn_new(car_, 1));
  opi_builder_def_const(bldr, "cdr", opi_fn_new(cdr_, 1));

  opi_builder_def_const(bldr, "List", opi_fn_new(List, -1));
  opi_builder_def_const(bldr, "Table", opi_fn_new(Table, 1));
  opi_builder_def_const(bldr, "number", opi_fn_new(number, 1));

  opi_builder_def_const(bldr, "regex", opi_fn_new(regex, 1));

  opi_builder_def_const(bldr, "#", opi_fn_new(table_ref, 2));

  opi_builder_def_const(bldr, "pairs", opi_fn_new(pairs, 1));
  opi_builder_def_const(bldr, "is", opi_fn_new(is_, 2));
  opi_builder_def_const(bldr, "eq", opi_fn_new(eq_, 2));
  opi_builder_def_const(bldr, "equal", opi_fn_new(equal_, 2));
  opi_builder_def_const(bldr, "not", opi_fn_new(not_, 1));
  opi_builder_def_const(bldr, "apply", opi_fn_new(apply, 2));
  opi_builder_def_const(bldr, "vaarg", opi_fn_new(vaarg, 2));

  opi_builder_def_const(bldr, "newline", opi_fn_new(newline_, -1));
  opi_builder_def_const(bldr, "print", opi_fn_new(print, -1));
  opi_builder_def_const(bldr, "printf", opi_fn_new(printf_, -2));
  opi_builder_def_const(bldr, "fprintf", opi_fn_new(fprintf_, -3));
  opi_builder_def_const(bldr, "format", opi_fn_new(format, -2));

  opi_builder_def_const(bldr, "()", opi_fn_new(undefined_, 0));
  opi_builder_def_const(bldr, "error", opi_fn_new(error_, 1));
  opi_builder_def_const(bldr, "die", opi_fn_new(die, 1));
  opi_builder_def_const(bldr, "id", opi_fn_new(id, 1));

  opi_builder_def_const(bldr, "lazy", opi_fn_new(lazy, 1));
  opi_builder_def_const(bldr, "force", opi_fn_new(force, 1));

  opi_builder_def_const(bldr, "system", opi_fn_new(system_, 1));
  opi_builder_def_const(bldr, "shell", opi_fn_new(shell, 1));

  opi_builder_def_const(bldr, "exit", opi_fn_new(exit_, 1));

  opi_builder_def_const(bldr, "__builtin_sr", opi_fn_new(search_replace, 4));

  opi_builder_def_const(bldr, "addressof", opi_fn_new(addressof, 1));
}
