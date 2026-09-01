/*
 * codegen_c.h — Ahead-Of-Time (AOT) C code generator.
 */

#ifndef PSEUDO_CODEGEN_C_H
#define PSEUDO_CODEGEN_C_H

#include <stdbool.h>
#include "ir.h"

/*
 * Generates standalone C11 source code from an IRProgram.
 * Caller owns the returned heap-allocated string and must free it.
 */
char *codegen_c_generate(const IRProgram *program);

/*
 * Writes the generated C source code directly to the given filepath.
 * Returns true on success, false on error.
 */
bool codegen_c_write_file(const IRProgram *program, const char *filepath);

#endif /* PSEUDO_CODEGEN_C_H */
