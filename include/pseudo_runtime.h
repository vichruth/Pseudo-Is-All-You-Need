/*
 * pseudo_runtime.h — Lightweight freestanding runtime for AOT-compiled pseudocode.
 *
 * Implements tagged PseudoValue, dynamic arrays, string operations, and I/O.
 */

#ifndef PSEUDO_RUNTIME_H
#define PSEUDO_RUNTIME_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

typedef enum {
    PV_NIL,
    PV_BOOL,
    PV_NUM,
    PV_STR,
    PV_ARR
} PVType;

typedef struct PseudoArray PseudoArray;

typedef struct {
    PVType type;
    union {
        bool b;
        double num;
        char *str;
        PseudoArray *arr;
    } as;
} PseudoValue;

struct PseudoArray {
    PseudoValue *items;
    int count;
    int capacity;
};

static inline PseudoValue pv_nil(void) {
    PseudoValue v;
    v.type = PV_NIL;
    v.as.num = 0;
    return v;
}

static inline PseudoValue pv_bool(bool b) {
    PseudoValue v;
    v.type = PV_BOOL;
    v.as.b = b;
    return v;
}

static inline PseudoValue pv_num(double num) {
    PseudoValue v;
    v.type = PV_NUM;
    v.as.num = num;
    return v;
}

static inline PseudoValue pv_str(const char *str) {
    PseudoValue v;
    v.type = PV_STR;
    if (str == NULL) {
        v.as.str = "";
    } else {
        size_t len = strlen(str);
        char *copy = (char *)malloc(len + 1);
        if (copy != NULL) {
            memcpy(copy, str, len + 1);
            v.as.str = copy;
        } else {
            v.as.str = "";
        }
    }
    return v;
}

static inline PseudoArray *pv_new_array(void) {
    PseudoArray *arr = (PseudoArray *)malloc(sizeof(PseudoArray));
    if (arr == NULL) {
        fprintf(stderr, "runtime error: out of memory allocating array\n");
        exit(1);
    }
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
    return arr;
}

static inline PseudoValue pv_arr(PseudoArray *arr) {
    PseudoValue v;
    v.type = PV_ARR;
    v.as.arr = arr;
    return v;
}

static inline bool pv_is_truthy(PseudoValue v) {
    switch (v.type) {
        case PV_NIL:  return false;
        case PV_BOOL: return v.as.b;
        case PV_NUM:  return v.as.num != 0.0;
        case PV_STR:  return v.as.str != NULL && v.as.str[0] != '\0';
        case PV_ARR:  return true;
    }
    return false;
}

static inline bool pv_is_falsey(PseudoValue v) {
    return !pv_is_truthy(v);
}

static inline double pv_to_num(PseudoValue v) {
    switch (v.type) {
        case PV_NUM:  return v.as.num;
        case PV_BOOL: return v.as.b ? 1.0 : 0.0;
        case PV_NIL:  return 0.0;
        case PV_STR:  return strtod(v.as.str, NULL);
        case PV_ARR:  return (double)(v.as.arr ? v.as.arr->count : 0);
    }
    return 0.0;
}

/* Arithmetic & String operations */
static inline PseudoValue pv_add(PseudoValue a, PseudoValue b) {
    if (a.type == PV_STR || b.type == PV_STR) {
        char buf_a[64], buf_b[64];
        const char *sa = a.type == PV_STR ? a.as.str : (snprintf(buf_a, sizeof(buf_a), "%g", pv_to_num(a)), buf_a);
        const char *sb = b.type == PV_STR ? b.as.str : (snprintf(buf_b, sizeof(buf_b), "%g", pv_to_num(b)), buf_b);
        size_t la = strlen(sa);
        size_t lb = strlen(sb);
        char *res = (char *)malloc(la + lb + 1);
        if (res == NULL) {
            fprintf(stderr, "runtime error: out of memory in string concat\n");
            exit(1);
        }
        memcpy(res, sa, la);
        memcpy(res + la, sb, lb + 1);
        PseudoValue v;
        v.type = PV_STR;
        v.as.str = res;
        return v;
    }
    return pv_num(pv_to_num(a) + pv_to_num(b));
}

static inline PseudoValue pv_sub(PseudoValue a, PseudoValue b) {
    return pv_num(pv_to_num(a) - pv_to_num(b));
}

static inline PseudoValue pv_mul(PseudoValue a, PseudoValue b) {
    return pv_num(pv_to_num(a) * pv_to_num(b));
}

static inline PseudoValue pv_div(PseudoValue a, PseudoValue b) {
    double denom = pv_to_num(b);
    return pv_num(denom != 0.0 ? pv_to_num(a) / denom : 0.0);
}

static inline PseudoValue pv_mod(PseudoValue a, PseudoValue b) {
    double denom = pv_to_num(b);
    return pv_num(denom != 0.0 ? fmod(pv_to_num(a), denom) : 0.0);
}

static inline PseudoValue pv_neg(PseudoValue a) {
    return pv_num(-pv_to_num(a));
}

static inline PseudoValue pv_not(PseudoValue a) {
    return pv_bool(pv_is_falsey(a));
}

static inline PseudoValue pv_and(PseudoValue a, PseudoValue b) {
    return pv_bool(pv_is_truthy(a) && pv_is_truthy(b));
}

static inline PseudoValue pv_or(PseudoValue a, PseudoValue b) {
    return pv_bool(pv_is_truthy(a) || pv_is_truthy(b));
}

static inline PseudoValue pv_equal(PseudoValue a, PseudoValue b) {
    if (a.type != b.type) {
        if ((a.type == PV_NUM || a.type == PV_BOOL) && (b.type == PV_NUM || b.type == PV_BOOL)) {
            return pv_bool(pv_to_num(a) == pv_to_num(b));
        }
        return pv_bool(false);
    }
    switch (a.type) {
        case PV_NIL:  return pv_bool(true);
        case PV_BOOL: return pv_bool(a.as.b == b.as.b);
        case PV_NUM:  return pv_bool(a.as.num == b.as.num);
        case PV_STR:  return pv_bool(strcmp(a.as.str, b.as.str) == 0);
        case PV_ARR:  return pv_bool(a.as.arr == b.as.arr);
    }
    return pv_bool(false);
}

static inline PseudoValue pv_not_equal(PseudoValue a, PseudoValue b) {
    return pv_bool(!pv_equal(a, b).as.b);
}

static inline PseudoValue pv_less(PseudoValue a, PseudoValue b) {
    return pv_bool(pv_to_num(a) < pv_to_num(b));
}

static inline PseudoValue pv_less_equal(PseudoValue a, PseudoValue b) {
    return pv_bool(pv_to_num(a) <= pv_to_num(b));
}

static inline PseudoValue pv_greater(PseudoValue a, PseudoValue b) {
    return pv_bool(pv_to_num(a) > pv_to_num(b));
}

static inline PseudoValue pv_greater_equal(PseudoValue a, PseudoValue b) {
    return pv_bool(pv_to_num(a) >= pv_to_num(b));
}

/* Array indexing */
static inline PseudoValue pv_get_idx(PseudoValue arr_val, PseudoValue idx_val) {
    if (arr_val.type != PV_ARR || arr_val.as.arr == NULL) {
        return pv_nil();
    }
    int idx = (int)pv_to_num(idx_val);
    PseudoArray *arr = arr_val.as.arr;
    if (idx < 0 || idx >= arr->count) {
        return pv_nil();
    }
    return arr->items[idx];
}

static inline PseudoValue pv_set_idx(PseudoValue *arr_val, PseudoValue idx_val, PseudoValue val) {
    if (arr_val->type != PV_ARR || arr_val->as.arr == NULL) {
        *arr_val = pv_arr(pv_new_array());
    }
    int idx = (int)pv_to_num(idx_val);
    if (idx < 0) return val;
    PseudoArray *arr = arr_val->as.arr;
    if (idx >= arr->capacity) {
        int new_cap = arr->capacity < 8 ? 8 : arr->capacity * 2;
        if (new_cap <= idx) new_cap = idx + 1;
        PseudoValue *new_items = (PseudoValue *)realloc(arr->items, sizeof(PseudoValue) * (size_t)new_cap);
        if (new_items == NULL) {
            fprintf(stderr, "runtime error: out of memory expanding array\n");
            exit(1);
        }
        for (int i = arr->capacity; i < new_cap; i++) {
            new_items[i] = pv_nil();
        }
        arr->items = new_items;
        arr->capacity = new_cap;
    }
    if (idx >= arr->count) {
        arr->count = idx + 1;
    }
    arr->items[idx] = val;
    return val;
}

static inline PseudoValue pv_len(PseudoValue arr_val) {
    if (arr_val.type == PV_ARR && arr_val.as.arr != NULL) {
        return pv_num((double)arr_val.as.arr->count);
    }
    return pv_num(0.0);
}

/* I/O Helpers */
static inline void pv_output(PseudoValue val) {
    switch (val.type) {
        case PV_NIL:
            printf("nil\n");
            break;
        case PV_BOOL:
            printf("%s\n", val.as.b ? "true" : "false");
            break;
        case PV_NUM:
            if (val.as.num == (long)val.as.num) {
                printf("%ld\n", (long)val.as.num);
            } else {
                printf("%g\n", val.as.num);
            }
            break;
        case PV_STR:
            printf("%s\n", val.as.str != NULL ? val.as.str : "");
            break;
        case PV_ARR:
            printf("<array %d>\n", val.as.arr ? val.as.arr->count : 0);
            break;
    }
}

static inline PseudoValue pv_input(void) {
    size_t cap = 128;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (buf == NULL) return pv_nil();

    int c;
    while ((c = fgetc(stdin)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *new_buf = (char *)realloc(buf, cap);
            if (new_buf == NULL) {
                free(buf);
                return pv_nil();
            }
            buf = new_buf;
        }
        buf[len++] = (char)c;
    }

    if (len == 0 && c == EOF) {
        free(buf);
        return pv_nil();
    }

    buf[len] = '\0';
    char *endptr;
    double num = strtod(buf, &endptr);
    if (*buf != '\0' && *endptr == '\0') {
        free(buf);
        return pv_num(num);
    }
    PseudoValue res;
    res.type = PV_STR;
    res.as.str = buf;
    return res;
}

#endif /* PSEUDO_RUNTIME_H */
