#ifndef OPIUM_H
#define OPIUM_H

#include "opium/utility.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define opi_assert(expr) assert(expr)

typedef struct opi_type *opi_type_t;
typedef struct opi_cell *opi_t;

typedef uintptr_t opi_rc_t;

void
opi_init(void);


/* ==========================================================================
 * Type
 */
#define OPI_TYPE_NAME_MAX 255
struct opi_type {
  char name[OPI_TYPE_NAME_MAX + 1];

  void (*destroy_cell)(opi_type_t ty, opi_t cell);

  void *data;
  void (*destroy_data)(opi_type_t ty);

  void (*show)(opi_type_t ty, opi_t x, FILE *out);
};

opi_type_t
opi_type(const char *name);

void
opi_type_set_destroy_cell(opi_type_t ty, void (*fn)(opi_type_t,opi_t));

void
opi_type_set_data(opi_type_t ty, void *data, void (*fn)(opi_type_t));

void
opi_type_set_show(opi_type_t ty, void (*fn)(opi_type_t,opi_t,FILE*));

const char*
opi_type_get_name(opi_type_t ty);


/* ==========================================================================
 * Cell
 */
struct opi_cell {
  struct opi_type *type;
  opi_rc_t rc;
  uintptr_t data[2];
};

#define opi_as(cell, type) ((type*)&cell->data)[0]
#define opi_as_ptr(cell) ((void*)&cell->data)

static inline void
opi_show(opi_t x, FILE *out)
{ x->type->show(x->type, x, out); }

static inline void
opi_init_cell(void* x_, opi_type_t ty)
{
  opi_t x = x_;
  x->type = ty;
  x->rc = 0;
}

static inline void
opi_destroy(opi_t x)
{ x->type->destroy_cell(x->type->data, x); }

static inline opi_t
opi_alloc()
{ return malloc(sizeof(struct opi_cell)); }

static inline void
opi_free(opi_t x)
{ free(x); }

static inline opi_rc_t
opi_inc_rc(opi_t x)
{ return ++x->rc; }

static inline opi_rc_t
opi_dec_rc(opi_t x)
{ return --x->rc; }

static inline void
opi_drop(opi_t x)
{
  if (x->rc == 0) {
    opi_destroy(x);
    opi_free(x);
  }
}

static inline void
opi_unref(opi_t x)
{
  if (--x->rc == 0) {
    opi_destroy(x);
    opi_free(x);
  }
}


/* ==========================================================================
 * State
 */
struct opi_state {
  opi_t *sp;
};

static inline void
opi_state_push(struct opi_state *state, opi_t x)
{ *state->sp++ = x; }

static inline opi_t
opi_state_pop(struct opi_state *state)
{ return *--state->sp; }

static inline opi_t
opi_state_get(struct opi_state *state, size_t offs)
{ return *(state->sp - offs); }

/* ==========================================================================
 * Number
 */
extern
opi_type_t opi_number_type;

void
opi_number_init(void);

static inline opi_t
opi_number(opi_t cell, long double x)
{
  opi_init_cell(cell, opi_number_type);
  opi_as(cell, long double) = x;
  return cell;
}

static inline long double
opi_number_get_value(opi_t cell)
{ return opi_as(cell, long double); }

/* ==========================================================================
 * Nil
 */
extern
opi_type_t opi_nil_type;

void
opi_nil_init(void);

extern
opi_t opi_nil;

/* ==========================================================================
 * Fn
 */
extern
opi_type_t opi_fn_type;

extern
struct opi_state *opi_current_state;
extern
opi_t opi_current_fn;

void
opi_fn_init(void);

opi_t
opi_fn(opi_t cell, const char *name, opi_t (*fn)(void), size_t arity);

void
opi_fn_set_data(opi_t cell, void *data, void (*destroy_data)(void *data));

size_t
opi_fn_get_arity(opi_t cell);

void*
opi_fn_get_data(opi_t cell);

opi_t
opi_fn_apply(opi_t cell);


/* ==========================================================================
 * AST
 */
enum opi_ast_tag {
  OPI_AST_CONST,
  OPI_AST_VAR,
  OPI_AST_APPLY,
  OPI_AST_FN,
  OPI_AST_LET,
};

struct opi_ast {
  enum opi_ast_tag tag;
  union {
    opi_t cnst;
    char *var;
    struct { struct opi_ast *fn, **args; size_t nargs; } apply;
    struct { char **args; size_t nargs; struct opi_ast *body; } fn;
    struct { char **vars; struct opi_ast **vals; size_t n; struct opi_ast *body; } let;
  };
};

void
opi_ast_delete(struct opi_ast *node);

struct opi_ast*
opi_ast_const(opi_t x);

struct opi_ast*
opi_ast_var(const char *name);

struct opi_ast*
opi_ast_apply(struct opi_ast *fn, struct opi_ast **args, size_t nargs);

struct opi_ast*
opi_ast_fn(char **args, size_t nargs, struct opi_ast *body);

struct opi_ast*
opi_ast_let(char **vars, struct opi_ast **vals, size_t n, struct opi_ast *body);

/* ==========================================================================
 * IR
 */
struct opi_builder {
  struct opi_strvec decls;
  int frame_offset;
};

void
opi_builder_init(struct opi_builder *bldr);

void
opi_builder_destroy(struct opi_builder *bldr);

struct opi_ir*
opi_builder_build(struct opi_builder *bldr, struct opi_ast *ast);

enum opi_ir_tag {
  OPI_IR_CONST,
  OPI_IR_VAR,
  OPI_IR_APPLY,
  OPI_IR_FN,
  OPI_IR_LET,
};

struct opi_ir {
  enum opi_ir_tag tag;
  union {
    opi_t cnst;
    size_t var;
    struct { struct opi_ir *fn, **args; size_t nargs; } apply;
    struct { struct opi_ir **caps; size_t ncaps, nargs; struct opi_ir *body; } fn;
    struct { struct opi_ir **vals; size_t n; struct opi_ir *body; } let;
  };
};

void
opi_ir_delete(struct opi_ir *node);

opi_t
opi_ir_eval(struct opi_ir *ir);

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

#endif
