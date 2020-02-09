#include "opium/opium.h"
#include "opium/lambda.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dlfcn.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>

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

  cod_vec_init(bldr->decls);
  bldr->frame_offset = 0;

  opi_alist_init(bldr->alist = malloc(sizeof(OpiAlist)));

  cod_strvec_init(bldr->srcdirs = malloc(sizeof(struct cod_strvec)));

  cod_strvec_init(bldr->loaded = malloc(sizeof(struct cod_strvec)));
  cod_strvec_init(bldr->load_state = malloc(sizeof(struct cod_strvec)));

  cod_strvec_init(bldr->type_names = malloc(sizeof(struct cod_strvec)));
  cod_ptrvec_init(bldr->types = malloc(sizeof(struct cod_ptrvec)));

  cod_strvec_init(bldr->trait_names = malloc(sizeof(struct cod_strvec)));
  cod_ptrvec_init(bldr->traits = malloc(sizeof(struct cod_ptrvec)));

  opi_builder_def_type(bldr, "Undefined", opi_undefined_type); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "Num"      , opi_num_type      ); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "Sym"      , opi_symbol_type   ); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "Nil"      , opi_nil_type      ); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "Str"      , opi_str_type      ); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "Bool"     , opi_boolean_type  ); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "Cons"     , opi_pair_type     ); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "Fn"       , opi_fn_type       ); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "Lazy"     , opi_lazy_type     ); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "File"     , opi_file_type     ); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "Seq"      , opi_seq_type      ); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "Array"    , opi_array_type    ); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "Table"    , opi_table_type    ); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "Buffer"   , opi_buffer_type   ); cod_vec_pop(ctx->types);
  opi_builder_def_type(bldr, "TypeObject",opi_type_type     ); cod_vec_pop(ctx->types);

  opi_builder_def_trait(bldr, "Add", opi_trait_add); cod_vec_pop(ctx->traits);
  opi_builder_def_trait(bldr, "Sub", opi_trait_sub); cod_vec_pop(ctx->traits);
  opi_builder_def_trait(bldr, "Mul", opi_trait_mul); cod_vec_pop(ctx->traits);
  opi_builder_def_trait(bldr, "Div", opi_trait_div); cod_vec_pop(ctx->traits);
  opi_builder_def_trait(bldr, "Hash", opi_trait_hash); cod_vec_pop(ctx->traits);
}

void
opi_builder_init_derived(OpiBuilder *bldr, OpiBuilder *parent)
{
  bldr->parent = parent;
  bldr->ctx = parent->ctx;

  cod_vec_init(bldr->decls);
  bldr->frame_offset = 0;

  bldr->alist = parent->alist;

  bldr->srcdirs = parent->srcdirs;

  bldr->loaded = parent->loaded;
  bldr->load_state = parent->load_state;

  bldr->type_names = parent->type_names;
  bldr->types = parent->types;

  bldr->trait_names = parent->trait_names;
  bldr->traits = parent->traits;
}

void
opi_builder_destroy(OpiBuilder *bldr)
{
  cod_vec_iter(bldr->decls, i, d,
      free(d.name);
      if (d.c_val)
        opi_unref(d.c_val);
  );
  cod_vec_destroy(bldr->decls);

  if (opi_builder_is_derived(bldr))
    return;

  opi_alist_destroy(bldr->alist);
  free(bldr->alist);

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

  cod_strvec_destroy(bldr->trait_names);
  free(bldr->trait_names);
  cod_ptrvec_destroy(bldr->traits, NULL);
  free(bldr->traits);
}

void
opi_builder_add_source_directory(OpiBuilder *bldr, const char *path)
{
  char fullpath[PATH_MAX];
  opi_assert(realpath(path, fullpath));
  cod_strvec_push(bldr->srcdirs, fullpath);
}

static int
is_regular_file(const char *path)
{
  struct stat path_stat;
  stat(path, &path_stat);
  return S_ISREG(path_stat.st_mode);
}

char*
opi_builder_find_path(OpiBuilder *bldr, const char *path, char *fullpath)
{
  if (realpath(path, fullpath) && is_regular_file(fullpath))
    return fullpath;

  char buf[PATH_MAX];
  for (size_t i = 0; i < bldr->srcdirs->size; ++i) {
    sprintf(buf, "%s/%s", bldr->srcdirs->data[i], path);
    if (realpath(buf, fullpath) && is_regular_file(fullpath)) {
      return fullpath;
    }
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
  if (opi_builder_find_trait(bldr, name)) {
    opi_error("type '%s' shadows a trait with the same name\n", name);
    opi_error = 1;
    return OPI_ERR;
  }
  cod_strvec_push(bldr->type_names, name);
  cod_ptrvec_push(bldr->types, type, NULL);
  opi_context_add_type(bldr->ctx, type);
  return OPI_OK;
}

int
opi_builder_add_trait(OpiBuilder *bldr, const char *name, OpiTrait *trait)
{
  if (opi_builder_find_trait(bldr, name)) {
    opi_error("trait named '%s' already present\n", name);
    opi_error = 1;
    return OPI_ERR;
  }
  if (opi_builder_find_type(bldr, name)) {
    opi_error("trait '%s' shadows a type with the same name\n", name);
    opi_error = 1;
    return OPI_ERR;
  }
  cod_strvec_push(bldr->trait_names, name);
  cod_ptrvec_push(bldr->traits, trait, NULL);
  opi_context_add_trait(bldr->ctx, trait);
  return OPI_OK;
}

opi_type_t
opi_builder_find_type(OpiBuilder *bldr, const char *name)
{
  long long int idx = cod_strvec_find(bldr->type_names, name);
  return idx < 0 ? NULL : bldr->types->data[idx];
}

OpiTrait*
opi_builder_find_trait(OpiBuilder *bldr, const char *name)
{
  long long int idx = cod_strvec_find(bldr->trait_names, name);
  return idx < 0 ? NULL : bldr->traits->data[idx];
}

int
opi_builder_find_offs(OpiBuilder *bldr, int d_idx)
{
  opi_assert(bldr->decls.data[d_idx].c_val == NULL);

  int offs = 1;
  cod_vec_riter(bldr->decls, i, d,
    if (i == d_idx)
      break;
    if (d.c_val == NULL)
      offs += 1;
  );
  return offs;
}

void
opi_builder_def_const(OpiBuilder *bldr, const char *name, opi_t val)
{
  opi_inc_rc(val);
  cod_vec_emplace(bldr->decls, { strdup(name), val });
  opi_alist_push(bldr->alist, name, NULL);
}

int
opi_builder_def_type(OpiBuilder *bldr, const char *name, opi_type_t type)
{
  if (opi_builder_add_type(bldr, name, type) == OPI_ERR)
    return OPI_ERR;
  opi_alist_push(bldr->alist, name, NULL);
  return OPI_OK;
}

int
opi_builder_def_trait(OpiBuilder *bldr, const char *name, OpiTrait *trait)
{
  if (opi_builder_add_trait(bldr, name, trait) == OPI_ERR)
    return OPI_ERR;
  opi_alist_push(bldr->alist, name, NULL);
  return OPI_OK;
}

int
opi_builder_load_dl(OpiBuilder *bldr, void *dl)
{
  int (*build)(OpiBuilder*) = dlsym(dl, "opium_library");
  if (!build) {
    opi_error("failed to resolve library entry point (%s)\n", dlerror());
    opi_error = 1;
    return OPI_ERR;
  }
  opi_assert(build(bldr) == 0);
  return OPI_OK;
}

void
opi_builder_push_decl(OpiBuilder *bldr, const char *var)
{
  cod_vec_emplace(bldr->decls, { strdup(var), NULL });
  opi_alist_push(bldr->alist, var, NULL);
}

void
opi_builder_pop_decl(OpiBuilder *bldr)
{
  const char *var = bldr->alist->keys.data[bldr->alist->keys.size-1];
  const char *decl = bldr->decls.data[bldr->decls.len - 1].name;
  opi_assert(strcmp(var, decl) == 0);
  opi_decl_destroy(cod_vec_pop(bldr->decls));
  opi_alist_pop(bldr->alist, 1);
}

void
opi_builder_capture(OpiBuilder *bldr, const char *var)
{
  cod_vec_insert(bldr->decls, ((OpiDecl) { strdup(var), NULL }), 0);
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
  scp->nvars1 = bldr->decls.len - bldr->frame_offset;
  scp->ntypes1 = bldr->types->size;
  scp->ntraits1 = bldr->traits->size;
  scp->vasize1 = opi_alist_get_size(bldr->alist);
}

void
opi_builder_drop_scope(OpiBuilder *bldr, OpiScope *scp)
{
  size_t nvars1 = scp->nvars1;
  size_t nvars2 = bldr->decls.len - bldr->frame_offset;
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
    opi_decl_destroy(cod_vec_pop(bldr->decls));
  // pop alist
  opi_alist_pop(bldr->alist, vasize);
  // pop types
  while (ntypes--) {
    cod_strvec_pop(bldr->type_names);
    cod_ptrvec_pop(bldr->types, NULL);
  }
  // pop traits
  while (ntraits--) {
    cod_strvec_pop(bldr->trait_names);
    cod_ptrvec_pop(bldr->traits, NULL);
  }
}

void
opi_builder_make_namespace(OpiBuilder *bldr, OpiScope *scp, const char *prefix)
{
  size_t nvars1 = scp->nvars1;
  size_t nvars2 = bldr->decls.len - bldr->frame_offset;
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
  for (size_t i = bldr->decls.len - nvars; i < bldr->decls.len; ++i) {
    // add namespace prefix
    size_t len = strlen(bldr->decls.data[i].name) + nslen;
    char *newname = malloc(len + 1);
    sprintf(newname, "%s%s", prefix, bldr->decls.data[i].name);
    // change declaration
    free(bldr->decls.data[i].name);
    bldr->decls.data[i].name = newname;

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
    // push new type name into a-list
    opi_alist_push(bldr->alist, newname, NULL);
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
    // push new trait name into a-list
    opi_alist_push(bldr->alist, newname, NULL);
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

static int
opi_struct_equal(opi_type_t type, opi_t x, opi_t y)
{
  size_t nfields = opi_type_get_nfields(type);
  opi_t *fx = ((struct opi_struct*)x)->data;
  opi_t *fy = ((struct opi_struct*)y)->data;
  for (size_t i = 0; i < nfields; ++i) {
    if (!opi_equal(fx[i], fy[i]))
      return FALSE;
  }
  return TRUE;
}

static opi_t
make_struct(void)
{
  opi_type_t type = opi_current_fn->data;
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

typedef struct CopyCtorData_s {
  opi_type_t type;
  int offs[];
} CopyCtorData;

static void
copy_ctor_delete(OpiFn *fn)
{
  free(fn->data);
  opi_fn_delete(fn);
}

// TODO: reuse source if RC = 0.
static opi_t
copy_ctor(void)
{
  CopyCtorData *restrict data = opi_current_fn->data;
  if (opi_get(1)->type != data->type) {
    OPI_BEGIN_FN()
    OPI_THROW("type-error");
  }

  int nflds = opi_type_get_nfields(data->type);
  struct opi_struct *src = (struct opi_struct*)opi_pop();

  if (OPI(src)->rc > 0) {
    //
    // Construct new struct.
    //
    struct opi_struct *s = malloc(sizeof(struct opi_struct) + sizeof(opi_t) * nflds);
    int n_new = opi_nargs - 1;
    for (int i = 0, i_offs = 0; i < nflds; ++i) {
      if (i_offs < n_new && data->offs[i_offs] == i) {
        opi_inc_rc(s->data[i] = opi_pop());
        i_offs++;
      } else {
        opi_inc_rc(s->data[i] = src->data[i]);
      }
    }
    opi_init_cell(s, data->type);
    return OPI(s);

  } else {
    //
    // Destructive update (reuse old struct).
    //
    int n_new = opi_nargs - 1;
    for (int i = 0, i_offs = 0; i < nflds; ++i) {
      // Update supplied fields.
      if (i_offs < n_new && data->offs[i_offs] == i) {
        opi_t x = opi_pop();
        opi_inc_rc(x);
        opi_unref(src->data[i]);
        src->data[i] = x;
        i_offs++;
      }
    }
    return OPI(src);
  }
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

typedef struct ImplData_s {
  OpiTrait *trait;
  OpiType *type;
  char **f_nams;
  int nnams;
} ImplData;

static void
impl_data_delete(OpiFn *fn)
{
  ImplData *data = fn->data;
  for (int i = 0; i < data->nnams; ++i)
    free(data->f_nams[i]);
  free(data->f_nams);
  free(data);
  opi_fn_delete(fn);
}

static opi_t
impl_default(void)
{
  ImplData *data = opi_current_fn->data;
  opi_assert((int)opi_nargs == data->nnams);
  opi_t fs[data->nnams];
  for (int i = 0; i < data->nnams; ++i)
    fs[i] = opi_pop();
  opi_trait_set_default(data->trait, data->f_nams, fs, data->nnams);
  return opi_nil;
}

static opi_t
impl_for_type(void)
{
  ImplData *data = opi_current_fn->data;
  opi_assert((int)opi_nargs == data->nnams);
  opi_t fs[data->nnams];
  for (int i = 0; i < data->nnams; ++i)
    fs[i] = opi_pop();
  int err = opi_trait_impl(data->trait, data->type, data->f_nams, fs,
      data->nnams, FALSE);
  if (err != OPI_OK) {
    opi_error("failed to implement trait for type '%s'\n",
        opi_type_get_name(data->type));
    abort();
  }
  return opi_nil;
}

static opi_t
test_trait(void)
{
  OpiTrait *trait = opi_current_fn->data;
  opi_t x = opi_pop();
  opi_type_t type = x->type;
  opi_drop(x);
  return opi_trait_get_impl(trait, type, 0) ? opi_true : opi_false;
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
        if (pattern->unpack.fields[i][0] == '#') // index is set explicitly
          field_idx = atoi(pattern->unpack.fields[i] + 1);
        else // find index by field-name
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

      if (pattern->unpack.alias)
        opi_builder_push_decl(bldr, pattern->unpack.alias);

      // create IR-pattern
      return opi_ir_pattern_new_unpack(type, subs, offsets, pattern->unpack.n,
          pattern->unpack.alias);
    }
  }
  abort();
}

OpiDecl*
opi_builder_find_deep(OpiBuilder *bldr, const char *var)
{
  OpiDecl *decl = NULL;
  cod_vec_riter(bldr->decls, i, d,
    if (strcmp(d.name, var) == 0) {
      decl = bldr->decls.data + i;
      break;
    }
  );
  if (decl)
    return decl;
  else if (bldr->parent)
    return opi_builder_find_deep(bldr->parent, var);
  else
    return NULL;
}

static int
resolve_load_path(OpiBuilder *bldr, const char *load, char *path)
{
  if (!opi_builder_find_path(bldr, load, path)) {
    opi_error("no such file: \"%s\"\n", load);
    opi_error("source directories:\n");
    for (size_t i = 0; i < bldr->srcdirs->size; ++i)
      opi_error("  %2.zu. %s\n", i + 1, bldr->srcdirs->data[i]);
    return -1;
  }
  return 0;
}

static int
verify_load(OpiBuilder *bldr, const char *path)
{
  for (size_t i = 0; i < bldr->loaded->size; ++i) {
    if (strcmp(bldr->loaded->data[i], path) == 0) {
      // file already visited, test for cyclic dependence
      if (strcmp(bldr->load_state->data[i], "loading") == 0) {
        opi_error("cyclic dependence detected while loading \"%s\"\n", path);
        return -1;
      }
      return TRUE;
    }
  }
  return FALSE;
}

static int
load_dl(OpiBuilder *bldr, const char *path)
{
  void *dl = opi_context_find_dl(bldr->ctx, path);
  int is_old = dl != NULL;

  if (!is_old) {
    dl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!dl) {
      opi_error("%s\n", dlerror());
      return -1;
    }
  }

  if (opi_builder_load_dl(bldr, dl) == OPI_ERR)
    return -1;

  if (!is_old)
    opi_context_add_dl(bldr->ctx, path, dl);

  return 0;
}

static OpiIr*
load_script(OpiBuilder *bldr, const char *path)
{
  // mark file before building contents
  size_t id = bldr->loaded->size;
  cod_strvec_push(bldr->loaded, path);
  cod_strvec_push(bldr->load_state, "loading");

  // load file
  FILE *in = fopen(path, "r");
  opi_assert(in);
  OpiAst *subast = opi_parse(in);
  fclose(in);
  if (subast == NULL)
    return NULL;

  opi_assert(subast->tag == OPI_AST_BLOCK);
  opi_ast_block_set_drop(subast, FALSE);
  OpiIr *ir = opi_builder_build_ir(bldr, subast);
  opi_assert(ir->tag == OPI_IR_BLOCK);
  opi_assert(ir->block.drop == FALSE);
  opi_ast_delete(subast);

  // mark file as loaded
  free(bldr->load_state->data[id]);
  bldr->load_state->data[id] = strdup("ready");

  return ir;
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
      if (name0) free(name0);
      if (!varname)
        return build_error();

      int var_idx = -1;
      cod_vec_riter(bldr->decls, i, d,
        if (strcmp(varname, d.name) == 0) {
          var_idx = i;
          break;
        }
      );
      OpiDecl *d;
      if (var_idx >= 0) {
        // # Found in local variables:
        OpiDecl *d = bldr->decls.data + var_idx;
        if (d->c_val)
          return opi_ir_const(d->c_val);
        else
          offs = opi_builder_find_offs(bldr, var_idx);

      } else if ((d = opi_builder_find_deep(bldr, varname))) {
        if (d->c_val) {
          return opi_ir_const(d->c_val);
        } else {
          // # Add to captures:
          // insert at the beginning of declarations => won't change other offsets
          opi_builder_capture(bldr, varname);
          offs = opi_builder_find_offs(bldr, 0);
        }

      } else  {
        opi_error("logic error: failed to resolve variable\n");
        exit(EXIT_FAILURE);
      }

      return opi_ir_var(offs);
    }

    case OPI_AST_USE:
    {
      if (strcmp(ast->use.nw, "*") == 0) {
        char *prefix = ast->use.old;
        size_t prefixlen = strlen(ast->use.old);
        for (size_t i = 0, n = opi_alist_get_size(bldr->alist); i < n; ++i) {
          char *vname = bldr->alist->vals.data[i];
          if (strncmp(prefix, vname, prefixlen) == 0)
            opi_alist_push(bldr->alist, vname + prefixlen, vname);
        }
      } else {
        opi_alist_push(bldr->alist, ast->use.nw, ast->use.old);
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
        OpiAst *tmp_var = opi_ast_var(fn_bldr.decls.data[i].name);
        caps[i] = opi_builder_build_ir(bldr, tmp_var);
        opi_ast_delete(tmp_var);
      }

      for (size_t i = 0; i < ast->fn.nargs; ++i)
        opi_builder_pop_decl(&fn_bldr);

      opi_builder_destroy(&fn_bldr);

      if (ncaps > 0) {
        /* Runtime lambda constructor. */
        return opi_ir_fn(caps, ncaps, ast->fn.nargs, body);

      } else /* ncaps == 0 */ {
        /* Instant lambda constructor (i.e. create it NOW). */
        // emit bytecode
        OpiBytecode *bc = opi_emit_free_fn_body(body, ast->fn.nargs);

        // create lambda
        OpiLambda *lam = opi_lambda_allocate(0);
        lam->bc = bc;
        lam->ir = body;
        opi_ir_ref(body);
        lam->ncaps = 0;
        lam->scp = NULL;
        opi_t fn = opi_fn_new(opi_lambda_fn, ast->fn.nargs);
        void delete(OpiFn *fn) {
          OpiLambda* lam = fn->data;
          opi_bytecode_delete(lam->bc);
          opi_lambda_delete(fn);
        }
        opi_fn_set_data(fn, lam, delete);
        opi_context_drain_bytecode(bldr->ctx, bc);
        return opi_ir_const(fn);
      }
    }

    case OPI_AST_LET:
    {
      OpiIr *vals[ast->let.n];

      // evaluate values
      for (size_t i = 0; i < ast->let.n; ++i)
        vals[i] = opi_builder_build_ir(bldr, ast->let.vals[i]);
      // declare variables
      OpiIr *dynvals[ast->let.n];
      int ndynvals = 0;
      for (size_t i = 0; i < ast->let.n; ++i) {
        if (vals[i]->tag == OPI_IR_CONST) {
          opi_builder_def_const(bldr, ast->let.vars[i], vals[i]->cnst);
          opi_ir_drop(vals[i]);
        } else {
          opi_builder_push_decl(bldr, ast->let.vars[i]);
          dynvals[ndynvals++] = vals[i];
        }
      }

      return opi_ir_let(dynvals, ndynvals);
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
      return opi_ir_fix(vals, ast->let.n);
    }

    case OPI_AST_IF:
      // TODO: optimize for OPI_AST_ISOF
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
      else if (ast->block.ns)
        opi_builder_make_namespace(bldr, &scp, ast->block.ns);

      OpiIr *ret = opi_ir_block(exprs, ast->block.n);
      opi_ir_block_set_drop(ret, ast->block.drop);
      return ret;
    }

    case OPI_AST_LOAD:
    {
      // find absolute path
      char path[PATH_MAX];
      if (resolve_load_path(bldr, ast->load, path) < 0)
        return build_error();

      // check if already loaded
      int found = verify_load(bldr, path);
      if (found < 0)
        return build_error();

      if (!found) {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);

        char tmp[PATH_MAX];
        strcpy(tmp, path);
        opi_assert(chdir(dirname(tmp)) == 0);

        if (opi_is_dl(path)) {
          // Load dynamic library.
          int err = load_dl(bldr, path);
          opi_assert(chdir(cwd) == 0);
          if (err < 0)
            return build_error();
          else
            return opi_ir_const(opi_nil);

        } else {
          // Load script.
          OpiIr *ret = load_script(bldr, path);
          opi_assert(chdir(cwd) == 0);
          if (ret == NULL)
            return build_error();
          else
            return ret;
        }

      } else {
        // Already loaded by current builder.
        return opi_ir_const(opi_nil);
      }
    }

    case OPI_AST_MATCH:
    {
      // evaluate expr
      OpiIr *expr = opi_builder_build_ir(bldr, ast->match.expr);

      // build pattern
      size_t ndecls_start = bldr->decls.len;
      OpiIrPattern *pattern = build_pattern(bldr, ast->match.pattern);
      size_t ndecls = bldr->decls.len - ndecls_start;
      if (!pattern) {
        opi_ir_drop(expr);
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
      opi_type_t type = opi_type_new(ast->strct.name);
      size_t offset = offsetof(struct opi_struct, data);
      opi_type_set_fields(type, offset, ast->strct.fields, ast->strct.nfields);
      opi_type_set_delete_cell(type, opi_struct_delete);
      opi_type_set_equal(type, opi_struct_equal);
      opi_type_set_write(type, write_struct);
      opi_type_set_is_struct(type, TRUE);

      // create constructor
      opi_t ctor = opi_fn_new(make_struct, ast->strct.nfields);
      opi_fn_set_data(ctor, type, NULL);

      // declare type
      if (opi_builder_add_type(bldr, ast->strct.name, type) == OPI_ERR) {
        opi_drop(ctor);
        opi_type_delete(type);
        return build_error();
      }

      /*opi_builder_push_decl(bldr, ast->strct.name);*/
      /*OpiIr *ctor_ir = opi_ir_const(ctor);*/
      /*return opi_ir_let(&ctor_ir, 1, NULL);*/

      // declare constructor and type-object
      opi_builder_def_const(bldr, ast->strct.name, ctor);
      char buf[strlen(ast->strct.name) + 1 + strlen("type") + 1];
      sprintf(buf, "%s.type", ast->strct.name);
      opi_builder_def_const(bldr, buf, opi_type_get_type_object(type));
      return opi_ir_const(opi_nil);
    }

    case OPI_AST_CTOR:
    {
      OpiType *type = opi_builder_find_type(bldr, ast->ctor.name);
      if (type == NULL) {
        opi_error("no such type, '%s'\n", ast->ctor.name);
        return build_error();
      }

      // validate fields
      int nflds = opi_type_get_nfields(type);
      char* const* fields = opi_type_get_fields(type);
      for (int i = 0; i < ast->ctor.nflds; ++i) {
        if (opi_find_string(ast->ctor.fldnams[i], fields, nflds) < 0) {
          opi_error("no field '%s' in struct %s\n",
              ast->ctor.fldnams[i], ast->ctor.name);
          opi_error = 1;
          return build_error();
        }
      }

      if (ast->ctor.src) {
        //
        // Copy constructor.
        //
        if (!opi_type_is_struct(type)) {
          opi_error("can't use copy-construct for non-struct types\n");
          return build_error();
        }

        // Resolve fields and sort arguments.
        typedef struct Field_s { OpiAst *fld; int offs; } Field;
        Field fldarg[ast->ctor.nflds];
        for (int i = 0; i < ast->ctor.nflds; ++i) {
          int idx = opi_find_string(ast->ctor.fldnams[i], fields, nflds);
          fldarg[i].fld = ast->ctor.flds[i];
          fldarg[i].offs = idx;
          opi_assert(idx >= 0);
        }
        int compare(const Field* a, const Field* b) {
          if (a->offs == b->offs) return 0;
          else if (a->offs < b->offs) return -1;
          else return 1;
        }
        qsort(fldarg, ast->ctor.nflds, sizeof(Field), (void*)compare);

        CopyCtorData *data = malloc(sizeof(CopyCtorData) + sizeof(int) * ast->ctor.nflds);
        data->type = type;
        for (int i = 0; i < ast->ctor.nflds; ++i)
          data->offs[i] = fldarg[i].offs;

        OpiIr *args[ast->ctor.nflds + 1];
        args[0] = opi_builder_build_ir(bldr, ast->ctor.src);
        for (int i = 0; i < ast->ctor.nflds; ++i)
          args[i + 1] = opi_builder_build_ir(bldr, fldarg[i].fld);
        opi_t fn = opi_fn_new(copy_ctor, ast->ctor.nflds + 1);
        opi_fn_set_data(fn, data, copy_ctor_delete);
        OpiIr *fn_ir = opi_ir_const(fn);
        return opi_ir_apply(fn_ir, args, ast->ctor.nflds + 1);

      } else {
        // test if number of fields in constructor matches number of fields
        // of the struct
        if (nflds != ast->ctor.nflds) {
          opi_error("not enought fields to construct struct %s\n",
              ast->ctor.name);
          opi_error = 1;
          return build_error();
        }

        // resolve fields offsets
        int idxs[nflds];
        for (int i = 0; i < nflds; ++i)
          idxs[i] = opi_find_string(fields[i], ast->ctor.fldnams, nflds);

        // apply default constructor
        OpiIr *args[nflds];
        for (int i = 0; i < nflds; ++i)
          args[i] = opi_builder_build_ir(bldr, ast->ctor.flds[idxs[i]]);
        OpiAst *tmp = opi_ast_var(ast->ctor.name);
        OpiIr *ctor = opi_builder_build_ir(bldr, tmp);
        opi_ast_delete(tmp);
        return opi_ir_apply(ctor, args, nflds);
      }
    }

    case OPI_AST_TRAIT:
    {
      // create trait
      OpiTrait *trait = opi_trait_new(ast->trait.f_nams, ast->trait.nfs);
      opi_builder_def_trait(bldr, ast->trait.name, trait);

      // declare generics
      int trait_len = strlen(ast->trait.name);
      for (int i = 0; i < ast->trait.nfs; ++i) {
        opi_t g = opi_trait_get_generic(trait, i);
        char buf[trait_len + strlen(ast->trait.f_nams[i]) + 1];
        sprintf(buf, "%s.%s", ast->trait.name, ast->trait.f_nams[i]);
        opi_builder_def_const(bldr, buf, g);
      }

      OpiIr *build = NULL;
      int ndecls = 0;
      if (ast->trait.build) {
        int n0 = bldr->decls.len;
        build = opi_builder_build_ir(bldr, ast->trait.build);
        ndecls = bldr->decls.len - n0;
      } else {
        build = opi_ir_const(opi_nil);
      }

      // default methods
      OpiIr *methods[ast->trait.nfs];
      char **method_nams = malloc(sizeof(char*) *ast->trait.nfs);
      int method_cnt = 0;
      for (int i = 0; i < ast->trait.nfs; ++i) {
        if (ast->trait.fs[i] == NULL)
          continue;
        methods[method_cnt] = opi_builder_build_ir(bldr, ast->trait.fs[i]);
        method_nams[method_cnt] = strdup(ast->trait.f_nams[i]);
        method_cnt++;
      }
      while (ndecls--)
        opi_builder_pop_decl(bldr);

      // create implementer for defaults
      ImplData *data = malloc(sizeof(ImplData));
      data->trait = trait;
      data->f_nams = method_nams;
      data->nnams = method_cnt;
      opi_t impl_fn = opi_fn_new(impl_default, -1);
      opi_fn_set_data(impl_fn, data, impl_data_delete);

      // apply implementer
      OpiIr *impl = opi_ir_apply(opi_ir_const(impl_fn), methods, method_cnt);
      OpiIr *body[] = { build, impl };
      return opi_ir_block(body, 2);
    }

    case OPI_AST_IMPL:
    {
      OpiTrait *trait = opi_builder_find_trait(bldr, ast->impl.trait);
      if (trait == NULL) {
        opi_error("no such trait, '%s'\n", ast->impl.trait);
        opi_error = 1;
        return build_error();
      }

      OpiType *type = opi_builder_find_type(bldr, ast->impl.target);
      if (type == NULL) {
        opi_error("no such type, '%s'\n", ast->impl.target);
        opi_error = 1;
        return build_error();
      }

      int nfs = ast->impl.nfs;
      OpiIr *methods[nfs];
      char **method_nams = malloc(sizeof(char*) * nfs);
      for (int i = 0; i < nfs; ++i) {
        method_nams[i] = strdup(ast->impl.f_nams[i]);
        methods[i] = opi_builder_build_ir(bldr, ast->impl.fs[i]);
      }

      ImplData *data = malloc(sizeof(ImplData));
      data->trait = trait;
      data->type = type;
      data->f_nams = method_nams;
      data->nnams = nfs;
      opi_t impl_fn = opi_fn_new(impl_for_type, -1);
      opi_fn_set_data(impl_fn, data, impl_data_delete);

      // apply implementer
      return opi_ir_apply(opi_ir_const(impl_fn), methods, nfs);
    }

    case OPI_AST_RETURN:
      return opi_ir_return(opi_builder_build_ir(bldr, ast->ret));

    case OPI_AST_BINOP:
      return opi_ir_binop(ast->binop.opc,
          opi_builder_build_ir(bldr, ast->binop.lhs),
          opi_builder_build_ir(bldr, ast->binop.rhs)
      );

    case OPI_AST_ISOF:
    {
      const char *name = opi_builder_assoc(bldr, ast->isof.of);
      if (!name)
        name = ast->isof.of;

      OpiType *type = opi_builder_find_type(bldr, name);
      if (type) {
        // TODO: need instruction to conver native bool into boolean object.
        OpiIrPattern *pattern = opi_ir_pattern_new_unpack(type, NULL, NULL, 0, NULL);
        OpiIr *expr = opi_builder_build_ir(bldr, ast->isof.expr);
        OpiIr *then = opi_ir_const(opi_true);
        OpiIr *els = opi_ir_const(opi_false);
        return opi_ir_match(pattern, expr, then, els);
      }

      OpiTrait *trait = opi_builder_find_trait(bldr, name);
      if (trait) {
        // TODO: reuse these functions, don't create a new one each time.
        opi_t test_fn = opi_fn_new(test_trait, 1);
        opi_fn_set_data(test_fn, trait, NULL);
        OpiIr *expr = opi_builder_build_ir(bldr, ast->isof.expr);
        return opi_ir_apply(opi_ir_const(test_fn), &expr, 1);
      }

      opi_error("no such type or trait, '%s'\n", ast->isof.of);
      opi_error = 1;
      return build_error();
    }

    case OPI_AST_SETREF:
    {
      // get offset of the variable
      OpiAst *astvar = opi_ast_var(ast->setref.var);
      OpiIr *irvar = opi_builder_build_ir(bldr, astvar);
      opi_assert(irvar->tag == OPI_IR_VAR);
      int offs = irvar->var;
      opi_ast_delete(astvar);
      opi_ir_drop(irvar);

      return opi_ir_setref(offs, opi_builder_build_ir(bldr, ast->setref.val));
    }
  }

  abort();
}

static opi_t
_export(void)
{
  OpiBuilder *bldr = opi_current_fn->data;
  opi_t l = opi_pop();
  for (opi_t it = l; it != opi_nil; it = opi_cdr(it)) {
    opi_t elt = opi_car(it);
    opi_t nam = opi_car(elt);
    opi_t val = opi_cdr(elt);
    opi_builder_def_const(bldr, OPI_STR(nam)->str, val);
  }
  opi_drop(l);
  return opi_nil;
}

OpiBytecode*
opi_build(OpiBuilder *bldr, OpiAst *ast, int mode)
{
  opi_error = 0;
  int ndec0 = bldr->decls.len;
  OpiIr *ir = opi_builder_build_ir(bldr, ast);
  if (opi_error) {
    opi_ir_drop(ir);
    return NULL;
  }
  if (bldr->frame_offset != 0) {
    opi_error("logic error: captures at top-level\n");
    for (int i = 0; i < bldr->frame_offset; ++i)
      opi_trace("  %s\n", bldr->decls.data[i].name);
    exit(EXIT_FAILURE);
  }

  OpiBytecode *bc = opi_bytecode();

  switch (mode) {
    case OPI_BUILD_DEFAULT:
      break;

    case OPI_BUILD_EXPORT:
    {
      size_t ndecls = bldr->decls.len - ndec0;
      if (ndecls == 0)
        break;

      OpiIr *list = opi_ir_const(opi_nil);
      for (size_t i = ndec0; i < bldr->decls.len; ++i) {
        OpiAst *var_ast = opi_ast_var(bldr->decls.data[i].name);
        OpiIr *var_ir = opi_builder_build_ir(bldr, var_ast);
        opi_ast_delete(var_ast);
        OpiIr *nam_ir = opi_ir_const(opi_str_new(bldr->decls.data[i].name));
        OpiIr *pair = opi_ir_binop(OPI_OPC_CONS, nam_ir, var_ir);
        list = opi_ir_binop(OPI_OPC_CONS, pair, list);
      }
      opi_t export_fn = opi_fn_new(_export, 1);
      opi_fn_set_data(export_fn, bldr, NULL);
      OpiIr *export_ir = opi_ir_const(export_fn);
      OpiIr *call = opi_ir_apply(export_ir, &list, 1);

      OpiIr *block[] = { ir, call };
      ir = opi_ir_block(block, 2);

      while (ndecls--)
        opi_decl_destroy(cod_vec_pop(bldr->decls));
      break;
    }

    default:
      opi_assert(!"undefined build mode");
  }

  opi_ir_emit(ir, bc);
  opi_ir_drop(ir);
  opi_bytecode_finalize(bc);

  return bc;
}

int
opi_load(OpiBuilder *bldr, const char *path)
{
  OpiAst *ast = opi_ast_load(path);
  OpiBytecode *bc = opi_build(bldr, ast, OPI_BUILD_EXPORT);
  opi_ast_delete(ast);
  if (bc == NULL)
    return OPI_ERR;
  opi_t ret = opi_vm(bc);
  opi_context_drain_bytecode(bldr->ctx, bc);
  opi_bytecode_delete(bc);
  if (ret->type == opi_undefined_type) {
    opi_drop(ret);
    return OPI_ERR;
  }
  opi_drop(ret);
  return OPI_OK;
}

void
_opi_ir_delete(OpiIr *node)
{
  switch (node->tag) {
    case OPI_IR_CONST:
      opi_unref(node->cnst);
      break;

    case OPI_IR_VAR:
      break;

    case OPI_IR_APPLY:
      opi_ir_unref(node->apply.fn);
      if (node->apply.loc)
        opi_location_delete(node->apply.loc);
      for (size_t i = 0; i < node->apply.nargs; ++i)
        opi_ir_unref(node->apply.args[i]);
      free(node->apply.args);
      break;

    case OPI_IR_FN:
      for (size_t i = 0; i < node->fn.ncaps; ++i)
        opi_ir_unref(node->fn.caps[i]);
      free(node->fn.caps);
      opi_ir_unref(node->fn.body);
      break;

    case OPI_IR_LET:
    case OPI_IR_FIX:
      for (size_t i = 0; i < node->let.n; ++i)
        opi_ir_unref(node->let.vals[i]);
      free(node->let.vals);
      break;

    case OPI_IR_IF:
      opi_ir_unref(node->iff.test);
      opi_ir_unref(node->iff.then);
      opi_ir_unref(node->iff.els);
      break;

    case OPI_IR_BLOCK:
      for (size_t i = 0; i < node->block.n; ++i)
        opi_ir_unref(node->block.exprs[i]);
      free(node->block.exprs);
      break;

    case OPI_IR_MATCH:
      opi_ir_pattern_delete(node->match.pattern);
      opi_ir_unref(node->match.expr);
      if (node->match.then)
        opi_ir_unref(node->match.then);
      if (node->match.els)
        opi_ir_unref(node->match.els);
      break;

    case OPI_IR_RETURN:
      opi_ir_unref(node->ret);
      break;

    case OPI_IR_BINOP:
      opi_ir_unref(node->binop.lhs);
      opi_ir_unref(node->binop.rhs);
      break;

    case OPI_IR_SETREF:
      opi_ir_unref(node->setref.val);
      break;
  }

  free(node);
}

static OpiIr*
ir_new(void)
{
  OpiIr *node = malloc(sizeof(OpiIr));
  node->rc = 0;
  return node;
}

OpiIr*
opi_ir_const(opi_t x)
{
  OpiIr *node = ir_new();
  node->tag = OPI_AST_CONST;
  opi_inc_rc(node->cnst = x);
  return node;
}

OpiIr*
opi_ir_var(size_t offs)
{
  OpiIr *node = ir_new();
  node->tag = OPI_IR_VAR;
  node->var = offs;
  return node;
}

OpiIr*
opi_ir_apply(OpiIr *fn, OpiIr **args, size_t nargs)
{
  OpiIr *node = ir_new();
  node->tag = OPI_IR_APPLY;
  node->apply.fn = fn;
  node->apply.args = malloc(sizeof(OpiIr*) * nargs);
  memcpy(node->apply.args, args, sizeof(OpiIr*) * nargs);
  node->apply.nargs = nargs;
  node->apply.eflag = TRUE;
  node->apply.loc = NULL;
  opi_ir_ref(fn);
  opi_ir_ref_arr(args, nargs);
  return node;
}

OpiIr*
opi_ir_fn(OpiIr **caps, size_t ncaps, size_t nargs, OpiIr *body)
{
  OpiIr *node = ir_new();
  node->tag = OPI_IR_FN;
  node->fn.caps = malloc(sizeof(OpiIr*) * ncaps);
  memcpy(node->fn.caps, caps, sizeof(OpiIr*) * ncaps);
  node->fn.ncaps = ncaps;
  node->fn.nargs = nargs;
  node->fn.body = body;
  opi_ir_ref_arr(caps, ncaps);
  opi_ir_ref(body);
  return node;
}

OpiIr*
opi_ir_let(OpiIr **vals, size_t n)
{
  OpiIr *node = ir_new();
  node->tag = OPI_IR_LET;
  node->let.vals = malloc(sizeof(OpiIr*) * n);
  memcpy(node->let.vals, vals, sizeof(OpiIr*) * n);
  node->let.n = n;
  opi_ir_ref_arr(vals, n);
  return node;
}

OpiIr*
opi_ir_fix(OpiIr **vals, size_t n)
{
  OpiIr *ir = opi_ir_let(vals, n);
  ir->tag = OPI_IR_FIX;
  return ir;
}

OpiIr*
opi_ir_if(OpiIr *test, OpiIr *then, OpiIr *els)
{
  OpiIr *node = ir_new();
  node->tag = OPI_IR_IF;
  node->iff.test = test;
  node->iff.then = then;
  node->iff.els  = els;
  opi_ir_ref(test);
  opi_ir_ref(then);
  opi_ir_ref(els);
  return node;
}


OpiIr*
opi_ir_block(OpiIr **exprs, size_t n)
{
  OpiIr *node = ir_new();
  node->tag = OPI_IR_BLOCK;
  node->block.exprs = malloc(sizeof(OpiIr*) *  n);
  memcpy(node->block.exprs, exprs, sizeof(OpiIr*) * n);
  node->block.n = n;
  node->block.drop = TRUE;
  opi_ir_ref_arr(exprs, n);
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
    size_t n, char *alias)
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
  pattern->unpack.alias = alias ? strdup(alias) : NULL;
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
    if (pattern->unpack.alias)
      free(pattern->unpack.alias);
  }
  free(pattern);
}

OpiIr*
opi_ir_match(OpiIrPattern *pattern, OpiIr *expr, OpiIr *then, OpiIr *els)
{
  OpiIr *node = ir_new();
  node->tag = OPI_IR_MATCH;
  node->match.pattern = pattern;
  node->match.expr = expr;
  node->match.then = then;
  node->match.els = els;
  opi_ir_ref(expr);
  opi_ir_ref(then);
  opi_ir_ref(els);
  return node;
}

OpiIr*
opi_ir_return(OpiIr *val)
{
  OpiIr *node = ir_new();
  node->tag = OPI_IR_RETURN;
  node->ret = val;
  opi_ir_ref(val);
  return node;
}

OpiIr*
opi_ir_binop(int opc, OpiIr *lhs, OpiIr *rhs)
{
  OpiIr *node = ir_new();
  node->tag = OPI_IR_BINOP;
  node->binop.opc = opc;
  node->binop.lhs = lhs;
  node->binop.rhs = rhs;
  opi_ir_ref(lhs);
  opi_ir_ref(rhs);
  return node;
}

OpiIr*
opi_ir_setref(int var, OpiIr *val)
{
  OpiIr *node = ir_new();
  node->tag = OPI_IR_SETREF;
  node->setref.var = var;
  node->setref.val = val;
  opi_ir_ref(val);
  return node;
}
