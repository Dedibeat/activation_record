#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "frame.h"

/*
 * Chapter 6 exercise scaffold.
 *
 * The original implementation in this file solved frame layout, formal/local
 * placement, and access-to-IR translation. Those decisions are intentionally
 * removed so you can implement them while working through activation records.
 */

struct F_frame_ {
  Temp_label label;
  F_accessList formals;
  F_accessList locals;
};

struct F_access_ {
  enum { inFrame, inReg } kind;
  union {
    int offset;
    Temp_temp reg;
  } u;
};

const int F_wordSize = 8;

static F_access InFrame(int offset);
static F_access InReg(Temp_temp reg);
static F_access placeholderAccess(bool escape);

Temp_label F_name(F_frame f) { return f->label; }

F_accessList F_formals(F_frame f) { return f->formals; }

F_access F_allocLocal(F_frame f, bool escape) {
  /*
   * TODO(ch6): Allocate a local in the frame or in a register.
   *
   * Decide where escaping and non-escaping locals live, maintain frame-local
   * offset state, and add the new access to f->locals.
   */
  F_access access = placeholderAccess(escape);
  f->locals = F_AccessList(access, f->locals);
  return access;
}

F_frame F_newFrame(Temp_label label, U_boolList formals) {
  /*
   * TODO(ch6): Build the target-specific frame.
   *
   * Include the static link formal, assign formal accesses according to the
   * escape flags and your calling convention, and initialize local-allocation
   * state for the frame.
   */
  F_frame f = (F_frame)checked_malloc(sizeof(*f));
  f->label = label;
  f->formals = NULL;
  f->locals = NULL;

  F_accessList head = F_AccessList(NULL, NULL);
  F_accessList tail = head;
  for (; formals; formals = formals->tail) {
    tail->tail = F_AccessList(placeholderAccess(formals->head), NULL);
    tail = tail->tail;
  }
  f->formals = head->tail;
  free(head);

  return f;
}

static F_access InFrame(int offset) {
  F_access a = checked_malloc(sizeof(*a));
  a->kind = inFrame;
  a->u.offset = offset;
  return a;
}

static F_access InReg(Temp_temp reg) {
  F_access a = checked_malloc(sizeof(*a));
  a->kind = inReg;
  a->u.reg = reg;
  return a;
}

static F_access placeholderAccess(bool escape) {
  /*
   * Temporary scaffold only. Replace this policy when implementing frame
   * layout. Keeping distinct constructors makes it easy to test both access
   * forms later.
   */
  return escape ? InFrame(0) : InReg(Temp_newtemp());
}

F_accessList F_AccessList(F_access h, F_accessList t) {
  F_accessList p = (F_accessList)checked_malloc(sizeof(*p));
  p->head = h;
  p->tail = t;
  return p;
}

static Temp_temp framePointer = NULL;
static Temp_temp returnValue = NULL;

Temp_temp F_FP(void) {
  if (!framePointer) framePointer = Temp_newtemp();
  return framePointer;
}

Temp_temp F_RV(void) {
  if (!returnValue) returnValue = Temp_newtemp();
  return returnValue;
}

T_exp F_Exp(F_access acc, T_exp framePtr) {
  /*
   * TODO(ch6): Translate an F_access into IR.
   *
   * In-frame accesses should become MEM(framePtr + offset). In-register
   * accesses should become TEMP(reg).
   */
  (void)acc;
  (void)framePtr;
  return T_Const(0);
}

F_frag F_StringFrag(Temp_label label, string str) {
  F_frag f = (F_frag)checked_malloc(sizeof(*f));
  f->kind = F_stringFrag;
  f->u.stringg.label = label;
  f->u.stringg.str = str;
  return f;
}

F_frag F_ProcFrag(T_stm body, F_frame frame) {
  F_frag f = (F_frag)checked_malloc(sizeof(*f));
  f->kind = F_procFrag;
  f->u.proc.body = body;
  f->u.proc.frame = frame;
  return f;
}

F_fragList F_FragList(F_frag head, F_fragList tail) {
  F_fragList l = (F_fragList)checked_malloc(sizeof(*l));
  l->head = head;
  l->tail = tail;
  return l;
}

T_exp F_externalCall(string s, T_expList args) {
  return T_Call(T_Name(Temp_namedlabel(s)), args);
}

T_stm F_procEntryExit1(F_frame frame, T_stm stm) {
  /*
   * TODO(ch6): Add procedure-entry/procedure-exit moves once your frame
   * representation knows where formals and special registers belong.
   */
  (void)frame;
  return stm;
}
