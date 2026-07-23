#ifndef VEXIUM_COMPILER_H
#define VEXIUM_COMPILER_H

#include "ast.h"
#include "chunk.h"

bool compile_ast_to_chunk(ASTNode* program, Chunk* chunk);

#endif
