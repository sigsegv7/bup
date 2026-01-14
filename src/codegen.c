/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stddef.h>
#include <errno.h>
#include "bup/codegen.h"
#include "bup/trace.h"
#include "bup/mu.h"

/*
 * Emit a procedure to assembly
 *
 * @state: Compiler state
 * @root:  Root node of procedure
 *
 * Returns zero on success
 */
static int
cg_emit_proc(struct bup_state *state, struct ast_node *root)
{
    struct symbol *symbol;

    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if ((symbol = root->symbol) == NULL) {
        trace_error(state, "proc node has no symbol\n");
        return -1;
    }

    trace_debug("detected procedure %s\n", symbol->name);
    return mu_cg_label(state, symbol->name, symbol->is_global);
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
