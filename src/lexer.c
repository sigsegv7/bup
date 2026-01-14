/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <stdbool.h>
#include "bup/lexer.h"

static inline void
lexer_putback_chr(struct bup_state *state, char c)
{
    if (state == NULL) {
        return;
    }

    state->putback = c;
}

/*
 * Returns true if the given character counts
 * as a whitespace character.
 */
static inline bool
lexer_is_ws(char c)
{
    switch (c) {
    case '\n':
    case '\t':
    case '\f':
    case ' ':
        return true;
    }

    return false;
}

/*
 * Nom a single character from the input source file.
 *
 * @state: Compiler state
 * @skip_ws: If true, skip whitespace
 *
 * Returns '\0' when there are no more characters,
 * otherwise the next character on success
 */
static char
lexer_nom(struct bup_state *state, bool skip_ws)
{
    char c;

    if (state == NULL) {
        return '\0';
    }

    /*
     * If there is something in the putback buffer, grab it
     * and if it is not a whitespace, return it.
     *
     * XXX: We do not want to handle whitespace at all as we are
     *      not a whitespace significant language. That would be
     *      silly.
     */
    if ((c = state->putback) != '\0') {
        state->putback = '\0';
        if (!lexer_is_ws(c))
            return c;
    }

    /* Begin scanning for tokens */
    while (read(state->in_fd, &c, 1) > 0) {
        if (lexer_is_ws(c) && skip_ws)
            continue;

        return c;
    }

    return '\0';
}

int
lexer_scan(struct bup_state *state, struct token *res)
{
    char c;

    if (state == NULL || res == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if ((c = lexer_nom(state, true)) == '\0') {
        return -1;
    }

    switch (c) {
    case '+':
        res->type = TT_PLUS;
        res->c = c;
        return 0;
    case '-':
        res->type = TT_MINUS;
        res->c = c;
        if ((c = lexer_nom(state, true)) != '>') {
            lexer_putback_chr(state, c);
            return 0;
        }

        res->type = TT_ARROW;
        return 0;
    }

    return -1;
}
