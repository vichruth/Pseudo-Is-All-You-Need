/*
 * ir_gen.c — AST to Intermediate Representation lowering.
 *
 * Implements the lowering table from docs/IR_SPEC.md:
 *   - Expressions lower into virtual temporary registers (t0, t1, ...)
 *   - Control flow (if/for/while/repeat) flattens into explicit jumps and labels
 *   - Functions lower into IR_FUNC_BEGIN / IR_FUNC_END blocks
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "ir.h"

typedef struct {
    IRProgram *program;
    int temp_counter;
    int label_counter;
} IRGen;

static IROperand gen_expression(IRGen *gen, const AstNode *expr);
static void gen_statement(IRGen *gen, const AstNode *stmt);

/* ------------------------------------------------------------------------ */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------ */

static int next_temp(IRGen *gen) {
    return gen->temp_counter++;
}

static char *next_label(IRGen *gen, const char *prefix) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s_%d", prefix, gen->label_counter++);
    size_t len = strlen(buf);
    char *label = malloc(len + 1);
    if (label == NULL) {
        fprintf(stderr, "error: out of memory allocating label\n");
        exit(1);
    }
    memcpy(label, buf, len + 1);
    return label;
}

static void emit_instr(IRGen *gen, IRInstruction instr) {
    ir_program_emit(gen->program, instr);
}

static void emit_label(IRGen *gen, const char *label_name) {
    IRInstruction instr;
    memset(&instr, 0, sizeof(IRInstruction));
    instr.op = IR_LABEL;
    instr.dst = ir_op_label(label_name);
    emit_instr(gen, instr);
}

static void emit_jump(IRGen *gen, const char *target_label) {
    IRInstruction instr;
    memset(&instr, 0, sizeof(IRInstruction));
    instr.op = IR_JUMP;
    instr.dst = ir_op_label(target_label);
    emit_instr(gen, instr);
}

static void emit_jump_if_false(IRGen *gen, IROperand cond, const char *target_label) {
    IRInstruction instr;
    memset(&instr, 0, sizeof(IRInstruction));
    instr.op = IR_JUMP_IF_FALSE;
    instr.src1 = cond;
    instr.dst = ir_op_label(target_label);
    emit_instr(gen, instr);
}

/* ------------------------------------------------------------------------ */
/* Expression Lowering                                                      */
/* ------------------------------------------------------------------------ */

static IROperand gen_expression(IRGen *gen, const AstNode *expr) {
    if (expr == NULL) return ir_op_none();

    switch (expr->kind) {
        case NODE_NUMBER_LIT: {
            int t = next_temp(gen);
            IRInstruction instr;
            memset(&instr, 0, sizeof(IRInstruction));
            instr.op = IR_CONST;
            instr.dst = ir_op_temp(t);
            instr.src1 = ir_op_num(expr->as.number_lit.value);
            instr.line = expr->line;
            instr.col = expr->col;
            emit_instr(gen, instr);
            return ir_op_temp(t);
        }

        case NODE_STRING_LIT: {
            int t = next_temp(gen);
            IRInstruction instr;
            memset(&instr, 0, sizeof(IRInstruction));
            instr.op = IR_CONST;
            instr.dst = ir_op_temp(t);
            instr.src1 = ir_op_str(expr->as.string_lit.value);
            instr.line = expr->line;
            instr.col = expr->col;
            emit_instr(gen, instr);
            return ir_op_temp(t);
        }

        case NODE_BOOL_LIT: {
            int t = next_temp(gen);
            IRInstruction instr;
            memset(&instr, 0, sizeof(IRInstruction));
            instr.op = IR_CONST;
            instr.dst = ir_op_temp(t);
            instr.src1 = ir_op_bool(expr->as.bool_lit.value);
            instr.line = expr->line;
            instr.col = expr->col;
            emit_instr(gen, instr);
            return ir_op_temp(t);
        }

        case NODE_IDENT: {
            int t = next_temp(gen);
            IRInstruction instr;
            memset(&instr, 0, sizeof(IRInstruction));
            instr.op = IR_LOAD;
            instr.dst = ir_op_temp(t);
            instr.src1 = ir_op_var(expr->as.ident.name);
            instr.line = expr->line;
            instr.col = expr->col;
            emit_instr(gen, instr);
            return ir_op_temp(t);
        }

        case NODE_BINARY_OP: {
            IROperand left = gen_expression(gen, expr->as.binary_op.left);
            IROperand right = gen_expression(gen, expr->as.binary_op.right);
            int t = next_temp(gen);
            IRInstruction instr;
            memset(&instr, 0, sizeof(IRInstruction));
            instr.op = IR_BINOP;
            instr.sub_op = expr->as.binary_op.op;
            instr.dst = ir_op_temp(t);
            instr.src1 = left;
            instr.src2 = right;
            instr.line = expr->line;
            instr.col = expr->col;
            emit_instr(gen, instr);
            return ir_op_temp(t);
        }

        case NODE_UNARY_OP: {
            IROperand operand = gen_expression(gen, expr->as.unary_op.operand);
            int t = next_temp(gen);
            IRInstruction instr;
            memset(&instr, 0, sizeof(IRInstruction));
            instr.op = IR_UNOP;
            instr.sub_op = expr->as.unary_op.op;
            instr.dst = ir_op_temp(t);
            instr.src1 = operand;
            instr.line = expr->line;
            instr.col = expr->col;
            emit_instr(gen, instr);
            return ir_op_temp(t);
        }

        case NODE_INDEX: {
            IROperand idx = gen_expression(gen, expr->as.index_expr.index);
            const char *arr_name = expr->as.index_expr.array->as.ident.name;
            int t = next_temp(gen);
            IRInstruction instr;
            memset(&instr, 0, sizeof(IRInstruction));
            instr.op = IR_LOAD_IDX;
            instr.dst = ir_op_temp(t);
            instr.src1 = ir_op_var(arr_name);
            instr.src2 = idx;
            instr.line = expr->line;
            instr.col = expr->col;
            emit_instr(gen, instr);
            return ir_op_temp(t);
        }

        case NODE_MEMBER: {
            const char *obj_name = expr->as.member_expr.object->as.ident.name;
            int t = next_temp(gen);
            IRInstruction instr;
            memset(&instr, 0, sizeof(IRInstruction));
            instr.op = IR_LEN;
            instr.dst = ir_op_temp(t);
            instr.src1 = ir_op_var(obj_name);
            instr.line = expr->line;
            instr.col = expr->col;
            emit_instr(gen, instr);
            return ir_op_temp(t);
        }

        case NODE_CALL: {
            IROperandList args;
            ir_operand_list_init(&args);
            for (int i = 0; i < expr->as.call.args.count; i++) {
                IROperand arg = gen_expression(gen, expr->as.call.args.items[i]);
                ir_operand_list_append(&args, arg);
            }

            int t = next_temp(gen);
            IRInstruction instr;
            memset(&instr, 0, sizeof(IRInstruction));
            instr.op = IR_CALL;
            instr.dst = ir_op_temp(t);
            instr.src1 = ir_op_var(expr->as.call.callee);
            instr.args = args;
            instr.line = expr->line;
            instr.col = expr->col;
            emit_instr(gen, instr);
            return ir_op_temp(t);
        }

        default:
            return ir_op_none();
    }
}

/* ------------------------------------------------------------------------ */
/* Statement Lowering                                                       */
/* ------------------------------------------------------------------------ */

static void gen_statement(IRGen *gen, const AstNode *stmt) {
    if (stmt == NULL) return;

    switch (stmt->kind) {
        case NODE_FUNCTION_DECL: {
            IRInstruction begin_instr;
            memset(&begin_instr, 0, sizeof(IRInstruction));
            begin_instr.op = IR_FUNC_BEGIN;
            begin_instr.dst = ir_op_var(stmt->as.function_decl.name);
            ir_operand_list_init(&begin_instr.args);
            for (int i = 0; i < stmt->as.function_decl.params.count; i++) {
                ir_operand_list_append(&begin_instr.args, ir_op_var(stmt->as.function_decl.params.items[i]));
            }
            emit_instr(gen, begin_instr);

            bool ends_with_return = false;
            for (int i = 0; i < stmt->as.function_decl.body.count; i++) {
                gen_statement(gen, stmt->as.function_decl.body.items[i]);
                if (stmt->as.function_decl.body.items[i]->kind == NODE_RETURN) {
                    ends_with_return = true;
                }
            }

            if (!ends_with_return) {
                IRInstruction ret_instr;
                memset(&ret_instr, 0, sizeof(IRInstruction));
                ret_instr.op = IR_RETURN;
                ret_instr.src1 = ir_op_none();
                emit_instr(gen, ret_instr);
            }

            IRInstruction end_instr;
            memset(&end_instr, 0, sizeof(IRInstruction));
            end_instr.op = IR_FUNC_END;
            emit_instr(gen, end_instr);
            break;
        }

        case NODE_IF: {
            IROperand cond = gen_expression(gen, stmt->as.if_stmt.condition);
            char *else_lbl = next_label(gen, "if_else");
            char *end_lbl = next_label(gen, "if_end");

            emit_jump_if_false(gen, cond, else_lbl);

            for (int i = 0; i < stmt->as.if_stmt.then_branch.count; i++) {
                gen_statement(gen, stmt->as.if_stmt.then_branch.items[i]);
            }

            emit_jump(gen, end_lbl);
            emit_label(gen, else_lbl);

            if (stmt->as.if_stmt.has_else) {
                for (int i = 0; i < stmt->as.if_stmt.else_branch.count; i++) {
                    gen_statement(gen, stmt->as.if_stmt.else_branch.items[i]);
                }
            }

            emit_label(gen, end_lbl);
            free(else_lbl);
            free(end_lbl);
            break;
        }

        case NODE_FOR: {
            /* var = start */
            IROperand start_val = gen_expression(gen, stmt->as.for_stmt.start);
            IRInstruction init_store;
            memset(&init_store, 0, sizeof(IRInstruction));
            init_store.op = IR_STORE;
            init_store.dst = ir_op_var(stmt->as.for_stmt.var_name);
            init_store.src1 = start_val;
            emit_instr(gen, init_store);

            char *start_lbl = next_label(gen, "for_start");
            char *end_lbl = next_label(gen, "for_end");

            emit_label(gen, start_lbl);

            /* check var <= end */
            int t_var = next_temp(gen);
            IRInstruction load_var;
            memset(&load_var, 0, sizeof(IRInstruction));
            load_var.op = IR_LOAD;
            load_var.dst = ir_op_temp(t_var);
            load_var.src1 = ir_op_var(stmt->as.for_stmt.var_name);
            emit_instr(gen, load_var);

            IROperand end_val = gen_expression(gen, stmt->as.for_stmt.end);

            int t_cmp = next_temp(gen);
            IRInstruction cmp_instr;
            memset(&cmp_instr, 0, sizeof(IRInstruction));
            cmp_instr.op = IR_BINOP;
            cmp_instr.sub_op = T_LTE;
            cmp_instr.dst = ir_op_temp(t_cmp);
            cmp_instr.src1 = ir_op_temp(t_var);
            cmp_instr.src2 = end_val;
            emit_instr(gen, cmp_instr);

            emit_jump_if_false(gen, ir_op_temp(t_cmp), end_lbl);

            /* body */
            for (int i = 0; i < stmt->as.for_stmt.body.count; i++) {
                gen_statement(gen, stmt->as.for_stmt.body.items[i]);
            }

            /* increment: var = var + 1 */
            int t_cur = next_temp(gen);
            IRInstruction load_cur;
            memset(&load_cur, 0, sizeof(IRInstruction));
            load_cur.op = IR_LOAD;
            load_cur.dst = ir_op_temp(t_cur);
            load_cur.src1 = ir_op_var(stmt->as.for_stmt.var_name);
            emit_instr(gen, load_cur);

            int t_one = next_temp(gen);
            IRInstruction const_one;
            memset(&const_one, 0, sizeof(IRInstruction));
            const_one.op = IR_CONST;
            const_one.dst = ir_op_temp(t_one);
            const_one.src1 = ir_op_num(1.0);
            emit_instr(gen, const_one);

            int t_next = next_temp(gen);
            IRInstruction inc_instr;
            memset(&inc_instr, 0, sizeof(IRInstruction));
            inc_instr.op = IR_BINOP;
            inc_instr.sub_op = T_PLUS;
            inc_instr.dst = ir_op_temp(t_next);
            inc_instr.src1 = ir_op_temp(t_cur);
            inc_instr.src2 = ir_op_temp(t_one);
            emit_instr(gen, inc_instr);

            IRInstruction store_inc;
            memset(&store_inc, 0, sizeof(IRInstruction));
            store_inc.op = IR_STORE;
            store_inc.dst = ir_op_var(stmt->as.for_stmt.var_name);
            store_inc.src1 = ir_op_temp(t_next);
            emit_instr(gen, store_inc);

            emit_jump(gen, start_lbl);
            emit_label(gen, end_lbl);
            free(start_lbl);
            free(end_lbl);
            break;
        }

        case NODE_WHILE: {
            char *start_lbl = next_label(gen, "while_start");
            char *end_lbl = next_label(gen, "while_end");

            emit_label(gen, start_lbl);
            IROperand cond = gen_expression(gen, stmt->as.while_stmt.condition);
            emit_jump_if_false(gen, cond, end_lbl);

            for (int i = 0; i < stmt->as.while_stmt.body.count; i++) {
                gen_statement(gen, stmt->as.while_stmt.body.items[i]);
            }

            emit_jump(gen, start_lbl);
            emit_label(gen, end_lbl);
            free(start_lbl);
            free(end_lbl);
            break;
        }

        case NODE_REPEAT: {
            char *start_lbl = next_label(gen, "repeat_start");
            emit_label(gen, start_lbl);

            for (int i = 0; i < stmt->as.repeat_stmt.body.count; i++) {
                gen_statement(gen, stmt->as.repeat_stmt.body.items[i]);
            }

            /* Repeat loops until condition is true, so jump back if false! */
            IROperand cond = gen_expression(gen, stmt->as.repeat_stmt.condition);
            emit_jump_if_false(gen, cond, start_lbl);

            free(start_lbl);
            break;
        }

        case NODE_INPUT: {
            IRInstruction instr;
            memset(&instr, 0, sizeof(IRInstruction));
            instr.op = IR_INPUT;
            instr.dst = ir_op_var(stmt->as.input_stmt.var_name);
            instr.line = stmt->line;
            instr.col = stmt->col;
            emit_instr(gen, instr);
            break;
        }

        case NODE_OUTPUT: {
            IROperand val = gen_expression(gen, stmt->as.output_stmt.value);
            IRInstruction instr;
            memset(&instr, 0, sizeof(IRInstruction));
            instr.op = IR_OUTPUT;
            instr.src1 = val;
            instr.line = stmt->line;
            instr.col = stmt->col;
            emit_instr(gen, instr);
            break;
        }

        case NODE_ASSIGN: {
            IROperand val = gen_expression(gen, stmt->as.assign.value);
            if (stmt->as.assign.target->kind == NODE_IDENT) {
                IRInstruction instr;
                memset(&instr, 0, sizeof(IRInstruction));
                instr.op = IR_STORE;
                instr.dst = ir_op_var(stmt->as.assign.target->as.ident.name);
                instr.src1 = val;
                instr.line = stmt->line;
                instr.col = stmt->col;
                emit_instr(gen, instr);
            } else if (stmt->as.assign.target->kind == NODE_INDEX) {
                IROperand idx = gen_expression(gen, stmt->as.assign.target->as.index_expr.index);
                const char *arr_name = stmt->as.assign.target->as.index_expr.array->as.ident.name;
                IRInstruction instr;
                memset(&instr, 0, sizeof(IRInstruction));
                instr.op = IR_STORE_IDX;
                instr.dst = ir_op_var(arr_name);
                instr.src1 = idx;
                instr.src2 = val;
                instr.line = stmt->line;
                instr.col = stmt->col;
                emit_instr(gen, instr);
            }
            break;
        }

        case NODE_RETURN: {
            IROperand val = stmt->as.return_stmt.value != NULL ?
                            gen_expression(gen, stmt->as.return_stmt.value) :
                            ir_op_none();
            IRInstruction instr;
            memset(&instr, 0, sizeof(IRInstruction));
            instr.op = IR_RETURN;
            instr.src1 = val;
            instr.line = stmt->line;
            instr.col = stmt->col;
            emit_instr(gen, instr);
            break;
        }

        case NODE_CALL: {
            gen_expression(gen, stmt);
            break;
        }

        default:
            break;
    }
}

/* ------------------------------------------------------------------------ */
/* Public Interface                                                          */
/* ------------------------------------------------------------------------ */

IRProgram *ir_generate(const AstNode *ast_root) {
    if (ast_root == NULL || ast_root->kind != NODE_PROGRAM) return NULL;

    IRProgram *program = malloc(sizeof(IRProgram));
    if (program == NULL) {
        fprintf(stderr, "error: out of memory allocating IRProgram\n");
        exit(1);
    }
    ir_program_init(program);

    IRGen gen;
    gen.program = program;
    gen.temp_counter = 0;
    gen.label_counter = 0;

    for (int i = 0; i < ast_root->as.program.statements.count; i++) {
        gen_statement(&gen, ast_root->as.program.statements.items[i]);
    }

    return program;
}
