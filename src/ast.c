/*
 * ast.c — AST node construction, dynamic lists, memory destruction, and printing.
 *
 * Follows memory ownership rules in docs/AST_SPEC.md:
 *   - Nodes own their child nodes, owned lists, and string fields.
 *   - String literals/identifiers are copied using heap allocation.
 *   - ast_free() cleanly frees entire AST subtrees without leaks.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

/* ------------------------------------------------------------------------ */
/* String helper                                                             */
/* ------------------------------------------------------------------------ */

static char *copy_string(const char *src, int len) {
    if (src == NULL || len < 0) return NULL;
    char *copy = malloc((size_t)len + 1);
    if (copy == NULL) {
        fprintf(stderr, "error: out of memory copying string\n");
        exit(1);
    }
    memcpy(copy, src, (size_t)len);
    copy[len] = '\0';
    return copy;
}

/* ------------------------------------------------------------------------ */
/* Dynamic List Operations                                                  */
/* ------------------------------------------------------------------------ */

void ast_node_list_init(AstNodeList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void ast_node_list_append(AstNodeList *list, AstNode *node) {
    if (node == NULL) return;
    if (list->count + 1 > list->capacity) {
        int new_capacity = list->capacity < 8 ? 8 : list->capacity * 2;
        AstNode **new_items = realloc(list->items, sizeof(AstNode *) * (size_t)new_capacity);
        if (new_items == NULL) {
            fprintf(stderr, "error: out of memory expanding AstNodeList\n");
            exit(1);
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->count++] = node;
}

void ast_node_list_free(AstNodeList *list) {
    if (list == NULL || list->items == NULL) return;
    for (int i = 0; i < list->count; i++) {
        ast_free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void ast_string_list_init(AstStringList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void ast_string_list_append(AstStringList *list, char *str) {
    if (str == NULL) return;
    if (list->count + 1 > list->capacity) {
        int new_capacity = list->capacity < 8 ? 8 : list->capacity * 2;
        char **new_items = realloc(list->items, sizeof(char *) * (size_t)new_capacity);
        if (new_items == NULL) {
            fprintf(stderr, "error: out of memory expanding AstStringList\n");
            exit(1);
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->count++] = str;
}

void ast_string_list_free(AstStringList *list) {
    if (list == NULL || list->items == NULL) return;
    for (int i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* ------------------------------------------------------------------------ */
/* Node Allocation Helper                                                    */
/* ------------------------------------------------------------------------ */

static AstNode *alloc_node(AstNodeKind kind, int line, int col) {
    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL) {
        fprintf(stderr, "error: out of memory allocating AstNode\n");
        exit(1);
    }
    memset(node, 0, sizeof(AstNode));
    node->kind = kind;
    node->line = line;
    node->col = col;
    return node;
}

/* ------------------------------------------------------------------------ */
/* Node Constructors                                                        */
/* ------------------------------------------------------------------------ */

AstNode *ast_new_program(int line, int col) {
    AstNode *node = alloc_node(NODE_PROGRAM, line, col);
    ast_node_list_init(&node->as.program.statements);
    return node;
}

AstNode *ast_new_if(int line, int col, AstNode *condition, AstNodeList then_branch, AstNodeList else_branch, bool has_else) {
    AstNode *node = alloc_node(NODE_IF, line, col);
    node->as.if_stmt.condition = condition;
    node->as.if_stmt.then_branch = then_branch;
    node->as.if_stmt.else_branch = else_branch;
    node->as.if_stmt.has_else = has_else;
    return node;
}

AstNode *ast_new_for(int line, int col, const char *var_name, int var_len, AstNode *start, AstNode *end, AstNodeList body) {
    AstNode *node = alloc_node(NODE_FOR, line, col);
    node->as.for_stmt.var_name = copy_string(var_name, var_len);
    node->as.for_stmt.start = start;
    node->as.for_stmt.end = end;
    node->as.for_stmt.body = body;
    return node;
}

AstNode *ast_new_while(int line, int col, AstNode *condition, AstNodeList body) {
    AstNode *node = alloc_node(NODE_WHILE, line, col);
    node->as.while_stmt.condition = condition;
    node->as.while_stmt.body = body;
    return node;
}

AstNode *ast_new_repeat(int line, int col, AstNodeList body, AstNode *condition) {
    AstNode *node = alloc_node(NODE_REPEAT, line, col);
    node->as.repeat_stmt.body = body;
    node->as.repeat_stmt.condition = condition;
    return node;
}

AstNode *ast_new_function_decl(int line, int col, const char *name, int name_len, AstStringList params, AstNodeList body) {
    AstNode *node = alloc_node(NODE_FUNCTION_DECL, line, col);
    node->as.function_decl.name = copy_string(name, name_len);
    node->as.function_decl.params = params;
    node->as.function_decl.body = body;
    return node;
}

AstNode *ast_new_return(int line, int col, AstNode *value) {
    AstNode *node = alloc_node(NODE_RETURN, line, col);
    node->as.return_stmt.value = value;
    return node;
}

AstNode *ast_new_input(int line, int col, const char *var_name, int var_len) {
    AstNode *node = alloc_node(NODE_INPUT, line, col);
    node->as.input_stmt.var_name = copy_string(var_name, var_len);
    return node;
}

AstNode *ast_new_output(int line, int col, AstNode *value) {
    AstNode *node = alloc_node(NODE_OUTPUT, line, col);
    node->as.output_stmt.value = value;
    return node;
}

AstNode *ast_new_assign(int line, int col, AstNode *target, AstNode *value) {
    AstNode *node = alloc_node(NODE_ASSIGN, line, col);
    node->as.assign.target = target;
    node->as.assign.value = value;
    return node;
}

AstNode *ast_new_call(int line, int col, const char *callee, int callee_len, AstNodeList args) {
    AstNode *node = alloc_node(NODE_CALL, line, col);
    node->as.call.callee = copy_string(callee, callee_len);
    node->as.call.args = args;
    return node;
}

AstNode *ast_new_binary_op(int line, int col, TokenType op, AstNode *left, AstNode *right) {
    AstNode *node = alloc_node(NODE_BINARY_OP, line, col);
    node->as.binary_op.op = op;
    node->as.binary_op.left = left;
    node->as.binary_op.right = right;
    return node;
}

AstNode *ast_new_unary_op(int line, int col, TokenType op, AstNode *operand) {
    AstNode *node = alloc_node(NODE_UNARY_OP, line, col);
    node->as.unary_op.op = op;
    node->as.unary_op.operand = operand;
    return node;
}

AstNode *ast_new_index(int line, int col, AstNode *array, AstNode *index) {
    AstNode *node = alloc_node(NODE_INDEX, line, col);
    node->as.index_expr.array = array;
    node->as.index_expr.index = index;
    return node;
}

AstNode *ast_new_member(int line, int col, AstNode *object, const char *member, int member_len) {
    AstNode *node = alloc_node(NODE_MEMBER, line, col);
    node->as.member_expr.object = object;
    node->as.member_expr.member = copy_string(member, member_len);
    return node;
}

AstNode *ast_new_ident(int line, int col, const char *name, int len) {
    AstNode *node = alloc_node(NODE_IDENT, line, col);
    node->as.ident.name = copy_string(name, len);
    return node;
}

AstNode *ast_new_number_lit(int line, int col, double value) {
    AstNode *node = alloc_node(NODE_NUMBER_LIT, line, col);
    node->as.number_lit.value = value;
    return node;
}

AstNode *ast_new_string_lit(int line, int col, const char *value, int len) {
    AstNode *node = alloc_node(NODE_STRING_LIT, line, col);
    /* If the token starts and ends with quotes, strip them */
    if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
        node->as.string_lit.value = copy_string(value + 1, len - 2);
    } else {
        node->as.string_lit.value = copy_string(value, len);
    }
    return node;
}

AstNode *ast_new_bool_lit(int line, int col, bool value) {
    AstNode *node = alloc_node(NODE_BOOL_LIT, line, col);
    node->as.bool_lit.value = value;
    return node;
}

/* ------------------------------------------------------------------------ */
/* Destructor                                                                */
/* ------------------------------------------------------------------------ */

void ast_free(AstNode *node) {
    if (node == NULL) return;

    switch (node->kind) {
        case NODE_PROGRAM:
            ast_node_list_free(&node->as.program.statements);
            break;

        case NODE_IF:
            ast_free(node->as.if_stmt.condition);
            ast_node_list_free(&node->as.if_stmt.then_branch);
            ast_node_list_free(&node->as.if_stmt.else_branch);
            break;

        case NODE_FOR:
            free(node->as.for_stmt.var_name);
            ast_free(node->as.for_stmt.start);
            ast_free(node->as.for_stmt.end);
            ast_node_list_free(&node->as.for_stmt.body);
            break;

        case NODE_WHILE:
            ast_free(node->as.while_stmt.condition);
            ast_node_list_free(&node->as.while_stmt.body);
            break;

        case NODE_REPEAT:
            ast_node_list_free(&node->as.repeat_stmt.body);
            ast_free(node->as.repeat_stmt.condition);
            break;

        case NODE_FUNCTION_DECL:
            free(node->as.function_decl.name);
            ast_string_list_free(&node->as.function_decl.params);
            ast_node_list_free(&node->as.function_decl.body);
            break;

        case NODE_RETURN:
            ast_free(node->as.return_stmt.value);
            break;

        case NODE_INPUT:
            free(node->as.input_stmt.var_name);
            break;

        case NODE_OUTPUT:
            ast_free(node->as.output_stmt.value);
            break;

        case NODE_ASSIGN:
            ast_free(node->as.assign.target);
            ast_free(node->as.assign.value);
            break;

        case NODE_CALL:
            free(node->as.call.callee);
            ast_node_list_free(&node->as.call.args);
            break;

        case NODE_BINARY_OP:
            ast_free(node->as.binary_op.left);
            ast_free(node->as.binary_op.right);
            break;

        case NODE_UNARY_OP:
            ast_free(node->as.unary_op.operand);
            break;

        case NODE_INDEX:
            ast_free(node->as.index_expr.array);
            ast_free(node->as.index_expr.index);
            break;

        case NODE_MEMBER:
            ast_free(node->as.member_expr.object);
            free(node->as.member_expr.member);
            break;

        case NODE_IDENT:
            free(node->as.ident.name);
            break;

        case NODE_NUMBER_LIT:
            break;

        case NODE_STRING_LIT:
            free(node->as.string_lit.value);
            break;

        case NODE_BOOL_LIT:
            break;
    }

    free(node);
}

/* ------------------------------------------------------------------------ */
/* Pretty Printer                                                           */
/* ------------------------------------------------------------------------ */

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void ast_print(const AstNode *node, int indent) {
    if (node == NULL) {
        print_indent(indent);
        printf("(null)\n");
        return;
    }

    print_indent(indent);

    switch (node->kind) {
        case NODE_PROGRAM:
            printf("Program [%d:%d]\n", node->line, node->col);
            for (int i = 0; i < node->as.program.statements.count; i++) {
                ast_print(node->as.program.statements.items[i], indent + 1);
            }
            break;

        case NODE_IF:
            printf("If [%d:%d]\n", node->line, node->col);
            print_indent(indent + 1);
            printf("Condition:\n");
            ast_print(node->as.if_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("Then:\n");
            for (int i = 0; i < node->as.if_stmt.then_branch.count; i++) {
                ast_print(node->as.if_stmt.then_branch.items[i], indent + 2);
            }
            if (node->as.if_stmt.has_else) {
                print_indent(indent + 1);
                printf("Else:\n");
                for (int i = 0; i < node->as.if_stmt.else_branch.count; i++) {
                    ast_print(node->as.if_stmt.else_branch.items[i], indent + 2);
                }
            }
            break;

        case NODE_FOR:
            printf("For '%s' [%d:%d]\n", node->as.for_stmt.var_name, node->line, node->col);
            print_indent(indent + 1);
            printf("Start:\n");
            ast_print(node->as.for_stmt.start, indent + 2);
            print_indent(indent + 1);
            printf("End:\n");
            ast_print(node->as.for_stmt.end, indent + 2);
            print_indent(indent + 1);
            printf("Body:\n");
            for (int i = 0; i < node->as.for_stmt.body.count; i++) {
                ast_print(node->as.for_stmt.body.items[i], indent + 2);
            }
            break;

        case NODE_WHILE:
            printf("While [%d:%d]\n", node->line, node->col);
            print_indent(indent + 1);
            printf("Condition:\n");
            ast_print(node->as.while_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("Body:\n");
            for (int i = 0; i < node->as.while_stmt.body.count; i++) {
                ast_print(node->as.while_stmt.body.items[i], indent + 2);
            }
            break;

        case NODE_REPEAT:
            printf("Repeat [%d:%d]\n", node->line, node->col);
            print_indent(indent + 1);
            printf("Body:\n");
            for (int i = 0; i < node->as.repeat_stmt.body.count; i++) {
                ast_print(node->as.repeat_stmt.body.items[i], indent + 2);
            }
            print_indent(indent + 1);
            printf("Until Condition:\n");
            ast_print(node->as.repeat_stmt.condition, indent + 2);
            break;

        case NODE_FUNCTION_DECL:
            printf("FunctionDecl '%s' (", node->as.function_decl.name);
            for (int i = 0; i < node->as.function_decl.params.count; i++) {
                printf("%s%s", node->as.function_decl.params.items[i],
                       i + 1 < node->as.function_decl.params.count ? ", " : "");
            }
            printf(") [%d:%d]\n", node->line, node->col);
            print_indent(indent + 1);
            printf("Body:\n");
            for (int i = 0; i < node->as.function_decl.body.count; i++) {
                ast_print(node->as.function_decl.body.items[i], indent + 2);
            }
            break;

        case NODE_RETURN:
            printf("Return [%d:%d]\n", node->line, node->col);
            if (node->as.return_stmt.value != NULL) {
                ast_print(node->as.return_stmt.value, indent + 1);
            }
            break;

        case NODE_INPUT:
            printf("Input '%s' [%d:%d]\n", node->as.input_stmt.var_name, node->line, node->col);
            break;

        case NODE_OUTPUT:
            printf("Output [%d:%d]\n", node->line, node->col);
            ast_print(node->as.output_stmt.value, indent + 1);
            break;

        case NODE_ASSIGN:
            printf("Assign [%d:%d]\n", node->line, node->col);
            print_indent(indent + 1);
            printf("Target:\n");
            ast_print(node->as.assign.target, indent + 2);
            print_indent(indent + 1);
            printf("Value:\n");
            ast_print(node->as.assign.value, indent + 2);
            break;

        case NODE_CALL:
            printf("Call '%s' [%d:%d]\n", node->as.call.callee, node->line, node->col);
            for (int i = 0; i < node->as.call.args.count; i++) {
                ast_print(node->as.call.args.items[i], indent + 1);
            }
            break;

        case NODE_BINARY_OP:
            printf("BinaryOp %s [%d:%d]\n", token_type_name(node->as.binary_op.op), node->line, node->col);
            ast_print(node->as.binary_op.left, indent + 1);
            ast_print(node->as.binary_op.right, indent + 1);
            break;

        case NODE_UNARY_OP:
            printf("UnaryOp %s [%d:%d]\n", token_type_name(node->as.unary_op.op), node->line, node->col);
            ast_print(node->as.unary_op.operand, indent + 1);
            break;

        case NODE_INDEX:
            printf("Index [%d:%d]\n", node->line, node->col);
            print_indent(indent + 1);
            printf("Array:\n");
            ast_print(node->as.index_expr.array, indent + 2);
            print_indent(indent + 1);
            printf("Index:\n");
            ast_print(node->as.index_expr.index, indent + 2);
            break;

        case NODE_MEMBER:
            printf("Member .%s [%d:%d]\n", node->as.member_expr.member, node->line, node->col);
            ast_print(node->as.member_expr.object, indent + 1);
            break;

        case NODE_IDENT:
            printf("Ident '%s' [%d:%d]\n", node->as.ident.name, node->line, node->col);
            break;

        case NODE_NUMBER_LIT:
            printf("Number %g [%d:%d]\n", node->as.number_lit.value, node->line, node->col);
            break;

        case NODE_STRING_LIT:
            printf("String \"%s\" [%d:%d]\n", node->as.string_lit.value, node->line, node->col);
            break;

        case NODE_BOOL_LIT:
            printf("Bool %s [%d:%d]\n", node->as.bool_lit.value ? "true" : "false", node->line, node->col);
            break;
    }
}
