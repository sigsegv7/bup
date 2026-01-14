/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <errno.h>
#include "bup/scope.h"
#include "bup/trace.h"

int
scope_push(struct bup_state *state, tt_t tok)
{
    if (state == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (state->scope_depth >= SCOPE_STACK_MAX) {
        trace_error(state, "maximum scope reached\n");
        return -1;
    }

    state->scope_stack[state->scope_depth++] = tok;
    return 0;
}

tt_t
scope_pop(struct bup_state *state)
{
    tt_t scope;

    if (state->scope_depth == 0) {
        return state->scope_stack[0];
    }

    scope = state->scope_stack[--state->scope_depth];
    state->scope_stack[state->scope_depth] = TT_NONE;
    return scope;
}

tt_t
scope_top(struct bup_state *state)
{
    if (state->scope_depth == 0) {
        return state->scope_stack[0];
    }

    return state->scope_stack[state->scope_depth - 1];
}
