/*
 * compiler.c — Compiles Intermediate Representation (IR) into Bytecode Chunks.
 *
 * Implements operand translation, opcode generation, function encapsulation,
 * and label jump resolution with two-pass backpatching.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "compiler.h"
#include "value.h"

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

typedef struct {
    char *name;
    int offset;
} LabelEntry;

typedef struct {
    int jump_offset;
    char *target_label;
} UnresolvedJump;

typedef struct {
    Chunk *current_chunk;
    char **params;
    int arity;

    LabelEntry *labels;
    int label_count;
    int label_capacity;

    UnresolvedJump *jumps;
    int jump_count;
    int jump_capacity;
} CompilerContext;

/* ------------------------------------------------------------------------ */
/* Context Helpers                                                          */
/* ------------------------------------------------------------------------ */

static void context_init(CompilerContext *ctx, Chunk *chunk, char **params, int arity) {
    ctx->current_chunk = chunk;
    ctx->params = params;
    ctx->arity = arity;
    ctx->labels = NULL;
    ctx->label_count = 0;
    ctx->label_capacity = 0;
    ctx->jumps = NULL;
    ctx->jump_count = 0;
    ctx->jump_capacity = 0;
}

static void context_add_label(CompilerContext *ctx, const char *name, int offset) {
    if (ctx->label_count + 1 > ctx->label_capacity) {
        int new_capacity = ctx->label_capacity < 8 ? 8 : ctx->label_capacity * 2;
        LabelEntry *new_labels = realloc(ctx->labels, sizeof(LabelEntry) * (size_t)new_capacity);
        if (new_labels == NULL) {
            fprintf(stderr, "error: out of memory allocating labels\n");
            exit(1);
        }
        ctx->labels = new_labels;
        ctx->label_capacity = new_capacity;
    }
    ctx->labels[ctx->label_count].name = dup_str(name);
    ctx->labels[ctx->label_count].offset = offset;
    ctx->label_count++;
}

static void context_add_jump(CompilerContext *ctx, int jump_offset, const char *target_label) {
    if (ctx->jump_count + 1 > ctx->jump_capacity) {
        int new_capacity = ctx->jump_capacity < 8 ? 8 : ctx->jump_capacity * 2;
        UnresolvedJump *new_jumps = realloc(ctx->jumps, sizeof(UnresolvedJump) * (size_t)new_capacity);
        if (new_jumps == NULL) {
            fprintf(stderr, "error: out of memory allocating jumps\n");
            exit(1);
        }
        ctx->jumps = new_jumps;
        ctx->jump_capacity = new_capacity;
    }
    ctx->jumps[ctx->jump_count].jump_offset = jump_offset;
    ctx->jumps[ctx->jump_count].target_label = dup_str(target_label);
    ctx->jump_count++;
}

static void context_resolve_jumps(CompilerContext *ctx) {
    for (int i = 0; i < ctx->jump_count; i++) {
        const char *target = ctx->jumps[i].target_label;
        int target_offset = -1;

        for (int j = 0; j < ctx->label_count; j++) {
            if (strcmp(ctx->labels[j].name, target) == 0) {
                target_offset = ctx->labels[j].offset;
                break;
            }
        }

        if (target_offset == -1) {
            fprintf(stderr, "compiler error: unresolved jump target label '%s'\n", target);
            continue;
        }

        int jump_offset = ctx->jumps[i].jump_offset;
        int rel = target_offset - (jump_offset + 2);
        ctx->current_chunk->code[jump_offset] = (uint8_t)((rel >> 8) & 0xff);
        ctx->current_chunk->code[jump_offset + 1] = (uint8_t)(rel & 0xff);
    }
}

static void context_free(CompilerContext *ctx) {
    for (int i = 0; i < ctx->label_count; i++) {
        free(ctx->labels[i].name);
    }
    free(ctx->labels);

    for (int i = 0; i < ctx->jump_count; i++) {
        free(ctx->jumps[i].target_label);
    }
    free(ctx->jumps);
}

/* ------------------------------------------------------------------------ */
/* Bytecode Emission Helpers                                                */
/* ------------------------------------------------------------------------ */

static void emit_byte(Chunk *chunk, uint8_t byte, int line) {
    chunk_write(chunk, byte, line);
}

static void emit_bytes(Chunk *chunk, uint8_t byte1, uint8_t byte2, int line) {
    chunk_write(chunk, byte1, line);
    chunk_write(chunk, byte2, line);
}

static void emit_constant(Chunk *chunk, Value value, int line) {
    int constant = chunk_add_constant(chunk, value);
    emit_bytes(chunk, OP_CONSTANT, (uint8_t)constant, line);
}

static void emit_load_operand(CompilerContext *ctx, IROperand op, int line) {
    Chunk *chunk = ctx->current_chunk;
    switch (op.kind) {
        case IR_OP_NONE:
            emit_byte(chunk, OP_NIL, line);
            break;
        case IR_OP_TEMP: {
            uint8_t slot = (uint8_t)(ctx->arity + op.as.temp_id);
            emit_bytes(chunk, OP_GET_LOCAL, slot, line);
            break;
        }
        case IR_OP_VAR: {
            int param_idx = -1;
            if (ctx->params != NULL) {
                for (int i = 0; i < ctx->arity; i++) {
                    if (strcmp(ctx->params[i], op.as.name) == 0) {
                        param_idx = i;
                        break;
                    }
                }
            }
            if (param_idx != -1) {
                uint8_t slot = (uint8_t)param_idx;
                emit_bytes(chunk, OP_GET_LOCAL, slot, line);
            } else {
                ObjString *str = copy_obj_string(op.as.name, (int)strlen(op.as.name));
                int const_idx = chunk_add_constant(chunk, STRING_VAL(str));
                emit_bytes(chunk, OP_GET_GLOBAL, (uint8_t)const_idx, line);
            }
            break;
        }
        case IR_OP_NUM:
            emit_constant(chunk, NUMBER_VAL(op.as.num_val), line);
            break;
        case IR_OP_STR: {
            ObjString *str = copy_obj_string(op.as.str_val, (int)strlen(op.as.str_val));
            emit_constant(chunk, STRING_VAL(str), line);
            break;
        }
        case IR_OP_BOOL:
            emit_byte(chunk, op.as.bool_val ? OP_TRUE : OP_FALSE, line);
            break;
        case IR_OP_LABEL:
            break;
    }
}

static void emit_store_operand(CompilerContext *ctx, IROperand op, int line) {
    Chunk *chunk = ctx->current_chunk;
    switch (op.kind) {
        case IR_OP_TEMP: {
            uint8_t slot = (uint8_t)(ctx->arity + op.as.temp_id);
            emit_bytes(chunk, OP_SET_LOCAL, slot, line);
            emit_byte(chunk, OP_POP, line);
            break;
        }
        case IR_OP_VAR: {
            int param_idx = -1;
            if (ctx->params != NULL) {
                for (int i = 0; i < ctx->arity; i++) {
                    if (strcmp(ctx->params[i], op.as.name) == 0) {
                        param_idx = i;
                        break;
                    }
                }
            }
            if (param_idx != -1) {
                uint8_t slot = (uint8_t)param_idx;
                emit_bytes(chunk, OP_SET_LOCAL, slot, line);
                emit_byte(chunk, OP_POP, line);
            } else {
                ObjString *str = copy_obj_string(op.as.name, (int)strlen(op.as.name));
                int const_idx = chunk_add_constant(chunk, STRING_VAL(str));
                emit_bytes(chunk, OP_SET_GLOBAL, (uint8_t)const_idx, line);
                emit_byte(chunk, OP_POP, line);
            }
            break;
        }
        default:
            break;
    }
}

/* ------------------------------------------------------------------------ */
/* Instruction Compilation                                                  */
/* ------------------------------------------------------------------------ */

static void compile_instruction(CompilerContext *ctx, const IRInstruction *instr) {
    Chunk *chunk = ctx->current_chunk;
    int line = instr->line;

    switch (instr->op) {
        case IR_CONST:
            emit_load_operand(ctx, instr->src1, line);
            emit_store_operand(ctx, instr->dst, line);
            break;

        case IR_LOAD:
            emit_load_operand(ctx, instr->src1, line);
            emit_store_operand(ctx, instr->dst, line);
            break;

        case IR_STORE:
            emit_load_operand(ctx, instr->src1, line);
            emit_store_operand(ctx, instr->dst, line);
            break;

        case IR_LOAD_IDX:
            emit_load_operand(ctx, instr->src1, line);
            emit_load_operand(ctx, instr->src2, line);
            emit_byte(chunk, OP_GET_INDEX, line);
            emit_store_operand(ctx, instr->dst, line);
            break;

        case IR_STORE_IDX:
            emit_load_operand(ctx, instr->dst, line);
            emit_load_operand(ctx, instr->src1, line);
            emit_load_operand(ctx, instr->src2, line);
            emit_byte(chunk, OP_SET_INDEX, line);
            emit_byte(chunk, OP_POP, line);
            break;

        case IR_LEN:
            emit_load_operand(ctx, instr->src1, line);
            emit_byte(chunk, OP_ARRAY_LEN, line);
            emit_store_operand(ctx, instr->dst, line);
            break;

        case IR_BINOP:
            emit_load_operand(ctx, instr->src1, line);
            emit_load_operand(ctx, instr->src2, line);
            switch (instr->sub_op) {
                case T_PLUS:  emit_byte(chunk, OP_ADD, line); break;
                case T_MINUS: emit_byte(chunk, OP_SUB, line); break;
                case T_STAR:  emit_byte(chunk, OP_MUL, line); break;
                case T_SLASH: emit_byte(chunk, OP_DIV, line); break;
                case T_MOD:   emit_byte(chunk, OP_MOD, line); break;
                case T_EQ:    emit_byte(chunk, OP_EQUAL, line); break;
                case T_NEQ:   emit_byte(chunk, OP_NOT_EQUAL, line); break;
                case T_LT:    emit_byte(chunk, OP_LESS, line); break;
                case T_LTE:   emit_byte(chunk, OP_LESS_EQUAL, line); break;
                case T_GT:    emit_byte(chunk, OP_GREATER, line); break;
                case T_GTE:   emit_byte(chunk, OP_GREATER_EQUAL, line); break;
                case T_AND:   emit_byte(chunk, OP_AND, line); break;
                case T_OR:    emit_byte(chunk, OP_OR, line); break;
                default: break;
            }
            emit_store_operand(ctx, instr->dst, line);
            break;

        case IR_UNOP:
            emit_load_operand(ctx, instr->src1, line);
            switch (instr->sub_op) {
                case T_MINUS: emit_byte(chunk, OP_NEG, line); break;
                case T_NOT:   emit_byte(chunk, OP_NOT, line); break;
                default: break;
            }
            emit_store_operand(ctx, instr->dst, line);
            break;

        case IR_LABEL:
            context_add_label(ctx, instr->dst.as.name, chunk->count);
            break;

        case IR_JUMP: {
            emit_byte(chunk, OP_JUMP, line);
            int jump_pos = chunk->count;
            emit_bytes(chunk, 0xff, 0xff, line);
            context_add_jump(ctx, jump_pos, instr->dst.as.name);
            break;
        }

        case IR_JUMP_IF_FALSE: {
            emit_load_operand(ctx, instr->src1, line);
            emit_byte(chunk, OP_JUMP_IF_FALSE, line);
            int jump_pos = chunk->count;
            emit_bytes(chunk, 0xff, 0xff, line);
            context_add_jump(ctx, jump_pos, instr->dst.as.name);
            break;
        }

        case IR_CALL: {
            for (int i = 0; i < instr->args.count; i++) {
                emit_load_operand(ctx, instr->args.items[i], line);
            }
            emit_load_operand(ctx, instr->src1, line);
            emit_bytes(chunk, OP_CALL, (uint8_t)instr->args.count, line);
            if (instr->dst.kind != IR_OP_NONE) {
                emit_store_operand(ctx, instr->dst, line);
            } else {
                emit_byte(chunk, OP_POP, line);
            }
            break;
        }

        case IR_RETURN:
            if (instr->src1.kind != IR_OP_NONE) {
                emit_load_operand(ctx, instr->src1, line);
            } else {
                emit_byte(chunk, OP_NIL, line);
            }
            emit_byte(chunk, OP_RETURN, line);
            break;

        case IR_INPUT:
            emit_byte(chunk, OP_INPUT, line);
            emit_store_operand(ctx, instr->dst, line);
            break;

        case IR_OUTPUT:
            emit_load_operand(ctx, instr->src1, line);
            emit_byte(chunk, OP_OUTPUT, line);
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------------ */
/* Public Interface                                                          */
/* ------------------------------------------------------------------------ */

ObjFunction *compile_ir(const IRProgram *program) {
    if (program == NULL) return NULL;

    ObjFunction *main_fn = new_obj_function("<main>", 0, NULL);
    CompilerContext main_ctx;
    context_init(&main_ctx, &main_fn->chunk, NULL, 0);

    for (int i = 0; i < program->count; i++) {
        const IRInstruction *instr = &program->instructions[i];

        if (instr->op == IR_FUNC_BEGIN) {
            const char *fn_name = instr->dst.as.name;
            int arity = instr->args.count;
            char **param_names = malloc(sizeof(char *) * (size_t)(arity > 0 ? arity : 1));
            for (int p = 0; p < arity; p++) {
                param_names[p] = instr->args.items[p].as.name;
            }

            ObjFunction *fn = new_obj_function(fn_name, arity, param_names);

            CompilerContext fn_ctx;
            context_init(&fn_ctx, &fn->chunk, param_names, arity);

            i++;
            while (i < program->count && program->instructions[i].op != IR_FUNC_END) {
                compile_instruction(&fn_ctx, &program->instructions[i]);
                i++;
            }

            emit_byte(&fn->chunk, OP_NIL, instr->line);
            emit_byte(&fn->chunk, OP_RETURN, instr->line);

            context_resolve_jumps(&fn_ctx);
            context_free(&fn_ctx);
            free(param_names);

            int fn_const = chunk_add_constant(&main_fn->chunk, FUNCTION_VAL(fn));
            emit_bytes(&main_fn->chunk, OP_CONSTANT, (uint8_t)fn_const, instr->line);
            ObjString *name_str = copy_obj_string(fn_name, (int)strlen(fn_name));
            int name_const = chunk_add_constant(&main_fn->chunk, STRING_VAL(name_str));
            emit_bytes(&main_fn->chunk, OP_SET_GLOBAL, (uint8_t)name_const, instr->line);
            emit_byte(&main_fn->chunk, OP_POP, instr->line);
            continue;
        }

        compile_instruction(&main_ctx, instr);
    }

    emit_byte(&main_fn->chunk, OP_HALT, 1);
    context_resolve_jumps(&main_ctx);
    context_free(&main_ctx);

    return main_fn;
}
