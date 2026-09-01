/*
 * compiler.h — Bytecode compiler from Intermediate Representation (IR).
 */

#ifndef PSEUDO_COMPILER_H
#define PSEUDO_COMPILER_H

#include "chunk.h"
#include "ir.h"

/*
 * Compiles an IRProgram into a main executable ObjFunction.
 * Returns NULL on compilation error.
 */
ObjFunction *compile_ir(const IRProgram *program);

#endif /* PSEUDO_COMPILER_H */
