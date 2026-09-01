/*
 * vm.h — Stack-based Virtual Machine execution engine.
 */

#ifndef PSEUDO_VM_H
#define PSEUDO_VM_H

#include "chunk.h"
#include "value.h"

#define FRAMES_MAX 64
#define LOCALS_MAX 256
#define STACK_MAX (FRAMES_MAX * 256)

typedef struct {
    ObjFunction *function;
    uint8_t *ip;
    Value locals[LOCALS_MAX];
} CallFrame;

typedef struct {
    char *key;
    Value value;
} GlobalEntry;

typedef struct {
    GlobalEntry *entries;
    int count;
    int capacity;
} Table;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frame_count;

    Value stack[STACK_MAX];
    Value *stack_top;

    Table globals;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void vm_init(VM *vm);
void vm_free(VM *vm);
InterpretResult vm_interpret(VM *vm, ObjFunction *function);

#endif /* PSEUDO_VM_H */
