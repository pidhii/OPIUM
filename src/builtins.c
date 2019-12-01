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
  if (opi_unlikely(fmt->type != opi_string_type)) {
    err = opi_undefined(opi_symbol("type-error"));
    goto error;
  }

  if ((err = format_aux(opi_string_get_value(fmt), 3, port, nargs - 2)))
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
  if (opi_unlikely(fmt->type != opi_string_type)) {
    err = opi_undefined(opi_symbol("type-error"));
    goto error;
  }

  if ((err = format_aux(opi_string_get_value(fmt), 2, port, nargs - 1)))
    goto error;

  opi_unref(port);
  err = opi_string_new(ptr);
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
list(void)
{
  opi_t acc = opi_nil;
  for (size_t i = opi_nargs; i > 0; --i)
    acc = opi_cons(opi_get(i), acc);
  opi_popn(opi_nargs);
  return acc;
}

static opi_t
table(void)
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
string_at(void)
{
  opi_t str = opi_pop();
  opi_inc_rc(str);
  opi_t idx = opi_pop();
  opi_inc_rc(idx);

  if (opi_unlikely(str->type != opi_string_type)) {
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
  if (opi_unlikely(k >= opi_string_get_length(str))) {
    opi_unref(str);
    opi_unref(idx);
    return opi_undefined(opi_symbol("out-of-range"));
  }

  opi_t ret = opi_string_from_char(opi_string_get_value(str)[k]);
  opi_unref(str);
  opi_unref(idx);
  return ret;
}

static opi_t
apply(void)
{
  opi_t f = opi_pop();
  opi_t l = opi_pop();

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
  struct compose_data *data = opi_fn_get_data(opi_current_fn);

  opi_t tmp = opi_apply(data->g, opi_nargs);
  if (opi_unlikely(tmp->type == opi_undefined_type))
    return tmp;

  opi_push(tmp);
  return opi_apply(data->f, 1);
}

static opi_t
concat(void)
{
  opi_t lhs = opi_pop();
  opi_inc_rc(lhs);
  opi_t rhs = opi_pop();
  opi_inc_rc(rhs);
  if (lhs->type == opi_string_type) {
    // String concatenation
    if (rhs->type != opi_string_type) {
      opi_unref(lhs);
      opi_unref(rhs);
      return opi_undefined(opi_symbol("type-error"));
    }
    size_t l1 = opi_string_get_length(lhs);
    size_t l2 = opi_string_get_length(rhs);
    char *buf = malloc(l1 + l2 + 1);
    memcpy(buf, opi_string_get_value(lhs), l1);
    memcpy(buf + l1, opi_string_get_value(rhs), l2);
    buf[l1 + l2] = '\0';
    opi_unref(lhs);
    opi_unref(rhs);
    return opi_string_drain_with_len(buf, l1 + l2);

  } else if (lhs->type == opi_fn_type) {
    // Function composition
    if (rhs->type != opi_fn_type) {
      opi_unref(lhs);
      opi_unref(rhs);
      return opi_undefined(opi_symbol("type-error"));
    }
    struct compose_data *data = malloc(sizeof(struct compose_data));
    data->f = lhs;
    data->g = rhs;
    opi_t aux = opi_fn("composition", compose_aux, opi_fn_get_arity(rhs));
    opi_fn_set_data(aux, data, compose_delete);
    return aux;

  } else {
    opi_unref(lhs);
    opi_unref(rhs);
    return opi_undefined(opi_symbol("type-error"));
  }
}

#define TYPE_PRED(name, ty)                          \
  static opi_t                                       \
  name(void)                                         \
  {                                                  \
    opi_t x = opi_pop();                             \
    opi_t ret = x->type == ty? opi_true : opi_false; \
    opi_drop(x);                                     \
    return ret;                                      \
  }

TYPE_PRED(boolean_p, opi_boolean_type)
TYPE_PRED(pair_p, opi_pair_type)
TYPE_PRED(string_p, opi_string_type)
TYPE_PRED(undefined_p, opi_undefined_type)
TYPE_PRED(number_p, opi_num_type)
TYPE_PRED(symbol_p, opi_symbol_type)
TYPE_PRED(fn_p, opi_fn_type)
TYPE_PRED(svector_p, opi_svector_type)
TYPE_PRED(dvector_p, opi_dvector_type)
TYPE_PRED(FILE_p, opi_file_type)
TYPE_PRED(table_p, opi_table_type)
TYPE_PRED(lazy_p, opi_lazy_type)

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
  struct vaarg_data *data = opi_fn_get_data(opi_current_fn);

  if (opi_unlikely(opi_nargs < data->nmin)) {
    while (opi_nargs--)
      opi_drop(opi_pop());
    return opi_undefined(opi_symbol("arity-error"));
  }

  opi_t args[data->nmin + 1];
  for (size_t i = 0; i < data->nmin; ++i, --opi_nargs)
    args[i] = opi_pop();
  args[data->nmin] = list();

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

  opi_t f_va = opi_fn(NULL, vaarg_aux, -(ari + 1));
  opi_fn_set_data(f_va, data, vaarg_delete);
  return f_va;
}

static opi_t
system_(void)
{
  opi_t cmd = opi_pop();
  if (opi_unlikely(cmd->type != opi_string_type)) {
    opi_drop(cmd);
    return opi_undefined(opi_symbol("type-error"));
  }
  int err = system(opi_string_get_value(cmd));
  opi_drop(cmd);
  return opi_num_new(err);
}

static opi_t
shell(void)
{
  opi_t cmd = opi_pop();
  if (opi_unlikely(cmd->type != opi_string_type)) {
    opi_drop(cmd);
    return opi_undefined(opi_symbol("type-error"));
  }

  FILE *fs = popen(opi_string_get_value(cmd), "r");
  opi_drop(cmd);

  if (!fs)
    return opi_undefined(opi_string_new(strerror(errno)));

  cod_vec(char) buf;
  cod_vec_init(buf);

  errno = 0;
  while (TRUE) {
    int c = fgetc(fs);
    if (opi_unlikely(c == EOF)) {
      if (errno) {
        pclose(fs);
        cod_vec_destroy(buf);
        return opi_undefined(opi_string_new(strerror(errno)));

      } else {
        errno = 0;
        int err = pclose(fs);
        if (err || errno) {
          cod_vec_destroy(buf);
          if (err)
            return opi_undefined(opi_symbol("shell-error"));
          else
            return opi_undefined(opi_string_new(strerror(errno)));
        }
        if (buf.len > 0 && buf.data[buf.len - 1] == '\n')
          buf.data[buf.len - 1] = 0;
        else
          cod_vec_push(buf, '\0');
        return opi_string_drain_with_len(buf.data, buf.len - 1);
      }
    }
    cod_vec_push(buf, c);
  }
}

static opi_t
loadfile(void)
{
  OpiContext *ctx = opi_fn_get_data(opi_current_fn);

  opi_t path = opi_pop();
  opi_t srcd = opi_nargs > 1 ? opi_pop() : opi_nil;
  if (opi_unlikely(path->type != opi_string_type)) {
    opi_drop(path);
    opi_drop(srcd);
    return opi_undefined(opi_symbol("type-error"));
  }

  FILE *fs = fopen(opi_string_get_value(path), "r");
  opi_drop(path);
  if (!fs) {
    opi_drop(srcd);
    return opi_undefined(opi_string_new(strerror(errno)));
  }

  opi_error = 0;
  OpiAst *ast = opi_parse(fs);
  fclose(fs);
  if (opi_error) {
    opi_drop(srcd);
    return opi_undefined(opi_symbol("parse-error"));
  }

  OpiBuilder bldr;
  opi_builder_init(&bldr, ctx);
  opi_builtins(&bldr);
  for (opi_t it = srcd; it->type == opi_pair_type; it = opi_cdr(it)) {
    opi_t d = opi_car(it);
    if (d->type != opi_string_type) {
      opi_drop(srcd);
      opi_ast_delete(ast);
      opi_builder_destroy(&bldr);
      return opi_undefined(opi_symbol("type-error"));
    }
    opi_builder_add_source_directory(&bldr, opi_string_get_value(d));
  }
  opi_drop(srcd);

  OpiBytecode *bc = opi_build(&bldr, ast, OPI_BUILD_DEFAULT);
  opi_ast_delete(ast);
  if (bc == NULL) {
    opi_builder_destroy(&bldr);
    return opi_undefined(opi_symbol("build-error"));
  }

  opi_t ret = opi_vm(bc);
  opi_inc_rc(ret);

  opi_context_drain_bytecode(ctx, bc);
  opi_bytecode_delete(bc);
  opi_builder_destroy(&bldr);

  opi_dec_rc(ret);
  return ret;
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
dvector(void)
{
  cod_vec(double) buf;
  cod_vec_init(buf);
  opi_t l = opi_pop();
  for (opi_t it = l; it->type == opi_pair_type; it = opi_cdr(it)) {
    opi_t x = opi_car(it);
    if (opi_unlikely(x->type != opi_num_type)) {
      cod_vec_destroy(buf);
      opi_drop(l);
      return opi_undefined(opi_symbol("type-error"));
    }
    cod_vec_push(buf, OPI_NUM(x));
  }
  opi_drop(l);
  return opi_dvector_new_moved(buf.data, buf.len);
}

static opi_t
svector(void)
{
  cod_vec(float) buf;
  cod_vec_init(buf);
  opi_t l = opi_pop();
  for (opi_t it = l; it->type == opi_pair_type; it = opi_cdr(it)) {
    opi_t x = opi_car(it);
    if (opi_unlikely(x->type != opi_num_type)) {
      cod_vec_destroy(buf);
      opi_drop(l);
      return opi_undefined(opi_symbol("type-error"));
    }
    cod_vec_push(buf, OPI_NUM(x));
  }
  opi_drop(l);
  return opi_svector_new_moved(buf.data, buf.len);
}

static opi_t
regex(void)
{
  opi_t pattern = opi_pop();
  opi_assert(pattern->type == opi_string_type);

  const char *err;
  opi_t regex = opi_regex_new(OPI_STR(pattern), 0, &err);
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
  OPI_FN()
  OPI_ARG(re, opi_regex_type)
  OPI_ARG(pat_, opi_string_type);
  OPI_ARG(opt_, opi_string_type);
  OPI_ARG(str_, opi_string_type);

  const char *pat = OPI_STR(pat_);
  const char *str = OPI_STR(str_);
  int len = OPI_STRLEN(str_);
  const char *opt = OPI_STR(opt_);

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
  OPI_RETURN(opi_string_drain_with_len(out.data, out.len));
}

static opi_t
number(void)
{
  OPI_FN()
  OPI_ARG(str, opi_string_type)
  char *endptr;
  long double num = strtold(OPI_STR(str), &endptr);
  if (endptr == OPI_STR(str))
    OPI_THROW("format-error");
  OPI_RETURN(opi_num_new(num));
}

/*static opi_t*/
/*next(void)*/
/*{*/
  /*opi_t g = opi_pop();*/
  /*if (g->type != opi_gen_type) {*/
    /*return g;*/
  /*}*/

  /*OpiGen *gen = opi_as_ptr(g);*/
  /*if (gen->is_done) {*/
    /*[>opi_debug("next (1)\n");<]*/
    /*opi_t ret = opi_cons(gen->val, gen->next);*/
    /*opi_drop(g);*/
    /*return ret;*/

  /*} else {*/
    /*if (g->rc == 0) {*/
      /*[>opi_debug("next (2)\n");<]*/
      /*opi_t this = gen->val;*/
      /*opi_t next = opi_vm_continue(gen->state);*/
      /*if (next) {*/
        /*gen->val = next; // incremented by VM*/
        /*opi_dec_rc(this);*/
        /*return opi_cons(this, g);*/
      /*} else {*/
        /*opi_t ret = opi_cons(this, opi_nil);*/
        /*gen->is_done = TRUE;*/
        /*opi_state_destroy(gen->state);*/
        /*free(gen->state);*/
        /*gen->next = NULL;*/
        /*opi_drop(g);*/
        /*return ret;*/
      /*}*/

    /*} else {*/
      /*[>opi_debug("next (3)\n");<]*/
      /*opi_t next = opi_vm_continue(gen->state); // incremented by VM*/
      /*if (next) {*/
        /*opi_t next_gen = opi_gen_new(next, gen->state);*/
        /*gen->is_done = TRUE;*/
        /*opi_inc_rc(gen->next = next_gen);*/
        /*return opi_cons(gen->val, next_gen);*/
      /*} else {*/
        /*gen->is_done = TRUE;*/
        /*opi_state_destroy(gen->state);*/
        /*free(gen->state);*/
        /*opi_inc_rc(gen->next = opi_nil);*/
        /*return opi_cons(gen->val, opi_nil);*/
      /*}*/
    /*}*/
  /*}*/
/*}*/

static opi_t
next(void)
{
  opi_t g = opi_pop();
  if (g->type != opi_gen_type)
    return g;

drain:;
  opi_t ret = opi_gen_drain_value(g);
  if (ret) {
    opi_drop(g);
    opi_dec_rc(ret);
    return ret;
  }

  if (opi_gen_is_done(g)) {
    opi_drop(g);
    return opi_undefined(opi_symbol("End"));
  }

  opi_gen_next(g);
  goto drain;
}

void
opi_builtins(OpiBuilder *bldr)
{
  opi_builder_def_const(bldr, "null?", opi_fn("null?", null_, 1));
  opi_builder_def_const(bldr, "boolean?", opi_fn("boolean?", boolean_p, 1));
  opi_builder_def_const(bldr, "pair?", opi_fn("pair?", pair_p, 1));
  opi_builder_def_const(bldr, "string?", opi_fn("string?", string_p, 1));
  opi_builder_def_const(bldr, "undefined?", opi_fn("undefined?", undefined_p, 1));
  opi_builder_def_const(bldr, "number?", opi_fn("number?", number_p, 1));
  opi_builder_def_const(bldr, "symbol?", opi_fn("symbol?", symbol_p, 1));
  opi_builder_def_const(bldr, "fn?", opi_fn("fn?", fn_p, 1));
  opi_builder_def_const(bldr, "lazy?", opi_fn("lazy?", lazy_p, 1));
  opi_builder_def_const(bldr, "svector?", opi_fn("svector?", svector_p, 1));
  opi_builder_def_const(bldr, "dvector?", opi_fn("dvector?", dvector_p, 1));
  opi_builder_def_const(bldr, "FILE?", opi_fn("FILE?", FILE_p, 1));
  opi_builder_def_const(bldr, "table?", opi_fn("table?", table_p, 1));

  opi_builder_def_const(bldr, "++", opi_fn("++", concat, 2));
  opi_builder_def_const(bldr, "car", opi_fn("car", car_, 1));
  opi_builder_def_const(bldr, "cdr", opi_fn("cdr", cdr_, 1));

  opi_builder_def_const(bldr, "list", opi_fn("list", list, -1));
  opi_builder_def_const(bldr, "table", opi_fn("table", table, 1));
  opi_builder_def_const(bldr, "dvector", opi_fn("dvector", dvector, 1));
  opi_builder_def_const(bldr, "svector", opi_fn("svector", svector, 1));
  opi_builder_def_const(bldr, "number", opi_fn("number", number, 1));

  opi_builder_def_const(bldr, "regex", opi_fn("regex", regex, 1));

  opi_builder_def_const(bldr, "#", opi_fn("#", table_ref, 2));

  opi_builder_def_const(bldr, "pairs", opi_fn("pairs", pairs, 1));
  opi_builder_def_const(bldr, "is", opi_fn("is", is_, 2));
  opi_builder_def_const(bldr, "eq", opi_fn("eq", eq_, 2));
  opi_builder_def_const(bldr, "equal", opi_fn("equal", equal_, 2));
  opi_builder_def_const(bldr, "not", opi_fn("not", not_, 1));
  opi_builder_def_const(bldr, "apply", opi_fn("apply", apply, 2));
  opi_builder_def_const(bldr, "vaarg", opi_fn("vaarg", vaarg, 2));

  opi_builder_def_const(bldr, "newline", opi_fn("newline", newline_, -1));
  opi_builder_def_const(bldr, "print", opi_fn("print", print, -1));
  opi_builder_def_const(bldr, "printf", opi_fn("printf", printf_, -2));
  opi_builder_def_const(bldr, "fprintf", opi_fn("fprintf", fprintf_, -3));
  opi_builder_def_const(bldr, "format", opi_fn("format", format, -2));

  opi_builder_def_const(bldr, "()", opi_fn("()", undefined_, 0));
  opi_builder_def_const(bldr, "error", opi_fn("error", error_, 1));
  opi_builder_def_const(bldr, "die", opi_fn("die", die, 1));
  opi_builder_def_const(bldr, "id", opi_fn("id", id, 1));

  opi_builder_def_const(bldr, "lazy", opi_fn("lazy", lazy, 1));
  opi_builder_def_const(bldr, "force", opi_fn("force", force, 1));

  opi_builder_def_const(bldr, "system", opi_fn("system", system_, 1));
  opi_builder_def_const(bldr, "shell", opi_fn("shell", shell, 1));

  opi_t loadfile_fn = opi_fn("loadfile", loadfile, -2);
  opi_fn_set_data(loadfile_fn, bldr->ctx, NULL);
  opi_builder_def_const(bldr, "loadfile", loadfile_fn);

  opi_builder_def_const(bldr, "exit", opi_fn("exit", exit_, 1));

  opi_builder_def_const(bldr, "__builtin_sr", opi_fn("search_replace", search_replace, 4));

  opi_builder_def_const(bldr, "next", opi_fn("next", next, 1));
}
