#include "opium/opium.h"

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
  fprintf(stderr, "  --show-bytecode     Show final bytecode.\n");
  exit(err);
}

int
main(int argc, char **argv)
{
  struct cod_strvec srcdirs;
  cod_strvec_init(&srcdirs);
  int show_bytecode = FALSE;
  int use_base = TRUE;

  struct option opts[] = {
    { "help", FALSE, NULL, 'h' },
    { "show-bytecode", FALSE, NULL, 0x01 },
    { "no-base", FALSE, NULL, 0x02 },
    { 0, 0, 0, 0 }
  };
  int opt;
  while ((opt = getopt_long(argc, argv, "hI:", opts, NULL)) > 0) {
    switch (opt) {
      case 'h':
        help_and_exit(argv[0], EXIT_SUCCESS);

      case 'I':
        cod_strvec_push(&srcdirs, optarg);
        break;

      case 0x01:
        show_bytecode = TRUE;
        break;

      case 0x02:
        use_base = FALSE;
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

  /*opi_debug("initialize environment\n");*/
  opi_init();

  struct opi_context ctx;
  opi_context_init(&ctx);
  struct opi_builder builder;
  opi_builder_init(&builder, &ctx);

  /*opi_debug("add source directories:\n");*/
  cod_strvec_push(&srcdirs, dir);
  for (size_t i = 0; i < srcdirs.size; ++i) {
    opi_builder_add_source_directory(&builder, srcdirs.data[i]);
    /*opi_debug("  %2.zu. %s\n", i + 1, builder.srcdirs->data[builder.srcdirs->size - 1]);*/
  }
  cod_strvec_destroy(&srcdirs);

  // change working direcotry to the source location
  /*opi_debug("chdir \"%s\"\n", dir);*/
  opi_assert(chdir(dir) == 0);

  /*opi_debug("define builtins\n");*/
  opi_builtins(&builder);

  /*opi_debug("parse\n");*/
  struct opi_ast *ast = opi_parse(in);
  if (in != stdin)
    fclose(in);

  /*opi_debug("translate AST\n");*/
  struct opi_bytecode bc;
  opi_bytecode_init(&bc);
  opi_build(&builder, ast, &bc);
  opi_ast_delete(ast);

  if (show_bytecode) {
    opi_debug("bytecode:\n");
    opi_insn_dump(bc.head, stdout);
  }

  {
    // Evaluate:
    // initialize stack
    opi_t *stack = malloc(sizeof(opi_t) * 0x1000);
    opi_sp = stack;

    opi_t ret = opi_vm(&bc);
    if (ret->type == opi_undefined_type) {
      opi_error("unhandled error: ");
      opi_display(opi_undefined_get_what(ret), OPI_ERROR);
      putc('\n', OPI_ERROR);
    }
    opi_drop(ret);

    free(stack);
  }
  opi_bytecode_destroy(&bc);

  opi_builder_destroy(&builder);
  opi_context_destroy(&ctx);
  opi_cleanup();
}
