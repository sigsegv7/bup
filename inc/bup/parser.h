/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_PARSER_H
#define BUP_PARSER_H 1

#include "bup/state.h"

/*
 * Begin parsing the input source file
 *
 * @state: Compiler state
 */
int parser_parse(struct bup_state *state);

#endif  /* !BUP_PARSER_H */
