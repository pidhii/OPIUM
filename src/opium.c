#include "opium/opium.h"
#include "opium/hash-map.h"

#include <string.h>
#include <math.h>
#include <float.h>
#include <stdarg.h>

opi_t opi_current_fn = NULL;
opi_t *opi_sp = NULL;
size_t opi_nargs;

__attribute__((noreturn)) void
opi_die(const char *fmt, ...)
{
  fprintf(OPI_ERROR, "[\x1b[38;5;9;7m opium \x1b[0m] \x1b[38;5;1;1mdie\x1b[0m "); \
  va_list arg;
  va_start(arg, fmt);
  vfprintf(stderr, fmt, arg);
  va_end(arg);
  putc('\n', stderr);
  exit(EXIT_FAILURE);
}

extern int
yylex_destroy(void);

void
opi_init(void)
{
  opi_allocators_init();

  opi_undefined_init();
  opi_nil_init();
  opi_number_init();
  opi_fn_init();
  opi_string_init();
  opi_boolean_init();
  opi_pair_init();
  opi_symbol_init();
  opi_file_init();
  opi_lazy_init();
  opi_table_init();
}

void
opi_cleanup(void)
{
  opi_file_cleanup();
  opi_number_cleanup();
  opi_symbol_cleanup();
  opi_undefined_cleanup();
  opi_nil_cleanup();
  opi_string_cleanup();
  opi_boolean_cleanup();
  opi_pair_cleanup();
  opi_fn_cleanup();
  opi_lazy_cleanup();
  opi_table_cleanup();

  opi_allocators_cleanup();
}

/******************************************************************************/
opi_type_t opi_type_type = NULL;

struct opi_type {
  char name[OPI_TYPE_NAME_MAX + 1];

  void (*delete_cell)(opi_type_t ty, opi_t cell);

  void *data;
  void (*delete_data)(opi_type_t ty);

  void (*display)(opi_type_t ty, opi_t x, FILE *out);
  void (*write)(opi_type_t ty, opi_t x, FILE *out);
  int (*eq)(opi_type_t ty, opi_t x, opi_t y);
  int (*equal)(opi_type_t ty, opi_t x, opi_t y);
  size_t (*hash)(opi_type_t ty, opi_t x);

  size_t fields_offset;
  size_t nfields;
  char **fields;
};

static void
default_destroy_cell(opi_type_t ty, opi_t cell)
{ }

static void
default_destroy_data(opi_type_t ty)
{ }

static void
default_display(opi_type_t ty, opi_t x, FILE *out)
{ ty->write(ty, x, out); }

static void
default_write(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, "<%s>", opi_type_get_name(ty)); }

static int
default_eq(opi_type_t ty, opi_t x, opi_t y)
{ return opi_is(x, y); }

static int
default_equal(opi_type_t ty, opi_t x, opi_t y)
{ return ty->eq(ty, x, y); }

static opi_type_t
new_type(const char *name)
{
  struct opi_type *ty = malloc(sizeof(struct opi_type));
  opi_assert(strlen(name) <= OPI_TYPE_NAME_MAX);
  strcpy(ty->name, name);
  ty->delete_cell = default_destroy_cell;
  ty->data = NULL;
  ty->delete_data = default_destroy_data;
  ty->display = default_display;
  ty->write = default_write;
  ty->eq = default_eq;
  ty->equal = default_equal;
  ty->hash = NULL;
  ty->fields = NULL;
  return ty;
}

opi_type_t
opi_type(const char *name)
{
  opi_type_t type = new_type(name);
  return type;
}

void
opi_type_delete(opi_type_t ty)
{
  if (ty->delete_data)
    ty->delete_data(ty);

  if (ty->fields) {
    for (size_t i = 0; i < ty->nfields; ++i)
      free(ty->fields[i]);
    free(ty->fields);
  }

  free(ty);
}

void
opi_type_set_delete_cell(opi_type_t ty, void (*fn)(opi_type_t,opi_t))
{ ty->delete_cell = fn; }

void
opi_type_set_data(opi_type_t ty, void *data, void (*fn)(opi_type_t))
{
  ty->data = data;
  ty->delete_data = fn;
}

void
opi_type_set_display(opi_type_t ty, void (*fn)(opi_type_t,opi_t,FILE*))
{ ty->display = fn; }

void
opi_type_set_write(opi_type_t ty, void (*fn)(opi_type_t,opi_t,FILE*))
{ ty->write = fn; }

void
opi_type_set_eq(opi_type_t ty, int (*fn)(opi_type_t,opi_t,opi_t))
{
  ty->eq = fn;
  if (ty->equal == default_equal)
    ty->equal = fn;
}

void
opi_type_set_equal(opi_type_t ty, int (*fn)(opi_type_t,opi_t,opi_t))
{ ty->equal = fn; }

void
opi_type_set_hash(opi_type_t ty, size_t (*fn)(opi_type_t,opi_t))
{ ty->hash = fn; }

int
opi_type_is_hashable(opi_type_t ty)
{ return !!ty->hash; }

void
opi_type_set_fields(opi_type_t ty, size_t offs, char **fields, size_t n)
{
  ty->fields_offset = offs;
  ty->nfields = n;
  ty->fields = malloc(sizeof(char*) * n);
  for (size_t i = 0; i < n; ++i)
    ty->fields[i] = strdup(fields[i]);
}

int
opi_type_get_field_idx(opi_type_t ty, const char *field)
{
  opi_assert(ty->fields);
  for (size_t i = 0; i < ty->nfields; ++i) {
    if (strcmp(ty->fields[i], field) == 0)
      return i;
  }
  return -1;
}

size_t
opi_type_get_field_offset(opi_type_t ty, size_t field_idx)
{ return ty->fields_offset + sizeof(opi_t) * field_idx; }

const char*
opi_type_get_name(opi_type_t ty)
{ return ty->name; }

void*
opi_type_get_data(opi_type_t ty)
{ return ty->data; }

void
opi_display(opi_t x, FILE *out)
{ x->type->display(x->type, x, out); }

void
opi_write(opi_t x, FILE *out)
{ x->type->write(x->type, x, out); }

int
opi_eq(opi_t x, opi_t y)
{ return x->type == y->type && x->type->eq(x->type, x, y); }

int
opi_equal(opi_t x, opi_t y)
{ return x->type == y->type && x->type->equal(x->type, x, y); }

void
opi_delete(opi_t x)
{ x->type->delete_cell(x->type, x); }

size_t
opi_hashof(opi_t x)
{ return x->type->hash(x->type, x); }

/******************************************************************************/
opi_type_t opi_number_type;

static void
number_write(opi_type_t ty, opi_t x, FILE *out)
{
  long double val = opi_as(x, struct opi_number).val;
  fprintf(out, "%Lf", val);
}

static void
number_display(opi_type_t ty, opi_t x, FILE *out)
{
  long double val = opi_as(x, struct opi_number).val;
  long double i;
  long double f = modfl(val, &i);
  if (f == 0)
    fprintf(out, "%.0Lf", i);
  else
    fprintf(out, "%Lg", val);
}

static int
number_eq(opi_type_t ty, opi_t x, opi_t y)
{ return opi_number_get_value(x) == opi_number_get_value(y); }

static void
number_delete(opi_type_t ty, opi_t cell)
{ opi_free(cell); }

static size_t
number_hash(opi_type_t type, opi_t x)
{ return opi_number_get_value(x); }

void
opi_number_init(void)
{
  opi_number_type = opi_type("number");
  opi_type_set_write(opi_number_type, number_write);
  opi_type_set_display(opi_number_type, number_display);
  opi_type_set_delete_cell(opi_number_type, number_delete);
  opi_type_set_eq(opi_number_type, number_eq);
  opi_type_set_hash(opi_number_type, number_hash);
}

void
opi_number_cleanup(void)
{ opi_type_delete(opi_number_type); }

/******************************************************************************/
struct symbol {
  struct opi_header header;
  char *str;
  uint64_t hash;
};

opi_type_t opi_symbol_type;

static
struct opi_hash_map g_sym_map;

static void
symbol_write(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, "'%s", opi_symbol_get_string(x)); }

static void
symbol_display(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, "%s", opi_symbol_get_string(x)); }

static size_t
symbol_hash(opi_type_t ty, opi_t x)
{ return opi_as(x, struct symbol).hash; }

static int
symbol_eq(opi_type_t ty, opi_t x, opi_t y)
{
  char *s1 = opi_as(x, struct symbol).str;
  char *s2 = opi_as(y, struct symbol).str;
  return strcmp(s1, s2) == 0;
}

static void
symbol_delete(opi_type_t ty, opi_t x)
{
  struct symbol *sym = opi_as_ptr(x);
  free(sym->str);
  free(sym);
}

void
opi_symbol_init(void)
{
  opi_symbol_type = opi_type("symbol");
  opi_type_set_write(opi_symbol_type, symbol_write);
  opi_type_set_display(opi_symbol_type, symbol_display);
  opi_type_set_hash(opi_symbol_type, symbol_hash);
  opi_type_set_eq(opi_symbol_type, symbol_eq);
  opi_type_set_delete_cell(opi_symbol_type, symbol_delete);

  opi_hash_map_init(&g_sym_map);
}

void
opi_symbol_cleanup(void)
{
  opi_hash_map_destroy(&g_sym_map);
  opi_type_delete(opi_symbol_type);
}

opi_t
opi_symbol(const char *str)
{
  struct opi_hash_map_elt elt;
  uint64_t hash = opi_hash(str, strlen(str));

  struct symbol sym;
  sym.str = (void*)str;
  opi_init_cell(&sym, opi_symbol_type);

  if (opi_hash_map_find(&g_sym_map, (opi_t)&sym, hash, &elt)) {
    return elt.val;
  } else {
    // Create new symbol:
    struct symbol *sym = malloc(sizeof(struct symbol));
    sym->str = strdup(str);
    sym->hash = hash;
    opi_init_cell(sym, opi_symbol_type);
    // insert it into global hash-table
    opi_hash_map_insert(&g_sym_map, (opi_t)sym, hash, (opi_t)sym, &elt);
    return (opi_t)sym;
  }
}

const char*
opi_symbol_get_string(opi_t x)
{ return opi_as(x, struct symbol).str; }

/******************************************************************************/
opi_type_t opi_undefined_type;

static void
undefined_delete(opi_type_t ty, opi_t cell)
{
  struct opi_undefined *undef = opi_as_ptr(cell);
  opi_unref(undef->what);
  opi_free(cell);
}

static void
undefined_write(opi_type_t type, opi_t x, FILE *out)
{
  fprintf(out, "undefined { ");
  opi_write(opi_undefined_get_what(x), out);
  fprintf(out, " }");
}

static void
undefined_display(opi_type_t type, opi_t x, FILE *out)
{
  fprintf(out, "undefined { ");
  opi_display(opi_undefined_get_what(x), out);
  fprintf(out, " }");
}

void
opi_undefined_init(void)
{
  opi_undefined_type = opi_type("undefined");
  opi_type_set_delete_cell(opi_undefined_type, undefined_delete);
  char *fields[] = { "what" };
  opi_type_set_fields(opi_undefined_type, offsetof(struct opi_undefined, what), fields, 1);
  opi_type_set_write(opi_undefined_type, undefined_write);
  opi_type_set_display(opi_undefined_type, undefined_display);
}

void
opi_undefined_cleanup(void)
{ opi_type_delete(opi_undefined_type); }

opi_t
opi_undefined(opi_t what)
{
  struct opi_undefined *undef = opi_allocate();
  opi_inc_rc(undef->what = what);
  opi_init_cell(undef, opi_undefined_type);
  return (opi_t)undef;
}

/******************************************************************************/
opi_type_t opi_null_type;

struct opi_header g_nil;
opi_t opi_nil;

static void
nil_write(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, "nil"); }

void
opi_nil_init(void)
{
  opi_null_type = opi_type("null");
  opi_type_set_write(opi_null_type, nil_write);

  opi_nil = &g_nil;
  opi_init_cell(opi_nil, opi_null_type);
  opi_inc_rc(opi_nil);
}

void
opi_nil_cleanup(void)
{
  opi_unref(opi_nil);
  opi_type_delete(opi_null_type);
}

/******************************************************************************/
opi_type_t opi_string_type;

static void
string_write(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, "\"%s\"", opi_as(x, struct opi_string).str); }

static void
string_display(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, "%s", opi_as(x, struct opi_string).str); }

static void
string_delete(opi_type_t ty, opi_t x)
{
  struct opi_string *s = opi_as_ptr(x);
  free(s->str);
  opi_free(s);
}

static int
string_eq(opi_type_t ty, opi_t x, opi_t y)
{
  size_t l1 = opi_string_get_length(x);
  size_t l2 = opi_string_get_length(y);
  const char *s1 = opi_string_get_value(x);
  const char *s2 = opi_string_get_value(y);
  return l1 == l2 && strcmp(s1, s2) == 0;
}

static size_t
string_hash(opi_type_t ty, opi_t x)
{
  size_t len = opi_string_get_length(x);
  const char *str = opi_string_get_value(x);
  return opi_hash(str, len);
}

void
opi_string_init(void)
{
  opi_string_type = opi_type("string");
  opi_type_set_display(opi_string_type, string_display);
  opi_type_set_write(opi_string_type, string_write);
  opi_type_set_delete_cell(opi_string_type, string_delete);
  opi_type_set_eq(opi_string_type, string_eq);
  opi_type_set_hash(opi_string_type, string_hash);
}

void
opi_string_cleanup(void)
{ opi_type_delete(opi_string_type); }

extern inline opi_t
opi_string_move2(char *str, size_t len)
{
  struct opi_string *s = opi_allocate();
  opi_init_cell(s, opi_string_type);
  s->str = str;
  s->size = len;
  return (opi_t)s;
}

extern inline opi_t
opi_string_move(char *str)
{ return opi_string_move2(str, strlen(str)); }

opi_t
opi_string2(const char *str, size_t len)
{
  char *mystr = malloc(len + 1);
  memcpy(mystr, str, len);
  mystr[len] = 0;
  return opi_string_move2(mystr, len);
}

opi_t
opi_string(const char *str)
{ return opi_string_move(strdup(str)); }

opi_t
opi_string_from_char(char c)
{
  struct opi_string *s = opi_allocate();
  opi_init_cell(s, opi_string_type);
  s->str = malloc(2);;
  s->str[0] = c;
  s->str[1] = 0;
  s->size = 1;
  return (opi_t)s;
}

const char*
opi_string_get_value(opi_t x)
{ return opi_as(x, struct opi_string).str; }

size_t
opi_string_get_length(opi_t x)
{ return opi_as(x, struct opi_string).size; }

/******************************************************************************/
static
struct opi_header g_true, g_false;
opi_type_t opi_boolean_type;
opi_t opi_true, opi_false;

static void
boolean_write(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, x == opi_true ? "true" : "false"); }

void
opi_boolean_init(void)
{
  opi_boolean_type = opi_type("boolean");
  opi_type_set_write(opi_boolean_type, boolean_write);

  opi_true = &g_true;
  opi_false = &g_false;
  opi_init_cell(opi_true, opi_boolean_type);
  opi_init_cell(opi_false, opi_boolean_type);
  opi_inc_rc(opi_true);
  opi_inc_rc(opi_false);
}

void
opi_boolean_cleanup(void)
{
  opi_unref(opi_true);
  opi_unref(opi_false);
  opi_type_delete(opi_boolean_type);
}


/******************************************************************************/
opi_type_t opi_pair_type;

static void
pair_display(opi_type_t ty, opi_t x, FILE *out)
{
  while (x->type == opi_pair_type) {
    opi_display(opi_car(x), out);
    putc(':', out);
    x = opi_cdr(x);
  }
  opi_display(x, out);
}

static void
pair_write(opi_type_t ty, opi_t x, FILE *out)
{
  while (x->type == opi_pair_type) {
    opi_write(opi_car(x), out);
    putc(':', out);
    x = opi_cdr(x);
  }
  opi_write(x, out);
}

static void
pair_delete(opi_type_t ty, opi_t x) {
  while (x->type == opi_pair_type) {
    opi_t tmp = opi_cdr(x);
    opi_unref(opi_car(x));
    opi_free(x);
    if (opi_dec_rc(x = tmp) > 0)
      return;
  }
  opi_drop(x);
}

void
opi_pair_init(void)
{
  opi_pair_type = opi_type("pair");
  opi_type_set_display(opi_pair_type, pair_display);
  opi_type_set_write(opi_pair_type, pair_write);
  opi_type_set_delete_cell(opi_pair_type, pair_delete);
  char *fields[] = { "car", "cdr" };
  opi_type_set_fields(opi_pair_type, offsetof(struct opi_pair, car), fields, 2);
}

void
opi_pair_cleanup(void)
{ opi_type_delete(opi_pair_type); }

/******************************************************************************/
struct table {
  struct opi_header header;
  struct opi_hash_map map;
};

opi_type_t opi_table_type;

static void
table_delete(opi_type_t ty, opi_t x)
{
  opi_hash_map_destroy(&opi_as(x, struct table).map);
  opi_free(x);
}

void
opi_table_init(void)
{
  opi_table_type = opi_type("table");
  opi_type_set_delete_cell(opi_table_type, table_delete);
}

void
opi_table_cleanup(void)
{ opi_type_delete(opi_table_type); }

opi_t
opi_table(void)
{
  struct table *tab = opi_allocate();
  opi_hash_map_init(&tab->map);
  opi_init_cell(tab, opi_table_type);
  return (opi_t)tab;
}

opi_t
opi_table_at(opi_t tab, opi_t key, opi_t *err)
{
  struct table *t = opi_as_ptr(tab);

  if (opi_unlikely(!opi_type_is_hashable(key->type))) {
    if (err)
      *err = opi_undefined(opi_symbol("hash_error"));
    return NULL;
  }

  size_t hash = opi_hashof(key);
  struct opi_hash_map_elt elt;
  if (opi_hash_map_find(&t->map, key, hash, &elt)) {
    return elt.val;
  } else {
    if (err)
      *err = opi_undefined(opi_symbol("out_of_range"));
    return NULL;
  }
}

int
opi_table_insert(opi_t tab, opi_t key, opi_t val, int replace, opi_t *err)
{
  struct table *t = opi_as_ptr(tab);

  if (opi_unlikely(!opi_type_is_hashable(key->type))) {
    if (err)
      *err = opi_undefined(opi_symbol("hash_error"));
    return FALSE;
  }

  size_t hash = opi_hashof(key);
  struct opi_hash_map_elt elt;
  if (opi_hash_map_find(&t->map, key, hash, &elt)) {
    if (replace) {
      opi_hash_map_insert(&t->map, key, hash, val, &elt);
      return TRUE;
    } else {
      if (err)
        *err = NULL;
      return FALSE;
    }
  } else {
    opi_hash_map_insert(&t->map, key, hash, val, &elt);
    return TRUE;
  }
}

/******************************************************************************/
struct file {
  struct opi_header header;
  FILE *fs;
  int (*close)(FILE*);
};

opi_type_t opi_file_type;

opi_t opi_stdin, opi_stdout, opi_stderr;

static void
file_delete(opi_type_t type, opi_t x)
{
  struct file *p = opi_as_ptr(x);
  if (p->close)
    p->close(p->fs);
  opi_free(p);
}

void
opi_file_init(void)
{
  opi_file_type = opi_type("FILE");

  opi_type_set_delete_cell(opi_file_type, file_delete);

  static struct file stdin_, stdout_, stderr_;
  stdin_.fs  = stdin;
  stdout_.fs = stdout;
  stderr_.fs = stderr;

  stdin_.close  = NULL;
  stdout_.close = NULL;
  stderr_.close = NULL;

  opi_stdin = (opi_t)&stdin_;
  opi_stdout = (opi_t)&stdout_;
  opi_stderr = (opi_t)&stderr_;

  opi_init_cell(opi_stdin,  opi_file_type);
  opi_init_cell(opi_stdout, opi_file_type);
  opi_init_cell(opi_stderr, opi_file_type);

  opi_inc_rc(opi_stdin);
  opi_inc_rc(opi_stdout);
  opi_inc_rc(opi_stderr);
}

void
opi_file_cleanup(void)
{
  opi_type_delete(opi_file_type);
}

opi_t
opi_file(FILE *fs, int (*close)(FILE*))
{
  struct file *p = opi_allocate();
  p->fs = fs;
  p->close = close;
  opi_init_cell(p, opi_file_type);
  return (opi_t)p;
}

FILE*
opi_file_get_value(opi_t x)
{ return opi_as(x, struct file).fs; }

/******************************************************************************/
opi_type_t opi_fn_type;

static void
fn_display(opi_type_t type, opi_t cell, FILE *out)
{
  struct opi_fn *fn = opi_as_ptr(cell);
  if (fn->name)
    fprintf(out, "<function %s>", fn->name);
  else
    fprintf(out, "<function>");
}

static void
fn_delete(opi_type_t type, opi_t cell)
{
  struct opi_fn *fn = opi_as_ptr(cell);
  fn->delete(fn);
}

void
opi_fn_delete(struct opi_fn *fn)
{
  if (fn->name)
    free(fn->name);
  free(fn);
}

void
opi_fn_init(void)
{
  opi_fn_type = opi_type("function");
  opi_type_set_display(opi_fn_type, fn_display);
  opi_type_set_delete_cell(opi_fn_type, fn_delete);
}

void
opi_fn_cleanup(void)
{ opi_type_delete(opi_fn_type); }

static opi_t
fn_default_handle(void)
{ return opi_undefined(opi_nil); }

static void
fn_default_delete_data(void *data)
{ }

opi_t
opi_fn_alloc()
{
  struct opi_fn *fn = malloc(sizeof(struct opi_fn));
  opi_init_cell(fn, opi_fn_type);
  fn->handle = fn_default_handle;
  return (opi_t)fn;
}

void
opi_fn_finalize(opi_t cell, const char *name, opi_fn_handle_t f, int arity)
{
  struct opi_fn *fn = opi_as_ptr(cell);
  fn->name = name ? strdup(name) : NULL;
  fn->handle = f;
  fn->data = NULL;
  fn->delete = opi_fn_delete;
  fn->arity = arity;
}

opi_t
opi_fn(const char *name, opi_t (*f)(void), int arity)
{
  opi_t fn = opi_fn_alloc();
  opi_fn_finalize(fn, name, f, arity);
  return (opi_t)fn;
}

void
opi_fn_set_data(opi_t cell, void *data, void (*delete)(struct opi_fn*))
{
  struct opi_fn *fn = opi_as_ptr(cell);
  fn->data = data;
  if (delete)
    fn->delete = delete;
}

struct curry_data {
  opi_t f;
  size_t n;
  opi_t p[];
};

static void
curry_delete(struct opi_fn *fn)
{
  struct curry_data *data = fn->data;
  opi_unref(data->f);
  for (size_t i = 0; i < data->n; ++i)
    opi_unref(data->p[i]);
  free(data);
  opi_fn_delete(fn);
}

static opi_t
curry(void)
{
  struct curry_data *data = opi_fn_get_data(opi_current_fn);
  for (int i = data->n - 1; i >= 0; --i)
    opi_push(data->p[i]);
  return opi_apply(data->f, opi_nargs + data->n);
}

opi_t
opi_apply_partial(opi_t f, int nargs)
{
  int arity = opi_fn_get_arity(f);

  if (arity < 0) {
    opi_assert(!"unimplemented partial application for variadic function");

  } else {
    if (arity < nargs) {
      // Apply part of the arguments and pass the rest to the return value.

      for (int i = arity; i < nargs; ++i)
        opi_inc_rc(opi_sp[-(i + 1)]);
      nargs -= arity;

      opi_t tmp_f = opi_fn_apply(f, arity);
      opi_inc_rc(tmp_f);
      if (opi_unlikely(tmp_f->type != opi_fn_type)) {
        opi_unref(tmp_f);
        while (nargs--)
          opi_unref(opi_pop());
        return opi_undefined(opi_symbol("not-a-function"));
      }

      for (int i = 0; i < nargs; ++i)
        opi_dec_rc(opi_sp[-(i + 1)]);
      opi_t ret = opi_apply(tmp_f, nargs);
      opi_inc_rc(ret);
      opi_unref(tmp_f);
      opi_dec_rc(ret);
      return ret;

    } else {
      // Curry functoin.
      //
      struct curry_data *data =
        malloc(sizeof(struct curry_data) + sizeof(opi_t) * nargs);
      opi_inc_rc(data->f = f);
      data->n = nargs;
      for (int i = 0; i < nargs; ++i)
        opi_inc_rc(data->p[i] = opi_pop());

      opi_t curry_f = opi_fn(opi_fn_get_name(f), curry, arity - nargs);
      opi_fn_set_data(curry_f, data, curry_delete);

      return curry_f;
    }
  }
}

/******************************************************************************/
opi_type_t opi_lazy_type;

static void
lazy_delete(opi_type_t type, opi_t x)
{
  struct opi_lazy *lazy = opi_as_ptr(x);
  opi_unref(lazy->cell);
  opi_free(lazy);
}

void
opi_lazy_init(void)
{
  opi_lazy_type = opi_type("lazy");
  opi_type_set_delete_cell(opi_lazy_type, lazy_delete);
}

void
opi_lazy_cleanup(void)
{ opi_type_delete(opi_lazy_type); }

opi_t
opi_lazy(opi_t x)
{
  struct opi_lazy *lazy = opi_allocate();
  opi_inc_rc(lazy->cell = x);
  lazy->is_ready = FALSE;
  opi_init_cell(lazy, opi_lazy_type);
  return (opi_t)lazy;
}

