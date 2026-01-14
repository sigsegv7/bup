/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
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
        if (c == '\n')
            ++state->line_num;
        if (lexer_is_ws(c) && skip_ws)
            continue;

        return c;
    }

    return '\0';
}

/*
 * Scan for an identifier
 *
 * @state: Compiler state
 * @lc:    Last character
 * @res:   Result is written here
 *
 * Returns zero on success
 */
static int
lexer_scan_ident(struct bup_state *state, int lc, struct token *res)
{
    char *buf;
    size_t bufcap, bufsz;
    char c;

    bufcap = 8;
    bufsz = 0;

    if (!isalpha(lc) && lc != '_') {
        return -1;
    }

    buf = malloc(bufcap);
    if (buf == NULL) {
        return -1;
    }

    buf[bufsz++] = lc;
    for (;;) {
        c = lexer_nom(state, false);
        if (!isalnum(c) && c != '_') {
            buf[bufsz] = '\0';
            lexer_putback_chr(state, c);
            break;
        }

        buf[bufsz++] = c;
        if (bufsz >= bufcap - 1) {
            bufcap += 8;
            buf = realloc(buf, bufcap);
        }

        if (buf == NULL) {
            return -1;
        }
    }

    res->s = ptrbox_strdup(&state->ptrbox, buf);
    res->type = TT_IDENT;
    free(buf);
    return 0;
}

/*
 * Checks if an identifier token is actually a keyword and
 * reassigns its type if so
 *
 * @state: Compiler state
 * @tok:   Token to check
 *
 * Returns zero on success
 */
static int
lexer_check_kw(struct bup_state *state, struct token *tok)
{
    if (state == NULL || tok == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (tok->type != TT_IDENT) {
        errno = -EINVAL;
        return -1;
    }

    switch (*tok->s) {
    case 'p':
        if (strcmp(tok->s, "proc") == 0) {
            tok->type = TT_PROC;
            return 0;
        }

        if (strcmp(tok->s, "pub") == 0) {
            tok->type = TT_PUB;
            return 0;
        }

        break;
    }

    return -1;
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
    case '/':
        res->type = TT_SLASH;
        res->c = c;
        return 0;
    case '*':
        res->type = TT_STAR;
        res->c = c;
        return 0;
    case '>':
        res->type = TT_GT;
        res->c = c;
        if ((c = lexer_nom(state, true)) != '=') {
            lexer_putback_chr(state, c);
            return 0;
        }

        res->type = TT_GTE;
        return 0;
    case '<':
        res->type = TT_LT;
        res->c = c;
        if ((c = lexer_nom(state, true)) != '=') {
            lexer_putback_chr(state, c);
            return 0;
        }

        res->type = TT_LTE;
        return 0;
    default:
        if (lexer_scan_ident(state, c, res) == 0) {
            lexer_check_kw(state, res);
            return 0;
        }

        break;
    }

    return -1;
}
