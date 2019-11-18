#include "opium/opium.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dlfcn.h>
#include <unistd.h>
#include <libgen.h>

void
opi_alist_init(struct opi_alist *a)
{
  cod_strvec_init(&a->keys);
  cod_strvec_init(&a->vals);
}

void
opi_alist_destroy(struct opi_alist *a)
{
  cod_strvec_destroy(&a->keys);
  cod_strvec_destroy(&a->vals);
}

size_t
opi_alist_get_size(struct opi_alist *a)
{ return a->keys.size; }

void
opi_alist_push(struct opi_alist *a, const char *var, const char *map)
{
  cod_strvec_push(&a->keys, var);
  cod_strvec_push(&a->vals, map ? map : var);
}

void
opi_alist_pop(struct opi_alist *a, size_t n)
{
  opi_assert(n <= opi_alist_get_size(a));
  while (n--) {
    cod_strvec_pop(&a->keys);
    cod_strvec_pop(&a->vals);
  }
}

void
opi_builder_init(struct opi_builder *bldr, struct opi_context *ctx)
{
  bldr->is_derived = FALSE;
  bldr->ctx = ctx;

  cod_strvec_init(&bldr->decls);
  bldr->frame_offset = 0;

  opi_alist_init(bldr->alist = malloc(sizeof(struct opi_alist)));

  cod_strvec_init(bldr->const_names = malloc(sizeof(struct cod_strvec)));
  cod_ptrvec_init(bldr->const_vals = malloc(sizeof(struct cod_ptrvec)));

  cod_strvec_init(bldr->srcdirs = malloc(sizeof(struct cod_strvec)));

  cod_strvec_init(bldr->loaded = malloc(sizeof(struct cod_strvec)));
  cod_strvec_init(bldr->load_state = malloc(sizeof(struct cod_strvec)));

  cod_strvec_init(bldr->type_names = malloc(sizeof(struct cod_strvec)));
  cod_ptrvec_init(bldr->types = malloc(sizeof(struct cod_ptrvec)));

  opi_builder_def_type(bldr, "undefined", opi_undefined_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "number", opi_number_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "symbol", opi_symbol_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "null", opi_null_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "string", opi_string_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "boolean", opi_boolean_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "pair", opi_pair_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "fn", opi_fn_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "lazy", opi_lazy_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "FILE", opi_file_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
}

void
opi_builder_init_derived(struct opi_builder *bldr, struct opi_builder *parent)
{
  bldr->is_derived = TRUE;
  bldr->ctx = parent->ctx;

  cod_strvec_init(&bldr->decls);
  bldr->frame_offset = 0;

  bldr->alist = parent->alist;

  bldr->const_names = parent->const_names;
  bldr->const_vals = parent->const_vals;

  bldr->srcdirs = parent->srcdirs;

  bldr->loaded = parent->loaded;
  bldr->load_state = parent->load_state;

  bldr->type_names = parent->type_names;
  bldr->types = parent->types;
}

void
opi_builder_destroy(struct opi_builder *bldr)
{
  cod_strvec_destroy(&bldr->decls);
  if (bldr->is_derived) {
    /*opi_alist_pop(bldr->alist, bldr->frame_offset);*/
    return;
  }

  opi_alist_destroy(bldr->alist);
  free(bldr->alist);

  cod_strvec_destroy(bldr->const_names);
  free(bldr->const_names);
  cod_ptrvec_destroy(bldr->const_vals, (void*)opi_unref);
  free(bldr->const_vals);

  cod_strvec_destroy(bldr->srcdirs);
  free(bldr->srcdirs);
  cod_strvec_destroy(bldr->loaded);
  free(bldr->loaded);
  cod_strvec_destroy(bldr->load_state);
  free(bldr->load_state);

  cod_strvec_destroy(bldr->type_names);
  free(bldr->type_names);
  cod_ptrvec_destroy(bldr->types, NULL);
  free(bldr->types);
}

void
opi_builder_add_source_directory(struct opi_builder *bldr, const char *path)
{
  char fullpath[PATH_MAX];
  opi_assert(realpath(path, fullpath));
  cod_strvec_push(bldr->srcdirs, fullpath);
}

char*
opi_builder_find_path(struct opi_builder *bldr, const char *path, char *fullpath)
{
  if (realpath(path, fullpath))
    return fullpath;

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
  cod_strvec_push(bldr->type_names, name);
  cod_ptrvec_push(bldr->types, type, NULL);
  opi_context_add_type(bldr->ctx, type);
}

opi_type_t
opi_builder_find_type(struct opi_builder *bldr, const char *typename)
{
  long long int idx = cod_strvec_find(bldr->type_names, typename);
  return idx < 0 ? NULL : bldr->types->data[idx];
}

void
opi_builder_def_const(struct opi_builder *bldr, const char *name, opi_t val)
{
  cod_strvec_push(bldr->const_names, name);
  cod_ptrvec_push(bldr->const_vals, val, NULL);
  opi_alist_push(bldr->alist, name, NULL);
  opi_inc_rc(val);
}

void
opi_builder_def_type(struct opi_builder *bldr, const char *name, opi_type_t type)
{
  opi_builder_add_type(bldr, name, type);
  opi_alist_push(bldr->alist, name, NULL);
}

opi_t
opi_builder_find_const(struct opi_builder *bldr, const char *name)
{
  long long int idx = cod_strvec_rfind(bldr->const_names, name);
  return idx < 0 ? NULL : bldr->const_vals->data[idx];
}

typedef int (*build_t)(struct opi_builder*);
void
opi_builder_load_dl(struct opi_builder *bldr, void *dl)
{
  build_t build = dlsym(dl, "opium_library");
  if (!build) {
    opi_error(dlerror());
    exit(EXIT_FAILURE);
  }
  opi_assert(build(bldr) == 0);
}

void
opi_builder_push_decl(struct opi_builder *bldr, const char *var)
{
  cod_strvec_push(&bldr->decls, var);
  opi_alist_push(bldr->alist, var, NULL);
}

void
opi_builder_pop_decl(struct opi_builder *bldr)
{
  const char *var = bldr->alist->keys.data[bldr->alist->keys.size-1];
  const char *decl = bldr->decls.data[bldr->decls.size - 1];
  /*opi_debug("var = %s, decl = %s\n", var, decl);*/
  opi_assert(strcmp(var, decl) == 0);
  cod_strvec_pop(&bldr->decls);
  opi_alist_pop(bldr->alist, 1);
}

void
opi_builder_capture(struct opi_builder *bldr, const char *var)
{
  cod_strvec_insert(&bldr->decls, var, 0);
  /*opi_alist_push(bldr->alist, var, NULL);*/
  bldr->frame_offset += 1;
}

const char*
opi_builder_assoc(struct opi_builder *bldr, const char *var)
{
  int idx = cod_strvec_rfind(&bldr->alist->keys, var);
  if (idx < 0) {
    opi_error("no such variable, '%s'\n", var);
    exit(EXIT_FAILURE);
  }
  return bldr->alist->vals.data[idx];
}

const char*
opi_builder_try_assoc(struct opi_builder *bldr, const char *var)
{
  int idx = cod_strvec_rfind(&bldr->alist->keys, var);
  if (idx < 0)
    return NULL;
  return bldr->alist->vals.data[idx];
}

void
opi_builder_begin_scope(struct opi_builder *bldr, struct opi_build_scope *scp)
{
  scp->nvars1 = bldr->decls.size - bldr->frame_offset;
  scp->ntypes1 = bldr->types->size;
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

  size_t vasize1 = scp->vasize1;
  size_t vasize2 = opi_alist_get_size(bldr->alist);
  size_t vasize = vasize2 - vasize1;

  // pop declarations
  while (nvars--)
    cod_strvec_pop(&bldr->decls);
  // pop alist
  opi_alist_pop(bldr->alist, vasize);
  // pop types
  while (ntypes--) {
    cod_strvec_pop(bldr->type_names);
    cod_ptrvec_pop(bldr->types, NULL);
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

struct opi_ir*
opi_builder_build_ir(struct opi_builder *bldr, struct opi_ast *ast)
{
  switch (ast->tag) {
    case OPI_AST_CONST:
      return opi_ir_const(ast->cnst);

    case OPI_AST_VAR:
    {
      opi_t const_val;
      size_t offs;

      char *name0 = NULL;
      char *p = strchr(ast->var, ':');
      if (p && p[1] == ':') {
        size_t len = p - ast->var;
        char namespace[len + 1];
        memcpy(namespace, ast->var, len);
        namespace[len] = 0;
        const char *map = opi_builder_try_assoc(bldr, namespace);
        if (map) {
          len =  strlen(map) + 2 + strlen(p + 2);
          name0 = malloc(len + 1);
          sprintf(name0, "%s::%s", map, p + 2);
        }
      }

      const char *varname = opi_builder_assoc(bldr, name0 ? name0 : ast->var);
      if (name0)
        free(name0);

      int var_idx = cod_strvec_rfind(&bldr->decls, varname);
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
      if (strcmp(ast->use.new, "*") == 0) {
        char *prefix = ast->use.old;
        size_t prefixlen = strlen(ast->use.old);
        for (size_t i = 0, n = opi_alist_get_size(bldr->alist); i < n; ++i) {
          char *vname = bldr->alist->vals.data[i];
          if (strncmp(prefix, vname, prefixlen) == 0)
            opi_alist_push(bldr->alist, vname + prefixlen, vname);
        }
      } else {
        opi_alist_push(bldr->alist, ast->use.new, ast->use.old);
      }
      return opi_ir_const(opi_nil);
    }

    case OPI_AST_APPLY:
    {
      struct opi_ir *fn = opi_builder_build_ir(bldr, ast->apply.fn);
      struct opi_ir *args[ast->apply.nargs];
      for (size_t i = 0; i < ast->apply.nargs; ++i)
        args[i] = opi_builder_build_ir(bldr, ast->apply.args[i]);
      struct opi_ir *ret = opi_ir_apply(fn, args, ast->apply.nargs);
      ret->apply.eflag = ast->apply.eflag;
      return ret;
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
      struct opi_ir *body = opi_builder_build_ir(&fn_bldr, ast->fn.body);

      // process captures
      size_t ncaps = fn_bldr.frame_offset;
      struct opi_ir *caps[ncaps];
      for (size_t i = 0; i < ncaps; ++i) {
        // build expression to get captured variable
        struct opi_ast *tmp_var = opi_ast_var(fn_bldr.decls.data[i]);
        caps[i] = opi_builder_build_ir(bldr, tmp_var);
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
        vals[i] = opi_builder_build_ir(bldr, ast->let.vals[i]);
      // declare variables
      for (size_t i = 0; i < ast->let.n; ++i)
        opi_builder_push_decl(bldr, ast->let.vars[i]);

      if (ast->let.body) {
        // evaluate body
        struct opi_ir *body = opi_builder_build_ir(bldr, ast->let.body);
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
        vals[i] = opi_builder_build_ir(bldr, ast->let.vals[i]);

      if (ast->let.body) {
        // evaluate body
        struct opi_ir *body = opi_builder_build_ir(bldr, ast->let.body);
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
          opi_builder_build_ir(bldr, ast->iff.test),
          opi_builder_build_ir(bldr, ast->iff.then),
          opi_builder_build_ir(bldr, ast->iff.els));

    case OPI_AST_BLOCK:
    {
      struct opi_build_scope scp;
      opi_builder_begin_scope(bldr, &scp);

      struct opi_ir *exprs[ast->block.n];
      for (size_t i = 0; i < ast->block.n; ++i)
        exprs[i] = opi_builder_build_ir(bldr, ast->block.exprs[i]);

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
        /*opi_debug("load \"%s\"\n", path);*/

        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);

        char tmp[PATH_MAX];
        strcpy(tmp, path);
        char *dir = dirname(tmp);
        /*opi_debug("chdir %s\n", dir);*/
        opi_assert(chdir(dir) == 0);

        struct opi_ir *ret;

        if (opi_is_dl(path)) {
          void *dl = opi_context_find_dl(bldr->ctx, path);
          if (!dl) {
            dl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
            if (!dl) {
              opi_error(dlerror());
              exit(EXIT_FAILURE);
            }
            opi_context_add_dl(bldr->ctx, path, dl);
          }
          opi_builder_load_dl(bldr, dl);
          ret = opi_ir_const(opi_nil);

        } else {
          // mark file before building contents
          size_t id = bldr->loaded->size;
          cod_strvec_push(bldr->loaded, path);
          cod_strvec_push(bldr->load_state, "loading");

          // load file
          FILE *in = fopen(path, "r");
          opi_assert(in);
          struct opi_ast *subast = opi_parse(in);
          fclose(in);
          opi_assert(subast->tag == OPI_AST_BLOCK);
          opi_ast_block_set_drop(subast, FALSE);
          struct opi_ir *ir = opi_builder_build_ir(bldr, subast);
          opi_assert(ir->tag == OPI_IR_BLOCK);
          opi_assert(ir->block.drop == FALSE);
          opi_ast_delete(subast);

          // mark file as loaded
          free(bldr->load_state->data[id]);
          bldr->load_state->data[id] = strdup("ready");

          ret = ir;
        }

        /*opi_debug("chdir %s\n", cwd);*/
        opi_assert(chdir(cwd) == 0);
        return ret;

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
      struct opi_ir *expr = opi_builder_build_ir(bldr, ast->match.expr);

      // declare variables
      for (size_t i = 0; i < ast->match.n; ++i)
        opi_builder_push_decl(bldr, ast->match.vars[i]);

      struct opi_ir *then = NULL, *els = NULL;
      if (ast->match.then) {
        // eval then-branch
        then = opi_builder_build_ir(bldr, ast->match.then);
        // hide variables
        for (size_t i = 0; i < ast->match.n; ++i)
          opi_builder_pop_decl(bldr);
        // eval else-branch
        els = opi_builder_build_ir(bldr, ast->match.els);
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

    case OPI_AST_RETURN:
      return opi_ir_return(opi_builder_build_ir(bldr, ast->ret));

    case OPI_AST_BINOP:
      return opi_ir_binop(ast->binop.opc,
          opi_builder_build_ir(bldr, ast->binop.lhs),
          opi_builder_build_ir(bldr, ast->binop.rhs)
      );
  }

  abort();
}

void
opi_build(struct opi_builder *bldr, struct opi_ast *ast, struct opi_bytecode* bc)
{
  struct opi_ir *ir = opi_builder_build_ir(bldr, ast);
  opi_assert(bldr->frame_offset == 0);
  opi_ir_emit(ir, bc);
  opi_ir_delete(ir);
  opi_bytecode_finalize(bc);
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

    case OPI_IR_RETURN:
      opi_ir_delete(node->ret);
      break;

    case OPI_IR_BINOP:
      opi_ir_delete(node->binop.lhs);
      opi_ir_delete(node->binop.rhs);
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
  node->apply.eflag = TRUE;
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

struct opi_ir*
opi_ir_return(struct opi_ir *val)
{
  struct opi_ir *node = malloc(sizeof(struct opi_ir));
  node->tag = OPI_IR_RETURN;
  node->ret = val;
  return node;
}

struct opi_ir*
opi_ir_binop(int opc, struct opi_ir *lhs, struct opi_ir *rhs)
{
  struct opi_ir *node = malloc(sizeof(struct opi_ir));
  node->tag = OPI_IR_BINOP;
  node->binop.opc = opc;
  node->binop.lhs = lhs;
  node->binop.rhs = rhs;
  return node;
}

