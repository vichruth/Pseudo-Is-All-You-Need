/*
 * lexer.c — on-demand lexer for the pseudocode language.
 *
 * Adapted from Robert Nystrom's "Crafting Interpreters" clox scanner
 * (https://github.com/munificent/craftinginterpreters, c/scanner.c),
 * Copyright (c) 2015 Robert Nystrom, MIT licensed.
 *
 * Retained from the original: the start/current pointer pair, the
 * advance/peek/match helpers, skip_whitespace's switch loop, and the shape of
 * number()/string()/identifier().
 *
 * Rewritten for this project: the whole token set (docs/grammar.md), the
 * keyword lookup (clox's per-character trie is case-sensitive by construction
 * and cannot express this language's case-insensitive keywords), column
 * tracking, and the removal of the file-scope Scanner global.
 *
 * See lexer.h for the four design decisions this file implements.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "token.h"

/* ------------------------------------------------------------------------ */
/* Token kind names (for --dump-tokens and parser error messages)           */
/* ------------------------------------------------------------------------ */

/*
 * Indexed by TokenType, so this array's order must match the enum in token.h
 * exactly. The _Static_assert below catches the most likely mistake — adding
 * a token to the enum and forgetting to add its name here.
 */
static const char *const TOKEN_NAMES[] = {
    "T_IF", "T_THEN", "T_ELSE", "T_ENDIF",
    "T_FOR", "T_TO", "T_DO", "T_ENDFOR",
    "T_WHILE", "T_ENDWHILE",
    "T_REPEAT", "T_UNTIL",
    "T_FUNCTION", "T_ENDFUNCTION", "T_RETURN",
    "T_INPUT", "T_OUTPUT",
    "T_AND", "T_OR", "T_NOT",
    "T_TRUE", "T_FALSE",
    "T_IDENT", "T_NUMBER", "T_STRING",
    "T_PLUS", "T_MINUS", "T_STAR", "T_SLASH", "T_MOD",
    "T_ASSIGN",
    "T_EQ", "T_NEQ", "T_LT", "T_GT", "T_LTE", "T_GTE",
    "T_LPAREN", "T_RPAREN",
    "T_LBRACKET", "T_RBRACKET",
    "T_COMMA", "T_DOT",
    "T_ERROR", "T_EOF"
};

_Static_assert(sizeof(TOKEN_NAMES) / sizeof(TOKEN_NAMES[0]) == T_EOF + 1,
               "TOKEN_NAMES is out of sync with the TokenType enum in token.h");

const char *token_type_name(TokenType type) {
    return TOKEN_NAMES[type];
}

/* ------------------------------------------------------------------------ */
/* Keyword table                                                             */
/* ------------------------------------------------------------------------ */

/*
 * All keywords are stored lowercase; lookup folds the candidate word to
 * lowercase before comparing, which is what makes keywords case-insensitive
 * (docs/grammar.md, "Design choices").
 *
 * Linear search is deliberate: 22 entries, and this only runs once per
 * identifier-shaped word. A hash table here would be more code for no
 * measurable gain at this language's scale.
 */
static const struct {
    const char *word;
    TokenType type;
} KEYWORDS[] = {
    {"if", T_IF}, {"then", T_THEN}, {"else", T_ELSE}, {"endif", T_ENDIF},
    {"for", T_FOR}, {"to", T_TO}, {"do", T_DO}, {"endfor", T_ENDFOR},
    {"while", T_WHILE}, {"endwhile", T_ENDWHILE},
    {"repeat", T_REPEAT}, {"until", T_UNTIL},
    {"function", T_FUNCTION}, {"endfunction", T_ENDFUNCTION},
    {"return", T_RETURN},
    {"input", T_INPUT}, {"output", T_OUTPUT},
    {"and", T_AND}, {"or", T_OR}, {"not", T_NOT},
    {"true", T_TRUE}, {"false", T_FALSE},
};

#define KEYWORD_COUNT (sizeof(KEYWORDS) / sizeof(KEYWORDS[0]))

/* ------------------------------------------------------------------------ */
/* Character helpers                                                         */
/* ------------------------------------------------------------------------ */

/*
 * The ctype.h functions take an int whose value must be representable as
 * unsigned char (or EOF) — passing a plain char with the high bit set is
 * undefined behaviour. These wrappers do the cast once so no caller forgets.
 */
static int is_alpha(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_digit(char c) {
    return isdigit((unsigned char)c);
}

static int is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

/* ------------------------------------------------------------------------ */
/* Cursor primitives                                                         */
/* ------------------------------------------------------------------------ */

static int is_at_end(const Lexer *lexer) {
    return *lexer->current == '\0';
}

/* Consume one character and return it. */
static char advance(Lexer *lexer) {
    lexer->current++;
    return lexer->current[-1];
}

/* Next character without consuming it. Safe at end of input: the buffer is
 * NUL-terminated, so this returns '\0' there rather than reading past it. */
static char peek(const Lexer *lexer) {
    return *lexer->current;
}

/* The character after next. Guarded because peeking two past the NUL would
 * read out of bounds. */
static char peek_next(const Lexer *lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

/* Consume the next character only if it matches — used for the two-character
 * operators (`==`, `!=`, `<=`, `>=`). */
static int match(Lexer *lexer, char expected) {
    if (is_at_end(lexer)) return 0;
    if (*lexer->current != expected) return 0;
    lexer->current++;
    return 1;
}

/* Record that a newline was just consumed. Resetting line_start to the
 * character after it is what makes column numbers restart at 1 per line. */
static void bump_line(Lexer *lexer) {
    lexer->line++;
    lexer->line_start = lexer->current;
}

/* ------------------------------------------------------------------------ */
/* Token construction                                                        */
/* ------------------------------------------------------------------------ */

/*
 * Build a token covering everything from lexer->start to lexer->current.
 * Column is derived from where the token *started*, not where scanning
 * currently is, so a multi-character token points at its first character.
 */
static Token make_token(const Lexer *lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (int)(lexer->current - lexer->start);
    token.line = lexer->line;
    token.col = (int)(lexer->start - lexer->line_start) + 1;
    return token;
}

/*
 * Build an error token. Unlike every other token, `start` points at the
 * static `message` string rather than into the source buffer — the position
 * fields are what locate the problem in the source. Callers that print token
 * text generically still work, because the message is also a (start, length)
 * slice; it just happens to live elsewhere.
 */
static Token error_token(const Lexer *lexer, const char *message) {
    Token token;
    token.type = T_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer->line;
    token.col = (int)(lexer->start - lexer->line_start) + 1;
    return token;
}

/* ------------------------------------------------------------------------ */
/* Whitespace and comments                                                   */
/* ------------------------------------------------------------------------ */

/*
 * Consume whitespace and `//` comments. Neither becomes a token — the parser
 * never sees them (docs/grammar.md: "Whitespace and comments are not tokens").
 *
 * Newlines are consumed like any other whitespace because the language is
 * whitespace-insensitive: blocks end with keywords (endif, endfor, ...), not
 * with line breaks, so the parser has no use for a newline token.
 */
static void skip_whitespace(Lexer *lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;

            case '\n':
                advance(lexer);
                bump_line(lexer);
                break;

            case '/':
                /* Only `//` starts a comment; a lone `/` is division and must
                 * be left for scan_token to turn into T_SLASH. Deciding this
                 * needs the second character, which is the whole reason
                 * peek_next exists. */
                if (peek_next(lexer) == '/') {
                    while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                        advance(lexer);
                    }
                } else {
                    return;
                }
                break;

            default:
                return;
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Literal and identifier scanners                                           */
/* ------------------------------------------------------------------------ */

/*
 * Classify a just-scanned word as a keyword or T_IDENT.
 *
 * Words at or over LEXER_MAX_KEYWORD_LEN cannot be keywords, so they skip the
 * fold entirely — which doubles as the bounds check on the stack buffer.
 */
static TokenType identifier_type(const Lexer *lexer) {
    size_t length = (size_t)(lexer->current - lexer->start);
    char folded[LEXER_MAX_KEYWORD_LEN];

    if (length >= LEXER_MAX_KEYWORD_LEN) return T_IDENT;

    for (size_t i = 0; i < length; i++) {
        folded[i] = (char)tolower((unsigned char)lexer->start[i]);
    }
    folded[length] = '\0';

    for (size_t i = 0; i < KEYWORD_COUNT; i++) {
        if (strcmp(folded, KEYWORDS[i].word) == 0) return KEYWORDS[i].type;
    }
    return T_IDENT;
}

static Token identifier(Lexer *lexer) {
    while (is_alnum(peek(lexer))) advance(lexer);
    return make_token(lexer, identifier_type(lexer));
}

/*
 * Numbers: integers and floats share one token kind (docs/AST_SPEC.md stores
 * both as double; int-vs-float is a semantic-analysis concern, not a lexical
 * one).
 *
 * The `.` is only swallowed when a digit follows it. That single check is what
 * keeps `3.14` one token while leaving the `.` in `arr.length` — and in a
 * hypothetical `1.length` — as a separate T_DOT.
 */
static Token number(Lexer *lexer) {
    while (is_digit(peek(lexer))) advance(lexer);

    if (peek(lexer) == '.' && is_digit(peek_next(lexer))) {
        advance(lexer);  /* consume the '.' */
        while (is_digit(peek(lexer))) advance(lexer);
    }

    return make_token(lexer, T_NUMBER);
}

/*
 * Strings run to the closing quote. Unlike clox, a newline inside a string is
 * an error rather than a multi-line string: the grammar has no multi-line
 * string form, and treating a missing quote as "keep swallowing lines" turns
 * one typo into a cascade of bogus errors on every following line.
 *
 * No escape sequences in v1 — a `\` is an ordinary character. Adding them
 * later is a pure lexer change and breaks nothing already written.
 */
static Token string(Lexer *lexer) {
    while (peek(lexer) != '"' && !is_at_end(lexer)) {
        if (peek(lexer) == '\n') {
            return error_token(lexer, "unterminated string literal");
        }
        advance(lexer);
    }

    if (is_at_end(lexer)) {
        return error_token(lexer, "unterminated string literal");
    }

    advance(lexer);  /* the closing quote */
    return make_token(lexer, T_STRING);
}

/* ------------------------------------------------------------------------ */
/* Public interface                                                          */
/* ------------------------------------------------------------------------ */

void lexer_init(Lexer *lexer, const char *source) {
    lexer->start = source;
    lexer->current = source;
    lexer->line_start = source;
    lexer->line = 1;
}

Token lexer_scan_token(Lexer *lexer) {
    skip_whitespace(lexer);

    /* Everything from here is one token, so anchor start at the current
     * position — this is what make_token measures the token's extent from. */
    lexer->start = lexer->current;

    if (is_at_end(lexer)) return make_token(lexer, T_EOF);

    char c = advance(lexer);

    if (is_alpha(c)) return identifier(lexer);
    if (is_digit(c)) return number(lexer);

    switch (c) {
        /* Single-character tokens */
        case '(': return make_token(lexer, T_LPAREN);
        case ')': return make_token(lexer, T_RPAREN);
        case '[': return make_token(lexer, T_LBRACKET);
        case ']': return make_token(lexer, T_RBRACKET);
        case ',': return make_token(lexer, T_COMMA);
        case '.': return make_token(lexer, T_DOT);
        case '+': return make_token(lexer, T_PLUS);
        case '-': return make_token(lexer, T_MINUS);
        case '*': return make_token(lexer, T_STAR);
        case '%': return make_token(lexer, T_MOD);

        /* A '/' that reaches here is division: skip_whitespace already
         * consumed anything that was actually a comment. */
        case '/': return make_token(lexer, T_SLASH);

        /* One-or-two-character operators */
        case '=': return make_token(lexer, match(lexer, '=') ? T_EQ : T_ASSIGN);
        case '<': return make_token(lexer, match(lexer, '=') ? T_LTE : T_LT);
        case '>': return make_token(lexer, match(lexer, '=') ? T_GTE : T_GT);

        /*
         * '!' differs from clox: this language spells logical negation as the
         * keyword `not`, so '!' is only ever the first half of '!='. A bare
         * '!' is an error, and saying so beats the generic "unexpected
         * character" — it is almost always someone writing C by habit.
         */
        case '!':
            if (match(lexer, '=')) return make_token(lexer, T_NEQ);
            return error_token(lexer, "unexpected '!' (use the keyword 'not' for negation)");

        case '"': return string(lexer);
    }

    return error_token(lexer, "unexpected character");
}
