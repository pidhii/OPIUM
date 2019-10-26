#include "opium/opium.h"

#include "jit/jit.h"

#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

 __attribute__((noreturn)) void
help_and_exit(char *argv0, int err)
{
  fprintf(stderr, "usage: %s [OPTIONS] [<script-path>]\n", argv0);
//fprintf(stderr, "-------------------------------------------------------------------------------\n");
  fprintf(stderr, "OPTIONS:\n");
  fprintf(stderr, "  --help  -h          Show this help and exit.\n");
  fprintf(stderr, "          -I  <path>  Add path to the list of directories to be searched for\n");
  fprintf(stderr, "                      imported files.\n");
  exit(err);
}

int
main(int argc, char **argv)
{
  struct opi_strvec srcdirs;
  opi_strvec_init(&srcdirs);
  int show_bytecode = FALSE;

  struct option opts[] = {
    { "help", FALSE, NULL, 'h' },
    { "show-bytecode", FALSE, NULL, 0x01 },
    { 0, 0, 0, 0 }
  };
  int opt;
  while ((opt = getopt_long(argc, argv, "hI:", opts, NULL)) > 0) {
    switch (opt) {
      case 'h':
        help_and_exit(argv[0], EXIT_SUCCESS);

      case 'I':
        opi_strvec_push(&srcdirs, optarg);
        break;

      case 0x01:
        show_bytecode = TRUE;
        break;

      default:
        help_and_exit(argv[0], EXIT_FAILURE);
    }
  }

  FILE *in = stdin;
  char path[PATH_MAX] = ".";
  if (optind < argc) {
    if (!(in = fopen(argv[optind], "r"))) {
      opi_error("%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    strcpy(path, argv[optind]);
  }

  // determine working directory (will chdir later)
  char *dir = dirname(path);
  opi_assert(dir);

  opi_debug("initialize environment\n");
  opi_init();

  opi_debug("initialize builder\n");
  struct opi_builder builder;
  opi_builder_init(&builder);

  opi_debug("add source directories:\n");
  opi_strvec_push(&srcdirs, dir);
  for (size_t i = 0; i < srcdirs.size; ++i) {
    opi_builder_add_source_directory(&builder, srcdirs.data[i]);
    opi_debug("  %2.zu. %s\n", i + 1, builder.srcdirs->data[builder.srcdirs->size - 1]);
  }
  opi_strvec_destroy(&srcdirs);

  // change working direcotry to the source location
  opi_debug("chdir \"%s\"\n", dir);
  opi_assert(chdir(dir) == 0);

  opi_debug("define builtins\n");
  opi_builtins(&builder);

  opi_debug("parse\n");
  struct opi_ast *ast = opi_parse(in);
  if (in != stdin)
    fclose(in);

  opi_debug("translate AST\n");
  struct opi_ir *ir = opi_builder_build(&builder, ast);
  if (builder.frame_offset > 0) {
    for (int i = 0; i < builder.frame_offset; ++i)
      opi_error("undefined variable: '%s'\n", builder.decls.data[i]);
    exit(EXIT_FAILURE);
  }
  opi_ast_delete(ast);

  opi_debug("emit bytecode\n");
  struct opi_bytecode bc;
  opi_bytecode_init(&bc);
  opi_ir_emit(ir, &bc);
  opi_ir_delete(ir);

  if (show_bytecode) {
    opi_debug("bytecode before optimization:\n");
    opi_insn_dump(bc.head, stdout);
  }

  opi_debug("optimize bytecode\n");
  opi_bytecode_finalize(&bc);

  if (show_bytecode) {
    opi_debug("bytecode after optimization:\n");
    opi_insn_dump(bc.head, stdout);
  }

  {
    // Evaluate:
    // initialize stack
    opi_t *stack = malloc(sizeof(opi_t) * 0x1000);
    opi_sp = stack;

    opi_drop(opi_vm(&bc));

    free(stack);
  }
  opi_bytecode_destroy(&bc);

  opi_builder_destroy(&builder);
  opi_cleanup();
}
