# Pseudo Is All You Need

> A pseudocode compiler with a dual-backend architecture — and an adaptive layer that learns how *you* actually type, so it can tell the difference between a real syntax error and just your handwriting.

---

## Overview

Most student compiler projects stop at "parses a toy grammar, interprets it." This one is built around two real differentiators:

1. **Dual-backend execution** — a single shared intermediate representation (IR) that can be run two ways: through a **bytecode VM** (fast to start, good for iteration and debugging) or compiled **ahead-of-time to C** (for genuine performance when you need it). Same front-end, same IR, two very different execution strategies — a decision most toy compilers never have to make.

2. **Adaptive input understanding** — an ML layer that watches how a specific user types, learns their personal patterns (common typos, abbreviations, habitual shorthand), and uses that — combined with the surrounding code context — to intelligently correct or complete ambiguous input, instead of just throwing a syntax error at the first thing that doesn't parse.

---

## Architecture

```
                    ┌───────────────────────┐
   Raw keystrokes → │  Adaptive Input Layer  │ → Cleaned / corrected
                    │   (ML-assisted)        │      source text
                    └───────────┬───────────┘
                                │
                                ▼
                        ┌───────────────┐
                        │     Lexer      │
                        └───────┬───────┘
                                ▼
                        ┌───────────────┐
                        │     Parser     │
                        └───────┬───────┘
                                ▼
                        ┌───────────────┐
                        │  Common IR     │
                        └───┬───────┬───┘
                            │       │
                  ┌─────────┘       └─────────┐
                  ▼                           ▼
          ┌───────────────┐           ┌───────────────┐
          │  Bytecode VM   │           │  AOT → C       │
          │  (interpreter) │           │  (compiled)    │
          └───────────────┘           └───────────────┘
```

---

## Core Compiler

### Phase 1 — Grammar & front-end
- [ ] Grammar design (`docs/grammar.md`) — start here, before any code
- [ ] Lexer
- [ ] Parser → AST
- [ ] AST → common IR lowering

### Phase 2 — Bytecode VM backend
- [ ] IR → bytecode compiler
- [ ] Stack-based (or register-based) VM interpreter
- [ ] Basic standard library / built-ins

### Phase 3 — AOT-to-C backend
- [ ] IR → C code generator
- [ ] Build pipeline (invoke system C compiler, link runtime support)
- [ ] Shared runtime support library used by both backends

### Phase 4 — Tooling & polish
- [ ] Error messages with source location + suggestions
- [ ] CLI (`run`, `build`, `repl`)
- [ ] Test suite across both backends (same programs, same expected output)

---

## Adaptive Input Understanding (the new layer)

The core idea: **most "syntax errors" from a real user aren't random** — they're a small, personal, repeating set of habits. Someone might reliably type `fr` instead of `for`, drop closing brackets in a specific pattern, or use a shorthand keyword their own way. A generic compiler treats all of that as noise. This layer treats it as signal.

### How it works

1. **Logging layer** — every keystroke sequence is logged alongside the final version the user actually commits (i.e., what they kept after any manual correction, or accepted from a suggestion). This raw-input → committed-output pair is the training signal.

2. **Pattern recognition** — a lightweight model (starting point: edit-distance + n-gram context matching; upgrade path: a small sequence model fine-tuned per user) learns the *individual's* recurring typos and shorthand, not just generic dictionary corrections.

3. **Context-aware rectification** — when input is ambiguous or doesn't parse cleanly, the system checks: what's valid at this exact point in the grammar (not just "closest known word" spell-check), combined with what this specific user has historically meant when they typed something similar. It suggests or auto-corrects based on both.

4. **Feedback loop** — every time a suggestion is accepted or overridden, that outcome feeds back into the model, so it keeps adapting to the user over time rather than staying static.

### Why this is harder (and more interesting) than autocomplete

Standard IDE autocomplete matches against a fixed dictionary of valid tokens. This layer is trying to model *intent* — using both grammatical context (what's syntactically possible right here) and personal typing history (what this person tends to mean when they type something that doesn't quite parse). That combination is the actual research-flavored part of this project.

### Planned components
- [ ] Keystroke + commit logging pipeline (local, per-user, privacy-respecting by default)
- [ ] Baseline rectifier: edit-distance against valid tokens in current grammar context
- [ ] Personalized model: learns from a given user's log history over time
- [ ] Suggestion UI (inline, non-blocking — never forces a correction, always offers one)
- [ ] Evaluation: does personalization actually reduce real syntax errors over time, measured against the logs

---

## Tech Stack

| Component | Choice | Why |
|---|---|---|
| Lexer/Parser | Hand-written (or a parser generator like ANTLR if scope allows) | Full control over error recovery and IR lowering |
| Bytecode VM | Custom, written in the same language as the compiler | Keeps the whole toolchain self-contained |
| AOT backend | Emit C, shell out to `gcc`/`clang` | Reuses a mature, optimizing backend instead of writing your own codegen |
| Adaptive input layer | Python (prototyping) → could be ported closer to the core later | Fast iteration on the ML side; doesn't need to be as fast as the compiler itself |
| Logging store | SQLite | Simple, local, no server dependency |

---

## Project Status

**Planning stage.** Grammar design (`docs/grammar.md`) is the confirmed starting point, before any implementation begins.

This is a genuinely ambitious scope — three real subsystems (dual-backend compiler, adaptive ML input layer, and the tooling connecting them). Realistic sequencing: get the core compiler working end-to-end on both backends *first*, with a boring, no-frills text input. Only add the adaptive input layer once the compiler itself is solid — building the intelligent front-end before the thing it feeds into actually works is a common way these projects stall.

---

## Why This Project

Most student compilers are built once, run once, and demonstrate that a grammar can be parsed. This one is trying to be a system someone would actually want to keep typing into — where the tool adapts to the person, instead of the person being expected to type perfectly for the tool. The dual-backend design proves you understand execution strategy trade-offs; the adaptive layer proves you can connect a genuinely useful ML idea to a real, everyday interaction, not just a benchmark.

---

## Getting Started

Coming soon — once the grammar and Phase 1 front-end are in place.

---

## License

TBD — likely MIT once the first working version exists.
