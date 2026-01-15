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
#include "bup/trace.h"

static inline void
lexer_putback_chr(struct bup_state *state, char c)
{
    if (state == NULL) {
        return;
    }

    state->putback = c;
}

/*
 * Skip an entire line
 *
 * @state: Compiler state
 */
static void
lexer_skip_line(struct bup_state *state)
{
    char c;

    if (state == NULL) {
        return;
    }

    while (read(state->in_fd, &c, 1) > 0) {
        if (c == '\n') {
            break;
        }
    }

    ++state->line_num;
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
 * Scan for digits making up to a 64-bit integer
 *
 * @state: Compiler state
 * @lc:    Last character
 * @res:   Result token is written here
 *
 * Returns zero on success
 */
static int
lexer_scan_digits(struct bup_state *state, int lc, struct token *res)
{
    char c, buf[21];
    uint8_t buf_i = 0;

    if (state == NULL || res == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (!isdigit(lc)) {
        return -1;
    }

    buf[buf_i++] = lc;
    for (;;) {
        c = lexer_nom(state, true);
        if (!isdigit(c) && c != '_') {
            buf[buf_i] = '\0';
            lexer_putback_chr(state, c);
            break;
        }

        buf[buf_i++] = c;
        if (buf_i >= sizeof(buf) - 1) {
            return -1;
        }
    }

    res->v = atoi(buf);
    res->type = TT_NUMBER;
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
    case 'r':
        if (strcmp(tok->s, "return") == 0) {
            tok->type = TT_RETURN;
            return 0;
        }

        break;
    case 'u':
        if (strcmp(tok->s, "u8") == 0) {
            tok->type = TT_U8;
            return 0;
        }

        if (strcmp(tok->s, "u16") == 0) {
            tok->type = TT_U16;
            return 0;
        }

        if (strcmp(tok->s, "u32") == 0) {
            tok->type = TT_U32;
            return 0;
        }

        if (strcmp(tok->s, "u64") == 0) {
            tok->type = TT_U64;
            return 0;
        }

        break;
    case 'v':
        if (strcmp(tok->s, "void") == 0) {
            tok->type = TT_VOID;
            return 0;
        }

        break;
    case 'l':
        if (strcmp(tok->s, "loop") == 0) {
            tok->type = TT_LOOP;
            return 0;
        }

        break;
    }

    return -1;
}

static int
lexer_scan_asm(struct bup_state *state, struct token *tok)
{
    char c, *buf;
    size_t bufcap, bufsz;

    if (state == NULL || tok == NULL) {
        errno = -EINVAL;
        return -1;
    }

    bufcap = 8;
    bufsz = 0;
    if ((buf = malloc(bufcap)) == NULL) {
        errno = -ENOMEM;
        return -1;
    }

    c = lexer_nom(state, false);
    while (lexer_is_ws(c)) {
        c = lexer_nom(state, false);
    }

    lexer_putback_chr(state, c);
    for (;;) {
        c = lexer_nom(state, false);
        if (c == '\n') {
            buf[bufsz] = '\0';
            break;
        }

        buf[bufsz++] = c;
        if (bufsz >= bufcap - 1) {
            bufcap += 8;
            buf = realloc(buf, bufcap);
        }

        if (buf == NULL) {
            errno = -ENOMEM;
            return -1;
        }
    }

    tok->s = ptrbox_strdup(&state->ptrbox, buf);
    free(buf);
    return 0;
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
    case '@':
        res->type = TT_ASM;
        res->c = c;
        if (lexer_scan_asm(state, res) < 0) {
            return -1;
        }
        return 0;
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
        if ((c = lexer_nom(state, true)) != '/') {
            lexer_putback_chr(state, c);
            return 0;
        }

        lexer_skip_line(state);
        res->type = TT_COMMENT;
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
    case ';':
        res->type = TT_SEMI;
        res->c = c;
        return 0;
    case '{':
        res->type = TT_LBRACE;
        res->c = c;
        return 0;
    case '}':
        res->type = TT_RBRACE;
        res->c = c;
        return 0;
    case '=':
        res->type = TT_EQUALS;
        res->c = c;
        return 0;
    default:
        if (lexer_scan_ident(state, c, res) == 0) {
            lexer_check_kw(state, res);
            return 0;
        }

        if (lexer_scan_digits(state, c, res) == 0) {
            return 0;
        }

        break;
    }

    trace_error(state, "unexpected token %c\n", c);
    return -1;
}
