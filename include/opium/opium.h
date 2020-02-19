#ifndef OPIUM_H
#define OPIUM_H

#include "codeine/vec.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
# define restrict __restrict
#endif

#ifdef __cplusplus
# define OPI_EXTERN extern "C"
#else
# define OPI_EXTERN extern
#endif

#define OPI_DEBUG stderr
#define opi_debug(fmt, ...)                         \
  do {                                              \
    fprintf(OPI_DEBUG, "[\x1b[7m opium \x1b[0m] "); \
    fprintf(OPI_DEBUG, fmt, ##__VA_ARGS__);         \
  } while (0)

#define OPI_ERROR stderr
#define opi_error(fmt, ...)                                \
  do {                                                     \
    fprintf(OPI_ERROR, "[\x1b[38;5;9;7m opium \x1b[0m] "); \
    fprintf(OPI_ERROR, fmt, ##__VA_ARGS__);                \
  } while (0)

#define OPI_WARNING stderr
#define opi_warning(fmt, ...)                                 \
  do {                                                        \
    fprintf(OPI_WARNING, "[\x1b[38;5;11;7m opium \x1b[0m] "); \
    fprintf(OPI_WARNING, fmt, ##__VA_ARGS__);                 \
  } while (0)

#define OPI_TRACE stderr
#define opi_trace(fmt, ...)                              \
  do {                                                   \
    fprintf(OPI_TRACE, "[\x1b[38;5;9m opium \x1b[0m] "); \
    fprintf(OPI_TRACE, fmt, ##__VA_ARGS__);              \
  } while (0)

__attribute__((noreturn, format(printf, 1, 2))) void
opi_die(const char *fmt, ...);

#define opi_likely(expr) __builtin_expect(!!(expr), 1)
#define opi_unlikely(expr) __builtin_expect(!!(expr), 0)

#define opi_assert(expr)                                          \
  do {                                                            \
    if (opi_unlikely(!(expr))) {                                  \
      opi_error("assertion failed, %s:%d\n", __FILE__, __LINE__); \
      opi_error("%s\n", #expr);                                   \
      abort();                                                    \
    }                                                             \
  } while (0)

#ifndef TRUE
# define TRUE 1
#endif

#ifndef FALSE
# define FALSE 0
#endif

#define OPI_OK (0)
#define OPI_ERR (-1)
OPI_EXTERN
int opi_error;

typedef struct OpiLocation_s OpiLocation;
typedef struct OpiType_s OpiType;
typedef OpiType *opi_type_t;
typedef struct OpiH2w_s OpiH2w;
typedef struct OpiH6w_s OpiH6w;
typedef struct OpiAst_s OpiAst;
typedef struct OpiScanner_s OpiScanner;
typedef struct OpiContext_s OpiContext;
typedef struct OpiBuilder_s OpiBuilder;
typedef struct OpiIr_s OpiIr;
typedef struct OpiInsn_s OpiInsn;
typedef struct OpiFlatInsn_s OpiFlatInsn;
typedef struct OpiBytecode_s OpiBytecode;

typedef struct OpiHeader_s OpiHeader;
typedef OpiHeader *opi_t;
#define OPI(x) ((opi_t)(x))

typedef struct OpiNum_s OpiNum;
#define OPI_NUM(x) ((OpiNum*)(x))

typedef struct OpiUndefined_s OpiUndefined;
#define OPI_UNDEFINED(x) ((OpiUndefined*)(x))

typedef struct OpiStr_s OpiStr;
#define OPI_STR(x) ((OpiStr*)(x))

typedef struct OpiPair_s OpiPair;
#define OPI_PAIR(x) ((OpiPair*)(x))

typedef struct OpiFn_s OpiFn;
#define OPI_FN(x) ((OpiFn*)(x))

typedef struct OpiLazy_s OpiLazy;
#define OPI_LAZY(x) ((OpiLazy*)(x))

typedef struct OpiArray_s OpiArray;
#define OPI_ARRAY(x) ((OpiArray*)(x))

typedef struct OpiBuffer_s OpiBuffer;
#define OPI_BUFFER(x) ((OpiBuffer*)(x))

typedef struct OpiVar_s OpiVar;
#define OPI_VAR(x) ((OpiVar*)(x))

typedef struct OpiSeq_s OpiSeq;
#define OPI_SEQ(x) ((OpiSeq*)(x))
/*
 * Argument-stack pointer.
 *
 * This stack is used ony to pass arguments during calls, which usually
 * instantly pop those arguments from the stack and store them in some local
 * variables. Thus size of this stack will only influence a maximal number of
 * arguments to be passed to function. ...which is still quite important since
 * e.g. lists and arrays are constructed by passing all the elemnts of a future
 * container to corresponding function.
 */
OPI_EXTERN
opi_t* opi_sp;

/*
 * Pointer to the "current" function object.
 *
 * Can be used inside function handles (and only!) to assecc closure.
 * Must be used as soon as possible (before any other function application).
 *
 * This parameter is implemented as global due to a bug in LibJIT I have faced
 * once. With this bug it is impossible to pass arguments during tail call.
 */
OPI_EXTERN
OpiFn *opi_current_fn;

/*
 * Number of arguments passed to the function via argument-stack.
 *
 * Must be used as soon as possible (before any other function application).
 *
 * This parameter is implemented as global due to a bug in LibJIT I have faced
 * once. With this bug it is impossible to pass arguments during tail call.
 */
OPI_EXTERN
size_t opi_nargs;

/*
 * Flags for opi_init().
 */
enum {
  /* If enabled, opi_init() will take care to allocate an argument-stack.
   * Otherwize, user must do it himself, and set opi_sp to point on it. */
  OPI_INIT_STACK = 0x1,
  /* Default flags to opi_init(). */
  OPI_INIT_DEFAULT = OPI_INIT_STACK,
};

/*
 * Initialize all relevant runtime environment. This is mandatory to be called
 * before working with major part of Opium.
 *
 * This function will take care of calling all other init-functions you
 * may find here. Don't call these yourself, order of "init"s matters, and only
 * opi_init() will do it in a propper way.
 */
void
opi_init(int flags);

/*
 * Release all runtime environment. Call to obtain clean picture from valgrind.
 *
 * Behaviour of any function in this library becomes undefined once this
 * functions is called.
 */
void
opi_cleanup(void);


/* ==========================================================================
 * Locations
 */
struct OpiLocation_s {
  char *path; /* file path */
  int fl, /* first line */
      fc, /* first column */
      ll, /* last line */
      lc; /* last column */
};

/*
 * Create new location object.
 *
 * Note: path MUST be a full path.
 */
OpiLocation*
opi_location_new(const char *path, int fl, int fc, int ll, int lc);

/*
 * Delete location object allocated by opi_location_new().
 */
void
opi_location_delete(OpiLocation *loc);

/*
 * Copy location object.
 *
 * It is safe to call opi_location_delete() on original location and continue
 * using the copy.
 *
 * Note: output lines are indented with Opium tags, so this function is only
 * usefull for error reporting.
 */
OpiLocation*
opi_location_copy(const OpiLocation *loc);

/*
 * Print text by the given location.
 *
 * Return OPI_OK on success, or OPI_ERR in case of error.
 */
int
opi_show_location(FILE *out, const char *path, int fc, int fl, int lc, int ll);

/*
 * Print location object.
 *
 * Return value is the same as for opi_show_location().
 */
int
opi_location_show(OpiLocation *loc, FILE *out);


/* ==========================================================================
 * Type
 */
#define OPI_TYPE_NAME_MAX 255

OPI_EXTERN
opi_type_t opi_type_type;

typedef struct OpiTypeObject_s OpiTypeObject;

opi_type_t
opi_type_new(const char *name);

void
opi_type_delete(opi_type_t ty);

void
opi_type_set_delete_cell(opi_type_t ty, void (*fn)(opi_type_t,opi_t));
static void OPI_FREE_CELL(opi_type_t ty, opi_t x) { free(x); }

void
opi_type_set_data(opi_type_t ty, void *data, void (*fn)(opi_type_t));

void
opi_type_set_display(opi_type_t ty, void (*fn)(opi_type_t,opi_t,FILE*));

void
opi_type_set_write(opi_type_t ty, void (*fn)(opi_type_t,opi_t,FILE*));

void
opi_type_set_eq(opi_type_t ty, int (*fn)(opi_type_t,opi_t,opi_t));

void
opi_type_set_equal(opi_type_t ty, int (*fn)(opi_type_t,opi_t,opi_t));

void
opi_type_set_hash(opi_type_t ty, size_t (*fn)(opi_type_t,opi_t));

int
opi_type_is_hashable(opi_type_t ty);

void
opi_type_set_fields(opi_type_t type, size_t offs, char **fields, size_t n);

const char*
opi_type_get_name(opi_type_t type);

int
opi_type_get_field_idx(opi_type_t type, const char *field);

size_t
opi_type_get_field_offset(opi_type_t type, size_t field_idx);

size_t
opi_type_get_nfields(opi_type_t type);

char* const*
opi_type_get_fields(opi_type_t type);

void*
opi_type_get_data(opi_type_t type);

void
opi_type_set_is_struct(opi_type_t type, int is_struct);

int
opi_type_is_struct(const opi_type_t type);

opi_t
opi_type_get_type_object(const opi_type_t type);

/* ==========================================================================
 * Trait
 */
typedef struct OpiTrait_s OpiTrait;

// TODO: Create dummy method if n = 0.
OpiTrait*
opi_trait_new(char *const nam[], int n);

void
opi_trait_delete(OpiTrait *trait);

/*
 * Set default methods implementation. Partial implementation is allowed.
 */
void
opi_trait_set_default(OpiTrait *trait, char *const nam[], opi_t f[], int n);

/*
 * Implement trait for given type.
 *
 * If <replace> is not zero, previous implementation (if present) will be
 * replaced by the new one. Otherwize, implementation will fail on such
 * collision.
 *
 * Return OPI_OK on success; otherwize, return OPI_ERR, in case if given
 * implementation does not satisfy trait requirements.
 */
int
opi_trait_impl(OpiTrait *trait, opi_type_t type, char *const nam[], opi_t f[],
    int n, int replace);

/*
 * Add conditional trait implementation.
 *
 * Return OPI_OK on success; otherwize, return OPI_ERR, in case if given
 * implementation does not satisfy trait requirements.
 */
int
opi_trait_cond_impl(OpiTrait *trait, OpiTrait *traits[], int ntraits,
    char *const nam[], opi_t f[], int nf);

/*
 * Get number of methods.
 */
int
opi_trait_get_methods(const OpiTrait *trait);

/*
 * Get method offset.
 *
 * Return offset (>= 0) or -1 in case of error.
 */
int
opi_trait_get_method_offset(const OpiTrait *trait, const char *nam);

/*
 * Find matching conditional implementation.
 *
 * Return implementation identifier (>= 0) or -1 in case of error.
 */
int
opi_trait_find_cond_impl(OpiTrait *trait, opi_type_t type);

/*
 * Get method implementation for given type. If trait is not yet implemented
 * for this type, it will be checked if there is matching conditional
 * implementation; and if so, it will be assigned as an implementation for
 * supplied type.
 *
 * Return method, or NULL in case of error.
 */
opi_t
opi_trait_get_impl(OpiTrait *trait, opi_type_t type, int metoffs);

/*
 * Return function to dispatch implementations by type of the first argument.
 */
opi_t
opi_trait_get_generic(OpiTrait *trait, int metoffs);

OPI_EXTERN
OpiTrait *opi_trait_add,
         *opi_trait_sub,
         *opi_trait_mul,
         *opi_trait_div;

OPI_EXTERN
opi_t opi_generic_add, opi_generic_radd,
      opi_generic_sub, opi_generic_rsub,
      opi_generic_mul, opi_generic_rmul,
      opi_generic_div, opi_generic_rdiv;

OPI_EXTERN
OpiTrait *opi_trait_hash;

OPI_EXTERN
opi_t opi_generic_hash;

void
opi_traits_init(void);

void
opi_traits_cleanup(void);

/* ==========================================================================
 * Cell
 */
typedef uint32_t opi_rc_t;
typedef uint32_t opi_meta_t;

struct OpiHeader_s {
  OpiType *type;
  opi_meta_t meta;
  opi_rc_t rc;
};

#define opi_as(cell, type) ((type*)cell)[0]
#define opi_as_ptr(cell) ((void*)cell)

void
opi_display(opi_t x, FILE *out);

void
opi_write(opi_t x, FILE *out);

static inline int
opi_is(opi_t x, opi_t y)
{ return x == y; }

int
opi_eq(opi_t x, opi_t y);

int
opi_equal(opi_t x, opi_t y);

static inline void
opi_init_cell(void *x_, opi_type_t ty)
{
  opi_t restrict x = (opi_t)x_;
  x->type = ty;
  x->rc = 0;
  x->meta = 0;
}

void
opi_delete(opi_t x);

static inline opi_rc_t
opi_inc_rc(opi_t x)
{ return ++x->rc; }

static inline opi_rc_t
opi_dec_rc(opi_t x)
{ return --x->rc; }

static inline void
opi_drop(opi_t x)
{
  if (x->rc == 0)
    opi_delete(x);
}

static inline void
opi_unref(opi_t x)
{
  if (opi_dec_rc(x) == 0)
    opi_delete(x);
}

size_t
opi_hashof(opi_t x);

/* ==========================================================================
 * Allocators
 */
typedef uintptr_t opi_word_t;

void
opi_allocators_init(void);

void
opi_allocators_cleanup(void);

struct OpiH2w_s {
  OpiHeader header;
  opi_word_t w[2];
};

void*
opi_h2w();

void
opi_h2w_free(void *ptr);

struct OpiH6w_s {
  OpiHeader header;
  opi_word_t w[6];
};

void*
opi_h6w();

void
opi_h6w_free(void *ptr);

/* ==========================================================================
 * Stack
 */
static inline void
opi_push(opi_t x)
{ *opi_sp++ = x; }

static inline opi_t
opi_pop()
{ return *--opi_sp; }

static inline void
opi_popn(size_t n)
{ opi_sp -= n; }

static inline opi_t
opi_get(size_t offs)
{ return *(opi_sp - offs); }

/* ==========================================================================
 * Num
 */
struct OpiNum_s {
  OpiHeader header;
  long double val;
};

OPI_EXTERN opi_type_t
opi_num_type;

void
opi_num_init(void);

void
opi_num_cleanup(void);

static inline opi_t __attribute__((hot, flatten))
opi_num_new(long double x)
{
  OpiNum *num = (OpiNum*)opi_h2w();
  opi_init_cell(num, opi_num_type);
  num->val = x;
  return (opi_t)num;
}

static inline long double __attribute__((hot, always_inline))
opi_num_get_value(opi_t cell)
{ return opi_as(cell, OpiNum).val; }

/* ==========================================================================
 * Symbol
 */
OPI_EXTERN
opi_type_t opi_symbol_type;

void
opi_symbol_init(void);

void
opi_symbol_cleanup(void);

opi_t
opi_symbol(const char *str);

const char* __attribute__((pure))
opi_symbol_get_string(opi_t x);

/* ==========================================================================
 * Undefined
 */
typedef cod_vec(OpiLocation*) opi_trace_t;
struct OpiUndefined_s {
  OpiHeader header;
  opi_t what;
  opi_trace_t* trace;
};

OPI_EXTERN
opi_type_t opi_undefined_type;

void
opi_undefined_init(void);

void
opi_undefined_cleanup(void);

opi_t
opi_undefined(opi_t what);

static inline opi_t __attribute__((pure, deprecated))
opi_undefined_get_what(opi_t x)
{ return opi_as(x, OpiUndefined).what; }

static inline opi_trace_t* __attribute__((pure, deprecated))
opi_undefined_get_trace(opi_t x)
{ return opi_as(x, OpiUndefined).trace; }

/* ==========================================================================
 * Nil
 */
OPI_EXTERN
opi_type_t opi_nil_type;

void
opi_nil_init(void);

void
opi_nil_cleanup(void);

OPI_EXTERN
opi_t opi_nil;

/* ==========================================================================
 * String
 */
OPI_EXTERN
opi_type_t opi_str_type;

typedef struct OpiStr_s {
  OpiHeader header;
  char *restrict str;
  size_t len;
} OpiStr;

void
opi_str_init(void);

void
opi_str_cleanup(void);

opi_t
opi_str_new(const char *str);

opi_t
opi_str_new_with_len(const char *str, size_t len);

opi_t
opi_str_drain(char *str);

opi_t
opi_str_drain_with_len(char *str, size_t len);

opi_t
opi_str_from_char(char c);

static inline const char* __attribute__((pure, deprecated))
opi_str_get_value(opi_t x)
{ return opi_as(x, OpiStr).str; }

static inline size_t __attribute__((pure, deprecated))
opi_str_get_length(opi_t x)
{ return opi_as(x, OpiStr).len; }

/* ==========================================================================
 * RegEx
 */
OPI_EXTERN opi_type_t
opi_regex_type;

#define OPI_OVECTOR_SIZE 0x100

OPI_EXTERN int
opi_ovector[OPI_OVECTOR_SIZE];

void
opi_regex_init(void);

void
opi_regex_cleanup(void);

opi_t
opi_regex_new(const char *pattern, int options, const char** errptr);

int
opi_regex_exec(opi_t x, const char *str, size_t len, size_t offs, int opt);

int
opi_regex_get_capture_cout(opi_t x);

/* ==========================================================================
 * Boolean
 */
OPI_EXTERN
opi_type_t opi_boolean_type;

void
opi_boolean_init(void);

void
opi_boolean_cleanup(void);

OPI_EXTERN
opi_t opi_true, opi_false;

/* ==========================================================================
 * Pair
 */
struct OpiPair_s {
  OpiHeader header;
  opi_t car, cdr;
};

OPI_EXTERN
opi_type_t opi_pair_type;

void
opi_pair_init(void);

void
opi_pair_cleanup(void);

static inline void
_opi_cons_at(opi_t car, opi_t cdr, OpiPair *p)
{
  opi_inc_rc(p->car = car);
  opi_inc_rc(p->cdr = cdr);
  opi_init_cell(p, opi_pair_type);
  p->header.meta = cdr->meta + 1;
}

static inline opi_t
opi_cons(opi_t car, opi_t cdr)
{
  OpiPair *p = (OpiPair*)opi_h2w();
  _opi_cons_at(car, cdr, p);
  return (opi_t)p;
}

static inline __attribute__((pure)) opi_t
opi_car(opi_t x)
{ return opi_as(x, OpiPair).car; }

static inline __attribute__((pure)) opi_t
opi_cdr(opi_t x)
{ return opi_as(x, OpiPair).cdr; }

static inline __attribute__((pure)) size_t
opi_length(opi_t x)
{ return x->meta; }

/* ==========================================================================
 * Table
 */
OPI_EXTERN
opi_type_t opi_table_type;

void
opi_table_init(void);

void
opi_table_cleanup(void);

opi_t
opi_table(opi_t l, int replace);

opi_t
opi_table_at(opi_t tab, opi_t key, opi_t *err);

opi_t
opi_table_pairs(opi_t tab);

int
opi_table_insert(opi_t tab, opi_t pair, int replace, opi_t *err);

opi_t
opi_table_copy(opi_t tab);

/* ==========================================================================
 * Port
 */
OPI_EXTERN
opi_type_t opi_file_type;

OPI_EXTERN
opi_t opi_stdin, opi_stdout, opi_stderr;

void
opi_file_init(void);

void
opi_file_cleanup(void);

opi_t
opi_file(FILE *fs, int (*close)(FILE*));

FILE*
opi_file_get_value(opi_t x);

/* ==========================================================================
 * Fn
 */
typedef opi_t (*opi_fn_handle_t)(void);

struct OpiFn_s {
  OpiHeader header;
  void *data;
  opi_fn_handle_t handle;
  void (*dtor)(OpiFn *self);
  intptr_t arity;
};

void
opi_fn_delete(OpiFn *fn);

OPI_EXTERN
opi_type_t opi_fn_type;

void
opi_fn_init(void);

void
opi_fn_cleanup(void);

static inline opi_t
opi_fn_alloc()
{
  OpiFn *fn = (OpiFn*)opi_h6w();
  fn->handle = NULL;
  opi_init_cell(fn, opi_fn_type);
  return (opi_t)fn;
};

void
opi_fn_finalize(opi_t fn, opi_fn_handle_t f, int arity);

opi_t
opi_fn_new(opi_fn_handle_t f, int arity);

void
opi_fn_set_data(opi_t cell, void *data, void (*dtor)(OpiFn *self));

static inline int
opi_fn_get_arity(opi_t cell)
{ return opi_as(cell, OpiFn).arity; }

static inline opi_fn_handle_t
opi_fn_get_handle(opi_t cell)
{ return opi_as(cell, OpiFn).handle; }

static inline opi_t
opi_fn_apply(opi_t cell, size_t nargs)
{
  OpiFn *fn = (OpiFn*)cell;
  opi_nargs = nargs;
  opi_current_fn = OPI_FN(cell);
  return fn->handle();
}

static inline int __attribute__((pure))
opi_test_arity(int arity, int nargs)
{ return ((arity < 0) & (nargs >= -(1 + arity))) | (arity == nargs); }

/* ==========================================================================
 * Lazy
 */
OPI_EXTERN
opi_type_t opi_lazy_type;

struct OpiLazy_s {
  OpiHeader header;
  opi_t cell;
  uint_fast8_t is_ready;
};

void
opi_lazy_init(void);

void
opi_lazy_cleanup(void);

opi_t
opi_lazy(opi_t x);

static inline opi_t
opi_lazy_get_value(opi_t x)
{
  OpiLazy *lazy = (OpiLazy*)x;
  if (!lazy->is_ready) {
    opi_t val = opi_fn_apply(lazy->cell, 0);
    opi_inc_rc(val);
    opi_unref(lazy->cell);
    lazy->cell = val;
    lazy->is_ready = TRUE;
  }
  return lazy->cell;
}

/* ==========================================================================
 * Array
 */
OPI_EXTERN opi_type_t
opi_array_type;

typedef struct OpiArray_s {
  OpiHeader header;
  opi_t *data;
  size_t len;
  size_t cap;
} OpiArray;

void
opi_array_init(void);

void
opi_array_cleanup(void);

opi_t
opi_array_drain(opi_t *data, size_t len, size_t cap);

opi_t
opi_array_new_empty(size_t reserve);

static inline opi_t*
opi_array_get_data(opi_t x)
{ return opi_as(x, OpiArray).data; }

static inline size_t
opi_array_get_length(opi_t x)
{ return opi_as(x, OpiArray).len; }

void
opi_array_push(opi_t a, opi_t x);

opi_t
opi_array_push_with_copy(opi_t a, opi_t x);

/* ==========================================================================
 * Seq
 */
OPI_EXTERN opi_type_t
opi_seq_type;

typedef struct OpiIter_s OpiIter;

typedef struct OpiSeqCfg_s {
  opi_t    (* next )(OpiIter *iter);
  OpiIter* (* copy )(OpiIter *iter);
  void     (* dtor )(OpiIter *iter);
} OpiSeqCfg;

typedef struct OpiSeqCache_s {
  size_t rc;
  opi_t arr;
  uint32_t end_cnt;
} OpiSeqCache;

static inline OpiSeqCache*
opi_seq_cache_new(opi_t arr, int end_cnt)
{
  OpiSeqCache *cache = opi_h2w();
  cache->arr = arr;
  opi_inc_rc(arr);
  cache->end_cnt = (uint32_t)end_cnt;
  cache->rc = 0;
  return cache;
}

static inline void
opi_seq_cache_ref(OpiSeqCache *cache)
{ cache->rc += 1; }
#define opi_seq_cache_inc_rc opi_seq_cache_ref

static inline void
opi_seq_cache_dec_rc(OpiSeqCache *cache)
{ cache->rc -= 1; }

static inline void
opi_seq_cache_unref(OpiSeqCache *cache)
{
  if (--cache->rc == 0) {
    opi_unref(cache->arr);
    opi_h2w_free(cache);
  }
}

struct OpiSeq_s {
  OpiHeader header;
  OpiIter *restrict iter;
  OpiSeqCfg cfg;
  OpiSeqCache *cache;
  uint32_t cnt;
  int32_t is_free;
};

void
opi_seq_init(void);

void
opi_seq_cleanup(void);

static OpiSeqCfg
opi_seq_cfg_undefined = { .next = NULL, .copy = NULL, .dtor = NULL };

opi_t
opi_seq_new_with_cache(OpiIter *iter, OpiSeqCfg cfg, OpiSeqCache *cache, int cnt);

opi_t
opi_seq_new(OpiIter *iter, OpiSeqCfg cfg);

// TODO: think more
static inline opi_t
opi_seq_next(opi_t x)
{
  OpiSeq *seq = (OpiSeq*)x;

  if (opi_unlikely(seq->cnt == seq->cache->end_cnt))
    return NULL;

  opi_t arr = seq->cache->arr;
  if (seq->cnt < OPI_ARRAY(arr)->len) {
    /*
     * Get element from cache.
     */
    opi_t elt = OPI_ARRAY(arr)->data[seq->cnt++];
    return elt;

  } else {
    /*
     * Evaluate handle.
     */
    opi_t elt = seq->cfg.next(seq->iter);
    if (opi_unlikely(elt == NULL)) {
      seq->cache->end_cnt = seq->cnt;
      return NULL;
    }
    seq->cnt++;

    // If there are no other references to the cache-array then no need to
    // to store elements to cache.
    int is_free = seq->is_free;
    if (!is_free) {
      is_free = seq->cache->rc == 1;
      if (!is_free)
        opi_array_push(arr, elt);
      else
        seq->is_free = TRUE;
    } else {
      assert(seq->cache->rc == 1);
    }
    return elt;
  }

  return seq->cfg.next(seq->iter);
}

opi_t
opi_seq_copy(opi_t x);

/*
 * Evaluate full sequence.
 *
 * Return NULL to indicate succesfull evaluation; otherwize, return object
 * of opi_undefined_type.
 *
 * Note: elements are not guaranteed to be cached, unless the sequenced is
 * referenced elsewhere.
 */
opi_t
opi_seq_force(opi_t s, int must_cache);

/* ==========================================================================
 * Buffer
 */
OPI_EXTERN opi_type_t
opi_buffer_type;

struct OpiBuffer_s {
  OpiHeader header;
  void *ptr;
  size_t size;
  void (*free)(void* ptr, void *c);
  void *c;
};

void
opi_buffer_init(void);

void
opi_buffer_cleanup(void);

OpiBuffer*
opi_buffer_new(void *ptr, size_t size, void (*free)(void* ptr,void* c), void *c);
static void OPI_BUFFER_FREE(void *ptr, void *c) { free(ptr); }

/* ==========================================================================
 * Mutable Variable
 */
struct OpiVar_s {
  OpiHeader header;
  opi_t val;
};

OPI_EXTERN
opi_type_t opi_var_type;

void
opi_var_init(void);

void
opi_var_cleanup(void);

static inline opi_t
opi_var_new(opi_t x)
{
  OpiVar *var = opi_h2w();
  var->val = x;
  opi_inc_rc(x);
  opi_init_cell(var, opi_var_type);
  return OPI(var);
}

static inline void
opi_var_set(opi_t var, opi_t x)
{
  opi_inc_rc(x);
  opi_unref(OPI_VAR(var)->val);
  OPI_VAR(var)->val = x;
}

/* ==========================================================================
 * AST
 */
typedef enum OpiAstTag_e {
  OPI_AST_CONST,
  OPI_AST_VAR,
  OPI_AST_APPLY,
  OPI_AST_FN,
  OPI_AST_LET,
  OPI_AST_IF,
  OPI_AST_FIX,
  OPI_AST_BLOCK,
  OPI_AST_LOAD,
  OPI_AST_MATCH,
  OPI_AST_STRUCT,
  OPI_AST_USE,
  OPI_AST_RETURN,
  OPI_AST_BINOP,
  OPI_AST_TRAIT,
  OPI_AST_IMPL,
  OPI_AST_ISOF,
  OPI_AST_CTOR,
  OPI_AST_SETVAR,
} OpiAstTag;

typedef enum OpiPatternTag_e {
  OPI_PATTERN_IDENT,
  OPI_PATTERN_UNPACK,
} OpiPatternTag;

typedef struct OpiAstPattern_s OpiAstPattern;
struct OpiAstPattern_s {
  OpiPatternTag tag;
  union {
    char *ident;
    struct {
      char *type, *alias, **fields;
      OpiAstPattern **subs;
      size_t n;
    } unpack;
  };
};

OpiAstPattern*
opi_ast_pattern_new_ident(const char *ident);

OpiAstPattern*
opi_ast_pattern_new_unpack(const char *type, OpiAstPattern **subs, char **fields,
    size_t n, char *alias);

void
opi_ast_pattern_delete(OpiAstPattern *pattern);

struct OpiAst_s {
  OpiAstTag tag;
  union {
    opi_t cnst;
    char *var;
    struct { OpiAst *fn, **args; size_t nargs; char eflag; OpiLocation *loc; } apply;
    struct { char **args; size_t nargs; OpiAst *body; } fn;
    struct { char **vars; OpiAst **vals; size_t n; int is_vars; } let;
    struct { OpiAst *test, *then, *els; } iff;
    struct { OpiAst **exprs; size_t n; int drop; char *ns; } block;
    char *load;
    struct { OpiAstPattern *pattern; OpiAst *then, *els, *expr; } match;
    struct { char *name, **fields; size_t nfields; } strct;
    struct { char *name, **f_nams; OpiAst *build, **fs; int nfs; } trait;
    struct { char *trait, *target; char **f_nams; OpiAst **fs; int nfs; } impl;
    struct { char *old, *nw; } use;
    OpiAst *ret;
    struct { int opc; OpiAst *lhs, *rhs; } binop;
    struct { OpiAst *expr; char *of; } isof;
    struct { char *name, **fldnams; OpiAst *src, **flds; int nflds; } ctor;
    struct { char *var; OpiAst *val; } setvar;
  };
};

OpiScanner*
opi_scanner();

int
opi_scanner_delete(OpiScanner *scanner);

void
opi_scanner_set_in(OpiScanner *scanner, FILE *in);

FILE*
opi_scanner_get_in(OpiScanner *scanner);

OpiAst*
opi_parse(FILE *in);

OpiAst*
opi_parse_expr(OpiScanner *scanner, char **errorptr);

OpiAst*
opi_parse_string(const char *str);

void
opi_ast_delete(OpiAst *node);

OpiAst*
opi_ast_const(opi_t x);

OpiAst*
opi_ast_var(const char *name);

OpiAst*
opi_ast_use(const char *old, const char *nw);

OpiAst*
opi_ast_apply(OpiAst *fn, OpiAst **args, size_t nargs);

void
opi_ast_append_arg(OpiAst *appy, OpiAst *arg);

OpiAst*
opi_ast_fn(char **args, size_t nargs, OpiAst *body);

OpiAst*
opi_ast_fn_new_with_patterns(OpiAstPattern **args, size_t nargs, OpiAst *body);

OpiAst*
opi_ast_let(char **vars, OpiAst **vals, size_t n, OpiAst *body);

OpiAst*
opi_ast_let_var(char **vars, OpiAst **vals, size_t n, OpiAst *body);

OpiAst*
opi_ast_if(OpiAst *test, OpiAst *then, OpiAst *els);

OpiAst*
opi_ast_fix(char **vars, OpiAst **lams, size_t n, OpiAst *body);

OpiAst*
opi_ast_block(OpiAst **exprs, size_t n);

void
opi_ast_block_set_drop(OpiAst *block, int drop);

void
opi_ast_block_set_namespace(OpiAst *block, const char *ns);

void
opi_ast_block_prepend(OpiAst *block, OpiAst *node);

void
opi_ast_block_append(OpiAst *block, OpiAst *node);

OpiAst*
opi_ast_load(const char *path);

OpiAst*
opi_ast_match(OpiAstPattern *pattern, OpiAst *expr, OpiAst *then, OpiAst *els);

OpiAst*
opi_ast_match_new_simple(const char *type, char **vars, char **fields, size_t n,
    OpiAst *expr, OpiAst *then, OpiAst *els);

OpiAst*
opi_ast_struct(const char *name, char** fields, size_t nfields);

OpiAst*
opi_ast_trait(const char *name, char *const f_nams[], OpiAst *fsp[], int n);

OpiAst*
opi_ast_impl(const char *trait, const char *target, char *const f_nams[],
    OpiAst *fs[], int nfs);

OpiAst*
opi_ast_and(OpiAst *x, OpiAst *y);

OpiAst*
opi_ast_or(OpiAst *x, OpiAst *y);

OpiAst*
opi_ast_eor(OpiAst *expr, OpiAst *els, const char *ename);

OpiAst*
opi_ast_when(OpiAst *test_expr,
    const char *then_bind, OpiAst *then_expr,
    const char *else_bind, OpiAst *else_expr);

OpiAst*
opi_ast_return(OpiAst *val);

OpiAst*
opi_ast_binop(int opc, OpiAst *lhs, OpiAst *rhs);

OpiAst*
opi_ast_isof(OpiAst *expr, const char *of);

OpiAst*
opi_ast_ctor(const char *name, char* const fldnams[], OpiAst *flds[], int nflds,
    OpiAst *src);

OpiAst*
opi_ast_setvar(const char *var, OpiAst *val);

/* ==========================================================================
 * Context
 */
struct OpiContext_s {
  cod_vec(OpiType*) types;
  cod_vec(OpiTrait*) traits;
  cod_vec(OpiInsn*) bc;
  struct cod_strvec dl_paths;
  cod_vec(void*) dls;
};

void
opi_context_init(OpiContext *ctx);

void
opi_context_destroy(OpiContext *ctx);

void
opi_context_add_type(OpiContext *ctx, opi_type_t type);

void
opi_context_add_trait(OpiContext *ctx, OpiTrait *trait);

void
opi_context_drain_bytecode(OpiContext *ctx, OpiBytecode *bc);

void
opi_context_add_dl(OpiContext *ctx, const char *path, void *dl);

void*
opi_context_find_dl(OpiContext *ctx, const char *path);

int
opi_is_dl(const char *path);

/* ==========================================================================
 * IR
 */
typedef struct OpiAlist_s {
  struct cod_strvec keys, vals;
} OpiAlist;

void
opi_alist_init(OpiAlist *a);

void
opi_alist_destroy(OpiAlist *a);

size_t
opi_alist_get_size(OpiAlist *a);

void
opi_alist_push(OpiAlist *a, const char *var, const char *map);

void
opi_alist_pop(OpiAlist *a, size_t n);

typedef struct {
  char *name;
  opi_t c_val;
} OpiDecl;

static inline void
opi_decl_destroy(OpiDecl d)
{
  free(d.name);
  if (d.c_val)
    opi_unref(d.c_val);
}

struct OpiBuilder_s {
  OpiBuilder *parent;
  OpiContext *ctx;

  opi_t var_ctor;

  int frame_offset;
  cod_vec(OpiDecl) decls;
  OpiAlist *alist;

  struct cod_strvec *srcdirs;
  struct cod_strvec *loaded;
  struct cod_strvec *load_state;

  struct cod_strvec *type_names;
  struct cod_ptrvec *types;

  struct cod_strvec *trait_names;
  struct cod_ptrvec *traits;
};

static inline int
opi_builder_is_derived(const OpiBuilder *bldr)
{ return bldr->parent != NULL; }

void
opi_builder_init(OpiBuilder *bldr, OpiContext *ctx);

void
opi_builder_init_derived(OpiBuilder *bldr, OpiBuilder *parent);

void
opi_builder_destroy(OpiBuilder *bldr);

void
opi_builtins(OpiBuilder *bldr);

int
opi_builder_load_dl(OpiBuilder *bldr, void *dl);

void
opi_builder_push_decl(OpiBuilder *bldr, const char *var);

void
opi_builder_pop_decl(OpiBuilder *bldr);

void
opi_builder_capture(OpiBuilder *bldr, const char *var);

const char*
opi_builder_assoc(OpiBuilder *bldr, const char *var);

const char*
opi_builder_try_assoc(OpiBuilder *bldr, const char *var);

OpiDecl*
opi_builder_find_deep(OpiBuilder *bldr, const char *var);

typedef struct OpiScope_s {
  size_t nvars1, ntypes1, ntraits1, nconsts1, vasize1;
} OpiScope;

void
opi_builder_begin_scope(OpiBuilder *bldr, OpiScope *scp);

void
opi_builder_drop_scope(OpiBuilder *bldr, OpiScope *scp);

void
opi_builder_add_source_directory(OpiBuilder *bldr, const char *path);

char*
opi_builder_find_path(OpiBuilder *bldr, const char *path, char *fullpath);

int
opi_builder_add_type(OpiBuilder *bldr, const char *name, opi_type_t type);

int
opi_builder_add_trait(OpiBuilder *bldr, const char *name, OpiTrait *trait);

opi_type_t
opi_builder_find_type(OpiBuilder *bldr, const char *name);

OpiTrait*
opi_builder_find_trait(OpiBuilder *bldr, const char *name);

void
opi_builder_def_const(OpiBuilder *bldr, const char *name, opi_t val);

int
opi_builder_def_type(OpiBuilder *bldr, const char *name, opi_type_t type);

int
opi_builder_def_trait(OpiBuilder *bldr, const char *name, OpiTrait *trait);

typedef enum OpiIrTag_e {
  OPI_IR_CONST,
  OPI_IR_VAR,
  OPI_IR_APPLY,
  OPI_IR_FN,
  OPI_IR_LET,
  OPI_IR_IF,
  OPI_IR_FIX,
  OPI_IR_BLOCK,
  OPI_IR_MATCH,
  OPI_IR_RETURN,
  OPI_IR_BINOP,
  OPI_IR_SETVAR,
} OpiIrTag;

typedef struct OpiIrPattern_s OpiIrPattern;
struct OpiIrPattern_s {
  OpiPatternTag tag;
  struct {
    opi_type_t type;
    OpiIrPattern **subs;
    size_t *offs, n;
    char *alias;
  } unpack;
};

OpiIrPattern*
opi_ir_pattern_new_ident(void);

OpiIrPattern*
opi_ir_pattern_new_unpack(opi_type_t type, OpiIrPattern **subs, size_t *offs,
    size_t n, char *alias);

void
opi_ir_pattern_delete(OpiIrPattern *pattern);

struct OpiIr_s {
  OpiIrTag tag;
  size_t rc;
  union {
    opi_t cnst;
    size_t var;
    struct { OpiIr *fn, **args; size_t nargs; char eflag; OpiLocation *loc; } apply;
    struct { OpiIr **caps; size_t ncaps, nargs; OpiIr *body; } fn;
    struct { OpiIr **vals; size_t n; int is_vars; } let;
    struct { OpiIr *test, *then, *els; } iff;
    struct { OpiIr **exprs; size_t n; int drop; } block;
    struct { OpiIrPattern *pattern; OpiIr *expr, *then, *els; } match;
    OpiIr *ret;
    struct { int opc; OpiIr *lhs, *rhs; } binop;
    struct { int var; OpiIr *val; } setvar;
  };
  opi_type_t vtype; // used by APPLY to propagate the type of the
                    // value when it is known at build-stage.
};

OpiIr*
opi_builder_build_ir(OpiBuilder *bldr, OpiAst *ast);

enum {
  OPI_BUILD_DEFAULT,
  OPI_BUILD_EXPORT,
};

OpiBytecode*
opi_build(OpiBuilder *bldr, OpiAst *ast, int mode);

int
opi_load(OpiBuilder *bldr, const char *path);

void
_opi_ir_delete(OpiIr *node);

static inline void
opi_ir_ref(OpiIr *ir)
{
  if (ir)
    ir->rc += 1;
}

static inline void
opi_ir_ref_arr(OpiIr **arr, size_t n)
{
  for (size_t i = 0; i < n; ++i)
    opi_ir_ref(arr[i]);
}

static inline void
opi_ir_drop(OpiIr *ir)
{
  if (ir && ir->rc == 0)
    _opi_ir_delete(ir);
}

static inline void
opi_ir_drop_arr(OpiIr **arr, size_t n)
{
  for (size_t i = 0; i < n; ++i)
    opi_ir_drop(arr[i]);
}

static inline void
opi_ir_unref(OpiIr *ir)
{
  if (ir && --ir->rc == 0)
    _opi_ir_delete(ir);
}

static inline void
opi_ir_unref_arr(OpiIr **arr, size_t n)
{
  for (size_t i = 0; i < n; ++i)
    opi_ir_unref(arr[i]);
}

void
opi_ir_emit(OpiIr *ir, OpiBytecode *bc);

OpiBytecode*
opi_emit_free_fn_body(OpiIr *ir, int nargs);

OpiIr*
opi_ir_const(opi_t x);

OpiIr*
opi_ir_var(size_t offs);

OpiIr*
opi_ir_apply(OpiIr *fn, OpiIr **args, size_t nargs);

OpiIr*
opi_ir_fn(OpiIr **caps, size_t ncaps, size_t nargs, OpiIr *body);

OpiIr*
opi_ir_let(OpiIr **vals, size_t n);

OpiIr*
opi_ir_if(OpiIr *test, OpiIr *then, OpiIr *els);

OpiIr*
opi_ir_fix(OpiIr **vals, size_t n);

OpiIr*
opi_ir_block(OpiIr **exprs, size_t n);

static void
opi_ir_block_set_drop(OpiIr *block, int drop)
{ block->block.drop = drop; }

OpiIr*
opi_ir_match(OpiIrPattern *pattern, OpiIr *expr, OpiIr *then, OpiIr *els);

OpiIr*
opi_ir_return(OpiIr *val);

OpiIr*
opi_ir_binop(int opc, OpiIr *lhs, OpiIr *rhs);

OpiIr*
opi_ir_setvar(int var, OpiIr *val);


/* ==========================================================================
 * Bytecode
 */
typedef enum OpiOpc_e {
  OPI_OPC_NOP,
  OPI_OPC_END,

  OPI_OPC_CONST,
#define OPI_CONST_REG_OUT(insn) (insn)->reg[0]
#define OPI_CONST_ARG_CELL(insn) (insn)->ptr[1]

  OPI_OPC_APPLY,
  OPI_OPC_APPLYTC,
  OPI_OPC_APPLYI,
#define OPI_APPLY_REG_OUT(insn) (insn)->reg[0]
#define OPI_APPLY_REG_FN(insn) (insn)->reg[1]
#define OPI_APPLY_ARG_NARGS(insn) (insn)->reg[2]

  OPI_OPC_RET,
#define OPI_RET_REG_VAL(insn) (insn)->reg[0]

  OPI_OPC_PUSH,
#define OPI_PUSH_REG_VAL(insn) (insn)->reg[0]

  OPI_OPC_POP,
#define OPI_POP_ARG_N(insn) (insn)->reg[0]

  OPI_OPC_INCRC,
#define OPI_INCRC_REG_CELL(insn) (insn)->reg[0]

  OPI_OPC_DECRC,
#define OPI_DECRC_REG_CELL(insn) (insn)->reg[0]

  OPI_OPC_DROP,
#define OPI_DROP_REG_CELL(insn) (insn)->reg[0]

  OPI_OPC_UNREF,
#define OPI_UNREF_REG_CELL(insn) (insn)->reg[0]

  OPI_OPC_LDCAP,
#define OPI_LDCAP_REG_OUT(insn) (insn)->reg[0]
#define OPI_LDCAP_ARG_IDX(insn) (insn)->reg[1]

  OPI_OPC_PARAM,
#define OPI_PARAM_REG_OUT(insn) (insn)->reg[0]
#define OPI_PARAM_ARG_OFFS(insn) (insn)->reg[1]

  OPI_OPC_ALCFN,
#define OPI_ALCFN_REG_OUT(insn) (insn)->reg[0]

  OPI_OPC_FINFN,
#define OPI_FINFN_REG_CELL(insn) (insn)->reg[0]
#define OPI_FINFN_ARG_DATA(insn) ((OpiFnInsnData*)(insn)->ptr[1])

  OPI_OPC_IF,
#define OPI_IF_REG_TEST(insn) (insn)->reg[0]
#define OPI_IF_ARG_ELSE(insn) (insn)->ptr[1]

  OPI_OPC_JMP,
#define OPI_JMP_ARG_TO(insn) (insn)->ptr[0]

  OPI_OPC_PHI,
#define OPI_PHI_REG(insn) (insn)->reg[0]

  OPI_OPC_DUP,
#define OPI_DUP_REG_OUT(insn) (insn)->reg[0]
#define OPI_DUP_REG_IN(insn) (insn)->reg[1]

  OPI_OPC_BEGSCP,
#define OPI_BEGSCP_ARG_N(insn) (insn)->reg[0]

  OPI_OPC_ENDSCP,
#define OPI_ENDSCP_ARG_NCELLS(insn) (insn)->reg[0]
#define OPI_ENDSCP_ARG_CELLS(insn) (insn)->ptr[1]

  OPI_OPC_TEST,
#define OPI_TEST_REG_OUT(insn) (insn)->reg[0]
#define OPI_TEST_REG_IN(insn)  (insn)->reg[1]

  OPI_OPC_GUARD,
#define OPI_GUARD_REG(insn) (insn)->reg[0]

  OPI_OPC_TESTTY,
#define OPI_TESTTY_REG_OUT(insn)  (insn)->reg[0]
#define OPI_TESTTY_REG_CELL(insn) (insn)->reg[1]
#define OPI_TESTTY_ARG_TYPE(insn) (insn)->ptr[2]

  OPI_OPC_LDFLD,
#define OPI_LDFLD_REG_OUT(insn)  (insn)->reg[0]
#define OPI_LDFLD_REG_CELL(insn) (insn)->reg[1]
#define OPI_LDFLD_ARG_OFFS(insn) (insn)->reg[2]

  OPI_OPC_BINOP_START,
  OPI_OPC_CONS = OPI_OPC_BINOP_START,
  OPI_OPC_ADD, OPI_OPC_SUB, OPI_OPC_MUL, OPI_OPC_DIV, OPI_OPC_FMOD,
  OPI_OPC_NUMEQ, OPI_OPC_NUMNE, OPI_OPC_LT, OPI_OPC_GT, OPI_OPC_LE, OPI_OPC_GE,
  OPI_OPC_BINOP_END = OPI_OPC_GE,
#define OPI_BINOP_REG_OUT(insn) (insn)->reg[0]
#define OPI_BINOP_REG_LHS(insn) (insn)->reg[1]
#define OPI_BINOP_REG_RHS(insn) (insn)->reg[2]

  OPI_OPC_VAR,
#define OPI_VAR_REG(insn) (insn)->reg[0]
  OPI_OPC_SET,
#define OPI_SET_REG(insn) (insn)->reg[0]
#define OPI_SET_ARG_VAL(insn) (insn)->reg[1]

  OPI_OPC_SETVAR,
#define OPI_SETVAR_REG_REF(insn) (insn)->reg[0]
#define OPI_SETVAR_REG_VAL(insn) (insn)->reg[1]

  OPI_OPC_DEREF,
#define OPI_DEREF_REG_OUT(insn) (insn)->reg[0]
#define OPI_DEREF_REG_VAR(insn) (insn)->reg[1]
} OpiOpc;

typedef struct OpiFlatInsn_s {
  OpiOpc opc;
  union {
    uintptr_t reg[3];
    void *restrict ptr[3];
  };
} OpiFlatInsn;

struct OpiInsn_s {
  OpiOpc opc;
  union {
    uintptr_t reg[3];
    void *ptr[3];
  };

  OpiInsn *prev;
  OpiInsn *next;
};

static inline void
opi_insn_chain(OpiInsn *prev, OpiInsn *insn, OpiInsn *next)
{
  insn->prev = prev;
  insn->next = next;

  if (prev)
    prev->next = insn;
  if (next)
    next->prev = insn;
}

void
opi_insn_delete1(OpiInsn *insn);

void
opi_insn_delete(OpiInsn *insn);

void
opi_insn_dump1(OpiInsn *insn, FILE *out);

void
opi_insn_dump(OpiInsn *insn, FILE *out);

int
opi_insn_is_using(OpiInsn *insn, int vid);

int
opi_insn_is_killing(OpiInsn *insn, int vid);

int
opi_insn_is_end(OpiInsn *insn);

OpiInsn*
opi_insn_nop();

OpiInsn*
opi_insn_const(int ret, opi_t cell);

OpiInsn*
opi_insn_apply(int ret, int fn, size_t nargs, int tc);

OpiInsn*
opi_insn_applyi(int ret, int fn, size_t nargs);

OpiInsn*
opi_insn_ret(int val);

OpiInsn*
opi_insn_push(int val);

OpiInsn*
opi_insn_pop(size_t n);

OpiInsn*
opi_insn_ldcap(int out, int idx);

OpiInsn*
opi_insn_param(int out, int offs);

OpiInsn*
opi_insn_test(int out, int in);

OpiInsn*
opi_insn_if(int test);

OpiInsn*
opi_insn_jmp(OpiInsn *to);

OpiInsn*
opi_insn_phi(int reg);

OpiInsn*
opi_insn_incrc(int cell);

OpiInsn*
opi_insn_decrc(int cell);

OpiInsn*
opi_insn_drop(int cell);

OpiInsn*
opi_insn_unref(int cell);

typedef struct OpiFnInsnData_s {
  OpiBytecode *bc;
  OpiIr *ir;
  int arity;
  int ncaps;
  int caps[];
} OpiFnInsnData;

OpiInsn*
opi_insn_alcfn(int out);

OpiInsn*
opi_insn_finfn(int cell, int arity, OpiBytecode *bc, OpiIr *ir, int *cap, size_t ncap);

OpiInsn*
opi_insn_begscp(size_t n);

OpiInsn*
opi_insn_endscp(int *cells, size_t n);

OpiInsn*
opi_insn_testty(int out, int cell, opi_type_t type);

OpiInsn*
opi_insn_ldfld(int out, int cell, size_t offs);

OpiInsn*
opi_insn_guard(int in);

OpiInsn*
opi_insn_binop(OpiOpc opc, int out, int lhs, int rhs);

OpiInsn*
opi_insn_var(int reg);

OpiInsn*
opi_insn_set(int reg, int val);

OpiInsn*
opi_insn_and(int out, int lhs, int rhs);

OpiInsn*
opi_insn_setvar(int var, int val);

OpiInsn*
opi_insn_deref(int out, int var);

typedef enum OpiValType_e {
  // Value is "born" in local scope with RC petentionaly set to zero at the
  // beginning.
  OPI_VAL_LOCAL,
  // External value with already (and for sure) non-zero RC.
  OPI_VAL_GLOBAL,
  OPI_VAL_PHI,
  OPI_VAL_BOOL,
} OpiValType;

typedef struct OpiValInfo_s {
  OpiValType type;
  opi_t c; // constant value
  OpiInsn *creatat; // isntruction created the value
  int is_var; // weather the value is a mutable variable
  opi_type_t vtype;
} OpiValInfo;

struct OpiBytecode_s {
  size_t nvals;
  size_t vinfo_cap;
  OpiValInfo *vinfo;
  cod_vec(int) ulist; // list of values whose type was updated
  OpiInsn *head;
  OpiInsn *tail;
  OpiInsn *point;
  OpiFlatInsn *tape;
  int is_generator;
};

OpiBytecode*
opi_bytecode();

void
opi_bytecode_delete(OpiBytecode *bc);

OpiInsn*
opi_bytecode_drain(OpiBytecode *bc);

void
opi_bytecode_fix_lifetimes(OpiBytecode *bc);

void
opi_bytecode_cleanup(OpiBytecode *bc);

OpiFlatInsn*
opi_bytecode_flatten(OpiBytecode *bc);

static void
opi_bytecode_finalize(OpiBytecode *bc)
{
  opi_bytecode_fix_lifetimes(bc);
  opi_bytecode_cleanup(bc);
  bc->tape = opi_bytecode_flatten(bc);
}

int
opi_bytecode_new_val(OpiBytecode *bc, OpiValType vtype);

static inline int
opi_bytecode_value_is_local(OpiBytecode *bc, int vid)
{ return bc->vinfo[vid].type == OPI_VAL_LOCAL; }

static inline int
opi_bytecode_value_is_global(OpiBytecode *bc, int vid)
{ return bc->vinfo[vid].type == OPI_VAL_GLOBAL; }

void
opi_bytecode_set_vtype(OpiBytecode *bc, int vid, opi_type_t type);

void
opi_bytecode_restore_vtypes(OpiBytecode *bc, int start);

void
opi_bytecode_append(OpiBytecode *bc, OpiInsn *insn);

void
opi_bytecode_prepend(OpiBytecode *bc, OpiInsn *insn);

void
opi_bytecode_write(OpiBytecode *bc, OpiInsn *insn);

OpiInsn*
opi_bytecode_find_creating(OpiBytecode *bc, int vid);

int
opi_bytecode_const(OpiBytecode *bc, opi_t cell);

int
opi_bytecode_apply(OpiBytecode *bc, int fn, size_t nargs, ...);

int
opi_bytecode_apply_tailcall(OpiBytecode *bc, int fn, size_t nargs, ...);

int
opi_bytecode_apply_arr(OpiBytecode *bc, int fn, size_t nargs, const int *args);

int
opi_bytecode_apply_tailcall_arr(OpiBytecode *bc, int fn, size_t nargs, const int *args);

int
opi_bytecode_applyi_arr(OpiBytecode *bc, int fn, size_t nargs, const int *args);

void
opi_bytecode_ret(OpiBytecode *bc, int val);

void
opi_bytecode_push(OpiBytecode *bc, int val);

void
opi_bytecode_pop(OpiBytecode *bc, size_t n);

int
opi_bytecode_ldcap(OpiBytecode *bc, size_t idx);

int
opi_bytecode_param(OpiBytecode *bc, size_t offs);

int
opi_bytecode_alcfn(OpiBytecode *bc, OpiValType valtype);

void
opi_bytecode_finfn(OpiBytecode *bc, int cell, int arity, OpiBytecode *body,
    OpiIr *ir, int *cap, size_t ncap);

int
opi_bytecode_test(OpiBytecode *bc, int in);

typedef struct OpiIf_s {
  OpiInsn *iff, *els;
} OpiIf;
void
opi_bytecode_if(OpiBytecode *bc, int test, OpiIf *iff);
void
opi_bytecode_if_else(OpiBytecode *bc, OpiIf *iff);
void
opi_bytecode_if_end(OpiBytecode *bc, OpiIf *iff);

int
opi_bytecode_phi(OpiBytecode *bc);

void
opi_bytecode_dup(OpiBytecode *bc, int dst, int src);

void
opi_bytecode_incrc(OpiBytecode *bc, int cell);

void
opi_bytecode_decrc(OpiBytecode *bc, int cell);

void
opi_bytecode_drop(OpiBytecode *bc, int cell);

void
opi_bytecode_unref(OpiBytecode *bc, int cell);

void
opi_bytecode_begscp(OpiBytecode *bc, size_t n);

void
opi_bytecode_endscp(OpiBytecode *bc, int *cells, size_t n);

int
opi_bytecode_testty(OpiBytecode *bc, int cell, opi_type_t type);

int
opi_bytecode_ldfld(OpiBytecode *bc, int cell, size_t offs);

void
opi_bytecode_guard(OpiBytecode *bc, int cell);

int
opi_bytecode_binop(OpiBytecode *bc, OpiOpc opc, int lhs, int rhs);

int
opi_bytecode_var(OpiBytecode *bc);

void
opi_bytecode_set(OpiBytecode *bc, int reg, int val);

int
opi_bytecode_and(OpiBytecode *bc, int lhs, int rhs);

void
opi_bytecode_setvar(OpiBytecode *bc, int ref, int val);

int
opi_bytecode_deref(OpiBytecode *bc, int var);

/* ==========================================================================
 * VM and evaluation
 */
opi_t
opi_vm(OpiBytecode *bc);

opi_t
opi_apply_partial(opi_t f, int nargs);

/*
 * Function application.
 *
 * Use this one to apply function, not a opi_fn_apply(). This function
 * handles both currying and "over-application".
 */
static inline opi_t
opi_apply(opi_t f, size_t nargs)
{
  if (opi_test_arity(opi_fn_get_arity(f), nargs))
    return opi_fn_apply(f, nargs);
  else
    return opi_apply_partial(f, nargs);
}

/* ==========================================================================
 * Misc
 */
#define OPI_BEGIN_FN()                             \
  opi_t opi_this_arg[opi_nargs];                   \
  struct { int nargs, iarg; opi_t ret; } opi_this; \
  opi_this.nargs = opi_nargs;                      \
  opi_this.iarg = 0;                               \
  for (size_t i = 0; i < opi_nargs; ++i)           \
    opi_inc_rc(opi_this_arg[i] = opi_pop());

#define OPI_UNREF_ARGS()                     \
  for (int i = 0; i < opi_this.nargs; ++i) { \
    if (opi_this_arg[i])                     \
      opi_unref(opi_this_arg[i]);            \
  }

#define OPI_THROW(error_string)                     \
  do {                                              \
    OPI_UNREF_ARGS();                               \
    return opi_undefined(opi_symbol(error_string)); \
  } while (0)

#define OPI_ARG(ident, arg_type)                           \
  opi_assert(opi_this.iarg < opi_this.nargs);              \
  opi_t ident = opi_this_arg[opi_this.iarg++];             \
  if (arg_type && opi_unlikely(ident->type != arg_type)) { \
    OPI_UNREF_ARGS()                                       \
    OPI_THROW("type-error");                               \
  }

#define OPI_RETURN(return_value)             \
  do {                                       \
    opi_inc_rc(opi_this.ret = return_value); \
    OPI_UNREF_ARGS()                         \
    opi_dec_rc(opi_this.ret);                \
    return opi_this.ret;                     \
  } while (0)

static inline void
opi_drop_args(int nargs)
{
  // increment all arguments
  for (int i = 0; i < nargs; ++i)
    opi_inc_rc(opi_get(i + 1));
  // unref all arguments
  for (int i = 0; i < nargs; ++i)
    opi_unref(opi_get(i + 1));
  opi_sp -= nargs;
}

#define OPI_DEF(name, body...) \
  opi_t                     \
  name(void)                \
  {                         \
    OPI_BEGIN_FN()          \
    body                    \
    opi_return(opi_nil);    \
  }

#define opi_arg OPI_ARG
#define opi_throw OPI_THROW
#define opi_return OPI_RETURN

static inline uint32_t
opi_flp2_u32(uint32_t x)
{
  x = x | (x >> 1);
  x = x | (x >> 2);
  x = x | (x >> 4);
  x = x | (x >> 8);
  x = x | (x >> 16);
  return x - (x >> 1);
}

static inline uint64_t
opi_flp2_u64(uint64_t x)
{
  x = x | (x >> 1);
  x = x | (x >> 2);
  x = x | (x >> 4);
  x = x | (x >> 8);
  x = x | (x >> 16);
  x = x | (x >> 32);
  return x - (x >> 1);
}

static inline uint32_t
opi_cep2_u32(uint32_t x)
{
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return ++x;
}

static inline uint64_t
opi_cep2_u64(uint64_t x)
{
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x |= x >> 32;
  return ++x;
}

int
opi_find_string(const char *str, char* const arr[], int n);

#ifdef __cplusplus
}
#endif

#endif
