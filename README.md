# Pseudo Is All You Need

A compiler for pseudocode. Yes, the stuff from your DSA textbook that you've
never actually been able to run — this compiles and executes it.

## What it does

- Lexes and parses standard textbook pseudocode (`IF/THEN/ELSE`, `FOR`,
  `WHILE`, `REPEAT/UNTIL`, `FUNCTION`, arrays)
- Compiles it down to a custom bytecode via a hand-written recursive descent
  parser and AST
- Runs the bytecode on a stack-based virtual machine
- (Planned) Also emits standalone C source from the same IR, so pseudocode
  can be AOT-compiled into a native binary with zero runtime dependency

## Why

Built as a compiler design course project — the goal was to actually
understand every stage of a real compiler pipeline (lexer -> parser -> AST
-> semantic analysis -> codegen -> VM), not just call a library that does it.

## Architecture

```
source (.pseudo)
      |
   Lexer          -> tokens
      |
   Parser         -> AST
      |
Semantic Analysis -> scope/type checked AST
      |
   Codegen        -> bytecode
      |
      VM          -> execution
```

## Status

Work in progress. See `docs/grammar.md` for the language spec.

## Build

```bash
make
./pcc examples/hello.pseudo
```

## License

MIT
