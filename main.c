#include "opium/opium.h"

#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <libgen.h>

#include <readline/readline.h>
#include <readline/history.h>
;

static const char*
logo() {
  static char logo_template[] =
"                                               \n"
"                   d8b                         \n"
"                   Y8P                         \n"
"                                               \n"
"  .d88b.  88888b.  888 888  888 88888b.d88b.   \n"
" d88\"\"88b 888 \"88b 888 888  888 888 \"888 \"88b  \n"
" 888  888 888  888 888 888  888 888  888  888  \n"
" Y88..88P 888 d88P 888 Y88b 888 888  888  888  \n"
"  \"Y88P\"  88888P\"  888  \"Y88888 888  888  888  \n"
"          888                                  \n"
"          888                                  \n"
"          888                                  \n"
"                                               \n"
;
  static char alphabet[] =
".?<..v/*nbRBQ3?,f+<,t3/j^`|,lhK>cP(IM.#H341.&*7/xV}D>3TpYYp~KZ,GLi@>X;/>4rP3;F"
"<SS6DmqV>$Dso=~]{NYJIz\\FhN{iW*<@=i0p]<xCvxHBo@51ip|L3VK?5W4/`rfl@RMs%Oj?!,*,>V"
"$l\\5x2Zrg\\.6cL[>#?LUPTl~:piqPCQ$~pGSe(ty_H.S5|HN<<7fyQ>Cn#B>@$%q.VHZ</`|8>?<jl"
"$/g.i<#>D`=V8Sn>6QuV8puR2aqh?,Lk?sm=v}noh|gx.A..6>KPF]F`vt<;}RDWeF\\NGLPx>p>s=E"
"V.gQT3Tj[J.[P,[}f.Dw0?3E/y**pHhjDA/P)}T,__,__zu5laH?pVgr8<Vq,3Ah/j/Ez<{YoL@([."
",<ng}KE1v:EH8:;o@_pcK5eD(WO!fU1gN.Qx^5fWHujs`v^K}4>M,4.RV^E62?5>.Mo|?R_se9XxeU"
"<fn_6Lv*M/s1Qf@}i@?/+\\WK?rD<r!MC=hAS?WtE?.H/h{WJy;(BxEW}w}%[.48xa!g.]bHe&&`,%U"
"0Yeb.zF)LjD4???KVS><y[}E.H.z&vu5`.6vT0aA:!AvY3VJ5,x<_x>*U|~OcZM1M=l2l9ie.>]_4:"
"e`tTf`m.0U4*n>>5R2y\\Q5b5E.UMO|nWA>A0)0j~3>#/:Alk0vmybJ#X7./%gv&4cf=(x[o[zX[niC"
"k6V|=?io4o{cUQ9TuMDPw@#m?b=E7NO>w#f0{+4<`yt.{8?9R_A#Ljn5t,{L(g7vM<.Db~/WM?/H.l"
"w,uTN/@<7iC.PyxbDj@o/ssCG+u@kVg<S;gqyjr>uez,kB.h+~0^<Ev)mqC.&8<NJ.gxGmH:%on.^^"
"y{:.i^.@$U~<p8^_R:0y./FX?=AMUEaPLGL/[x,s!\\,4IQ0>bVg.2^I[lt>Qt%R?oA>ol_Ny#JU?)1"
";?zAkI_>RYNx<86?:g3CEQ2&/L/3}P1K}J&Qt\\\\[[?<O%XW/kGf,..o>lPx<(HL!<]6FAq&W0xGmID"
"./Dt.^9O/}?<j*5<$|;G*<uDXZmHAh/)BfJa5ctwOLW0Vc}*@7/Rq&7Q[c2U@u.z`JU+^<TOG|7LaI"
"*14<[x#/[,(,JxL`ow/QCp[,`;Nyi}}P>BIWT#llvS!TVbnN]4$G]_hS?H.3|Pvbss#/BJ6YpQo7e."
;

  static char logo[sizeof logo_template] = { 0 };
  if (logo[0] == 0) {
    srand(time(0));
    for (size_t i = 0; i < sizeof logo_template - 1; ++i)
      logo[i] = isspace(logo_template[i])
              ? logo_template[i]
              : alphabet[rand() % (sizeof alphabet - 1)];
  }

  return logo;
}

 __attribute__((noreturn)) void
help_and_exit(char *argv0, int err)
{
  if (err == EXIT_SUCCESS) {
    fputs("\x1b[38;5;36;1m", stderr);
    fputs(logo(), stderr);
    fputs("\x1b[0m", stderr);
  }

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
  if (optind < argc && strcmp(argv[optind-1], "--") != 0) {
    if (!(in = fopen(argv[optind], "r"))) {
      opi_error("failed to open file \"%s\" (%s)\n", argv[optind],
          strerror(errno));
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
    argv_ = opi_cons(opi_str_new(argv[i]), argv_);
  if (in == stdin)
    argv_ = opi_cons(opi_str_from_char('-'), argv_);
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
    opi_t val = opi_str_new(eq + 1);
    env_list = opi_cons(opi_cons(key, val), env_list);
  }
  opi_t env_ = opi_table(env_list, TRUE);
  opi_builder_def_const(&builder, "Sys.env", env_);

  // Builtins.
  opi_builtins(&builder);

  if (use_base) {
    // Add base-library.
    opi_load(&builder, "base");
  }

  if (in == stdin) {
/*******************************************************************************
 * Begin REPL.
 */
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

    fputs("\x1b[38;5;36;1m", stderr);
    fputs(logo(), stderr);
    fputs("\x1b[0m", stderr);
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
            cod_vec_push(input_buf, ' ');
            cnt--;
          } else {
            // real parse error
            opi_error("%s\n", errorptr);
            free(errorptr);
            input_buf.len = 0;
            opi_error = 0;
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
      if (bc == NULL) {
        opi_error = 0;
        continue;
      }

      if (show_bytecode) {
        opi_debug("bytecode:\n");
        opi_insn_dump(bc->head, stdout);
      }

      opi_t ret = opi_vm(bc);
      if (ret->type == opi_undefined_type) {
        opi_error("unhandled error: ");
        opi_display(OPI_UNDEFINED(ret)->what, OPI_ERROR);
        putc('\n', OPI_ERROR);
        opi_trace_t *trace = opi_undefined_get_trace(ret);
        if (trace->len > 0) {
          for (size_t i = 0; i < trace->len; ++i) {
            OpiLocation *loc = trace->data[i];
            opi_trace("%s:%d:%d:\n", loc->path, loc->fl, loc->fc);
            opi_show_location(OPI_ERROR, loc->path, loc->fc, loc->fl, loc->lc, loc->ll);
          }
        }
        opi_error = 0;
      } else if (ret != opi_nil) {
        opi_display(ret, stdout);
        putc('\n', stdout);
      }
      opi_drop(ret);
      opi_context_drain_bytecode(&ctx, bc);
      opi_bytecode_delete(bc);
    }
/*
 * End REPL.
 ******************************************************************************/

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
      opi_display(OPI_UNDEFINED(ret)->what, OPI_ERROR);
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
