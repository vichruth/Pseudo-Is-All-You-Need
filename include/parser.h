/*
 * parser.h — Recursive descent parser for the pseudocode language.
 *
 * Implements the grammar specification in docs/grammar.md.
 * Builds an AST conforming to docs/AST_SPEC.md.
 *
 * Error handling:
 *   - Emits structured E1xx syntax errors to stderr:
 *     [E1xx] [Parser] line L, col C: <message>
 *   - Synchronizes at statement boundaries to report multiple errors per pass.
 */

#ifndef PSEUDO_PARSER_H
#define PSEUDO_PARSER_H

#include <stdbool.h>
#include "ast.h"
#include "lexer.h"
#include "token.h"

typedef struct {
    Lexer *lexer;
    Token current;
    Token peek;
    Token previous;
    int error_count;
    bool panic_mode;
} Parser;

/*
 * Initialize the parser with an active Lexer.
 */
void parser_init(Parser *parser, Lexer *lexer);

/*
 * Parse the full program into a root NODE_PROGRAM AST node.
 * Returns NULL if parsing fails with syntax errors.
 */
AstNode *parser_parse(Parser *parser);

#endif /* PSEUDO_PARSER_H */
