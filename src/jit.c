#define OPI_USE_LIBJIT 0
#if OPI_USE_LIBJIT

#include "opium/opium.h"
#include "jit/jit.h"

typedef struct OpiJIT_s {
  jit_function_t func;
} OpiJIT;

typedef struct Block_s {
  jit_label_t start, end;
} Block;

static inline jit_value_t
load_type(jit_function_t func, jit_value_t x)
{
  int offset = offsetof(OpiHeader, type);
  return jit_insn_load_relative(func, x, offset, jit_type_void_ptr);
}

static inline jit_value_t
type_constant(jit_function_t func, opi_type_t type)
{
  jit_constant_t c = {
    .type = jit_type_void_ptr,
    .un.ptr_value = type,
  };
  return jit_value_create_constant(func, &c);
}

static inline jit_value_t
type_is(jit_function_t func, jit_value_t ty1_val, opi_type_t ty2)
{
  jit_value_t ty2_val = type_constant(func, ty2);
  return jit_insn_eq(func, ty1_val, ty2_val);
}

static inline jit_value_t
test_type(jit_function_t func, jit_value_t x, opi_type_t type)
{
  return type_is(func, load_type(func, x), type);
}

static inline jit_value_t
const_ptr(jit_function_t func, void *ptr)
{
  jit_constant_t c = {
    .type = jit_type_void_ptr,
    .un.ptr_value = ptr,
  };
  return jit_value_create_constant(func, &c);
}

static inline jit_value_t
load_num(jit_function_t func, jit_value_t x)
{
  int offset = offsetof(OpiNum, val);
  return jit_insn_load_relative(func, x, offset, jit_type_nfloat);
}

static inline jit_value_t
new_num(jit_function_t func, jit_value_t val)
{
  jit_type_t param[] = { jit_type_nfloat };
  jit_type_t sig =
    jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, param, 1, 0);
  jit_value_t ret = jit_insn_call_native(func, 0, opi_num_new, sig, &val, 1, 0);
  jit_type_free(sig);
  return ret;
}

static inline jit_value_t
cons(jit_function_t func, jit_value_t car, jit_value_t cdr)
{
  jit_type_t param[] = { jit_type_void_ptr, jit_type_void_ptr };
  jit_type_t sig =
    jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, param, 2, 0);
  jit_value_t arg[] = { car, cdr };
  jit_value_t ret = jit_insn_call_native(func, 0, opi_cons, sig, arg, 2, 0);
  jit_type_free(sig);
  return ret;
}

static inline jit_value_t
boolean(jit_function_t func, jit_value_t x)
{
  static opi_t false_true[2];
  false_true[0] = opi_false;
  false_true[1] = opi_true;
  jit_constant_t c = {
    .type = jit_type_void_ptr,
    .un.ptr_value = false_true,
  };
  jit_value_t tf = jit_value_create_constant(func, &c);
  return jit_insn_load_elem(func, tf, x, jit_type_void_ptr);
}

static inline void
guard(jit_function_t func)
{
  jit_type_t sig =
    jit_type_create_signature(jit_abi_cdecl, jit_type_void, 0, 0, 0);
  jit_insn_call_native(func, "abort", abort, sig, 0, 0, 0);
  jit_type_free(sig);
}

static int
jit(OpiJIT *jit, OpiBytecode *bc)
{
  jit_value_t r[bc->nvals];
  OpiInsn *ip = bc->head;

  cod_vec(Block) tail_blocks;
  cod_vec_init(tail_blocks);

  jit_function_t func = jit->func;

  while (1) {
    switch (ip->opc) {
      case OPI_OPC_END:
        goto end;
        
      case OPI_OPC_NOP:
        break;

      case OPI_OPC_VAR:
        r[OPI_VAR_REG(ip)] = jit_value_create(func, jit_type_int);
        break;

      case OPI_OPC_SET:
      {
        jit_value_t dest = r[OPI_SET_REG(ip)];
        jit_constant_t c = {
          .type = jit_type_int,
          .un.int_value = OPI_SET_ARG_VAL(ip),
        };
        jit_value_t val = jit_value_create_constant(func, &c);
        jit_insn_store(func, dest, val);
        break;
      }

      case OPI_OPC_AND:
      {
        jit_value_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        jit_value_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        jit_value_t ret = jit_insn_and(func, lhs, rhs);
        r[OPI_AND_REG_OUT(ip)] = ret;
        break;
      }

      case OPI_OPC_ADD:
      {
        jit_value_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        jit_value_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        jit_value_t lhs_val = load_num(func, lhs);
        jit_value_t rhs_val = load_num(func, rhs);
        jit_value_t ret_val = jit_insn_add(func, lhs_val, rhs_val);
        r[OPI_BINOP_REG_OUT(ip)] = new_num(func, ret_val);
        break;
      }

      case OPI_OPC_SUB:
      {
        jit_value_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        jit_value_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        jit_value_t lhs_val = load_num(func, lhs);
        jit_value_t rhs_val = load_num(func, rhs);
        jit_value_t ret_val = jit_insn_sub(func, lhs_val, rhs_val);
        r[OPI_BINOP_REG_OUT(ip)] = new_num(func, ret_val);
        break;
      }

      case OPI_OPC_MUL:
      {
        jit_value_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        jit_value_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        jit_value_t lhs_val = load_num(func, lhs);
        jit_value_t rhs_val = load_num(func, rhs);
        jit_value_t ret_val = jit_insn_mul(func, lhs_val, rhs_val);
        r[OPI_BINOP_REG_OUT(ip)] = new_num(func, ret_val);
        break;
      }

      case OPI_OPC_DIV:
      {
        jit_value_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        jit_value_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        jit_value_t lhs_val = load_num(func, lhs);
        jit_value_t rhs_val = load_num(func, rhs);
        jit_value_t ret_val = jit_insn_div(func, lhs_val, rhs_val);
        r[OPI_BINOP_REG_OUT(ip)] = new_num(func, ret_val);
        break;
      }

      case OPI_OPC_MOD:
      {
        opi_error("unimplemented MOD\n");
        abort();
      }

      case OPI_OPC_NUMEQ:
      {
        jit_value_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        jit_value_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        jit_value_t lhs_val = load_num(func, lhs);
        jit_value_t rhs_val = load_num(func, rhs);
        jit_value_t ret_val = jit_insn_eq(func, lhs_val, rhs_val);
        r[OPI_BINOP_REG_OUT(ip)] = boolean(func, ret_val);
        break;
      }

      case OPI_OPC_NUMNE:
      {
        jit_value_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        jit_value_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        jit_value_t lhs_val = load_num(func, lhs);
        jit_value_t rhs_val = load_num(func, rhs);
        jit_value_t ret_val = jit_insn_ne(func, lhs_val, rhs_val);
        r[OPI_BINOP_REG_OUT(ip)] = boolean(func, ret_val);
        break;
      }

      case OPI_OPC_LT:
      {
        jit_value_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        jit_value_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        jit_value_t lhs_val = load_num(func, lhs);
        jit_value_t rhs_val = load_num(func, rhs);
        jit_value_t ret_val = jit_insn_lt(func, lhs_val, rhs_val);
        r[OPI_BINOP_REG_OUT(ip)] = boolean(func, ret_val);
        break;
      }

      case OPI_OPC_GT:
      {
        jit_value_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        jit_value_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        jit_value_t lhs_val = load_num(func, lhs);
        jit_value_t rhs_val = load_num(func, rhs);
        jit_value_t ret_val = jit_insn_gt(func, lhs_val, rhs_val);
        r[OPI_BINOP_REG_OUT(ip)] = boolean(func, ret_val);
        break;
      }

      case OPI_OPC_LE:
      {
        jit_value_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        jit_value_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        jit_value_t lhs_val = load_num(func, lhs);
        jit_value_t rhs_val = load_num(func, rhs);
        jit_value_t ret_val = jit_insn_le(func, lhs_val, rhs_val);
        r[OPI_BINOP_REG_OUT(ip)] = boolean(func, ret_val);
        break;
      }

      case OPI_OPC_GE:
      {
        jit_value_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        jit_value_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        jit_value_t lhs_val = load_num(func, lhs);
        jit_value_t rhs_val = load_num(func, rhs);
        jit_value_t ret_val = jit_insn_ge(func, lhs_val, rhs_val);
        r[OPI_BINOP_REG_OUT(ip)] = boolean(func, ret_val);
        break;
      }

      case OPI_OPC_CONS:
      {
        jit_value_t lhs = r[OPI_BINOP_REG_LHS(ip)];
        jit_value_t rhs = r[OPI_BINOP_REG_RHS(ip)];
        r[OPI_BINOP_REG_OUT(ip)] = cons(func, lhs, rhs);
        break;
      }

      case OPI_OPC_PHI:
      {
        jit_value_t phi = jit_value_create(func, jit_type_void_ptr);
        jit_insn_store(func, phi, const_ptr(func, opi_nil));
        r[OPI_PHI_REG(ip)] = phi;
        break;
      }

      case OPI_OPC_TEST:
      {
        jit_value_t x = r[OPI_TEST_REG_IN(ip)];
        jit_value_t test = jit_insn_ne(func, x, const_ptr(func, opi_false));
        r[OPI_TEST_REG_OUT(ip)] = test;
        break;
      }

      case OPI_OPC_GUARD:
      {
        jit_value_t x = r[OPI_GUARD_REG(ip)];
        jit_value_t test = jit_insn_eq(func, x, const_ptr(func, opi_false));
        jit_label_t label = jit_label_undefined;
        jit_insn_branch_if_not(func, test, &label);
        guard(func);
        jit_insn_label(func, &label);
        break;
      }

      case OPI_OPC_CONST:
      {
        opi_t x = OPI_CONST_ARG_CELL(ip);
        r[OPI_CONST_REG_OUT(ip)] = const_ptr(func, x);
        break;
      }

      case OPI_OPC_APPLY:
      {
        jit_value_t fn = r[OPI_APPLY_REG_FN(ip)];
        size_t nargs = OPI_APPLY_ARG_NARGS(ip);
        r[OPI_APPLY_REG_OUT(ip)] = opi_apply(fn, nargs);
        break;
      }


    }

    ip = ip->next;
  }
end:;
}

#endif/* OPI_USE_LIBJIT */
