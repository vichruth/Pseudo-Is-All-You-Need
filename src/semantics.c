/*
 * semantics.c — Semantic analysis, scope resolution, and type checking.
 *
 * Enforces semantic rules from docs/grammar.md:
 *   - Scope resolution: declared/assigned-before-use verification (E201)
 *   - Condition expressions must be boolean (E202)
 *   - Loop ranges and array indices must be numeric/integer (E203)
 *   - Function call argument counts must match declarations (E204)
 *   - Member access must be valid (.length only) (E205)
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "errlog.h"
#include "semantics.h"

/* ------------------------------------------------------------------------ */
/* String helper                                                             */
/* ------------------------------------------------------------------------ */

static char *dup_str(const char *s) {
    if (s == NULL) return NULL;
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (copy == NULL) {
        fprintf(stderr, "error: out of memory duplicating string\n");
        exit(1);
    }
    memcpy(copy, s, len + 1);
    return copy;
}

/* ------------------------------------------------------------------------ */
/* Scope Management                                                         */
/* ------------------------------------------------------------------------ */

static Scope *scope_create(Scope *parent, bool is_function) {
    Scope *scope = malloc(sizeof(Scope));
    if (scope == NULL) {
        fprintf(stderr, "error: out of memory allocating Scope\n");
        exit(1);
    }
    scope->parent = parent;
    scope->is_function = is_function;

    scope->symbols = NULL;
    scope->symbol_count = 0;
    scope->symbol_capacity = 0;

    scope->functions = NULL;
    scope->function_count = 0;
    scope->function_capacity = 0;

    return scope;
}

static void scope_free(Scope *scope) {
    if (scope == NULL) return;

    for (int i = 0; i < scope->symbol_count; i++) {
        free(scope->symbols[i].name);
    }
    free(scope->symbols);

    for (int i = 0; i < scope->function_count; i++) {
        free(scope->functions[i].name);
    }
    free(scope->functions);

    free(scope);
}

static void scope_define_var(Scope *scope, const char *name, TypeKind type, int line, int col) {
    /* Check if variable already exists in current scope */
    for (int i = 0; i < scope->symbol_count; i++) {
        if (strcmp(scope->symbols[i].name, name) == 0) {
            scope->symbols[i].type = type;
            return;
        }
    }

    if (scope->symbol_count + 1 > scope->symbol_capacity) {
        int new_capacity = scope->symbol_capacity < 8 ? 8 : scope->symbol_capacity * 2;
        Symbol *new_symbols = realloc(scope->symbols, sizeof(Symbol) * (size_t)new_capacity);
        if (new_symbols == NULL) {
            fprintf(stderr, "error: out of memory expanding Symbol table\n");
            exit(1);
        }
        scope->symbols = new_symbols;
        scope->symbol_capacity = new_capacity;
    }

    Symbol *sym = &scope->symbols[scope->symbol_count++];
    sym->name = dup_str(name);
    sym->type = type;
    sym->line = line;
    sym->col = col;
}

static Symbol *scope_lookup_var(Scope *scope, const char *name) {
    for (Scope *s = scope; s != NULL; s = s->parent) {
        for (int i = 0; i < s->symbol_count; i++) {
            if (strcmp(s->symbols[i].name, name) == 0) {
                return &s->symbols[i];
            }
        }
    }
    return NULL;
}

static void scope_define_func(Scope *scope, const char *name, int arity, int line, int col) {
    for (int i = 0; i < scope->function_count; i++) {
        if (strcmp(scope->functions[i].name, name) == 0) {
            return;
        }
    }

    if (scope->function_count + 1 > scope->function_capacity) {
        int new_capacity = scope->function_capacity < 8 ? 8 : scope->function_capacity * 2;
        FunctionSymbol *new_funcs = realloc(scope->functions, sizeof(FunctionSymbol) * (size_t)new_capacity);
        if (new_funcs == NULL) {
            fprintf(stderr, "error: out of memory expanding Function table\n");
            exit(1);
        }
        scope->functions = new_funcs;
        scope->function_capacity = new_capacity;
    }

    FunctionSymbol *fn = &scope->functions[scope->function_count++];
    fn->name = dup_str(name);
    fn->arity = arity;
    fn->line = line;
    fn->col = col;
}

static FunctionSymbol *scope_lookup_func(Scope *scope, const char *name) {
    for (Scope *s = scope; s != NULL; s = s->parent) {
        for (int i = 0; i < s->function_count; i++) {
            if (strcmp(s->functions[i].name, name) == 0) {
                return &s->functions[i];
            }
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------------ */
/* Error Logging                                                            */
/* ------------------------------------------------------------------------ */

static void semantic_error(SemanticAnalyzer *analyzer, int line, int col, int code, const char *format, ...) {
    analyzer->error_count++;

    char buf[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    errlog_report(code, "Semantic", line, col, "%s", buf);
}

/* ------------------------------------------------------------------------ */
/* AST Traversal & Analysis                                                 */
/* ------------------------------------------------------------------------ */

static TypeKind analyze_expression(SemanticAnalyzer *analyzer, AstNode *expr, Scope *scope);
static void analyze_statement(SemanticAnalyzer *analyzer, AstNode *stmt, Scope *scope);

static TypeKind analyze_expression(SemanticAnalyzer *analyzer, AstNode *expr, Scope *scope) {
    if (expr == NULL) return TYPE_UNKNOWN;

    switch (expr->kind) {
        case NODE_NUMBER_LIT:
            return TYPE_NUMBER;

        case NODE_STRING_LIT:
            return TYPE_STRING;

        case NODE_BOOL_LIT:
            return TYPE_BOOL;

        case NODE_IDENT: {
            Symbol *sym = scope_lookup_var(scope, expr->as.ident.name);
            if (sym == NULL) {
                semantic_error(analyzer, expr->line, expr->col, 201,
                               "variable '%s' used before assignment", expr->as.ident.name);
                return TYPE_UNKNOWN;
            }
            return sym->type;
        }

        case NODE_BINARY_OP: {
            TypeKind left = analyze_expression(analyzer, expr->as.binary_op.left, scope);
            TypeKind right = analyze_expression(analyzer, expr->as.binary_op.right, scope);
            TokenType op = expr->as.binary_op.op;

            if (op == T_AND || op == T_OR) {
                if (left != TYPE_UNKNOWN && left != TYPE_BOOL) {
                    semantic_error(analyzer, expr->as.binary_op.left->line, expr->as.binary_op.left->col,
                                   202, "operand of '%s' must be boolean", token_type_name(op));
                }
                if (right != TYPE_UNKNOWN && right != TYPE_BOOL) {
                    semantic_error(analyzer, expr->as.binary_op.right->line, expr->as.binary_op.right->col,
                                   202, "operand of '%s' must be boolean", token_type_name(op));
                }
                return TYPE_BOOL;
            }

            if (op == T_EQ || op == T_NEQ || op == T_LT || op == T_GT || op == T_LTE || op == T_GTE) {
                return TYPE_BOOL;
            }

            /* Arithmetic operators */
            if (left == TYPE_STRING || right == TYPE_STRING) {
                if (op != T_PLUS) {
                    semantic_error(analyzer, expr->line, expr->col, 203,
                                   "operator '%s' cannot be applied to strings", token_type_name(op));
                }
                return TYPE_STRING;
            }

            return TYPE_NUMBER;
        }

        case NODE_UNARY_OP: {
            TypeKind operand = analyze_expression(analyzer, expr->as.unary_op.operand, scope);
            TokenType op = expr->as.unary_op.op;

            if (op == T_NOT) {
                if (operand != TYPE_UNKNOWN && operand != TYPE_BOOL) {
                    semantic_error(analyzer, expr->line, expr->col, 202,
                                   "operand of 'not' must be boolean");
                }
                return TYPE_BOOL;
            }

            if (op == T_MINUS) {
                if (operand != TYPE_UNKNOWN && operand != TYPE_NUMBER) {
                    semantic_error(analyzer, expr->line, expr->col, 203,
                                   "operand of unary '-' must be a number");
                }
                return TYPE_NUMBER;
            }

            return TYPE_UNKNOWN;
        }

        case NODE_INDEX: {
            /* Array must be a defined variable or expression */
            if (expr->as.index_expr.array->kind == NODE_IDENT) {
                const char *arr_name = expr->as.index_expr.array->as.ident.name;
                Symbol *sym = scope_lookup_var(scope, arr_name);
                if (sym == NULL) {
                    /* In pseudocode, assigning to arr[0] can initialize the array */
                    scope_define_var(scope, arr_name, TYPE_ARRAY, expr->line, expr->col);
                }
            } else {
                analyze_expression(analyzer, expr->as.index_expr.array, scope);
            }

            TypeKind idx_type = analyze_expression(analyzer, expr->as.index_expr.index, scope);
            if (idx_type != TYPE_UNKNOWN && idx_type != TYPE_NUMBER) {
                semantic_error(analyzer, expr->as.index_expr.index->line, expr->as.index_expr.index->col,
                               203, "array index must be an integer");
            }
            return TYPE_UNKNOWN;
        }

        case NODE_MEMBER: {
            analyze_expression(analyzer, expr->as.member_expr.object, scope);
            if (strcmp(expr->as.member_expr.member, "length") != 0) {
                semantic_error(analyzer, expr->line, expr->col, 205,
                               "unknown member '.%s' (only '.length' is supported)",
                               expr->as.member_expr.member);
            }
            return TYPE_NUMBER;
        }

        case NODE_CALL: {
            FunctionSymbol *fn = scope_lookup_func(scope, expr->as.call.callee);
            if (fn == NULL) {
                semantic_error(analyzer, expr->line, expr->col, 201,
                               "undefined function '%s'", expr->as.call.callee);
            } else {
                if (expr->as.call.args.count != fn->arity) {
                    semantic_error(analyzer, expr->line, expr->col, 204,
                                   "function '%s' expects %d argument(s) but got %d",
                                   expr->as.call.callee, fn->arity, expr->as.call.args.count);
                }
            }

            for (int i = 0; i < expr->as.call.args.count; i++) {
                analyze_expression(analyzer, expr->as.call.args.items[i], scope);
            }
            return TYPE_UNKNOWN;
        }

        default:
            return TYPE_UNKNOWN;
    }
}

static void analyze_statement(SemanticAnalyzer *analyzer, AstNode *stmt, Scope *scope) {
    if (stmt == NULL) return;

    switch (stmt->kind) {
        case NODE_FUNCTION_DECL: {
            Scope *fn_scope = scope_create(scope, true);

            for (int i = 0; i < stmt->as.function_decl.params.count; i++) {
                scope_define_var(fn_scope, stmt->as.function_decl.params.items[i], TYPE_UNKNOWN,
                                 stmt->line, stmt->col);
            }

            for (int i = 0; i < stmt->as.function_decl.body.count; i++) {
                analyze_statement(analyzer, stmt->as.function_decl.body.items[i], fn_scope);
            }

            scope_free(fn_scope);
            break;
        }

        case NODE_IF: {
            TypeKind cond_type = analyze_expression(analyzer, stmt->as.if_stmt.condition, scope);
            if (cond_type != TYPE_UNKNOWN && cond_type != TYPE_BOOL) {
                semantic_error(analyzer, stmt->as.if_stmt.condition->line, stmt->as.if_stmt.condition->col,
                               202, "if condition must evaluate to a boolean");
            }

            Scope *then_scope = scope_create(scope, false);
            for (int i = 0; i < stmt->as.if_stmt.then_branch.count; i++) {
                analyze_statement(analyzer, stmt->as.if_stmt.then_branch.items[i], then_scope);
            }
            scope_free(then_scope);

            if (stmt->as.if_stmt.has_else) {
                Scope *else_scope = scope_create(scope, false);
                for (int i = 0; i < stmt->as.if_stmt.else_branch.count; i++) {
                    analyze_statement(analyzer, stmt->as.if_stmt.else_branch.items[i], else_scope);
                }
                scope_free(else_scope);
            }
            break;
        }

        case NODE_FOR: {
            TypeKind start_type = analyze_expression(analyzer, stmt->as.for_stmt.start, scope);
            TypeKind end_type = analyze_expression(analyzer, stmt->as.for_stmt.end, scope);

            if (start_type != TYPE_UNKNOWN && start_type != TYPE_NUMBER) {
                semantic_error(analyzer, stmt->as.for_stmt.start->line, stmt->as.for_stmt.start->col,
                               203, "for loop start range must be an integer");
            }
            if (end_type != TYPE_UNKNOWN && end_type != TYPE_NUMBER) {
                semantic_error(analyzer, stmt->as.for_stmt.end->line, stmt->as.for_stmt.end->col,
                               203, "for loop end range must be an integer");
            }

            Scope *loop_scope = scope_create(scope, false);
            scope_define_var(loop_scope, stmt->as.for_stmt.var_name, TYPE_NUMBER, stmt->line, stmt->col);

            for (int i = 0; i < stmt->as.for_stmt.body.count; i++) {
                analyze_statement(analyzer, stmt->as.for_stmt.body.items[i], loop_scope);
            }

            scope_free(loop_scope);
            break;
        }

        case NODE_WHILE: {
            TypeKind cond_type = analyze_expression(analyzer, stmt->as.while_stmt.condition, scope);
            if (cond_type != TYPE_UNKNOWN && cond_type != TYPE_BOOL) {
                semantic_error(analyzer, stmt->as.while_stmt.condition->line, stmt->as.while_stmt.condition->col,
                               202, "while condition must evaluate to a boolean");
            }

            Scope *loop_scope = scope_create(scope, false);
            for (int i = 0; i < stmt->as.while_stmt.body.count; i++) {
                analyze_statement(analyzer, stmt->as.while_stmt.body.items[i], loop_scope);
            }
            scope_free(loop_scope);
            break;
        }

        case NODE_REPEAT: {
            Scope *loop_scope = scope_create(scope, false);
            for (int i = 0; i < stmt->as.repeat_stmt.body.count; i++) {
                analyze_statement(analyzer, stmt->as.repeat_stmt.body.items[i], loop_scope);
            }
            scope_free(loop_scope);

            TypeKind cond_type = analyze_expression(analyzer, stmt->as.repeat_stmt.condition, scope);
            if (cond_type != TYPE_UNKNOWN && cond_type != TYPE_BOOL) {
                semantic_error(analyzer, stmt->as.repeat_stmt.condition->line, stmt->as.repeat_stmt.condition->col,
                               202, "until condition must evaluate to a boolean");
            }
            break;
        }

        case NODE_INPUT:
            scope_define_var(scope, stmt->as.input_stmt.var_name, TYPE_UNKNOWN, stmt->line, stmt->col);
            break;

        case NODE_OUTPUT:
            analyze_expression(analyzer, stmt->as.output_stmt.value, scope);
            break;

        case NODE_ASSIGN: {
            TypeKind val_type = analyze_expression(analyzer, stmt->as.assign.value, scope);

            if (stmt->as.assign.target->kind == NODE_IDENT) {
                scope_define_var(scope, stmt->as.assign.target->as.ident.name, val_type,
                                 stmt->line, stmt->col);
            } else if (stmt->as.assign.target->kind == NODE_INDEX) {
                AstNode *arr_node = stmt->as.assign.target->as.index_expr.array;
                if (arr_node->kind == NODE_IDENT) {
                    scope_define_var(scope, arr_node->as.ident.name, TYPE_ARRAY, stmt->line, stmt->col);
                } else {
                    analyze_expression(analyzer, arr_node, scope);
                }

                TypeKind idx_type = analyze_expression(analyzer, stmt->as.assign.target->as.index_expr.index, scope);
                if (idx_type != TYPE_UNKNOWN && idx_type != TYPE_NUMBER) {
                    semantic_error(analyzer, stmt->as.assign.target->as.index_expr.index->line,
                                   stmt->as.assign.target->as.index_expr.index->col,
                                   203, "array index must be an integer");
                }
            }
            break;
        }

        case NODE_RETURN:
            if (stmt->as.return_stmt.value != NULL) {
                analyze_expression(analyzer, stmt->as.return_stmt.value, scope);
            }
            break;

        case NODE_CALL:
            analyze_expression(analyzer, stmt, scope);
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------------ */
/* Public Interface                                                          */
/* ------------------------------------------------------------------------ */

void semantics_init(SemanticAnalyzer *analyzer) {
    analyzer->global_scope = scope_create(NULL, false);
    analyzer->current_scope = analyzer->global_scope;
    analyzer->error_count = 0;
}

void semantics_destroy(SemanticAnalyzer *analyzer) {
    scope_free(analyzer->global_scope);
    analyzer->global_scope = NULL;
    analyzer->current_scope = NULL;
}

int semantics_analyze(SemanticAnalyzer *analyzer, AstNode *program) {
    if (program == NULL || program->kind != NODE_PROGRAM) return 0;

    /* Pass 1: Declare all functions in global scope for forward references */
    for (int i = 0; i < program->as.program.statements.count; i++) {
        AstNode *stmt = program->as.program.statements.items[i];
        if (stmt->kind == NODE_FUNCTION_DECL) {
            scope_define_func(analyzer->global_scope,
                              stmt->as.function_decl.name,
                              stmt->as.function_decl.params.count,
                              stmt->line, stmt->col);
        }
    }

    /* Pass 2: Analyze each statement */
    for (int i = 0; i < program->as.program.statements.count; i++) {
        analyze_statement(analyzer, program->as.program.statements.items[i], analyzer->global_scope);
    }

    return analyzer->error_count;
}
