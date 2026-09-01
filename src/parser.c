/*
 * parser.c — Recursive descent parser for the pseudocode language.
 *
 * Implements the grammar rules from docs/grammar.md:
 *   - Precedence climbing for expressions (or -> and -> not -> comparison ->
 *     additive -> multiplicative -> unary -> postfix -> primary)
 *   - 1-token lookahead statement disambiguation for identifiers
 *   - Synchronization at statement boundaries on syntax errors (E1xx)
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "errlog.h"
#include "lexer.h"
#include "parser.h"

/* ------------------------------------------------------------------------ */
/* Forward Declarations                                                      */
/* ------------------------------------------------------------------------ */

static AstNode *parse_statement(Parser *parser);
static AstNode *parse_expression(Parser *parser);
static void advance(Parser *parser);
static void synchronize(Parser *parser);

/* ------------------------------------------------------------------------ */
/* Error Reporting & Recovery                                               */
/* ------------------------------------------------------------------------ */

static void parser_error_at(Parser *parser, const Token *token, const char *format, ...) {
    if (parser->panic_mode) return; /* Suppress cascaded errors */
    parser->panic_mode = true;
    parser->error_count++;

    char buf[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    errlog_report(100, "Parser", token->line, token->col, "%s", buf);
}

static void parser_error_at_current(Parser *parser, const char *format, ...) {
    if (parser->panic_mode) return;
    parser->panic_mode = true;
    parser->error_count++;

    char buf[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    errlog_report(100, "Parser", parser->current.line, parser->current.col, "%s", buf);
}

/*
 * Skip tokens until we find a clean boundary to resume statement parsing.
 */
static void synchronize(Parser *parser) {
    parser->panic_mode = false;

    while (parser->current.type != T_EOF) {
        if (parser->previous.type == T_ENDIF ||
            parser->previous.type == T_ENDFOR ||
            parser->previous.type == T_ENDWHILE ||
            parser->previous.type == T_ENDFUNCTION) {
            return;
        }

        switch (parser->current.type) {
            case T_IF:
            case T_FOR:
            case T_WHILE:
            case T_REPEAT:
            case T_FUNCTION:
            case T_RETURN:
            case T_INPUT:
            case T_OUTPUT:
                return;
            default:
                break;
        }

        advance(parser);
    }
}

/* ------------------------------------------------------------------------ */
/* Token Navigation Helpers                                                  */
/* ------------------------------------------------------------------------ */

static void advance(Parser *parser) {
    parser->previous = parser->current;
    parser->current = parser->peek;

    for (;;) {
        parser->peek = lexer_scan_token(parser->lexer);
        if (parser->peek.type != T_ERROR) break;
        parser_error_at(parser, &parser->peek, "%.*s", parser->peek.length, parser->peek.start);
    }
}

static bool check(const Parser *parser, TokenType type) {
    return parser->current.type == type;
}

static bool match(Parser *parser, TokenType type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static void consume(Parser *parser, TokenType type, const char *message) {
    if (check(parser, type)) {
        advance(parser);
        return;
    }
    parser_error_at_current(parser, "%s", message);
}

static bool is_expression_start(TokenType type) {
    switch (type) {
        case T_NUMBER:
        case T_STRING:
        case T_TRUE:
        case T_FALSE:
        case T_IDENT:
        case T_LPAREN:
        case T_NOT:
        case T_MINUS:
            return true;
        default:
            return false;
    }
}

/* ------------------------------------------------------------------------ */
/* Expression Parsing (Precedence Climbing)                                  */
/* ------------------------------------------------------------------------ */

static AstNode *parse_primary(Parser *parser) {
    int line = parser->current.line;
    int col = parser->current.col;

    if (match(parser, T_NUMBER)) {
        char *endptr;
        double val = strtod(parser->previous.start, &endptr);
        return ast_new_number_lit(line, col, val);
    }

    if (match(parser, T_STRING)) {
        return ast_new_string_lit(line, col, parser->previous.start, parser->previous.length);
    }

    if (match(parser, T_TRUE)) {
        return ast_new_bool_lit(line, col, true);
    }

    if (match(parser, T_FALSE)) {
        return ast_new_bool_lit(line, col, false);
    }

    if (match(parser, T_IDENT)) {
        Token ident = parser->previous;
        /* Function call: name(args...) */
        if (match(parser, T_LPAREN)) {
            AstNodeList args;
            ast_node_list_init(&args);
            if (!check(parser, T_RPAREN)) {
                do {
                    AstNode *arg = parse_expression(parser);
                    if (arg) ast_node_list_append(&args, arg);
                } while (match(parser, T_COMMA));
            }
            consume(parser, T_RPAREN, "expected ')' after arguments");
            return ast_new_call(line, col, ident.start, ident.length, args);
        }
        return ast_new_ident(line, col, ident.start, ident.length);
    }

    if (match(parser, T_LPAREN)) {
        AstNode *expr = parse_expression(parser);
        consume(parser, T_RPAREN, "expected ')' after expression");
        return expr;
    }

    parser_error_at_current(parser, "expected expression");
    return NULL;
}

static AstNode *parse_postfix(Parser *parser) {
    AstNode *expr = parse_primary(parser);
    if (expr == NULL) return NULL;

    while (true) {
        if (match(parser, T_LBRACKET)) {
            int line = parser->previous.line;
            int col = parser->previous.col;
            AstNode *index = parse_expression(parser);
            consume(parser, T_RBRACKET, "expected ']' after array index");
            expr = ast_new_index(line, col, expr, index);
        } else if (match(parser, T_DOT)) {
            int line = parser->previous.line;
            int col = parser->previous.col;
            Token member = parser->current;
            consume(parser, T_IDENT, "expected member name after '.'");
            expr = ast_new_member(line, col, expr, member.start, member.length);
        } else {
            break;
        }
    }
    return expr;
}

static AstNode *parse_unary(Parser *parser) {
    if (match(parser, T_MINUS)) {
        int line = parser->previous.line;
        int col = parser->previous.col;
        AstNode *operand = parse_postfix(parser);
        return ast_new_unary_op(line, col, T_MINUS, operand);
    }
    return parse_postfix(parser);
}

static AstNode *parse_multiplicative(Parser *parser) {
    AstNode *expr = parse_unary(parser);
    while (match(parser, T_STAR) || match(parser, T_SLASH) || match(parser, T_MOD)) {
        TokenType op = parser->previous.type;
        int line = parser->previous.line;
        int col = parser->previous.col;
        AstNode *right = parse_unary(parser);
        expr = ast_new_binary_op(line, col, op, expr, right);
    }
    return expr;
}

static AstNode *parse_additive(Parser *parser) {
    AstNode *expr = parse_multiplicative(parser);
    while (match(parser, T_PLUS) || match(parser, T_MINUS)) {
        TokenType op = parser->previous.type;
        int line = parser->previous.line;
        int col = parser->previous.col;
        AstNode *right = parse_multiplicative(parser);
        expr = ast_new_binary_op(line, col, op, expr, right);
    }
    return expr;
}

static AstNode *parse_comparison(Parser *parser) {
    AstNode *expr = parse_additive(parser);
    /* Comparison operators are non-associative per docs/grammar.md */
    if (match(parser, T_EQ) || match(parser, T_NEQ) ||
        match(parser, T_LT) || match(parser, T_GT) ||
        match(parser, T_LTE) || match(parser, T_GTE)) {
        TokenType op = parser->previous.type;
        int line = parser->previous.line;
        int col = parser->previous.col;
        AstNode *right = parse_additive(parser);
        expr = ast_new_binary_op(line, col, op, expr, right);
    }
    return expr;
}

static AstNode *parse_not(Parser *parser) {
    if (match(parser, T_NOT)) {
        int line = parser->previous.line;
        int col = parser->previous.col;
        AstNode *operand = parse_comparison(parser);
        return ast_new_unary_op(line, col, T_NOT, operand);
    }
    return parse_comparison(parser);
}

static AstNode *parse_and(Parser *parser) {
    AstNode *expr = parse_not(parser);
    while (match(parser, T_AND)) {
        int line = parser->previous.line;
        int col = parser->previous.col;
        AstNode *right = parse_not(parser);
        expr = ast_new_binary_op(line, col, T_AND, expr, right);
    }
    return expr;
}

static AstNode *parse_or(Parser *parser) {
    AstNode *expr = parse_and(parser);
    while (match(parser, T_OR)) {
        int line = parser->previous.line;
        int col = parser->previous.col;
        AstNode *right = parse_and(parser);
        expr = ast_new_binary_op(line, col, T_OR, expr, right);
    }
    return expr;
}

static AstNode *parse_expression(Parser *parser) {
    return parse_or(parser);
}

/* ------------------------------------------------------------------------ */
/* Statement Parsing                                                         */
/* ------------------------------------------------------------------------ */

static AstNode *parse_if(Parser *parser) {
    int line = parser->previous.line;
    int col = parser->previous.col;

    consume(parser, T_LPAREN, "expected '(' after 'if'");
    AstNode *condition = parse_expression(parser);
    consume(parser, T_RPAREN, "expected ')' after if condition");
    consume(parser, T_THEN, "expected 'then' after if condition");

    AstNodeList then_branch;
    ast_node_list_init(&then_branch);

    while (!check(parser, T_ELSE) && !check(parser, T_ENDIF) && !check(parser, T_EOF)) {
        AstNode *stmt = parse_statement(parser);
        if (stmt != NULL) {
            ast_node_list_append(&then_branch, stmt);
        }
        if (parser->panic_mode) {
            synchronize(parser);
        }
    }

    bool has_else = false;
    AstNodeList else_branch;
    ast_node_list_init(&else_branch);

    if (match(parser, T_ELSE)) {
        has_else = true;
        while (!check(parser, T_ENDIF) && !check(parser, T_EOF)) {
            AstNode *stmt = parse_statement(parser);
            if (stmt != NULL) {
                ast_node_list_append(&else_branch, stmt);
            }
            if (parser->panic_mode) {
                synchronize(parser);
            }
        }
    }

    consume(parser, T_ENDIF, "expected 'endif' after if statement");
    return ast_new_if(line, col, condition, then_branch, else_branch, has_else);
}

static AstNode *parse_for(Parser *parser) {
    int line = parser->previous.line;
    int col = parser->previous.col;

    Token var = parser->current;
    consume(parser, T_IDENT, "expected variable name after 'for'");
    consume(parser, T_ASSIGN, "expected '=' after for variable");
    AstNode *start = parse_expression(parser);
    consume(parser, T_TO, "expected 'to' in for loop header");
    AstNode *end = parse_expression(parser);
    consume(parser, T_DO, "expected 'do' after for loop range");

    AstNodeList body;
    ast_node_list_init(&body);

    while (!check(parser, T_ENDFOR) && !check(parser, T_EOF)) {
        AstNode *stmt = parse_statement(parser);
        if (stmt != NULL) {
            ast_node_list_append(&body, stmt);
        }
        if (parser->panic_mode) {
            synchronize(parser);
        }
    }

    consume(parser, T_ENDFOR, "expected 'endfor' after for loop body");
    return ast_new_for(line, col, var.start, var.length, start, end, body);
}

static AstNode *parse_while(Parser *parser) {
    int line = parser->previous.line;
    int col = parser->previous.col;

    consume(parser, T_LPAREN, "expected '(' after 'while'");
    AstNode *condition = parse_expression(parser);
    consume(parser, T_RPAREN, "expected ')' after while condition");
    consume(parser, T_DO, "expected 'do' after while condition");

    AstNodeList body;
    ast_node_list_init(&body);

    while (!check(parser, T_ENDWHILE) && !check(parser, T_EOF)) {
        AstNode *stmt = parse_statement(parser);
        if (stmt != NULL) {
            ast_node_list_append(&body, stmt);
        }
        if (parser->panic_mode) {
            synchronize(parser);
        }
    }

    consume(parser, T_ENDWHILE, "expected 'endwhile' after while loop body");
    return ast_new_while(line, col, condition, body);
}

static AstNode *parse_repeat(Parser *parser) {
    int line = parser->previous.line;
    int col = parser->previous.col;

    AstNodeList body;
    ast_node_list_init(&body);

    while (!check(parser, T_UNTIL) && !check(parser, T_EOF)) {
        AstNode *stmt = parse_statement(parser);
        if (stmt != NULL) {
            ast_node_list_append(&body, stmt);
        }
        if (parser->panic_mode) {
            synchronize(parser);
        }
    }

    consume(parser, T_UNTIL, "expected 'until' after repeat loop body");
    consume(parser, T_LPAREN, "expected '(' after 'until'");
    AstNode *condition = parse_expression(parser);
    consume(parser, T_RPAREN, "expected ')' after until condition");

    return ast_new_repeat(line, col, body, condition);
}

static AstNode *parse_function(Parser *parser) {
    int line = parser->previous.line;
    int col = parser->previous.col;

    Token name = parser->current;
    consume(parser, T_IDENT, "expected function name after 'function'");
    consume(parser, T_LPAREN, "expected '(' after function name");

    AstStringList params;
    ast_string_list_init(&params);

    if (!check(parser, T_RPAREN)) {
        do {
            Token p = parser->current;
            consume(parser, T_IDENT, "expected parameter name");
            char *param_str = malloc((size_t)p.length + 1);
            if (param_str == NULL) {
                fprintf(stderr, "error: out of memory\n");
                exit(1);
            }
            memcpy(param_str, p.start, (size_t)p.length);
            param_str[p.length] = '\0';
            ast_string_list_append(&params, param_str);
        } while (match(parser, T_COMMA));
    }

    consume(parser, T_RPAREN, "expected ')' after parameter list");

    AstNodeList body;
    ast_node_list_init(&body);

    while (!check(parser, T_ENDFUNCTION) && !check(parser, T_EOF)) {
        AstNode *stmt = parse_statement(parser);
        if (stmt != NULL) {
            ast_node_list_append(&body, stmt);
        }
        if (parser->panic_mode) {
            synchronize(parser);
        }
    }

    consume(parser, T_ENDFUNCTION, "expected 'endfunction' after function body");
    return ast_new_function_decl(line, col, name.start, name.length, params, body);
}

static AstNode *parse_return(Parser *parser) {
    int line = parser->previous.line;
    int col = parser->previous.col;

    AstNode *value = NULL;
    if (is_expression_start(parser->current.type)) {
        value = parse_expression(parser);
    }
    return ast_new_return(line, col, value);
}

static AstNode *parse_input(Parser *parser) {
    int line = parser->previous.line;
    int col = parser->previous.col;

    Token var = parser->current;
    consume(parser, T_IDENT, "expected variable name after 'input'");
    return ast_new_input(line, col, var.start, var.length);
}

static AstNode *parse_output(Parser *parser) {
    int line = parser->previous.line;
    int col = parser->previous.col;

    AstNode *value = parse_expression(parser);
    return ast_new_output(line, col, value);
}

static AstNode *parse_assignment(Parser *parser) {
    Token ident = parser->current;
    int line = ident.line;
    int col = ident.col;
    advance(parser); /* Consume T_IDENT */

    AstNode *target = ast_new_ident(line, col, ident.start, ident.length);

    if (match(parser, T_LBRACKET)) {
        int idx_line = parser->previous.line;
        int idx_col = parser->previous.col;
        AstNode *index = parse_expression(parser);
        consume(parser, T_RBRACKET, "expected ']' after array index");
        target = ast_new_index(idx_line, idx_col, target, index);
    }

    consume(parser, T_ASSIGN, "expected '=' in assignment");
    AstNode *value = parse_expression(parser);

    return ast_new_assign(line, col, target, value);
}

static AstNode *parse_statement(Parser *parser) {
    if (match(parser, T_IF)) return parse_if(parser);
    if (match(parser, T_FOR)) return parse_for(parser);
    if (match(parser, T_WHILE)) return parse_while(parser);
    if (match(parser, T_REPEAT)) return parse_repeat(parser);
    if (match(parser, T_FUNCTION)) return parse_function(parser);
    if (match(parser, T_RETURN)) return parse_return(parser);
    if (match(parser, T_INPUT)) return parse_input(parser);
    if (match(parser, T_OUTPUT)) return parse_output(parser);

    /* Disambiguate IDENT: assignment (IDENT [ "[" expr "]" ] "=" expr) vs expr_stmt (call_expr) */
    if (check(parser, T_IDENT)) {
        if (parser->peek.type == T_ASSIGN || parser->peek.type == T_LBRACKET) {
            return parse_assignment(parser);
        } else if (parser->peek.type == T_LPAREN) {
            return parse_expression(parser);
        } else {
            parser_error_at_current(parser, "unexpected identifier '%.*s'; expected assignment or function call",
                                    parser->current.length, parser->current.start);
            advance(parser);
            return NULL;
        }
    }

    parser_error_at_current(parser, "expected statement");
    advance(parser);
    return NULL;
}

/* ------------------------------------------------------------------------ */
/* Public Interface                                                          */
/* ------------------------------------------------------------------------ */

void parser_init(Parser *parser, Lexer *lexer) {
    parser->lexer = lexer;
    parser->error_count = 0;
    parser->panic_mode = false;

    /* Prime current and peek */
    parser->current = lexer_scan_token(lexer);
    while (parser->current.type == T_ERROR) {
        parser_error_at(parser, &parser->current, "%.*s", parser->current.length, parser->current.start);
        parser->current = lexer_scan_token(lexer);
    }

    parser->peek = lexer_scan_token(lexer);
    while (parser->peek.type == T_ERROR) {
        parser_error_at(parser, &parser->peek, "%.*s", parser->peek.length, parser->peek.start);
        parser->peek = lexer_scan_token(lexer);
    }
}

AstNode *parser_parse(Parser *parser) {
    AstNode *program = ast_new_program(1, 1);

    while (!check(parser, T_EOF)) {
        AstNode *stmt = parse_statement(parser);
        if (stmt != NULL) {
            ast_node_list_append(&program->as.program.statements, stmt);
        }
        if (parser->panic_mode) {
            synchronize(parser);
        }
    }

    if (parser->error_count > 0) {
        ast_free(program);
        return NULL;
    }

    return program;
}
