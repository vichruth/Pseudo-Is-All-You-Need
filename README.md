# Pseudo Is All You Need

A dual-backend pseudocode compiler — write once in plain pseudocode, run it two ways.

## Overview

Algorithms are usually designed and taught in pseudocode (the DAA-style notation), then hand-translated into a real language to actually run. This project removes that translation step: pseudocode is the source language itself, compiled through one shared core into two different execution targets.

- **Bytecode VM** — interpreted, fast to iterate, good for testing and debugging. 

- **AOT-to-C backend** — compiles all the way down to a native C binary, for real performance. 

Both backends are generated from the **same intermediate representation (IR)** — nothing is written twice, and both stay behaviorally consistent by construction.


## Architecture / Pipeline

```
` Pseudocode source`

`        │`

`        ▼`

`   ┌───────────┐`

`   │ Frontend  │   lexer → parser → AST`

`   └─────┬─────┘`

`         │`

`         ▼`

`   ┌───────────┐`

`   │ Shared IR │   one core, two backends`

`   └─────┬─────┘`

`         │`

`    ┌────┴─────┐`

`    ▼          ▼`

`┌────────┐ ┌──────────────┐`

`│Bytecode│ │  AOT-to-C    │`

`│  VM    │ │  backend     │`

`└────────┘ └──────────────┘`

`interpreted   compiles to`

` fast loop    native binary`
```

**Why a shared IR, not two independent pipelines:** the two backends have opposite goals (iteration speed vs. execution speed), but the semantics of the language must be identical either way — an `if` that behaves one way in the VM and another way when compiled would make the whole tool untrustworthy. A single IR is the thing that guarantees both backends agree, and it means every language feature is implemented once, not twice.


## Project phases

| **Phase** | **Scope** | **Status** |
| :-: | :-: | :-: |
| 0 | Design — this README, architecture, IR spec | Done |
| 1 | Frontend — lexer, parser, AST | Not started |
| 2 | Shared IR design + construction from AST | Not started |
| 3 | Bytecode VM backend | Not started |
| 4 | AOT-to-C backend | Not started |
| 5 | Error handling & logging system (see below) | Not started |
| 6 | **ML layer — Adaptive Input Understanding** (see below) | Deferred — starts only after Phase 1–5 are solid |

Phase 6 is deliberately last. It depends on real error-log data produced by Phases 1–5, and building it before the core compiler is stable would mean training against a moving target.


## Error handling & error logs

Every compiler phase (lexer, parser, IR construction, codegen) can fail, and a pseudocode compiler's whole value proposition is being *helpful* when it does — this project treats error reporting as a first-class feature, not an afterthought bolted on at the end.

### Error categories

| **Code range** | **Phase** | **Example** |
| :-: | :-: | :-: |
| `E0xx` | Lexical | Unrecognized token, unterminated string |
| `E1xx` | Syntax | Missing keyword, unbalanced block, malformed expression |
| `E2xx` | Semantic | Undefined variable, type mismatch, unreachable code |
| `E3xx` | Codegen | IR lowering failure, backend-specific limitation hit |

### Error log format

Every error is emitted in a single structured line, so it's both human-readable in the terminal and machine-parseable later (this format is also what feeds the ML layer in Phase 6):

```
`\[E103\] \[Parser\] line 14, col 8: expected 'THEN' after IF condition`

`\[E201\] \[Semantic\] line 22, col 3: variable 'total' used before assignment`

`\[E301\] \[Codegen\] line 40, col 1: recursive call unsupported in AOT-to-C v1`
```

### Design principles for error handling

- **Never stop at the first error.** Collect as many real errors as possible in one pass (error recovery / synchronization points in the parser) so the user sees the full picture in one run instead of fixing errors one at a time. 

- **Point at the exact token**, not just the line — line + column, always. 

- **Every session's errors are logged to a `.errlog` file**, not just printed to the terminal. This is intentional: the log is a durable artifact, and it is the raw training data for Phase 6. 

- **Error messages describe what was expected, not just what was found** — e.g. "expected 'THEN' after IF condition" rather than a bare "syntax error". 


## ML layer — Adaptive Input Understanding (Phase 6, deferred)

### The idea

Standard error correction (a dictionary spell-checker, or a fixed set of "did you mean" rules) only catches generic mistakes. This layer instead learns **how a specific person actually types and typos**, and combines that with **grammar validity at the exact parse position** to propose corrections — context-aware rectification, not lookup-table correction.

### How it works

1. **Keystroke logging.** While editing, raw keystrokes are logged alongside the code the user eventually commits (saves/compiles). The diff between "what was typed along the way" and "what was finally kept" is the signal — it captures a user's *own* typo patterns (e.g. consistently typing `wend` for `end`, or transposing two characters in `THEN`). 

2. **Personal typo model.** Over time, this builds a small per-user model of likely substitutions — not a generic dictionary, but *this person's* habits. 

3. **Context-aware rectification.** When a token fails to parse, instead of immediately raising a hard `E1xx`, the ML layer proposes candidate corrections, ranked by: 

   - edit distance to the failing token, 

   - **grammar validity** — does substituting this candidate actually produce a valid parse at this exact position in the AST?, 

   - the user's personal typing-history frequency for that substitution. 

4. **Logged alongside the original error**, not instead of it — every suggestion is recorded as `\[E1xx\] ... → suggested correction (confidence: X%)` in the same `.errlog` format, so the correction layer is auditable, not a silent black box. 

### Why this depends on Phases 1–5

The training data for this layer *is* the error log format described above. Phase 6 does not start until the core compiler has been in real use long enough to produce a genuine corpus of real errors and real corrections — this is a deliberate sequencing decision, not a scope cut. Building the ML layer first would mean training on synthetic or too-small data.


## Tech stack

- **Core compiler (Phases 1–5):** C — both the bytecode VM and the AOT-to-C backend are implemented in C, sharing the same IR data structures. 

- **AOT backend output:** generated C source, compiled via the system's `cc`/`gcc`. 

- **ML layer (Phase 6):** likely a lightweight, per-user model (not a large neural net — personalization at this scale doesn't need one) trained on the compiler's own `.errlog` corpus. 


## Status

Currently in Phase 0 (design). Architecture and error-handling design covered in this README; Phase 1 (frontend) is the next concrete implementation step.

## License

TBD.

