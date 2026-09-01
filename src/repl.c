/*
 * repl.c — Interactive Read-Eval-Print Loop (REPL) implementation.
 *
 * Maintains a persistent VM session and symbol table across inputs,
 * supports multi-line blocks, and provides clean error recovery.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "ast.h"
#include "compiler.h"
#include "ir.h"
#include "lexer.h"
#include "parser.h"
#include "repl.h"
#include "semantics.h"
#include "vm.h"

static void print_banner(void) {
    printf("============================================================\n");
    printf(" Pseudo Is All You Need — Interactive REPL v0.1\n");
    printf(" Type statements to evaluate. Type 'help' or 'exit'/'quit'.\n");
    printf("============================================================\n");
}

static void print_help(void) {
    printf("\nAvailable Commands:\n");
    printf("  help        Show this help message\n");
    printf("  exit, quit  Exit the REPL session\n");
    printf("  clear       Clear the terminal screen\n\n");
    printf("Syntax Examples:\n");
    printf("  x = 10 + 20\n");
    printf("  output x\n");
    printf("  for i = 1 to 5 do output i endfor\n");
    printf("  function square(n) return n * n endfunction\n");
    printf("  output square(5)\n\n");
}

static int count_keywords(const char *line, const char *kw) {
    int count = 0;
    size_t kw_len = strlen(kw);
    const char *p = line;
    while ((p = strstr(p, kw)) != NULL) {
        bool left_ok = (p == line || *(p - 1) == ' ' || *(p - 1) == '\t' || *(p - 1) == '(');
        bool right_ok = (*(p + kw_len) == '\0' || *(p + kw_len) == ' ' || *(p + kw_len) == '\t' || *(p + kw_len) == '\n' || *(p + kw_len) == ')');
        if (left_ok && right_ok) {
            count++;
        }
        p += kw_len;
    }
    return count;
}

static int get_indent_delta(const char *line) {
    int delta = 0;

    delta += count_keywords(line, "if");
    delta += count_keywords(line, "IF");
    delta += count_keywords(line, "for");
    delta += count_keywords(line, "FOR");
    delta += count_keywords(line, "while");
    delta += count_keywords(line, "WHILE");
    delta += count_keywords(line, "repeat");
    delta += count_keywords(line, "REPEAT");
    delta += count_keywords(line, "function");
    delta += count_keywords(line, "FUNCTION");

    delta -= count_keywords(line, "endif");
    delta -= count_keywords(line, "ENDIF");
    delta -= count_keywords(line, "endfor");
    delta -= count_keywords(line, "ENDFOR");
    delta -= count_keywords(line, "endwhile");
    delta -= count_keywords(line, "ENDWHILE");
    delta -= count_keywords(line, "until");
    delta -= count_keywords(line, "UNTIL");
    delta -= count_keywords(line, "endfunction");
    delta -= count_keywords(line, "ENDFUNCTION");

    return delta;
}

static void evaluate_buffer(SemanticAnalyzer *analyzer, VM *vm, const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);

    Parser parser;
    parser_init(&parser, &lexer);

    AstNode *ast = parser_parse(&parser);
    if (ast == NULL || parser.error_count > 0) {
        if (ast != NULL) ast_free(ast);
        return;
    }

    analyzer->error_count = 0;
    int semantic_errors = semantics_analyze(analyzer, ast);

    if (semantic_errors > 0) {
        ast_free(ast);
        return;
    }

    IRProgram *ir = ir_generate(ast);
    ast_free(ast);

    if (ir == NULL) {
        fprintf(stderr, "error: IR generation failed\n");
        return;
    }

    ObjFunction *fn = compile_ir(ir);
    ir_program_free(ir);
    free(ir);

    if (fn == NULL) {
        fprintf(stderr, "error: bytecode compilation failed\n");
        return;
    }

    vm_interpret(vm, fn);
}

void repl_run(void) {
    print_banner();

    VM *vm = (VM *)malloc(sizeof(VM));
    if (vm == NULL) {
        fprintf(stderr, "error: out of memory allocating REPL VM\n");
        return;
    }
    vm_init(vm);

    SemanticAnalyzer analyzer;
    semantics_init(&analyzer);

    char buffer[4096];
    buffer[0] = '\0';
    int block_depth = 0;

    for (;;) {
        if (block_depth == 0) {
            printf("pseudo> ");
        } else {
            printf("...     ");
        }
        fflush(stdout);

        char line[512];
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        /* Check for REPL commands */
        if (block_depth == 0) {
            if (strcmp(line, "exit\n") == 0 || strcmp(line, "quit\n") == 0) {
                break;
            }
            if (strcmp(line, "help\n") == 0) {
                print_help();
                continue;
            }
            if (strcmp(line, "clear\n") == 0) {
                printf("\033[H\033[J");
                continue;
            }
            if (strcmp(line, "\n") == 0) {
                continue;
            }
        }

        block_depth += get_indent_delta(line);
        if (block_depth < 0) block_depth = 0;

        strncat(buffer, line, sizeof(buffer) - strlen(buffer) - 1);

        if (block_depth == 0) {
            evaluate_buffer(&analyzer, vm, buffer);
            buffer[0] = '\0';
        }
    }

    semantics_destroy(&analyzer);
    vm_free(vm);
    free(vm);
    printf("Goodbye!\n");
}
