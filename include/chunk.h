/*
 * chunk.h — Bytecode instruction sequences, function objects, and constants pool.
 */

#ifndef PSEUDO_CHUNK_H
#define PSEUDO_CHUNK_H

#include <stdint.h>
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_INDEX,
    OP_SET_INDEX,
    OP_ARRAY_LEN,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_NEG,
    OP_NOT,
    OP_AND,
    OP_OR,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_CALL,
    OP_RETURN,
    OP_OUTPUT,
    OP_INPUT,
    OP_HALT,
} OpCode;

typedef struct {
    uint8_t *code;
    int count;
    int capacity;
    int *lines;
    ValueArray constants;
} Chunk;

typedef struct ObjFunction {
    char *name;
    int arity;
    char **params;
    Chunk chunk;
} ObjFunction;

void chunk_init(Chunk *chunk);
void chunk_write(Chunk *chunk, uint8_t byte, int line);
int chunk_add_constant(Chunk *chunk, Value value);
void chunk_free(Chunk *chunk);

ObjFunction *new_obj_function(const char *name, int arity, char **params);
void free_obj_function(ObjFunction *fn);

/* Disassembler */
void disassemble_chunk(const Chunk *chunk, const char *name);
int disassemble_instruction(const Chunk *chunk, int offset);

#endif /* PSEUDO_CHUNK_H */
