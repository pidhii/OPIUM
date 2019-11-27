#include "opium/opium.h"
#include "opium/hash-map.h"

#include <string.h>
#include <math.h>
#include <float.h>
#include <stdarg.h>
#include <errno.h>
#include <pcre.h>

opi_t opi_current_fn = NULL;
opi_t *opi_sp = NULL;
size_t opi_nargs;

int opi_error = 0;
opi_trace_t oip_trace;

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

int
opi_show_location(FILE *out, const char *path, int fc, int fl, int lc, int ll)
{
  FILE *fs = fopen(path, "r");
  if (fs == NULL) {
    opi_error = 1;
    return OPI_ERR;
  }

  int line = 1;
  int col = 1;
  int hl = FALSE;
  while (TRUE) {
    errno = 0;
    int c = fgetc(fs);
    if (errno) {
      opi_error("print location: %s\n", strerror(errno));
      opi_error = 1;
      opi_assert(fclose(fs) == 0);
      return OPI_ERR;
    }
    if (c == EOF) {
      /*opi_error("print location: unexpected end of file\n");*/
      /*opi_error = 1;*/
      opi_assert(fclose(fs) == 0);
      fputs("\e[0m", out);
      return OPI_OK;
    }

    if (line >= fl && line <= ll) {
      if (line == fl && col == fc) {
        hl = TRUE;
        fputs("\e[38;5;9;1m", out);
      }

      if (col == 1) {
        fputs("\e[0m", out);
        fprintf(out, "[\x1b[38;5;9m opium \x1b[0m] %4d: ", line);
        if (hl)
          fputs("\e[38;5;9;1m", out);
      }

      putc(c, out);

      if (line == ll && col == lc - 1)
        fputs("\e[0m", out);
    }

    if (c == '\n') {
      line += 1;
      col = 1;
    } else {
      col += 1;
    }

    if (line > ll)
      break;
  }

  opi_assert(fclose(fs) == 0);
  return OPI_OK;
}

void
opi_init(void)
{
  opi_allocators_init();
  opi_lexer_init();

  opi_undefined_init();
  opi_nil_init();
  opi_num_init();
  opi_fn_init();
  opi_string_init();
  opi_boolean_init();
  opi_pair_init();
  opi_symbol_init();
  opi_file_init();
  opi_lazy_init();
  opi_table_init();
  opi_regex_init();
  opi_vectors_init();
}

void
opi_cleanup(void)
{
  opi_file_cleanup();
  opi_num_cleanup();
  opi_symbol_cleanup();
  opi_undefined_cleanup();
  opi_nil_cleanup();
  opi_string_cleanup();
  opi_boolean_cleanup();
  opi_pair_cleanup();
  opi_fn_cleanup();
  opi_lazy_cleanup();
  opi_table_cleanup();
  opi_regex_cleanup();
  opi_vectors_cleanup();

  opi_lexer_cleanup();
  opi_allocators_cleanup();
}

/******************************************************************************/
opi_type_t opi_type_type = NULL;

struct OpiType_s {
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
  OpiType *ty = malloc(sizeof(OpiType));
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
opi_type_new(const char *name)
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

size_t
opi_type_get_nfields(opi_type_t ty)
{ return ty->nfields; }

char* const*
opi_type_get_fields(opi_type_t ty)
{ return ty->fields; }

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
opi_type_t opi_num_type;

static void
number_write(opi_type_t ty, opi_t x, FILE *out)
{
  long double val = opi_as(x, OpiNum).val;
  long double i;
  long double f = modfl(val, &i);
  if (f == 0)
    fprintf(out, "%.0Lf", i);
  else
    fprintf(out, "%Lf", val);
}

static void
number_display(opi_type_t ty, opi_t x, FILE *out)
{
  long double val = opi_as(x, OpiNum).val;
  long double i;
  long double f = modfl(val, &i);
  if (f == 0)
    fprintf(out, "%.0Lf", i);
  else
    fprintf(out, "%Lg", val);
}

static int
number_eq(opi_type_t ty, opi_t x, opi_t y)
{ return opi_num_get_value(x) == opi_num_get_value(y); }

static void
number_delete(opi_type_t ty, opi_t cell)
{ opi_h2w_free(cell); }

static size_t
number_hash(opi_type_t type, opi_t x)
{ return opi_num_get_value(x); }

void
opi_num_init(void)
{
  opi_num_type = opi_type_new("Num");
  opi_type_set_write(opi_num_type, number_write);
  opi_type_set_display(opi_num_type, number_display);
  opi_type_set_delete_cell(opi_num_type, number_delete);
  opi_type_set_eq(opi_num_type, number_eq);
  opi_type_set_hash(opi_num_type, number_hash);
}

void
opi_num_cleanup(void)
{ opi_type_delete(opi_num_type); }

/******************************************************************************/
struct symbol {
  OpiHeader header;
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
  opi_symbol_type = opi_type_new("Sym");
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
  OpiUndefined *undef = opi_as_ptr(cell);
  opi_unref(undef->what);
  for (size_t i = 0; i < undef->trace->len; ++i)
    opi_location_delete(undef->trace->data[i]);
  cod_vec_destroy(*undef->trace);
  free(undef->trace);
  opi_h2w_free(cell);
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
  opi_undefined_type = opi_type_new("undefined");
  opi_type_set_delete_cell(opi_undefined_type, undefined_delete);
  char *fields[] = { "what" };
  opi_type_set_fields(opi_undefined_type, offsetof(OpiUndefined, what), fields, 1);
  opi_type_set_write(opi_undefined_type, undefined_write);
  opi_type_set_display(opi_undefined_type, undefined_display);
}

void
opi_undefined_cleanup(void)
{ opi_type_delete(opi_undefined_type); }

opi_t
opi_undefined(opi_t what)
{
  OpiUndefined *undef = opi_h2w();
  opi_inc_rc(undef->what = what);
  opi_init_cell(undef, opi_undefined_type);
  undef->trace = malloc(sizeof(opi_trace_t));
  cod_vec_init(*undef->trace);
  return (opi_t)undef;
}

/******************************************************************************/
opi_type_t opi_nil_type;

OpiHeader g_nil;
opi_t opi_nil;

static void
nil_write(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, "nil"); }

void
opi_nil_init(void)
{
  opi_nil_type = opi_type_new("Nil");
  opi_type_set_write(opi_nil_type, nil_write);

  opi_nil = &g_nil;
  opi_init_cell(opi_nil, opi_nil_type);
  opi_inc_rc(opi_nil);
}

void
opi_nil_cleanup(void)
{
  opi_unref(opi_nil);
  opi_type_delete(opi_nil_type);
}

/******************************************************************************/
opi_type_t opi_string_type;

static void
string_write(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, "\"%s\"", opi_as(x, OpiString).str); }

static void
string_display(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, "%s", opi_as(x, OpiString).str); }

static void
string_delete(opi_type_t ty, opi_t x)
{
  OpiString *s = opi_as_ptr(x);
  free(s->str);
  opi_h2w_free(s);
}

static int
string_eq(opi_type_t ty, opi_t x, opi_t y)
{
  size_t l1 = opi_string_get_length(x);
  size_t l2 = opi_string_get_length(y);
  const char *s1 = opi_string_get_value(x);
  const char *s2 = opi_string_get_value(y);
  return l1 == l2 && memcmp(s1, s2, l1) == 0;
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
  opi_string_type = opi_type_new("Str");
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
opi_string_drain_with_len(char *str, size_t len)
{
  OpiString *s = opi_h2w();
  opi_init_cell(s, opi_string_type);
  s->str = str;
  s->len = len;
  return (opi_t)s;
}

extern inline opi_t
opi_string_drain(char *str)
{ return opi_string_drain_with_len(str, strlen(str)); }

opi_t
opi_string_new_with_len(const char *str, size_t len)
{
  char *mystr = malloc(len + 1);
  memcpy(mystr, str, len);
  mystr[len] = 0;
  return opi_string_drain_with_len(mystr, len);
}

opi_t
opi_string_new(const char *str)
{ return opi_string_drain(strdup(str)); }

opi_t
opi_string_from_char(char c)
{
  OpiString *s = opi_h2w();
  opi_init_cell(s, opi_string_type);
  s->str = malloc(2);;
  s->str[0] = c;
  s->str[1] = 0;
  s->len = 1;
  return (opi_t)s;
}

const char*
opi_string_get_value(opi_t x)
{ return opi_as(x, OpiString).str; }

size_t
opi_string_get_length(opi_t x)
{ return opi_as(x, OpiString).len; }

/******************************************************************************/
typedef struct OpiRegEx_s {
  OpiHeader header;
  pcre *re;
} OpiRegEx;

opi_type_t
opi_regex_type;

int
opi_ovector[OPI_OVECTOR_SIZE];

static void
regex_delete(opi_type_t type, opi_t x)
{
  OpiRegEx *regex = opi_as_ptr(x);
  pcre_free(regex->re);
  opi_h2w_free(regex);
}

void
opi_regex_init(void)
{
  opi_regex_type = opi_type_new("regex");
  opi_type_set_delete_cell(opi_regex_type, regex_delete);
}

void
opi_regex_cleanup(void)
{
  opi_type_delete(opi_regex_type);
}

opi_t
opi_regex_new(const char *pattern, int options, const char **errptr)
{
  int errofset;
  pcre *re = pcre_compile(pattern, options, errptr, &errofset, NULL);
  if (re == NULL)
    return NULL;

  OpiRegEx *regex = opi_h2w();
  regex->re = re;
  opi_init_cell(regex, opi_regex_type);
  return (opi_t)regex;
}

int
opi_regex_exec(opi_t x, const char *str, size_t len, size_t offs, int opt)
{
  OpiRegEx *regex = opi_as_ptr(x);
  return pcre_exec(regex->re, NULL, str, len, offs, opt, opi_ovector,
      OPI_OVECTOR_SIZE);
}

int
opi_regex_get_capture_cout(opi_t x)
{
  int count;
  OpiRegEx *regex = opi_as_ptr(x);
  pcre_fullinfo(regex->re, NULL, PCRE_INFO_CAPTURECOUNT, &count);
  return count;
}


/******************************************************************************/
static
OpiHeader g_true, g_false;
opi_type_t opi_boolean_type;
opi_t opi_true, opi_false;

static void
boolean_write(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, x == opi_true ? "true" : "false"); }

void
opi_boolean_init(void)
{
  opi_boolean_type = opi_type_new("Bool");
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
    opi_h2w_free(x);
    if (opi_dec_rc(x = tmp) > 0)
      return;
  }
  opi_drop(x);
}

void
opi_pair_init(void)
{
  opi_pair_type = opi_type_new("Cons");
  opi_type_set_display(opi_pair_type, pair_display);
  opi_type_set_write(opi_pair_type, pair_write);
  opi_type_set_delete_cell(opi_pair_type, pair_delete);
  char *fields[] = { "car", "cdr" };
  opi_type_set_fields(opi_pair_type, offsetof(OpiPair, car), fields, 2);
}

void
opi_pair_cleanup(void)
{ opi_type_delete(opi_pair_type); }

/******************************************************************************/
struct table {
  OpiHeader header;
  struct opi_hash_map *map;
  opi_t list;
};

opi_type_t opi_table_type;

static void
table_delete(opi_type_t ty, opi_t x)
{
  struct opi_hash_map *table = opi_as(x, struct table).map;
  opi_unref(opi_as(x, struct table).list);
  opi_hash_map_destroy(table);
  free(table);
  opi_h2w_free(x);
}

void
opi_table_init(void)
{
  opi_table_type = opi_type_new("table");
  opi_type_set_delete_cell(opi_table_type, table_delete);
}

void
opi_table_cleanup(void)
{ opi_type_delete(opi_table_type); }

opi_t
opi_table(opi_t l, int replace)
{
  struct opi_hash_map *map = malloc(sizeof(struct opi_hash_map));
  opi_hash_map_init(map);

  for (opi_t it = l; it->type == opi_pair_type; it = opi_cdr(it)) {
    opi_t kv = opi_car(it);
    if (opi_unlikely(kv->type != opi_pair_type)) {
      opi_hash_map_destroy(map);
      free(map);
      return opi_undefined(opi_symbol("type-error"));
    }

    opi_t key = opi_car(kv);
    if (opi_unlikely(!opi_type_is_hashable(key->type))) {
      opi_hash_map_destroy(map);
      free(map);
      return opi_undefined(opi_symbol("hash-error"));
    }

    size_t hash = opi_hashof(key);
    struct opi_hash_map_elt elt;
    if (opi_hash_map_find(map, key, hash, &elt)) {
      if (replace) {
        opi_hash_map_insert(map, key, hash, kv, &elt);
      } else {
        opi_hash_map_destroy(map);
        free(map);
        return opi_undefined(opi_symbol("key-collision"));
      }
    } else {
      opi_hash_map_insert(map, key, hash, kv, &elt);
    }
  }

  struct table *tab = opi_h2w();
  tab->map = map;
  opi_inc_rc(tab->list = l);
  opi_init_cell(tab, opi_table_type);
  return (opi_t)tab;
}

opi_t
opi_table_at(opi_t tab, opi_t key, opi_t *err)
{
  struct table *t = opi_as_ptr(tab);

  if (opi_unlikely(!opi_type_is_hashable(key->type))) {
    if (err)
      *err = opi_undefined(opi_symbol("hash-error"));
    return NULL;
  }

  size_t hash = opi_hashof(key);
  struct opi_hash_map_elt elt;
  if (opi_hash_map_find(t->map, key, hash, &elt)) {
    return elt.val;
  } else {
    if (err)
      *err = opi_undefined(opi_symbol("out-of-range"));
    return NULL;
  }
}

opi_t
opi_table_pairs(opi_t tab)
{
  return opi_as(tab, struct table).list;
}

int
opi_table_insert(opi_t tab, opi_t key, opi_t val, int replace, opi_t *err)
{
  struct table *t = opi_as_ptr(tab);

  if (opi_unlikely(!opi_type_is_hashable(key->type))) {
    if (err)
      *err = opi_undefined(opi_symbol("hash-error"));
    return FALSE;
  }

  size_t hash = opi_hashof(key);
  struct opi_hash_map_elt elt;
  if (opi_hash_map_find(t->map, key, hash, &elt)) {
    if (replace) {
      opi_hash_map_insert(t->map, key, hash, val, &elt);
      return TRUE;
    } else {
      if (err)
        *err = NULL;
      return FALSE;
    }
  } else {
    opi_hash_map_insert(t->map, key, hash, val, &elt);
    return TRUE;
  }
}

/******************************************************************************/
struct file {
  OpiHeader header;
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
  opi_h2w_free(p);
}

void
opi_file_init(void)
{
  opi_file_type = opi_type_new("File");

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
  struct file *p = opi_h2w();
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
  OpiFn *fn = opi_as_ptr(cell);
  if (fn->name)
    fprintf(out, "<Fn %s>", fn->name);
  else
    fprintf(out, "<Fn>");
}

static void
fn_delete(opi_type_t type, opi_t cell)
{
  OpiFn *fn = opi_as_ptr(cell);
  fn->delete(fn);
}

void
opi_fn_delete(OpiFn *fn)
{
  if (fn->name)
    free(fn->name);
  free(fn);
}

void
opi_fn_init(void)
{
  opi_fn_type = opi_type_new("Fn");
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
  OpiFn *fn = malloc(sizeof(OpiFn));
  opi_init_cell(fn, opi_fn_type);
  fn->handle = fn_default_handle;
  return (opi_t)fn;
}

void
opi_fn_finalize(opi_t cell, const char *name, opi_fn_handle_t f, int arity)
{
  OpiFn *fn = opi_as_ptr(cell);
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
opi_fn_set_data(opi_t cell, void *data, void (*delete)(OpiFn*))
{
  OpiFn *fn = opi_as_ptr(cell);
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
curry_delete(OpiFn *fn)
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
  if (opi_current_fn->rc == 0) {
    // remove reference from curried arguments
    for (int i = data->n - 1; i >= 0; --i) {
      opi_dec_rc(data->p[i]);
      opi_push(data->p[i]);
    }
    data->n = 0; // don't touch them in destructor
  } else {
    for (int i = data->n - 1; i >= 0; --i)
      opi_push(data->p[i]);
  }
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
  OpiLazy *lazy = opi_as_ptr(x);
  opi_unref(lazy->cell);
  opi_h2w_free(lazy);
}

void
opi_lazy_init(void)
{
  opi_lazy_type = opi_type_new("lazy");
  opi_type_set_delete_cell(opi_lazy_type, lazy_delete);
}

void
opi_lazy_cleanup(void)
{ opi_type_delete(opi_lazy_type); }

opi_t
opi_lazy(opi_t x)
{
  OpiLazy *lazy = opi_h2w();
  opi_inc_rc(lazy->cell = x);
  lazy->is_ready = FALSE;
  opi_init_cell(lazy, opi_lazy_type);
  return (opi_t)lazy;
}

/******************************************************************************/
extern int
yylex_init(OpiScanner **scanner);

extern int
yylex_destroy(OpiScanner *scanner);

extern void
yyset_in(FILE *in, OpiScanner *scanner);

extern FILE*
yyget_in(OpiScanner *scanner);

OpiScanner*
opi_scanner()
{
  OpiScanner *scan;
  yylex_init(&scan);
  return scan;
}

int
opi_scanner_delete(OpiScanner *scanner)
{
  return yylex_destroy(scanner);
}

void
opi_scanner_set_in(OpiScanner *scanner, FILE *in)
{
  yyset_in(in, scanner);
}

FILE*
opi_scanner_get_in(OpiScanner *scanner)
{
  return yyget_in(scanner);
}

OpiLocation*
opi_location(const char *path, int fl, int fc, int ll, int lc)
{
  OpiLocation *loc = malloc(sizeof(OpiLocation));
  *loc = (OpiLocation) { strdup(path), fl, fc, ll, lc };
  return loc;
}

void
opi_location_delete(OpiLocation *loc)
{
  free(loc->path);
  free(loc);
}

OpiLocation*
opi_location_copy(const OpiLocation *loc)
{
  return opi_location(loc->path, loc->fl, loc->fc, loc->ll, loc->lc);
}

/******************************************************************************/
#define DEFINE_VECTOR(dtype, prefix, name, fmt)                                \
  typedef struct name##_s {                                                    \
    OpiHeader header;                                                          \
    dtype *data;                                                               \
    size_t size;                                                               \
  } name;                                                                      \
                                                                               \
  opi_type_t opi_##prefix##vector_type;                                        \
                                                                               \
  static void                                                                  \
  prefix##vector_delete(opi_type_t type, opi_t x)                              \
  {                                                                            \
    name *vec = opi_as_ptr(x);                                                 \
    free(vec->data);                                                           \
    opi_h2w_free(x);                                                           \
  }                                                                            \
                                                                               \
  static void                                                                  \
  prefix##vector_write(opi_type_t type, opi_t x, FILE *out)                    \
  {                                                                            \
    name *vec = opi_as_ptr(x);                                                 \
    fprintf(out, "(" #prefix "vector [");                                      \
    for (size_t i = 0; i < vec->size; ++i) {                                   \
      if (i > 0)                                                               \
        putc(' ', out);                                                        \
      fprintf(out, fmt, vec->data[i]);                                         \
    }                                                                          \
    fputs("])", out);                                                          \
  }                                                                            \
                                                                               \
  opi_t                                                                        \
  opi_##prefix##vector_new_moved(dtype *data, size_t size)                     \
  {                                                                            \
    name *vec = opi_h2w();                                                     \
    vec->data = data;                                                          \
    vec->size = size;                                                          \
    opi_init_cell(vec, opi_##prefix##vector_type);                             \
    return (opi_t)vec;                                                         \
  }                                                                            \
                                                                               \
  opi_t                                                                        \
  opi_##prefix##vector_new(const dtype *data, size_t size)                     \
  {                                                                            \
    dtype *mydata = malloc(sizeof(dtype) * size);                              \
    memcpy(mydata, data, sizeof(dtype) * size);                                \
    return opi_##prefix##vector_new_moved(mydata, size);                       \
  }                                                                            \
                                                                               \
  opi_t                                                                        \
  opi_##prefix##vector_new_raw(size_t size)                                    \
  {                                                                            \
    return opi_##prefix##vector_new_moved(malloc(sizeof(dtype) * size), size); \
  }                                                                            \
                                                                               \
  opi_t                                                                        \
  opi_##prefix##vector_new_filled(dtype fill, size_t size)                     \
  {                                                                            \
    dtype *data = malloc(sizeof(dtype) * size);                                \
    for (size_t i = 0; i < size; ++i)                                          \
      data[i] = fill;                                                          \
    return opi_##prefix##vector_new_moved(data, size);                         \
  }                                                                            \

DEFINE_VECTOR(float, s, OpiSVector, "%f")
#define SVECTOR(x) ((OpiSVector*)(x))

DEFINE_VECTOR(double, d, OpiDVector, "%lf")
#define DVECTOR(x) ((OpiDVector*)(x))

void
opi_vectors_init(void)
{
  opi_dvector_type = opi_type_new("dvector");
  opi_type_set_delete_cell(opi_dvector_type, dvector_delete);
  opi_type_set_write(opi_dvector_type, dvector_write);

  opi_svector_type = opi_type_new("svector");
  opi_type_set_delete_cell(opi_svector_type, svector_delete);
  opi_type_set_write(opi_svector_type, svector_write);
}

void
opi_vectors_cleanup(void)
{
  opi_type_delete(opi_dvector_type);
  opi_type_delete(opi_svector_type);
}

const float*
opi_svector_get_data(opi_t x)
{
  return opi_as(x, OpiSVector).data;
}

const double*
opi_dvector_get_data(opi_t x)
{
  return opi_as(x, OpiDVector).data;
}

size_t
opi_vector_get_size(opi_t x)
{
  return opi_as(x, OpiSVector).size;
}

