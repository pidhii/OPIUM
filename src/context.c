#include "opium/opium.h"

#include <string.h>
#include <dlfcn.h>

void
opi_context_init(OpiContext *ctx)
{
  cod_ptrvec_init(&ctx->types);
  cod_ptrvec_init(&ctx->bc);
  cod_strvec_init(&ctx->dl_paths);
  cod_ptrvec_init(&ctx->dls);
}

static void
delete_dl(void *dl)
{ dlclose(dl); }

void
opi_context_destroy(OpiContext *ctx)
{
  cod_ptrvec_destroy(&ctx->types, (void*)opi_type_delete);
  cod_ptrvec_destroy(&ctx->bc, (void*)opi_insn_delete);
  cod_strvec_destroy(&ctx->dl_paths);
  cod_ptrvec_destroy(&ctx->dls, delete_dl);
}

void
opi_context_add_type(OpiContext *ctx, opi_type_t type)
{ cod_ptrvec_push(&ctx->types, type, NULL); }

void
opi_context_drain_bytecode(OpiContext *ctx, OpiBytecode *bc)
{ cod_ptrvec_push(&ctx->bc, opi_bytecode_drain(bc), NULL); }

void
opi_context_add_dl(OpiContext *ctx, const char *path, void *dl)
{
  cod_strvec_push(&ctx->dl_paths, path);
  cod_ptrvec_push(&ctx->dls, dl, NULL);
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
  opi_assert(fread(header, 1, 4, fs) == 4);
  opi_assert(fclose(fs) == 0);

  return memcmp(header, elf_header, sizeof(elf_header)) == 0;
}

