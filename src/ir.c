#include "opium/opium.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

void
opi_alist_init(struct opi_alist *a)
{
  opi_strvec_init(&a->keys);
  opi_strvec_init(&a->vals);
}

void
opi_alist_destroy(struct opi_alist *a)
{
  opi_strvec_destroy(&a->keys);
  opi_strvec_destroy(&a->vals);
}

size_t
opi_alist_get_size(struct opi_alist *a)
{ return a->keys.size; }

void
opi_alist_push(struct opi_alist *a, const char *var, const char *map)
{
  opi_strvec_push(&a->keys, var);
  opi_strvec_push(&a->vals, map ? map : var);
}

void
opi_alist_pop(struct opi_alist *a, size_t n)
{
  opi_assert(n <= opi_alist_get_size(a));
  while (n--) {
    opi_strvec_pop(&a->keys);
    opi_strvec_pop(&a->vals);
  }
}


void
opi_builder_init(struct opi_builder *bldr)
{
  bldr->is_derived = FALSE;

  opi_strvec_init(&bldr->decls);
  bldr->frame_offset = 0;

  opi_alist_init(bldr->alist = malloc(sizeof(struct opi_alist)));

  opi_strvec_init(bldr->const_names = malloc(sizeof(struct opi_strvec)));
  opi_ptrvec_init(bldr->const_vals = malloc(sizeof(struct opi_ptrvec)));

  opi_strvec_init(bldr->srcdirs = malloc(sizeof(struct opi_strvec)));

  opi_strvec_init(bldr->loaded = malloc(sizeof(struct opi_strvec)));
  opi_strvec_init(bldr->load_state = malloc(sizeof(struct opi_strvec)));

  opi_strvec_init(bldr->type_names = malloc(sizeof(struct opi_strvec)));
  opi_ptrvec_init(bldr->types = malloc(sizeof(struct opi_ptrvec)));
  opi_ptrvec_init(bldr->all_types = malloc(sizeof(struct opi_ptrvec)));

  opi_strvec_init(bldr->trait_names = malloc(sizeof(struct opi_strvec)));
  opi_ptrvec_init(bldr->traits = malloc(sizeof(struct opi_ptrvec)));

  opi_builder_add_type(bldr, "pair", opi_pair_type);
  opi_alist_push(bldr->alist, "pair", NULL);
}

void
opi_builder_init_derived(struct opi_builder *bldr, struct opi_builder *parent)
{
  bldr->is_derived = TRUE;

  opi_strvec_init(&bldr->decls);
  bldr->frame_offset = 0;

  bldr->alist = parent->alist;

  bldr->const_names = parent->const_names;
  bldr->const_vals = parent->const_vals;

  bldr->srcdirs = parent->srcdirs;

  bldr->loaded = parent->loaded;
  bldr->load_state = parent->load_state;

  bldr->type_names = parent->type_names;
  bldr->types = parent->types;
  bldr->all_types = parent->all_types;

  bldr->trait_names = parent->trait_names;
  bldr->traits = parent->traits;
}

static void
delete_type(opi_type_t type)
{
  if (type != opi_pair_type)
    opi_type_delete(type);
}

void
opi_builder_destroy(struct opi_builder *bldr)
{
  opi_strvec_destroy(&bldr->decls);
  if (bldr->is_derived) {
    /*opi_alist_pop(bldr->alist, bldr->frame_offset);*/
    return;
  }

  opi_alist_destroy(bldr->alist);
  free(bldr->alist);

  opi_strvec_destroy(bldr->const_names);
  free(bldr->const_names);
  opi_ptrvec_destroy(bldr->const_vals, (void*)opi_unref);
  free(bldr->const_vals);

  opi_strvec_destroy(bldr->srcdirs);
  free(bldr->srcdirs);
  opi_strvec_destroy(bldr->loaded);
  free(bldr->loaded);
  opi_strvec_destroy(bldr->load_state);
  free(bldr->load_state);

  opi_strvec_destroy(bldr->type_names);
  free(bldr->type_names);
  opi_ptrvec_destroy(bldr->types, NULL);
  free(bldr->types);
  opi_ptrvec_destroy(bldr->all_types, (void*)delete_type);
  free(bldr->all_types);

  opi_strvec_destroy(bldr->trait_names);
  free(bldr->trait_names);
  opi_ptrvec_destroy(bldr->traits, NULL);
  free(bldr->traits);
}

void
opi_builder_add_source_directory(struct opi_builder *bldr, const char *path)
{
  char fullpath[PATH_MAX];
  opi_assert(realpath(path, fullpath));
  opi_strvec_push(bldr->srcdirs, fullpath);
}

char*
opi_builder_find_path(struct opi_builder *bldr, const char *path, char *fullpath)
{
  char buf[PATH_MAX];
  for (size_t i = 0; i < bldr->srcdirs->size; ++i) {
    sprintf(buf, "%s/%s", bldr->srcdirs->data[i], path);
    if (realpath(buf, fullpath))
      return fullpath;
  }
  return NULL;
}

void
opi_builder_add_type(struct opi_builder *bldr, const char *name, opi_type_t type)
{
  if (opi_builder_find_type(bldr, name)) {
    opi_error("type named '%s' already present\n", name);
    exit(EXIT_FAILURE);
  }
  opi_strvec_push(bldr->type_names, name);
  opi_ptrvec_push(bldr->types, type, NULL);
  opi_ptrvec_push(bldr->all_types, type, NULL);
}

void
opi_builder_add_trait(struct opi_builder *bldr, const char *name, struct opi_trait *t)
{
  if (opi_builder_find_trait(bldr, name)) {
    opi_error("trait named '%s' already present\n", name);
    exit(EXIT_FAILURE);
  }
  opi_strvec_push(bldr->trait_names, name);
  opi_ptrvec_push(bldr->traits, t, NULL);
}

opi_type_t
opi_builder_find_type(struct opi_builder *bldr, const char *typename)
{
  long long int idx = opi_strvec_find(bldr->type_names, typename);
  return idx < 0 ? NULL : bldr->types->data[idx];
}

struct opi_trait*
opi_builder_find_trait(struct opi_builder *bldr, const char *traitname)
{
  long long int idx = opi_strvec_find(bldr->trait_names, traitname);
  return idx < 0 ? NULL : bldr->traits->data[idx];
}

void
opi_builder_def_const(struct opi_builder *bldr, const char *name, opi_t val)
{
  opi_strvec_push(bldr->const_names, name);
  opi_ptrvec_push(bldr->const_vals, val, NULL);
  opi_alist_push(bldr->alist, name, NULL);
  opi_inc_rc(val);
}

opi_t
opi_builder_find_const(struct opi_builder *bldr, const char *name)
{
  long long int idx = opi_strvec_rfind(bldr->const_names, name);
  return idx < 0 ? NULL : bldr->const_vals->data[idx];
}

void
opi_builder_push_decl(struct opi_builder *bldr, const char *var)
{
  opi_strvec_push(&bldr->decls, var);
  opi_alist_push(bldr->alist, var, NULL);
}

void
opi_builder_pop_decl(struct opi_builder *bldr)
{
  const char *var = bldr->alist->keys.data[bldr->alist->keys.size-1];
  const char *decl = bldr->decls.data[bldr->decls.size - 1];
  /*opi_debug("var = %s, decl = %s\n", var, decl);*/
  opi_assert(strcmp(var, decl) == 0);
  opi_strvec_pop(&bldr->decls);
  opi_alist_pop(bldr->alist, 1);
}

void
opi_builder_capture(struct opi_builder *bldr, const char *var)
{
  opi_strvec_insert(&bldr->decls, var, 0);
  /*opi_alist_push(bldr->alist, var, NULL);*/
  bldr->frame_offset += 1;
}

const char*
opi_builder_assoc(struct opi_builder *bldr, const char *var)
{
  int idx = opi_strvec_rfind(&bldr->alist->keys, var);
  if (idx < 0) {
    opi_error("no such variable, '%s'\n", var);
    exit(EXIT_FAILURE);
  }
  return bldr->alist->vals.data[idx];
}

void
opi_builder_begin_scope(struct opi_builder *bldr, struct opi_build_scope *scp)
{
  scp->nvars1 = bldr->decls.size - bldr->frame_offset;
  scp->ntypes1 = bldr->types->size;
  scp->ntraits1 = bldr->traits->size;
  scp->vasize1 = opi_alist_get_size(bldr->alist);
}

void
opi_builder_drop_scope(struct opi_builder *bldr, struct opi_build_scope *scp)
{
  size_t nvars1 = scp->nvars1;
  size_t nvars2 = bldr->decls.size - bldr->frame_offset;
  size_t nvars = nvars2 - nvars1;

  size_t ntypes1 = scp->ntypes1;
  size_t ntypes2 = bldr->types->size;
  size_t ntypes = ntypes2 - ntypes1;

  size_t ntraits1 = scp->ntraits1;
  size_t ntraits2 = bldr->traits->size;
  size_t ntraits = ntraits2 - ntraits1;

  size_t vasize1 = scp->vasize1;
  size_t vasize2 = opi_alist_get_size(bldr->alist);
  size_t vasize = vasize2 - vasize1;

  // pop declarations
  while (nvars--)
    opi_strvec_pop(&bldr->decls);
  // pop alist
  opi_alist_pop(bldr->alist, vasize);
  // pop types
  while (ntypes--) {
    opi_strvec_pop(bldr->type_names);
    opi_ptrvec_pop(bldr->types, NULL);
  }
  // pop traits
  while (ntraits--) {
    opi_strvec_pop(bldr->trait_names);
    opi_ptrvec_pop(bldr->traits, NULL);
  }
}

void
opi_builder_make_namespace(struct opi_builder *bldr, struct opi_build_scope *scp, const char *prefix)
{
  size_t nvars1 = scp->nvars1;
  size_t nvars2 = bldr->decls.size - bldr->frame_offset;
  size_t nvars = nvars2 - nvars1;

  size_t ntypes1 = scp->ntypes1;
  size_t ntypes2 = bldr->types->size;
  size_t ntypes = ntypes2 - ntypes1;

  size_t ntraits1 = scp->ntraits1;
  size_t ntraits2 = bldr->traits->size;
  size_t ntraits = ntraits2 - ntraits1;

  size_t vasize1 = scp->vasize1;
  size_t vasize2 = opi_alist_get_size(bldr->alist);
  size_t vasize = vasize2 - vasize1;

  size_t nslen = strlen(prefix);

  // drop alist
  opi_alist_pop(bldr->alist, vasize);

  // fetch namespace variables and prefix with namespace name
  for (size_t i = bldr->decls.size - nvars; i < bldr->decls.size; ++i) {
    // add namespace prefix
    size_t len = strlen(bldr->decls.data[i]) + nslen;
    char *newname = malloc(len + 1);
    sprintf(newname, "%s%s", prefix, bldr->decls.data[i]);
    // change declaration
    free(bldr->decls.data[i]);
    bldr->decls.data[i] = newname;

    // push new name in alist
    opi_alist_push(bldr->alist, newname, NULL);
  }

  // fetch namespace types and prefix with namespace name
  for (size_t i = bldr->type_names->size - ntypes; i < bldr->type_names->size; ++i) {
    // add namespace prefix
    size_t len = strlen(bldr->type_names->data[i]) + nslen;
    char *newname = malloc(len + 1);
    sprintf(newname, "%s%s", prefix, bldr->type_names->data[i]);
    // change declaration
    free(bldr->type_names->data[i]);
    bldr->type_names->data[i] = newname;
  }

  // fetch namespace traits and prefix with namespace name
  for (size_t i = bldr->trait_names->size - ntraits; i < bldr->trait_names->size; ++i) {
    // add namespace prefix
    size_t len = strlen(bldr->trait_names->data[i]) + nslen;
    char *newname = malloc(len + 1);
    sprintf(newname, "%s%s", prefix, bldr->trait_names->data[i]);
    // change declaration
    free(bldr->trait_names->data[i]);
    bldr->trait_names->data[i] = newname;
  }
}

struct opi_struct_data {
  size_t nfields;
};

static void
opi_struct_data_delete(opi_type_t type)
{ free(opi_type_get_data(type)); }

struct opi_struct {
  struct opi_header header;
  opi_t data[];
};

static void
opi_struct_delete(opi_type_t ty, opi_t x)
{
  struct opi_struct *s = opi_as_ptr(x);
  struct opi_struct_data *data = opi_type_get_data(ty);
  size_t nfields = data->nfields;
  for (size_t i = 0; i < nfields; ++i)
    opi_unref(s->data[i]);
  free(s);
}

static opi_t
make_struct(void)
{
  opi_type_t type = opi_fn_get_data(opi_current_fn);
  struct opi_struct_data *data = opi_type_get_data(type);
  size_t nfields = data->nfields;

  struct opi_struct *s = malloc(sizeof(struct opi_struct) + sizeof(opi_t) * nfields);
  for (size_t i = 0; i < nfields; ++i) {
    opi_t x = opi_get(i + 1);
    opi_inc_rc(s->data[i] = x);
  }
  opi_popn(nfields);
  opi_init_cell(s, type);
  return (opi_t)s;
}

static void
generic_delete(struct opi_fn *self)
{
  opi_trait_delete(self->data);
  opi_fn_delete(self);
}

static opi_t
generic(void)
{
  opi_t x = opi_get(1);
  struct opi_trait *trait = opi_fn_get_data(opi_current_fn);
  opi_t handle = opi_trait_find(trait, x->type);
  return opi_fn_apply(handle);
}

static opi_t
generic_default(void)
{
  opi_assert(!"unimplemented generic");
  abort();
}

struct impl_data {
  struct opi_trait *trait;
  opi_type_t type;
};

static void
impl_data_delete(struct opi_fn *self)
{
  struct impl_data *data = self->data;
  free(data);
  opi_fn_delete(self);
}

static opi_t
impl(void)
{
  struct impl_data *data = opi_fn_get_data(opi_current_fn);
  opi_t f = opi_pop();
  if (data->type)
    opi_trait_impl(data->trait, data->type, f);
  else
    opi_trait_set_default(data->trait, f);
  return opi_nil;
}

struct opi_ir*
opi_builder_build(struct opi_builder *bldr, struct opi_ast *ast)
{
  switch (ast->tag) {
    case OPI_AST_CONST:
      return opi_ir_const(ast->cnst);

    case OPI_AST_VAR:
    {
      opi_t const_val;
      size_t offs;

      const char *varname = opi_builder_assoc(bldr, ast->var);
      int var_idx = opi_strvec_rfind(&bldr->decls, varname);
      if (var_idx >= 0) {
        // # Found in local variables:
        offs = bldr->decls.size - var_idx;

      } else if ((const_val = opi_builder_find_const(bldr, varname))) {
        // # Found constant definition:
        return opi_ir_const(const_val);

      } else  {
        // # Otherwize, add to captures:
        // insert at the beginning of declarations => won't change other offsets
        opi_builder_capture(bldr, varname);
        offs = bldr->decls.size;
      }

      return opi_ir_var(offs);
    }

    case OPI_AST_USE:
    {
      opi_alist_push(bldr->alist, ast->use.new, ast->use.old);
      return opi_ir_const(opi_nil);
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
      opi_builder_init_derived(&fn_bldr, bldr);

      // declare parameters as fn-local variables
      for (int i = ast->fn.nargs - 1; i >= 0; --i)
        opi_builder_push_decl(&fn_bldr, ast->fn.args[i]);

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

      for (size_t i = 0; i < ast->fn.nargs; ++i)
        opi_builder_pop_decl(&fn_bldr);

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
        opi_builder_push_decl(bldr, ast->let.vars[i]);

      if (ast->let.body) {
        // evaluate body
        struct opi_ir *body = opi_builder_build(bldr, ast->let.body);
        // hide variables
        for (size_t i = 0; i < ast->let.n; ++i)
          opi_builder_pop_decl(bldr);

        return opi_ir_let(vals, ast->let.n, body);
      } else {

        return opi_ir_let(vals, ast->let.n, NULL);
      }
    }

    case OPI_AST_FIX:
    {
      struct opi_ir *vals[ast->let.n];

      // declare functions
      for (size_t i = 0; i < ast->let.n; ++i)
        opi_builder_push_decl(bldr, ast->let.vars[i]);
      // evaluate lambdas
      for (size_t i = 0; i < ast->let.n; ++i)
        vals[i] = opi_builder_build(bldr, ast->let.vals[i]);

      if (ast->let.body) {
        // evaluate body
        struct opi_ir *body = opi_builder_build(bldr, ast->let.body);
        // hide functions
        for (size_t i = 0; i < ast->let.n; ++i)
          opi_builder_pop_decl(bldr);

        return opi_ir_fix(vals, ast->let.n, body);

      } else {
        return opi_ir_fix(vals, ast->let.n, NULL);
      }
    }

    case OPI_AST_IF:
      return opi_ir_if(
          opi_builder_build(bldr, ast->iff.test),
          opi_builder_build(bldr, ast->iff.then),
          opi_builder_build(bldr, ast->iff.els));

    case OPI_AST_BLOCK:
    {
      struct opi_build_scope scp;
      opi_builder_begin_scope(bldr, &scp);

      struct opi_ir *exprs[ast->block.n];
      for (size_t i = 0; i < ast->block.n; ++i)
        exprs[i] = opi_builder_build(bldr, ast->block.exprs[i]);

      if (ast->block.drop)
        opi_builder_drop_scope(bldr, &scp);
      else if (ast->block.namespace)
        opi_builder_make_namespace(bldr, &scp, ast->block.namespace);

      struct opi_ir *ret = opi_ir_block(exprs, ast->block.n);
      opi_ir_block_set_drop(ret, ast->block.drop);
      return ret;
    }

    case OPI_AST_LOAD:
    {
      size_t file_id;

      // find absolute path
      char path[PATH_MAX];
      if (!opi_builder_find_path(bldr, ast->load, path)) {
        opi_error("no such file: \"%s\"\n", ast->load);
        opi_error("source directories:\n");
        for (size_t i = 0; i < bldr->srcdirs->size; ++i)
          opi_error("  %2.zu. %s\n", i + 1, bldr->srcdirs->data[i]);
        exit(EXIT_FAILURE);
      }

      // check if already loaded
      int found = FALSE;
      for (size_t i = 0; i < bldr->loaded->size; ++i) {
        if (strcmp(bldr->loaded->data[i], path) == 0) {
          // file already visited, test for cyclic dependence
          if (strcmp(bldr->load_state->data[i], "loading") == 0) {
            opi_error("cyclic dependence detected while loading \"%s\"\n", path);
            exit(EXIT_FAILURE);
          }

          found = TRUE;
          break;
        }
      }

      if (!found) {
        opi_debug("load \"%s\"\n", path);

        // mark file before building contents
        size_t id = bldr->loaded->size;
        opi_strvec_push(bldr->loaded, path);
        opi_strvec_push(bldr->load_state, "loading");

        // load file
        FILE *in = fopen(path, "r");
        opi_assert(in);
        struct opi_ast *subast = opi_parse(in);
        fclose(in);
        opi_assert(subast->tag == OPI_AST_BLOCK);
        opi_ast_block_set_drop(subast, FALSE);
        struct opi_ir *ir = opi_builder_build(bldr, subast);
        opi_assert(ir->tag == OPI_IR_BLOCK);
        opi_assert(ir->block.drop == FALSE);
        opi_ast_delete(subast);

        // mark file as loaded
        free(bldr->load_state->data[id]);
        bldr->load_state->data[id] = strdup("ready");

        return ir;

      } else {
        return opi_ir_const(opi_nil);
      }
    }

    case OPI_AST_MATCH:
    {
      // find type
      const char *typename = opi_builder_assoc(bldr, ast->match.type);
      opi_type_t type = opi_builder_find_type(bldr, typename);
      if (type == NULL) {
        opi_error("no such type, '%s'\n", ast->match.type);
        exit(EXIT_FAILURE);
      }

      // load fields
      size_t offsets[ast->match.n];
      for (size_t i = 0; i < ast->match.n; ++i) {
        int field_idx;
        if (ast->match.fields[i][0] == '#')
          field_idx = atoi(ast->match.fields[i] + 1);
        else
          field_idx = opi_type_get_field_idx(type, ast->match.fields[i]);
        opi_assert(field_idx >= 0);
        offsets[i] = opi_type_get_field_offset(type, field_idx);
      }

      // evaluate expr
      struct opi_ir *expr = opi_builder_build(bldr, ast->match.expr);

      // declare variables
      for (size_t i = 0; i < ast->match.n; ++i)
        opi_builder_push_decl(bldr, ast->match.vars[i]);

      struct opi_ir *then = NULL, *els = NULL;
      if (ast->match.then) {
        // eval then-branch
        then = opi_builder_build(bldr, ast->match.then);
        // hide variables
        for (size_t i = 0; i < ast->match.n; ++i)
          opi_builder_pop_decl(bldr);
        // eval else-branch
        els = opi_builder_build(bldr, ast->match.els);
      }

      return opi_ir_match(type, offsets, ast->match.n, expr, then, els);
    }

    case OPI_AST_STRUCT:
    {
      // create type
      opi_type_t type = opi_type(ast->strct.typename);
      struct opi_struct_data *data = malloc(sizeof(struct opi_struct_data));
      data->nfields = ast->strct.nfields;
      opi_type_set_data(type, data, opi_struct_data_delete);
      size_t offset = offsetof(struct opi_struct, data);
      opi_type_set_fields(type, offset, ast->strct.fields, ast->strct.nfields);
      opi_type_set_delete_cell(type, opi_struct_delete);

      // create constructor
      opi_t ctor = opi_fn(ast->strct.typename, make_struct, ast->strct.nfields);
      opi_fn_set_data(ctor, type, NULL);

      // declare type
      opi_builder_add_type(bldr, ast->strct.typename, type);

      // declare constructor
      opi_builder_push_decl(bldr, ast->strct.typename);
      struct opi_ir *ctor_ir = opi_ir_const(ctor);
      return opi_ir_let(&ctor_ir, 1, NULL);
    }

    case OPI_AST_TRAIT:
    {
      // create default handle
      opi_assert(ast->trait.deflt->tag == OPI_AST_FN);
      int arity = ast->trait.deflt->fn.nargs;
      struct opi_ir *true_default = opi_builder_build(bldr, ast->trait.deflt);
      opi_t deflt = opi_fn("generic_deflt", generic_default, arity);

      // create trait
      struct opi_trait *trait = opi_trait(deflt);
      opi_builder_add_trait(bldr, ast->trait.name, trait);

      // create generic function
      opi_t generic_fn = opi_fn(ast->trait.name, generic, arity);
      opi_fn_set_data(generic_fn, trait, generic_delete);

      // declare generic function
      opi_builder_push_decl(bldr, ast->trait.name);
      struct opi_ir *fn_ir = opi_ir_const(generic_fn);

      // default implementation
      struct impl_data *data = malloc(sizeof(struct impl_data));
      data->trait = trait;
      data->type = NULL;
      opi_t impl_fn = opi_fn("impl-default", impl, 1);
      opi_fn_set_data(impl_fn, data, impl_data_delete);

      // auxilary block
      struct opi_ir *body[] = {
        opi_ir_let(&fn_ir, 1, NULL),
        opi_ir_apply(opi_ir_const(impl_fn), &true_default, 1),
      };
      struct opi_ir *block = opi_ir_block(body, 2);
      opi_ir_block_set_drop(block, FALSE);
      return block;
    }

    case OPI_AST_IMPL:
    {
      // find trait
      const char *traitname = opi_builder_assoc(bldr, ast->impl.traitname);
      struct opi_trait *trait = opi_builder_find_trait(bldr, traitname);
      opi_assert(trait);

      // find type
      const char *typename = opi_builder_assoc(bldr, ast->impl.typename);
      struct opi_type *type = opi_builder_find_type(bldr, typename);
      opi_assert(type);

      // create implementer
      struct impl_data *data = malloc(sizeof(struct impl_data));
      data->trait = trait;
      data->type = type;
      opi_t impl_fn = opi_fn("impl", impl, 1);
      opi_fn_set_data(impl_fn, data, impl_data_delete);
      
      // apply implementer
      opi_assert(ast->impl.fn->tag == OPI_AST_FN);
      opi_assert(ast->impl.fn->fn.nargs == opi_trait_get_arity(trait));
      struct opi_ir *handle = opi_builder_build(bldr, ast->impl.fn);
      return opi_ir_apply(opi_ir_const(impl_fn), &handle, 1);
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
    case OPI_IR_FIX:
      for (size_t i = 0; i < node->let.n; ++i)
        opi_ir_delete(node->let.vals[i]);
      free(node->let.vals);
      if (node->let.body)
        opi_ir_delete(node->let.body);
      break;

    case OPI_IR_IF:
      opi_ir_delete(node->iff.test);
      opi_ir_delete(node->iff.then);
      opi_ir_delete(node->iff.els);
      break;

    case OPI_IR_BLOCK:
      for (size_t i = 0; i < node->block.n; ++i)
        opi_ir_delete(node->block.exprs[i]);
      free(node->block.exprs);
      break;

    case OPI_IR_MATCH:
      free(node->match.offs);
      opi_ir_delete(node->match.expr);
      if (node->match.then)
        opi_ir_delete(node->match.then);
      if (node->match.els)
        opi_ir_delete(node->match.els);
      break;
  }

  free(node);
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

struct opi_ir*
opi_ir_fix(struct opi_ir **vals, size_t n, struct opi_ir *body)
{
  struct opi_ir *ir = opi_ir_let(vals, n, body);
  ir->tag = OPI_IR_FIX;
  return ir;
}

struct opi_ir*
opi_ir_if(struct opi_ir *test, struct opi_ir *then, struct opi_ir *els)
{
  struct opi_ir *node = malloc(sizeof(struct opi_ir));
  node->tag = OPI_IR_IF;
  node->iff.test = test;
  node->iff.then = then;
  node->iff.els  = els;
  return node;
}


struct opi_ir*
opi_ir_block(struct opi_ir **exprs, size_t n)
{
  struct opi_ir *node = malloc(sizeof(struct opi_ir));
  node->tag = OPI_IR_BLOCK;
  node->block.exprs = malloc(sizeof(struct opi_ir*) *  n);
  memcpy(node->block.exprs, exprs, sizeof(struct opi_ir*) * n);
  node->block.n = n;
  node->block.drop = TRUE;
  return node;
}

struct opi_ir*
opi_ir_match(opi_type_t type, size_t *offs, size_t n, struct opi_ir *expr,
    struct opi_ir *then, struct opi_ir *els)
{
  struct opi_ir *node = malloc(sizeof(struct opi_ir));
  node->tag = OPI_IR_MATCH;
  node->match.type = type;
  node->match.offs = malloc(sizeof(size_t) * n);
  memcpy(node->match.offs, offs, sizeof(size_t) * n);
  node->match.n = n;
  node->match.expr = expr;
  node->match.then = then;
  node->match.els = els;
  return node;
}

