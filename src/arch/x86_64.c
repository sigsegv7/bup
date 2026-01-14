/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stdio.h>
#include <errno.h>
#include "bup/state.h"
#include "bup/mu.h"

int
mu_cg_label(struct bup_state *state, const char *name, bool is_global)
{
    if (state == NULL || name == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (is_global) {
        fprintf(
            state->out_fp,
            "[global %s]\n",
            name
        );
    }

    fprintf(
        state->out_fp,
        "%s:\n",
        name
    );

    return 0;
}
