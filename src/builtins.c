#include "opium/opium.h"

#include <math.h>

#define BINOP(name, op)                                                               \
  static opi_t                                                                        \
  name(void)                                                                          \
  {                                                                                   \
    opi_t lhs = opi_pop();                                                            \
    opi_t rhs = opi_pop();                                                            \
    if (opi_unlikely(lhs->type != opi_number_type || rhs->type != opi_number_type)) { \
      opi_drop(lhs);                                                                  \
      opi_drop(rhs);                                                                  \
      return opi_undefined(opi_symbol("type_error"));                                 \
    }                                                                                 \
    opi_t ret = opi_number(opi_number_get_value(lhs) op opi_number_get_value(rhs));   \
    opi_drop(lhs);                                                                    \
    opi_drop(rhs);                                                                    \
    return ret;                                                                       \
  }

#define BINOP_CMP(name, op)                                                           \
  static opi_t                                                                        \
  name(void)                                                                          \
  {                                                                                   \
    opi_t lhs = opi_pop();                                                            \
    opi_t rhs = opi_pop();                                                            \
    if (opi_unlikely(lhs->type != opi_number_type || rhs->type != opi_number_type)) { \
      opi_drop(lhs);                                                                  \
      opi_drop(rhs);                                                                  \
      return opi_undefined(opi_symbol("type_error"));                                 \
    }                                                                                 \
    int tmp = opi_number_get_value(lhs) op opi_number_get_value(rhs);                 \
    opi_drop(lhs);                                                                    \
    opi_drop(rhs);                                                                    \
    return tmp ? opi_true : opi_false;                                                \
  }

BINOP(add, +)
BINOP(sub, -)
BINOP(mul, *)
BINOP(div_, /)

BINOP_CMP(lt, <)
BINOP_CMP(gt, >)
BINOP_CMP(le, <=)
BINOP_CMP(ge, >=)
BINOP_CMP(eq, ==)
BINOP_CMP(ne, !=)

static opi_t
mod_(void)
{
  opi_t lhs = opi_pop();
  opi_t rhs = opi_pop();
  if (opi_unlikely(lhs->type != opi_number_type || rhs->type != opi_number_type)) {
    opi_drop(lhs);
    opi_drop(rhs);
    return opi_undefined(opi_symbol("type_error"));
  }
  opi_t ret = opi_number(fmodl(opi_number_get_value(lhs), opi_number_get_value(rhs)));
  opi_drop(lhs);
  opi_drop(rhs);
  return ret;
}

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
cons_(void)
{
  opi_t car = opi_pop();
  opi_t cdr = opi_pop();
  return opi_cons(car, cdr);
}

static opi_t
car_(void)
{
  opi_t x = opi_pop();
  if (opi_unlikely(x->type != opi_pair_type)) {
    opi_drop(x);
    return opi_undefined(opi_symbol("type_error"));
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
    return opi_undefined(opi_symbol("type_error"));
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

static
opi_t g_generic_write, g_generic_display;

static opi_t
default_write(void)
{
  opi_t x = opi_pop();
  opi_t p = opi_pop();
  if (opi_unlikely(!opi_is_port(p))) {
    opi_drop(x);
    opi_drop(p);
    return opi_undefined(opi_symbol("type_error"));
  }
  if (opi_unlikely(p->type != opi_oport_type)) {
    opi_drop(x);
    opi_drop(p);
    return opi_undefined(opi_symbol("bad_port"));
  }
  opi_write(x, opi_port_get_filestream(p));
  opi_drop(x);
  opi_drop(p);
  return opi_nil;
}

static opi_t
write_wrap(void)
{
  opi_t x = opi_get(1);
  if (opi_nargs == 2) {
    opi_t p = opi_get(2);
    if (opi_unlikely(!opi_is_port(p))) {
      opi_drop(opi_pop());
      opi_drop(opi_pop());
      return opi_undefined(opi_symbol("type_error"));
    }
    if (opi_unlikely(p->type != opi_oport_type)) {
      opi_drop(opi_pop());
      opi_drop(opi_pop());
      return opi_undefined(opi_symbol("bad_port"));
    }
  } else {
    // insert port before object
    opi_pop();
    opi_push(opi_stdout);
    opi_push(x);
  }
  return opi_fn_apply(g_generic_write, 2);
}

static opi_t
default_display(void)
{
  opi_t x = opi_pop();
  opi_t p = opi_pop();
  if (opi_unlikely(!opi_is_port(p))) {
    opi_drop(x);
    opi_drop(p);
    return opi_undefined(opi_symbol("type_error"));
  }
  if (opi_unlikely(p->type != opi_oport_type)) {
    opi_drop(x);
    opi_drop(p);
    return opi_undefined(opi_symbol("bad_port"));
  }
  opi_display(x, opi_port_get_filestream(p));
  opi_drop(x);
  opi_drop(p);
  return opi_nil;
}

static opi_t
display_wrap(void)
{
  opi_t x = opi_get(1);
  if (opi_nargs == 2) {
    opi_t p = opi_get(2);
    if (opi_unlikely(!opi_is_port(p))) {
      opi_drop(opi_pop());
      opi_drop(opi_pop());
      return opi_undefined(opi_symbol("type_error"));
    }
    if (opi_unlikely(p->type != opi_oport_type)) {
      opi_drop(opi_pop());
      opi_drop(opi_pop());
      return opi_undefined(opi_symbol("bad_port"));
    }
  } else {
    // insert port before object
    opi_pop();
    opi_push(opi_stdout);
    opi_push(x);
  }
  return opi_fn_apply(g_generic_display, 2);
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
    opi_push(opi_stdout);
    opi_push(x);
    opi_drop(opi_fn_apply(g_generic_display, 2));
    isfirst = FALSE;
  }
  putc('\n', stdout);
  return opi_nil;
}

static opi_t
format_aux(const char *fmt, size_t offs, opi_t port, size_t nargs)
{
  FILE *fs = opi_port_get_filestream(port);
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
            return opi_undefined(opi_symbol("out_of_range"));
          x = opi_get(offs + iarg++);
          opi_push(port);
          opi_push(x);
          x = opi_fn_apply(g_generic_write, 2);
          if (opi_unlikely(x->type == opi_undefined_type))
            return x;
          else
            opi_drop(x);
          break;

        case 'd':
          if (opi_unlikely(iarg > nargs))
            return opi_undefined(opi_symbol("out_of_range"));
          x = opi_get(offs + iarg++);
          opi_push(port);
          opi_push(x);
          x = opi_fn_apply(g_generic_display, 2);
          if (opi_unlikely(x->type == opi_undefined_type))
            return x;
          else
            opi_drop(x);
          break;

        default:
          return opi_undefined(opi_symbol("format_error"));
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
  if (opi_unlikely(!opi_is_port(port))) {
    err = opi_undefined(opi_symbol("type_error"));
    goto error;
  }
  if (opi_unlikely(port->type != opi_oport_type)) {
    err = opi_undefined(opi_symbol("bad_port"));
    goto error;
  }
  if (opi_unlikely(fmt->type != opi_string_type)) {
    err = opi_undefined(opi_symbol("type_error"));
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
  opi_t port = opi_oport(fs, fclose);
  opi_inc_rc(port);

  opi_t fmt = opi_get(1);
  if (opi_unlikely(fmt->type != opi_string_type)) {
    err = opi_undefined(opi_symbol("type_error"));
    goto error;
  }

  if ((err = format_aux(opi_string_get_value(fmt), 2, port, nargs - 1)))
    goto error;

  opi_unref(port);
  err = opi_string(ptr);
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
  putc('\n', opi_port_get_filestream(p));
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
length(void)
{
  size_t len = opi_length(opi_get(1));
  opi_drop(opi_pop());
  return opi_number(len);
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
  opi_t tab = opi_table();

  opi_t err;
  for (opi_t it = l; it->type == opi_pair_type; it = opi_cdr(it)) {
    opi_t kv = opi_car(it);
    if (opi_unlikely(kv->type != opi_pair_type)) {
      opi_drop(l);
      opi_drop(tab);
      return opi_undefined(opi_symbol("type_error"));
    }
    err = NULL;
    opi_table_insert(tab, opi_car(kv), opi_cdr(kv), TRUE, &err);
    if (opi_unlikely(err)) {
      opi_drop(l);
      opi_drop(tab);
      return err;
    }
  }
  opi_drop(l);
  return tab;
}

static opi_t
default_at(void)
{
  opi_t l = opi_pop();
  opi_t idx = opi_pop();

  if (opi_unlikely(idx->type != opi_number_type)) {
    opi_drop(l);
    opi_drop(idx);
    return opi_undefined(opi_symbol("type_error"));
  }

  size_t k = opi_number_get_value(idx);
  if (opi_unlikely(k >= opi_length(l))) {
    opi_drop(l);
    opi_drop(idx);
    return opi_undefined(opi_symbol("out_of_range"));
  }

  while (k--)
    l = opi_cdr(l);

  opi_t ret = opi_car(l);
  opi_inc_rc(ret);
  opi_drop(l);
  opi_drop(idx);
  opi_dec_rc(ret);
  return ret;
}

static opi_t
table_at(void)
{
  opi_t tab = opi_pop();
  opi_t key = opi_pop();

  opi_t err = NULL;
  opi_t x = opi_table_at(tab, key, &err);
  opi_t ret = x ? x : err;

  opi_inc_rc(ret);
  opi_drop(tab);
  opi_drop(key);
  opi_dec_rc(ret);
  return ret;
}

static opi_t
string_at(void)
{
  opi_t str = opi_pop();
  opi_t idx = opi_pop();

  if (opi_unlikely(idx->type != opi_number_type)) {
    opi_drop(str);
    opi_drop(idx);
    return opi_undefined(opi_symbol("type_error"));
  }

  size_t k = opi_number_get_value(idx);
  if (opi_unlikely(k >= opi_string_get_length(str))) {
    opi_drop(str);
    opi_drop(idx);
    return opi_undefined(opi_symbol("out_of_range"));
  }

  opi_t ret = opi_string_from_char(opi_string_get_value(str)[k]);
  opi_drop(str);
  opi_drop(idx);
  return ret;
}

struct compose_data { opi_t f, g; };

static void
compose_delete(struct opi_fn *fn)
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
  if (!opi_test_arity(opi_fn_get_arity(data->g), opi_nargs)) {
    for (size_t i = 1; i <= opi_nargs; ++i)
      opi_drop(opi_pop());
    return opi_undefined(opi_symbol("arity_error"));
  }
  opi_t tmp = opi_fn_apply(data->g, opi_nargs);
  opi_push(tmp);
  return opi_fn_apply(data->f, 1);
}

static opi_t
compose(void)
{
  opi_t f = opi_pop();
  opi_t g = opi_pop();
  struct compose_data *data = malloc(sizeof(struct compose_data));
  opi_inc_rc(data->f = f);
  opi_inc_rc(data->g = g);
  opi_t aux = opi_fn("composition", compose_aux, opi_fn_get_arity(g));
  opi_fn_set_data(aux, data, compose_delete);
  return aux;
}

static opi_t
apply(void)
{
  opi_t f = opi_pop();
  opi_t l = opi_pop();

  if (f->type != opi_fn_type) {
    opi_drop(f);
    opi_drop(l);
    return opi_undefined(opi_symbol("type_error"));
  }

  size_t nargs = opi_length(l);
  if (!opi_test_arity(opi_fn_get_arity(f), nargs)) {
    opi_drop(f);
    opi_drop(l);
    return opi_undefined(opi_symbol("arity_error"));
  }

  opi_sp += nargs;
  size_t iarg = nargs;
  for (opi_t it = l; it->type == opi_pair_type; it = opi_cdr(it))
    opi_sp[-iarg--] = opi_car(it);
  opi_t ret = opi_fn_apply(f, nargs);

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
    return opi_undefined(opi_symbol("type_error"));
  }
  if (opi_unlikely(!opi_test_arity(opi_fn_get_arity(x), 0))) {
    opi_drop(x);
    return opi_undefined(opi_symbol("arity_error"));
  }
  return opi_lazy(x);
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
default_next(void)
{
  opi_drop(opi_pop());
  return opi_undefined(opi_symbol("unimplemented_trait"));
}

void
opi_builtins(struct opi_builder *bldr)
{
  opi_builder_def_const(bldr, "+", opi_fn("+", add, 2));
  opi_builder_def_const(bldr, "-", opi_fn("-", sub, 2));
  opi_builder_def_const(bldr, "*", opi_fn("*", mul, 2));
  opi_builder_def_const(bldr, "/", opi_fn("/", div_, 2));
  opi_builder_def_const(bldr, "%", opi_fn("%", mod_, 2));
  opi_builder_def_const(bldr, "<", opi_fn("<", lt, 2));
  opi_builder_def_const(bldr, ">", opi_fn(">", gt, 2));
  opi_builder_def_const(bldr, "<=", opi_fn("<=", le, 2));
  opi_builder_def_const(bldr, ">=", opi_fn(">=", ge, 2));
  opi_builder_def_const(bldr, "==", opi_fn("==", eq, 2));
  opi_builder_def_const(bldr, "/=", opi_fn("/=", ne, 2));
  opi_builder_def_const(bldr, ":", opi_fn(":", cons_, 2));
  opi_builder_def_const(bldr, ".", opi_fn(".", compose, 2));
  opi_builder_def_const(bldr, "car", opi_fn("car", car_, 1));
  opi_builder_def_const(bldr, "cdr", opi_fn("cdr", cdr_, 1));
  opi_builder_def_const(bldr, "list", opi_fn("list", list, -1));
  opi_builder_def_const(bldr, "table", opi_fn("table", table, 1));
  opi_builder_def_const(bldr, "null?", opi_fn("null?", null_, 1));
  opi_builder_def_const(bldr, "is", opi_fn("is", is_, 2));
  opi_builder_def_const(bldr, "eq", opi_fn("eq", eq_, 2));
  opi_builder_def_const(bldr, "equal", opi_fn("equal", equal_, 2));
  opi_builder_def_const(bldr, "not", opi_fn("not", not_, 1));
  opi_builder_def_const(bldr, "apply", opi_fn("apply", apply, 2));

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
  opi_builder_def_const(bldr, "!", opi_fn("!", flush_lazy, 1));

  struct opi_trait *write_trait = opi_trait(opi_fn("default_write", default_write, 2));
  g_generic_write = opi_trait_into_generic(write_trait, "write");
  opi_builder_add_trait(bldr, "write", write_trait);
  opi_builder_def_const(bldr, "write", g_generic_write);
  opi_builder_def_const(bldr, "write", opi_fn("write_wrap", write_wrap, -2));

  struct opi_trait *display_trait = opi_trait(opi_fn("default_display", default_display, 2));
  g_generic_display = opi_trait_into_generic(display_trait, "display");
  opi_builder_add_trait(bldr, "display", display_trait);
  opi_builder_def_const(bldr, "display", g_generic_display);
  opi_builder_def_const(bldr, "display", opi_fn("display_wrap", display_wrap, -2));

  struct opi_trait *length_trait = opi_trait(opi_fn("length", length, 1));
  opi_t length_generic = opi_trait_into_generic(length_trait, "length");
  opi_builder_add_trait(bldr, "length", length_trait);
  opi_builder_def_const(bldr, "length", length_generic);

  struct opi_trait *at_trait = opi_trait(opi_fn("!!", default_at, 2));
  opi_trait_impl(at_trait, opi_table_type, opi_fn("table_at", table_at, 2));
  opi_trait_impl(at_trait, opi_string_type, opi_fn("string_at", string_at, 2));
  opi_t at_generic = opi_trait_into_generic(at_trait, "!!");
  opi_builder_add_trait(bldr, "!!", at_trait);
  opi_builder_def_const(bldr, "!!", at_generic);

  struct opi_trait *next_trait = opi_trait(opi_fn("next", default_next, 1));
  opi_trait_impl(next_trait, opi_nil_type, opi_fn("nil_next", id, 1));
  opi_trait_impl(next_trait, opi_pair_type, opi_fn("pair_next", id, 1));
  opi_trait_impl(next_trait, opi_lazy_type, opi_fn("lazy_next", flush_lazy, 1));
  opi_t next_generic = opi_trait_into_generic(next_trait, "next");
  opi_builder_add_trait(bldr, "next", next_trait);
  opi_builder_def_const(bldr, "next", next_generic);
}
