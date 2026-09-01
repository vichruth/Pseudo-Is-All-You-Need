/*
 * lexer.h — on-demand lexer for the pseudocode language.
 *
 * Adapted from Robert Nystrom's "Crafting Interpreters" clox scanner
 * (https://github.com/munificent/craftinginterpreters, c/scanner.h),
 * Copyright (c) 2015 Robert Nystrom, MIT licensed.
 *
 * Deliberate changes from the original:
 *   - clox uses a single file-scope `Scanner scanner` global. This uses an
 *     explicit `Lexer` struct passed to every call, so tests can lex several
 *     sources without cross-contamination.
 *   - clox tracks `line` only. This tracks `line` and `col`, because
 *     docs/AST_SPEC.md requires both on every AST node for E2xx/E3xx errors.
 *
 * ---------------------------------------------------------------------------
 * DESIGN DECISIONS (locked here before the parser is written, per
 * docs/COMPILER_DESIGN_GUIDE.md "decide explicitly, in writing, before coding")
 * ---------------------------------------------------------------------------
 *
 * 1. TOKEN OWNERSHIP — nobody allocates, nobody frees.
 *    Tokens are values holding a non-owning (start, length) slice into the
 *    caller's source buffer. There is no malloc in the lexer at all, so there
 *    is nothing to leak and nothing to double-free. The cost is that the
 *    source buffer must stay alive for as long as any Token does; the driver
 *    keeps it alive for the whole compile, which it must do anyway to print
 *    the offending source line in error messages.
 *
 * 2. KEYWORD CASE-INSENSITIVITY — copy-and-fold into a small stack buffer.
 *    docs/grammar.md makes keywords case-insensitive (IF / if / If are one
 *    token). Because tokens are non-owning slices, the lexer cannot lowercase
 *    the source in place. Instead, a candidate word shorter than
 *    LEXER_MAX_KEYWORD_LEN is folded to lowercase in a stack buffer and looked
 *    up in a table. Words longer than that cannot be keywords ("endfunction",
 *    11 chars, is the longest) so they skip the lookup and are identifiers
 *    immediately. No allocation, and the length check is also the bounds check.
 *
 * 3. LOOKAHEAD — two characters, zero tokens.
 *    The lexer needs at most 2 chars of lookahead (`peek`/`peek_next`), which
 *    is enough for `//` vs `/`, `<=` vs `<`, and the `.` in `3.14` vs the `.`
 *    in `arr.length`. It deliberately does NOT buffer tokens: the one-token
 *    peek the parser needs for the assignment-vs-call ambiguity
 *    (docs/grammar.md, "Known ambiguities") is the parser's job to buffer, not
 *    the lexer's. Keeping the lexer purely on-demand means it has no state
 *    beyond a position, which is what makes decision 1 safe.
 *
 * 4. ERRORS — emitted as T_ERROR tokens, never printed, never fatal.
 *    lexer_scan_token() returns a T_ERROR token and keeps going, so a single
 *    pass reports every bad character rather than only the first. Phase 5
 *    collects these into the .errlog format described in README.md; until
 *    then the driver prints them.
 */

#ifndef PSEUDO_LEXER_H
#define PSEUDO_LEXER_H

#include "token.h"

/* "endfunction" is the longest keyword at 11 chars; 16 leaves room to add
 * keywords later without revisiting the fold buffer. */
#define LEXER_MAX_KEYWORD_LEN 16

typedef struct {
    const char *start;        /* first char of the token being scanned  */
    const char *current;      /* next char to consume                   */
    const char *line_start;   /* first char of the current line, for col */
    int line;                 /* 1-based                                */
} Lexer;

/*
 * Point `lexer` at a NUL-terminated source buffer. Does not copy or take
 * ownership of `source`; the caller must keep it alive longer than the lexer
 * and longer than any Token the lexer produced.
 */
void lexer_init(Lexer *lexer, const char *source);

/*
 * Scan and return the next token. At end of input it returns T_EOF and will
 * keep returning T_EOF on every subsequent call, so a caller loop that stops
 * on T_EOF cannot run off the end of the buffer.
 */
Token lexer_scan_token(Lexer *lexer);

#endif /* PSEUDO_LEXER_H */
