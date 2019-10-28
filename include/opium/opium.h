#ifndef OPIUM_H
#define OPIUM_H

#include "opium/utility.h"

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

typedef struct opi_type *opi_type_t;
typedef struct opi_header *opi_t;

typedef uintptr_t opi_rc_t;

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

struct opi_bytecode;


void
opi_allocators_init(void);

void
opi_allocators_cleanup(void);

void*
opi_alloc_number(void);

void*
opi_alloc_pair(void);

void*
opi_alloc_string(void);

void*
opi_alloc_lazy(void);

void
opi_free_number(void* ptr);

void
opi_free_pair(void* ptr);

void
opi_free_string(void* ptr);

void
opi_free_lazy(void *ptr);


/* ==========================================================================
 * Type
 */
#define OPI_TYPE_NAME_MAX 255

extern
opi_type_t opi_type_type;

void
opi_type_init(void);

void
opi_type_cleanup(void);

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
 * trait
 */
struct opi_trait;

struct opi_trait*
opi_trait(opi_t deflt);

void
opi_trait_delete(struct opi_trait *t);

int
opi_trait_get_arity(struct opi_trait *t);

opi_t
opi_trait_get_default(struct opi_trait *t);

void
opi_trait_set_default(struct opi_trait *t, opi_t f);

void
opi_trait_impl(struct opi_trait *t, opi_type_t type, opi_t fn);

opi_t
opi_trait_find(struct opi_trait *t, opi_type_t type);

opi_t
opi_trait_into_generic(struct opi_trait *trait, const char *name);

/* ==========================================================================
 * Cell
 */
struct opi_header {
  struct opi_type *type;
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
  if (--x->rc == 0)
    opi_delete(x);
}

size_t
opi_hashof(opi_t x);

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
 * Number
 */
struct opi_number {
  struct opi_header header;
  long double val;
};

extern
opi_type_t opi_number_type;

void
opi_number_init(void);

void
opi_number_cleanup(void);

static inline opi_t
opi_number(long double x)
{
  struct opi_number *num = opi_alloc_number();
  opi_init_cell(num, opi_number_type);
  num->val = x;
  return (opi_t)num;
}

static inline long double
opi_number_get_value(opi_t cell)
{ return opi_as(cell, struct opi_number).val; }

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
struct opi_undefined {
  struct opi_header header;
  opi_t what;
};

extern
opi_type_t opi_undefined_type;

void
opi_undefined_init(void);

void
opi_undefined_cleanup(void);

opi_t
opi_undefined(opi_t what);

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
struct opi_string {
  struct opi_header header;
  char *str;
  size_t size;
};

extern
opi_type_t opi_string_type;

void
opi_string_init(void);

void
opi_string_cleanup(void);

opi_t
opi_string(const char *str);

opi_t
opi_string2(const char *str, size_t len);

opi_t
opi_string_move(char *str);

opi_t
opi_string_move2(char *str, size_t len);

opi_t
opi_string_from_char(char c);

const char*
opi_string_get_value(opi_t x);

size_t
opi_string_get_length(opi_t x);

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
struct opi_pair {
  struct opi_header header;
  opi_t car, cdr;
  size_t len;
};

extern
opi_type_t opi_pair_type;

void
opi_pair_init(void);

void
opi_pair_cleanup(void);

static inline opi_t
opi_cons(opi_t car, opi_t cdr)
{
  struct opi_pair *p = opi_alloc_pair();
  opi_inc_rc(p->car = car);
  opi_inc_rc(p->cdr = cdr);
  p->len = cdr->type == opi_pair_type ? opi_as(cdr, struct opi_pair).len + 1 : 1;
  opi_init_cell(p, opi_pair_type);
  return (opi_t)p;
}

static inline opi_t
opi_car(opi_t x)
{ return opi_as(x, struct opi_pair).car; }

static inline opi_t
opi_cdr(opi_t x)
{ return opi_as(x, struct opi_pair).cdr; }

static inline size_t
opi_length(opi_t x)
{
  if (x->type == opi_pair_type)
    return opi_as(x, struct opi_pair).len;
  return 0;
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
opi_table(void);

opi_t
opi_table_at(opi_t tab, opi_t key, opi_t *err);

int
opi_table_insert(opi_t tab, opi_t key, opi_t val, int replace, opi_t *err);

/* ==========================================================================
 * Port
 */
extern
opi_type_t opi_iport_type, opi_oport_type; 

extern
opi_t opi_stdin, opi_stdout, opi_stderr;

void
opi_port_init(void);

void
opi_port_cleanup(void);

opi_t
opi_oport(FILE *fs, int (*close)(FILE*));

opi_t
opi_iport(FILE *fs, int (*close)(FILE*));

FILE*
opi_port_get_filestream(opi_t x);

static inline int
opi_is_port(opi_t x)
{ return (x->type == opi_iport_type) | (x->type == opi_oport_type); }

/* ==========================================================================
 * Fn
 */
typedef opi_t (*opi_fn_handle_t)(void);

struct opi_fn {
  struct opi_header header;

  char *name;
  opi_fn_handle_t handle;
  void *data;
  void (*delete)(struct opi_fn *self);
  int arity;
};

void
opi_fn_delete(struct opi_fn *fn);

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
opi_fn_set_data(opi_t cell, void *data, void (*delete)(struct opi_fn *self));

int
opi_fn_get_arity(opi_t cell);

void*
opi_fn_get_data(opi_t cell);

opi_fn_handle_t
opi_fn_get_handle(opi_t cell);

const char*
opi_fn_get_name(opi_t f);

opi_t
opi_fn_apply(opi_t cell, size_t nargs);

static inline int
opi_test_arity(int arity, int nargs)
{ return ((arity < 0) & (nargs >= -(1 + arity))) | (arity == nargs); }

/* ==========================================================================
 * Lazy
 */
extern
opi_type_t opi_lazy_type;

struct opi_lazy {
  struct opi_header header;
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
  struct opi_lazy *lazy = opi_as_ptr(x);
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
 * Blob
 */
extern
opi_type_t opi_blob_type;

void
opi_blob_init(void);

void
opi_blob_cleanup(void);

opi_t
opi_blob(void *data, size_t size);

const void*
opi_blob_get_data(opi_t x);

size_t
opi_blob_get_size(opi_t x);

void*
opi_blob_drain(opi_t x);

/* ==========================================================================
 * AST
 */
enum opi_ast_tag {
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
  OPI_AST_TRAIT,
  OPI_AST_IMPL,
  OPI_AST_RETURN,
};

struct opi_ast {
  enum opi_ast_tag tag;
  union {
    opi_t cnst;
    char *var;
    struct { struct opi_ast *fn, **args; size_t nargs; } apply;
    struct { char **args; size_t nargs; struct opi_ast *body; } fn;
    struct { char **vars; struct opi_ast **vals; size_t n; struct opi_ast *body; } let;
    struct { struct opi_ast *test, *then, *els; } iff;
    struct { struct opi_ast **exprs; size_t n; int drop; char *namespace; } block;
    char *load;
    struct { char *type, **vars, **fields; size_t n; struct opi_ast *then, *els, *expr; } match;
    struct { char *typename, **fields; size_t nfields; } strct;
    struct { char *old, *new; } use;
    struct { char *name; struct opi_ast *deflt; } trait;
    struct { char *traitname, *typename; struct opi_ast *fn; } impl;
    struct opi_ast *ret;
  };
};

struct opi_ast*
opi_parse(FILE *in);

struct opi_ast*
opi_parse_string(const char *str);

void
opi_ast_delete(struct opi_ast *node);

struct opi_ast*
opi_ast_const(opi_t x);

struct opi_ast*
opi_ast_var(const char *name);

struct opi_ast*
opi_ast_use(const char *old, const char *new);

struct opi_ast*
opi_ast_apply(struct opi_ast *fn, struct opi_ast **args, size_t nargs);

struct opi_ast*
opi_ast_fn(char **args, size_t nargs, struct opi_ast *body);

struct opi_ast*
opi_ast_let(char **vars, struct opi_ast **vals, size_t n, struct opi_ast *body);

struct opi_ast*
opi_ast_if(struct opi_ast *test, struct opi_ast *then, struct opi_ast *els);

struct opi_ast*
opi_ast_fix(char **vars, struct opi_ast **lams, size_t n, struct opi_ast *body);

struct opi_ast*
opi_ast_block(struct opi_ast **exprs, size_t n);

void
opi_ast_block_set_drop(struct opi_ast *block, int drop);

void
opi_ast_block_set_namespace(struct opi_ast *block, const char *namespace);

void
opi_ast_block_prepend(struct opi_ast *block, struct opi_ast *node);

void
opi_ast_block_append(struct opi_ast *block, struct opi_ast *node);

struct opi_ast*
opi_ast_load(const char *path);

struct opi_ast*
opi_ast_match(const char *type, char **vars, char **fields, size_t n,
    struct opi_ast *expr, struct opi_ast *then, struct opi_ast *els);

struct opi_ast*
opi_ast_struct(const char *typename, char** fields, size_t nfields);

struct opi_ast*
opi_ast_trait(const char *name, struct opi_ast *deflt);

struct opi_ast*
opi_ast_impl(const char *traitname, const char *typename, struct opi_ast *fn);

struct opi_ast*
opi_ast_and(struct opi_ast *x, struct opi_ast *y);

struct opi_ast*
opi_ast_or(struct opi_ast *x, struct opi_ast *y);

struct opi_ast*
opi_ast_eor(struct opi_ast *try, struct opi_ast *els);

struct opi_ast*
opi_ast_return(struct opi_ast *val);

/* ==========================================================================
 * Context
 */
struct opi_context {
  struct opi_ptrvec types;
  struct opi_ptrvec bc;
  struct opi_strvec dl_paths;
  struct opi_ptrvec dls;
};

void
opi_context_init(struct opi_context *ctx);

void
opi_context_destroy(struct opi_context *ctx);

void
opi_context_add_type(struct opi_context *ctx, opi_type_t type);

void
opi_context_drain_bytecode(struct opi_context *ctx, struct opi_bytecode *bc);

void
opi_context_add_dl(struct opi_context *ctx, const char *path, void *dl);

void*
opi_context_find_dl(struct opi_context *ctx, const char *path);

int
opi_is_dl(const char *path);

/* ==========================================================================
 * IR
 */
struct opi_alist {
  struct opi_strvec keys, vals;
};

void
opi_alist_init(struct opi_alist *a);

void
opi_alist_destroy(struct opi_alist *a);

size_t
opi_alist_get_size(struct opi_alist *a);

void
opi_alist_push(struct opi_alist *a, const char *var, const char *map);

void
opi_alist_pop(struct opi_alist *a, size_t n);

struct opi_builder {
  int is_derived;

  struct opi_context *ctx;

  int frame_offset;
  struct opi_strvec decls;
  struct opi_alist *alist;

  struct opi_strvec *srcdirs;
  struct opi_strvec *loaded;
  struct opi_strvec *load_state;

  struct opi_strvec *const_names;
  struct opi_ptrvec *const_vals;

  struct opi_strvec *type_names;
  struct opi_ptrvec *types;

  struct opi_strvec *trait_names;
  struct opi_ptrvec *traits;
};

void
opi_builder_init(struct opi_builder *bldr, struct opi_context *ctx);

void
opi_builder_init_derived(struct opi_builder *bldr, struct opi_builder *parent);

void
opi_builder_destroy(struct opi_builder *bldr);

void
opi_builtins(struct opi_builder *bldr);

void
opi_builder_load_dl(struct opi_builder *bldr, void *dl);

void
opi_builder_push_decl(struct opi_builder *bldr, const char *var);

void
opi_builder_pop_decl(struct opi_builder *bldr);

void
opi_builder_capture(struct opi_builder *bldr, const char *var);

const char*
opi_builder_assoc(struct opi_builder *bldr, const char *var);

const char*
opi_builder_try_assoc(struct opi_builder *bldr, const char *var);

struct opi_build_scope { size_t nvars1, ntypes1, ntraits1, vasize1; };

void
opi_builder_begin_scope(struct opi_builder *bldr, struct opi_build_scope *scp);

void
opi_builder_drop_scope(struct opi_builder *bldr, struct opi_build_scope *scp);

void
opi_builder_add_source_directory(struct opi_builder *bldr, const char *path);

char*
opi_builder_find_path(struct opi_builder *bldr, const char *path, char *fullpath);

void
opi_builder_add_type(struct opi_builder *bldr, const char *name, opi_type_t type);

void
opi_builder_add_trait(struct opi_builder *bldr, const char *name, struct opi_trait *t);

opi_type_t
opi_builder_find_type(struct opi_builder *bldr, const char *typename);

struct opi_trait*
opi_builder_find_trait(struct opi_builder *bldr, const char *traitname);

void
opi_builder_def_const(struct opi_builder *bldr, const char *name, opi_t val);

void
opi_builder_def_type(struct opi_builder *bldr, const char *name, opi_type_t type);

struct opi_ir*
opi_builder_build_ir(struct opi_builder *bldr, struct opi_ast *ast);

void
opi_build(struct opi_builder *bldr, struct opi_ast *ast, struct opi_bytecode* bc);

enum opi_ir_tag {
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
};

struct opi_ir {
  enum opi_ir_tag tag;
  union {
    opi_t cnst;
    size_t var;
    struct { struct opi_ir *fn, **args; size_t nargs; } apply;
    struct { struct opi_ir **caps; size_t ncaps, nargs; struct opi_ir *body; } fn;
    struct { struct opi_ir **vals; size_t n; struct opi_ir *body; } let;
    struct { struct opi_ir *test, *then, *els; } iff;
    struct { struct opi_ir **exprs; size_t n; int drop; } block;
    struct { opi_type_t type; size_t *offs, n; struct opi_ir *expr, *then, *els; } match;
    struct opi_ir *ret;
  };
};

void
opi_ir_delete(struct opi_ir *node);

void
opi_ir_emit(struct opi_ir *ir, struct opi_bytecode *bc);

struct opi_ir*
opi_ir_const(opi_t x);

struct opi_ir*
opi_ir_var(size_t offs);

struct opi_ir*
opi_ir_apply(struct opi_ir *fn, struct opi_ir **args, size_t nargs);

struct opi_ir*
opi_ir_fn(struct opi_ir **caps, size_t ncaps, size_t nargs, struct opi_ir *body);

struct opi_ir*
opi_ir_let(struct opi_ir **vals, size_t n, struct opi_ir *body);

struct opi_ir*
opi_ir_if(struct opi_ir *test, struct opi_ir *then, struct opi_ir *els);

struct opi_ir*
opi_ir_fix(struct opi_ir **vals, size_t n, struct opi_ir *body);

struct opi_ir*
opi_ir_block(struct opi_ir **exprs, size_t n);

static void
opi_ir_block_set_drop(struct opi_ir *block, int drop)
{ block->block.drop = drop; }

struct opi_ir*
opi_ir_match(opi_type_t type, size_t *offs, size_t n, struct opi_ir *expr,
    struct opi_ir *then, struct opi_ir *els);

struct opi_ir*
opi_ir_return(struct opi_ir *val);

/* ==========================================================================
 * Bytecode
 */
enum opi_opc {
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
#define OPI_FINFN_ARG_DATA(insn) ((struct opi_insn_fn_data*)(insn)->ptr[1])

  OPI_OPC_IF,
#define OPI_IF_REG_TEST(insn) (insn)->reg[0]
#define OPI_IF_ARG_ELSE(insn) (struct opi_insn*)(insn)->ptr[1]

  OPI_OPC_JMP,
#define OPI_JMP_ARG_TO(insn) (struct opi_insn*)(insn)->ptr[0]

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
};

struct opi_insn {
  struct opi_insn *prev;
  struct opi_insn *next;

  enum opi_opc opc;
  union {
    uintptr_t reg[3];
    void *ptr[3];
  };
};

static inline void
opi_insn_chain(struct opi_insn *prev, struct opi_insn *insn, struct opi_insn *next)
{
  insn->prev = prev;
  insn->next = next;

  if (prev)
    prev->next = insn;
  if (next)
    next->prev = insn;
}

void
opi_insn_delete1(struct opi_insn *insn);

void
opi_insn_delete(struct opi_insn *insn);

void
opi_insn_dump1(struct opi_insn *insn, FILE *out);

void
opi_insn_dump(struct opi_insn *insn, FILE *out);

int
opi_insn_is_using(struct opi_insn *insn, int vid);

int
opi_insn_is_killing(struct opi_insn *insn, int vid);

int
opi_insn_is_creating(struct opi_insn *insn, int vid);

int
opi_insn_is_end(struct opi_insn *insn);

struct opi_insn*
opi_insn_nop();

struct opi_insn*
opi_insn_const(int ret, opi_t cell);

struct opi_insn*
opi_insn_apply(int ret, int fn, size_t nargs, int tc);

struct opi_insn*
opi_insn_ret(int val);

struct opi_insn*
opi_insn_push(int val);

struct opi_insn*
opi_insn_pop(size_t n);

struct opi_insn*
opi_insn_ldcap(int out, int idx);

struct opi_insn*
opi_insn_param(int out, int offs);

struct opi_insn*
opi_insn_test(int out, int in);

struct opi_insn*
opi_insn_if(int test);

static inline int
opi_insn_if_is_analysed(struct opi_insn *iff)
{ return iff->reg[2] & 0x1; }

static inline void
opi_insn_if_set_analysed(struct opi_insn *iff)
{ iff->reg[2] |= 0x1; }

static inline int
opi_insn_if_is_then_end(struct opi_insn *iff)
{ return iff->reg[2] & 0x2; }

static inline void
opi_insn_if_set_then_end(struct opi_insn *iff, int is_end)
{  iff->reg[2] |= 0x2; }

static inline int
opi_insn_if_is_else_end(struct opi_insn *iff)
{ return iff->reg[2] & 0x4; }

static inline void
opi_insn_if_set_else_end(struct opi_insn *iff, int is_end)
{  iff->reg[2] |= 0x4; }

struct opi_insn*
opi_insn_jmp(struct opi_insn *to);

struct opi_insn*
opi_insn_phi(int reg);

struct opi_insn*
opi_insn_incrc(int cell);

struct opi_insn*
opi_insn_decrc(int cell);

struct opi_insn*
opi_insn_drop(int cell);

struct opi_insn*
opi_insn_unref(int cell);

struct opi_insn_fn_data {
  struct opi_bytecode *bc;
  int arity;
  int ncaps;
  int caps[];
};

struct opi_insn*
opi_insn_alcfn(int out);

struct opi_insn*
opi_insn_finfn(int cell, int arity, struct opi_bytecode *bc, int *cap, size_t ncap);

struct opi_insn*
opi_insn_begscp(size_t n);

struct opi_insn*
opi_insn_endscp(int *cells, size_t n);

struct opi_insn*
opi_insn_testty(int out, int cell, opi_type_t type);

struct opi_insn*
opi_insn_ldfld(int out, int cell, size_t offs);

struct opi_insn*
opi_insn_guard(int in);

enum opi_val_type {
  // Value is "born" in local scope with RC petentionaly set to zero at the
  // beginning.
  OPI_VAL_LOCAL,

  // External value with already (and for sure) non-zero RC.
  OPI_VAL_GLOBAL,

  OPI_VAL_PHI,

  OPI_VAL_BOOL,
};

struct opi_val_info {
  enum opi_val_type type;
};

struct opi_bytecode {
  size_t nvals;
  struct opi_val_info *vinfo;
  size_t vinfo_cap;
  struct opi_insn *head;
  struct opi_insn *tail;
  struct opi_insn *point;
};

void
opi_bytecode_init(struct opi_bytecode *bc);

void
opi_bytecode_destroy(struct opi_bytecode *bc);

struct opi_insn*
opi_bytecode_drain(struct opi_bytecode *bc);

void
opi_bytecode_fix_lifetimes(struct opi_bytecode *bc);

static void
opi_bytecode_finalize(struct opi_bytecode *bc)
{
  opi_bytecode_fix_lifetimes(bc);
}

int
opi_bytecode_new_val(struct opi_bytecode *bc, enum opi_val_type vtype);

static inline int
opi_bytecode_value_is_local(struct opi_bytecode *bc, int vid)
{ return bc->vinfo[vid].type == OPI_VAL_LOCAL; }

static inline int
opi_bytecode_value_is_global(struct opi_bytecode *bc, int vid)
{ return bc->vinfo[vid].type == OPI_VAL_GLOBAL; }

void
opi_bytecode_append(struct opi_bytecode *bc, struct opi_insn *insn);

void
opi_bytecode_prepend(struct opi_bytecode *bc, struct opi_insn *insn);

void
opi_bytecode_write(struct opi_bytecode *bc, struct opi_insn *insn);

int
opi_bytecode_while(struct opi_bytecode *bc, int (*test)(struct opi_insn *insn, void *data), void *data);

struct opi_insn*
opi_bytecode_find_creating(struct opi_bytecode *bc, int vid);

int
opi_bytecode_const(struct opi_bytecode *bc, opi_t cell);

int
opi_bytecode_apply(struct opi_bytecode *bc, int fn, size_t nargs, ...);

int
opi_bytecode_apply_tailcall(struct opi_bytecode *bc, int fn, size_t nargs, ...);

int
opi_bytecode_apply_arr(struct opi_bytecode *bc, int fn, size_t nargs, const int *args);

int
opi_bytecode_apply_tailcall_arr(struct opi_bytecode *bc, int fn, size_t nargs, const int *args);

void
opi_bytecode_ret(struct opi_bytecode *bc, int val);

void
opi_bytecode_push(struct opi_bytecode *bc, int val);

void
opi_bytecode_pop(struct opi_bytecode *bc, size_t n);

int
opi_bytecode_ldcap(struct opi_bytecode *bc, size_t idx);

int
opi_bytecode_param(struct opi_bytecode *bc, size_t offs);

int
opi_bytecode_alcfn(struct opi_bytecode *bc, enum opi_val_type valtype);

void
opi_bytecode_finfn(struct opi_bytecode *bc,
    int cell, int arity, struct opi_bytecode *body, int *cap, size_t ncap);

int
opi_bytecode_test(struct opi_bytecode *bc, int in);

struct opi_if { struct opi_insn *iff, *els; };
void
opi_bytecode_if(struct opi_bytecode *bc, int test, struct opi_if *iff);
void
opi_bytecode_if_else(struct opi_bytecode *bc, struct opi_if *iff);
void
opi_bytecode_if_end(struct opi_bytecode *bc, struct opi_if *iff);

int
opi_bytecode_phi(struct opi_bytecode *bc);

void
opi_bytecode_dup(struct opi_bytecode *bc, int dst, int src);

void
opi_bytecode_incrc(struct opi_bytecode *bc, int cell);

void
opi_bytecode_decrc(struct opi_bytecode *bc, int cell);

void
opi_bytecode_drop(struct opi_bytecode *bc, int cell);

void
opi_bytecode_unref(struct opi_bytecode *bc, int cell);

void
opi_bytecode_begscp(struct opi_bytecode *bc, size_t n);

void
opi_bytecode_endscp(struct opi_bytecode *bc, int *cells, size_t n);

int
opi_bytecode_testty(struct opi_bytecode *bc, int cell, opi_type_t type);

int
opi_bytecode_ldfld(struct opi_bytecode *bc, int cell, size_t offs);

void
opi_bytecode_guard(struct opi_bytecode *bc, int cell);

#define OPI_VM_REG_MAX 0x200
opi_t
opi_vm(struct opi_bytecode *bc);

#endif
