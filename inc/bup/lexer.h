/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_LEXER_H
#define BUP_LEXER_H 1

#include "bup/token.h"
#include "bup/state.h"

/*
 * Scan for a single token
 *
 * @state: Compiler state
 * @res:   Token result
 *
 * Returns zero on success
 */
int lexer_scan(struct bup_state *state, struct token *res);

#endif  /* !BUP_LEXER_H */
