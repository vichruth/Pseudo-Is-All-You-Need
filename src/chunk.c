/*
 * chunk.c — Bytecode chunk implementation and disassembler.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"

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

void chunk_init(Chunk *chunk) {
    chunk->code = NULL;
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->lines = NULL;
    value_array_init(&chunk->constants);
}

void chunk_write(Chunk *chunk, uint8_t byte, int line) {
    if (chunk->count + 1 > chunk->capacity) {
        int new_capacity = chunk->capacity < 8 ? 8 : chunk->capacity * 2;
        uint8_t *new_code = realloc(chunk->code, sizeof(uint8_t) * (size_t)new_capacity);
        int *new_lines = realloc(chunk->lines, sizeof(int) * (size_t)new_capacity);
        if (new_code == NULL || new_lines == NULL) {
            fprintf(stderr, "error: out of memory expanding Chunk\n");
            exit(1);
        }
        chunk->code = new_code;
        chunk->lines = new_lines;
        chunk->capacity = new_capacity;
    }
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int chunk_add_constant(Chunk *chunk, Value value) {
    value_array_write(&chunk->constants, value);
    return chunk->constants.count - 1;
}

void chunk_free(Chunk *chunk) {
    if (chunk == NULL) return;
    free(chunk->code);
    free(chunk->lines);
    value_array_free(&chunk->constants);
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->count = 0;
    chunk->capacity = 0;
}

ObjFunction *new_obj_function(const char *name, int arity, char **params) {
    ObjFunction *fn = malloc(sizeof(ObjFunction));
    if (fn == NULL) {
        fprintf(stderr, "error: out of memory allocating ObjFunction\n");
        exit(1);
    }
    fn->name = name != NULL ? dup_str(name) : NULL;
    fn->arity = arity;
    if (arity > 0 && params != NULL) {
        fn->params = malloc(sizeof(char *) * (size_t)arity);
        if (fn->params == NULL) {
            fprintf(stderr, "error: out of memory allocating function params\n");
            exit(1);
        }
        for (int i = 0; i < arity; i++) {
            fn->params[i] = dup_str(params[i]);
        }
    } else {
        fn->params = NULL;
    }
    chunk_init(&fn->chunk);
    return fn;
}

void free_obj_function(ObjFunction *fn) {
    if (fn == NULL) return;
    free(fn->name);
    if (fn->params != NULL) {
        for (int i = 0; i < fn->arity; i++) {
            free(fn->params[i]);
        }
        free(fn->params);
    }
    chunk_free(&fn->chunk);
    free(fn);
}

/* ------------------------------------------------------------------------ */
/* Disassembler                                                             */
/* ------------------------------------------------------------------------ */

static int simple_instruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int constant_instruction(const char *name, const Chunk *chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int byte_instruction(const char *name, const Chunk *chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jump_instruction(const char *name, int sign, const Chunk *chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

int disassemble_instruction(const Chunk *chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_NIL:
            return simple_instruction("OP_NIL", offset);
        case OP_TRUE:
            return simple_instruction("OP_TRUE", offset);
        case OP_FALSE:
            return simple_instruction("OP_FALSE", offset);
        case OP_POP:
            return simple_instruction("OP_POP", offset);
        case OP_GET_LOCAL:
            return byte_instruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byte_instruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constant_instruction("OP_GET_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return constant_instruction("OP_SET_GLOBAL", chunk, offset);
        case OP_GET_INDEX:
            return simple_instruction("OP_GET_INDEX", offset);
        case OP_SET_INDEX:
            return simple_instruction("OP_SET_INDEX", offset);
        case OP_ARRAY_LEN:
            return simple_instruction("OP_ARRAY_LEN", offset);
        case OP_ADD:
            return simple_instruction("OP_ADD", offset);
        case OP_SUB:
            return simple_instruction("OP_SUB", offset);
        case OP_MUL:
            return simple_instruction("OP_MUL", offset);
        case OP_DIV:
            return simple_instruction("OP_DIV", offset);
        case OP_MOD:
            return simple_instruction("OP_MOD", offset);
        case OP_NEG:
            return simple_instruction("OP_NEG", offset);
        case OP_NOT:
            return simple_instruction("OP_NOT", offset);
        case OP_AND:
            return simple_instruction("OP_AND", offset);
        case OP_OR:
            return simple_instruction("OP_OR", offset);
        case OP_EQUAL:
            return simple_instruction("OP_EQUAL", offset);
        case OP_NOT_EQUAL:
            return simple_instruction("OP_NOT_EQUAL", offset);
        case OP_GREATER:
            return simple_instruction("OP_GREATER", offset);
        case OP_GREATER_EQUAL:
            return simple_instruction("OP_GREATER_EQUAL", offset);
        case OP_LESS:
            return simple_instruction("OP_LESS", offset);
        case OP_LESS_EQUAL:
            return simple_instruction("OP_LESS_EQUAL", offset);
        case OP_JUMP:
            return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_CALL:
            return byte_instruction("OP_CALL", chunk, offset);
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        case OP_OUTPUT:
            return simple_instruction("OP_OUTPUT", offset);
        case OP_INPUT:
            return simple_instruction("OP_INPUT", offset);
        case OP_HALT:
            return simple_instruction("OP_HALT", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

void disassemble_chunk(const Chunk *chunk, const char *name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
    }
}
