/*
 * ir.h — Intermediate Representation (IR) data structures and interface.
 *
 * Implements the linear Three-Address Code specification from docs/IR_SPEC.md.
 * Both the Bytecode VM (Phase 3) and AOT-to-C (Phase 4) backends consume this IR.
 */

#ifndef PSEUDO_IR_H
#define PSEUDO_IR_H

#include <stdbool.h>
#include "ast.h"
#include "token.h"

typedef enum {
    IR_OP_NONE,
    IR_OP_TEMP,    /* Virtual register: t0, t1, ... */
    IR_OP_VAR,     /* Variable name */
    IR_OP_NUM,     /* Literal number */
    IR_OP_STR,     /* Literal string */
    IR_OP_BOOL,    /* Literal boolean */
    IR_OP_LABEL,   /* Jump label name */
} IROperandKind;

typedef struct {
    IROperandKind kind;
    union {
        int temp_id;
        char *name;
        double num_val;
        char *str_val;
        bool bool_val;
    } as;
} IROperand;

typedef struct {
    IROperand *items;
    int count;
    int capacity;
} IROperandList;

typedef enum {
    IR_CONST,
    IR_LOAD,
    IR_STORE,
    IR_LOAD_IDX,
    IR_STORE_IDX,
    IR_LEN,
    IR_BINOP,
    IR_UNOP,
    IR_LABEL,
    IR_JUMP,
    IR_JUMP_IF_FALSE,
    IR_CALL,
    IR_RETURN,
    IR_INPUT,
    IR_OUTPUT,
    IR_FUNC_BEGIN,
    IR_FUNC_END,
} IROpKind;

typedef struct IRInstruction {
    IROpKind op;
    TokenType sub_op;  /* Operator token for IR_BINOP and IR_UNOP */
    IROperand dst;
    IROperand src1;
    IROperand src2;
    IROperandList args; /* For IR_CALL and IR_FUNC_BEGIN params */
    int line;
    int col;
} IRInstruction;

typedef struct {
    IRInstruction *instructions;
    int count;
    int capacity;
} IRProgram;

/* ------------------------------------------------------------------------ */
/* Operand Constructors & Helpers                                           */
/* ------------------------------------------------------------------------ */

IROperand ir_op_none(void);
IROperand ir_op_temp(int temp_id);
IROperand ir_op_var(const char *name);
IROperand ir_op_num(double value);
IROperand ir_op_str(const char *value);
IROperand ir_op_bool(bool value);
IROperand ir_op_label(const char *name);

void ir_operand_list_init(IROperandList *list);
void ir_operand_list_append(IROperandList *list, IROperand op);
void ir_operand_list_free(IROperandList *list);

/* ------------------------------------------------------------------------ */
/* IR Program Operations                                                    */
/* ------------------------------------------------------------------------ */

void ir_program_init(IRProgram *program);
void ir_program_emit(IRProgram *program, IRInstruction instr);
void ir_program_free(IRProgram *program);
void ir_program_print(const IRProgram *program);

/* ------------------------------------------------------------------------ */
/* AST to IR Lowering                                                       */
/* ------------------------------------------------------------------------ */

IRProgram *ir_generate(const AstNode *ast_root);

#endif /* PSEUDO_IR_H */
