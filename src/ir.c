#include "opium/opium.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dlfcn.h>
#include <unistd.h>
#include <libgen.h>

void
opi_alist_init(OpiAlist *a)
{
  cod_strvec_init(&a->keys);
  cod_strvec_init(&a->vals);
}

void
opi_alist_destroy(OpiAlist *a)
{
  cod_strvec_destroy(&a->keys);
  cod_strvec_destroy(&a->vals);
}

size_t
opi_alist_get_size(OpiAlist *a)
{ return a->keys.size; }

void
opi_alist_push(OpiAlist *a, const char *var, const char *map)
{
  cod_strvec_push(&a->keys, var);
  cod_strvec_push(&a->vals, map ? map : var);
}

void
opi_alist_pop(OpiAlist *a, size_t n)
{
  opi_assert(n <= opi_alist_get_size(a));
  while (n--) {
    cod_strvec_pop(&a->keys);
    cod_strvec_pop(&a->vals);
  }
}

void
opi_builder_init(OpiBuilder *bldr, OpiContext *ctx)
{
  bldr->parent = NULL;
  bldr->ctx = ctx;

  cod_strvec_init(&bldr->decls);
  bldr->frame_offset = 0;

  opi_alist_init(bldr->alist = malloc(sizeof(OpiAlist)));

  cod_strvec_init(bldr->const_names = malloc(sizeof(struct cod_strvec)));
  cod_ptrvec_init(bldr->const_vals = malloc(sizeof(struct cod_ptrvec)));

  cod_strvec_init(bldr->srcdirs = malloc(sizeof(struct cod_strvec)));

  cod_strvec_init(bldr->loaded = malloc(sizeof(struct cod_strvec)));
  cod_strvec_init(bldr->load_state = malloc(sizeof(struct cod_strvec)));

  cod_strvec_init(bldr->type_names = malloc(sizeof(struct cod_strvec)));
  cod_ptrvec_init(bldr->types = malloc(sizeof(struct cod_ptrvec)));

  opi_builder_def_type(bldr, "Undefined", opi_undefined_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "Num", opi_num_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "Sym", opi_symbol_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "Nil", opi_nil_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "Str", opi_string_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "Bool", opi_boolean_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "Cons", opi_pair_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "Fn", opi_fn_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "Lazy", opi_lazy_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "File", opi_file_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "Array", opi_array_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "svector", opi_svector_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
  opi_builder_def_type(bldr, "dvector", opi_dvector_type);
  cod_ptrvec_pop(&bldr->ctx->types, NULL);
}

void
opi_builder_init_derived(OpiBuilder *bldr, OpiBuilder *parent)
{
  bldr->parent = parent;
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
opi_builder_destroy(OpiBuilder *bldr)
{
  cod_strvec_destroy(&bldr->decls);
  if (opi_builder_is_derived(bldr)) {
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
opi_builder_add_source_directory(OpiBuilder *bldr, const char *path)
{
  char fullpath[PATH_MAX];
  opi_assert(realpath(path, fullpath));
  cod_strvec_push(bldr->srcdirs, fullpath);
}

char*
opi_builder_find_path(OpiBuilder *bldr, const char *path, char *fullpath)
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

int
opi_builder_add_type(OpiBuilder *bldr, const char *name, opi_type_t type)
{
  if (opi_builder_find_type(bldr, name)) {
    opi_error("type named '%s' already present\n", name);
    opi_error = 1;
    return OPI_ERR;
  }
  cod_strvec_push(bldr->type_names, name);
  cod_ptrvec_push(bldr->types, type, NULL);
  opi_context_add_type(bldr->ctx, type);
  return OPI_OK;
}

opi_type_t
opi_builder_find_type(OpiBuilder *bldr, const char *typename)
{
  long long int idx = cod_strvec_find(bldr->type_names, typename);
  return idx < 0 ? NULL : bldr->types->data[idx];
}

void
opi_builder_def_const(OpiBuilder *bldr, const char *name, opi_t val)
{
  cod_strvec_push(bldr->const_names, name);
  cod_ptrvec_push(bldr->const_vals, val, NULL);
  opi_alist_push(bldr->alist, name, NULL);
  opi_inc_rc(val);
}

int
opi_builder_def_type(OpiBuilder *bldr, const char *name, opi_type_t type)
{
  if (opi_builder_add_type(bldr, name, type) == OPI_ERR)
    return OPI_ERR;
  opi_alist_push(bldr->alist, name, NULL);
  return OPI_OK;
}

opi_t
opi_builder_find_const(OpiBuilder *bldr, const char *name)
{
  long long int idx = cod_strvec_rfind(bldr->const_names, name);
  return idx < 0 ? NULL : bldr->const_vals->data[idx];
}

typedef int (*build_t)(OpiBuilder*);
int
opi_builder_load_dl(OpiBuilder *bldr, void *dl)
{
  build_t build = dlsym(dl, "opium_library");
  if (!build) {
    opi_error(dlerror());
    opi_error = 1;
    return OPI_ERR;
  }
  opi_assert(build(bldr) == 0);
  return OPI_OK;
}

void
opi_builder_push_decl(OpiBuilder *bldr, const char *var)
{
  cod_strvec_push(&bldr->decls, var);
  opi_alist_push(bldr->alist, var, NULL);
}

void
opi_builder_pop_decl(OpiBuilder *bldr)
{
  const char *var = bldr->alist->keys.data[bldr->alist->keys.size-1];
  const char *decl = bldr->decls.data[bldr->decls.size - 1];
  /*opi_debug("var = %s, decl = %s\n", var, decl);*/
  opi_assert(strcmp(var, decl) == 0);
  cod_strvec_pop(&bldr->decls);
  opi_alist_pop(bldr->alist, 1);
}

void
opi_builder_capture(OpiBuilder *bldr, const char *var)
{
  cod_strvec_insert(&bldr->decls, var, 0);
  /*opi_alist_push(bldr->alist, var, NULL);*/
  bldr->frame_offset += 1;
}

const char*
opi_builder_assoc(OpiBuilder *bldr, const char *var)
{
  int idx = cod_strvec_rfind(&bldr->alist->keys, var);
  if (idx < 0) {
    opi_error("no such variable, '%s'\n", var);
    opi_error = 1;
    return NULL;
  }
  return bldr->alist->vals.data[idx];
}

const char*
opi_builder_try_assoc(OpiBuilder *bldr, const char *var)
{
  int idx = cod_strvec_rfind(&bldr->alist->keys, var);
  if (idx < 0)
    return NULL;
  return bldr->alist->vals.data[idx];
}

void
opi_builder_begin_scope(OpiBuilder *bldr, OpiScope *scp)
{
  scp->nvars1 = bldr->decls.size - bldr->frame_offset;
  scp->ntypes1 = bldr->types->size;
  scp->vasize1 = opi_alist_get_size(bldr->alist);
}

void
opi_builder_drop_scope(OpiBuilder *bldr, OpiScope *scp)
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
opi_builder_make_namespace(OpiBuilder *bldr, OpiScope *scp, const char *prefix)
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

static void
opi_struct_data_delete(opi_type_t type)
{ free(opi_type_get_data(type)); }

struct opi_struct {
  OpiHeader header;
  opi_t data[];
};

static void
opi_struct_delete(opi_type_t type, opi_t x)
{
  struct opi_struct *s = opi_as_ptr(x);
  size_t nfields = opi_type_get_nfields(type);
  for (size_t i = 0; i < nfields; ++i)
    opi_unref(s->data[i]);
  free(s);
}

static opi_t
make_struct(void)
{
  opi_type_t type = opi_fn_get_data(opi_current_fn);
  size_t nfields = opi_type_get_nfields(type);

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
write_struct(opi_type_t type, opi_t x, FILE *out)
{
  size_t nfields = opi_type_get_nfields(type);
  struct opi_struct *s = opi_as_ptr(x);
  char *const *fields = opi_type_get_fields(type);
  fprintf(out, "%s { ", opi_type_get_name(type));
  for (size_t i = 0; i < nfields; ++i) {
    if (i != 0)
      fputs(", ", out);
    fprintf(out, "%s: ", fields[i]);
    opi_write(s->data[i], out);
  }
  fputs(" }", out);
}

static OpiIr*
build_error()
{
  opi_error = 1;
  return opi_ir_return(opi_ir_const(opi_symbol("build-error")));
}

static OpiIrPattern*
build_pattern(OpiBuilder *bldr, OpiAstPattern *pattern)
{
  switch (pattern->tag) {
    case OPI_PATTERN_IDENT:
    {
      // declare variable
      opi_builder_push_decl(bldr, pattern->ident);
      return opi_ir_pattern_new_ident();
    }

    case OPI_PATTERN_UNPACK:
    {
      // resolve type
      const char *typename = opi_builder_assoc(bldr, pattern->unpack.type);
      if (!typename)
        return NULL;
      opi_type_t type = opi_builder_find_type(bldr, typename);
      if (type == NULL) {
        opi_error("no such type, '%s'\n", pattern->unpack.type);
        return NULL;
      }

      // match sub-patterns
      size_t offsets[pattern->unpack.n];
      OpiIrPattern *subs[pattern->unpack.n];
      for (size_t i = 0; i < pattern->unpack.n; ++i) {
        // resolve field index
        int field_idx;
        if (pattern->unpack.fields[i][0] == '#')
          // index is set explicitly
          field_idx = atoi(pattern->unpack.fields[i] + 1);
        else
          // find index by field-name
          field_idx = opi_type_get_field_idx(type, pattern->unpack.fields[i]);
        opi_assert(field_idx >= 0);

        // get field offset
        offsets[i] = opi_type_get_field_offset(type, field_idx);

        // build sub-pattern
        if (!(subs[i] = build_pattern(bldr, pattern->unpack.subs[i]))) {
          // error
          while (i--)
            opi_ir_pattern_delete(subs[i]);
          return NULL;
        }
      }

      // create IR-pattern
      return opi_ir_pattern_new_unpack(type, subs, offsets, pattern->unpack.n);
    }
  }
  abort();
}

int
opi_builder_find_deep(OpiBuilder *bldr, const char *var)
{
  return cod_strvec_rfind(&bldr->decls, var) >= 0 ||
         (bldr->parent && opi_builder_find_deep(bldr->parent, var));
}

OpiIr*
opi_builder_build_ir(OpiBuilder *bldr, OpiAst *ast)
{
  switch (ast->tag) {
    case OPI_AST_CONST:
      return opi_ir_const(ast->cnst);

    case OPI_AST_VAR:
    {
      opi_t const_val;
      size_t offs;

      char *name0 = NULL;
      char *p = strchr(ast->var, '.');
      if (p) {
        size_t len = p - ast->var;
        char namespace[len + 1];
        memcpy(namespace, ast->var, len);
        namespace[len] = 0;
        const char *map = opi_builder_try_assoc(bldr, namespace);
        if (map) {
          len =  strlen(map) + 1 + strlen(p + 1);
          name0 = malloc(len + 1);
          sprintf(name0, "%s.%s", map, p + 1);
        }
      }

      const char *varname = opi_builder_assoc(bldr, name0 ? name0 : ast->var);
      if (!varname)
        return build_error();

      if (name0)
        free(name0);

      int var_idx = cod_strvec_rfind(&bldr->decls, varname);
      if (var_idx >= 0) {
        // # Found in local variables:
        offs = bldr->decls.size - var_idx;

      } else if (opi_builder_find_deep(bldr, varname)) {
        // # Add to captures:
        // insert at the beginning of declarations => won't change other offsets
        opi_builder_capture(bldr, varname);
        offs = bldr->decls.size;

      } else if ((const_val = opi_builder_find_const(bldr, varname))) {
        // # Found constant definition:
        return opi_ir_const(const_val);

      } else  {
        opi_error("logic error: failed to resolve variable\n");
        exit(EXIT_FAILURE);
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
      OpiIr *fn = opi_builder_build_ir(bldr, ast->apply.fn);
      OpiIr *args[ast->apply.nargs];
      for (size_t i = 0; i < ast->apply.nargs; ++i)
        args[i] = opi_builder_build_ir(bldr, ast->apply.args[i]);
      OpiIr *ret = opi_ir_apply(fn, args, ast->apply.nargs);
      ret->apply.eflag = ast->apply.eflag;
      if (ast->apply.loc)
        ret->apply.loc = opi_location_copy(ast->apply.loc);
      return ret;
    }

    case OPI_AST_FN:
    {
      // create separate builder for function body
      OpiBuilder fn_bldr;
      opi_builder_init_derived(&fn_bldr, bldr);

      // declare parameters as fn-local variables
      for (int i = ast->fn.nargs - 1; i >= 0; --i)
        opi_builder_push_decl(&fn_bldr, ast->fn.args[i]);

      // build body (with local builder)
      OpiIr *body = opi_builder_build_ir(&fn_bldr, ast->fn.body);

      // process captures
      size_t ncaps = fn_bldr.frame_offset;
      OpiIr *caps[ncaps];
      for (size_t i = 0; i < ncaps; ++i) {
        // build expression to get captured variable
        OpiAst *tmp_var = opi_ast_var(fn_bldr.decls.data[i]);
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
      OpiIr *vals[ast->let.n];

      // evaluate values
      for (size_t i = 0; i < ast->let.n; ++i)
        vals[i] = opi_builder_build_ir(bldr, ast->let.vals[i]);
      // declare variables
      for (size_t i = 0; i < ast->let.n; ++i)
        opi_builder_push_decl(bldr, ast->let.vars[i]);

      if (ast->let.body) {
        // evaluate body
        OpiIr *body = opi_builder_build_ir(bldr, ast->let.body);
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
      OpiIr *vals[ast->let.n];

      // declare functions
      for (size_t i = 0; i < ast->let.n; ++i)
        opi_builder_push_decl(bldr, ast->let.vars[i]);
      // evaluate lambdas
      for (size_t i = 0; i < ast->let.n; ++i)
        vals[i] = opi_builder_build_ir(bldr, ast->let.vals[i]);

      if (ast->let.body) {
        // evaluate body
        OpiIr *body = opi_builder_build_ir(bldr, ast->let.body);
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
      OpiScope scp;
      opi_builder_begin_scope(bldr, &scp);

      OpiIr *exprs[ast->block.n];
      for (size_t i = 0; i < ast->block.n; ++i)
        exprs[i] = opi_builder_build_ir(bldr, ast->block.exprs[i]);

      if (ast->block.drop)
        opi_builder_drop_scope(bldr, &scp);
      else if (ast->block.namespace)
        opi_builder_make_namespace(bldr, &scp, ast->block.namespace);

      OpiIr *ret = opi_ir_block(exprs, ast->block.n);
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
        return build_error();
      }

      // check if already loaded
      int found = FALSE;
      for (size_t i = 0; i < bldr->loaded->size; ++i) {
        if (strcmp(bldr->loaded->data[i], path) == 0) {
          // file already visited, test for cyclic dependence
          if (strcmp(bldr->load_state->data[i], "loading") == 0) {
            opi_error("cyclic dependence detected while loading \"%s\"\n", path);
            return build_error();
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

        OpiIr *ret;

        if (opi_is_dl(path)) {
          void *dl = opi_context_find_dl(bldr->ctx, path);
          if (!dl) {
            dl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
            if (!dl) {
              opi_error(dlerror());
              opi_assert(chdir(cwd) == 0);
              return build_error();
            }
            opi_context_add_dl(bldr->ctx, path, dl);
          }

          if (opi_builder_load_dl(bldr, dl) == OPI_ERR) {
            opi_assert(chdir(cwd) == 0);
            return build_error();
          }

          ret = opi_ir_const(opi_nil);

        } else {
          // mark file before building contents
          size_t id = bldr->loaded->size;
          cod_strvec_push(bldr->loaded, path);
          cod_strvec_push(bldr->load_state, "loading");

          // load file
          FILE *in = fopen(path, "r");
          opi_assert(in);
          OpiAst *subast = opi_parse(in);
          fclose(in);
          if (subast == NULL) {
            opi_assert(chdir(cwd) == 0);
            return build_error();
          }

          opi_assert(subast->tag == OPI_AST_BLOCK);
          opi_ast_block_set_drop(subast, FALSE);
          OpiIr *ir = opi_builder_build_ir(bldr, subast);
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
      // evaluate expr
      OpiIr *expr = opi_builder_build_ir(bldr, ast->match.expr);

      // build pattern
      size_t ndecls_start = bldr->decls.size;
      OpiIrPattern *pattern = build_pattern(bldr, ast->match.pattern);
      size_t ndecls = bldr->decls.size - ndecls_start;
      if (!pattern) {
        opi_ir_delete(expr);
        return build_error();
      }

      OpiIr *then = NULL, *els = NULL;
      if (ast->match.then) {
        // eval then-branch
        then = opi_builder_build_ir(bldr, ast->match.then);
        // hide variables
        while (ndecls--)
          opi_builder_pop_decl(bldr);
        // eval else-branch
        els = opi_builder_build_ir(bldr, ast->match.els);
      }

      return opi_ir_match(pattern, expr, then, els);
    }

    case OPI_AST_STRUCT:
    {
      // create type
      opi_type_t type = opi_type_new(ast->strct.typename);
      size_t offset = offsetof(struct opi_struct, data);
      opi_type_set_fields(type, offset, ast->strct.fields, ast->strct.nfields);
      opi_type_set_delete_cell(type, opi_struct_delete);
      opi_type_set_write(type, write_struct);

      // create constructor
      opi_t ctor = opi_fn(ast->strct.typename, make_struct, ast->strct.nfields);
      opi_fn_set_data(ctor, type, NULL);

      // declare type
      if (opi_builder_add_type(bldr, ast->strct.typename, type) == OPI_ERR) {
        opi_drop(ctor);
        opi_type_delete(type);
        return build_error();
      }

      // declare constructor
      opi_builder_push_decl(bldr, ast->strct.typename);
      OpiIr *ctor_ir = opi_ir_const(ctor);
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

static opi_t
_export(void)
{
  OpiBuilder *bldr = opi_fn_get_data(opi_current_fn);
  opi_t l = opi_pop();
  for (opi_t it = l; it != opi_nil; it = opi_cdr(it)) {
    opi_t elt = opi_car(it);
    opi_t nam = opi_car(elt);
    opi_t val = opi_cdr(elt);
    opi_builder_def_const(bldr, opi_string_get_value(nam), val);
  }
  opi_drop(l);
  return opi_nil;
}

OpiBytecode*
opi_build(OpiBuilder *bldr, OpiAst *ast, int mode)
{
  opi_error = 0;
  OpiIr *ir = opi_builder_build_ir(bldr, ast);
  if (opi_error) {
    opi_ir_delete(ir);
    return NULL;
  }
  if (bldr->frame_offset != 0) {
    opi_error("logic error: captures at top-level\n");
    for (int i = 0; i < bldr->frame_offset; ++i)
      opi_trace("  %s\n", bldr->decls.data[i]);
    exit(EXIT_FAILURE);
  }

  OpiBytecode *bc = opi_bytecode();

  switch (mode) {
    case OPI_BUILD_DEFAULT:
      break;

    case OPI_BUILD_EXPORT:
    {
      size_t ndecls = bldr->decls.size;
      if (ndecls == 0)
        break;

      struct cod_strvec *decls = &bldr->decls;
      OpiIr *list = opi_ir_const(opi_nil);
      for (size_t i = 0; i < ndecls; ++i) {
        OpiAst *var_ast = opi_ast_var(decls->data[i]);
        OpiIr *var_ir = opi_builder_build_ir(bldr, var_ast);
        opi_ast_delete(var_ast);
        OpiIr *nam_ir = opi_ir_const(opi_string_new(decls->data[i]));
        OpiIr *pair = opi_ir_binop(OPI_OPC_CONS, nam_ir, var_ir);
        list = opi_ir_binop(OPI_OPC_CONS, pair, list);
      }
      opi_t export_fn = opi_fn("__export", _export, 1);
      opi_fn_set_data(export_fn, bldr, NULL);
      OpiIr *export_ir = opi_ir_const(export_fn);
      OpiIr *call = opi_ir_apply(export_ir, &list, 1);

      OpiIr *block[] = { ir, call };
      ir = opi_ir_block(block, 2);

      while (decls->size)
        cod_strvec_pop(decls);
      break;
    }

    default:
      opi_assert(!"undefined build mode");
  }

  opi_ir_emit(ir, bc);
  opi_ir_delete(ir);
  opi_bytecode_finalize(bc);

  return bc;
}

void
opi_ir_delete(OpiIr *node)
{
  switch (node->tag) {
    case OPI_IR_CONST:
      opi_unref(node->cnst);
      break;

    case OPI_IR_VAR:
      break;

    case OPI_IR_APPLY:
      opi_ir_delete(node->apply.fn);
      if (node->apply.loc)
        opi_location_delete(node->apply.loc);
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
      opi_ir_pattern_delete(node->match.pattern);
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

OpiIr*
opi_ir_const(opi_t x)
{
  OpiIr *node = malloc(sizeof(OpiIr));
  node->tag = OPI_AST_CONST;
  opi_inc_rc(node->cnst = x);
  return node;
}

OpiIr*
opi_ir_var(size_t offs)
{
  OpiIr *node = malloc(sizeof(OpiIr));
  node->tag = OPI_IR_VAR;
  node->var = offs;
  return node;
}

OpiIr*
opi_ir_apply(OpiIr *fn, OpiIr **args, size_t nargs)
{
  OpiIr *node = malloc(sizeof(OpiIr));
  node->tag = OPI_IR_APPLY;
  node->apply.fn = fn;
  node->apply.args = malloc(sizeof(OpiIr*) * nargs);
  memcpy(node->apply.args, args, sizeof(OpiIr*) * nargs);
  node->apply.nargs = nargs;
  node->apply.eflag = TRUE;
  node->apply.loc = NULL;
  return node;
}

OpiIr*
opi_ir_fn(OpiIr **caps, size_t ncaps, size_t nargs, OpiIr *body)
{
  OpiIr *node = malloc(sizeof(OpiIr));
  node->tag = OPI_IR_FN;
  node->fn.caps = malloc(sizeof(OpiIr*) * ncaps);
  memcpy(node->fn.caps, caps, sizeof(OpiIr*) * ncaps);
  node->fn.ncaps = ncaps;
  node->fn.nargs = nargs;
  node->fn.body = body;
  return node;
}

OpiIr*
opi_ir_let(OpiIr **vals, size_t n, OpiIr *body)
{
  OpiIr *node = malloc(sizeof(OpiIr));
  node->tag = OPI_IR_LET;
  node->let.vals = malloc(sizeof(OpiIr*) * n);
  memcpy(node->let.vals, vals, sizeof(OpiIr*) * n);
  node->let.n = n;
  node->let.body = body;
  return node;
}

OpiIr*
opi_ir_fix(OpiIr **vals, size_t n, OpiIr *body)
{
  OpiIr *ir = opi_ir_let(vals, n, body);
  ir->tag = OPI_IR_FIX;
  return ir;
}

OpiIr*
opi_ir_if(OpiIr *test, OpiIr *then, OpiIr *els)
{
  OpiIr *node = malloc(sizeof(OpiIr));
  node->tag = OPI_IR_IF;
  node->iff.test = test;
  node->iff.then = then;
  node->iff.els  = els;
  return node;
}


OpiIr*
opi_ir_block(OpiIr **exprs, size_t n)
{
  OpiIr *node = malloc(sizeof(OpiIr));
  node->tag = OPI_IR_BLOCK;
  node->block.exprs = malloc(sizeof(OpiIr*) *  n);
  memcpy(node->block.exprs, exprs, sizeof(OpiIr*) * n);
  node->block.n = n;
  node->block.drop = TRUE;
  return node;
}

OpiIrPattern*
opi_ir_pattern_new_ident(void)
{
  OpiIrPattern *pattern = malloc(sizeof(OpiIrPattern));
  pattern->tag = OPI_PATTERN_IDENT;
  return pattern;
}

OpiIrPattern*
opi_ir_pattern_new_unpack(opi_type_t type, OpiIrPattern **subs, size_t *offs,
    size_t n)
{
  OpiIrPattern *pattern = malloc(sizeof(OpiIrPattern));
  pattern->tag = OPI_PATTERN_UNPACK;
  pattern->unpack.type = type;
  pattern->unpack.subs = malloc(sizeof(OpiIrPattern*) * n);
  pattern->unpack.offs = malloc(sizeof(size_t) * n);
  for (size_t i = 0; i < n; ++i) {
    pattern->unpack.subs[i] = subs[i];
    pattern->unpack.offs[i] = offs[i];
  }
  pattern->unpack.n = n;
  return pattern;
}

void
opi_ir_pattern_delete(OpiIrPattern *pattern)
{
  if (pattern->tag == OPI_PATTERN_UNPACK) {
    for (size_t i = 0; i < pattern->unpack.n; ++i)
      opi_ir_pattern_delete(pattern->unpack.subs[i]);
    free(pattern->unpack.subs);
    free(pattern->unpack.offs);
  }
  free(pattern);
}

OpiIr*
opi_ir_match(OpiIrPattern *pattern, OpiIr *expr, OpiIr *then, OpiIr *els)
{
  OpiIr *node = malloc(sizeof(OpiIr));
  node->tag = OPI_IR_MATCH;
  node->match.pattern = pattern;
  node->match.expr = expr;
  node->match.then = then;
  node->match.els = els;
  return node;
}

OpiIr*
opi_ir_return(OpiIr *val)
{
  OpiIr *node = malloc(sizeof(OpiIr));
  node->tag = OPI_IR_RETURN;
  node->ret = val;
  return node;
}

OpiIr*
opi_ir_binop(int opc, OpiIr *lhs, OpiIr *rhs)
{
  OpiIr *node = malloc(sizeof(OpiIr));
  node->tag = OPI_IR_BINOP;
  node->binop.opc = opc;
  node->binop.lhs = lhs;
  node->binop.rhs = rhs;
  return node;
}

