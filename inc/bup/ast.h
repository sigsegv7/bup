/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_AST_H
#define BUP_AST_H

#include "bup/state.h"

/*
 * Represents valid AST types
 *
 * @AST_NONE: No type associated
 */
typedef enum {
    AST_NONE
} ast_type_t;

/*
 * Represents a single abstract syntax tree
 * node.
 *
 * @type: Node type
 */
struct ast_node {
    ast_type_t type;
};

/*
 * Allocate a new AST node
 *
 * @state: Compiler state
 * @type:  Node type
 * @res:   Result is written here
 *
 * Returns zero on success
 */
int ast_alloc_node(struct bup_state *state, ast_type_t type, struct ast_node **res);

#endif  /* !BUP_AST_H */
