#include "opium/opium.h"

static opi_t
sub(void)
{
  opi_t lhs = opi_state_get(opi_current_state, 2);
  opi_t rhs = opi_state_get(opi_current_state, 1);
  opi_assert(lhs->type == opi_number_type);
  opi_assert(rhs->type == opi_number_type);
  opi_t ret = opi_alloc();
  opi_number(ret, opi_number_get_value(lhs) - opi_number_get_value(rhs));

  opi_drop(opi_state_pop(opi_current_state));
  opi_drop(opi_state_pop(opi_current_state));

  return ret;
}

int
main()
{
  struct opi_ast *ast;
  struct opi_ir *ir;
  struct opi_builder builder;
  opi_t sub_fn;

  opi_init();

  sub_fn = opi_alloc();
  opi_fn(sub_fn, "sub", sub, 2);

  ast =
    opi_ast_fn((char*[]) { "a", "b" }, 2,

      opi_ast_fn((char*[]) { }, 0,

        opi_ast_apply(
          opi_ast_const(sub_fn),
          (struct opi_ast*[]) {
            opi_ast_var("a"),
            opi_ast_var("b"),
          }, 2
        )

      )

    );

  /*ast =*/
    /*opi_ast_apply(*/
      /*opi_ast_const(sub_fn),*/
      /*(struct opi_ast*[]) {*/
        /*opi_ast_const(opi_number(opi_alloc(), 40)),*/
        /*opi_ast_const(opi_number(opi_alloc(), 2))*/
      /*}, 2*/
    /*);*/

  opi_builder_init(&builder);
  ir = opi_builder_build(&builder, ast);

  opi_t *stack = malloc(sizeof(opi_t) * 0x1000);
  struct opi_state state;
  state.sp = stack;
  opi_current_state = &state;

  opi_t fn1 = opi_ir_eval(ir);
  printf("fn1: ");
  opi_show(fn1, stdout);
  printf("\n");

  opi_state_push(opi_current_state, opi_number(opi_alloc(), 3));
  opi_state_push(opi_current_state, opi_number(opi_alloc(), 2));
  opi_t fn2 = opi_fn_apply(fn1);
  printf("fn2: ");
  opi_show(fn2, stdout);
  printf("\n");

  opi_t ret = opi_fn_apply(fn2);
  printf("ret: ");
  opi_show(ret, stdout);
  printf("\n");

  opi_drop(fn1);
  opi_drop(fn2);
  opi_drop(ret);

  free(stack);
  opi_builder_destroy(&builder);
  opi_ast_delete(ast);
  opi_ir_delete(ir);
}
