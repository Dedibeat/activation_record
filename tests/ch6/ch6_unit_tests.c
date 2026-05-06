#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "frame.h"
#include "translate.h"
#include "escape.h"

static int failures = 0;

#define CHECK(name, condition)                                                \
  do {                                                                        \
    if (condition) {                                                          \
      printf("PASS %s\n", name);                                              \
    } else {                                                                  \
      printf("FAIL %s\n", name);                                              \
      failures += 1;                                                          \
    }                                                                         \
  } while (0)

static int count_f_accesses(F_accessList xs) {
  int n = 0;
  for (; xs; xs = xs->tail) n += 1;
  return n;
}

static int count_tr_accesses(Tr_accessList xs) {
  int n = 0;
  for (; xs; xs = xs->tail) n += 1;
  return n;
}

static int frame_offset(T_exp e, int *offset) {
  if (!e || e->kind != T_MEM) return 0;
  T_exp addr = e->u.MEM;
  if (!addr || addr->kind != T_BINOP || addr->u.BINOP.op != T_plus) return 0;
  if (!addr->u.BINOP.right || addr->u.BINOP.right->kind != T_CONST) return 0;
  *offset = addr->u.BINOP.right->u.CONST;
  return 1;
}

static void test_frame_alloc_local(void) {
  F_frame f = F_newFrame(Temp_newlabel(), NULL);
  T_exp fp = T_Temp(F_FP());

  F_access non_escape = F_allocLocal(f, FALSE);
  T_exp non_escape_exp = F_Exp(non_escape, fp);
  CHECK("F_allocLocal(FALSE) translates to TEMP",
        non_escape_exp && non_escape_exp->kind == T_TEMP);

  F_access escape1 = F_allocLocal(f, TRUE);
  F_access escape2 = F_allocLocal(f, TRUE);
  T_exp escape_exp1 = F_Exp(escape1, fp);
  T_exp escape_exp2 = F_Exp(escape2, fp);
  int off1 = 0, off2 = 0;

  CHECK("F_allocLocal(TRUE) translates to MEM",
        escape_exp1 && escape_exp1->kind == T_MEM);
  CHECK("escaping locals have inspectable frame offsets",
        frame_offset(escape_exp1, &off1) && frame_offset(escape_exp2, &off2));
  CHECK("escaping locals get distinct offsets", off1 != off2);
  CHECK("escaping locals use negative offsets", off1 < 0 && off2 < 0);
}

static void test_frame_formals(void) {
  U_boolList escapes =
      U_BoolList(TRUE, U_BoolList(FALSE, U_BoolList(TRUE, NULL)));
  F_frame f = F_newFrame(Temp_newlabel(), escapes);
  F_accessList formals = F_formals(f);
  T_exp fp = T_Temp(F_FP());

  CHECK("F_newFrame preserves formal count", count_f_accesses(formals) == 3);
  CHECK("escaping formal translates to MEM",
        formals && F_Exp(formals->head, fp)->kind == T_MEM);
  CHECK("non-escaping formal translates to TEMP",
        formals && formals->tail &&
            F_Exp(formals->tail->head, fp)->kind == T_TEMP);
  CHECK("later escaping formal translates to MEM",
        formals && formals->tail && formals->tail->tail &&
            F_Exp(formals->tail->tail->head, fp)->kind == T_MEM);
}

static void test_translate_formals_hide_static_link(void) {
  Tr_level parent = Tr_outermost();
  Tr_level child = Tr_newLevel(parent, Temp_newlabel(),
                              U_BoolList(TRUE, U_BoolList(FALSE, NULL)));

  CHECK("Tr_formals exposes only user formals",
        count_tr_accesses(Tr_formals(child)) == 2);
}

static void test_escape_var_from_nested_function(void) {
  S_symbol x = S_Symbol("x");
  A_dec var = A_VarDec(1, x, NULL, A_IntExp(1, 10));
  var->u.var.escape = FALSE;

  A_exp body = A_VarExp(1, A_SimpleVar(1, x));
  A_fundec f = A_Fundec(1, S_Symbol("f"), NULL, S_Symbol("int"), body);
  A_dec fundec = A_FunctionDec(1, A_FundecList(f, NULL));
  A_exp program = A_LetExp(1,
      A_DecList(var, A_DecList(fundec, NULL)),
      A_CallExp(1, S_Symbol("f"), NULL));

  Esc_findEscape(program);
  CHECK("Esc_findEscape marks outer var used by nested function",
        var->u.var.escape == TRUE);
}

static void test_escape_same_depth_var_stays_false(void) {
  S_symbol x = S_Symbol("x");
  A_dec var = A_VarDec(1, x, NULL, A_IntExp(1, 10));
  var->u.var.escape = FALSE;
  A_exp program = A_LetExp(1, A_DecList(var, NULL),
                           A_VarExp(1, A_SimpleVar(1, x)));

  Esc_findEscape(program);
  CHECK("Esc_findEscape keeps same-depth local non-escaping",
        var->u.var.escape == FALSE);
}

static void test_escape_parameter_from_nested_function(void) {
  S_symbol a = S_Symbol("a");
  A_field param = A_Field(1, a, S_Symbol("int"));
  param->escape = FALSE;

  A_exp inner_body = A_VarExp(1, A_SimpleVar(1, a));
  A_fundec inner = A_Fundec(1, S_Symbol("inner"), NULL, S_Symbol("int"),
                            inner_body);
  A_exp outer_body = A_LetExp(1,
      A_DecList(A_FunctionDec(1, A_FundecList(inner, NULL)), NULL),
      A_CallExp(1, S_Symbol("inner"), NULL));
  A_fundec outer = A_Fundec(1, S_Symbol("outer"), A_FieldList(param, NULL),
                            S_Symbol("int"), outer_body);
  A_exp program = A_LetExp(1,
      A_DecList(A_FunctionDec(1, A_FundecList(outer, NULL)), NULL),
      A_CallExp(1, S_Symbol("outer"), A_ExpList(A_IntExp(1, 1), NULL)));

  Esc_findEscape(program);
  CHECK("Esc_findEscape marks param used by nested function",
        param->escape == TRUE);
}

int main(void) {
  test_frame_alloc_local();
  test_frame_formals();
  test_translate_formals_hide_static_link();
  test_escape_var_from_nested_function();
  test_escape_same_depth_var_stays_false();
  test_escape_parameter_from_nested_function();

  if (failures) {
    printf("%d ch6 unit test(s) failed\n", failures);
    return 1;
  }
  printf("all ch6 unit tests passed\n");
  return 0;
}
