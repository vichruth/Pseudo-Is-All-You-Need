/*
 * ir.c — Intermediate Representation (IR) memory management and disassembly.
 *
 * Implements operand construction, instruction list growth, memory cleanup,
 * and pretty-printing of linear 3-address code instructions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.h"

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
/* Operand Constructors                                                     */
/* ------------------------------------------------------------------------ */

IROperand ir_op_none(void) {
    IROperand op;
    op.kind = IR_OP_NONE;
    return op;
}

IROperand ir_op_temp(int temp_id) {
    IROperand op;
    op.kind = IR_OP_TEMP;
    op.as.temp_id = temp_id;
    return op;
}

IROperand ir_op_var(const char *name) {
    IROperand op;
    op.kind = IR_OP_VAR;
    op.as.name = dup_str(name);
    return op;
}

IROperand ir_op_num(double value) {
    IROperand op;
    op.kind = IR_OP_NUM;
    op.as.num_val = value;
    return op;
}

IROperand ir_op_str(const char *value) {
    IROperand op;
    op.kind = IR_OP_STR;
    op.as.str_val = dup_str(value);
    return op;
}

IROperand ir_op_bool(bool value) {
    IROperand op;
    op.kind = IR_OP_BOOL;
    op.as.bool_val = value;
    return op;
}

IROperand ir_op_label(const char *name) {
    IROperand op;
    op.kind = IR_OP_LABEL;
    op.as.name = dup_str(name);
    return op;
}

static void ir_op_free(IROperand *op) {
    if (op->kind == IR_OP_VAR || op->kind == IR_OP_LABEL) {
        free(op->as.name);
        op->as.name = NULL;
    } else if (op->kind == IR_OP_STR) {
        free(op->as.str_val);
        op->as.str_val = NULL;
    }
}

/* ------------------------------------------------------------------------ */
/* Operand List Operations                                                  */
/* ------------------------------------------------------------------------ */

void ir_operand_list_init(IROperandList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void ir_operand_list_append(IROperandList *list, IROperand op) {
    if (list->count + 1 > list->capacity) {
        int new_capacity = list->capacity < 8 ? 8 : list->capacity * 2;
        IROperand *new_items = realloc(list->items, sizeof(IROperand) * (size_t)new_capacity);
        if (new_items == NULL) {
            fprintf(stderr, "error: out of memory expanding IROperandList\n");
            exit(1);
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->count++] = op;
}

void ir_operand_list_free(IROperandList *list) {
    if (list == NULL || list->items == NULL) return;
    for (int i = 0; i < list->count; i++) {
        ir_op_free(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* ------------------------------------------------------------------------ */
/* Program Operations                                                       */
/* ------------------------------------------------------------------------ */

void ir_program_init(IRProgram *program) {
    program->instructions = NULL;
    program->count = 0;
    program->capacity = 0;
}

void ir_program_emit(IRProgram *program, IRInstruction instr) {
    if (program->count + 1 > program->capacity) {
        int new_capacity = program->capacity < 16 ? 16 : program->capacity * 2;
        IRInstruction *new_instrs = realloc(program->instructions, sizeof(IRInstruction) * (size_t)new_capacity);
        if (new_instrs == NULL) {
            fprintf(stderr, "error: out of memory expanding IRProgram\n");
            exit(1);
        }
        program->instructions = new_instrs;
        program->capacity = new_capacity;
    }
    program->instructions[program->count++] = instr;
}

void ir_program_free(IRProgram *program) {
    if (program == NULL || program->instructions == NULL) return;
    for (int i = 0; i < program->count; i++) {
        ir_op_free(&program->instructions[i].dst);
        ir_op_free(&program->instructions[i].src1);
        ir_op_free(&program->instructions[i].src2);
        ir_operand_list_free(&program->instructions[i].args);
    }
    free(program->instructions);
    program->instructions = NULL;
    program->count = 0;
    program->capacity = 0;
}

/* ------------------------------------------------------------------------ */
/* Disassembly & Printing                                                   */
/* ------------------------------------------------------------------------ */

static void print_operand(const IROperand *op) {
    switch (op->kind) {
        case IR_OP_NONE:
            printf("_");
            break;
        case IR_OP_TEMP:
            printf("t%d", op->as.temp_id);
            break;
        case IR_OP_VAR:
            printf("%s", op->as.name);
            break;
        case IR_OP_NUM:
            printf("%g", op->as.num_val);
            break;
        case IR_OP_STR:
            printf("\"%s\"", op->as.str_val);
            break;
        case IR_OP_BOOL:
            printf("%s", op->as.bool_val ? "true" : "false");
            break;
        case IR_OP_LABEL:
            printf("%s", op->as.name);
            break;
    }
}

void ir_program_print(const IRProgram *program) {
    if (program == NULL) return;

    for (int i = 0; i < program->count; i++) {
        const IRInstruction *instr = &program->instructions[i];

        if (instr->op == IR_LABEL) {
            printf("%s:\n", instr->dst.as.name);
            continue;
        }

        printf("    ");
        switch (instr->op) {
            case IR_CONST:
                print_operand(&instr->dst);
                printf(" = const ");
                print_operand(&instr->src1);
                break;

            case IR_LOAD:
                print_operand(&instr->dst);
                printf(" = load ");
                print_operand(&instr->src1);
                break;

            case IR_STORE:
                printf("store ");
                print_operand(&instr->dst);
                printf(", ");
                print_operand(&instr->src1);
                break;

            case IR_LOAD_IDX:
                print_operand(&instr->dst);
                printf(" = load_idx ");
                print_operand(&instr->src1);
                printf("[");
                print_operand(&instr->src2);
                printf("]");
                break;

            case IR_STORE_IDX:
                printf("store_idx ");
                print_operand(&instr->dst);
                printf("[");
                print_operand(&instr->src1);
                printf("], ");
                print_operand(&instr->src2);
                break;

            case IR_LEN:
                print_operand(&instr->dst);
                printf(" = len ");
                print_operand(&instr->src1);
                break;

            case IR_BINOP:
                print_operand(&instr->dst);
                printf(" = binop %s ", token_type_name(instr->sub_op));
                print_operand(&instr->src1);
                printf(", ");
                print_operand(&instr->src2);
                break;

            case IR_UNOP:
                print_operand(&instr->dst);
                printf(" = unop %s ", token_type_name(instr->sub_op));
                print_operand(&instr->src1);
                break;

            case IR_JUMP:
                printf("jump ");
                print_operand(&instr->dst);
                break;

            case IR_JUMP_IF_FALSE:
                printf("jump_if_false ");
                print_operand(&instr->src1);
                printf(", ");
                print_operand(&instr->dst);
                break;

            case IR_CALL:
                if (instr->dst.kind != IR_OP_NONE) {
                    print_operand(&instr->dst);
                    printf(" = ");
                }
                printf("call %s(", instr->src1.as.name);
                for (int j = 0; j < instr->args.count; j++) {
                    print_operand(&instr->args.items[j]);
                    if (j + 1 < instr->args.count) printf(", ");
                }
                printf(")");
                break;

            case IR_RETURN:
                printf("return");
                if (instr->src1.kind != IR_OP_NONE) {
                    printf(" ");
                    print_operand(&instr->src1);
                }
                break;

            case IR_INPUT:
                printf("input ");
                print_operand(&instr->dst);
                break;

            case IR_OUTPUT:
                printf("output ");
                print_operand(&instr->src1);
                break;

            case IR_FUNC_BEGIN:
                printf("func_begin %s(", instr->dst.as.name);
                for (int j = 0; j < instr->args.count; j++) {
                    print_operand(&instr->args.items[j]);
                    if (j + 1 < instr->args.count) printf(", ");
                }
                printf(")");
                break;

            case IR_FUNC_END:
                printf("func_end");
                break;

            default:
                break;
        }
        printf("\n");
    }
}
