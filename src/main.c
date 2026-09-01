/*
 * main.c — command-line driver.
 *
 * Supported commands:
 *   pseudoc                           (starts interactive REPL)
 *   pseudoc repl                      (starts interactive REPL)
 *   pseudoc run <file.pseudo>         (Bytecode VM execution)
 *   pseudoc build <file.pseudo> [-o]  (AOT compilation to native binary)
 *   pseudoc --check <file.pseudo>     (semantic analysis & error logging)
 *   pseudoc --dump-tokens <file>      (lexer token dump)
 *   pseudoc --dump-ast <file>         (parser syntax tree dump)
 *   pseudoc --dump-ir <file>          (Three-Address Code IR dump)
 *   pseudoc --dump-c <file>           (AOT C code generation dump)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "chunk.h"
#include "codegen_c.h"
#include "compiler.h"
#include "errlog.h"
#include "ir.h"
#include "lexer.h"
#include "parser.h"
#include "repl.h"
#include "semantics.h"
#include "token.h"
#include "vm.h"

/*
 * Read a whole file into a NUL-terminated heap buffer.
 */
static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "error: could not open '%s'\n", path);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fprintf(stderr, "error: could not seek in '%s'\n", path);
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0) {
        fprintf(stderr, "error: could not size '%s'\n", path);
        fclose(file);
        return NULL;
    }
    rewind(file);

    char *buffer = malloc((size_t)size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "error: out of memory reading '%s'\n", path);
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)size, file);
    if (read < (size_t)size) {
        fprintf(stderr, "error: could not read '%s'\n", path);
        free(buffer);
        fclose(file);
        return NULL;
    }
    buffer[read] = '\0';

    fclose(file);
    return buffer;
}

/*
 * Lex the whole source and print one line per token.
 */
static int dump_tokens(const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);

    int error_count = 0;

    for (;;) {
        Token token = lexer_scan_token(&lexer);

        printf("%4d:%-3d %-15s '%.*s'\n",
               token.line, token.col,
               token_type_name(token.type),
               token.length, token.start);

        if (token.type == T_ERROR) error_count++;
        if (token.type == T_EOF) break;
    }

    return error_count;
}

/*
 * Parse the whole source and print the AST.
 */
static int dump_ast(const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);

    Parser parser;
    parser_init(&parser, &lexer);

    AstNode *ast = parser_parse(&parser);
    if (ast != NULL) {
        ast_print(ast, 0);
        ast_free(ast);
    }

    return parser.error_count;
}

/*
 * Lex, parse, and semantically type-check the program.
 */
static int check_program(const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);

    Parser parser;
    parser_init(&parser, &lexer);

    AstNode *ast = parser_parse(&parser);
    if (ast == NULL) {
        return parser.error_count;
    }

    SemanticAnalyzer analyzer;
    semantics_init(&analyzer);

    int semantic_errors = semantics_analyze(&analyzer, ast);

    semantics_destroy(&analyzer);
    ast_free(ast);

    return parser.error_count + semantic_errors;
}

/*
 * Parse, check, and dump the generated Intermediate Representation.
 */
static int dump_ir(const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);

    Parser parser;
    parser_init(&parser, &lexer);

    AstNode *ast = parser_parse(&parser);
    if (ast == NULL) return parser.error_count;

    SemanticAnalyzer analyzer;
    semantics_init(&analyzer);
    int semantic_errors = semantics_analyze(&analyzer, ast);
    semantics_destroy(&analyzer);

    if (semantic_errors > 0) {
        ast_free(ast);
        return semantic_errors;
    }

    IRProgram *ir = ir_generate(ast);
    if (ir != NULL) {
        ir_program_print(ir);
        ir_program_free(ir);
        free(ir);
    }

    ast_free(ast);
    return 0;
}

/*
 * Generate and dump AOT C source code to stdout.
 */
static int dump_c(const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);

    Parser parser;
    parser_init(&parser, &lexer);

    AstNode *ast = parser_parse(&parser);
    if (ast == NULL) return parser.error_count;

    SemanticAnalyzer analyzer;
    semantics_init(&analyzer);
    int semantic_errors = semantics_analyze(&analyzer, ast);
    semantics_destroy(&analyzer);

    if (semantic_errors > 0) {
        ast_free(ast);
        return semantic_errors;
    }

    IRProgram *ir = ir_generate(ast);
    ast_free(ast);

    if (ir == NULL) {
        fprintf(stderr, "error: IR generation failed\n");
        return 1;
    }

    char *c_code = codegen_c_generate(ir);
    ir_program_free(ir);
    free(ir);

    if (c_code != NULL) {
        printf("%s", c_code);
        free(c_code);
    }
    return 0;
}

/*
 * Compile end-to-end and execute on the Bytecode Virtual Machine.
 */
static int run_program(const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);

    Parser parser;
    parser_init(&parser, &lexer);

    AstNode *ast = parser_parse(&parser);
    if (ast == NULL) return parser.error_count;

    SemanticAnalyzer analyzer;
    semantics_init(&analyzer);
    int semantic_errors = semantics_analyze(&analyzer, ast);
    semantics_destroy(&analyzer);

    if (semantic_errors > 0) {
        ast_free(ast);
        return semantic_errors;
    }

    IRProgram *ir = ir_generate(ast);
    ast_free(ast);

    if (ir == NULL) {
        fprintf(stderr, "error: IR generation failed\n");
        return 1;
    }

    ObjFunction *main_fn = compile_ir(ir);
    ir_program_free(ir);
    free(ir);

    if (main_fn == NULL) {
        fprintf(stderr, "error: bytecode compilation failed\n");
        return 1;
    }

    VM *vm = (VM *)malloc(sizeof(VM));
    if (vm == NULL) {
        fprintf(stderr, "error: out of memory allocating VM\n");
        return 1;
    }
    vm_init(vm);
    InterpretResult res = vm_interpret(vm, main_fn);
    vm_free(vm);
    free(vm);

    return res == INTERPRET_OK ? 0 : 70;
}

/*
 * Compile Ahead-Of-Time (AOT) to a native binary using the host C compiler.
 */
static int build_program(const char *source, const char *output_binary) {
    Lexer lexer;
    lexer_init(&lexer, source);

    Parser parser;
    parser_init(&parser, &lexer);

    AstNode *ast = parser_parse(&parser);
    if (ast == NULL) return parser.error_count;

    SemanticAnalyzer analyzer;
    semantics_init(&analyzer);
    int semantic_errors = semantics_analyze(&analyzer, ast);
    semantics_destroy(&analyzer);

    if (semantic_errors > 0) {
        ast_free(ast);
        return semantic_errors;
    }

    IRProgram *ir = ir_generate(ast);
    ast_free(ast);

    if (ir == NULL) {
        fprintf(stderr, "error: IR generation failed\n");
        return 1;
    }

    const char *tmp_c = "build/_aot_temp.c";
    if (!codegen_c_write_file(ir, tmp_c)) {
        ir_program_free(ir);
        free(ir);
        return 1;
    }
    ir_program_free(ir);
    free(ir);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cc -std=c11 -O2 -Iinclude %s -o %s -lm", tmp_c, output_binary);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "error: native C compilation failed with exit code %d\n", ret);
        return 1;
    }

    return 0;
}

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Pseudo Is All You Need — Dual-Backend Pseudocode Compiler\n\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s [repl]                       Start interactive REPL\n", prog_name);
    fprintf(stderr, "  %s run <file.pseudo>            Execute on Bytecode VM\n", prog_name);
    fprintf(stderr, "  %s build <file.pseudo> [-o out] Compile to native binary\n", prog_name);
    fprintf(stderr, "  %s --check <file.pseudo>        Validate syntax & semantics (records .errlog)\n", prog_name);
    fprintf(stderr, "  %s --dump-tokens <file.pseudo>  Dump token stream\n", prog_name);
    fprintf(stderr, "  %s --dump-ast <file.pseudo>     Dump Abstract Syntax Tree\n", prog_name);
    fprintf(stderr, "  %s --dump-ir <file.pseudo>      Dump 3-Address Code IR\n", prog_name);
    fprintf(stderr, "  %s --dump-c <file.pseudo>       Dump generated C11 code\n", prog_name);
}

int main(int argc, char *argv[]) {
    errlog_init(".errlog");

    if (argc == 1 || (argc == 2 && strcmp(argv[1], "repl") == 0)) {
        repl_run();
        errlog_close();
        return 0;
    }

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_usage(argv[0]);
        errlog_close();
        return 0;
    }

    if (argc < 3) {
        print_usage(argv[0]);
        errlog_close();
        return 64;  /* EX_USAGE */
    }

    const char *flag = argv[1];
    const char *path = argv[2];

    char *source = read_file(path);
    if (source == NULL) {
        errlog_close();
        return 66;  /* EX_NOINPUT */
    }

    int error_count = 0;

    if (strcmp(flag, "--dump-tokens") == 0) {
        error_count = dump_tokens(source);
    } else if (strcmp(flag, "--dump-ast") == 0) {
        error_count = dump_ast(source);
    } else if (strcmp(flag, "--check") == 0) {
        error_count = check_program(source);
    } else if (strcmp(flag, "--dump-ir") == 0) {
        error_count = dump_ir(source);
    } else if (strcmp(flag, "--dump-c") == 0) {
        error_count = dump_c(source);
    } else if (strcmp(flag, "run") == 0 || strcmp(flag, "--run") == 0) {
        error_count = run_program(source);
    } else if (strcmp(flag, "build") == 0) {
        const char *out_binary = "a.out";
        if (argc >= 5 && strcmp(argv[3], "-o") == 0) {
            out_binary = argv[4];
        }
        error_count = build_program(source, out_binary);
    } else {
        fprintf(stderr, "unknown flag: '%s'\n", flag);
        print_usage(argv[0]);
        free(source);
        errlog_close();
        return 64;
    }

    free(source);
    errlog_close();

    if (error_count > 0) {
        fprintf(stderr, "\n%d error(s)\n", error_count);
        return 65;  /* EX_DATAERR */
    }
    return 0;
}
