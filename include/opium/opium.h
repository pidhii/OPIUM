#ifndef OPIUM_H
#define OPIUM_H

#include "codeine/vec.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

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

#define OPI_TRACE stderr
#define opi_trace(fmt, ...)                              \
  do {                                                   \
    fprintf(OPI_TRACE, "[\x1b[38;5;9m opium \x1b[0m] "); \
    fprintf(OPI_TRACE, fmt, ##__VA_ARGS__);              \
  } while (0)

__attribute__((noreturn)) void
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
extern
int opi_error;

typedef struct OpiType_s OpiType;
typedef OpiType *opi_type_t;

typedef struct OpiHeader_s OpiHeader;
typedef OpiHeader *opi_t;

typedef uintptr_t opi_rc_t;

typedef struct OpiLocation_s {
  char *path;
  int fl, fc, ll, lc;
} OpiLocation;

OpiLocation*
opi_location(const char *path, int fl, int fc, int ll, int lc);

void
opi_location_delete(OpiLocation *loc);

OpiLocation*
opi_location_copy(const OpiLocation *loc);

int
opi_show_location(FILE *out, const char *path, int fc, int fl, int lc, int ll);

void
opi_lexer_init(void);

void
opi_lexer_cleanup(void);

void
opi_init(void);

void
opi_cleanup(void);

extern
opi_t *opi_sp;
extern
opi_t opi_current_fn;
extern
size_t opi_nargs;

typedef struct OpiBytecode_s OpiBytecode;

/* ==========================================================================
 * Type
 */
#define OPI_TYPE_NAME_MAX 255

extern
opi_type_t opi_type_type;

opi_type_t
opi_type(const char *name);

void
opi_type_delete(opi_type_t ty);

void
opi_type_set_delete_cell(opi_type_t ty, void (*fn)(opi_type_t,opi_t));

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
opi_type_set_fields(opi_type_t ty, size_t offs, char **fields, size_t n);

const char*
opi_type_get_name(opi_type_t ty);

int
opi_type_get_field_idx(opi_type_t ty, const char *field);

size_t
opi_type_get_field_offset(opi_type_t ty, size_t field_idx);

void*
opi_type_get_data(opi_type_t ty);

/* ==========================================================================
 * Cell
 */
struct OpiHeader_s {
  OpiType *type;
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
{
  return x == y;
}

int
opi_eq(opi_t x, opi_t y);

int
opi_equal(opi_t x, opi_t y);

static inline void
opi_init_cell(void* x_, opi_type_t ty)
{
  opi_t x = x_;
  x->type = ty;
  x->rc = 0;
}

void
opi_delete(opi_t x);

static inline opi_rc_t
opi_inc_rc(opi_t x)
{
  return ++x->rc;
}

static inline opi_rc_t
opi_dec_rc(opi_t x)
{
  return --x->rc;
}

static inline void
opi_drop(opi_t x)
{
  if (x->rc == 0)
    opi_delete(x);
}

static inline void
opi_unref(opi_t x)
{
  if (--x->rc == 0)
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

typedef struct OpiH2w_s {
  OpiHeader header;
  opi_word_t w[2];
} OpiH2w;

void*
opi_h2w();

void
opi_h2w_free(void *ptr);

typedef struct OpiH3w_s {
  OpiHeader header;
  opi_word_t w[3];
} OpiH3w;

void*
opi_h3w();

void
opi_h3w_free(void *ptr);

/* ==========================================================================
 * Stack
 */
static inline void
opi_push(opi_t x)
{
  *opi_sp++ = x;
}

static inline opi_t
opi_pop()
{
  return *--opi_sp;
}

static inline void
opi_popn(size_t n)
{
  opi_sp -= n;
}

static inline opi_t
opi_get(size_t offs)
{
  return *(opi_sp - offs);
}

/* ==========================================================================
 * Number
 */
typedef struct OpiNumber_s {
  OpiHeader header;
  long double val;
} OpiNumber;

extern
opi_type_t opi_number_type;

void
opi_number_init(void);

void
opi_number_cleanup(void);

static inline opi_t
opi_number(long double x)
{
  OpiNumber *num = opi_h2w();
  opi_init_cell(num, opi_number_type);
  num->val = x;
  return (opi_t)num;
}

static inline long double
opi_number_get_value(opi_t cell)
{ return opi_as(cell, OpiNumber).val; }

/* ==========================================================================
 * Symbol
 */
extern
opi_type_t opi_symbol_type;

void
opi_symbol_init(void);

void
opi_symbol_cleanup(void);

opi_t
opi_symbol(const char *str);

const char*
opi_symbol_get_string(opi_t x);

/* ==========================================================================
 * Undefined
 */
typedef cod_vec(OpiLocation*) opi_trace_t;
typedef struct OpiUndefined_s {
  OpiHeader header;
  opi_t what;
  opi_trace_t* trace;
} OpiUndefined;

extern
opi_type_t opi_undefined_type;

void
opi_undefined_init(void);

void
opi_undefined_cleanup(void);

opi_t
opi_undefined(opi_t what);

static inline opi_t
opi_undefined_get_what(opi_t x)
{
  return opi_as(x, OpiUndefined).what;
}

static inline opi_trace_t*
opi_undefined_get_trace(opi_t x)
{
  return opi_as(x, OpiUndefined).trace;
}

/* ==========================================================================
 * Nil
 */
extern
opi_type_t opi_null_type;

void
opi_nil_init(void);

void
opi_nil_cleanup(void);

extern
opi_t opi_nil;

/* ==========================================================================
 * String
 */
typedef struct OpiString_s {
  OpiHeader header;
  char *str;
  size_t len;
} OpiString;

extern
opi_type_t opi_string_type;

void
opi_string_init(void);

void
opi_string_cleanup(void);

opi_t
opi_string_new(const char *str);

opi_t
opi_string_new_with_len(const char *str, size_t len);

opi_t
opi_string_drain(char *str);

opi_t
opi_string_drain_with_len(char *str, size_t len);

opi_t
opi_string_from_char(char c);

const char*
opi_string_get_value(opi_t x);

size_t
opi_string_get_length(opi_t x);

/* ==========================================================================
 * RegEx
 */
extern opi_type_t
opi_regex_type;

#define OPI_OVECTOR_SIZE 0x100

extern int
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
extern
opi_type_t opi_boolean_type;

void
opi_boolean_init(void);

void
opi_boolean_cleanup(void);

extern
opi_t opi_true, opi_false;

/* ==========================================================================
 * Pair
 */
typedef struct OpiPair_s {
  OpiHeader header;
  opi_t car, cdr;
} OpiPair;

extern
opi_type_t opi_pair_type;

void
opi_pair_init(void);

void
opi_pair_cleanup(void);

static inline opi_t
opi_cons(opi_t car, opi_t cdr)
{
  OpiPair *p = opi_h2w();
  opi_inc_rc(p->car = car);
  opi_inc_rc(p->cdr = cdr);
  opi_init_cell(p, opi_pair_type);
  return (opi_t)p;
}

static inline opi_t
opi_car(opi_t x)
{
  return opi_as(x, OpiPair).car;
}

static inline opi_t
opi_cdr(opi_t x)
{
  return opi_as(x, OpiPair).cdr;
}

static inline size_t
opi_length(opi_t x)
{
  size_t len = 0;
  while (x->type == opi_pair_type) {
    len += 1;
    x = opi_cdr(x);
  }
  return len;
}

/* ==========================================================================
 * Table
 */
extern
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
opi_table_insert(opi_t tab, opi_t key, opi_t val, int replace, opi_t *err);

/* ==========================================================================
 * Port
 */
extern
opi_type_t opi_file_type;

extern
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

typedef struct OpiFn_s OpiFn;
struct OpiFn_s {
  OpiHeader header;
  char *name;
  opi_fn_handle_t handle;
  void *data;
  void (*delete)(OpiFn *self);
  intptr_t arity;
};

void
opi_fn_delete(OpiFn *fn);

extern
opi_type_t opi_fn_type;

void
opi_fn_init(void);

void
opi_fn_cleanup(void);

opi_t
opi_fn_alloc();

void
opi_fn_finalize(opi_t fn, const char *name, opi_fn_handle_t f, int arity);

opi_t
opi_fn(const char *name, opi_fn_handle_t f, int arity);

void
opi_fn_set_data(opi_t cell, void *data, void (*delete)(OpiFn *self));

static inline int
opi_fn_get_arity(opi_t cell)
{
  return opi_as(cell, OpiFn).arity;
}

static inline void*
opi_fn_get_data(opi_t cell)
{
  return opi_as(cell, OpiFn).data;
}

static inline opi_fn_handle_t
opi_fn_get_handle(opi_t cell)
{
  return opi_as(cell, OpiFn).handle;
}

static inline const char*
opi_fn_get_name(opi_t f)
{
  return opi_as(f, OpiFn).name;
}

static inline opi_t
opi_fn_apply(opi_t cell, size_t nargs)
{
  OpiFn *fn = opi_as_ptr(cell);
  opi_nargs = nargs;
  opi_current_fn = cell;
  return fn->handle();
}

static inline int
opi_test_arity(int arity, int nargs)
{
  return ((arity < 0) & (nargs >= -(1 + arity))) | (arity == nargs);
}

opi_t
opi_apply_partial(opi_t f, int nargs);

static inline opi_t
opi_apply(opi_t f, size_t nargs)
{
  if (opi_test_arity(opi_fn_get_arity(f), nargs))
    return opi_fn_apply(f, nargs);
  else
    return opi_apply_partial(f, nargs);
}

/* ==========================================================================
 * Lazy
 */
extern
opi_type_t opi_lazy_type;

typedef struct OpiLazy_s {
  OpiHeader header;
  opi_t cell;
  uint_fast8_t is_ready;
} OpiLazy;

void
opi_lazy_init(void);

void
opi_lazy_cleanup(void);

opi_t
opi_lazy(opi_t x);

static inline opi_t
opi_lazy_get_value(opi_t x)
{
  OpiLazy *lazy = opi_as_ptr(x);
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
 * Vectors
 */
typedef struct OpiSVector_s OpiSVector;
typedef struct OpiDVector_s OpiDVector;

extern
opi_type_t opi_svector_type, opi_dvector_type;

void
opi_vectors_init(void);

void
opi_vectors_cleanup(void);

opi_t
opi_svector_new(const float *data, size_t size);

opi_t
opi_dvector_new(const double *data, size_t size);

opi_t
opi_svector_new_moved(float *data, size_t size);

opi_t
opi_dvector_new_moved(double *data, size_t size);

opi_t
opi_svector_new_raw(size_t size);

opi_t
opi_dvector_new_raw(size_t size);

opi_t
opi_svector_new_filled(float fill, size_t size);

opi_t
opi_dvector_new_filled(double fill, size_t size);

const float*
opi_svector_get_data(opi_t x);

const double*
opi_dvector_get_data(opi_t x);

size_t
opi_vector_get_size(opi_t x);

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
    struct { char *type, **fields; OpiAstPattern **subs; size_t n; } unpack;
  };
};

OpiAstPattern*
opi_ast_pattern_new_ident(const char *ident);

OpiAstPattern*
opi_ast_pattern_new_unpack(const char *type, OpiAstPattern **subs, char **fields,
    size_t n);

void
opi_ast_pattern_delete(OpiAstPattern *pattern);

typedef struct OpiAst_s OpiAst;
struct OpiAst_s {
  OpiAstTag tag;
  union {
    opi_t cnst;
    char *var;
    struct { OpiAst *fn, **args; size_t nargs; char eflag; OpiLocation *loc; } apply;
    struct { char **args; size_t nargs; OpiAst *body; } fn;
    struct { char **vars; OpiAst **vals; size_t n; OpiAst *body; } let;
    struct { OpiAst *test, *then, *els; } iff;
    struct { OpiAst **exprs; size_t n; int drop; char *namespace; } block;
    char *load;
    struct { OpiAstPattern *pattern; OpiAst *then, *els, *expr; } match;
    struct { char *typename, **fields; size_t nfields; } strct;
    struct { char *old, *new; } use;
    OpiAst *ret;
    struct { int opc; OpiAst *lhs, *rhs; } binop;
  };
};

typedef struct OpiScanner_s OpiScanner;

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
opi_parse_expr(OpiScanner *scanner);

OpiAst*
opi_parse_string(const char *str);

void
opi_ast_delete(OpiAst *node);

OpiAst*
opi_ast_const(opi_t x);

OpiAst*
opi_ast_var(const char *name);

OpiAst*
opi_ast_use(const char *old, const char *new);

OpiAst*
opi_ast_apply(OpiAst *fn, OpiAst **args, size_t nargs);

OpiAst*
opi_ast_fn(char **args, size_t nargs, OpiAst *body);

OpiAst*
opi_ast_let(char **vars, OpiAst **vals, size_t n, OpiAst *body);

OpiAst*
opi_ast_if(OpiAst *test, OpiAst *then, OpiAst *els);

OpiAst*
opi_ast_fix(char **vars, OpiAst **lams, size_t n, OpiAst *body);

OpiAst*
opi_ast_block(OpiAst **exprs, size_t n);

void
opi_ast_block_set_drop(OpiAst *block, int drop);

void
opi_ast_block_set_namespace(OpiAst *block, const char *namespace);

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
opi_ast_struct(const char *typename, char** fields, size_t nfields);

OpiAst*
opi_ast_and(OpiAst *x, OpiAst *y);

OpiAst*
opi_ast_or(OpiAst *x, OpiAst *y);

OpiAst*
opi_ast_eor(OpiAst *try, OpiAst *els, const char *ename);

OpiAst*
opi_ast_when(OpiAst *test_expr,
    const char *then_bind, OpiAst *then_expr,
    const char *else_bind, OpiAst *else_expr);

OpiAst*
opi_ast_return(OpiAst *val);

OpiAst*
opi_ast_binop(int opc, OpiAst *lhs, OpiAst *rhs);

/* ==========================================================================
 * Context
 */
typedef struct OpiContext_s {
  struct cod_ptrvec types;
  struct cod_ptrvec bc;
  struct cod_strvec dl_paths;
  struct cod_ptrvec dls;
} OpiContext;

void
opi_context_init(OpiContext *ctx);

void
opi_context_destroy(OpiContext *ctx);

void
opi_context_add_type(OpiContext *ctx, opi_type_t type);

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

typedef struct OpiBuilder_s {
  int is_derived;

  OpiContext *ctx;

  int frame_offset;
  struct cod_strvec decls;
  OpiAlist *alist;

  struct cod_strvec *srcdirs;
  struct cod_strvec *loaded;
  struct cod_strvec *load_state;

  struct cod_strvec *const_names;
  struct cod_ptrvec *const_vals;

  struct cod_strvec *type_names;
  struct cod_ptrvec *types;
} OpiBuilder;

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

typedef struct OpiScope_s {
  size_t nvars1, ntypes1, vasize1;
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

opi_type_t
opi_builder_find_type(OpiBuilder *bldr, const char *typename);

void
opi_builder_def_const(OpiBuilder *bldr, const char *name, opi_t val);

int
opi_builder_def_type(OpiBuilder *bldr, const char *name, opi_type_t type);

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
} OpiIrTag;

typedef struct OpiIrPattern_s OpiIrPattern;
struct OpiIrPattern_s {
  OpiPatternTag tag;
  struct { opi_type_t type; OpiIrPattern **subs; size_t *offs, n; } unpack;
};

OpiIrPattern*
opi_ir_pattern_new_ident(void);

OpiIrPattern*
opi_ir_pattern_new_unpack(opi_type_t type, OpiIrPattern **subs, size_t *offs,
    size_t n);

void
opi_ir_pattern_delete(OpiIrPattern *pattern);

typedef struct OpiIr_s OpiIr;
struct OpiIr_s {
  OpiIrTag tag;
  union {
    opi_t cnst;
    size_t var;
    struct { OpiIr *fn, **args; size_t nargs; char eflag; OpiLocation *loc; } apply;
    struct { OpiIr **caps; size_t ncaps, nargs; OpiIr *body; } fn;
    struct { OpiIr **vals; size_t n; OpiIr *body; } let;
    struct { OpiIr *test, *then, *els; } iff;
    struct { OpiIr **exprs; size_t n; int drop; } block;
    struct { OpiIrPattern *pattern; OpiIr *expr, *then, *els; } match;
    OpiIr *ret;
    struct { int opc; OpiIr *lhs, *rhs; } binop;
  };
};

OpiIr*
opi_builder_build_ir(OpiBuilder *bldr, OpiAst *ast);

enum {
  OPI_BUILD_DEFAULT,
  OPI_BUILD_EXPORT,
};

OpiBytecode*
opi_build(OpiBuilder *bldr, OpiAst *ast, int mode);

void
opi_ir_delete(OpiIr *node);

void
opi_ir_emit(OpiIr *ir, OpiBytecode *bc);

OpiIr*
opi_ir_const(opi_t x);

OpiIr*
opi_ir_var(size_t offs);

OpiIr*
opi_ir_apply(OpiIr *fn, OpiIr **args, size_t nargs);

OpiIr*
opi_ir_fn(OpiIr **caps, size_t ncaps, size_t nargs, OpiIr *body);

OpiIr*
opi_ir_let(OpiIr **vals, size_t n, OpiIr *body);

OpiIr*
opi_ir_if(OpiIr *test, OpiIr *then, OpiIr *els);

OpiIr*
opi_ir_fix(OpiIr **vals, size_t n, OpiIr *body);

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
  OPI_OPC_CONS,
  OPI_OPC_ADD, OPI_OPC_SUB, OPI_OPC_MUL, OPI_OPC_DIV, OPI_OPC_MOD,
  OPI_OPC_NUMEQ, OPI_OPC_NUMNE, OPI_OPC_LT, OPI_OPC_GT, OPI_OPC_LE, OPI_OPC_GE,
  OPI_OPC_BINOP_END,
#define OPI_BINOP_REG_OUT(insn) (insn)->reg[0]
#define OPI_BINOP_REG_LHS(insn) (insn)->reg[1]
#define OPI_BINOP_REG_RHS(insn) (insn)->reg[2]

  OPI_OPC_VAR,
#define OPI_VAR_REG(insn) (insn)->reg[0]
  OPI_OPC_SET,
#define OPI_SET_REG(insn) (insn)->reg[0]
#define OPI_SET_ARG_VAL(insn) (insn)->reg[1]
} OpiOpc;

typedef struct OpiFlatInsn_s {
  OpiOpc opc;
  union {
    uintptr_t reg[3];
    void *ptr[3];
  };
} OpiFlatInsn;

typedef struct OpiInsn_s OpiInsn;
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
opi_insn_is_creating(OpiInsn *insn, int vid);

int
opi_insn_is_end(OpiInsn *insn);

OpiInsn*
opi_insn_nop();

OpiInsn*
opi_insn_const(int ret, opi_t cell);

OpiInsn*
opi_insn_apply(int ret, int fn, size_t nargs, int tc);

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
  int arity;
  int ncaps;
  int caps[];
} OpiFnInsnData;

OpiInsn*
opi_insn_alcfn(int out);

OpiInsn*
opi_insn_finfn(int cell, int arity, OpiBytecode *bc, int *cap, size_t ncap);

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
} OpiValInfo;

struct OpiBytecode_s {
  size_t nvals;
  OpiValInfo *vinfo;
  size_t vinfo_cap;
  OpiInsn *head;
  OpiInsn *tail;
  OpiInsn *point;
  OpiFlatInsn *tape;
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
opi_bytecode_append(OpiBytecode *bc, OpiInsn *insn);

void
opi_bytecode_prepend(OpiBytecode *bc, OpiInsn *insn);

void
opi_bytecode_write(OpiBytecode *bc, OpiInsn *insn);

int
opi_bytecode_while(OpiBytecode *bc, int (*test)(OpiInsn *insn, void *data), void *data);

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
opi_bytecode_finfn(OpiBytecode *bc, int cell, int arity, OpiBytecode *body, int *cap, size_t ncap);

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

#define OPI_VM_REG_MAX 0x200
opi_t
opi_vm(OpiBytecode *bc);

#define OPI_FN()                                                         \
  struct { int nargs, iarg; opi_t arg[opi_nargs]; opi_t ret; } opi_this; \
  opi_this.nargs = opi_nargs;                                            \
  opi_this.iarg = 0;                                                     \
  for (size_t i = 0; i < opi_nargs; ++i)                                 \
    opi_inc_rc(opi_this.arg[i] = opi_pop());

#define OPI_UNREF_ARGS()                   \
  for (int i = 0; i < opi_this.nargs; ++i) \
    opi_unref(opi_this.arg[i]);

#define OPI_THROW(error_string)                     \
  do {                                              \
    OPI_UNREF_ARGS();                               \
    return opi_undefined(opi_symbol(error_string)); \
  } while (0)

#define OPI_ARG(ident, arg_type)                           \
  opi_assert(opi_this.iarg < opi_this.nargs);              \
  opi_t ident = opi_this.arg[opi_this.iarg++];             \
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

#define OPI_NUM(x) opi_number_get_value(x)
#define OPI_STR(x) opi_string_get_value(x)
#define OPI_STRLEN(x) opi_string_get_length(x)
#define OPI_SYM(x) opi_symbol_get_string(x)
#define OPI_SVEC(x) opi_svector_get_data(x)
#define OPI_DVEC(x) opi_dvector_get_data(x)
#define OPI_VECSZ(x) opi_vector_get_size(x)

#endif
