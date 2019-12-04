#include "opium/opium.h"

#include <string.h>
#include <dlfcn.h>
#include <errno.h>

void
opi_context_init(OpiContext *ctx)
{
  cod_vec_init(ctx->types);
  cod_vec_init(ctx->traits);
  cod_vec_init(ctx->bc);
  cod_strvec_init(&ctx->dl_paths);
  cod_vec_init(ctx->dls);
}

void
opi_context_destroy(OpiContext *ctx)
{
  // Note: MUST delete traits before types
  cod_vec_iter(ctx->traits, i, x, opi_trait_delete(x));
  cod_vec_destroy(ctx->traits);

  cod_vec_iter(ctx->types, i, x, opi_type_delete(x));
  cod_vec_destroy(ctx->types);

  cod_vec_iter(ctx->bc, i, x, opi_insn_delete(x));
  cod_vec_destroy(ctx->bc);

  cod_strvec_destroy(&ctx->dl_paths);

  cod_vec_iter(ctx->dls, i, x, dlclose(x));
  cod_vec_destroy(ctx->dls);
}

void
opi_context_add_type(OpiContext *ctx, opi_type_t type)
{ cod_vec_push(ctx->types, type); }

void
opi_context_add_trait(OpiContext *ctx, OpiTrait *trait)
{ cod_vec_push(ctx->traits, trait); }

void
opi_context_drain_bytecode(OpiContext *ctx, OpiBytecode *bc)
{ cod_vec_push(ctx->bc, opi_bytecode_drain(bc)); }

void
opi_context_add_dl(OpiContext *ctx, const char *path, void *dl)
{
  cod_strvec_push(&ctx->dl_paths, path);
  cod_vec_push(ctx->dls, dl);
}

void*
opi_context_find_dl(OpiContext *ctx, const char *path)
{
  int idx = cod_strvec_find(&ctx->dl_paths, path);
  return idx < 0 ? NULL : ctx->dls.data[idx];
}

int
opi_is_dl(const char *path)
{
  static char elf_header[] = { 0x7F, 'E', 'L', 'F' };
  char header[4];

  FILE *fs = fopen(path, "r");
  opi_assert(fs);
  if (fread(header, 1, 4, fs) != 4) {
    opi_error("%s\n", strerror(errno));
    abort();
  }
  opi_assert(fclose(fs) == 0);

  return memcmp(header, elf_header, sizeof(elf_header)) == 0;
}

