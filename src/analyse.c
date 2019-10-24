#include "opium/opium.h"

#include <stdlib.h>
#include <string.h>

int
opi_bytecode_while(struct opi_bytecode *bc, int (*test)(struct opi_insn *insn, void *data), void *data)
{
  for (struct opi_insn *ip = bc->point; ip; ip = ip->next) {
    if (!test(ip, data))
      return TRUE;

    if (ip->opc == OPI_OPC_IF) {
      bc->point = ip->next;
      int then_ret = opi_bytecode_while(bc, test, data);

      bc->point = ((struct opi_insn*)OPI_IF_ARG_ELSE(ip))->next;
      int else_ret = opi_bytecode_while(bc, test, data);

      return then_ret || else_ret;
    }

    if (ip->opc == OPI_OPC_JMP) {
      ip = OPI_JMP_ARG_TO(ip);
      continue;
    }

    /*if (opi_insn_is_end(ip))*/
      /*break;*/
  }

  test(NULL, data);
  return FALSE;
}

struct opi_insn*
opi_bytecode_find_creating(struct opi_bytecode *bc, int vid)
{
  struct opi_insn *ret = NULL;
  int test(struct opi_insn *insn, void *data) {
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

int
opi_bytecode_find_last_users(struct opi_bytecode *bc, int vid, struct opi_ptrvec *vec)
{
  struct opi_insn *buf = NULL;;
  int test(struct opi_insn *insn, void *data) {
    if (insn) {
      if (opi_insn_is_using(insn, vid))
        buf = insn;
    } else {
      if (buf)
        opi_ptrvec_push(vec, buf, NULL);
    }
    return TRUE;
  }
  return opi_bytecode_while(bc, test, NULL);
}

struct trace { struct opi_insn *start, *end; };

static struct opi_insn*
split_if(struct opi_insn *if_insn, struct trace *then, struct trace *els)
{
  struct opi_insn *else_label = OPI_IF_ARG_ELSE(if_insn);
  struct opi_insn *jmp = else_label->prev;
  opi_assert(jmp->opc == OPI_OPC_JMP);

  then->start = if_insn->next;
  then->end = jmp;

  els->start = OPI_IF_ARG_ELSE(if_insn);
  els->start = els->start->next;
  els->end = OPI_JMP_ARG_TO(jmp);
  return els->end->next;
}

static int
kill_value_aux(struct opi_bytecode *bc, struct opi_insn *begin, struct opi_insn *end, int vid)
{
  struct opi_insn *last_user = NULL;

  for (struct opi_insn *ip = begin; ip != end; ip = ip->next) {
    if (opi_insn_is_using(ip, vid))
      last_user = ip;

    if (ip->opc == OPI_OPC_IF) {
      struct trace then_trace, else_trace;
      struct opi_insn *cont_start;

      cont_start = split_if(ip, &then_trace, &else_trace);

      if (kill_value_aux(bc, cont_start, end, vid)) {
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

    /*if (opi_insn_is_end(ip))*/
      /*break;*/
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
kill_value_local(struct opi_bytecode *bc, int vid)
{
  struct opi_insn *start = opi_bytecode_find_creating(bc, vid);
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
kill_value_phi(struct opi_bytecode *bc, int vid)
{
  struct opi_insn *start = opi_bytecode_find_creating(bc, vid);
  opi_assert(start);
  if (!kill_value_aux(bc, start, bc->tail, vid)) {
    opi_error("failed to kill %%%d\n", vid);
    abort();
  }
}

void
opi_bytecode_fix_lifetimes(struct opi_bytecode *bc)
{
  for (size_t vid = 0; vid < bc->nvals; ++vid) {
    if (bc->vinfo[vid].type == OPI_VAL_LOCAL)
      kill_value_local(bc, vid);
    else if (bc->vinfo[vid].type == OPI_VAL_PHI)
      kill_value_phi(bc, vid);
  }
}

