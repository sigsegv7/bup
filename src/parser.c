#include <stdio.h>
#include <stdint.h>
#include "bup/lexer.h"
#include "bup/parser.h"
#include "bup/token.h"
#include "bup/tokbuf.h"

static struct token last_token;

/*
 * Represents a table used to convert tokens to human
 * readable strings.
 */
static const char *toktab[] = {
    [TT_NONE]       = "NONE",
    [TT_PLUS]       = "PLUS",
    [TT_MINUS]      = "MINUS",
    [TT_SLASH]      = "SLASH",
    [TT_STAR]       = "STAR",
    [TT_GT]         = "GREATER-THAN",
    [TT_LT]         = "LESS-THAN",
    [TT_GTE]        = "GREATER-THAN-OR-EQUAL",
    [TT_LTE]        = "LESS-THAN-OR-EQUAL",
    [TT_ARROW]      = "ARROW",
};

/*
 * Scan for a single token
 *
 * @state: Compiler state
 * @tok:   Token result
 *
 * XXX: This function should be used instead of lexer_scan()
 *      as tokens must be recorded by the parser.
 *
 * Returns zero on success
 */
static int
parse_scan(struct bup_state *state, struct token *tok)
{
    if (state == NULL) {
        return -1;
    }

    if (lexer_scan(state, tok) < 0) {
        return -1;
    }

    token_buf_push(&state->tbuf, tok);
    return 0;
}

int
parser_parse(struct bup_state *state)
{
    if (state == NULL) {
        return -1;
    }

    while (parse_scan(state, &last_token) == 0) {
        printf("got %s\n", toktab[last_token.type]);
    }

    return 0;
}
