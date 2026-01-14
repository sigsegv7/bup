/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <errno.h>
#include <string.h>
#include "bup/ast.h"
#include "bup/state.h"
#include "bup/ptrbox.h"

int
ast_alloc_node(struct bup_state *state, ast_type_t type, struct ast_node **res)
{
    struct ast_node *node;

    if (state == NULL || res == NULL) {
        errno = -EINVAL;
        return -1;
    }

    node = ptrbox_alloc(&state->ptrbox, sizeof(*node));
    if (node == NULL) {
        errno = -ENOMEM;
        return -1;
    }

    memset(node, 0, sizeof(*node));
    node->type = type;
    if (res != NULL) {
        *res = node;
    }
    return 0;
}
