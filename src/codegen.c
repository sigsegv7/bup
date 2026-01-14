/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stddef.h>
#include <errno.h>
#include "bup/codegen.h"
#include "bup/trace.h"

static int
cg_emit_proc(struct bup_state *state, struct ast_node *root)
{
    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    trace_error(state, "procedures are a TODO\n");
    return -1;
}

int
cg_compile_node(struct bup_state *state, struct ast_node *root)
{
    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    switch (root->type) {
    case AST_PROC:
        if (cg_emit_proc(state, root) < 0) {
            return -1;
        }

        return 0;
    default:
        trace_error(state, "got bad ast node %d\n", root->type);
        break;
    }

    return -1;
}
