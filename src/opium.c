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
uint32_t opi_nargs;

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

static opi_t*
g_my_stack = NULL;

extern void
opi_lexer_init(void);

extern void
opi_lexer_cleanup(void);

void
opi_init(int flags)
{
  if (flags & OPI_INIT_STACK)
    opi_sp = g_my_stack = malloc(sizeof(opi_t) * 0x400);

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
  opi_array_init();
  opi_seq_init();
  opi_buffer_init();
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
  opi_array_cleanup();
  opi_seq_cleanup();
  opi_buffer_cleanup();

  opi_lexer_cleanup();
  opi_allocators_cleanup();

  if (g_my_stack)
    free(g_my_stack);
}

/******************************************************************************/
struct OpiType_s {
  char name[OPI_TYPE_NAME_MAX + 1];

  OpiHeader type_object; // used to map types in generics;

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

static void
type_init(OpiType *ty, const char *name)
{
  static OpiType type_type = {
    .name = "TypeObject",
    .delete_cell = default_destroy_cell,
    .data = NULL,
    .delete_data = default_destroy_data,
    .display = default_display,
    .write = default_write,
    .eq = default_eq,
    .equal = default_equal,
    .hash = NULL,
    .fields = NULL,
  };

  opi_assert(strlen(name) <= OPI_TYPE_NAME_MAX);
  strcpy(ty->name, name);
  opi_init_cell(&ty->type_object, &type_type);
  ty->delete_cell = default_destroy_cell;
  ty->data = NULL;
  ty->delete_data = default_destroy_data;
  ty->display = default_display;
  ty->write = default_write;
  ty->eq = default_eq;
  ty->equal = default_equal;
  ty->hash = NULL;
  ty->fields = NULL;
  ty->nfields = 0;
}

opi_type_t
opi_type_new(const char *name)
{
  OpiType *ty = malloc(sizeof(OpiType));
  type_init(ty, name);
  return ty;
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
typedef struct Impl_s {
  char **f_names;
  opi_t *fs;
  int n;
} Impl;

static Impl*
impl_new(char *const names[], opi_t fs[], int n)
{
  Impl* impl = malloc(sizeof(Impl));
  impl->f_names = malloc(sizeof(char*) * n);
  impl->fs = malloc(sizeof(opi_t) * n);
  for (int i = 0; i < n; ++i) {
    impl->f_names[i] = strdup(names[i]);
    if ((impl->fs[i] = fs[i]))
      opi_inc_rc(fs[i]);
  }
  impl->n = n;
  return impl;
}

static void
impl_delete(Impl *impl)
{
  for (int i = 0; i < impl->n; ++i) {
    free(impl->f_names[i]);
    if (impl->fs[i])
      opi_unref(impl->fs[i]);
  }
  free(impl->f_names);
  free(impl->fs);
  free(impl);
}

/*
 * Check if all unimplemented functions are supplied.
 */
static int
impl_is_full_with(Impl *impl, char *const names[], int n)
{
  for (int i = 0; i < impl->n; ++i) {
    if (impl->fs[i] == NULL) {
      int is_ok = FALSE;
      for (int j = 0; j < n; ++j) {
        if (strcmp(impl->f_names[i], names[j]) == 0) {
          is_ok = TRUE;
          break;
        }
      }
      if (!is_ok)
        return FALSE;
    }
  }
  return TRUE;
}

static void
impl_insert(Impl *impl, char *const nam, opi_t f)
{
  for (int i = 0; i < impl->n; ++i) {
    if (strcmp(impl->f_names[i], nam) == 0) {
      opi_inc_rc(f);
      if (impl->fs[i])
        opi_unref(impl->fs[i]);
      impl->fs[i] = f;
      return;
    }
  }
  opi_warning("failed to insert trait method '%s', no such method\n", nam);
}

typedef struct CondImpl_s {
  Impl *impl;
  int lock; // to prevent cycles
  int n;
  OpiTrait *traits[];
} CondImpl;

static CondImpl*
cond_impl_new(Impl *impl, OpiTrait *traits[], int n)
{
  CondImpl *ci = malloc(sizeof(CondImpl) + sizeof(OpiTrait*) * n);
  ci->impl = impl;
  ci->n = n;
  memcpy(ci->traits, traits, sizeof(OpiTrait*) * n);
  ci->lock = 0;
  return ci;
}

static void
cond_impl_delete(CondImpl *ci)
{
  impl_delete(ci->impl);
  free(ci);
}

// TODO: Handle cyclic dependences.
static int
cond_impl_fits(CondImpl *ci, opi_type_t type)
{
  opi_assert(!ci->lock);
  ci->lock = 1;
  for (int i = 0; i < ci->n; ++i) {
    if (!opi_trait_get_impl(ci->traits[i], type, 0)) {
      ci->lock = 0;
      return FALSE;
    }
  }
  ci->lock = 0;
  return TRUE;
}

struct OpiTrait_s {
  Impl *default_impl;
  OpiHashMap *impls;
  cod_vec(CondImpl*) cond_impls;
  opi_t *generics;
};

typedef struct GenericData_s {
  OpiTrait *trait;
  int moffs;
} GenericData;

static void
generic_data_delete(OpiFn *fn)
{
  free(fn->data);
  opi_fn_delete(fn);
}

static opi_t
generic(void)
{
  GenericData *data = opi_fn_get_data(opi_current_fn);
  opi_t x = opi_get(1);
  opi_t m = opi_trait_get_impl(data->trait, x->type, data->moffs);
  if (m == NULL) {
    opi_drop_args(opi_nargs);
    return opi_undefined(opi_symbol("method-dispatch-error"));
  }
  return opi_apply(m, opi_nargs);
}

OpiTrait*
opi_trait_new(char *const nam[], int n)
{
  opi_assert(n > 0);
  OpiTrait *trait = malloc(sizeof(OpiTrait));
  // default_iml
  opi_t fs[n];
  memset(fs, 0, sizeof fs);
  trait->default_impl = impl_new(nam, fs, n);
  // impls
  trait->impls = malloc(sizeof(OpiHashMap) * n);
  for (int i = 0; i < n; ++i)
    opi_hash_map_init(&trait->impls[i]);
  // cond_impls
  cod_vec_init(trait->cond_impls);
  // generics
  trait->generics = malloc(sizeof(opi_t) * n);
  for (int i = 0; i < n; ++i) {
    opi_t g = opi_fn(NULL, generic, -2);
    GenericData *data = malloc(sizeof(GenericData));
    data->trait = trait;
    data->moffs = i;
    opi_fn_set_data(g, data, generic_data_delete);
    opi_inc_rc(trait->generics[i] = g);
  }
  return trait;
}

void
opi_trait_delete(OpiTrait *trait)
{
  for (int i = 0; i < trait->default_impl->n; ++i)
    opi_hash_map_destroy(&trait->impls[i]);
  free(trait->impls);
  cod_vec_iter(trait->cond_impls, i, x, cond_impl_delete(x));
  cod_vec_destroy(trait->cond_impls);
  for (int i = 0; i < trait->default_impl->n; ++i)
    opi_unref(trait->generics[i]);
  impl_delete(trait->default_impl);
  free(trait->generics);
  free(trait);
}

void
opi_trait_set_default(OpiTrait *trait, char *const nam[], opi_t f[], int n)
{
  for (int i = 0; i < n; ++i)
    impl_insert(trait->default_impl, nam[i], f[i]);
}

int
opi_trait_get_methods(const OpiTrait *trait)
{
  return trait->default_impl->n;
}

int
opi_trait_get_method_offset(const OpiTrait *trait, const char *nam)
{

  for (int i = 0; i < trait->default_impl->n; ++i) {
    if (strcmp(trait->default_impl->f_names[i], nam) == 0)
      return i;
  }
  return -1;
}

int
opi_trait_impl(OpiTrait *trait, opi_type_t type, char *const nam[], opi_t f[],
    int n, int replace)
{
  if (!impl_is_full_with(trait->default_impl, nam, n))
    return OPI_ERR;

  opi_t tyobj = &type->type_object;
  size_t hash = (size_t)type;

  int offs[n];
  for (int i = 0; i < n; ++i) {
    offs[i] = opi_trait_get_method_offset(trait, nam[i]);
    if (!replace) {
      OpiHashMapElt elt;
      if (opi_hash_map_find(&trait->impls[offs[i]], tyobj, hash, &elt))
        return OPI_ERR;
    }
  }

  // apply supplied implementation
  for (int i = 0; i < n; ++i) {
    OpiHashMapElt elt;
    opi_hash_map_find(&trait->impls[offs[i]], tyobj, hash, &elt);
    opi_hash_map_insert(&trait->impls[offs[i]], tyobj, hash, f[i], &elt);
  }
  // apply default implementations
  for (int i = 0; i < trait->default_impl->n; ++i) {
    // skip supplied methods
    int skip = FALSE;
    for (int j = 0; j < n; ++j) {
      if (i == offs[j]) {
        skip = TRUE;
        break;
      }
    }
    if (skip)
      continue;

    OpiHashMapElt elt;
    opi_hash_map_find(&trait->impls[i], tyobj, hash, &elt);
    opi_hash_map_insert(&trait->impls[i], tyobj, hash,
        trait->default_impl->fs[i], &elt);
  }

  return OPI_OK;
}

int
opi_trait_cond_impl(OpiTrait *trait, OpiTrait *traits[], int ntraits,
    char *const nam[], opi_t f[], int nf)
{
  if (!impl_is_full_with(trait->default_impl, nam, nf))
    return OPI_ERR;
  CondImpl *cimpl = cond_impl_new(impl_new(nam, f, nf), traits, ntraits);
  cod_vec_push(trait->cond_impls, cimpl);
  return OPI_OK;
}

int
opi_trait_find_cond_impl(OpiTrait *trait, opi_type_t type)
{
  for (size_t i = 0; i < trait->cond_impls.len; ++i) {
    if (cond_impl_fits(trait->cond_impls.data[i], type))
      return i;
  }
  return -1;
}

opi_t
opi_trait_get_impl(OpiTrait *trait, opi_type_t type, int metoffs)
{
  opi_t tyobj = &type->type_object;
  OpiHashMapElt elt;
  size_t hash = (size_t)type;

  if (opi_hash_map_find_is(&trait->impls[metoffs], tyobj, hash, &elt)) {
    return elt.val;

  } else {
    int impl_id = opi_trait_find_cond_impl(trait, type);
    if (impl_id < 0)
      return NULL;

    char **nam = trait->cond_impls.data[impl_id]->impl->f_names;
    opi_t *f = trait->cond_impls.data[impl_id]->impl->fs;
    int n = trait->cond_impls.data[impl_id]->impl->n;
    opi_assert(opi_trait_impl(trait, type, nam, f, n, FALSE) == OPI_OK);
    return opi_trait_get_impl(trait, type, metoffs);
  }
}

opi_t
opi_trait_get_generic(OpiTrait *trait, int metoffs)
{
  return trait->generics[metoffs];
}

/******************************************************************************/
opi_type_t
opi_num_type;

static void
num_write(opi_type_t ty, opi_t x, FILE *out)
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
num_display(opi_type_t ty, opi_t x, FILE *out)
{
  long double val = opi_as(x, OpiNum).val;
  long double i;
  long double f = modfl(val, &i);
  if (f == 0)
    fprintf(out, "%.0Lf", i);
  else
    fprintf(out, "%Lg", val);
}

static void
num_delete(opi_type_t ty, opi_t cell)
{ opi_h2w_free(cell); }

static int
num_eq(opi_type_t typ, opi_t x, opi_t y)
{ return opi_num_get_value(x) == opi_num_get_value(y); }

static size_t
num_hash(opi_type_t type, opi_t x)
{ return opi_num_get_value(x); }

void
opi_num_init(void)
{
  opi_num_type = opi_type_new("Num");
  opi_type_set_write(opi_num_type, num_write);
  opi_type_set_display(opi_num_type, num_display);
  opi_type_set_delete_cell(opi_num_type, num_delete);
  opi_type_set_eq(opi_num_type, num_eq);
  opi_type_set_hash(opi_num_type, num_hash);
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
OpiHashMap g_sym_map;

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
  OpiHashMapElt elt;
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
{ fprintf(out, "\"%s\"", opi_as(x, OpiStr).str); }

static void
string_display(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, "%s", opi_as(x, OpiStr).str); }

static void
string_delete(opi_type_t ty, opi_t x)
{
  OpiStr *s = opi_as_ptr(x);
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
  OpiStr *s = opi_h2w();
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
  OpiStr *s = opi_h2w();
  opi_init_cell(s, opi_string_type);
  s->str = malloc(2);;
  s->str[0] = c;
  s->str[1] = 0;
  s->len = 1;
  return (opi_t)s;
}

const char*
opi_string_get_value(opi_t x)
{ return opi_as(x, OpiStr).str; }

size_t
opi_string_get_length(opi_t x)
{ return opi_as(x, OpiStr).len; }

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
    if (opi_car(x)->type == opi_pair_type)
      putc('(', out);
    opi_display(opi_car(x), out);
    if (opi_car(x)->type == opi_pair_type)
      putc(')', out);
    putc(':', out);
    x = opi_cdr(x);
  }
  opi_display(x, out);
}

static void
pair_write(opi_type_t ty, opi_t x, FILE *out)
{
  while (x->type == opi_pair_type) {
    if (opi_car(x)->type == opi_pair_type)
      putc('(', out);
    opi_write(opi_car(x), out);
    if (opi_car(x)->type == opi_pair_type)
      putc(')', out);
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
  OpiHashMap *map;
  opi_t list;
};

opi_type_t opi_table_type;

static void
table_delete(opi_type_t ty, opi_t x)
{
  OpiHashMap *table = opi_as(x, struct table).map;
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
  OpiHashMap *map = malloc(sizeof(OpiHashMap));
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
    OpiHashMapElt elt;
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
  OpiHashMapElt elt;
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
  OpiHashMapElt elt;
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
  fprintf(out, "<Fn>");
}

static void
fn_delete(opi_type_t type, opi_t cell)
{
  OpiFn *fn = opi_as_ptr(cell);
  fn->dtor(fn);
}

void
opi_fn_delete(OpiFn *fn)
{
  opi_h6w_free(fn);
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

void
opi_fn_finalize(opi_t cell, const char *name, opi_fn_handle_t f, int arity)
{
  OpiFn *fn = opi_as_ptr(cell);
  fn->name = NULL; // WTF: need this line for better performance
  fn->handle = f;
  fn->data = NULL;
  fn->dtor = opi_fn_delete;
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
opi_fn_set_data(opi_t cell, void *data, void (*dtor)(OpiFn*))
{
  OpiFn *fn = opi_as_ptr(cell);
  fn->data = data;
  if (dtor)
    fn->dtor = dtor;
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

      opi_t curry_f = opi_fn(NULL, curry, arity - nargs);
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
opi_type_t
opi_seq_type;

static void
seq_delete(opi_type_t type, opi_t x)
{
  OpiSeq *seq = opi_as_ptr(x);
  seq->cfg.dtor(seq->iter);
  free(seq);
}

void
opi_seq_init(void)
{
  opi_seq_type = opi_type_new("Seq");
  opi_type_set_delete_cell(opi_seq_type, seq_delete);
}

void
opi_seq_cleanup(void)
{
  opi_type_delete(opi_seq_type);
}

opi_t
opi_seq_new(OpiIter *iter, OpiSeqCfg cfg)
{
  opi_assert(cfg.next && cfg.dtor && cfg.copy);
  OpiSeq *seq = malloc(sizeof(OpiSeq));
  seq->iter = iter;
  seq->cfg = cfg;
  opi_init_cell(seq, opi_seq_type);
  return (opi_t)seq;
}

opi_t
opi_seq_copy(opi_t x)
{
  OpiSeq *seq = opi_as_ptr(x);
  return opi_seq_new(seq->cfg.copy(seq->iter), seq->cfg);
}

/******************************************************************************/
opi_type_t
opi_array_type;

static void
array_delete(opi_type_t type, opi_t x)
{
  opi_t *a = opi_array_get_data(x);
  size_t n = opi_array_get_length(x);
  for (size_t i = 0; i < n; ++i)
    opi_unref(a[i]);
  free(a);
  free(x);
}

static void
array_write(opi_type_t type, opi_t x, FILE *out)
{
  opi_t *a = opi_array_get_data(x);
  size_t n = opi_array_get_length(x);
  fputs("[|", out);
  for (size_t i = 0; i < n; ++i) {
    if (i != 0)
      fputs(" ", out);
    opi_write(a[i], out);
  }
  fputs("|]", out);
}

static void
array_display(opi_type_t type, opi_t x, FILE *out)
{
  opi_t *a = opi_array_get_data(x);
  size_t n = opi_array_get_length(x);
  fputs("[|", out);
  for (size_t i = 0; i < n; ++i) {
    if (i != 0)
      fputs(" ", out);
    opi_display(a[i], out);
  }
  fputs("|]", out);
}

void
opi_array_init(void)
{
  opi_array_type = opi_type_new("Array");
  opi_type_set_delete_cell(opi_array_type, array_delete);
  opi_type_set_write(opi_array_type, array_write);
  opi_type_set_display(opi_array_type, array_display);
}

void
opi_array_cleanup(void)
{
  opi_type_delete(opi_array_type);
}

opi_t
opi_array_drain(opi_t *data, size_t len, size_t cap)
{
  OpiArray *arr = malloc(sizeof(OpiArray));
  if (cap == 0) {
    cap = 0x10;
    data = malloc(sizeof(opi_t) * cap);
  }
  arr->data = data;
  arr->len = len;
  arr->cap = cap;
  opi_init_cell(arr, opi_array_type);
  return (opi_t)arr;
}

opi_t
opi_array_new_empty(size_t reserve)
{
  opi_t *data = malloc(sizeof(opi_t) * reserve);
  return opi_array_drain(data, 0, reserve);
}

void __attribute__((hot, flatten))
opi_array_push(opi_t a, opi_t x)
{
  OpiArray *arr = (OpiArray*)a;
  if (opi_unlikely(arr->len == arr->cap)) {
    arr->cap <<= 1;
    arr->data = realloc(arr->data, sizeof(opi_t) * arr->cap);
  }
  opi_inc_rc(arr->data[arr->len++] = x);
}

opi_t
opi_array_push_with_copy(opi_t a, opi_t x)
{
  OpiArray *arr = (OpiArray*)a;

  size_t cap = arr->cap;
  size_t len = arr->len;
  opi_t *data = NULL;
  if (len == cap)
    cap <<= 1;
  data = malloc(sizeof(opi_t) * cap);

  for (size_t i = 0; i < len; ++i)
    opi_inc_rc(data[i] = arr->data[i]);
  opi_inc_rc(data[len++] = x);

  opi_inc_rc(arr->data[arr->len++] = x);
  return opi_array_drain(data, len, cap);
}

/******************************************************************************/
opi_type_t
opi_buffer_type;

static void
buffer_delete(opi_type_t type, opi_t x)
{
  OpiBuffer *buf = OPI_BUFFER(x);
  if (buf->free)
    buf->free(buf->ptr, buf->c);
  opi_h6w_free(buf);
}

void
opi_buffer_init(void)
{
  opi_buffer_type = opi_type_new("Buffer");
  opi_type_set_delete_cell(opi_buffer_type, buffer_delete);
}

void
opi_buffer_cleanup(void)
{
  opi_type_delete(opi_buffer_type);
}

OpiBuffer*
opi_buffer_new(void *ptr, size_t size, void (*free)(void* ptr,void* c), void *c)
{
  OpiBuffer *buf = opi_h6w();
  buf->ptr = ptr;
  buf->size = size;
  buf->free = free;
  buf->c = c;
  opi_init_cell(buf, opi_buffer_type);
  return buf;
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
opi_location_new(const char *path, int fl, int fc, int ll, int lc)
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
  return opi_location_new(loc->path, loc->fl, loc->fc, loc->ll, loc->lc);
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

int
opi_location_show(OpiLocation *loc, FILE *out)
{
  return opi_show_location(out, loc->path, loc->fc, loc->fl, loc->lc, loc->ll);
}

int
opi_find_string(const char *str, char* const arr[], int n)
{
  for (int i = 0; i < n; ++i) {
    if (strcmp(str, arr[i]) == 0)
      return i;
  }
  return -1;
}

