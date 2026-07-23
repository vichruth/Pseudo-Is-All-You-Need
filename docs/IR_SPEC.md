# Shared IR (sketch)

Pre-code sketch of the intermediate representation that sits between the AST (`docs/AST_SPEC.md`) and the two backends (bytecode VM, AOT-to-C). Written on paper first, per `PROJECT_PLAN.md` Phase 2 — this is called out everywhere as "hardest to redo later if wrong," so it gets a design pass before any lowering code exists.

**This is a sketch, not a final instruction encoding.** The goal here is just to fix the *shape* — a flat, linear, three-address-ish instruction list — so both backends have one common thing to consume.

---

## Why a linear instruction list, not a second tree

The AST is a tree because that's the natural shape of nested `if`/`for`/`while` source syntax. The IR is a **flat list of instructions with explicit labels and jumps** instead, because:

- A stack-based bytecode VM (Phase 3) executes a linear instruction stream directly — a tree would need re-flattening at VM-build time anyway.
- A C code generator (Phase 4) can emit structured C control flow (`if`/`for`/`while`) from a flat IR just as easily as from a tree, using the label/jump structure to reconstruct blocks — but the reverse (going from a fixed tree shape to two very different backends) forces more backend-specific tree-walking logic into both.
- Flattening control flow into jumps once, in the AST→IR lowering step, means neither backend has to re-implement "how does an `if` node's control flow work" — it's already resolved into explicit `jump_if_false` / `label` pairs by the time either backend sees it.

## Instruction set (sketch)

Three-address-style: each instruction produces at most one result, referencing operands by **temporary slot** (`t0`, `t1`, ...) or **local variable name**.

```
IR_CONST      dst, value           # dst = literal (number/string/bool)
IR_LOAD       dst, var_name        # dst = var_name
IR_STORE      var_name, src        # var_name = src
IR_LOAD_IDX   dst, arr, idx        # dst = arr[idx]
IR_STORE_IDX  arr, idx, src        # arr[idx] = src
IR_LEN        dst, arr             # dst = arr.length

IR_BINOP      dst, op, lhs, rhs    # dst = lhs <op> rhs   (op: ADD/SUB/MUL/DIV/MOD/
                                   #                        EQ/NEQ/LT/GT/LTE/GTE/AND/OR)
IR_UNOP       dst, op, src         # dst = <op> src        (op: NEG/NOT)

IR_LABEL      name                 # jump target, no-op at runtime
IR_JUMP       label                # unconditional jump
IR_JUMP_IF_FALSE  cond, label       # jump if cond is falsy

IR_CALL       dst, func_name, args # dst = func_name(args...); dst may be unused
IR_RETURN     value                # return value (value may be absent)

IR_INPUT      dst                  # dst = read from stdin
IR_OUTPUT     src                  # print src

IR_FUNC_BEGIN name, params         # start of a function body
IR_FUNC_END                        # end of a function body
```

## Lowering sketch (AST node → IR)

| AST node | Lowers to |
| --- | --- |
| `If` | `<condition>` → `IR_JUMP_IF_FALSE else_label` → then-body → `IR_JUMP end_label` → `IR_LABEL else_label` → else-body → `IR_LABEL end_label` |
| `For` | init `IR_STORE var, start` → `IR_LABEL loop_start` → bounds check `IR_JUMP_IF_FALSE loop_end` → body → increment → `IR_JUMP loop_start` → `IR_LABEL loop_end` |
| `While` | `IR_LABEL loop_start` → `<condition>` → `IR_JUMP_IF_FALSE loop_end` → body → `IR_JUMP loop_start` → `IR_LABEL loop_end` |
| `Repeat` | `IR_LABEL loop_start` → body → `<condition>` → `IR_JUMP_IF_FALSE loop_start` *(note: inverted vs. while — repeat exits when true)* |
| `FunctionDecl` | `IR_FUNC_BEGIN` → lowered body → implicit `IR_RETURN` (no value) if no explicit return → `IR_FUNC_END` |
| `BinaryOp` / `UnaryOp` | `IR_BINOP` / `IR_UNOP` into a fresh temp slot |
| `Index` / `Assign` to indexed lvalue | `IR_LOAD_IDX` / `IR_STORE_IDX` |
| `Member` (`.length`) | `IR_LEN` |
| `Call` | `IR_CALL` |
| `Input` / `Output` | `IR_INPUT` / `IR_OUTPUT` |

## Open questions to resolve before Phase 2 coding starts

These are flagged deliberately rather than pre-decided, since the design guide's philosophy is "decide explicitly, in writing, before coding" — but a couple of these have real tradeoffs worth discussing together rather than silently picking one:

- **Temp slot allocation**: infinite virtual temps (simplest to generate, VM assigns registers/stack slots later) vs. a fixed small register set. Leaning toward infinite virtual temps for the IR itself — resolving them to real VM stack slots or C variables is a backend concern, not an IR concern.
- **Type info in the IR**: does `IR_BINOP ADD` need a type tag (int vs. float add), or does the AOT-to-C backend just emit `double` everywhere and let C handle it? Affects how much semantic analysis needs to annotate onto the AST before lowering.
- **String representation**: IR-level strings as a distinct value kind, or deferred entirely to backend-specific string handling? Affects `IR_CONST` and how `IR_OUTPUT` needs to dispatch on value kind.

Resolve these at the start of Phase 2, once Phase 1 is done and the AST's actual shape (including whatever semantic analysis annotates onto it) is settled.
