# Modern Compiler Implementation in C: Chapter 6 Scaffold

This repository is a learning scaffold for the Tiger compiler from *Modern Compiler Implementation in C*. It focuses on Chapter 6, activation records. The solved frame and translation logic was removed from `x86_64frame.c` and `translate.c`; the goal is to fill it back in gradually while keeping the compiler buildable and testable after each step.

## Build

Build artifacts are kept in `bin/`.

```sh
make clean
make
```

The compiler binary is:

```sh
bin/a.out
```

Run it on a Tiger file:

```sh
bin/a.out tests/ch6/01_local_var.tig
```

The compiler currently prints IR trees. While the scaffold TODOs are unfinished, some output is intentionally placeholder IR.

## Testing

The project has automatic tests for the Chapter 6 TODO areas.

Run the full test target:

```sh
make test
```

Run only C unit tests:

```sh
make ch6-unit-tests
```

Run only Tiger IR integration tests:

```sh
make ch6-ir-tests
```

Test output is written under:

```text
bin/testoutput/
```

The Chapter 6 tests are intentionally written against the desired implementation, not the placeholder scaffold. Until you implement the TODOs, failures are expected. Use them as checkpoints:

- `ch6_unit_tests.c` checks frame allocation, formal access behavior, static-link formal hiding, and escape analysis flags.
- `tests/ch6/*.tig` are small Tiger programs that exercise locals, escaping variables, nested functions, static links, records, arrays, and control flow.
- `run_ir_tests.sh` runs the compiler on those Tiger files and checks for expected IR patterns.

If you have the book's external `../testcases_correct/*.tig` directory, `make test` also runs those existing tests and writes output to `bin/testoutput/`.

## Project Flow

The core data flow is:

1. `semant.c` type-checks AST nodes and calls `Tr_*` functions.
2. `translate.c` turns Tiger constructs into IR trees from `tree.h`.
3. `x86_64frame.c` describes where variables live in a procedure frame.
4. `Tr_access` connects a source variable to both its declaration level and its frame-level access.
5. `Tr_level` connects a Tiger function nesting level to an `F_frame`.

Start with frame/access objects before implementing complicated expressions, calls, or control flow.

## Background

An activation record, or stack frame, is the runtime workspace for one function call. It normally contains incoming arguments, local variables, saved registers, return information, and possibly outgoing argument space.

The compiler uses a stack because function calls are last-in, first-out. A call creates a new frame; returning from the call removes that frame. The frame pointer gives the compiler a stable base address for variables even if the stack pointer moves.

This project splits activation-record work into two layers:

- `frame.h` / `x86_64frame.c`: target-specific frame layout, word size, frame-pointer offsets, abstract registers, and conversion from `F_access` to IR.
- `translate.h` / `translate.c`: target-independent source-language translation, lexical levels, static links, and source constructs.

## Important IR Types

`tree.h` defines the intermediate representation.

Common expressions:

- `T_Const(i)`: integer constant.
- `T_Temp(t)`: abstract temporary.
- `T_Name(label)`: symbolic address.
- `T_Mem(addr)`: memory load/store location.
- `T_Binop(op, left, right)`: arithmetic or bitwise operation.
- `T_Call(fun, args)`: function call.
- `T_Eseq(stm, exp)`: execute a statement, then produce a value.

Common statements:

- `T_Move(dst, src)`: assignment or store.
- `T_Exp(exp)`: evaluate for side effects.
- `T_Label(label)`: mark a control-flow position.
- `T_Jump(exp, labels)`: unconditional jump.
- `T_Cjump(op, left, right, true, false)`: conditional jump.
- `T_Seq(left, right)`: execute statements in order.

Frame-pointer-relative memory access usually becomes:

```c
T_Mem(T_Binop(T_plus, framePtr, T_Const(offset)))
```

## Frames and Accesses

`F_frame` represents one function frame. The scaffold stores a function label, formal accesses, and local accesses. You will likely add allocation state such as a next-local offset counter.

`F_access` says where one variable lives:

- `inFrame(offset)`: stored in memory at `framePointer + offset`.
- `inReg(temp)`: stored in an abstract temporary.

A simple x86-64 policy for this scaffold:

- `F_wordSize = 8`.
- escaping formals can use positive frame-pointer offsets.
- escaping locals commonly use negative offsets.
- non-escaping variables can use `Temp_newtemp()`.

`F_Exp(access, framePtr)` converts an access into IR:

- `inFrame(k)` becomes `MEM(framePtr + k)`.
- `inReg(t)` becomes `TEMP(t)`.

Keep this target-specific function small. `translate.c` should compute the right frame pointer expression and then call `F_Exp`.

## Escape Analysis

A variable escapes when it is used outside the function where it was declared:

```tiger
let
  var x := 10
  function f(): int = x
in
  f()
end
```

Here `x` must be in a stable frame location because `f` accesses it through a static link.

Escape analysis should walk the AST before semantic translation:

1. Track each variable or parameter declaration and its depth.
2. On variable use, compare current depth to declaration depth.
3. If used from a deeper depth, set that declaration's `escape` flag to `TRUE`.

Relevant AST fields:

- `A_varDec`: `d->u.var.escape`
- `A_field`: `field->escape`
- `A_forExp`: `a->u.forr.escape`

After escape analysis is wired in, `semant.c` should use those flags for `Tr_allocLocal` and `Tr_newLevel`.

## Lexical Levels and Static Links

Tiger allows nested functions. `Tr_level` represents source-level lexical nesting:

```c
struct Tr_level_ {
  Tr_level parent;
  F_frame frame;
};
```

The parent pointer is lexical, not dynamic. If `inner` is declared inside `outer`, then `inner->parent == outer`.

A static link is a hidden argument that points to the frame of the callee's lexical parent. It lets nested functions access variables declared in outer functions.

Recommended convention for this scaffold:

- add the static link as the first formal in `Tr_newLevel`
- make it escaping
- hide it from user parameter binding by having `Tr_formals(level)` return only user formals

When translating a variable:

1. If it was declared in the current level, use `T_Temp(F_FP())`.
2. If it was declared in an outer level, follow static links one level at a time.
3. Stop at the declaration level.
4. Call `F_Exp(access, computedFramePointer)`.

When translating a function call, compute the static-link argument for the callee's parent level and put it in the argument position required by your convention.

## Expression Wrappers

Appel's translation layer uses three expression wrappers:

- `Ex`: expression with a value.
- `Nx`: statement with no value.
- `Cx`: conditional expression represented as a branch with patchable labels.

The conversion helpers are:

- `unEx(e)`: produce a value expression.
- `unNx(e)`: produce a statement.
- `unCx(e)`: produce a conditional branch.

`Cx` uses patch lists because true and false labels are often not known when a conditional is first created. `doPatch` fills in labels later; `joinPatch` combines pending patch lists.

## Implementation Plan

### 0. Baseline

1. Run `make clean && make`.
2. Run `bin/a.out` on a tiny Tiger file and inspect the placeholder IR.
3. Read `frame.h`, `translate.h`, `tree.h`, `temp.h`, `semant.c`, `x86_64frame.c`, and `translate.c`.

### 1. Frame Access Constructors

File: `x86_64frame.c`

1. Verify `InFrame(offset)` sets `kind = inFrame`.
2. Verify `InReg(temp)` sets `kind = inReg`.
3. Keep `F_wordSize = 8`.
4. Decide frame offset direction.
5. Add frame-local allocation state if needed.

Checkpoint: `make` succeeds.

### 2. `F_newFrame`

File: `x86_64frame.c`

1. Accept a function label and formal escape flags.
2. Create one `F_access` per formal.
3. Put escaping formals in the frame.
4. Put non-escaping formals in temps if your convention allows it.
5. Preserve formal declaration order.
6. Initialize local-allocation state.

Checkpoint: formal count and order are correct.

### 3. `F_allocLocal`

File: `x86_64frame.c`

1. If `escape == TRUE`, allocate an `InFrame` access at the next local offset.
2. If `escape == FALSE`, allocate `InReg(Temp_newtemp())`.
3. Add the access to `f->locals`.
4. Ensure escaping locals get distinct offsets.

Checkpoint: `make ch6-unit-tests` should start passing frame-local tests.

### 4. `F_Exp`

File: `x86_64frame.c`

1. `inFrame` becomes `T_Mem(T_Binop(T_plus, framePtr, T_Const(offset)))`.
2. `inReg` becomes `T_Temp(reg)`.

Checkpoint: variable references should stop printing as placeholder constants.

### 5. Translation Levels

File: `translate.c`

1. In `Tr_newLevel`, prepend the static-link formal to user formals.
2. Decide whether `Tr_formals(level)` hides or exposes the static link.
3. Keep `semant.c` parameter binding consistent with that decision.
4. Implement `Tr_allocLocal` as a wrapper around `F_allocLocal`.

Recommended: hide the static link from semantic parameter binding.

### 6. `Tr_simpleVar`

File: `translate.c`

1. Use the current frame pointer for same-level variables.
2. Follow static links for variables declared in outer levels.
3. Pass the resulting frame pointer expression to `F_Exp`.

Checkpoint Tiger idea:

```tiger
let
  var x := 10
  function f(): int = x
in
  f()
end
```

### 7. Expression Wrappers

File: `translate.c`

Implement and verify:

- `unEx`
- `unNx`
- `unCx`
- `doPatch`
- `joinPatch`

Do this before `if`, `while`, and relational operators.

### 8. Simple Expressions

File: `translate.c`

Implement:

- nil, int, and string expressions
- arithmetic operators
- relational operators
- string equality and inequality via runtime calls

### 9. Variables and Memory Objects

File: `translate.c`

Implement:

- `Tr_fieldVar`
- `Tr_subscriptVar`
- `Tr_recordExp`
- `Tr_arrayExp`

Nil checks and bounds checks can wait unless required by your course.

### 10. Control Flow

File: `translate.c`

Implement:

- `Tr_ifExp`
- `Tr_ifExp_noValue`
- `Tr_whileExp`
- `Tr_breakExp`
- `Tr_seqStm`
- `Tr_eseqExp`
- `Tr_LetExp`

### 11. Function Calls

File: `translate.c`

1. Convert Tiger arguments to `T_expList`.
2. Compute the static-link argument from caller and callee levels.
3. Put the static link in the correct argument position.
4. Return `T_Call`.

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

### 12. Procedure Fragments

File: `translate.c`

1. Convert function bodies into statements.
2. Move value results into `F_RV()` when needed.
3. Call `F_procEntryExit1`.
4. Add `F_ProcFrag` to the fragment list.
5. Keep string fragments in the same global fragment list.

### 13. Escape Analysis

Files: `escape.c`, `semant.c`

1. Implement `Esc_findEscape`.
2. Track declaration depth for variables and parameters.
3. Mark escaping declarations.
4. Call `Esc_findEscape(exp)` before semantic translation.
5. Use escape flags in variable declarations and formal lists.

Checkpoint: non-escaping locals become `InReg`; escaping locals become `InFrame`.

## Small Manual Test Categories

Use tiny programs while implementing:

1. integer literal
2. local variable
3. two local variables
4. nested function reads outer variable
5. function with parameters
6. record creation and field access
7. array creation and indexing
8. if expression returning a value
9. while loop with break
10. mutually recursive functions

## Common Mistakes

- Reversing formal order while building `F_formals`.
- Adding a static link to the frame and accidentally binding it as a user parameter.
- Using the current frame pointer for every variable.
- Allocating every escaping local at offset `0`.
- Forgetting that `F_Exp` needs the frame pointer for the variable's declaration level.
- Confusing lexical nesting with caller/callee order.
- Returning `Nx` from a let expression whose body has a value.
- Implementing escape analysis but still passing `TRUE` for every variable and parameter.
- Dropping either the static link or the first user argument in `Tr_callExp`.

## Cleanup Guidance

Remove scaffold TODO comments only after the corresponding implementation is complete. Keep short comments that explain target-specific frame choices. After each step, run:

```sh
make clean
make test
```
