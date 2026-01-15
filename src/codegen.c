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
    int retval = 0;

    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if ((symbol = root->symbol) == NULL && !root->epilogue) {
        trace_error(state, "proc node has no symbol\n");
        return -1;
    }

    if (root->epilogue) {
        retval = mu_cg_ret(state);
    } else {
        trace_debug("detected procedure %s\n", symbol->name);
        retval = mu_cg_label(state, symbol->name, symbol->is_global);
    }
    return retval;
}

/*
 * Emit a return to assembly
 *
 * @state: Compiler state
 * @root:  Root node of procedure
 *
 * Returns zero on success
 */
static int
cg_emit_return(struct bup_state *state, struct ast_node *root)
{
    struct ast_node *node;
    struct symbol *cur_proc;
    struct datum_type *dtype;
    msize_t ret_size;

    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if ((cur_proc = state->this_proc) == NULL) {
        trace_error(state, "return is not in procedure\n");
        return -1;
    }

    if (root->type != AST_RETURN) {
        errno = -EINVAL;
        return -1;
    }

    /* TODO: Support void returns */
    if (root->right == NULL) {
        trace_error(state, "void returns not yet supported\n");
        return -1;
    }

    dtype = &cur_proc->data_type;
    ret_size = type_to_msize(dtype->type);
    node = root->right;
    return mu_cg_retimm(state, ret_size, node->v);
}

/*
 * Emit an inline assembly line
 *
 * @state: Compiler state
 * @root:  Root node of inline assembly
 *
 * Returns zero on success
 */
static int
cg_emit_asm(struct bup_state *state, struct ast_node *root)
{
    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (root->type != AST_ASM) {
        errno = -EINVAL;
        return -1;
    }

    return mu_cg_inject(state, root->s);
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
    case AST_RETURN:
        if (cg_emit_return(state, root) < 0) {
            return -1;
        }

        return 0;
    case AST_ASM:
        if (cg_emit_asm(state, root) < 0) {
            return -1;
        }

        return 0;
    default:
        trace_error(state, "got bad ast node %d\n", root->type);
        break;
    }

    return -1;
}
