/*
 * value.c — VM runtime value implementations, dynamic arrays, and string objects.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "value.h"

/* ------------------------------------------------------------------------ */
/* Heap Object Allocation                                                   */
/* ------------------------------------------------------------------------ */

ObjString *copy_obj_string(const char *chars, int length) {
    ObjString *string = malloc(sizeof(ObjString));
    if (string == NULL) {
        fprintf(stderr, "error: out of memory allocating ObjString\n");
        exit(1);
    }
    string->length = length;
    string->chars = malloc((size_t)length + 1);
    if (string->chars == NULL) {
        fprintf(stderr, "error: out of memory allocating string chars\n");
        exit(1);
    }
    memcpy(string->chars, chars, (size_t)length);
    string->chars[length] = '\0';
    return string;
}

ObjArray *new_obj_array(void) {
    ObjArray *array = malloc(sizeof(ObjArray));
    if (array == NULL) {
        fprintf(stderr, "error: out of memory allocating ObjArray\n");
        exit(1);
    }
    array->elements = NULL;
    array->count = 0;
    array->capacity = 0;
    return array;
}

void obj_array_set(ObjArray *array, int index, Value value) {
    if (index < 0) {
        fprintf(stderr, "runtime error: negative array index %d\n", index);
        return;
    }

    /* Auto-expand array if indexed beyond current bounds */
    if (index >= array->count) {
        int new_count = index + 1;
        if (new_count > array->capacity) {
            int new_capacity = array->capacity < 8 ? 8 : array->capacity * 2;
            if (new_capacity < new_count) new_capacity = new_count * 2;
            Value *new_elements = realloc(array->elements, sizeof(Value) * (size_t)new_capacity);
            if (new_elements == NULL) {
                fprintf(stderr, "error: out of memory expanding array\n");
                exit(1);
            }
            array->elements = new_elements;
            array->capacity = new_capacity;
        }

        /* Fill intermediate slots with NIL */
        for (int i = array->count; i < new_count; i++) {
            array->elements[i] = NIL_VAL;
        }
        array->count = new_count;
    }

    array->elements[index] = value;
}

Value obj_array_get(const ObjArray *array, int index) {
    if (index < 0 || index >= array->count) {
        return NIL_VAL;
    }
    return array->elements[index];
}

/* ------------------------------------------------------------------------ */
/* Value Array Operations                                                   */
/* ------------------------------------------------------------------------ */

void value_array_init(ValueArray *array) {
    array->values = NULL;
    array->count = 0;
    array->capacity = 0;
}

void value_array_write(ValueArray *array, Value value) {
    if (array->count + 1 > array->capacity) {
        int new_capacity = array->capacity < 8 ? 8 : array->capacity * 2;
        Value *new_values = realloc(array->values, sizeof(Value) * (size_t)new_capacity);
        if (new_values == NULL) {
            fprintf(stderr, "error: out of memory expanding ValueArray\n");
            exit(1);
        }
        array->values = new_values;
        array->capacity = new_capacity;
    }
    array->values[array->count++] = value;
}

void value_array_free(ValueArray *array) {
    if (array == NULL || array->values == NULL) return;
    free(array->values);
    array->values = NULL;
    array->count = 0;
    array->capacity = 0;
}

/* ------------------------------------------------------------------------ */
/* Value Operations                                                         */
/* ------------------------------------------------------------------------ */

void print_value(Value value) {
    switch (value.type) {
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_BOOL:
            printf("%s", AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;
        case VAL_STRING:
            printf("%s", AS_STRING(value)->chars);
            break;
        case VAL_ARRAY: {
            ObjArray *arr = AS_ARRAY(value);
            printf("[");
            for (int i = 0; i < arr->count; i++) {
                print_value(arr->elements[i]);
                if (i + 1 < arr->count) printf(", ");
            }
            printf("]");
            break;
        }
        case VAL_FUNCTION:
            printf("<fn>");
            break;
    }
}

bool values_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_NIL:
            return true;
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_STRING:
            return strcmp(AS_STRING(a)->chars, AS_STRING(b)->chars) == 0;
        case VAL_ARRAY:
            return AS_ARRAY(a) == AS_ARRAY(b);
        case VAL_FUNCTION:
            return AS_FUNCTION(a) == AS_FUNCTION(b);
    }
    return false;
}

bool is_falsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)) || (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}
