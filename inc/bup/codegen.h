/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_CODEGEN_H
#define BUP_CODEGEN_H 1

#include "bup/state.h"
#include "bup/ast.h"

/*
 * Compile a node and generate assembly
 *
 * @state: Compiler state
 * @root:  Root node to compiler
 *
 * Returns zero on success
 */
int cg_compile_node(struct bup_state *state, struct ast_node *root);

#endif  /* !BUP_CODEGEN_H */
