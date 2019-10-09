#include "opium/opium.h"

#include <string.h>

void
opi_init(void)
{
  opi_number_init();
  opi_fn_init();
}

/******************************************************************************/
static void
default_destroy_cell(opi_type_t ty, opi_t cell)
{ }

static void
default_destroy_data(opi_type_t ty)
{ }

static void
default_show(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, "<%s>", opi_type_get_name(ty)); }

opi_type_t
opi_type(const char *name)
{
  struct opi_type *ty = malloc(sizeof(struct opi_type));
  opi_assert(strlen(name) <= OPI_TYPE_NAME_MAX);
  strcpy(ty->name, name);
  ty->destroy_cell = default_destroy_cell;
  ty->data = NULL;
  ty->destroy_data = default_destroy_data;
  ty->show = default_show;
  return ty;
}

void
opi_type_set_destroy_cell(opi_type_t ty, void (*fn)(opi_type_t,opi_t))
{ ty->destroy_cell = fn; }

void
opi_type_set_data(opi_type_t ty, void *data, void (*fn)(opi_type_t))
{
  ty->data = data;
  ty->destroy_data = fn;
}

void
opi_type_set_show(opi_type_t ty, void (*fn)(opi_type_t,opi_t,FILE*))
{ ty->show = fn; }

const char*
opi_type_get_name(opi_type_t ty)
{ return ty->name; }

/******************************************************************************/
opi_type_t opi_number_type;

static void
number_show(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, "%Lg", opi_as(x, long double)); }

void
opi_number_init(void)
{
  opi_number_type = opi_type("number");
  opi_type_set_show(opi_number_type, number_show);
}

/******************************************************************************/
opi_type_t opi_nil_type;
opi_t opi_nil;

static void
nil_show(opi_type_t ty, opi_t x, FILE *out)
{ fprintf(out, "nil"); }

void
opi_nil_init(void)
{
  opi_nil_type = opi_type("nil");
  opi_type_set_show(opi_nil_type, nil_show);

  opi_nil = opi_alloc();
  opi_init_cell(opi_nil, opi_nil_type);
  opi_inc_rc(opi_nil);
}

/******************************************************************************/
opi_type_t opi_fn_type;

struct opi_state *opi_current_state;
opi_t opi_current_fn;

struct fn {
  struct {
    char *name;
    opi_t (*fn)(void);
    void *data;
    void (*destroy_data)(void *data);
  } *d;

  uintptr_t arity;
};

static void
fn_show(opi_type_t type, opi_t cell, FILE *out)
{
  struct fn *fn = opi_as_ptr(cell);
  if (fn->d->name)
    fprintf(out, "<fn %s>", fn->d->name);
  else
    fprintf(out, "<fn>");
}

static void
fn_destroy(opi_type_t type, opi_t cell)
{
  struct fn *fn = opi_as_ptr(cell);
  if (fn->d->name)
    free(fn->d->name);
  fn->d->destroy_data(fn->d->data);
  free(fn->d);
}

void
opi_fn_init(void)
{
  opi_fn_type = opi_type("fn");
  opi_type_set_show(opi_fn_type, fn_show);
  opi_type_set_destroy_cell(opi_fn_type, fn_destroy);
}

static void
fn_default_destroy_data(void *data)
{ }

opi_t
opi_fn(opi_t cell, const char *name, opi_t (*f)(void), size_t arity)
{
  opi_init_cell(cell, opi_fn_type);
  struct fn *fn = opi_as_ptr(cell);
  fn->d = malloc(sizeof(*fn->d));
  fn->d->name = name ? strdup(name) : NULL;
  fn->d->fn = f;
  fn->d->data = NULL;
  fn->d->destroy_data = fn_default_destroy_data;
  fn->arity = arity;
  return cell;
}

void
opi_fn_set_data(opi_t cell, void *data, void (*destroy_data)(void *data))
{
  struct fn *fn = opi_as_ptr(cell);
  fn->d->data = data;
  fn->d->destroy_data = destroy_data;
}

size_t
opi_fn_get_arity(opi_t cell)
{ return opi_as(cell, struct fn).arity; }

void*
opi_fn_get_data(opi_t cell)
{ return opi_as(cell, struct fn).d->data; }

opi_t
opi_fn_apply(opi_t cell)
{
  struct fn *fn = opi_as_ptr(cell);
  opi_current_fn = cell;
  return fn->d->fn();
}

/******************************************************************************/
void
opi_ast_delete(struct opi_ast *node)
{
  switch (node->tag) {
    case OPI_AST_CONST:
      opi_unref(node->cnst);
      break;

    case OPI_AST_VAR:
      free(node->var);
      break;

    case OPI_AST_APPLY:
      opi_ast_delete(node->apply.fn);
      for (size_t i = 0; i < node->apply.nargs; ++i)
        opi_ast_delete(node->apply.args[i]);
      free(node->apply.args);
      break;

    case OPI_AST_FN:
      for (size_t i = 0; i < node->fn.nargs; ++i)
        free(node->fn.args[i]);
      free(node->fn.args);
      opi_ast_delete(node->fn.body);
      break;

    case OPI_AST_LET:
      for (size_t i = 0; i < node->let.n; ++i) {
        free(node->let.vars[i]);
        opi_ast_delete(node->let.vals[i]);
      }
      free(node->let.vars);
      free(node->let.vals);
      opi_ast_delete(node->let.body);
      break;
  }

  free(node);
}

struct opi_ast*
opi_ast_const(opi_t x)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_CONST;
  opi_inc_rc(node->cnst = x);
  return node;
}

struct opi_ast*
opi_ast_var(const char *name)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_VAR;
  node->var = strdup(name);
  return node;
}

struct opi_ast*
opi_ast_apply(struct opi_ast *fn, struct opi_ast **args, size_t nargs)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_APPLY;
  node->apply.fn = fn;
  node->apply.args = malloc(sizeof(struct opi_ast*) * nargs);
  memcpy(node->apply.args, args, sizeof(struct opi_ast*) * nargs);
  node->apply.nargs = nargs;
  return node;
}

struct opi_ast*
opi_ast_fn(char **args, size_t nargs, struct opi_ast *body)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_FN;
  node->fn.args = malloc(sizeof(char*) * nargs);
  for (size_t i = 0; i < nargs; ++i)
    node->fn.args[i] = strdup(args[i]);
  node->fn.nargs = nargs;
  node->fn.body = body;
  return node;
}

struct opi_ast*
opi_ast_let(char **vars, struct opi_ast **vals, size_t n, struct opi_ast *body)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_LET;
  node->let.vars = malloc(sizeof(char*) * n);
  for (size_t i = 0; i < n; ++i)
    node->let.vars[i] = strdup(vars[i]);
  node->let.vals = malloc(sizeof(struct opi_ast*) * n);
  memcpy(node->let.vals, vals, sizeof(struct opi_ast*) * n);
  node->let.n = n;
  node->let.body = body;
  return node;
}

/******************************************************************************/
void
opi_builder_init(struct opi_builder *bldr)
{
  opi_strvec_init(&bldr->decls);
  bldr->frame_offset = 0;
}

void
opi_builder_destroy(struct opi_builder *bldr)
{ opi_strvec_destroy(&bldr->decls); }

struct opi_ir*
opi_builder_build(struct opi_builder *bldr, struct opi_ast *ast)
{
  switch (ast->tag) {
    case OPI_AST_CONST:
      return opi_ir_const(ast->cnst);

    case OPI_AST_VAR:
    {
      int var_idx = opi_strvec_rfind(&bldr->decls, ast->var);
      if (var_idx >= 0) {
        // # Found in local variables:
        return opi_ir_var(bldr->decls.size - var_idx);
      } else  {
        // # Otherwize, add to captures:
        // insert at the beginning of declarations => won't change other offsetes
        opi_strvec_insert(&bldr->decls, ast->var, 0);

        // shift frame-offset
        bldr->frame_offset += 1;

        return opi_ir_var(bldr->decls.size);
      }
    }

    case OPI_AST_APPLY:
    {
      struct opi_ir *fn = opi_builder_build(bldr, ast->apply.fn);
      struct opi_ir *args[ast->apply.nargs];
      for (size_t i = 0; i < ast->apply.nargs; ++i)
        args[i] = opi_builder_build(bldr, ast->apply.args[i]);
      return opi_ir_apply(fn, args, ast->apply.nargs);
    }

    case OPI_AST_FN:
    {
      // create separate builder for function body
      struct opi_builder fn_bldr;
      opi_builder_init(&fn_bldr);

      // declare parameters as fn-local variables
      for (size_t i = 0; i < ast->fn.nargs; ++i)
        opi_strvec_push(&fn_bldr.decls, ast->fn.args[i]);

      // build body (with local builder)
      struct opi_ir *body = opi_builder_build(&fn_bldr, ast->fn.body);

      // process captures
      size_t ncaps = fn_bldr.frame_offset;
      struct opi_ir *caps[ncaps];
      for (size_t i = 0; i < ncaps; ++i) {
        // build expression to get captured variable
        struct opi_ast *tmp_var = opi_ast_var(fn_bldr.decls.data[i]);
        caps[i] = opi_builder_build(bldr, tmp_var);
        opi_ast_delete(tmp_var);
      }

      opi_builder_destroy(&fn_bldr);
      return opi_ir_fn(caps, ncaps, ast->fn.nargs, body);
    }

    case OPI_AST_LET:
    {
      struct opi_ir *vals[ast->let.n];

      // evaluate values
      for (size_t i = 0; i < ast->let.n; ++i)
        vals[i] = opi_builder_build(bldr, ast->let.vals[i]);

      // declare variables
      for (size_t i = 0; i < ast->let.n; ++i)
        opi_strvec_push(&bldr->decls, ast->let.vars[i]);

      // evaluate body
      struct opi_ir *body = opi_builder_build(bldr, ast->let.body);

      // hide variables
      for (size_t i = 0; i < ast->let.n; ++i)
        opi_strvec_pop(&bldr->decls);

      return opi_ir_let(vals, ast->let.n, body);
    }
  }

  abort();
}

void
opi_ir_delete(struct opi_ir *node)
{
  switch (node->tag) {
    case OPI_IR_CONST:
      opi_unref(node->cnst);
      break;

    case OPI_IR_VAR:
      break;

    case OPI_IR_APPLY:
      opi_ir_delete(node->apply.fn);
      for (size_t i = 0; i < node->apply.nargs; ++i)
        opi_ir_delete(node->apply.args[i]);
      free(node->apply.args);
      break;

    case OPI_IR_FN:
      for (size_t i = 0; i < node->fn.ncaps; ++i)
        opi_ir_delete(node->fn.caps[i]);
      free(node->fn.caps);
      opi_ir_delete(node->fn.body);
      break;

    case OPI_IR_LET:
      for (size_t i = 0; i < node->let.n; ++i)
        opi_ir_delete(node->let.vals[i]);
      free(node->let.vals);
      opi_ir_delete(node->let.body);
      break;
  }

  free(node);
}

struct ir_fn_data {
  opi_t *caps;
  size_t ncaps;
  struct opi_ir *body;
};

static void
ir_fn_data_delete(void *ptr)
{
  struct ir_fn_data *data = ptr;
  for (size_t i = 0; i < data->ncaps; ++i)
    opi_unref(data->caps[i]);
  free(data->caps);
  free(data);
}

static opi_t
ir_fn(void)
{
  struct ir_fn_data *data = opi_fn_get_data(opi_current_fn);
  size_t ncaps = data->ncaps;

  // shift arguments to insert captures before them (and increment RC)
  size_t nargs = opi_fn_get_arity(opi_current_fn);
  opi_t *sp = opi_current_state->sp - nargs;
  for (size_t i = 0; i < nargs; ++i) {
    opi_t arg = sp[i];
    opi_current_state->sp[ncaps + i] = arg;
    opi_inc_rc(arg);
  }

  // insert captures
  for (size_t i = 0; i < ncaps; ++i)
    sp[i] = data->caps[i];

  // shift SP
  opi_current_state->sp += ncaps;

  // evaluate body
  opi_t ret = opi_ir_eval(data->body);

  // pop stack
  for (size_t i = 0; i < nargs; ++i)
    opi_unref(opi_state_pop(opi_current_state));
  opi_current_state->sp -= ncaps;

  return ret;
}

opi_t
opi_ir_eval(struct opi_ir *ir)
{
  switch (ir->tag) {
    case OPI_IR_CONST:
      return ir->cnst;

    case OPI_IR_VAR:
      return opi_state_get(opi_current_state, ir->var);
      break;

    case OPI_IR_APPLY:
      {
        // evaluate callee
        opi_t fn = opi_ir_eval(ir->apply.fn);
        opi_assert(fn->type == opi_fn_type);
        opi_assert(opi_fn_get_arity(fn) == ir->apply.nargs);

        // evaluate arguments
        opi_t args[ir->apply.nargs];
        for (size_t i = 0; i < ir->apply.nargs; ++i)
          args[i] = opi_ir_eval(ir->apply.args[i]);

        // push arguments
        for (size_t i = 0; i < ir->apply.nargs; ++i)
          opi_state_push(opi_current_state, args[i]);

        // call
        return opi_fn_apply(fn);
      }
      break;

    case OPI_IR_FN:
      {
        // evaluate captures
        struct ir_fn_data *data = malloc(sizeof(struct ir_fn_data));
        data->caps = malloc(sizeof(opi_t) * ir->fn.ncaps);
        data->ncaps = ir->fn.ncaps;
        for (size_t i = 0; i < ir->fn.ncaps; ++i) {
          data->caps[i] = opi_ir_eval(ir->fn.caps[i]);
          opi_inc_rc(data->caps[i]);
        }

        // set body
        data->body = ir->fn.body;

        // create fn
        opi_t fn = opi_alloc();
        opi_fn(fn, NULL, ir_fn, ir->fn.nargs);
        opi_fn_set_data(fn, data, ir_fn_data_delete);

        return fn;
      }

    case OPI_IR_LET:
      opi_assert(!"eval LET: unimplemented");
      /*for (size_t i = 0; i < node->let.n; ++i)*/
        /*opi_ir_delete(node->let.vals[i]);*/
      /*free(node->let.vals);*/
      /*opi_ir_delete(node->let.body);*/
      /*break;*/
  }

  abort();
}

struct opi_ir*
opi_ir_const(opi_t x)
{
  struct opi_ir *node = malloc(sizeof(struct opi_ir));
  node->tag = OPI_AST_CONST;
  opi_inc_rc(node->cnst = x);
  return node;
}

struct opi_ir*
opi_ir_var(size_t offs)
{
  struct opi_ir *node = malloc(sizeof(struct opi_ir));
  node->tag = OPI_IR_VAR;
  node->var = offs;
  return node;
}

struct opi_ir*
opi_ir_apply(struct opi_ir *fn, struct opi_ir **args, size_t nargs)
{
  struct opi_ir *node = malloc(sizeof(struct opi_ir));
  node->tag = OPI_IR_APPLY;
  node->apply.fn = fn;
  node->apply.args = malloc(sizeof(struct opi_ir*) * nargs);
  memcpy(node->apply.args, args, sizeof(struct opi_ir*) * nargs);
  node->apply.nargs = nargs;
  return node;
}

struct opi_ir*
opi_ir_fn(struct opi_ir **caps, size_t ncaps, size_t nargs, struct opi_ir *body)
{
  struct opi_ir *node = malloc(sizeof(struct opi_ir));
  node->tag = OPI_IR_FN;
  node->fn.caps = malloc(sizeof(struct opi_ir*) * ncaps);
  memcpy(node->fn.caps, caps, sizeof(struct opi_ir*) * ncaps);
  node->fn.ncaps = ncaps;
  node->fn.nargs = nargs;
  node->fn.body = body;
  return node;
}

struct opi_ir*
opi_ir_let(struct opi_ir **vals, size_t n, struct opi_ir *body)
{
  struct opi_ir *node = malloc(sizeof(struct opi_ir));
  node->tag = OPI_IR_LET;
  node->let.vals = malloc(sizeof(struct opi_ir*) * n);
  memcpy(node->let.vals, vals, sizeof(struct opi_ir*) * n);
  node->let.n = n;
  node->let.body = body;
  return node;
}

