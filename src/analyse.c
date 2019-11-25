#include "opium/opium.h"

#include <stdlib.h>
#include <string.h>

int
opi_bytecode_while(OpiBytecode *bc, int (*test)(OpiInsn *insn, void *data), void *data)
{
  for (OpiInsn *ip = bc->point; ip; ip = ip->next) {
    if (!test(ip, data))
      return TRUE;

    if (ip->opc == OPI_OPC_IF) {
      bc->point = ip->next;
      int then_ret = opi_bytecode_while(bc, test, data);

      bc->point = ((OpiInsn*)OPI_IF_ARG_ELSE(ip))->next;
      int else_ret = opi_bytecode_while(bc, test, data);

      return then_ret || else_ret;
    }

    if (ip->opc == OPI_OPC_JMP) {
      ip = OPI_JMP_ARG_TO(ip);
      continue;
    }

    if (opi_insn_is_end(ip))
      break;
  }

  test(NULL, data);
  return FALSE;
}

OpiInsn*
opi_bytecode_find_creating(OpiBytecode *bc, int vid)
{
  OpiInsn *ret = NULL;
  int test(OpiInsn *insn, void *data) {
    if (insn && opi_insn_is_creating(insn, vid)) {
      ret = insn;
      return FALSE;
    }
    return TRUE;
  }
  bc->point = bc->head;
  opi_bytecode_while(bc, test, NULL);
  return ret;
}

struct trace { OpiInsn *start, *end; };

static OpiInsn*
split_if(OpiInsn *if_insn, struct trace *then, struct trace *els)
{
  OpiInsn *else_label = OPI_IF_ARG_ELSE(if_insn);
  OpiInsn *jmp = else_label->prev;
  opi_assert(jmp->opc == OPI_OPC_JMP);

  then->start = if_insn->next;
  then->end = jmp;

  els->start = OPI_IF_ARG_ELSE(if_insn);
  els->start = els->start->next;
  els->end = OPI_JMP_ARG_TO(jmp);
  return els->end->next;
}

static int
check_end(OpiBytecode *bc, OpiInsn *begin, OpiInsn *end)
{
  for (OpiInsn *ip = begin; ip != end; ip = ip->next) {
    if (ip->opc == OPI_OPC_IF) {
      struct trace then_trace, else_trace;
      OpiInsn *cont_start;

      cont_start = split_if(ip, &then_trace, &else_trace);

      int a = check_end(bc, then_trace.start, then_trace.end);
      int b = check_end(bc, else_trace.start, else_trace.end);
      int c = check_end(bc, cont_start, end);
      return c || (a && b);
    }

    if (ip->opc == OPI_OPC_JMP) {
      ip = OPI_JMP_ARG_TO(ip);
      continue;
    }

    if (opi_insn_is_end(ip))
      return TRUE;
  }

  return FALSE;
}

static int
kill_value_aux(OpiBytecode *bc, OpiInsn *begin, OpiInsn *end, int vid)
{
  OpiInsn *last_user = NULL;

  for (OpiInsn *ip = begin; ip != end; ip = ip->next) {
    if (opi_insn_is_using(ip, vid))
      last_user = ip;

    if (ip->opc == OPI_OPC_IF) {
      struct trace then_trace, else_trace;
      OpiInsn *cont_start;

      cont_start = split_if(ip, &then_trace, &else_trace);

      if (kill_value_aux(bc, cont_start, end, vid)) {
        if (check_end(bc, then_trace.start, then_trace.end)) {
          if (!kill_value_aux(bc, then_trace.start, then_trace.end, vid)) {
            // kill value at the beginning of a then-branch
            bc->point = then_trace.start;
            opi_bytecode_unref(bc, vid);
          }
        }
        if (check_end(bc, else_trace.start, else_trace.end)) {
          if (!kill_value_aux(bc, else_trace.start, else_trace.end, vid)) {
            // kill value at the beginning of a else-branch
            bc->point = else_trace.start;
            opi_bytecode_unref(bc, vid);
          }
        }
        return TRUE;

      } else {
        int then_found = kill_value_aux(bc, then_trace.start, then_trace.end, vid);
        int else_found = kill_value_aux(bc, else_trace.start, else_trace.end, vid);

        if (!then_found && !else_found) {
          break;

        } else {
          if (!else_found) {
            // kill value at the beginning of an else-branch
            bc->point = else_trace.start;
            opi_bytecode_unref(bc, vid);
          } else if (!then_found) {
            // kill value at the beginning of a then-branch
            bc->point = then_trace.start;
            opi_bytecode_unref(bc, vid);
          }
          return TRUE;
        }
      }

      abort();
    }

    if (ip->opc == OPI_OPC_JMP) {
      ip = OPI_JMP_ARG_TO(ip);
      continue;
    }
  }

  if (last_user == NULL) {
    return FALSE;

  } else {
    if (opi_insn_is_killing(last_user, vid)) {
      bc->point = last_user;
      opi_bytecode_decrc(bc, vid);
    } else {
      bc->point = last_user->next;
      opi_bytecode_unref(bc, vid);
    }
    return TRUE;
  }
}

static void
kill_value_local(OpiBytecode *bc, int vid)
{
  OpiInsn *start = opi_bytecode_find_creating(bc, vid);
  opi_assert(start);

  if (kill_value_aux(bc, start, bc->tail, vid)) {
    bc->point = start->next;
    opi_bytecode_incrc(bc, vid);
  } else {
    bc->point = start->next;
    opi_bytecode_drop(bc, vid);
  }
}

static void
kill_value_phi(OpiBytecode *bc, int vid)
{
  OpiInsn *start = opi_bytecode_find_creating(bc, vid);
  opi_assert(start);
  if (!kill_value_aux(bc, start, bc->tail, vid)) {
    opi_error("failed to kill %%%d\n", vid);
    abort();
  }
}

void
opi_bytecode_fix_lifetimes(OpiBytecode *bc)
{
  for (size_t vid = 0; vid < bc->nvals; ++vid) {
    if (bc->vinfo[vid].type == OPI_VAL_LOCAL)
      kill_value_local(bc, vid);
    else if (bc->vinfo[vid].type == OPI_VAL_PHI)
      kill_value_phi(bc, vid);
  }
}

static int
will_be_used(OpiInsn *insn, int vid)
{
  while (insn->opc != OPI_OPC_END) {
    if (opi_insn_is_using(insn, vid))
      return TRUE;
    insn = insn->next;
  }
  return FALSE;
}

void
opi_bytecode_cleanup(OpiBytecode *bc)
{
  OpiInsn *insn = bc->head;
  while (insn->opc != OPI_OPC_END) {
    if (insn->opc == OPI_OPC_CONST && OPI_CONST_ARG_CELL(insn) == opi_nil) {
      if (!will_be_used(insn, OPI_CONST_REG_OUT(insn))) {
        OpiInsn *tmp = insn->next;
        insn->prev->next = insn->next;
        insn->next->prev = insn->prev;
        opi_insn_delete1(insn);
        /*opi_debug("erased dummy NIL\n");*/
        insn = tmp;
        continue;
      }
    } else if (insn->opc == OPI_OPC_INCRC) {
      int vid = OPI_INCRC_REG_CELL(insn);
      if (insn->next->opc == OPI_OPC_DECRC &&
          (int)OPI_DECRC_REG_CELL(insn->next) == vid)
      {
        OpiInsn *tmp = insn->next->next;
        insn->prev->next = insn->next->next;
        insn->next->next->prev = insn->prev;
        opi_insn_delete1(insn->next);
        opi_insn_delete1(insn);
        /*opi_debug("erased dummy INC->DEC\n");*/
        insn = tmp;
        continue;
      }
    }
    insn = insn->next;
  }
}

static size_t
distance(OpiInsn *from, OpiInsn *to)
{
  size_t d = 0;
  while (from != to) {
    d += 1;
    from = from->next;
  }
  return d;
}

OpiFlatInsn*
opi_bytecode_flatten(OpiBytecode *bc)
{
  size_t size = distance(bc->head, NULL);
  OpiFlatInsn *buf = malloc(sizeof(OpiFlatInsn) * size);
  size_t i = 0;

  OpiInsn *insn = bc->head;
  while (insn) {
    OpiFlatInsn *finsn = buf + i++;
    *finsn = *(OpiFlatInsn*)insn;

    if (insn->opc == OPI_OPC_JMP) {
      size_t d = distance(insn, OPI_JMP_ARG_TO(insn));
      OPI_JMP_ARG_TO(finsn) = finsn + d;
    } else if (insn->opc == OPI_OPC_IF) {
      size_t d = distance(insn, OPI_IF_ARG_ELSE(insn));
      OPI_IF_ARG_ELSE(finsn) = finsn + d;
    }

    insn = insn->next;
  }

  return buf;
}
