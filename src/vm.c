/*
 * vm.c — Stack-based Virtual Machine interpreter.
 *
 * Implements value stack operations, call frames, globals table, arithmetic,
 * string concatenation, array runtime semantics, and I/O.
 */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "value.h"
#include "vm.h"

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
/* Globals Table                                                            */
/* ------------------------------------------------------------------------ */

static void table_init(Table *table) {
    table->entries = NULL;
    table->count = 0;
    table->capacity = 0;
}

static void table_set(Table *table, const char *key, Value value) {
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->entries[i].key, key) == 0) {
            table->entries[i].value = value;
            return;
        }
    }

    if (table->count + 1 > table->capacity) {
        int new_capacity = table->capacity < 16 ? 16 : table->capacity * 2;
        GlobalEntry *new_entries = realloc(table->entries, sizeof(GlobalEntry) * (size_t)new_capacity);
        if (new_entries == NULL) {
            fprintf(stderr, "error: out of memory expanding globals table\n");
            exit(1);
        }
        table->entries = new_entries;
        table->capacity = new_capacity;
    }

    table->entries[table->count].key = dup_str(key);
    table->entries[table->count].value = value;
    table->count++;
}

static bool table_get(Table *table, const char *key, Value *value) {
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->entries[i].key, key) == 0) {
            *value = table->entries[i].value;
            return true;
        }
    }
    return false;
}

static void table_free(Table *table) {
    if (table == NULL || table->entries == NULL) return;
    for (int i = 0; i < table->count; i++) {
        free(table->entries[i].key);
    }
    free(table->entries);
    table->entries = NULL;
    table->count = 0;
    table->capacity = 0;
}

/* ------------------------------------------------------------------------ */
/* Stack Operations                                                         */
/* ------------------------------------------------------------------------ */

static void reset_stack(VM *vm) {
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
}

static void push(VM *vm, Value value) {
    *vm->stack_top = value;
    vm->stack_top++;
}

static Value pop(VM *vm) {
    vm->stack_top--;
    return *vm->stack_top;
}

static Value peek(VM *vm, int distance) {
    return vm->stack_top[-1 - distance];
}

/* ------------------------------------------------------------------------ */
/* VM Initialization & Destruction                                          */
/* ------------------------------------------------------------------------ */

void vm_init(VM *vm) {
    reset_stack(vm);
    table_init(&vm->globals);
}

void vm_free(VM *vm) {
    table_free(&vm->globals);
}

/* ------------------------------------------------------------------------ */
/* Interpreter Loop                                                         */
/* ------------------------------------------------------------------------ */

static InterpretResult run(VM *vm) {
    CallFrame *frame = &vm->frames[vm->frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (int16_t)((int16_t)(frame->ip[-2] << 8) | (int16_t)(frame->ip[-1] & 0xff)))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() (AS_STRING(READ_CONSTANT()))

    for (;;) {
        uint8_t instruction = READ_BYTE();

        switch (instruction) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(vm, constant);
                break;
            }

            case OP_NIL:
                push(vm, NIL_VAL);
                break;

            case OP_TRUE:
                push(vm, BOOL_VAL(true));
                break;

            case OP_FALSE:
                push(vm, BOOL_VAL(false));
                break;

            case OP_POP:
                pop(vm);
                break;

            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(vm, frame->locals[slot]);
                break;
            }

            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->locals[slot] = peek(vm, 0);
                break;
            }

            case OP_GET_GLOBAL: {
                ObjString *name = READ_STRING();
                Value value;
                if (!table_get(&vm->globals, name->chars, &value) || IS_NIL(value)) {
                    ObjArray *arr = new_obj_array();
                    value = ARRAY_VAL(arr);
                    table_set(&vm->globals, name->chars, value);
                }
                push(vm, value);
                break;
            }

            case OP_SET_GLOBAL: {
                ObjString *name = READ_STRING();
                table_set(&vm->globals, name->chars, peek(vm, 0));
                break;
            }

            case OP_GET_INDEX: {
                Value index_val = pop(vm);
                Value array_val = pop(vm);
                if (!IS_ARRAY(array_val)) {
                    push(vm, NIL_VAL);
                } else {
                    int idx = (int)AS_NUMBER(index_val);
                    push(vm, obj_array_get(AS_ARRAY(array_val), idx));
                }
                break;
            }

            case OP_SET_INDEX: {
                Value val = pop(vm);
                Value index_val = pop(vm);
                Value array_val = pop(vm);
                if (IS_ARRAY(array_val)) {
                    int idx = (int)AS_NUMBER(index_val);
                    obj_array_set(AS_ARRAY(array_val), idx, val);
                }
                push(vm, val);
                break;
            }

            case OP_ARRAY_LEN: {
                Value array_val = pop(vm);
                if (IS_ARRAY(array_val)) {
                    push(vm, NUMBER_VAL(AS_ARRAY(array_val)->count));
                } else {
                    push(vm, NUMBER_VAL(0));
                }
                break;
            }

            case OP_ADD: {
                Value b = pop(vm);
                Value a = pop(vm);
                if (IS_STRING(a) && IS_STRING(b)) {
                    int len = AS_STRING(a)->length + AS_STRING(b)->length;
                    char *buf = malloc((size_t)len + 1);
                    memcpy(buf, AS_STRING(a)->chars, (size_t)AS_STRING(a)->length);
                    memcpy(buf + AS_STRING(a)->length, AS_STRING(b)->chars, (size_t)AS_STRING(b)->length);
                    buf[len] = '\0';
                    ObjString *str = copy_obj_string(buf, len);
                    free(buf);
                    push(vm, STRING_VAL(str));
                } else if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    push(vm, NUMBER_VAL(AS_NUMBER(a) + AS_NUMBER(b)));
                } else {
                    push(vm, NUMBER_VAL(0));
                }
                break;
            }

            case OP_SUB: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, NUMBER_VAL(AS_NUMBER(a) - AS_NUMBER(b)));
                break;
            }

            case OP_MUL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, NUMBER_VAL(AS_NUMBER(a) * AS_NUMBER(b)));
                break;
            }

            case OP_DIV: {
                Value b = pop(vm);
                Value a = pop(vm);
                double denom = AS_NUMBER(b);
                push(vm, NUMBER_VAL(denom != 0.0 ? AS_NUMBER(a) / denom : 0.0));
                break;
            }

            case OP_MOD: {
                Value b = pop(vm);
                Value a = pop(vm);
                double denom = AS_NUMBER(b);
                push(vm, NUMBER_VAL(denom != 0.0 ? fmod(AS_NUMBER(a), denom) : 0.0));
                break;
            }

            case OP_NEG: {
                Value a = pop(vm);
                push(vm, NUMBER_VAL(-AS_NUMBER(a)));
                break;
            }

            case OP_NOT: {
                Value a = pop(vm);
                push(vm, BOOL_VAL(is_falsey(a)));
                break;
            }

            case OP_AND: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(!is_falsey(a) && !is_falsey(b)));
                break;
            }

            case OP_OR: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(!is_falsey(a) || !is_falsey(b)));
                break;
            }

            case OP_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(values_equal(a, b)));
                break;
            }

            case OP_NOT_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(!values_equal(a, b)));
                break;
            }

            case OP_GREATER: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(AS_NUMBER(a) > AS_NUMBER(b)));
                break;
            }

            case OP_GREATER_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(AS_NUMBER(a) >= AS_NUMBER(b)));
                break;
            }

            case OP_LESS: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(AS_NUMBER(a) < AS_NUMBER(b)));
                break;
            }

            case OP_LESS_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(AS_NUMBER(a) <= AS_NUMBER(b)));
                break;
            }

            case OP_JUMP: {
                int16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }

            case OP_JUMP_IF_FALSE: {
                int16_t offset = READ_SHORT();
                Value cond = pop(vm);
                if (is_falsey(cond)) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_CALL: {
                int arg_count = READ_BYTE();
                Value callee = pop(vm);
                if (!IS_FUNCTION(callee)) {
                    fprintf(stderr, "runtime error: can only call functions\n");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (vm->frame_count == FRAMES_MAX) {
                    fprintf(stderr, "runtime error: recursion stack overflow (> %d frames)\n", FRAMES_MAX);
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjFunction *func = AS_FUNCTION(callee);
                CallFrame *new_frame = &vm->frames[vm->frame_count++];
                new_frame->function = func;
                new_frame->ip = func->chunk.code;
                memset(new_frame->locals, 0, sizeof(new_frame->locals));

                for (int p = arg_count - 1; p >= 0; p--) {
                    new_frame->locals[p] = pop(vm);
                }

                frame = new_frame;
                break;
            }

            case OP_RETURN: {
                Value result = pop(vm);
                vm->frame_count--;
                if (vm->frame_count == 0) {
                    return INTERPRET_OK;
                }

                push(vm, result);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_OUTPUT: {
                Value val = pop(vm);
                print_value(val);
                printf("\n");
                break;
            }

            case OP_INPUT: {
                char buffer[512];
                if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                    size_t len = strlen(buffer);
                    if (len > 0 && buffer[len - 1] == '\n') {
                        buffer[len - 1] = '\0';
                        len--;
                    }
                    char *endptr;
                    double num = strtod(buffer, &endptr);
                    if (*buffer != '\0' && *endptr == '\0') {
                        push(vm, NUMBER_VAL(num));
                    } else {
                        ObjString *str = copy_obj_string(buffer, (int)len);
                        push(vm, STRING_VAL(str));
                    }
                } else {
                    push(vm, NIL_VAL);
                }
                break;
            }

            case OP_HALT:
                return INTERPRET_OK;

            default:
                fprintf(stderr, "runtime error: unknown opcode %d\n", instruction);
                return INTERPRET_RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
}

InterpretResult vm_interpret(VM *vm, ObjFunction *function) {
    if (function == NULL) return INTERPRET_RUNTIME_ERROR;

    CallFrame *frame = &vm->frames[vm->frame_count++];
    frame->function = function;
    frame->ip = function->chunk.code;
    memset(frame->locals, 0, sizeof(frame->locals));

    InterpretResult result = run(vm);
    return result;
}
