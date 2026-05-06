# Knowledge for Chapter 6 Activation Records

This file summarizes the background needed to work through `plan.md`. It is not an implementation recipe with finished code; it explains the ideas, the repo contracts, and the decisions you need to make before editing `x86_64frame.c`, `translate.c`, `escape.c`, and `semant.c`.

## Big Picture

By the end of chapter 5, semantic analysis knows whether a Tiger program is type-correct. Chapter 6 starts connecting that semantic information to intermediate representation (IR). The important new problem is this:

> When source code mentions a variable or calls a function, where is that variable at run time, and how do nested functions find variables declared by outer functions?

The answer is split across two modules:

- `frame.h` / `x86_64frame.c`: target-machine frame layout. This code knows word size, frame-pointer-relative offsets, registers represented as temps, and how to turn an `F_access` into IR.
- `translate.h` / `translate.c`: target-independent semantic translation. This code knows Tiger lexical nesting levels, static links, and how to translate source constructs into `tree.h` IR.

`semant.c` sits above both. It type-checks the AST and calls `Tr_*` functions to build IR.

## Important Repo Types

### IR Trees: `tree.h`

The compiler builds IR with two major categories:

- `T_exp`: an expression that computes a value.
- `T_stm`: a statement that performs an action.

Common expression nodes:

- `T_Const(i)`: integer constant.
- `T_Temp(t)`: temporary register-like value.
- `T_Name(label)`: symbolic address.
- `T_Mem(addr)`: memory load or store location at address `addr`.
- `T_Binop(op, left, right)`: arithmetic or bitwise operation.
- `T_Call(fun, args)`: function call.
- `T_Eseq(stm, exp)`: execute `stm`, then produce `exp`.

Common statement nodes:

- `T_Move(dst, src)`: assignment. If `dst` is `T_Temp`, assign a temp. If `dst` is `T_Mem`, store to memory.
- `T_Exp(exp)`: evaluate an expression for side effects.
- `T_Label(label)`: mark a position.
- `T_Jump(exp, labels)`: unconditional jump.
- `T_Cjump(op, left, right, true, false)`: conditional branch.
- `T_Seq(left, right)`: execute two statements in order.

Chapter 6 mostly creates IR such as:

```c
T_Mem(T_Binop(T_plus, framePtr, T_Const(offset)))
```

That means "the memory word at `framePtr + offset`."

### Temps and Labels: `temp.h`

`Temp_temp` is the compiler's abstract temporary. It may later become a machine register or stack slot.

`Temp_label` is a symbolic label used for functions, strings, jumps, and runtime calls.

In this repo:

- `F_FP()` returns the abstract frame-pointer temp.
- `F_RV()` returns the abstract return-value temp.
- `Temp_newtemp()` creates a fresh temporary.
- `Temp_newlabel()` creates a fresh label.
- `Temp_namedlabel("name")` creates a stable external/runtime label.

## Frames and Accesses

An activation record, or stack frame, stores the runtime state for one function call. It usually contains:

- incoming arguments or homes for incoming arguments
- escaping locals
- saved registers
- return address and caller information, depending on target conventions
- sometimes outgoing argument space for calls

For this project, start with a simplified frame model. You mainly need to represent where formals and locals live.

### `F_frame`

`F_frame` represents one function's frame. The scaffold currently stores:

- `label`: function label
- `formals`: list of formal parameter accesses
- `locals`: list of local variable accesses

You will likely add local allocation state, for example a next-local offset counter. A common simple policy is:

- frame pointer is the stable base
- escaping formals use positive offsets
- escaping locals use negative offsets
- non-escaping values use fresh temps

### `F_access`

An `F_access` says where one variable lives inside a frame:

- `inFrame(offset)`: the variable is stored in memory at `framePointer + offset`
- `inReg(temp)`: the variable is stored in an abstract temp

Escaping variables generally need `inFrame`, because an inner function may need to find them through a static link. Non-escaping variables can often be `inReg`.

### `F_Exp`

`F_Exp(access, framePtr)` converts an access into IR:

- `inFrame(k)` becomes `MEM(framePtr + k)`
- `inReg(t)` becomes `TEMP(t)`

This is target-specific code and should stay small. `translate.c` should decide which frame pointer to use, then delegate the final address construction to `F_Exp`.

## Escape Analysis

A variable escapes when it is used outside the function where it was declared. Example:

```tiger
let
  var x := 10
  function f(): int = x
in
  f()
end
```

`x` is declared in the outer level but used inside `f`, so it escapes. It needs a stable frame location.

Escape analysis walks the AST before semantic translation:

- remember each variable declaration and the depth where it was declared
- when seeing a variable use, compare current depth with declaration depth
- if current depth is deeper, set that declaration's `escape` flag to `TRUE`

In this repo, AST nodes already have escape fields:

- `A_varDec`: `d->u.var.escape`
- `A_field`: `field->escape` for function parameters
- `A_forExp`: `a->u.forr.escape`

After escape analysis is wired into `SEM_transProg`, semantic translation should use those flags when calling `Tr_allocLocal` and `Tr_newLevel`.

## Lexical Levels

Tiger allows nested functions. A source-level function nesting is represented by `Tr_level`.

In this repo:

```c
struct Tr_level_ {
  Tr_level parent;
  F_frame frame;
};
```

The parent pointer represents lexical nesting, not caller/callee order. If function `inner` is declared inside `outer`, then:

```text
inner level -> parent = outer level
outer level -> parent = outermost or another containing function
```

`Tr_access` combines:

- the `Tr_level` where the variable was declared
- the target-specific `F_access`

That pair is what allows `Tr_simpleVar` to find the right frame at run time.

## Static Links

A static link is a hidden argument passed to nested functions. It points to the frame of the callee's lexical parent.

For a function declared inside `outer`, the static link passed to that function should point to `outer`'s current frame. Then the nested function can access variables declared in `outer`.

Common convention for this project:

- add the static link as the first formal of every non-outermost frame
- make it an escaping formal, because it must live somewhere addressable
- hide it from semantic binding of user parameters, or explicitly skip it in `semant.c`

The plan recommends hiding it in `Tr_formals(level)`: the frame stores the static link, but `Tr_formals` returns only user-visible parameters. That keeps `semant.c` parameter binding simpler.

## Following Static Links

Suppose a variable was declared at level `acc->level`, but it is used from current level `lev`.

If both levels are the same, the variable is in the current frame:

```text
frame pointer = TEMP(F_FP())
```

If the variable is from an outer level, repeatedly load the static link from the current frame:

```text
fp0 = TEMP(F_FP())                 // current frame
fp1 = MEM(fp0 + static_link_offset) // parent frame
fp2 = MEM(fp1 + static_link_offset) // grandparent frame
...
```

Stop when the frame pointer expression refers to the declaration level. Then call:

```c
F_Exp(acc->access, computedFramePointer)
```

This is why `Tr_access` must remember the declaration level.

## Function Calls and Static Link Arguments

When translating a call, semantic analysis passes:

- `cur`: current caller level
- `lev`: callee level

The static link argument should be the frame pointer for `lev->parent`, computed from `cur`.

Cases:

- Calling a function declared at the current level's child: pass current frame pointer.
- Calling a sibling nested function: follow current static link to the shared parent, then pass that.
- Calling a function in an outer scope: follow enough static links to reach the callee's parent.
- Calling outermost/runtime functions: no meaningful static link is needed, or you can use a placeholder only if your convention expects one for every call.

Keep one consistent convention between:

- `Tr_newLevel`
- `Tr_formals`
- `Tr_simpleVar`
- `Tr_callExp`
- `semant.c` parameter binding

## Expression Wrappers: `Ex`, `Nx`, `Cx`

Appel uses three wrappers because not every source expression is best represented the same way in IR:

- `Ex`: expression with a value
- `Nx`: statement with no value
- `Cx`: conditional expression represented as a branch with patchable labels

Examples:

- `5 + 2` is naturally `Ex`.
- `x := 3` is naturally `Nx`.
- `x < y` is naturally `Cx`.

The helper functions convert between forms:

- `unEx(e)`: produce a value expression.
- `unNx(e)`: produce a statement.
- `unCx(e)`: produce conditional branch structure.

For a `Cx`, the true and false labels may not be known when the conditional is first created. That is why `patchList`, `doPatch`, and `joinPatch` exist. A `T_Cjump` stores label fields, and patch lists store pointers to those label fields so later code can fill them in.

## Control Flow Translation

Conditionals and loops are mostly labels, conditional jumps, and sequencing.

For `if test then a else b` with a value:

- convert `test` to `Cx`
- patch true branch to the then label
- patch false branch to the else label
- move the selected branch value into a fresh temp
- jump both branches to a join label
- final expression is that temp

For `while test do body`:

- create test, body, and done labels
- jump to body when test succeeds
- jump to done when test fails
- body jumps back to test
- `break` jumps to the loop's done label

## Records and Arrays

Records and arrays are heap objects. The IR usually calls runtime helpers:

- `malloc(bytes)` for records
- `initArray(size, init)` for arrays

Field and subscript access are address calculations:

```text
record field i: MEM(recordBase + i * wordSize)
array element i: MEM(arrayBase + i * wordSize)
```

This scaffold does not require nil checks or bounds checks at first.

## Procedure Fragments

The translator gathers fragments:

- string fragments: labels plus string literals
- procedure fragments: function body IR plus frame

`Tr_procEntryExit` should eventually:

- convert the function body into a statement
- if the function returns a value, move that value into `F_RV()`
- call `F_procEntryExit1`
- append an `F_ProcFrag`

`F_procEntryExit1` is target-specific. Early on it may be simple, but later it is the place for moves between incoming argument locations and formal accesses, plus callee-save handling if you implement it.

## Practical Implementation Order

Do not begin with control flow or calls. The dependency order is:

1. Make `F_access` constructors and frame-local offset state correct.
2. Make `F_newFrame`, `F_allocLocal`, and `F_Exp` produce meaningful access locations.
3. Make `Tr_newLevel`, `Tr_formals`, and `Tr_allocLocal` agree about static links.
4. Make `Tr_simpleVar` follow static links.
5. Make expression wrappers reliable.
6. Then handle operators, records, arrays, control flow, calls, fragments, and escape analysis.

After every step, run `make clean && make` and inspect a tiny Tiger program's printed IR. The printed IR is the feedback loop for this chapter.

## Common Mistakes

- Reversing formal order when building `F_formals`.
- Adding a static link to the frame but also binding it as a user parameter.
- Using the current frame pointer for every variable, including variables declared in outer functions.
- Allocating every local at offset `0`.
- Forgetting that `F_Exp` needs the frame pointer expression for the frame where the variable was declared.
- Treating caller/callee nesting as the same thing as lexical parent/child nesting.
- Returning `Nx` from a let expression whose body has a value.
- Implementing escape analysis but continuing to pass `TRUE` for every variable and parameter.
- Letting `Tr_callExp` build an argument list that drops either the static link or the first user argument.

## Minimal Mental Model

When stuck, use this checklist:

1. Where was the variable declared? That is its `Tr_access->level`.
2. Where am I using it from? That is the current `Tr_level`.
3. If those differ, how many static links do I follow?
4. Once I have the correct frame pointer expression, what does `F_Exp` generate?
5. Does the variable escape? If yes, it needs a frame location. If no, it can be a temp.

