#include "opium/opium.h"

#include <stdlib.h>
#include <string.h>

struct stack {
  int *vals;
  size_t size;
  size_t cap;
};

void
stack_init(struct stack *stack)
{
  stack->cap = 0x10;
  stack->size = 0;
  stack->vals = malloc(sizeof(int) * stack->cap);
}

void
stack_destroy(struct stack *stack)
{ free(stack->vals); }

void
stack_push(struct stack *stack, int vid)
{
  if (stack->size == stack->cap) {
    stack->cap <<= 1;
    stack->vals = realloc(stack->vals, sizeof(int) * stack->cap);
  }
  stack->vals[stack->size++] = vid;
}

void
stack_pop(struct stack *stack, size_t n)
{ stack->size -= n; }

static int
emit(struct opi_ir *ir, struct opi_bytecode *bc, struct stack *stack, int tc);

static int
emit_fn(struct opi_ir *ir, struct opi_bytecode *bc, struct stack *stack, int cell)
{
  opi_assert(ir->tag == OPI_IR_FN);

  // get captures
  size_t ncaps = ir->fn.ncaps;
  int caps[ncaps];
  for (size_t i = 0; i < ncaps; ++i)
    caps[i] = emit(ir->fn.caps[i], bc, stack, FALSE);

  // create body
  struct opi_bytecode *body = malloc(sizeof(struct opi_bytecode));
  opi_bytecode_init(body);

  // create separate stack
  struct stack body_stack;
  stack_init(&body_stack);

  // declare captures
  for (size_t i = 0; i < ncaps; ++i) {
    int val = opi_bytecode_ldcap(body, i);
    stack_push(&body_stack, val);
  }

  // declare arguments
  size_t nargs = ir->fn.nargs;
  for (int i = nargs - 1; i >= 0; --i) {
    int val = opi_bytecode_param(body, i + 1);
    stack_push(&body_stack, val);
  }
  // remove arguments from the stack
  if (nargs > 0)
    opi_bytecode_pop(body, nargs);

  int ret = emit(ir->fn.body, body, &body_stack, TRUE);
  opi_bytecode_ret(body, ret);
  stack_destroy(&body_stack);

  opi_bytecode_finalize(body);

  int fn = cell < 0 ? opi_bytecode_alcfn(bc, OPI_VAL_LOCAL) : cell;
  opi_bytecode_finfn(bc, fn, ir->fn.nargs, body, caps, ncaps);
  return fn;
}

static int
emit(struct opi_ir *ir, struct opi_bytecode *bc, struct stack *stack, int tc)
{
  switch (ir->tag) {
    case OPI_IR_CONST:
      return opi_bytecode_const(bc, ir->cnst);

    case OPI_IR_VAR:
      if (stack->size < ir->var) {
        opi_error("[ir:emit:var] try reference %zu/%zu\n", ir->var, stack->size);
      }
      return stack->vals[stack->size - ir->var];

    case OPI_IR_APPLY:
    {
      int args[ir->apply.nargs];
      for (size_t i = 0; i < ir->apply.nargs; ++i)
        args[i] = emit(ir->apply.args[i], bc, stack, FALSE);
      int fn = emit(ir->apply.fn, bc, stack, FALSE);

      if (tc && opi_bytecode_value_is_global(bc, fn)) {
        return opi_bytecode_apply_tailcall_arr(bc, fn, ir->apply.nargs, args);
      } else {
        int ret = opi_bytecode_apply_arr(bc, fn, ir->apply.nargs, args);
        if (ir->apply.eflag) {
          // Implicit error-test:
          // if
          int test = opi_bytecode_testty(bc, ret, opi_undefined_type);
          struct opi_if iff;
          opi_bytecode_if(bc, test, &iff);
          // then
          opi_bytecode_ret(bc, ret);
          // else
          opi_bytecode_if_else(bc, &iff);
          opi_bytecode_if_end(bc, &iff);
        }
        return ret;
      }
    }

    case OPI_IR_BINOP:
    {
      int lhs = emit(ir->binop.lhs, bc, stack, FALSE);
      int rhs = emit(ir->binop.rhs, bc, stack, FALSE);
      int ret = opi_bytecode_binop(bc, ir->binop.opc, lhs, rhs);
      if (ir->binop.opc != OPI_OPC_CONS) {
        // Implicit error-test:
        // if
        int test = opi_bytecode_testty(bc, ret, opi_undefined_type);
        struct opi_if iff;
        opi_bytecode_if(bc, test, &iff);
        // then
        opi_bytecode_ret(bc, ret);
        // else
        opi_bytecode_if_else(bc, &iff);
        opi_bytecode_if_end(bc, &iff);
      }
      return ret;
    }

    case OPI_IR_FN:
      return emit_fn(ir, bc, stack, -1);

    case OPI_IR_LET:
    {
      int vals[ir->let.n];
      for (size_t i = 0; i < ir->let.n; ++i)
        vals[i] = emit(ir->let.vals[i], bc, stack, FALSE);
      for (size_t i = 0; i < ir->let.n; ++i)
        stack_push(stack, vals[i]);
      if (ir->let.body) {
        int ret = emit(ir->let.body, bc, stack, tc);
        stack_pop(stack, ir->let.n);
        return ret;
      } else {
        return opi_bytecode_const(bc, opi_nil);
      }
    }

    case OPI_IR_FIX:
    {
      if (ir->let.n > 0) {
        int vals[ir->let.n];
        for (size_t i = 0; i < ir->let.n; ++i) {
          vals[i] = opi_bytecode_alcfn(bc, OPI_VAL_PHI);
          stack_push(stack, vals[i]);
        }

        opi_bytecode_begscp(bc, ir->let.n);
        for (size_t i = 0; i < ir->let.n; ++i)
          emit_fn(ir->let.vals[i], bc, stack, vals[i]);
        opi_bytecode_endscp(bc, vals, ir->let.n);
      }

      if (ir->let.body) {
        int ret = emit(ir->let.body, bc, stack, tc);
        stack_pop(stack, ir->let.n);
        return ret;
      } else {
        return opi_bytecode_const(bc, opi_nil);
      }
    }

    case OPI_IR_IF:
    {
      // IF
      int test = opi_bytecode_test(bc, emit(ir->iff.test, bc, stack, FALSE));
      struct opi_if iff;
      // PHI
      int phi = opi_bytecode_phi(bc);
      // THEN
      opi_bytecode_if(bc, test, &iff);
      int then_ret = emit(ir->iff.then, bc, stack, tc);
      opi_bytecode_dup(bc, phi, then_ret);
      // ELSE
      opi_bytecode_if_else(bc, &iff);
      int else_ret = emit(ir->iff.els, bc, stack, tc);
      opi_bytecode_dup(bc, phi, else_ret);
      // END IF
      opi_bytecode_if_end(bc, &iff);

      return phi;
    }

    case OPI_IR_BLOCK:
      if (ir->block.n) {
        size_t start = stack->size;
        for (size_t i = 0; i + 1 < ir->block.n; ++i)
          emit(ir->block.exprs[i], bc, stack, FALSE);
        int ret = emit(ir->block.exprs[ir->block.n - 1], bc, stack, tc);
        if (ir->block.drop)
          stack_pop(stack, stack->size - start);
        return ret;
      } else {
        return opi_bytecode_const(bc, opi_nil);
      }

    case OPI_IR_MATCH:
    {
      // evaluate expr
      int expr = emit(ir->match.expr, bc, stack, FALSE);

      // test type
      int test = opi_bytecode_testty(bc, expr, ir->match.type);

      if (ir->match.then == NULL) {
        opi_bytecode_guard(bc, test);

        // load fields
        int vals[ir->match.n];
        for (size_t i = 0; i < ir->match.n; ++i)
          vals[i] = opi_bytecode_ldfld(bc, expr, ir->match.offs[i]);

        // declare values
        for (size_t i = 0; i < ir->match.n; ++i)
          stack_push(stack, vals[i]);

        return opi_bytecode_const(bc, opi_nil);

      } else {
        // IF
        struct opi_if iff;
        // PHI
        int phi = opi_bytecode_phi(bc);
        // THEN
        opi_bytecode_if(bc, test, &iff);
        // load fields
        int vals[ir->match.n];
        for (size_t i = 0; i < ir->match.n; ++i)
          vals[i] = opi_bytecode_ldfld(bc, expr, ir->match.offs[i]);
        // declare values
        for (size_t i = 0; i < ir->match.n; ++i)
          stack_push(stack, vals[i]);
        int then_ret = emit(ir->match.then, bc, stack, tc);
        stack_pop(stack, ir->match.n);
        opi_bytecode_dup(bc, phi, then_ret);
        // ELSE
        opi_bytecode_if_else(bc, &iff);
        int else_ret = emit(ir->match.els, bc, stack, tc);
        opi_bytecode_dup(bc, phi, else_ret);
        // END IF
        opi_bytecode_if_end(bc, &iff);

        return phi;
      }
    }

    case OPI_IR_RETURN:
    {
      int ret = emit(ir->ret, bc, stack, TRUE);
      opi_bytecode_ret(bc, ret);
      return opi_bytecode_const(bc, opi_nil);
    }
  }

  abort();
}

void
opi_ir_emit(struct opi_ir *ir, struct opi_bytecode *bc)
{
  struct stack stack;
  stack_init(&stack);
  int ret = emit(ir, bc, &stack, TRUE);
  opi_bytecode_ret(bc, ret);
  stack_destroy(&stack);
}
