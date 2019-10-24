#include "opium/opium.h"

#include "jit/jit.h"

#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

#define BINOP(name, op)                                                             \
  static opi_t                                                                      \
  name(void)                                                                        \
  {                                                                                 \
    opi_t lhs = opi_get(1);                                                         \
    opi_t rhs = opi_get(2);                                                         \
    opi_assert(lhs->type == opi_number_type);                                       \
    opi_assert(rhs->type == opi_number_type);                                       \
    opi_t ret = opi_number(opi_number_get_value(lhs) op opi_number_get_value(rhs)); \
    opi_drop(opi_pop());                                                            \
    opi_drop(opi_pop());                                                            \
    return ret;                                                                     \
  }

#define BINOP_CMP(name, op)                                           \
  static opi_t                                                        \
  name(void)                                                          \
  {                                                                   \
    opi_t lhs = opi_get(1);                                           \
    opi_t rhs = opi_get(2);                                           \
    opi_assert(lhs->type == opi_number_type);                         \
    opi_assert(rhs->type == opi_number_type);                         \
    int tmp = opi_number_get_value(lhs) op opi_number_get_value(rhs); \
    opi_drop(opi_pop());                                              \
    opi_drop(opi_pop());                                              \
    return tmp ? opi_true : opi_false;                                \
  }

BINOP(add, +)
BINOP(sub, -)
BINOP(mul, *)
BINOP(div_, /)

BINOP_CMP(lt, <)
BINOP_CMP(gt, >)
BINOP_CMP(le, <=)
BINOP_CMP(ge, >=)
BINOP_CMP(eq, ==)
BINOP_CMP(ne, !=)

static opi_t
is_(void)
{
  opi_t x = opi_get(1);
  opi_t y = opi_get(2);
  opi_t ret = opi_is(x, y) ? opi_true : opi_false;
  opi_drop(opi_pop());
  opi_drop(opi_pop());
  return ret;
}

static opi_t
eq_(void)
{
  opi_t x = opi_get(1);
  opi_t y = opi_get(2);
  opi_t ret = opi_eq(x, y) ? opi_true : opi_false;
  opi_drop(opi_pop());
  opi_drop(opi_pop());
  return ret;
}

static opi_t
equal_(void)
{
  opi_t x = opi_get(1);
  opi_t y = opi_get(2);
  opi_t ret = opi_equal(x, y) ? opi_true : opi_false;
  opi_drop(opi_pop());
  opi_drop(opi_pop());
  return ret;
}

static opi_t
cons_(void)
{
  opi_t car = opi_get(1);
  opi_t cdr = opi_get(2);
  opi_t ret = opi_cons(car, cdr);
  opi_pop();
  opi_pop();
  return ret;
}

static opi_t
car_(void)
{
  opi_t x = opi_pop();
  opi_assert(x->type == opi_pair_type);
  opi_t ret = opi_car(x);
  opi_inc_rc(ret);
  opi_drop(x);
  opi_dec_rc(ret);
  return ret;
}

static opi_t
cdr_(void)
{
  opi_t x = opi_pop();
  opi_assert(x->type == opi_pair_type);
  opi_t ret = opi_cdr(x);
  opi_inc_rc(ret);
  opi_drop(x);
  opi_dec_rc(ret);
  return ret;
}

static opi_t
null_(void)
{
  opi_t x = opi_pop();
  opi_t ret = x == opi_nil ? opi_true : opi_false;
  opi_drop(x);
  return ret;
}

static opi_t
write_(void)
{
  opi_t x = opi_pop();
  opi_write(x, stderr);
  opi_drop(x);
  return opi_nil;
}

static opi_t
display_(void)
{
  opi_t x = opi_pop();
  opi_display(x, stderr);
  opi_drop(x);
  return opi_nil;
}

static opi_t
newline_(void)
{
  putc('\n', stderr);
  return opi_nil;
}

static opi_t
undefined_(void)
{
  return opi_undefined();
}

static opi_t
not_(void)
{
  opi_t x = opi_pop();
  opi_t ret = x == opi_false ? opi_true : opi_false;
  opi_drop(x);
  return ret;
}

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

  struct option opts[] = {
    { "help", FALSE, NULL, 'h' },
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
  opi_builder_def_const(&builder, "+", opi_fn("+", add, 2));
  opi_builder_def_const(&builder, "-", opi_fn("-", sub, 2));
  opi_builder_def_const(&builder, "*", opi_fn("*", mul, 2));
  opi_builder_def_const(&builder, "/", opi_fn("/", div_, 2));
  opi_builder_def_const(&builder, "<", opi_fn("<", lt, 2));
  opi_builder_def_const(&builder, ">", opi_fn(">", gt, 2));
  opi_builder_def_const(&builder, "<=", opi_fn("<=", le, 2));
  opi_builder_def_const(&builder, ">=", opi_fn(">=", ge, 2));
  opi_builder_def_const(&builder, "==", opi_fn("==", eq, 2));
  opi_builder_def_const(&builder, "/=", opi_fn("/=", ne, 2));
  opi_builder_def_const(&builder, ":", opi_fn(":", cons_, 2));
  opi_builder_def_const(&builder, "car", opi_fn("car", car_, 1));
  opi_builder_def_const(&builder, "cdr", opi_fn("cdr", cdr_, 1));
  opi_builder_def_const(&builder, "null?", opi_fn("null?", null_, 1));
  opi_builder_def_const(&builder, "is", opi_fn("is", is_, 2));
  opi_builder_def_const(&builder, "eq", opi_fn("eq", eq_, 2));
  opi_builder_def_const(&builder, "equal", opi_fn("equal", equal_, 2));
  opi_builder_def_const(&builder, "not", opi_fn("not", not_, 1));
  opi_builder_def_const(&builder, "write", opi_fn("write", write_, 1));
  opi_builder_def_const(&builder, "display", opi_fn("display", display_, 1));
  opi_builder_def_const(&builder, "newline", opi_fn("newline", newline_, 0));
  opi_builder_def_const(&builder, "()", opi_fn("()", undefined_, 0));

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

  /*opi_debug("bytecode before optimization:\n");*/
  /*opi_insn_dump(bc.head, stdout);*/

  opi_debug("optimize bytecode\n");
  opi_bytecode_finalize(&bc);

  /*opi_debug("bytecode after optimization:\n");*/
  /*opi_insn_dump(bc.head, stdout);*/

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
