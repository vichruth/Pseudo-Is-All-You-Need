/*
 * ast.h — Abstract Syntax Tree definitions and node operations.
 *
 * Implements the AST specification from docs/AST_SPEC.md.
 * Every AST node carries source location (line and column) from its leading
 * token to enable accurate E2xx/E3xx diagnostic reporting.
 *
 * Memory ownership:
 *   - The AST owns all child nodes and heap-allocated string copies.
 *   - Tokens are transient and not retained inside the AST.
 *   - ast_free() recursively frees a subtree and all owned resources.
 */

#ifndef PSEUDO_AST_H
#define PSEUDO_AST_H

#include <stdbool.h>
#include <stddef.h>
#include "token.h"

/*
 * AST Node Kinds matching docs/AST_SPEC.md.
 */
typedef enum {
    NODE_PROGRAM,
    NODE_IF,
    NODE_FOR,
    NODE_WHILE,
    NODE_REPEAT,
    NODE_FUNCTION_DECL,
    NODE_RETURN,
    NODE_INPUT,
    NODE_OUTPUT,
    NODE_ASSIGN,
    NODE_CALL,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_INDEX,
    NODE_MEMBER,
    NODE_IDENT,
    NODE_NUMBER_LIT,
    NODE_STRING_LIT,
    NODE_BOOL_LIT,
} AstNodeKind;

struct AstNode;

/*
 * Dynamic array of AST node pointers.
 */
typedef struct {
    struct AstNode **items;
    int count;
    int capacity;
} AstNodeList;

/*
 * Dynamic array of strings (e.g. function parameter names).
 */
typedef struct {
    char **items;
    int count;
    int capacity;
} AstStringList;

/*
 * AST Node structure with tagged union.
 */
typedef struct AstNode {
    AstNodeKind kind;
    int line;
    int col;

    union {
        /* NODE_PROGRAM */
        struct {
            AstNodeList statements;
        } program;

        /* NODE_IF */
        struct {
            struct AstNode *condition;
            AstNodeList then_branch;
            AstNodeList else_branch;
            bool has_else;
        } if_stmt;

        /* NODE_FOR */
        struct {
            char *var_name;
            struct AstNode *start;
            struct AstNode *end;
            AstNodeList body;
        } for_stmt;

        /* NODE_WHILE */
        struct {
            struct AstNode *condition;
            AstNodeList body;
        } while_stmt;

        /* NODE_REPEAT */
        struct {
            AstNodeList body;
            struct AstNode *condition;
        } repeat_stmt;

        /* NODE_FUNCTION_DECL */
        struct {
            char *name;
            AstStringList params;
            AstNodeList body;
        } function_decl;

        /* NODE_RETURN */
        struct {
            struct AstNode *value; /* NULL if bare return */
        } return_stmt;

        /* NODE_INPUT */
        struct {
            char *var_name;
        } input_stmt;

        /* NODE_OUTPUT */
        struct {
            struct AstNode *value;
        } output_stmt;

        /* NODE_ASSIGN */
        struct {
            struct AstNode *target; /* NODE_IDENT or NODE_INDEX */
            struct AstNode *value;
        } assign;

        /* NODE_CALL */
        struct {
            char *callee;
            AstNodeList args;
        } call;

        /* NODE_BINARY_OP */
        struct {
            TokenType op;
            struct AstNode *left;
            struct AstNode *right;
        } binary_op;

        /* NODE_UNARY_OP */
        struct {
            TokenType op;
            struct AstNode *operand;
        } unary_op;

        /* NODE_INDEX */
        struct {
            struct AstNode *array;
            struct AstNode *index;
        } index_expr;

        /* NODE_MEMBER */
        struct {
            struct AstNode *object;
            char *member;
        } member_expr;

        /* NODE_IDENT */
        struct {
            char *name;
        } ident;

        /* NODE_NUMBER_LIT */
        struct {
            double value;
        } number_lit;

        /* NODE_STRING_LIT */
        struct {
            char *value;
        } string_lit;

        /* NODE_BOOL_LIT */
        struct {
            bool value;
        } bool_lit;
    } as;
} AstNode;

/* ------------------------------------------------------------------------ */
/* List Helper Functions                                                    */
/* ------------------------------------------------------------------------ */

void ast_node_list_init(AstNodeList *list);
void ast_node_list_append(AstNodeList *list, AstNode *node);
void ast_node_list_free(AstNodeList *list);

void ast_string_list_init(AstStringList *list);
void ast_string_list_append(AstStringList *list, char *str);
void ast_string_list_free(AstStringList *list);

/* ------------------------------------------------------------------------ */
/* Node Constructors                                                        */
/* ------------------------------------------------------------------------ */

AstNode *ast_new_program(int line, int col);
AstNode *ast_new_if(int line, int col, AstNode *condition, AstNodeList then_branch, AstNodeList else_branch, bool has_else);
AstNode *ast_new_for(int line, int col, const char *var_name, int var_len, AstNode *start, AstNode *end, AstNodeList body);
AstNode *ast_new_while(int line, int col, AstNode *condition, AstNodeList body);
AstNode *ast_new_repeat(int line, int col, AstNodeList body, AstNode *condition);
AstNode *ast_new_function_decl(int line, int col, const char *name, int name_len, AstStringList params, AstNodeList body);
AstNode *ast_new_return(int line, int col, AstNode *value);
AstNode *ast_new_input(int line, int col, const char *var_name, int var_len);
AstNode *ast_new_output(int line, int col, AstNode *value);
AstNode *ast_new_assign(int line, int col, AstNode *target, AstNode *value);
AstNode *ast_new_call(int line, int col, const char *callee, int callee_len, AstNodeList args);
AstNode *ast_new_binary_op(int line, int col, TokenType op, AstNode *left, AstNode *right);
AstNode *ast_new_unary_op(int line, int col, TokenType op, AstNode *operand);
AstNode *ast_new_index(int line, int col, AstNode *array, AstNode *index);
AstNode *ast_new_member(int line, int col, AstNode *object, const char *member, int member_len);
AstNode *ast_new_ident(int line, int col, const char *name, int len);
AstNode *ast_new_number_lit(int line, int col, double value);
AstNode *ast_new_string_lit(int line, int col, const char *value, int len);
AstNode *ast_new_bool_lit(int line, int col, bool value);

/* ------------------------------------------------------------------------ */
/* Destructor & Pretty Printer                                              */
/* ------------------------------------------------------------------------ */

void ast_free(AstNode *node);
void ast_print(const AstNode *node, int indent);

#endif /* PSEUDO_AST_H */
