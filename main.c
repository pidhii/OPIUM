#include "opium/opium.h"

#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <libgen.h>

#include <readline/readline.h>
#include <readline/history.h>

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
main(int argc, char **argv, char **env)
{
  struct cod_strvec srcdirs;
  cod_strvec_init(&srcdirs);
  int show_bytecode = FALSE;
  int use_base = TRUE;

  char *opium_path = getenv("OPIUM_PATH");
  if (opium_path)
    cod_strvec_push(&srcdirs, opium_path);

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
      cod_strvec_destroy(&srcdirs);
      exit(EXIT_FAILURE);
    }
    strcpy(path, argv[optind]);
  }

  // determine working directory (will chdir later)
  char *dir = dirname(path);
  opi_assert(dir);

  /*opi_debug("initialize environment\n");*/
  opi_init(OPI_INIT_DEFAULT);

  OpiContext ctx;
  opi_context_init(&ctx);

  OpiBuilder builder;
  opi_builder_init(&builder, &ctx);

  // Add source-directories.
  cod_strvec_push(&srcdirs, dir);
  for (size_t i = 0; i < srcdirs.size; ++i) {
    opi_builder_add_source_directory(&builder, srcdirs.data[i]);
  }
  cod_strvec_destroy(&srcdirs);

  // Add command-line arguments.
  opi_t argv_ = opi_nil;
  for (int i = argc - 1; i >= optind; --i)
    argv_ = opi_cons(opi_string_new(argv[i]), argv_);
  opi_builder_def_const(&builder, "Sys.argv", argv_);
  
  // Add environment variables.
  opi_t env_list = opi_nil;
  for (int i = 0; env[i]; ++i) {
    char *eq = strchr(env[i], '=');
    opi_assert(eq);
    char key_buf[eq - env[i] + 1];
    memcpy(key_buf, env[i], eq - env[i]);
    key_buf[eq - env[i]] = 0;
    opi_t key = opi_symbol(key_buf);
    opi_t val = opi_string_new(eq + 1);
    env_list = opi_cons(opi_cons(key, val), env_list);
  }
  opi_t env_ = opi_table(env_list, TRUE);
  opi_builder_def_const(&builder, "Sys.env", env_);

  // change working direcotry to the source location
  /*opi_debug("chdir %s\n", dir);*/
  /*opi_assert(chdir(dir) == 0);*/

  opi_builtins(&builder);

  if (in == stdin) {
    // REPL
    char history_path[PATH_MAX];

    using_history();
    sprintf(history_path, "%s/.opium_history", getenv("HOME"));

    if (system("test -f $HOME/.opium_history") == 0)
      read_history(history_path);
    else
      system("touch $HOME/.opium_history");

    OpiAst *ast;
    OpiBytecode *bc;

    cod_vec(char) input_buf;
    cod_vec_init(input_buf);

    char *line;

    size_t cnt = 0;
    puts("\e[1mOpium REPL\e[0m");
    puts("press <C-d> to exit");
    puts("");

    int nhist = 0;
    while (TRUE) {

      while (TRUE) {
        // read line
        char prompt[0x40];
        sprintf(prompt, "opium [%zu] ", cnt++);
        if (!(line = readline(prompt))) {
          printf("End of input reached.\n");
          if (append_history(nhist, history_path))
            opi_warning("failed to write history: %s\n", strerror(errno));
          cod_vec_destroy(input_buf);
          goto cleanup;
        }

        if (line[0] == 0) {
          free(line);
          continue;
        }

        // append line input buffer
        for (int i = 0; line[i]; ++i)
          cod_vec_push(input_buf, line[i]);
        free(line);

        // open input buffer as file stream to pass into scanner
        FILE *parser_stream = fmemopen(input_buf.data, input_buf.len, "r");

        // create fresh scanner
        OpiScanner *scan = opi_scanner();
        opi_scanner_set_in(scan, parser_stream);

        // try parse
        opi_error = 0;
        char *errorptr;
        ast = opi_parse_expr(scan, &errorptr);
        fclose(parser_stream);
        opi_scanner_delete(scan);

        if (opi_error) {
          if (strcmp(errorptr, "syntax error, unexpected $end") == 0) {
            // must read more lines
            free(errorptr);
          } else {
            // real parse error
            opi_error("%s\n", errorptr);
            free(errorptr);
            input_buf.len = 0;
          }
        } else {
          // parser succeed
          cod_vec_push(input_buf, 0);
          add_history(input_buf.data);
          nhist += 1;
          input_buf.len = 0;
          break;
        }
      }

      bc = opi_build(&builder, ast, OPI_BUILD_EXPORT);
      opi_ast_delete(ast);
      if (bc == NULL)
        continue;

      if (show_bytecode) {
        opi_debug("bytecode:\n");
        opi_insn_dump(bc->head, stdout);
      }

      opi_t ret = opi_vm(bc);
      if (ret->type == opi_undefined_type) {
        opi_error("unhandled error: ");
        opi_display(opi_undefined_get_what(ret), OPI_ERROR);
        putc('\n', OPI_ERROR);
        opi_trace_t *trace = opi_undefined_get_trace(ret);
        if (trace->len > 0) {
          for (size_t i = 0; i < trace->len; ++i) {
            OpiLocation *loc = trace->data[i];
            opi_trace("%s:%d:%d:\n", loc->path, loc->fl, loc->fc);
            opi_show_location(OPI_ERROR, loc->path, loc->fc, loc->fl, loc->lc, loc->ll);
          }
        }
      } else if (ret != opi_nil) {
        opi_display(ret, stdout);
        putc('\n', stdout);
      }
      opi_drop(ret);
      opi_context_drain_bytecode(&ctx, bc);
      opi_bytecode_delete(bc);
    }

  } else {
    OpiAst *ast = opi_parse(in);
    fclose(in);
    if (ast == NULL)
      goto cleanup;

    OpiBytecode *bc = opi_build(&builder, ast, OPI_BUILD_DEFAULT);
    opi_ast_delete(ast);
    if (bc == NULL)
      goto cleanup;

    if (show_bytecode) {
      opi_debug("bytecode:\n");
      opi_insn_dump(bc->head, stdout);
    }

    opi_t ret = opi_vm(bc);
    if (ret->type == opi_undefined_type) {
      opi_error("unhandled error: ");
      opi_display(opi_undefined_get_what(ret), OPI_ERROR);
      putc('\n', OPI_ERROR);
      opi_trace_t *trace = opi_undefined_get_trace(ret);
      if (trace->len > 0) {
        for (size_t i = 0; i < trace->len; ++i) {
          OpiLocation *loc = trace->data[i];
          opi_trace("%s:%d:%d:\n", loc->path, loc->fl, loc->fc);
          opi_show_location(OPI_ERROR, loc->path, loc->fc, loc->fl, loc->lc, loc->ll);
        }
      }
      opi_error = 1;
    }
    opi_drop(ret);
    opi_bytecode_delete(bc);
  }

cleanup:
  opi_builder_destroy(&builder);
  opi_context_destroy(&ctx);
  opi_cleanup();

  return opi_error ? EXIT_FAILURE : EXIT_SUCCESS;
}
