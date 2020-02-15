#include "opium/opium.h"
#include "opium/lambda.h"

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
emit(OpiIr *ir, OpiBytecode *bc, struct stack *stack, int tc);

OpiBytecode*
opi_emit_free_fn_body(OpiIr *ir, int nargs)
{
  // create body
  OpiBytecode *body = opi_bytecode();

  // create separate stack
  struct stack body_stack;
  stack_init(&body_stack);

  // declare arguments
  for (int i = nargs - 1; i >= 0; --i) {
    int val = opi_bytecode_param(body, i + 1);
    stack_push(&body_stack, val);
  }
  // remove arguments from the stack
  if (nargs > 0)
    opi_bytecode_pop(body, nargs);

  int ret = emit(ir, body, &body_stack, TRUE);
  opi_bytecode_ret(body, ret);
  stack_destroy(&body_stack);

  opi_bytecode_finalize(body);

  return body;
}

static int
emit_fn(OpiIr *ir, OpiBytecode *bc, struct stack *stack, int cell)
{
  opi_assert(ir->tag == OPI_IR_FN);

  // get captures
  size_t ncaps = ir->fn.ncaps;
  int caps[ncaps];
  for (size_t i = 0; i < ncaps; ++i) {
    int vid = stack->vals[stack->size - ir->fn.caps[i]->var];
    caps[i] = vid;
  }

  // create body
  OpiBytecode *body = opi_bytecode();

  // create separate stack
  struct stack body_stack;
  stack_init(&body_stack);

  // declare captures
  for (size_t i = 0; i < ncaps; ++i) {
    int val = opi_bytecode_ldcap(body, i);
    body->vinfo[val].is_var = bc->vinfo[caps[i]].is_var;
    stack_push(&body_stack, val);
  }

  // declare arguments
  int nargs = ir->fn.nargs;
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
  opi_bytecode_finfn(bc, fn, ir->fn.nargs, body, ir->fn.body, caps, ncaps);
  return fn;
}

static void
trace_delete(OpiFn *fn)
{
  OpiLocation *loc = fn->data;
  opi_location_delete(loc);
  opi_fn_delete(fn);
}

static opi_t
trace(void)
{
  OpiLocation *loc = opi_current_fn->data;
  opi_t err = opi_pop();
  opi_assert(err->type == opi_undefined_type);
  OpiUndefined *u = (void*)err;
  cod_vec_push(*u->trace, opi_location_copy(loc));
  return err;
}

static void
emit_match_with_leak(int val, OpiIrPattern *pattern, OpiBytecode *bc, struct stack *stack)
{
  switch (pattern->tag) {
    case OPI_PATTERN_IDENT:
    {
      // declare variable
      stack_push(stack, val);
      return;
    }

    case OPI_PATTERN_UNPACK:
    {
      // test type
      if (bc->vinfo[val].vtype) {
        if (bc->vinfo[val].vtype != pattern->unpack.type) {
          opi_error("type won't match\n");
          abort();
        }
        opi_debug("optimized out a guard\n");
      } else {
        int test = opi_bytecode_testty(bc, val, pattern->unpack.type);
        opi_bytecode_guard(bc, test);
      }

      // emit sub-patterns on the field
      for (size_t i = 0; i < pattern->unpack.n; ++i) {
        int field = opi_bytecode_ldfld(bc, val, pattern->unpack.offs[i]);
        emit_match_with_leak(field, pattern->unpack.subs[i], bc, stack);
      }

      if (pattern->unpack.alias)
        stack_push(stack, val);
    }
  }
}

typedef cod_vec(OpiIf) iffs_t;

static void
emit_match_with_then(int expr, OpiIrPattern *pattern, OpiBytecode *bc,
    struct stack *stack, iffs_t *iffs)
{
  // start if-statement
  if (bc->vinfo[expr].vtype == pattern->unpack.type) {
      opi_debug("optimizaed out a type check (in match)\n");
  } else {
    OpiIf iff;
    int test = opi_bytecode_testty(bc, expr, pattern->unpack.type);
    opi_bytecode_if(bc, test, &iff);
    cod_vec_push(*iffs, iff);
    opi_bytecode_set_vtype(bc, expr, pattern->unpack.type);
  }

  // evaluate sub-patterns
  for (int i = 0; i < (int)pattern->unpack.n; ++i) {
    OpiIrPattern *sub = pattern->unpack.subs[i];

    int field = opi_bytecode_ldfld(bc, expr, pattern->unpack.offs[i]);
    if (sub->tag == OPI_PATTERN_UNPACK) {
      emit_match_with_then(field, pattern->unpack.subs[i], bc, stack, iffs);
      if (pattern->unpack.alias)
        stack_push(stack, field);
    } else {
      stack_push(stack, field);
    }
  }
}

static int
emit_match_onelevel_with_then(OpiIr *then, OpiIr *els, int expr,
    OpiIrPattern *pattern, OpiBytecode *bc, struct stack *stack, int tc)
{
  int ulstart = bc->ulist.len;

  if (bc->vinfo[expr].vtype) {
    /*
     * Resolve branch now.
     */
    if (bc->vinfo[expr].vtype == pattern->unpack.type) {
      opi_debug("optimize one-level match (then-branch)\n");

      int vals[pattern->unpack.n];
      for (size_t i = 0; i < pattern->unpack.n; ++i)
        vals[i] = opi_bytecode_ldfld(bc, expr, pattern->unpack.offs[i]);

      // declare values and alias
      for (size_t i = 0; i < pattern->unpack.n; ++i)
        stack_push(stack, vals[i]);
      if (pattern->unpack.alias)
        stack_push(stack, expr);

      int ret = emit(then, bc, stack, tc);

      // pop values and alias
      stack_pop(stack, pattern->unpack.n);
      if (pattern->unpack.alias)
        stack_pop(stack, 1);

      return ret;

    } else {
      opi_debug("optimize one-level match (else-branch)\n");
      return emit(els, bc, stack, tc);
    }

  } else {
    /*
     * Resolve branch in runtime.
     */
    int ulstart = bc->ulist.len;

    // IF
    OpiIf iff;
    int test = opi_bytecode_testty(bc, expr, pattern->unpack.type);
    int phi = opi_bytecode_phi(bc);
    // THEN
    opi_bytecode_if(bc, test, &iff);

    // load fields
    int vals[pattern->unpack.n];
    for (size_t i = 0; i < pattern->unpack.n; ++i)
      vals[i] = opi_bytecode_ldfld(bc, expr, pattern->unpack.offs[i]);
    // declare values
    for (size_t i = 0; i < pattern->unpack.n; ++i)
      stack_push(stack, vals[i]);
    if (pattern->unpack.alias)
      stack_push(stack, expr);
    opi_bytecode_set_vtype(bc, expr, pattern->unpack.type);
    int then_ret = emit(then, bc, stack, tc);
    opi_bytecode_restore_vtypes(bc, ulstart);

    stack_pop(stack, pattern->unpack.n);
    if (pattern->unpack.alias)
      stack_pop(stack, 1);
    opi_bytecode_dup(bc, phi, then_ret);
    // ELSE
    opi_bytecode_if_else(bc, &iff);
    int else_ret = emit(els, bc, stack, tc);
    opi_bytecode_dup(bc, phi, else_ret);
    // END IF
    opi_bytecode_if_end(bc, &iff);

    if (bc->vinfo[then_ret].vtype == bc->vinfo[else_ret].vtype)
      bc->vinfo[phi].vtype = bc->vinfo[then_ret].vtype;
    return phi;
  }
}

static void
emit_error_test(OpiBytecode *bc, int ret, OpiLocation *loc)
{
  // if
  int test = opi_bytecode_testty(bc, ret, opi_undefined_type);
  OpiIf iff;
  opi_bytecode_if(bc, test, &iff);
  // then
  if (loc) {
    opi_t trace_fn = opi_fn_new(trace, 1);
    opi_fn_set_data(trace_fn, opi_location_copy(loc), trace_delete);
    int trace_var = opi_bytecode_const(bc, trace_fn);
    opi_bytecode_ret(bc, opi_bytecode_apply_arr(bc, trace_var, 1, &ret));
  } else {
    opi_bytecode_ret(bc, ret);
  }
  // else
  opi_bytecode_if_else(bc, &iff);
  opi_bytecode_if_end(bc, &iff);
}

static int
emit(OpiIr *ir, OpiBytecode *bc, struct stack *stack, int tc)
{
  switch (ir->tag) {
    case OPI_IR_CONST:
    {
      int ret = opi_bytecode_const(bc, ir->cnst);
      bc->vinfo[ret].vtype = ir->cnst->type;
      return ret;
    }

    case OPI_IR_VAR:
    {
      if (stack->size < ir->var) {
        opi_error("[ir:emit:var] try reference %zu/%zu\n", ir->var, stack->size);
        abort();
      }
      int vid = stack->vals[stack->size - ir->var];
      if (bc->vinfo[vid].is_var)
        return opi_bytecode_deref(bc, vid);
      else
        return vid;
    }

    case OPI_IR_APPLY:
    {
      /* Evaluate arguments and function */
      int args[ir->apply.nargs];
      for (size_t i = 0; i < ir->apply.nargs; ++i)
        args[i] = emit(ir->apply.args[i], bc, stack, FALSE);
      int fn = emit(ir->apply.fn, bc, stack, FALSE);

      /* Try optimizations for constant functions */
      if (bc->vinfo[fn].c) {
        opi_t fn_val = bc->vinfo[fn].c;
        if (opi_unlikely(fn_val->type != opi_fn_type)) {
          opi_error("[ir:emit:apply] not a function\n");
          abort();
        }
        if (opi_test_arity(opi_fn_get_arity(fn_val), ir->apply.nargs)) {
          if (opi_is_lambda(fn_val)) {
            static int id = 0;
            /* Inline. */
            int nargs = ir->apply.nargs;
            size_t s0 = stack->size;
            for (int i = nargs - 1; i >= 0; --i)
              stack_push(stack, args[i]);
            OpiLambda *lam = OPI_FN(fn_val)->data;
            opi_assert(lam->ncaps == 0);
            int ret = emit(lam->ir, bc, stack, tc);
            stack_pop(stack, nargs);
            opi_assert(stack->size == s0);
            bc->vinfo[ret].vtype = ir->vtype;
            return ret;
          } else {
            /* Resolve arity statically. */
            int ret = opi_bytecode_applyi_arr(bc, fn, ir->apply.nargs, args);
            if (ir->apply.eflag)
              emit_error_test(bc, ret, ir->apply.loc);
            bc->vinfo[ret].vtype = ir->vtype;
            return ret;
          }
        }
      }

      /* Dynamic dispatch */
      if (tc && opi_bytecode_value_is_global(bc, fn)) {
        /* Tail Call */
        return opi_bytecode_apply_tailcall_arr(bc, fn, ir->apply.nargs, args);
      } else {
        int ret = opi_bytecode_apply_arr(bc, fn, ir->apply.nargs, args);
        bc->vinfo[ret].vtype = ir->vtype;
        if (ir->vtype == NULL && ir->apply.eflag)
          emit_error_test(bc, ret, ir->apply.loc);
        return ret;
      }
    }

    case OPI_IR_BINOP:
    {
      int lhs = emit(ir->binop.lhs, bc, stack, FALSE);
      int rhs = emit(ir->binop.rhs, bc, stack, FALSE);
      int ret = opi_bytecode_binop(bc, ir->binop.opc, lhs, rhs);
      opi_type_t vtype = NULL;
      int do_error_test;

      switch (ir->binop.opc) {
        case OPI_OPC_CONS:
          vtype = opi_pair_type;
          do_error_test = FALSE;
          break;

        case OPI_OPC_ADD ... OPI_OPC_FMOD:
          if (bc->vinfo[lhs].vtype == opi_num_type &&
              bc->vinfo[rhs].vtype == opi_num_type)
          {
            opi_debug("optimized binop\n");
            vtype = opi_num_type;
            do_error_test = FALSE;
          } else {
            vtype = NULL;
            do_error_test = TRUE;
          }
        break;

        case OPI_OPC_NUMEQ ... OPI_OPC_GE:
        if (bc->vinfo[lhs].vtype == opi_num_type &&
            bc->vinfo[rhs].vtype == opi_num_type)
        {
          opi_debug("optimized binop\n");
          vtype = opi_boolean_type;
          do_error_test = FALSE;
        } else {
          vtype = NULL;
          do_error_test = TRUE;
        }
        break;

        default:
          opi_error("undefined binary operator\n");
          abort();
      }

      if (do_error_test) {
        // Implicit error-test:
        // if
        int test = opi_bytecode_testty(bc, ret, opi_undefined_type);
        OpiIf iff;
        opi_bytecode_if(bc, test, &iff);
        // then
        opi_bytecode_ret(bc, ret);
        // else
        opi_bytecode_if_else(bc, &iff);
        opi_bytecode_if_end(bc, &iff);
      }

      bc->vinfo[ret].vtype = vtype;
      return ret;
    }

    case OPI_IR_FN:
      return emit_fn(ir, bc, stack, -1);

    case OPI_IR_LET:
    {
      int vals[ir->let.n];
      for (size_t i = 0; i < ir->let.n; ++i) {
        vals[i] = emit(ir->let.vals[i], bc, stack, FALSE);
        bc->vinfo[vals[i]].is_var = ir->let.is_vars;
      }
      for (size_t i = 0; i < ir->let.n; ++i)
        stack_push(stack, vals[i]);
      return opi_bytecode_const(bc, opi_nil);
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
        for (size_t i = 0; i < ir->let.n; ++i) {
          emit_fn(ir->let.vals[i], bc, stack, vals[i]);
        }
        opi_bytecode_endscp(bc, vals, ir->let.n);
      }

      return opi_bytecode_const(bc, opi_nil);
    }

    case OPI_IR_IF:
    {
      // IF
      int test = opi_bytecode_test(bc, emit(ir->iff.test, bc, stack, FALSE));
      OpiIf iff;
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

      if (bc->vinfo[then_ret].vtype == bc->vinfo[else_ret].vtype)
        bc->vinfo[phi].vtype = bc->vinfo[then_ret].vtype;
      return phi;
    }

    case OPI_IR_BLOCK:
      if (ir->block.n) {
        size_t start = stack->size;
        int ulstart = bc->ulist.len;
        for (size_t i = 0; i + 1 < ir->block.n; ++i)
          emit(ir->block.exprs[i], bc, stack, FALSE);
        int ret = emit(ir->block.exprs[ir->block.n - 1], bc, stack, tc);
        if (ir->block.drop)
          stack_pop(stack, stack->size - start);
        opi_bytecode_restore_vtypes(bc, ulstart);
        return ret;
      } else {
        return opi_bytecode_const(bc, opi_nil);
      }

    case OPI_IR_MATCH:
    {
      // evaluate expr
      int expr = emit(ir->match.expr, bc, stack, FALSE);

      if (ir->match.then == NULL) {
        // match pattern
        emit_match_with_leak(expr, ir->match.pattern, bc, stack);
        return opi_bytecode_const(bc, opi_nil);

      } else {
        // special case for bare identifier
        if (ir->match.pattern->tag == OPI_PATTERN_IDENT) {
          stack_push(stack, expr);
          int ret = emit(ir->match.then, bc, stack, tc);
          stack_pop(stack, 1);
          return ret;
        }

        // special case for 1-level pattern
        OpiIrPattern *pattern = ir->match.pattern;
        int is_onelevel = TRUE;
        for (size_t i = 0; i <  pattern->unpack.n; ++i) {
          if (pattern->unpack.subs[i]->tag != OPI_PATTERN_IDENT) {
            is_onelevel = FALSE;
            break;
          }
        }
        if (is_onelevel) {
          return emit_match_onelevel_with_then(ir->match.then, ir->match.els,
              expr, pattern, bc, stack, tc);
        }

        // general case with nested unpacks
        int stack_start = stack->size;
        int ulstart = bc->ulist.len;
        int var = opi_bytecode_var(bc);
        opi_bytecode_set(bc, var, 1);
        int phi = opi_bytecode_phi(bc);
        iffs_t iffs;
        cod_vec_init(iffs);
        emit_match_with_then(expr, ir->match.pattern, bc, stack, &iffs);
        opi_bytecode_set(bc, var, 0);
        // TODO: should be able to return right from here if this is in
        // tail-call position
        int then_ret = emit(ir->match.then, bc, stack, tc);
        opi_bytecode_dup(bc, phi, then_ret);
        stack_pop(stack, stack->size - stack_start);
        opi_bytecode_restore_vtypes(bc, ulstart);
        // close if-statements
        for (int i = iffs.len - 1; i >= 0; --i) {
          OpiIf *iff = iffs.data + i;
          opi_bytecode_if_else(bc, iff);
          opi_bytecode_if_end(bc, iff);
        }
        cod_vec_destroy(iffs);

        OpiIf iff;
        opi_bytecode_if(bc, var, &iff);
        int else_ret = emit(ir->match.els, bc, stack, tc);
        opi_bytecode_dup(bc, phi, else_ret);
        opi_bytecode_if_else(bc, &iff);
        opi_bytecode_if_end(bc, &iff);

        if (bc->vinfo[then_ret].vtype == bc->vinfo[else_ret].vtype)
          bc->vinfo[phi].vtype = bc->vinfo[then_ret].vtype;
        return phi;
      }
    }

    case OPI_IR_RETURN:
    {
      int ret = emit(ir->ret, bc, stack, TRUE);
      opi_bytecode_ret(bc, ret);
      return opi_bytecode_const(bc, opi_nil);
    }

    case OPI_IR_SETVAR:
    {
      if ((int)stack->size < ir->setvar.var) {
        opi_error("[ir:emit:setvar] try reference %zu/%zu\n",
            (size_t)ir->setvar.var, stack->size);
        abort();
      }
      int vid = stack->vals[stack->size - ir->setvar.var];
      assert(bc->vinfo[vid].is_var);
      int val = emit(ir->setvar.val, bc, stack, FALSE);
      opi_bytecode_setvar(bc, vid, val);
      return val;
    }
  }

  abort();
}

void
opi_ir_emit(OpiIr *ir, OpiBytecode *bc)
{
  struct stack stack;
  stack_init(&stack);
  int ret = emit(ir, bc, &stack, TRUE);
  opi_bytecode_ret(bc, ret);
  stack_destroy(&stack);
}
