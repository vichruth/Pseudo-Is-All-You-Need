/*
 * codegen_c.c — Ahead-Of-Time (AOT) C code generator.
 *
 * Translates linear 3-Address Code IR into clean, standalone C11 source code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "codegen_c.h"
#include "ir.h"

/* ------------------------------------------------------------------------ */
/* String Builder Helper                                                    */
/* ------------------------------------------------------------------------ */

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuilder;

static void sb_init(StringBuilder *sb) {
    sb->capacity = 1024;
    sb->data = (char *)malloc(sb->capacity);
    if (sb->data == NULL) {
        fprintf(stderr, "error: out of memory allocating StringBuilder\n");
        exit(1);
    }
    sb->data[0] = '\0';
    sb->length = 0;
}

static void sb_append(StringBuilder *sb, const char *str) {
    if (str == NULL) return;
    size_t len = strlen(str);
    if (sb->length + len + 1 > sb->capacity) {
        size_t new_cap = sb->capacity * 2;
        if (new_cap < sb->length + len + 1) {
            new_cap = sb->length + len + 1 + 1024;
        }
        char *new_data = (char *)realloc(sb->data, new_cap);
        if (new_data == NULL) {
            fprintf(stderr, "error: out of memory growing StringBuilder\n");
            exit(1);
        }
        sb->data = new_data;
        sb->capacity = new_cap;
    }
    memcpy(sb->data + sb->length, str, len + 1);
    sb->length += len;
}

static void sb_printf(StringBuilder *sb, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (n < 0) return;
    if ((size_t)n < sizeof(buf)) {
        sb_append(sb, buf);
    } else {
        char *dyn_buf = (char *)malloc((size_t)n + 1);
        if (dyn_buf == NULL) {
            fprintf(stderr, "error: out of memory formatting string\n");
            exit(1);
        }
        va_start(args, fmt);
        vsnprintf(dyn_buf, (size_t)n + 1, fmt, args);
        va_end(args);
        sb_append(sb, dyn_buf);
        free(dyn_buf);
    }
}

/* ------------------------------------------------------------------------ */
/* Variable and Temp Collection                                             */
/* ------------------------------------------------------------------------ */

typedef struct {
    char **items;
    int count;
    int capacity;
} StringSet;

static void set_init(StringSet *set) {
    set->items = NULL;
    set->count = 0;
    set->capacity = 0;
}

static bool set_contains(const StringSet *set, const char *item) {
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->items[i], item) == 0) return true;
    }
    return false;
}

static void set_add(StringSet *set, const char *item) {
    if (item == NULL || set_contains(set, item)) return;
    if (set->count + 1 > set->capacity) {
        int new_cap = set->capacity < 16 ? 16 : set->capacity * 2;
        char **new_items = (char **)realloc(set->items, sizeof(char *) * (size_t)new_cap);
        if (new_items == NULL) {
            fprintf(stderr, "error: out of memory growing StringSet\n");
            exit(1);
        }
        set->items = new_items;
        set->capacity = new_cap;
    }
    size_t len = strlen(item);
    char *copy = (char *)malloc(len + 1);
    memcpy(copy, item, len + 1);
    set->items[set->count++] = copy;
}

static void set_free(StringSet *set) {
    for (int i = 0; i < set->count; i++) {
        free(set->items[i]);
    }
    free(set->items);
    set->items = NULL;
    set->count = 0;
    set->capacity = 0;
}

/* ------------------------------------------------------------------------ */
/* Operand Formatting                                                       */
/* ------------------------------------------------------------------------ */

static char *format_operand(IROperand op, const StringSet *params) {
    char buf[256];
    switch (op.kind) {
        case IR_OP_NONE:
            snprintf(buf, sizeof(buf), "pv_nil()");
            break;
        case IR_OP_TEMP:
            snprintf(buf, sizeof(buf), "t%d", op.as.temp_id);
            break;
        case IR_OP_VAR:
            if (params != NULL && set_contains(params, op.as.name)) {
                snprintf(buf, sizeof(buf), "p_%s", op.as.name);
            } else {
                snprintf(buf, sizeof(buf), "g_%s", op.as.name);
            }
            break;
        case IR_OP_NUM:
            snprintf(buf, sizeof(buf), "pv_num(%g)", op.as.num_val);
            break;
        case IR_OP_STR:
            snprintf(buf, sizeof(buf), "pv_str(\"%s\")", op.as.str_val);
            break;
        case IR_OP_BOOL:
            snprintf(buf, sizeof(buf), "pv_bool(%s)", op.as.bool_val ? "true" : "false");
            break;
        case IR_OP_LABEL:
            snprintf(buf, sizeof(buf), "lbl_%s", op.as.name);
            break;
    }
    size_t len = strlen(buf);
    char *res = (char *)malloc(len + 1);
    memcpy(res, buf, len + 1);
    return res;
}

/* ------------------------------------------------------------------------ */
/* Code Generator Implementation                                            */
/* ------------------------------------------------------------------------ */

char *codegen_c_generate(const IRProgram *program) {
    if (program == NULL) return NULL;

    StringBuilder sb;
    sb_init(&sb);

    /* 1. Header and Runtime Inclusion */
    sb_append(&sb, "/*\n");
    sb_append(&sb, " * Generated by pseudoc (AOT C11 Backend)\n");
    sb_append(&sb, " */\n\n");
    sb_append(&sb, "#include \"pseudo_runtime.h\"\n\n");

    /* 2. Collect Function Prototypes & Global Variables */
    StringSet functions;
    set_init(&functions);
    StringSet globals;
    set_init(&globals);

    for (int i = 0; i < program->count; i++) {
        const IRInstruction *instr = &program->instructions[i];
        if (instr->op == IR_FUNC_BEGIN) {
            set_add(&functions, instr->dst.as.name);
        } else {
            if (instr->dst.kind == IR_OP_VAR) set_add(&globals, instr->dst.as.name);
            if (instr->src1.kind == IR_OP_VAR) set_add(&globals, instr->src1.as.name);
            if (instr->src2.kind == IR_OP_VAR) set_add(&globals, instr->src2.as.name);
        }
    }

    /* Emit Function Forward Declarations */
    for (int i = 0; i < program->count; i++) {
        const IRInstruction *instr = &program->instructions[i];
        if (instr->op == IR_FUNC_BEGIN) {
            sb_printf(&sb, "static PseudoValue fn_%s(", instr->dst.as.name);
            for (int p = 0; p < instr->args.count; p++) {
                if (p > 0) sb_append(&sb, ", ");
                sb_printf(&sb, "PseudoValue p_%s", instr->args.items[p].as.name);
            }
            if (instr->args.count == 0) sb_append(&sb, "void");
            sb_append(&sb, ");\n");
        }
    }
    sb_append(&sb, "\n");

    /* Emit Global Variables */
    for (int i = 0; i < globals.count; i++) {
        if (!set_contains(&functions, globals.items[i])) {
            sb_printf(&sb, "static PseudoValue g_%s;\n", globals.items[i]);
        }
    }
    sb_append(&sb, "\n");

    /* 3. Emit Functions and Main */
    for (int i = 0; i < program->count; i++) {
        const IRInstruction *instr = &program->instructions[i];

        if (instr->op == IR_FUNC_BEGIN) {
            const char *fn_name = instr->dst.as.name;
            StringSet params;
            set_init(&params);
            for (int p = 0; p < instr->args.count; p++) {
                set_add(&params, instr->args.items[p].as.name);
            }

            /* Find maximum temporary variable in function */
            int max_temp = -1;
            int j = i + 1;
            while (j < program->count && program->instructions[j].op != IR_FUNC_END) {
                if (program->instructions[j].dst.kind == IR_OP_TEMP && program->instructions[j].dst.as.temp_id > max_temp)
                    max_temp = program->instructions[j].dst.as.temp_id;
                if (program->instructions[j].src1.kind == IR_OP_TEMP && program->instructions[j].src1.as.temp_id > max_temp)
                    max_temp = program->instructions[j].src1.as.temp_id;
                if (program->instructions[j].src2.kind == IR_OP_TEMP && program->instructions[j].src2.as.temp_id > max_temp)
                    max_temp = program->instructions[j].src2.as.temp_id;
                j++;
            }

            /* Emit Function Header */
            sb_printf(&sb, "static PseudoValue fn_%s(", fn_name);
            for (int p = 0; p < instr->args.count; p++) {
                if (p > 0) sb_append(&sb, ", ");
                sb_printf(&sb, "PseudoValue p_%s", instr->args.items[p].as.name);
            }
            if (instr->args.count == 0) sb_append(&sb, "void");
            sb_append(&sb, ") {\n");

            /* Emit local temporaries */
            for (int t = 0; t <= max_temp; t++) {
                sb_printf(&sb, "    PseudoValue t%d = pv_nil();\n", t);
            }

            i++;
            while (i < program->count && program->instructions[i].op != IR_FUNC_END) {
                const IRInstruction *cur = &program->instructions[i];
                char *dst = format_operand(cur->dst, &params);
                char *s1 = format_operand(cur->src1, &params);
                char *s2 = format_operand(cur->src2, &params);

                switch (cur->op) {
                    case IR_CONST:
                        sb_printf(&sb, "    %s = %s;\n", dst, s1);
                        break;
                    case IR_LOAD:
                        sb_printf(&sb, "    %s = %s;\n", dst, s1);
                        break;
                    case IR_STORE:
                        sb_printf(&sb, "    %s = %s;\n", dst, s1);
                        break;
                    case IR_LOAD_IDX:
                        sb_printf(&sb, "    %s = pv_get_idx(%s, %s);\n", dst, s1, s2);
                        break;
                    case IR_STORE_IDX:
                        sb_printf(&sb, "    pv_set_idx(&%s, %s, %s);\n", dst, s1, s2);
                        break;
                    case IR_LEN:
                        sb_printf(&sb, "    %s = pv_len(%s);\n", dst, s1);
                        break;
                    case IR_BINOP:
                        switch (cur->sub_op) {
                            case T_PLUS:  sb_printf(&sb, "    %s = pv_add(%s, %s);\n", dst, s1, s2); break;
                            case T_MINUS: sb_printf(&sb, "    %s = pv_sub(%s, %s);\n", dst, s1, s2); break;
                            case T_STAR:  sb_printf(&sb, "    %s = pv_mul(%s, %s);\n", dst, s1, s2); break;
                            case T_SLASH: sb_printf(&sb, "    %s = pv_div(%s, %s);\n", dst, s1, s2); break;
                            case T_MOD:   sb_printf(&sb, "    %s = pv_mod(%s, %s);\n", dst, s1, s2); break;
                            case T_EQ:    sb_printf(&sb, "    %s = pv_equal(%s, %s);\n", dst, s1, s2); break;
                            case T_NEQ:   sb_printf(&sb, "    %s = pv_not_equal(%s, %s);\n", dst, s1, s2); break;
                            case T_LT:    sb_printf(&sb, "    %s = pv_less(%s, %s);\n", dst, s1, s2); break;
                            case T_LTE:   sb_printf(&sb, "    %s = pv_less_equal(%s, %s);\n", dst, s1, s2); break;
                            case T_GT:    sb_printf(&sb, "    %s = pv_greater(%s, %s);\n", dst, s1, s2); break;
                            case T_GTE:   sb_printf(&sb, "    %s = pv_greater_equal(%s, %s);\n", dst, s1, s2); break;
                            case T_AND:   sb_printf(&sb, "    %s = pv_and(%s, %s);\n", dst, s1, s2); break;
                            case T_OR:    sb_printf(&sb, "    %s = pv_or(%s, %s);\n", dst, s1, s2); break;
                            default: break;
                        }
                        break;
                    case IR_UNOP:
                        switch (cur->sub_op) {
                            case T_MINUS: sb_printf(&sb, "    %s = pv_neg(%s);\n", dst, s1); break;
                            case T_NOT:   sb_printf(&sb, "    %s = pv_not(%s);\n", dst, s1); break;
                            default: break;
                        }
                        break;
                    case IR_LABEL:
                        sb_printf(&sb, "%s:\n", dst);
                        break;
                    case IR_JUMP:
                        sb_printf(&sb, "    goto %s;\n", dst);
                        break;
                    case IR_JUMP_IF_FALSE:
                        sb_printf(&sb, "    if (pv_is_falsey(%s)) goto %s;\n", s1, dst);
                        break;
                    case IR_CALL:
                        if (cur->dst.kind != IR_OP_NONE) {
                            sb_printf(&sb, "    %s = fn_%s(", dst, cur->src1.as.name);
                        } else {
                            sb_printf(&sb, "    fn_%s(", cur->src1.as.name);
                        }
                        for (int a = 0; a < cur->args.count; a++) {
                            if (a > 0) sb_append(&sb, ", ");
                            char *arg_str = format_operand(cur->args.items[a], &params);
                            sb_append(&sb, arg_str);
                            free(arg_str);
                        }
                        sb_append(&sb, ");\n");
                        break;
                    case IR_RETURN:
                        if (cur->src1.kind != IR_OP_NONE) {
                            sb_printf(&sb, "    return %s;\n", s1);
                        } else {
                            sb_append(&sb, "    return pv_nil();\n");
                        }
                        break;
                    case IR_INPUT:
                        sb_printf(&sb, "    %s = pv_input();\n", dst);
                        break;
                    case IR_OUTPUT:
                        sb_printf(&sb, "    pv_output(%s);\n", s1);
                        break;
                    default:
                        break;
                }

                free(dst);
                free(s1);
                free(s2);
                i++;
            }

            sb_append(&sb, "    return pv_nil();\n");
            sb_append(&sb, "}\n\n");
            set_free(&params);
            continue;
        }
    }

    /* 4. Emit Main Entry Point */
    sb_append(&sb, "int main(void) {\n");

    /* Find max temp in main */
    int max_temp_main = -1;
    bool in_func = false;
    for (int i = 0; i < program->count; i++) {
        if (program->instructions[i].op == IR_FUNC_BEGIN) in_func = true;
        if (program->instructions[i].op == IR_FUNC_END) { in_func = false; continue; }
        if (!in_func) {
            if (program->instructions[i].dst.kind == IR_OP_TEMP && program->instructions[i].dst.as.temp_id > max_temp_main)
                max_temp_main = program->instructions[i].dst.as.temp_id;
            if (program->instructions[i].src1.kind == IR_OP_TEMP && program->instructions[i].src1.as.temp_id > max_temp_main)
                max_temp_main = program->instructions[i].src1.as.temp_id;
            if (program->instructions[i].src2.kind == IR_OP_TEMP && program->instructions[i].src2.as.temp_id > max_temp_main)
                max_temp_main = program->instructions[i].src2.as.temp_id;
        }
    }

    for (int t = 0; t <= max_temp_main; t++) {
        sb_printf(&sb, "    PseudoValue t%d = pv_nil();\n", t);
    }

    /* Translate top-level instructions */
    in_func = false;
    for (int i = 0; i < program->count; i++) {
        const IRInstruction *cur = &program->instructions[i];
        if (cur->op == IR_FUNC_BEGIN) { in_func = true; continue; }
        if (cur->op == IR_FUNC_END) { in_func = false; continue; }
        if (in_func) continue;

        char *dst = format_operand(cur->dst, NULL);
        char *s1 = format_operand(cur->src1, NULL);
        char *s2 = format_operand(cur->src2, NULL);

        switch (cur->op) {
            case IR_CONST:
                sb_printf(&sb, "    %s = %s;\n", dst, s1);
                break;
            case IR_LOAD:
                sb_printf(&sb, "    %s = %s;\n", dst, s1);
                break;
            case IR_STORE:
                sb_printf(&sb, "    %s = %s;\n", dst, s1);
                break;
            case IR_LOAD_IDX:
                sb_printf(&sb, "    %s = pv_get_idx(%s, %s);\n", dst, s1, s2);
                break;
            case IR_STORE_IDX:
                sb_printf(&sb, "    pv_set_idx(&%s, %s, %s);\n", dst, s1, s2);
                break;
            case IR_LEN:
                sb_printf(&sb, "    %s = pv_len(%s);\n", dst, s1);
                break;
            case IR_BINOP:
                switch (cur->sub_op) {
                    case T_PLUS:  sb_printf(&sb, "    %s = pv_add(%s, %s);\n", dst, s1, s2); break;
                    case T_MINUS: sb_printf(&sb, "    %s = pv_sub(%s, %s);\n", dst, s1, s2); break;
                    case T_STAR:  sb_printf(&sb, "    %s = pv_mul(%s, %s);\n", dst, s1, s2); break;
                    case T_SLASH: sb_printf(&sb, "    %s = pv_div(%s, %s);\n", dst, s1, s2); break;
                    case T_MOD:   sb_printf(&sb, "    %s = pv_mod(%s, %s);\n", dst, s1, s2); break;
                    case T_EQ:    sb_printf(&sb, "    %s = pv_equal(%s, %s);\n", dst, s1, s2); break;
                    case T_NEQ:   sb_printf(&sb, "    %s = pv_not_equal(%s, %s);\n", dst, s1, s2); break;
                    case T_LT:    sb_printf(&sb, "    %s = pv_less(%s, %s);\n", dst, s1, s2); break;
                    case T_LTE:   sb_printf(&sb, "    %s = pv_less_equal(%s, %s);\n", dst, s1, s2); break;
                    case T_GT:    sb_printf(&sb, "    %s = pv_greater(%s, %s);\n", dst, s1, s2); break;
                    case T_GTE:   sb_printf(&sb, "    %s = pv_greater_equal(%s, %s);\n", dst, s1, s2); break;
                    case T_AND:   sb_printf(&sb, "    %s = pv_and(%s, %s);\n", dst, s1, s2); break;
                    case T_OR:    sb_printf(&sb, "    %s = pv_or(%s, %s);\n", dst, s1, s2); break;
                    default: break;
                }
                break;
            case IR_UNOP:
                switch (cur->sub_op) {
                    case T_MINUS: sb_printf(&sb, "    %s = pv_neg(%s);\n", dst, s1); break;
                    case T_NOT:   sb_printf(&sb, "    %s = pv_not(%s);\n", dst, s1); break;
                    default: break;
                }
                break;
            case IR_LABEL:
                sb_printf(&sb, "%s:\n", dst);
                break;
            case IR_JUMP:
                sb_printf(&sb, "    goto %s;\n", dst);
                break;
            case IR_JUMP_IF_FALSE:
                sb_printf(&sb, "    if (pv_is_falsey(%s)) goto %s;\n", s1, dst);
                break;
            case IR_CALL:
                if (cur->dst.kind != IR_OP_NONE) {
                    sb_printf(&sb, "    %s = fn_%s(", dst, cur->src1.as.name);
                } else {
                    sb_printf(&sb, "    fn_%s(", cur->src1.as.name);
                }
                for (int a = 0; a < cur->args.count; a++) {
                    if (a > 0) sb_append(&sb, ", ");
                    char *arg_str = format_operand(cur->args.items[a], NULL);
                    sb_append(&sb, arg_str);
                    free(arg_str);
                }
                sb_append(&sb, ");\n");
                break;
            case IR_RETURN:
                sb_append(&sb, "    return 0;\n");
                break;
            case IR_INPUT:
                sb_printf(&sb, "    %s = pv_input();\n", dst);
                break;
            case IR_OUTPUT:
                sb_printf(&sb, "    pv_output(%s);\n", s1);
                break;
            default:
                break;
        }

        free(dst);
        free(s1);
        free(s2);
    }

    sb_append(&sb, "    return 0;\n");
    sb_append(&sb, "}\n");

    set_free(&functions);
    set_free(&globals);

    return sb.data;
}

bool codegen_c_write_file(const IRProgram *program, const char *filepath) {
    char *c_code = codegen_c_generate(program);
    if (c_code == NULL) return false;

    FILE *f = fopen(filepath, "w");
    if (f == NULL) {
        fprintf(stderr, "error: could not open output file '%s' for writing\n", filepath);
        free(c_code);
        return false;
    }

    fputs(c_code, f);
    fclose(f);
    free(c_code);
    return true;
}
