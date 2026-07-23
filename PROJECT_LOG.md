# Project Log

Running record of what actually changed in this project, in order. `PROJECT_PLAN.md` is the plan; this is the history. Newest entries at the top.

---

## 2026-07-23 — Phase 0 complete

- Rewrote `docs/grammar.md`: it previously only had the six statement forms with no formal grammar, no lexical token list, and no operator precedence table — even though `README.md`/the design guide already claimed AND/OR/NOT precedence was "covered." Added: full lexical token table, EBNF-style formal grammar (one rule per parser function to come), precedence table, comments syntax, I/O statements, semantic rules, and a "design choices" section explaining *why* each concrete syntax choice was made (case-insensitive keywords, `=` vs `==`, no semicolons, single-line comments only).
- Added `docs/AST_SPEC.md` — AST node shapes for every grammar rule, plus memory-ownership notes, written before any parser code exists.
- Added `docs/IR_SPEC.md` — shared IR instruction set sketch, an AST→IR lowering table, and explicitly flagged open questions (temp-slot allocation, type tagging, string representation) to resolve at the start of Phase 2 rather than silently deciding now.
- Edited `docs/COMPILER_DESIGN_GUIDE.md`: it was generated independently and numbered its phases 1–4, which didn't match `PROJECT_PLAN.md`'s 0–6 numbering — renumbered to match, added a Phase 0 section, and added a note that AI-assisted code must still be commented for review (per the "AI reviews, doesn't generate" philosophy already in the file).
- Created repo scaffolding: empty `src/`, `include/`, `tests/` directories, ready for Phase 1.
- Marked Phase 0 complete in `PROJECT_PLAN.md`.

## 2026-07-23 — Project plan & workflow

- Added `PROJECT_PLAN.md`: phase-by-phase workflow (Phase 0 design → Phase 1 frontend → Phase 2 shared IR → Phase 3 bytecode VM → Phase 4 AOT-to-C → Phase 5 error handling/tooling → Phase 6 deferred ML layer), with exit criteria per phase and a per-phase working workflow.
- Added this file (`PROJECT_LOG.md`) to track changes going forward.
- Re-indexed the repo in the codebase knowledge graph after the doc cleanup below.

## 2026-07-23 — README rewrite

- Rewrote `README.md` from scratch, merging content that used to be spread across the now-deleted `pseudo-compiler-README.md` draft and `pseudocode+ml` proposal dump: dual-backend rationale, full compiler pipeline (added semantic analysis, which the old README was missing), phase table, error-handling spec, ML layer (Phase 6) design, tech stack table, repository layout, working philosophy, and course context (VIT Vellore, original timeline).
- Fixed a stale "License: TBD" line — pointed it at the real `LICENSE` file (MIT) instead.

## 2026-07-23 — Docs cleanup

- Deleted `pseudo-compiler-README.md` — outdated duplicate draft of `README.md`.
- Deleted `pseudocode+ml` — broken file containing stray tool artifacts (`<task_progress>`, `</write_to_file>` tags) wrapping a plaintext copy of content already in `Pseudo_Is_All_You_Need_Project_Proposal.docx`.
- Deleted `.~lock.README.md#` and `docs/.~lock.grammar.md#` — LibreOffice lock files, not project content.
- Renamed `compiler design guide for lesser ui usage ` (bad filename, trailing space) → `docs/COMPILER_DESIGN_GUIDE.md`.

## Earlier history (from git log, pre-cleanup)

- `e12f57b` — Added formal grammar spec: control flow, functions, arrays, I/O, comments (`docs/grammar.md`).
- `9658768` — Fixed a syntax error in the grammar doc.
- `3a660c9` — Deleted a duplicate file.
- `66b1a50` — Removed AI-assisted tooling artifacts, kept the grammar spec.
- `5d41cc0` — Prep notes for using local models on this project.
- Initial commit + `.gitignore` setup.
