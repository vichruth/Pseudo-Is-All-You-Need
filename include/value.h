/*
 * value.h — Runtime value representations for the Virtual Machine.
 *
 * Implements numbers (double), booleans, strings, and dynamic 1D arrays.
 */

#ifndef PSEUDO_VALUE_H
#define PSEUDO_VALUE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_STRING,
    VAL_ARRAY,
    VAL_FUNCTION,
} ValueType;

struct ObjString;
struct ObjArray;
struct ObjFunction;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        struct ObjString *string;
        struct ObjArray *array;
        struct ObjFunction *function;
    } as;
} Value;

typedef struct ObjString {
    char *chars;
    int length;
} ObjString;

typedef struct ObjArray {
    Value *elements;
    int count;
    int capacity;
} ObjArray;

typedef struct {
    Value *values;
    int count;
    int capacity;
} ValueArray;

/* Value Constructors */
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define STRING_VAL(obj)   ((Value){VAL_STRING, {.string = obj}})
#define ARRAY_VAL(obj)    ((Value){VAL_ARRAY, {.array = obj}})
#define FUNCTION_VAL(obj) ((Value){VAL_FUNCTION, {.function = obj}})

/* Value Extractors */
#define AS_BOOL(value)     ((value).as.boolean)
#define AS_NUMBER(value)   ((value).as.number)
#define AS_STRING(value)   ((value).as.string)
#define AS_ARRAY(value)    ((value).as.array)
#define AS_FUNCTION(value) ((value).as.function)

/* Value Checkers */
#define IS_NIL(value)      ((value).type == VAL_NIL)
#define IS_BOOL(value)     ((value).type == VAL_BOOL)
#define IS_NUMBER(value)   ((value).type == VAL_NUMBER)
#define IS_STRING(value)   ((value).type == VAL_STRING)
#define IS_ARRAY(value)    ((value).type == VAL_ARRAY)
#define IS_FUNCTION(value) ((value).type == VAL_FUNCTION)

/* Heap Object Allocation */
ObjString *copy_obj_string(const char *chars, int length);
ObjArray *new_obj_array(void);
void obj_array_set(ObjArray *array, int index, Value value);
Value obj_array_get(const ObjArray *array, int index);

/* Dynamic Value Array Operations */
void value_array_init(ValueArray *array);
void value_array_write(ValueArray *array, Value value);
void value_array_free(ValueArray *array);

/* Value Operations */
void print_value(Value value);
bool values_equal(Value a, Value b);
bool is_falsey(Value value);

#endif /* PSEUDO_VALUE_H */
