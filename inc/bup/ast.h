/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_AST_H
#define BUP_AST_H

#include "bup/state.h"
#include "bup/symbol.h"

/*
 * Represents valid AST types
 *
 * @AST_NONE:   No type associated
 * @AST_PROC:   Procedure
 * @AST_NUMBER: Is a number
 * @AST_RETURN: Return statement
 * @AST_ASM:    Assembly block
 * @AST_LOOP:   Loop block
 */
typedef enum {
    AST_NONE,
    AST_PROC,
    AST_NUMBER,
    AST_RETURN,
    AST_ASM,
    AST_LOOP
} ast_type_t;

/*
 * Represents a single abstract syntax tree
 * node.
 *
 * @type: Node type
 * @symbol: Program symbol associated with node
 * @left: Left node
 * @right: Right node
 * @epilogue: Returns true if node is epilogue
 */
struct ast_node {
    ast_type_t type;
    struct symbol *symbol;
    struct ast_node *left;
    struct ast_node *right;
    uint8_t epilogue : 1;
    union {
        ssize_t v;
        char *s;
    };
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
