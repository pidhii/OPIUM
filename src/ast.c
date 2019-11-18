#include "opium/opium.h"

#include <stdlib.h>
#include <string.h>

struct opi_ast*
opi_parse_string(const char *str)
{
  FILE *fs = fmemopen((void*)str, strlen(str), "r");
  struct opi_ast *ast = opi_parse(fs);
  fclose(fs);
  return ast;
}

void
opi_ast_delete(struct opi_ast *node)
{
  if (node == NULL)
    return;

  switch (node->tag) {
    case OPI_AST_CONST:
      opi_unref(node->cnst);
      break;

    case OPI_AST_VAR:
      free(node->var);
      break;

    case OPI_AST_USE:
      free(node->use.old);
      free(node->use.new);
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
    case OPI_AST_FIX:
      for (size_t i = 0; i < node->let.n; ++i) {
        free(node->let.vars[i]);
        opi_ast_delete(node->let.vals[i]);
      }
      free(node->let.vars);
      free(node->let.vals);
      if (node->let.body)
        opi_ast_delete(node->let.body);
      break;

    case OPI_AST_IF:
      opi_ast_delete(node->iff.test);
      opi_ast_delete(node->iff.then);
      opi_ast_delete(node->iff.els);
      break;

    case OPI_AST_BLOCK:
      for (size_t i = 0; i < node->block.n; ++i)
        opi_ast_delete(node->block.exprs[i]);
      free(node->block.exprs);
      if (node->block.namespace)
        free(node->block.namespace);
      break;

    case OPI_AST_LOAD:
      free(node->load);
      break;

    case OPI_AST_MATCH:
      free(node->match.type);
      for (size_t i = 0; i < node->match.n; ++i) {
        free(node->match.vars[i]);
        free(node->match.fields[i]);
      }
      free(node->match.vars);
      free(node->match.fields);
      opi_ast_delete(node->match.expr);
      if (node->match.then)
        opi_ast_delete(node->match.then);
      if (node->match.els)
        opi_ast_delete(node->match.els);
      break;

    case OPI_AST_STRUCT:
      free(node->strct.typename);
      for (size_t i = 0; i < node->strct.nfields; ++i)
        free(node->strct.fields[i]);
      free(node->strct.fields);
      break;

    case OPI_AST_RETURN:
      opi_ast_delete(node->ret);
      break;

    case OPI_AST_BINOP:
      opi_ast_delete(node->binop.lhs);
      opi_ast_delete(node->binop.rhs);
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
opi_ast_use(const char *old, const char *new)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_USE;
  node->use.old = strdup(old);
  node->use.new = strdup(new);
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
  node->apply.eflag = TRUE;
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

struct opi_ast*
opi_ast_if(struct opi_ast *test, struct opi_ast *then, struct opi_ast *els)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_IF;
  node->iff.test = test;
  node->iff.then = then;
  node->iff.els  = els;
  return node;
}

struct opi_ast*
opi_ast_fix(char **vars, struct opi_ast **lams, size_t n, struct opi_ast *body)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_FIX;
  node->let.vars = malloc(sizeof(char*) * n);
  for (size_t i = 0; i < n; ++i)
    node->let.vars[i] = strdup(vars[i]);
  node->let.vals = malloc(sizeof(struct opi_ast*) * n);
  for (size_t i = 0; i < n; ++i) {
    opi_assert(lams[i]->tag == OPI_AST_FN);
    node->let.vals[i] = lams[i];
  }
  node->let.n = n;
  node->let.body = body;
  return node;
}

struct opi_ast*
opi_ast_block(struct opi_ast **exprs, size_t n)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_BLOCK;
  node->block.exprs = malloc(sizeof(struct opi_ast*) *  n);
  memcpy(node->block.exprs, exprs, sizeof(struct opi_ast*) * n);
  node->block.n = n;
  node->block.drop = TRUE;
  node->block.namespace = NULL;
  return node;
}

void
opi_ast_block_set_drop(struct opi_ast *block, int drop)
{
  opi_assert(block->tag == OPI_AST_BLOCK);
  block->block.drop = drop;
}

void
opi_ast_block_set_namespace(struct opi_ast *block, const char *namespace)
{
  opi_assert(block->tag == OPI_AST_BLOCK);
  if (block->block.namespace)
    free(block->block.namespace);
  block->block.namespace = strdup(namespace);
}

void
opi_ast_block_prepend(struct opi_ast *block, struct opi_ast *node)
{
  opi_assert(block->tag == OPI_AST_BLOCK);
  block->block.exprs = realloc(block->block.exprs, sizeof(struct opi_ast*) * block->block.n + 1);
  memmove(block->block.exprs + 1, block->block.exprs, sizeof(struct opi_ast*) * block->block.n);
  block->block.exprs[0] = node;
  block->block.n += 1;
}

void
opi_ast_block_append(struct opi_ast *block, struct opi_ast *node)
{
  opi_assert(block->tag == OPI_AST_BLOCK);
  block->block.exprs = realloc(block->block.exprs, sizeof(struct opi_ast*) * block->block.n + 1);
  block->block.exprs[block->block.n++] = node;
}

struct opi_ast*
opi_ast_load(const char *path)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_LOAD;
  node->load = strdup(path);
  return node;
}

struct opi_ast*
opi_ast_match(const char *type, char **vars, char **fields, size_t n,
    struct opi_ast *expr, struct opi_ast *then, struct opi_ast *els)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_MATCH;
  node->match.type = strdup(type);
  node->match.vars = malloc(sizeof(char*) * n);
  node->match.fields = malloc(sizeof(char*) * n);
  for (size_t i = 0; i < n; ++i) {
    node->match.vars[i] = strdup(vars[i]);
    node->match.fields[i] = strdup(fields[i]);
  }
  node->match.n = n;
  node->match.expr = expr;
  node->match.then = then;
  node->match.els = els;
  return node;
}

struct opi_ast*
opi_ast_struct(const char *typename, char** fields, size_t nfields)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_STRUCT;
  node->strct.typename = strdup(typename);
  node->strct.fields = malloc(sizeof(char*) * nfields);
  for (size_t i = 0; i < nfields; ++i)
    node->strct.fields[i] = strdup(fields[i]);
  node->strct.nfields = nfields;
  return node;
}

struct opi_ast*
opi_ast_and(struct opi_ast *x, struct opi_ast *y)
{ return opi_ast_if(x, y, opi_ast_const(opi_false)); }

struct opi_ast*
opi_ast_or(struct opi_ast *x, struct opi_ast *y)
{
  char *var = " or tmp ";
  return opi_ast_let(&var, &x, 1,
      opi_ast_if(opi_ast_var(" or tmp "), opi_ast_var(" or tmp "), y));
}

struct opi_ast*
opi_ast_return(struct opi_ast *val)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_RETURN;
  node->ret = val;
  return node;
}

struct opi_ast*
opi_ast_eor(struct opi_ast *try, struct opi_ast *els)
{
  char *var = " eor tmp ";
  char *vars[] = { "wtf" };
  char *fields[] = { "what" };
  if (try->tag == OPI_AST_APPLY)
    try->apply.eflag = FALSE;
  return
    opi_ast_let(&var, &try, 1,
      opi_ast_match("undefined", vars, fields, 1, opi_ast_var(" eor tmp "),
        els, opi_ast_var(" eor tmp ")));
}

struct opi_ast*
opi_ast_binop(int opc, struct opi_ast *lhs, struct opi_ast *rhs)
{
  struct opi_ast *node = malloc(sizeof(struct opi_ast));
  node->tag = OPI_AST_BINOP;
  node->binop.opc = opc;
  node->binop.lhs = lhs;
  node->binop.rhs = rhs;
  return node;
}

