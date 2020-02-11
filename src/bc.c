#include "opium/opium.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

OpiBytecode*
opi_bytecode()
{
  OpiBytecode *bc = malloc(sizeof(OpiBytecode));

  bc->nvals = 0;
  bc->vinfo_cap = 0x10;
  bc->nvals = 0;
  bc->vinfo = malloc(sizeof(OpiValInfo) * bc->vinfo_cap);

  bc->head = malloc(sizeof(OpiInsn));
  bc->head->opc = OPI_OPC_NOP;
  bc->tail = malloc(sizeof(OpiInsn));
  bc->tail->opc = OPI_OPC_END;
  opi_insn_chain(bc->head, bc->tail, NULL);
  opi_insn_chain(NULL, bc->head, bc->tail);

  bc->point = bc->tail;

  bc->tape = NULL;
  bc->is_generator = FALSE;

  return bc;
}

void
opi_bytecode_delete(OpiBytecode *bc)
{
  opi_insn_delete(bc->head);
  free(bc->vinfo);
  if (bc->tape)
    free(bc->tape);
  free(bc);
}

OpiInsn*
opi_bytecode_drain(OpiBytecode *bc)
{
  OpiInsn *start = bc->head->next,
                  *end = bc->tail->prev;
  start->prev = NULL;
  end->next = NULL;
  opi_insn_chain(bc->head, bc->tail, NULL);
  return start;
}

int
opi_bytecode_new_val(OpiBytecode *bc, OpiValType vtype)
{
  if (bc->vinfo_cap == bc->nvals) {
    bc->vinfo_cap <<= 1;
    bc->vinfo = realloc(bc->vinfo, sizeof(OpiValInfo) * bc->vinfo_cap);
  }

  bc->vinfo[bc->nvals].type = vtype;
  bc->vinfo[bc->nvals].c = NULL;
  bc->vinfo[bc->nvals].creatat = NULL;
  bc->vinfo[bc->nvals].is_var = FALSE;

  return bc->nvals++;
}

void
opi_bytecode_append(OpiBytecode *bc, OpiInsn *insn)
{ opi_insn_chain(bc->tail->prev, insn, bc->tail); }

void
opi_bytecode_prepend(OpiBytecode *bc, OpiInsn *insn)
{ opi_insn_chain(bc->head, insn, bc->head->next); }

void
opi_bytecode_write(OpiBytecode *bc, OpiInsn *insn)
{ opi_insn_chain(bc->point->prev, insn, bc->point); }

void
opi_insn_delete1(OpiInsn *insn)
{
  switch (insn->opc) {
    case OPI_OPC_ENDSCP:
      free(OPI_ENDSCP_ARG_CELLS(insn));
      break;

    case OPI_OPC_CONST:
      opi_unref(insn->ptr[1]);
      break;

    case OPI_OPC_FINFN:
      opi_bytecode_delete(OPI_FINFN_ARG_DATA(insn)->bc);
      opi_ir_unref(OPI_FINFN_ARG_DATA(insn)->ir);
      free(OPI_FINFN_ARG_DATA(insn));

    default:
      break;
  }

  free(insn);
}

void
opi_insn_delete(OpiInsn *insn)
{
  if (insn) {
    OpiInsn *next = insn->next;
    opi_insn_delete1(insn);
    opi_insn_delete(next);
  }
}

static
int dump_padding = 0;

static void
dump_pad(FILE *out)
{
  for (int i = 0; i < dump_padding; ++i)
    putc(' ', out);
}

void
opi_insn_dump1(OpiInsn *insn, FILE *out)
{
  switch (insn->opc) {
    case OPI_OPC_NOP:
      fprintf(out, "NOP <%p>", insn);
      break;

    case OPI_OPC_END:
      fprintf(out, "END");
      break;

    case OPI_OPC_APPLY:
      fprintf(out, "%%%zd = apply/%zd %%%zd",
          OPI_APPLY_REG_OUT(insn),
          OPI_APPLY_ARG_NARGS(insn),
          OPI_APPLY_REG_FN(insn));
      break;

    case OPI_OPC_APPLYTC:
      fprintf(out, "%%%zd = applytc/%zd %%%zd",
          OPI_APPLY_REG_OUT(insn),
          OPI_APPLY_ARG_NARGS(insn),
          OPI_APPLY_REG_FN(insn));
      break;

    case OPI_OPC_RET:
      fprintf(out, "return %%%zd", OPI_RET_REG_VAL(insn));
      break;

    case OPI_OPC_PUSH:
      fprintf(out, "push %%%zd", OPI_PUSH_REG_VAL(insn));
      break;

    case OPI_OPC_POP:
      fprintf(out, "pop %zd", OPI_POP_ARG_N(insn));
      break;

    case OPI_OPC_INCRC:
      fprintf(out, "incrc %%%zd", OPI_INCRC_REG_CELL(insn));
      break;

    case OPI_OPC_DECRC:
      fprintf(out, "decrc %%%zd", OPI_DECRC_REG_CELL(insn));
      break;

    case OPI_OPC_DROP:
      fprintf(out, "drop %%%zd", OPI_DROP_REG_CELL(insn));
      break;

    case OPI_OPC_UNREF:
      fprintf(out, "unref %%%zd", OPI_UNREF_REG_CELL(insn));
      break;

    case OPI_OPC_CONST:
      fprintf(out, "%%%zd = ", OPI_CONST_REG_OUT(insn));
      opi_display(OPI_CONST_ARG_CELL(insn), out);
      break;

    case OPI_OPC_LDCAP:
      fprintf(out, "%%%zd = ldcap %zd",
          OPI_LDCAP_REG_OUT(insn),
          OPI_LDCAP_ARG_IDX(insn));
      break;

    case OPI_OPC_PARAM:
      fprintf(out, "%%%zd = param %zd",
          OPI_PARAM_REG_OUT(insn),
          OPI_PARAM_ARG_OFFS(insn));
      break;

    case OPI_OPC_ALCFN:
      fprintf(out, "%%%zu = alcfn", OPI_ALCFN_REG_OUT(insn));
      break;

    case OPI_OPC_FINFN:
    {
      OpiFnInsnData *data = OPI_FINFN_ARG_DATA(insn);
      fprintf(out, "finfn/%d %%%zu [ ", data->arity, OPI_FINFN_REG_CELL(insn));
      for (int i = 0; i < data->ncaps; ++i)
        fprintf(out, "%%%d ", data->caps[i]);
      fprintf(out, "] {\n");
      dump_padding += 2;
      OpiBytecode *body = OPI_FINFN_ARG_DATA(insn)->bc;
      opi_insn_dump(body->head, out);
      dump_padding -= 2;
      dump_pad(out);
      fprintf(out, "}");
      break;
    }

    case OPI_OPC_IF:
      fprintf(out, "if %%%zd else %p",
          OPI_IF_REG_TEST(insn),
          OPI_IF_ARG_ELSE(insn));
      break;

    case OPI_OPC_JMP:
      fprintf(out, "jmp %p", OPI_JMP_ARG_TO(insn));
      break;

    case OPI_OPC_PHI:
      fprintf(out, "phi %%%zu", OPI_PHI_REG(insn));
      break;

    case OPI_OPC_DUP:
      fprintf(out, "%%%zu = %%%zu",
          OPI_DUP_REG_OUT(insn),
          OPI_DUP_REG_IN(insn));
      break;

    case OPI_OPC_BEGSCP:
      fprintf(out, "begscp %zu", OPI_BEGSCP_ARG_N(insn));
      break;

    case OPI_OPC_ENDSCP:
    {
      int *cells = OPI_ENDSCP_ARG_CELLS(insn);
      size_t n = OPI_ENDSCP_ARG_NCELLS(insn);
      fprintf(out, "endscp [ ");
      for (size_t i = 0; i < n; ++i)
        fprintf(out, "%%%d ", cells[i]);
      fprintf(out, "]");
      break;
    }

    case OPI_OPC_TESTTY:
      fprintf(out, "%%%zu = testty %%%zu <%s>",
          OPI_TESTTY_REG_OUT(insn),
          OPI_TESTTY_REG_CELL(insn),
          opi_type_get_name(OPI_TESTTY_ARG_TYPE(insn)));
      break;

    case OPI_OPC_TEST:
      fprintf(out, "%%%zu = test %%%zu",
          OPI_TEST_REG_OUT(insn),
          OPI_TEST_REG_IN(insn));
      break;

    case OPI_OPC_LDFLD:
      fprintf(out, "%%%zu = ldfld %%%zu +%zx",
          OPI_LDFLD_REG_OUT(insn),
          OPI_LDFLD_REG_CELL(insn),
          OPI_LDFLD_ARG_OFFS(insn));
      break;

    case OPI_OPC_GUARD:
      fprintf(out, "guard %%%zu", OPI_GUARD_REG(insn));
      break;

    case OPI_OPC_BINOP_START ... OPI_OPC_BINOP_END:
      fprintf(out, "%%%zu = binop <%d> %%%zu %%%zu",
          OPI_BINOP_REG_OUT(insn),
          (int)insn->opc,
          OPI_BINOP_REG_LHS(insn),
          OPI_BINOP_REG_RHS(insn));
      break;

    default:
      opi_assert(!"unimplemented insn dump");
  }
}

void
opi_insn_dump(OpiInsn *insn, FILE *out)
{
  if (insn) {
    if (insn->opc == OPI_OPC_END) {
      // skip
    } else {
      dump_pad(out);
      opi_insn_dump1(insn, out);
      putc('\n', out);
    }
    opi_insn_dump(insn->next, out);
  }
}

OpiInsn*
opi_insn_const(int ret, opi_t cell)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_CONST;
  OPI_CONST_REG_OUT(insn) = ret;
  OPI_CONST_ARG_CELL(insn) = cell;
  opi_inc_rc(cell);
  return insn;
}

OpiInsn*
opi_insn_apply(int ret, int fn, size_t nargs, int tc)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = tc ? OPI_OPC_APPLYTC : OPI_OPC_APPLY;
  OPI_APPLY_REG_OUT(insn) = ret;
  OPI_APPLY_REG_FN(insn) = fn;
  OPI_APPLY_ARG_NARGS(insn) = nargs;
  return insn;
}

OpiInsn*
opi_insn_applyi(int ret, int fn, size_t nargs)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_APPLYI;
  OPI_APPLY_REG_OUT(insn) = ret;
  OPI_APPLY_REG_FN(insn) = fn;
  OPI_APPLY_ARG_NARGS(insn) = nargs;
  return insn;
}

OpiInsn*
opi_insn_ret(int val)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_RET;
  OPI_RET_REG_VAL(insn) = val;
  return insn;
}

OpiInsn*
opi_insn_push(int val)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_PUSH;
  OPI_PUSH_REG_VAL(insn) = val;
  return insn;
}

OpiInsn*
opi_insn_pop(size_t n)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_POP;
  OPI_POP_ARG_N(insn) = n;
  return insn;
}

OpiInsn*
opi_insn_incrc(int cell)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_INCRC;
  OPI_INCRC_REG_CELL(insn) = cell;
  return insn;
}

OpiInsn*
opi_insn_decrc(int cell)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_DECRC;
  OPI_DECRC_REG_CELL(insn) = cell;
  return insn;
}

OpiInsn*
opi_insn_drop(int cell)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_DROP;
  OPI_DROP_REG_CELL(insn) = cell;
  return insn;
}

OpiInsn*
opi_insn_unref(int cell)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_UNREF;
  OPI_UNREF_REG_CELL(insn) = cell;
  return insn;
}

OpiInsn*
opi_insn_ldcap(int out, int idx)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_LDCAP;
  OPI_LDCAP_REG_OUT(insn) = out;
  OPI_LDCAP_ARG_IDX(insn) = idx;
  return insn;
}

OpiInsn*
opi_insn_param(int out, int offs)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_PARAM;
  OPI_PARAM_REG_OUT(insn) = out;
  OPI_PARAM_ARG_OFFS(insn) = offs;
  return insn;
}

OpiInsn*
opi_insn_alcfn(int out)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_ALCFN;
  OPI_ALCFN_REG_OUT(insn) = out;
  return insn;
}

OpiInsn*
opi_insn_finfn(int cell, int arity, OpiBytecode *bc, OpiIr *ir, int *cap, size_t ncap)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_FINFN;
  OPI_FINFN_REG_CELL(insn) = cell;
  OpiFnInsnData *data =
    malloc(sizeof(OpiFnInsnData) + sizeof(int) * ncap);
  memcpy(data->caps, cap, sizeof(int) * ncap);
  data->bc = bc;
  data->ir = ir;
  opi_ir_ref(ir);
  data->ncaps = ncap;
  data->arity = arity;
  insn->ptr[1] = data;
  return insn;
}

OpiInsn*
opi_insn_test(int out, int in)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_TEST;
  OPI_TEST_REG_OUT(insn) = out;
  OPI_TEST_REG_IN(insn) = in;
  return insn;
}

OpiInsn*
opi_insn_if(int test)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_IF;
  OPI_IF_REG_TEST(insn) = test;
  insn->ptr[1] = NULL;
  insn->reg[2] = 0;
  return insn;
}

OpiInsn*
opi_insn_nop()
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_NOP;
  return insn;
}

OpiInsn*
opi_insn_jmp(OpiInsn *to)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_JMP;
  insn->ptr[0] = to;
  return insn;
}

OpiInsn*
opi_insn_phi(int reg)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_PHI;
  OPI_PHI_REG(insn) = reg;
  return insn;
}

OpiInsn*
opi_insn_dup(int out, int in)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_DUP;
  OPI_DUP_REG_OUT(insn) = out;
  OPI_DUP_REG_IN(insn)  = in;
  return insn;
}

OpiInsn*
opi_insn_begscp(size_t n)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_BEGSCP;
  OPI_BEGSCP_ARG_N(insn) = n;
  return insn;
}

OpiInsn*
opi_insn_endscp(int *cells, size_t n)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_ENDSCP;
  insn->reg[0] = n;
  insn->ptr[1] = malloc(sizeof(int) * n);
  memcpy(insn->ptr[1], cells, sizeof(int) * n);
  return insn;
}

OpiInsn*
opi_insn_testty(int out, int cell, opi_type_t type)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_TESTTY;
  OPI_TESTTY_REG_OUT(insn) = out;
  OPI_TESTTY_REG_CELL(insn) = cell;
  OPI_TESTTY_ARG_TYPE(insn) = type;
  return insn;
}

OpiInsn*
opi_insn_ldfld(int out, int cell, size_t offs)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_LDFLD;
  OPI_LDFLD_REG_OUT(insn) = out;
  OPI_LDFLD_REG_CELL(insn) = cell;
  OPI_LDFLD_ARG_OFFS(insn) = offs;
  return insn;
}

OpiInsn*
opi_insn_guard(int in)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_GUARD;
  OPI_GUARD_REG(insn) = in;
  return insn;
}

OpiInsn*
opi_insn_binop(OpiOpc opc, int out, int lhs, int rhs)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = opc;
  OPI_BINOP_REG_OUT(insn) = out;
  OPI_BINOP_REG_LHS(insn) = lhs;
  OPI_BINOP_REG_RHS(insn) = rhs;
  return insn;
}

OpiInsn*
opi_insn_var(int reg)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_VAR;
  OPI_VAR_REG(insn) = reg;
  return insn;
}

OpiInsn*
opi_insn_set(int reg, int val)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_SET;
  OPI_SET_REG(insn) = reg;
  OPI_SET_ARG_VAL(insn) = val;
  return insn;
}

OpiInsn*
opi_insn_setvar(int var, int val)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_SETVAR;
  OPI_SETVAR_REG_REF(insn) = var;
  OPI_SETVAR_REG_VAL(insn) = val;
  return insn;
}

OpiInsn*
opi_insn_deref(int out, int var)
{
  OpiInsn *insn = malloc(sizeof(OpiInsn));
  insn->opc = OPI_OPC_DEREF;
  OPI_DEREF_REG_OUT(insn) = out;
  OPI_DEREF_REG_VAR(insn) = var;
  return insn;
}

int
opi_insn_is_using(OpiInsn *insn, int vid)
{
  switch (insn->opc) {
    case OPI_OPC_NOP:
    case OPI_OPC_END:
    case OPI_OPC_CONST:
    case OPI_OPC_LDCAP:
    case OPI_OPC_PARAM:
    case OPI_OPC_POP:
    case OPI_OPC_JMP:
    case OPI_OPC_PHI:
    case OPI_OPC_ALCFN:
    case OPI_OPC_BEGSCP:
    case OPI_OPC_IF:
    case OPI_OPC_GUARD:
    case OPI_OPC_VAR:
    case OPI_OPC_SET:
      return FALSE;

    case OPI_OPC_BINOP_START ... OPI_OPC_BINOP_END:
      return (int)OPI_BINOP_REG_LHS(insn) == vid ||
             (int)OPI_BINOP_REG_RHS(insn) == vid;

    case OPI_OPC_SETVAR:
      return (int)OPI_SETVAR_REG_REF(insn) == vid ||
             (int)OPI_SETVAR_REG_VAL(insn) == vid;

    // ignore manual RC-management
    case OPI_OPC_INCRC:
    case OPI_OPC_DECRC:
    case OPI_OPC_DROP:
    case OPI_OPC_UNREF:
      return FALSE;

    case OPI_OPC_APPLY:
    case OPI_OPC_APPLYTC:
    case OPI_OPC_APPLYI:
      return (int)OPI_APPLY_REG_FN(insn) == vid;

    case OPI_OPC_RET:
      return (int)OPI_RET_REG_VAL(insn) == vid;

    case OPI_OPC_PUSH:
      return (int)OPI_PUSH_REG_VAL(insn) == vid;

    case OPI_OPC_FINFN:
    {
      if ((int)OPI_FINFN_REG_CELL(insn) == vid)
        return TRUE;

      OpiFnInsnData *data = OPI_FINFN_ARG_DATA(insn);
      for (int i = 0; i < data->ncaps; ++i) {
        if (data->caps[i] == vid)
          return TRUE;
      }
      return FALSE;
    }

    case OPI_OPC_DUP:
      return (int)OPI_DUP_REG_OUT(insn) == vid ||
             (int)OPI_DUP_REG_IN(insn)  == vid;

    case OPI_OPC_ENDSCP:
      for (size_t i = 0; i < OPI_ENDSCP_ARG_NCELLS(insn); ++i) {
        if (((int*)OPI_ENDSCP_ARG_CELLS(insn))[i] == vid)
          return TRUE;
      }
      return FALSE;

    case OPI_OPC_TESTTY:
      return (int)OPI_TESTTY_REG_CELL(insn) == vid;

    case OPI_OPC_LDFLD:
      return (int)OPI_LDFLD_REG_CELL(insn) == vid;

    case OPI_OPC_TEST:
      return (int)OPI_TEST_REG_IN(insn) == vid;

    case OPI_OPC_DEREF:
      return (int)OPI_DEREF_REG_VAR(insn) == vid;
  }

  abort();
}

int
opi_insn_is_killing(OpiInsn *insn, int vid)
{
  switch (insn->opc) {
    case OPI_OPC_NOP:
    case OPI_OPC_END:
    case OPI_OPC_CONST:
    case OPI_OPC_LDCAP:
    case OPI_OPC_PARAM:
    case OPI_OPC_POP:
    case OPI_OPC_APPLY:
    case OPI_OPC_APPLYI:
    case OPI_OPC_APPLYTC:
    case OPI_OPC_IF:
    case OPI_OPC_JMP:
    case OPI_OPC_PHI:
    case OPI_OPC_ALCFN:
    case OPI_OPC_BEGSCP:
    case OPI_OPC_ENDSCP:
    case OPI_OPC_TESTTY:
    case OPI_OPC_LDFLD:
    case OPI_OPC_TEST:
    case OPI_OPC_GUARD:
    case OPI_OPC_BINOP_START ... OPI_OPC_BINOP_END:
    case OPI_OPC_VAR:
    case OPI_OPC_SET:
    case OPI_OPC_SETVAR:
    case OPI_OPC_DEREF:
      return FALSE;

    // ignore manual RC-management
    case OPI_OPC_INCRC:
    case OPI_OPC_DECRC:
    case OPI_OPC_DROP:
    case OPI_OPC_UNREF:
      return FALSE;

    case OPI_OPC_RET:
      return (int)OPI_RET_REG_VAL(insn) == vid;

    case OPI_OPC_PUSH:
      return (int)OPI_PUSH_REG_VAL(insn) == vid;

    case OPI_OPC_FINFN:
    {
      OpiFnInsnData *data = OPI_FINFN_ARG_DATA(insn);
      for (int i = 0; i < data->ncaps; ++i) {
        if (data->caps[i] == vid)
          return TRUE;
      }
      return FALSE;
    }

    case OPI_OPC_DUP:
      return (int)OPI_DUP_REG_IN(insn) == vid;
  }

  abort();
}

int
opi_insn_is_end(OpiInsn *insn)
{
  switch (insn->opc) {
    case OPI_OPC_RET:
    /*case OPI_OPC_APPLYTC:*/
      return TRUE;

    default:
      return FALSE;
  }

  abort();
}

int
opi_bytecode_const(OpiBytecode *bc, opi_t cell)
{
  int ret = opi_bytecode_new_val(bc, OPI_VAL_GLOBAL);
  bc->vinfo[ret].c = cell;
  OpiInsn *insn;
  opi_bytecode_write(bc, (insn = opi_insn_const(ret, cell)));
  bc->vinfo[ret].creatat = insn;
  return ret;
}

static int
bytecode_apply_va(OpiBytecode *bc, int tc, int fn, size_t nargs, va_list args)
{
  for (int i = nargs - 1; i >= 0; --i)
    opi_bytecode_push(bc, va_arg(args, int));

  int ret = opi_bytecode_new_val(bc, OPI_VAL_LOCAL);
  OpiInsn *insn;
  opi_bytecode_write(bc, (insn = opi_insn_apply(ret, fn, nargs, tc)));
  bc->vinfo[ret].creatat = insn;
  return ret;
}

int
opi_bytecode_apply(OpiBytecode *bc, int fn, size_t nargs, ...)
{
  va_list args;
  va_start(args, nargs);
  int ret = bytecode_apply_va(bc, FALSE, fn, nargs, args);
  va_end(args);
  return ret;
}

int
opi_bytecode_apply_tailcall(OpiBytecode *bc, int fn, size_t nargs, ...)
{
  va_list args;
  va_start(args, nargs);
  int ret = bytecode_apply_va(bc, TRUE, fn, nargs, args);
  va_end(args);
  return ret;
}

static int
bytecode_apply_arr(OpiBytecode *bc, int tc, int fn, size_t nargs, const int *args)
{
  for (int i = nargs - 1; i >= 0; --i)
    opi_bytecode_push(bc, args[i]);

  int ret = opi_bytecode_new_val(bc, OPI_VAL_LOCAL);
  OpiInsn *insn;
  opi_bytecode_write(bc, (insn = opi_insn_apply(ret, fn, nargs, tc)));
  bc->vinfo[ret].creatat = insn;
  return ret;
}

static int
bytecode_applyi_arr(OpiBytecode *bc, int fn, size_t nargs, const int *args)
{
  for (int i = nargs - 1; i >= 0; --i)
    opi_bytecode_push(bc, args[i]);

  int ret = opi_bytecode_new_val(bc, OPI_VAL_LOCAL);
  OpiInsn *insn;
  opi_bytecode_write(bc, (insn = opi_insn_applyi(ret, fn, nargs)));
  bc->vinfo[ret].creatat = insn;
  return ret;
}

int
opi_bytecode_apply_arr(OpiBytecode *bc, int fn, size_t nargs, const int *args)
{ return bytecode_apply_arr(bc, FALSE, fn, nargs, args); }

int
opi_bytecode_apply_tailcall_arr(OpiBytecode *bc, int fn, size_t nargs, const int *args)
{ return bytecode_apply_arr(bc, TRUE, fn, nargs, args); }

int
opi_bytecode_applyi_arr(OpiBytecode *bc, int fn, size_t nargs, const int *args)
{ return bytecode_applyi_arr(bc, fn, nargs, args); }

int
opi_bytecode_ldcap(OpiBytecode *bc, size_t idx)
{
  int ret = opi_bytecode_new_val(bc, OPI_VAL_GLOBAL);
  OpiInsn *insn;
  opi_bytecode_write(bc, (insn = opi_insn_ldcap(ret, idx)));
  bc->vinfo[ret].creatat = insn;
  return ret;
}

int
opi_bytecode_param(OpiBytecode *bc, size_t offs)
{
  int ret = opi_bytecode_new_val(bc, OPI_VAL_LOCAL);
  OpiInsn *insn;
  opi_bytecode_write(bc, (insn = opi_insn_param(ret, offs)));
  bc->vinfo[ret].creatat = insn;
  return ret;
}

void
opi_bytecode_finfn(OpiBytecode *bc, int cell, int arity, OpiBytecode *body,
    OpiIr *ir, int *cap, size_t ncap)
{ opi_bytecode_write(bc, opi_insn_finfn(cell, arity, body, ir, cap, ncap)); }

void
opi_bytecode_ret(OpiBytecode *bc, int val)
{ opi_bytecode_write(bc, opi_insn_ret(val)); }

void
opi_bytecode_push(OpiBytecode *bc, int val)
{ opi_bytecode_write(bc, opi_insn_push(val)); }

void
opi_bytecode_pop(OpiBytecode *bc, size_t n)
{ opi_bytecode_write(bc, opi_insn_pop(n)); }

int
opi_bytecode_test(OpiBytecode *bc, int in)
{
  int ret = opi_bytecode_new_val(bc, OPI_VAL_BOOL);
  opi_bytecode_write(bc, opi_insn_test(ret, in));
  return ret;
}

void
opi_bytecode_if(OpiBytecode *bc, int test, OpiIf *iff)
{
  opi_assert(bc->vinfo[test].type == OPI_VAL_BOOL);
  OpiInsn *insn = opi_insn_if(test);
  opi_bytecode_write(bc, insn);
  iff->iff = insn;
}

void
opi_bytecode_if_else(OpiBytecode *bc, OpiIf *iff)
{
  OpiInsn *else_label = opi_insn_nop();
  opi_bytecode_write(bc, else_label);
  iff->iff->ptr[1] = else_label;
  iff->els = else_label;
}

void
opi_bytecode_if_end(OpiBytecode *bc, OpiIf *iff)
{
  OpiInsn *end_label = opi_insn_nop();
  opi_bytecode_write(bc, end_label);

  OpiInsn *point_buf = bc->point;
  bc->point = iff->els;
  opi_bytecode_write(bc, opi_insn_jmp(end_label));
  bc->point = point_buf;
}

int
opi_bytecode_phi(OpiBytecode *bc)
{
  int ret = opi_bytecode_new_val(bc, OPI_VAL_PHI);
  OpiInsn *insn;
  opi_bytecode_write(bc, (insn = opi_insn_phi(ret)));
  bc->vinfo[ret].creatat = insn;
  return ret;
}

void
opi_bytecode_dup(OpiBytecode *bc, int dst, int src)
{
  opi_bytecode_write(bc, opi_insn_incrc(src));
  opi_bytecode_write(bc, opi_insn_dup(dst, src));
}

void
opi_bytecode_incrc(OpiBytecode *bc, int cell)
{ opi_bytecode_write(bc, opi_insn_incrc(cell)); }

void
opi_bytecode_decrc(OpiBytecode *bc, int cell)
{ opi_bytecode_write(bc, opi_insn_decrc(cell)); }

void
opi_bytecode_drop(OpiBytecode *bc, int cell)
{ opi_bytecode_write(bc, opi_insn_drop(cell)); }

void
opi_bytecode_unref(OpiBytecode *bc, int cell)
{ opi_bytecode_write(bc, opi_insn_unref(cell)); }

int
opi_bytecode_alcfn(OpiBytecode *bc, OpiValType valtype)
{
  int ret = opi_bytecode_new_val(bc, valtype);
  OpiInsn *insn;
  opi_bytecode_write(bc, (insn = opi_insn_alcfn(ret)));
  bc->vinfo[ret].creatat = insn;
  return ret;
}

void
opi_bytecode_begscp(OpiBytecode *bc, size_t n)
{ opi_bytecode_write(bc, opi_insn_begscp(n)); }

void
opi_bytecode_endscp(OpiBytecode *bc, int *cells, size_t n)
{ opi_bytecode_write(bc, opi_insn_endscp(cells, n)); }

int
opi_bytecode_testty(OpiBytecode *bc, int cell, opi_type_t type)
{
  int ret = opi_bytecode_new_val(bc, OPI_VAL_BOOL);
  opi_bytecode_write(bc, opi_insn_testty(ret, cell, type));
  return ret;
}

int
opi_bytecode_ldfld(OpiBytecode *bc, int cell, size_t offs)
{
  int ret = opi_bytecode_new_val(bc, OPI_VAL_LOCAL);
  OpiInsn *insn;
  opi_bytecode_write(bc, (insn = opi_insn_ldfld(ret, cell, offs)));
  bc->vinfo[ret].creatat = insn;
  return ret;
}

void
opi_bytecode_guard(OpiBytecode *bc, int cell)
{ opi_bytecode_write(bc, opi_insn_guard(cell)); }

int
opi_bytecode_binop(OpiBytecode *bc, OpiOpc opc, int lhs, int rhs)
{
  int out;
  switch (opc) {
    case OPI_OPC_NUMEQ ... OPI_OPC_GE:
      out = opi_bytecode_new_val(bc, OPI_VAL_GLOBAL);
      break;
    default:
      out = opi_bytecode_new_val(bc, OPI_VAL_LOCAL);
      break;
  }
  OpiInsn *insn;
  opi_bytecode_write(bc, (insn = opi_insn_binop(opc, out, lhs, rhs)));
  bc->vinfo[out].creatat = insn;
  return out;
}

int
opi_bytecode_var(OpiBytecode *bc)
{
  int reg = opi_bytecode_new_val(bc, OPI_VAL_BOOL);
  opi_bytecode_write(bc, opi_insn_var(reg));
  return reg;
}

void
opi_bytecode_set(OpiBytecode *bc, int reg, int val)
{ opi_bytecode_write(bc, opi_insn_set(reg, val)); }

void
opi_bytecode_setvar(OpiBytecode *bc, int ref, int val)
{
  assert(bc->vinfo[ref].is_var);
  opi_bytecode_write(bc, opi_insn_setvar(ref, val));
}

int
opi_bytecode_deref(OpiBytecode *bc, int var)
{
  assert(bc->vinfo[var].is_var);
  int out = opi_bytecode_new_val(bc, OPI_VAL_LOCAL);
  OpiInsn *insn = opi_insn_deref(out, var);
  opi_bytecode_write(bc, insn);
  bc->vinfo[out].creatat = insn;
  return out;
}

