/*
 * token.h — token kinds and the Token value type.
 *
 * Adapted from Robert Nystrom's "Crafting Interpreters" clox scanner
 * (https://github.com/munificent/craftinginterpreters, c/scanner.h),
 * Copyright (c) 2015 Robert Nystrom, MIT licensed. The token *set* below is
 * this project's own, derived from docs/grammar.md's lexical grammar table;
 * what is borrowed is the Token struct shape (non-owning start/length slice
 * into the source buffer).
 */

#ifndef PSEUDO_TOKEN_H
#define PSEUDO_TOKEN_H

/*
 * Every token kind the lexer can emit. One entry per row of the lexical
 * grammar table in docs/grammar.md, in the same order, so the two can be
 * diffed by eye when the grammar changes.
 */
typedef enum {
    /* Control flow keywords */
    T_IF, T_THEN, T_ELSE, T_ENDIF,
    T_FOR, T_TO, T_DO, T_ENDFOR,
    T_WHILE, T_ENDWHILE,
    T_REPEAT, T_UNTIL,

    /* Function keywords */
    T_FUNCTION, T_ENDFUNCTION, T_RETURN,

    /* I/O keywords */
    T_INPUT, T_OUTPUT,

    /* Logical operator keywords (words, not symbols — see docs/grammar.md) */
    T_AND, T_OR, T_NOT,

    /* Boolean literal keywords */
    T_TRUE, T_FALSE,

    /* Literals and names */
    T_IDENT,      /* total, array_name, i        */
    T_NUMBER,     /* 42, 3.14 — both lexed alike */
    T_STRING,     /* "hello"                     */

    /* Arithmetic operators */
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_MOD,

    /* Assignment */
    T_ASSIGN,     /* =  */

    /* Comparison operators */
    T_EQ, T_NEQ, T_LT, T_GT, T_LTE, T_GTE,   /* == != < > <= >= */

    /* Grouping and punctuation */
    T_LPAREN, T_RPAREN,       /* ( ) */
    T_LBRACKET, T_RBRACKET,   /* [ ] */
    T_COMMA,                  /* ,   */
    T_DOT,                    /* .   — member access, e.g. arr.length */

    /*
     * T_ERROR is not a grammar token. The lexer emits it instead of aborting
     * so that one lexing pass can surface several problems (the
     * "never stop at the first error" rule in README.md). Its `start`/`length`
     * point at a static message string, NOT into the source buffer — see the
     * ownership note in lexer.h.
     */
    T_ERROR,

    T_EOF
} TokenType;

/*
 * A Token is a plain value, copied by assignment. It owns nothing.
 *
 * `start` points directly into the source buffer held by the Lexer; `length`
 * is how many bytes of it this token covers. There is no NUL terminator at
 * start[length], so this is NOT a C string — always print it with the
 * "%.*s" precision form, never with %s.
 *
 * Consequence: the source buffer must outlive every Token derived from it.
 * The parser copies (strdup's) any text it needs to keep into AST nodes and
 * never stores a Token beyond the statement it is parsing — see the ownership
 * notes in docs/AST_SPEC.md.
 */
typedef struct {
    TokenType type;
    const char *start;   /* non-owning slice into the source buffer */
    int length;
    int line;            /* 1-based */
    int col;             /* 1-based, counted in bytes from start of line */
} Token;

/* Human-readable name of a token kind ("T_IF", "T_IDENT", ...).
 * Used by the --dump-tokens driver and by parser error messages. */
const char *token_type_name(TokenType type);

#endif /* PSEUDO_TOKEN_H */
