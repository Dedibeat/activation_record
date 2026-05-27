#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "escape.h"

typedef struct escapeEntry_ *escapeEntry;
struct escapeEntry_ {
  int depth;
  bool *escape;
};

static escapeEntry EscapeEntry(int depth, bool *escape);
static void traverseExpList(S_table env, int depth, A_expList exps);
static void traverseDecList(S_table env, int depth, A_decList decs);
static void traverseFieldList(S_table env, int depth, A_fieldList fields);
static void traverseEfieldList(S_table env, int depth, A_efieldList fields);
static void traverseFundecList(S_table env, int depth, A_fundecList funcs);
static void traverseVar(S_table env, int depth, A_var v);
static void traverseExp(S_table env, int depth, A_exp e);
static void traverseDec(S_table env, int depth, A_dec d);
static void traverseTy(S_table env, int depth, A_ty ty);

void Esc_findEscape(A_exp exp) {
  traverseExp(S_empty(), 0, exp);
}

static escapeEntry EscapeEntry(int depth, bool *escape) {
  escapeEntry e = checked_malloc(sizeof(*e));
  e->depth = depth;
  e->escape = escape;
  return e;
}

static void traverseExpList(S_table env, int depth, A_expList exps) {
  for (; exps; exps = exps->tail)
    if (exps->head) traverseExp(env, depth, exps->head);
}

static void traverseDecList(S_table env, int depth, A_decList decs) {
  for (; decs; decs = decs->tail)
    if (decs->head) traverseDec(env, depth, decs->head);
}

static void traverseFieldList(S_table env, int depth, A_fieldList fields) {
  for (; fields; fields = fields->tail) {
    if (!fields->head) continue;
    fields->head->escape = FALSE;
    S_enter(env, fields->head->name,
            EscapeEntry(depth, &fields->head->escape));
  }
}

static void traverseEfieldList(S_table env, int depth, A_efieldList fields) {
  for (; fields; fields = fields->tail)
    if (fields->head) traverseExp(env, depth, fields->head->exp);
}

static void traverseFundecList(S_table env, int depth, A_fundecList funcs) {
  for (; funcs; funcs = funcs->tail) {
    if (!funcs->head) continue;
    S_beginScope(env);
    traverseFieldList(env, depth + 1, funcs->head->params);
    traverseExp(env, depth + 1, funcs->head->body);
    S_endScope(env);
  }
}

static void traverseVar(S_table env, int depth, A_var v) {
  switch (v->kind) {
    case A_simpleVar: {
      escapeEntry x = S_look(env, v->u.simple);
      if (x && depth > x->depth) *x->escape = TRUE;
      break;
    }
    case A_fieldVar:
      traverseVar(env, depth, v->u.field.var);
      break;
    case A_subscriptVar:
      traverseVar(env, depth, v->u.subscript.var);
      traverseExp(env, depth, v->u.subscript.exp);
      break;
  }
}

static void traverseExp(S_table env, int depth, A_exp e) {
  if (!e) return;

  switch (e->kind) {
    case A_varExp:
      traverseVar(env, depth, e->u.var);
      break;
    case A_nilExp:
    case A_intExp:
    case A_stringExp:
    case A_breakExp:
      break;
    case A_callExp:
      traverseExpList(env, depth, e->u.call.args);
      break;
    case A_opExp:
      traverseExp(env, depth, e->u.op.left);
      traverseExp(env, depth, e->u.op.right);
      break;
    case A_recordExp:
      traverseEfieldList(env, depth, e->u.record.fields);
      break;
    case A_seqExp:
      traverseExpList(env, depth, e->u.seq);
      break;
    case A_assignExp:
      traverseVar(env, depth, e->u.assign.var);
      traverseExp(env, depth, e->u.assign.exp);
      break;
    case A_ifExp:
      traverseExp(env, depth, e->u.iff.test);
      traverseExp(env, depth, e->u.iff.then);
      traverseExp(env, depth, e->u.iff.elsee);
      break;
    case A_whileExp:
      traverseExp(env, depth, e->u.whilee.test);
      traverseExp(env, depth, e->u.whilee.body);
      break;
    case A_forExp:
      traverseExp(env, depth, e->u.forr.lo);
      traverseExp(env, depth, e->u.forr.hi);
      S_beginScope(env);
      e->u.forr.escape = FALSE;
      S_enter(env, e->u.forr.var,
              EscapeEntry(depth, &e->u.forr.escape));
      traverseExp(env, depth, e->u.forr.body);
      S_endScope(env);
      break;
    case A_letExp:
      S_beginScope(env);
      traverseDecList(env, depth, e->u.let.decs);
      traverseExp(env, depth, e->u.let.body);
      S_endScope(env);
      break;
    case A_arrayExp:
      traverseExp(env, depth, e->u.array.size);
      traverseExp(env, depth, e->u.array.init);
      break;
  }
}

static void traverseDec(S_table env, int depth, A_dec d) {
  if (!d) return;

  switch (d->kind) {
    case A_functionDec:
      traverseFundecList(env, depth, d->u.function);
      break;
    case A_varDec:
      traverseExp(env, depth, d->u.var.init);
      d->u.var.escape = FALSE;
      S_enter(env, d->u.var.var,
              EscapeEntry(depth, &d->u.var.escape));
      break;
    case A_typeDec:
      for (A_nametyList tys = d->u.type; tys; tys = tys->tail)
        traverseTy(env, depth, tys->head->ty);
      break;
  }
}

static void traverseTy(S_table env, int depth, A_ty ty) {
  (void)env;
  (void)depth;
  (void)ty;
}
