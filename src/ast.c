#include "opium/opium.h"

#include <stdlib.h>
#include <string.h>

OpiAst*
opi_parse_string(const char *str)
{
  FILE *fs = fmemopen((void*)str, strlen(str), "r");
  OpiAst *ast = opi_parse(fs);
  fclose(fs);
  return ast;
}

void
opi_ast_delete(OpiAst *node)
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
      if (node->apply.loc)
        opi_location_delete(node->apply.loc);
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

OpiAst*
opi_ast_const(opi_t x)
{
  OpiAst *node = malloc(sizeof(OpiAst));
  node->tag = OPI_AST_CONST;
  opi_inc_rc(node->cnst = x);
  return node;
}

OpiAst*
opi_ast_var(const char *name)
{
  OpiAst *node = malloc(sizeof(OpiAst));
  node->tag = OPI_AST_VAR;
  node->var = strdup(name);
  return node;
}

OpiAst*
opi_ast_use(const char *old, const char *new)
{
  OpiAst *node = malloc(sizeof(OpiAst));
  node->tag = OPI_AST_USE;
  node->use.old = strdup(old);
  node->use.new = strdup(new);
  return node;
}

OpiAst*
opi_ast_apply(OpiAst *fn, OpiAst **args, size_t nargs)
{
  OpiAst *node = malloc(sizeof(OpiAst));
  node->tag = OPI_AST_APPLY;
  node->apply.fn = fn;
  node->apply.args = malloc(sizeof(OpiAst*) * nargs);
  memcpy(node->apply.args, args, sizeof(OpiAst*) * nargs);
  node->apply.nargs = nargs;
  node->apply.eflag = TRUE;
  node->apply.loc = NULL;
  return node;
}

OpiAst*
opi_ast_fn(char **args, size_t nargs, OpiAst *body)
{
  OpiAst *node = malloc(sizeof(OpiAst));
  node->tag = OPI_AST_FN;
  node->fn.args = malloc(sizeof(char*) * nargs);
  for (size_t i = 0; i < nargs; ++i)
    node->fn.args[i] = strdup(args[i]);
  node->fn.nargs = nargs;
  node->fn.body = body;
  return node;
}

OpiAst*
opi_ast_let(char **vars, OpiAst **vals, size_t n, OpiAst *body)
{
  OpiAst *node = malloc(sizeof(OpiAst));
  node->tag = OPI_AST_LET;
  node->let.vars = malloc(sizeof(char*) * n);
  for (size_t i = 0; i < n; ++i)
    node->let.vars[i] = strdup(vars[i]);
  node->let.vals = malloc(sizeof(OpiAst*) * n);
  memcpy(node->let.vals, vals, sizeof(OpiAst*) * n);
  node->let.n = n;
  node->let.body = body;
  return node;
}

OpiAst*
opi_ast_if(OpiAst *test, OpiAst *then, OpiAst *els)
{
  OpiAst *node = malloc(sizeof(OpiAst));
  node->tag = OPI_AST_IF;
  node->iff.test = test;
  node->iff.then = then;
  node->iff.els  = els;
  return node;
}

OpiAst*
opi_ast_fix(char **vars, OpiAst **lams, size_t n, OpiAst *body)
{
  OpiAst *node = malloc(sizeof(OpiAst));
  node->tag = OPI_AST_FIX;
  node->let.vars = malloc(sizeof(char*) * n);
  for (size_t i = 0; i < n; ++i)
    node->let.vars[i] = strdup(vars[i]);
  node->let.vals = malloc(sizeof(OpiAst*) * n);
  for (size_t i = 0; i < n; ++i) {
    opi_assert(lams[i]->tag == OPI_AST_FN);
    node->let.vals[i] = lams[i];
  }
  node->let.n = n;
  node->let.body = body;
  return node;
}

OpiAst*
opi_ast_block(OpiAst **exprs, size_t n)
{
  OpiAst *node = malloc(sizeof(OpiAst));
  node->tag = OPI_AST_BLOCK;
  node->block.exprs = malloc(sizeof(OpiAst*) *  n);
  memcpy(node->block.exprs, exprs, sizeof(OpiAst*) * n);
  node->block.n = n;
  node->block.drop = TRUE;
  node->block.namespace = NULL;
  return node;
}

void
opi_ast_block_set_drop(OpiAst *block, int drop)
{
  opi_assert(block->tag == OPI_AST_BLOCK);
  block->block.drop = drop;
}

void
opi_ast_block_set_namespace(OpiAst *block, const char *namespace)
{
  opi_assert(block->tag == OPI_AST_BLOCK);
  if (block->block.namespace)
    free(block->block.namespace);
  block->block.namespace = strdup(namespace);
}

void
opi_ast_block_prepend(OpiAst *block, OpiAst *node)
{
  opi_assert(block->tag == OPI_AST_BLOCK);
  block->block.exprs = realloc(block->block.exprs, sizeof(OpiAst*) * block->block.n + 1);
  memmove(block->block.exprs + 1, block->block.exprs, sizeof(OpiAst*) * block->block.n);
  block->block.exprs[0] = node;
  block->block.n += 1;
}

void
opi_ast_block_append(OpiAst *block, OpiAst *node)
{
  opi_assert(block->tag == OPI_AST_BLOCK);
  block->block.exprs = realloc(block->block.exprs, sizeof(OpiAst*) * block->block.n + 1);
  block->block.exprs[block->block.n++] = node;
}

OpiAst*
opi_ast_load(const char *path)
{
  OpiAst *node = malloc(sizeof(OpiAst));
  node->tag = OPI_AST_LOAD;
  node->load = strdup(path);
  return node;
}

OpiAst*
opi_ast_match(const char *type, char **vars, char **fields, size_t n,
    OpiAst *expr, OpiAst *then, OpiAst *els)
{
  OpiAst *node = malloc(sizeof(OpiAst));
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

OpiAst*
opi_ast_struct(const char *typename, char** fields, size_t nfields)
{
  OpiAst *node = malloc(sizeof(OpiAst));
  node->tag = OPI_AST_STRUCT;
  node->strct.typename = strdup(typename);
  node->strct.fields = malloc(sizeof(char*) * nfields);
  for (size_t i = 0; i < nfields; ++i)
    node->strct.fields[i] = strdup(fields[i]);
  node->strct.nfields = nfields;
  return node;
}

OpiAst*
opi_ast_and(OpiAst *x, OpiAst *y)
{ return opi_ast_if(x, y, opi_ast_const(opi_false)); }

OpiAst*
opi_ast_or(OpiAst *x, OpiAst *y)
{
  char *var = " or tmp ";
  return opi_ast_let(&var, &x, 1,
      opi_ast_if(opi_ast_var(" or tmp "), opi_ast_var(" or tmp "), y));
}

OpiAst*
opi_ast_return(OpiAst *val)
{
  OpiAst *node = malloc(sizeof(OpiAst));
  node->tag = OPI_AST_RETURN;
  node->ret = val;
  return node;
}

OpiAst*
opi_ast_eor(OpiAst *try, OpiAst *els, const char *ename)
{
  char *var = " eor tmp ";
  char *vars[] = { (char*)ename };
  char *fields[] = { "what" };
  if (try->tag == OPI_AST_APPLY)
    try->apply.eflag = FALSE;
  return
    opi_ast_let(&var, &try, 1,
      opi_ast_match("undefined", vars, fields, 1, opi_ast_var(" eor tmp "),
        els, opi_ast_var(" eor tmp ")));
}

OpiAst*
opi_ast_when(OpiAst *test_expr,
    const char *then_bind, OpiAst *then_expr,
    const char *else_bind, OpiAst *else_expr)
{
  char *tmp = " when tmp ";

  char *then_vars[] = { (char*)then_bind };
  char *else_vars[] = { (char*)else_bind };

  char *fields[] = { "what" };

  if (test_expr->tag == OPI_AST_APPLY)
    test_expr->apply.eflag = FALSE;

  return
    opi_ast_let(&tmp, &test_expr, 1,
      opi_ast_match("undefined", else_vars, fields, 1, opi_ast_var(tmp),
        else_expr ? else_expr : opi_ast_return(opi_ast_var(tmp)),
        opi_ast_let(then_vars, (OpiAst*[]){ opi_ast_var(tmp) }, 1,
          then_expr)));
}

OpiAst*
opi_ast_binop(int opc, OpiAst *lhs, OpiAst *rhs)
{
  OpiAst *node = malloc(sizeof(OpiAst));
  node->tag = OPI_AST_BINOP;
  node->binop.opc = opc;
  node->binop.lhs = lhs;
  node->binop.rhs = rhs;
  return node;
}

