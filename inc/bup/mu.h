/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_MU_H
#define BUP_MU_H 1

#include <stdbool.h>
#include "bup/state.h"

/*
 * Generate a label of a specific name
 *
 * @state:      Compiler state
 * @name:       Name of label to create
 * @is_global:  If true, label shall be global
 *
 * Returns zero on success
 */
int mu_cg_label(struct bup_state *state, const char *name, bool is_global);

/*
 * Generate a 'ret'
 *
 * @state: Compiler state
 *
 * Returns zero on success
 */
int mu_cg_ret(struct bup_state *state);

#endif  /* !BUP_MU_H */
