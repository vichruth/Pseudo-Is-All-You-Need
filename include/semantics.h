/*
 * semantics.h — Semantic analysis and type checking.
 *
 * Implements scope resolution and semantic type validation for the AST.
 *
 * Error codes emitted:
 *   [E201] Variable or function used before declaration
 *   [E202] Condition expression type error (non-boolean)
 *   [E203] Integer type expected for loop range or array index
 *   [E204] Function call argument count mismatch (arity error)
 *   [E205] Invalid member access (only .length supported)
 */

#ifndef PSEUDO_SEMANTICS_H
#define PSEUDO_SEMANTICS_H

#include <stdbool.h>
#include "ast.h"

typedef enum {
    TYPE_UNKNOWN,
    TYPE_NUMBER,
    TYPE_BOOL,
    TYPE_STRING,
    TYPE_ARRAY,
} TypeKind;

typedef struct Symbol {
    char *name;
    TypeKind type;
    int line;
    int col;
} Symbol;

typedef struct FunctionSymbol {
    char *name;
    int arity;
    int line;
    int col;
} FunctionSymbol;

typedef struct Scope {
    struct Scope *parent;
    bool is_function;

    Symbol *symbols;
    int symbol_count;
    int symbol_capacity;

    FunctionSymbol *functions;
    int function_count;
    int function_capacity;
} Scope;

typedef struct {
    Scope *global_scope;
    Scope *current_scope;
    int error_count;
} SemanticAnalyzer;

/*
 * Initialize the semantic analyzer.
 */
void semantics_init(SemanticAnalyzer *analyzer);

/*
 * Free resources allocated by the semantic analyzer.
 */
void semantics_destroy(SemanticAnalyzer *analyzer);

/*
 * Perform semantic analysis on the AST.
 * Returns the total number of E2xx errors encountered.
 */
int semantics_analyze(SemanticAnalyzer *analyzer, AstNode *program);

#endif /* PSEUDO_SEMANTICS_H */
