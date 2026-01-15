/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stdio.h>
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
 * @root:  Root node of return
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

/*
 * Emit a loop
 *
 * @state: Compiler state
 * @root:  Root node of loop
 *
 * Returns zero on success
 */
static int
cg_emit_loop(struct bup_state *state, struct ast_node *root)
{
    char label_buf[32];

    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (root->type != AST_LOOP) {
        errno = -EINVAL;
        return -1;
    }

    if (!root->epilogue) {
        snprintf(
            label_buf,
            sizeof(label_buf),
            "L.%zu",
            state->loop_count++
        );
    } else {
        snprintf(
            label_buf,
            sizeof(label_buf),
            "L.%zu",
            state->loop_count - 1
        );

        mu_cg_jmp(state, label_buf);
        snprintf(
            label_buf,
            sizeof(label_buf),
            "L.%zu.1",
            state->loop_count - 1
        );
    }

    return mu_cg_label(state, label_buf, false);
}

/*
 * Emit a variable
 *
 * @state: Compiler state
 * @root:  Root node of
 *
 * Returns zero on success
 */
static int
cg_emit_var(struct bup_state *state, struct ast_node *root)
{
    struct symbol *symbol;
    struct datum_type *dtype;

    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if ((symbol = root->symbol) == NULL) {
        errno = -EIO;
        return -1;
    }


    dtype = &symbol->data_type;
    return mu_cg_globvar(
        state,
        symbol->name,
        type_to_msize(dtype->type),
        SECTION_DATA,
        0,
        false
    );
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
    case AST_LOOP:
        if (cg_emit_loop(state, root) < 0) {
            return -1;
        }

        return 0;
    case AST_VAR:
        if (cg_emit_var(state, root) < 0) {
            return -1;
        }

        return 0;
    default:
        trace_error(state, "got bad ast node %d\n", root->type);
        break;
    }

    return -1;
}
