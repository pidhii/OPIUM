#include "opium/opium.h"
#include "opium/lambda.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

static inline __attribute__((always_inline)) opi_t
vm(OpiState *state)
{
  OpiBytecode *bc = state->bc;
  OpiFlatInsn *restrict ip = state->ip;
  opi_t *restrict r = state->reg;

  OpiRecScope *scp = NULL;
  size_t scpcnt = 0;

  while (1) {
    switch (ip->opc) {
      case OPI_OPC_NOP:
      case OPI_OPC_VAR:
        break;

      case OPI_OPC_SET:
        r[OPI_SET_REG(ip)] = (void*)OPI_SET_ARG_VAL(ip);
        break;

#define NUM_BINOP(opc, op)                                                                               \
      case opc:                                                                                          \
      {                                                                                                  \
        opi_t lhs = r[OPI_BINOP_REG_LHS(ip)];                                                            \
        opi_t rhs = r[OPI_BINOP_REG_RHS(ip)];                                                            \
        if (opi_unlikely(lhs->type != opi_num_type || rhs->type != opi_num_type))                  \
          r[OPI_BINOP_REG_OUT(ip)] = opi_undefined(opi_symbol("type-error"));                            \
        else                                                                                             \
          r[OPI_BINOP_REG_OUT(ip)] = opi_num_new(opi_num_get_value(lhs) op opi_num_get_value(rhs)); \
        break;                                                                                           \
      }
      NUM_BINOP(OPI_OPC_ADD, +)
      NUM_BINOP(OPI_OPC_SUB, -)
      NUM_BINOP(OPI_OPC_MUL, *)
      NUM_BINOP(OPI_OPC_DIV, /)
      case OPI_OPC_MOD:
      {
        opi_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        opi_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        if (opi_unlikely(lhs->type != opi_num_type || rhs->type != opi_num_type))
          r[OPI_BINOP_REG_OUT(ip)] = opi_undefined(opi_symbol("type-error"));
        else
          r[OPI_BINOP_REG_OUT(ip)] = opi_num_new(fmodl(opi_num_get_value(lhs), opi_num_get_value(rhs)));
        break;
      }

#define NUM_CMPOP(opc, op)                                                                                          \
      case opc:                                                                                                     \
      {                                                                                                             \
        opi_t lhs = r[OPI_BINOP_REG_LHS(ip)];                                                                       \
        opi_t rhs = r[OPI_BINOP_REG_RHS(ip)];                                                                       \
        if (opi_unlikely(lhs->type != opi_num_type || rhs->type != opi_num_type))                             \
          r[OPI_BINOP_REG_OUT(ip)] = opi_undefined(opi_symbol("type-error"));                                       \
        else                                                                                                        \
          r[OPI_BINOP_REG_OUT(ip)] = opi_num_get_value(lhs) op opi_num_get_value(rhs) ? opi_true : opi_false; \
        break;                                                                                                      \
      }
      NUM_CMPOP(OPI_OPC_NUMEQ, ==)
      NUM_CMPOP(OPI_OPC_NUMNE, !=)
      NUM_CMPOP(OPI_OPC_LT, <)
      NUM_CMPOP(OPI_OPC_GT, >)
      NUM_CMPOP(OPI_OPC_LE, <=)
      NUM_CMPOP(OPI_OPC_GE, >=)

      case OPI_OPC_CONS:
      {
        opi_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        opi_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        r[OPI_BINOP_REG_OUT(ip)] = opi_cons(lhs, rhs);
        break;
      }

      case OPI_OPC_FORCE:
      {
        abort();
        /*opi_t g = r[OPI_UNOP_REG_IN(ip)];*/
        /*if (g->type != opi_gen_type) {*/
          /*r[OPI_UNOP_REG_OUT(ip)] = g;*/
          /*break;*/
        /*}*/

      /*drain:;*/
        /*opi_t ret = opi_gen_drain_value(g);*/
        /*if (ret) {*/
          /*opi_dec_rc(ret);*/
          /*r[OPI_UNOP_REG_OUT(ip)] = ret;*/
          /*break;*/
        /*}*/

        /*if (opi_gen_is_done(g)) {*/
          /*r[OPI_UNOP_REG_OUT(ip)] = opi_undefined(opi_symbol("End"));*/
          /*break;*/
        /*}*/

        /*opi_gen_next(g);*/
        /*goto drain;*/
      }

      case OPI_OPC_PHI:
        r[OPI_PHI_REG(ip)] = opi_nil;
        break;

      case OPI_OPC_END:
        opi_assert(!"unexpected end");
        abort();

      case OPI_OPC_TEST:
      {
        uintptr_t test = (uintptr_t)(r[OPI_TEST_REG_IN(ip)] != opi_false);
        r[OPI_TEST_REG_OUT(ip)] = (void*)test;
        break;
      }

      case OPI_OPC_GUARD:
        opi_assert(r[OPI_GUARD_REG(ip)]);
        break;

      case OPI_OPC_CONST:
        r[OPI_CONST_REG_OUT(ip)] = OPI_CONST_ARG_CELL(ip);
        break;

      case OPI_OPC_APPLY:
      {
        opi_t fn = r[OPI_APPLY_REG_FN(ip)];
        size_t nargs = OPI_APPLY_ARG_NARGS(ip);
        if (opi_unlikely(fn->type != opi_fn_type)) {
          while (nargs--)
            opi_drop(opi_pop());
          r[OPI_APPLY_REG_OUT(ip)] = opi_undefined(opi_symbol("type-error"));
          break;
        }
        r[OPI_APPLY_REG_OUT(ip)] = opi_apply(fn, nargs);
        break;
      }

      case OPI_OPC_APPLYTC:
      {
        opi_t fn = r[OPI_APPLY_REG_FN(ip)];
        size_t nargs = OPI_APPLY_ARG_NARGS(ip);
        if (opi_unlikely(fn->type != opi_fn_type)) {
          while (nargs--)
            opi_drop(opi_pop());
          r[OPI_APPLY_REG_OUT(ip)] = opi_undefined(opi_symbol("type-error"));
          break;
        }
        if (opi_is_lambda(fn) & opi_test_arity(opi_fn_get_arity(fn), nargs)) {
          // Tail Call
          OpiLambda *lam = opi_fn_get_data(fn);
          opi_current_fn = fn;
          opi_inc_rc(fn);
          if (state->this_fn)
            opi_unref(state->this_fn);
          state->this_fn = fn;
          bc = lam->bc;
          ip = bc->tape;
          if (bc->nvals > opi_get_pool_size(r))
            r = state->reg = opi_realloc_pool(r, bc->nvals);
          continue;
        } else {
          // Fall back to default APPLY
          r[OPI_APPLY_REG_OUT(ip)] = opi_apply(fn, nargs);
        }
        break;
      }

      case OPI_OPC_RET:
      {
        opi_t ret = r[OPI_RET_REG_VAL(ip)];
        opi_release_pool(r);
        state->reg = NULL; // to nitifiy about regular return
        return ret;
      }

      case OPI_OPC_YIELD:
      {
        // NOTE: state->{reg & this_fn} are updated on tail-call
        state->ip = ip + 1;
        state->bc = bc;
        return r[OPI_YIELD_REG_RET(ip)];
      }

      case OPI_OPC_PUSH:
        opi_push(r[OPI_PUSH_REG_VAL(ip)]);
        break;

      case OPI_OPC_POP:
        opi_sp -= OPI_POP_ARG_N(ip);
        break;

      case OPI_OPC_LDCAP:
      {
        OpiLambda *data = opi_fn_get_data(opi_current_fn);
        r[OPI_LDCAP_REG_OUT(ip)] = data->caps[OPI_LDCAP_ARG_IDX(ip)];
        break;
      }

      case OPI_OPC_PARAM:
        r[OPI_PARAM_REG_OUT(ip)] = opi_get(OPI_PARAM_ARG_OFFS(ip));
        break;

      case OPI_OPC_ALCFN:
        r[OPI_ALCFN_REG_OUT(ip)] = opi_fn_alloc();
        break;

      case OPI_OPC_FINFN:
      {
        OpiFnInsnData *data = OPI_FINFN_ARG_DATA(ip);
        size_t ncaps = data->ncaps;
        OpiLambda *lam = opi_lambda_allocate(ncaps);
        lam->bc = data->bc;
        lam->ncaps = ncaps;
        for (size_t i = 0; i < ncaps; ++i)
          opi_inc_rc(lam->caps[i] = r[data->caps[i]]);

        opi_t fn = r[OPI_FINFN_REG_CELL(ip)];
        opi_fn_finalize(fn, NULL, opi_lambda_fn, data->arity);
        opi_fn_set_data(fn, lam, opi_lambda_delete);

        if ((lam->scp = scp))
          opi_rec_scope_set(scp, scpcnt++, (void*)fn, (void*)opi_lam_destroy, (void*)opi_lam_free);

        break;
      }

      case OPI_OPC_INCRC:
        opi_inc_rc(r[OPI_INCRC_REG_CELL(ip)]);
        break;

      case OPI_OPC_DECRC:
        opi_dec_rc(r[OPI_DECRC_REG_CELL(ip)]);
        break;

      case OPI_OPC_DROP:
        opi_drop(r[OPI_DROP_REG_CELL(ip)]);
        break;

      case OPI_OPC_UNREF:
        opi_unref(r[OPI_UNREF_REG_CELL(ip)]);
        break;

      case OPI_OPC_IF:
      {
        if (!r[OPI_IF_REG_TEST(ip)]) {
          ip = OPI_IF_ARG_ELSE(ip);
          continue;
        }
        break;
      }

      case OPI_OPC_JMP:
        ip = OPI_JMP_ARG_TO(ip);
        continue;

      case OPI_OPC_DUP:
        r[OPI_DUP_REG_OUT(ip)] = r[OPI_DUP_REG_IN(ip)];
        break;

      case OPI_OPC_BEGSCP:
        scp = opi_rec_scope(OPI_BEGSCP_ARG_N(ip));
        scpcnt = 0;
        break;

      case OPI_OPC_ENDSCP:
        opi_assert(scpcnt == scp->nrefs);
        opi_rec_scope_finalize(scp);
        scp = NULL;
        break;

      case OPI_OPC_TESTTY:
        r[OPI_TESTTY_REG_OUT(ip)] =
          (void*)(uintptr_t)(r[OPI_TESTTY_REG_CELL(ip)]->type == OPI_TESTTY_ARG_TYPE(ip));
        break;

      case OPI_OPC_LDFLD:
      {
        char *ptr = (char*)r[OPI_LDFLD_REG_CELL(ip)];
        size_t offs = OPI_LDFLD_ARG_OFFS(ip);
        r[OPI_LDFLD_REG_OUT(ip)] = *(opi_t*)(ptr + offs);
        break;
      }

      case OPI_OPC_BINOP_START:
      case OPI_OPC_BINOP_END:
        abort();
    }

    ip += 1;
  }
}

opi_t
opi_vm_continue(OpiState *state)
{
  return vm(state);
}

opi_t
opi_vm(OpiBytecode *bc)
{
  if (opi_current_fn)
    opi_inc_rc(opi_current_fn);

  OpiState state = {
    .this_fn = opi_current_fn,
    .ip = bc->tape,
    .bc = bc,
    .reg = opi_request_pool(bc->nvals),
  };

  opi_t ret = vm(&state);

  if (state.this_fn)
    opi_dec_rc(state.this_fn);

  if (state.reg) {
    // yield => return generator
    opi_assert(state.this_fn);
    OpiState *s = opi_state_new(state.this_fn, state.bc, state.ip, state.reg);
    return opi_gen_new(ret, s);
  } else {
    // return
    return ret;
  }
}

