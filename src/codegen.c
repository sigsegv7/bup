/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "bup/codegen.h"
#include "bup/trace.h"
#include "bup/mu.h"

static void
cg_field_assign(struct bup_state *state, struct ast_node *symbol_node,
    struct ast_node *root, struct ast_node *value_node)
{
    char buf[256];
    struct ast_node *cur;
    struct symbol *symbol, *instance;

    if (root == NULL) {
        return;
    }

    if (root->type != AST_FIELD_ACCESS) {
        return;
    }

    if ((symbol = symbol_node->symbol) == NULL) {
        return;
    }

    snprintf(buf, sizeof(buf), "%s.", symbol->name);
    cur = root->right;
    instance = symbol->parent;

    while (cur != NULL) {
        instance = cur->symbol;
        strncat(buf, cur->s, sizeof(buf) - 1);
        if ((cur = cur->right) != NULL)
            strncat(buf, ".", sizeof(buf) - 1);
    }

    mu_cg_istorevar(
        state,
        datum_msize(&instance->data_type),
        buf,
        value_node->v
    );
}

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
    node = root->right;
    return mu_cg_retimm(state, datum_msize(dtype), node->v);
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
 * @root:  Root node of variable
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
    if (dtype->array_size > 0) {
        return mu_cg_array(
            state,
            symbol->name,
            symbol->is_global,
            dtype->array_size
        );
    }

    return mu_cg_globvar(
        state,
        symbol->name,
        datum_msize(dtype),
        SECTION_DATA,
        0,
        symbol->is_global
    );
}

/*
 * Emit a break
 *
 * @state: Compiler state
 * @root:  Root node of break
 *
 * Returns zero on success
 */
static int
cg_emit_break(struct bup_state *state, struct ast_node *root)
{
    char label_buf[32];

    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (root->type != AST_BREAK) {
        errno = -EINVAL;
        return -1;
    }

    snprintf(label_buf, sizeof(label_buf), "L.%zu.1", state->loop_count - 1);
    mu_cg_jmp(state, label_buf);
    return 0;
}

/*
 * Emit a variable definition
 *
 * @state: Compiler state
 * @root:  Root node of variable
 *
 * Returns zero on success
 */
static int
cg_emit_vardef(struct bup_state *state, struct ast_node *root)
{
    struct ast_node *expr;
    struct symbol *symbol;
    struct datum_type *dtype;

    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if ((symbol = root->left->symbol) == NULL) {
        errno = -EIO;
        return -1;
    }

    /* XXX: Perhaps handle binary expressions */
    if ((expr = root->right) == NULL) {
        errno = -EIO;
        return -1;
    }

    dtype = &symbol->data_type;
    return mu_cg_globvar(
        state,
        symbol->name,
        type_to_msize(dtype->type),
        SECTION_DATA,
        expr->v,
        symbol->is_global
    );
}

/*
 * Emit a continue
 *
 * @state: Compiler state
 * @root:  Root node of continue
 */
static int
cg_emit_cont(struct bup_state *state, struct ast_node *root)
{
    char label_buf[32];

    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (root->type != AST_CONT) {
        errno = -EINVAL;
        return -1;
    }

    snprintf(label_buf, sizeof(label_buf), "L.%zu", state->loop_count - 1);
    return mu_cg_jmp(state, label_buf);
}

/*
 * Emit an if
 *
 * @state: Compiler state
 * @root:  Root node of if
 */
static int
cg_emit_if(struct bup_state *state, struct ast_node *root)
{
    struct ast_node *expr;
    char label_buf[32];

    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (root->type != AST_IF) {
        errno = -EINVAL;
        return -1;
    }

    if ((expr = root->right) == NULL && !root->epilogue) {
        errno = -EIO;
        return -1;
    }

    /*
     * If this is the epilogue, simply create a label
     * to jump to if the condition fails.
     */
    if (root->epilogue) {
        snprintf(
            label_buf,
            sizeof(label_buf),
            "IF.%zu",
            state->if_count - 1
        );
        mu_cg_label(state, label_buf, false);
        return 0;
    }

    snprintf(
        label_buf,
        sizeof(label_buf),
        "IF.%zu",
        state->if_count++
    );

    /* TODO: Handle more complex expressions */
    return mu_cg_icmpnz(state, label_buf, expr->v);
}

/*
 * Emit an assign
 *
 * @state: Compiler state
 * @root:  Node root of assign
 */
static int
cg_emit_assign(struct bup_state *state, struct ast_node *root)
{
    struct datum_type *dtype;
    struct ast_node *symbol_node, *value_node;
    struct ast_node *field_node;
    struct symbol *symbol;

    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (root->type != AST_ASSIGN) {
        errno = -EINVAL;
        return -1;
    }

    if ((value_node = root->right) == NULL) {
        trace_error(state, "assign has no rhs\n");
        errno = -EIO;
        return -1;
    }

    /* TODO: Support a non-numeric lhs */
    if (value_node->type != AST_NUMBER) {
        trace_error(state, "non-numeric rhs for assign unsupported\n");
        return -1;
    }

    if ((symbol_node = root->left) == NULL) {
        trace_error(state, "assign has no lhs\n");
        errno = -EIO;
        return -1;
    }

    if ((field_node = root->mid) != NULL) {
        cg_field_assign(
            state,
            symbol_node,
            field_node->mid,
            field_node->right
        );
        return 0;
    }

    if ((symbol = symbol_node->symbol) == NULL) {
        errno = -EIO;
        return -1;
    }

    dtype = &symbol->data_type;
    return mu_cg_istorevar(
        state,
        datum_msize(dtype),
        symbol->name,
        value_node->v
    );
}

/*
 * Emit a procedure call
 *
 * @state: Compiler state
 * @root:  Node root of call
 */
static int
cg_emit_call(struct bup_state *state, struct ast_node *root)
{
    struct ast_node *symbol_node;
    struct symbol *symbol;

    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (root->type != AST_CALL) {
        errno = -EINVAL;
        return -1;
    }

    if ((symbol_node = root->left) == NULL) {
        trace_error(state, "no lhs for call node\n");
        errno = -EINVAL;
        return -1;
    }

    if (symbol_node->type != AST_SYMBOL) {
        trace_error(state, "call node lhs is not symbol\n");
        errno = -EIO;
        return -1;
    }

    if ((symbol = symbol_node->symbol) == NULL) {
        trace_error(state, "no symbol on call lhs\n");
        errno = -EIO;
        return -1;
    }

    if (symbol->type != SYMBOL_FUNC) {
        trace_error(state, "called symbol is not function\n");
        errno = -EIO;
        return -1;
    }

    return mu_cg_call(state, symbol->name);
}

/*
 * Emit a structure
 *
 * @state: Compiler state
 * @root:  Node root of structure
 */
static int
cg_emit_struct(struct bup_state *state, struct ast_node *root)
{
    struct symbol *symbol;
    struct symbol *instance;
    struct ast_node *symbol_node, *instance_node;

    if (state == NULL || root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (root == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (root->type != AST_STRUCT) {
        errno = -EINVAL;
        return -1;
    }

    if ((symbol_node = root->left) == NULL) {
        trace_error(state, "struct has no lhs\n");
        return -1;
    }

    if ((instance_node = root->right) == NULL) {
        trace_error(state, "struct has no rhs\n");
        return -1;
    }

    if ((instance = instance_node->symbol) == NULL) {
        trace_error(state, "struct rhs has no symbol\n");
        return -1;
    }

    if ((symbol = symbol_node->symbol) == NULL) {
        trace_error(state, "struct lhs has no symbol\n");
        return -1;
    }

    return mu_cg_struct(state, instance->name, symbol);
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
    case AST_BREAK:
        if (cg_emit_break(state, root) < 0) {
            return -1;
        }

        return 0;
    case AST_CONT:
        if (cg_emit_cont(state, root) < 0) {
            return -1;
        }

        return 0;
    case AST_VARDEF:
        if (cg_emit_vardef(state, root) < 0) {
            return -1;
        }

        return 0;
    case AST_IF:
        if (cg_emit_if(state, root) < 0) {
            return -1;
        }

        return 0;
    case AST_ASSIGN:
        if (cg_emit_assign(state, root) < 0) {
            return -1;
        }

        return 0;
    case AST_CALL:
        if (cg_emit_call(state, root) < 0) {
            return -1;
        }

        return 0;
    case AST_STRUCT:
        if (cg_emit_struct(state, root) < 0) {
            return -1;
        }

        return 0;
    default:
        trace_error(state, "got bad ast node %d\n", root->type);
        break;
    }

    return -1;
}
