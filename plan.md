# Chapter 6 Activation Records Implementation Plan

This project has been converted into a learning scaffold. The solved frame and translation logic was removed from `x86_64frame.c` and `translate.c`; your job is to fill it back in gradually while keeping the compiler building after each small step.

## 0. Baseline

1. Run `make clean && make`.
2. Run the compiler on a tiny Tiger file and confirm it prints placeholder IR.
3. Read these files before editing:
   - `frame.h`
   - `translate.h`
   - `tree.h`
   - `temp.h`
   - `semant.c`
   - `x86_64frame.c`
   - `translate.c`

## 1. Understand the Data Flow

1. `semant.c` type-checks AST nodes and calls `Tr_*` functions.
2. `translate.c` turns Tiger constructs into IR trees from `tree.h`.
3. `x86_64frame.c` describes where variables live in a procedure frame.
4. `Tr_access` connects a source variable to both:
   - the lexical level where it was declared
   - its frame-level access information
5. `Tr_level` connects a Tiger function nesting level to an `F_frame`.

Do not start with complicated expressions. First make frame/access objects correct.

## 2. Implement Frame Access Constructors

File: `x86_64frame.c`

1. Verify `InFrame(offset)` sets `kind = inFrame`.
2. Verify `InReg(temp)` sets `kind = inReg`.
3. Keep `F_wordSize = 8` for x86-64.
4. Decide your frame offset direction:
   - formals may use positive offsets from frame pointer
   - escaping locals commonly use negative offsets
5. Add state to `struct F_frame_` if needed, such as a local offset counter.

Checkpoint: `make` still succeeds.

## 3. Implement `F_newFrame`

File: `x86_64frame.c`

1. Accept a function label and a list of escaping flags.
2. Create one `F_access` for every formal.
3. Put escaping formals in the frame.
4. Put non-escaping formals in temporaries when your calling convention allows it.
5. Store the formal list in declaration order.
6. Initialize local-allocation state.

Keep it simple first. You can refine register argument handling later.

Checkpoint: write a small debug print or inspect with a debugger to confirm formal count and order.

## 4. Implement `F_allocLocal`

File: `x86_64frame.c`

1. If `escape == TRUE`, allocate an `InFrame` access at the next local offset.
2. If `escape == FALSE`, allocate an `InReg(Temp_newtemp())`.
3. Add the access to `f->locals`.
4. Make sure every escaping local gets a distinct frame offset.

Checkpoint: create Tiger programs with several local variables and confirm the offsets differ.

## 5. Implement `F_Exp`

File: `x86_64frame.c`

1. For `inFrame`, return `T_Mem(T_Binop(T_plus, framePtr, T_Const(offset)))`.
2. For `inReg`, return `T_Temp(reg)`.
3. Keep this function target-specific and small.

Checkpoint: variable references should no longer print as placeholder constants.

## 6. Implement Translation Levels

File: `translate.c`

1. In `Tr_newLevel`, prepend the static-link formal to the user formals.
2. Decide whether `Tr_formals(level)` returns:
   - all formals including the static link, or
   - only user formals
3. Update the function-parameter binding code in `semant.c` to match that decision.
4. Implement `Tr_allocLocal` as a wrapper around `F_allocLocal`.

Recommended for clarity: hide the static link from semantic parameter binding, but keep it available internally for static-link traversal.

Checkpoint: nested functions with parameters should not crash during semantic translation.

## 7. Implement `Tr_simpleVar`

File: `translate.c`

1. If the variable is declared in the current level, use `F_Exp(access, T_Temp(F_FP()))`.
2. If it is declared in an outer level, follow static links one level at a time.
3. Stop when you reach the declaration level.
4. Pass the computed frame-pointer expression to `F_Exp`.

Checkpoint Tiger idea:

```tiger
let
  var x := 10
  function f(): int = x
in
  f()
end
```

The access to `x` should follow one static link.

## 8. Implement Expression Wrappers

File: `translate.c`

1. Implement `unEx`.
2. Implement `unNx`.
3. Implement `unCx`.
4. Implement `doPatch`.
5. Implement `joinPatch`.

Use Appel's `Ex`, `Nx`, and `Cx` model closely. Do this before `if`, `while`, and relational operators.

Checkpoint: relational expressions can become both values and branches.

## 9. Implement Simple Expressions

File: `translate.c`

1. `Tr_nilExp`
2. `Tr_intExp`
3. `Tr_stringExp`
4. arithmetic operators
5. relational operators
6. string equality and inequality using runtime calls

Checkpoint: simple integer and string expressions produce meaningful IR.

## 10. Implement Variables and Memory Objects

File: `translate.c`

1. `Tr_fieldVar`
2. `Tr_subscriptVar`
3. `Tr_recordExp`
4. `Tr_arrayExp`

Keep nil checks and bounds checks as later improvements unless your course requires them now.

Checkpoint Tiger idea:

```tiger
let
  type pair = {a:int, b:int}
  var p := pair {a=1, b=2}
in
  p.b
end
```

## 11. Implement Control Flow

File: `translate.c`

1. `Tr_ifExp`
2. `Tr_ifExp_noValue`
3. `Tr_whileExp`
4. `Tr_breakExp`
5. `Tr_seqStm`
6. `Tr_eseqExp`
7. `Tr_LetExp`

Checkpoint: labels and jumps should be readable in printed IR.

## 12. Implement Function Calls

File: `translate.c`

1. Convert the Tiger argument list into a `T_expList`.
2. Compute the static link argument based on caller and callee levels.
3. Put the static link in the correct argument position.
4. Return a `T_Call`.

Checkpoint Tiger idea:

```tiger
let
  function outer(a:int): int =
    let
      function inner(b:int): int = a + b
    in
      inner(3)
    end
in
  outer(4)
end
```

## 13. Implement Procedure Fragments

File: `translate.c`

1. Convert each function body with `unNx` or move value results into `F_RV()`.
2. Call `F_procEntryExit1`.
3. Add the resulting `F_ProcFrag` to the fragment list.
4. Keep string fragments in the same global fragment list.

Checkpoint: function declarations should create procedure fragments.

## 14. Add Escape Analysis

Files: `escape.c`, `semant.c`

1. Implement `Esc_findEscape`.
2. Track declaration depth for variables and parameters.
3. Mark a variable's `escape` flag when it is used from a deeper depth.
4. Call `Esc_findEscape(exp)` before semantic translation in `SEM_transProg`.
5. Use `d->u.var.escape` in variable declarations.
6. Use each parameter field's `escape` flag when building formal escape lists.

Checkpoint: non-escaping locals should become `InReg`; escaping locals should become `InFrame`.

## 15. Test in Small Layers

After each section:

1. Run `make clean && make`.
2. Run one very small Tiger program.
3. Inspect printed IR.
4. Only then move to the next section.

Good test categories:

1. integer literal
2. local variable
3. two local variables
4. nested function reads outer variable
5. function with parameters
6. record creation and field access
7. array creation and indexing
8. if expression returning value
9. while loop with break
10. mutually recursive functions

## 16. Final Cleanup

1. Remove scaffold TODO comments only after the corresponding implementation is complete.
2. Keep comments that explain target-specific frame choices.
3. Add small tests for static links and escaping variables.
4. Run `make clean && make test` if you have the book testcases available.
