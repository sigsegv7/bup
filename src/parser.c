#include <stdio.h>
#include <stdint.h>
#include "bup/lexer.h"
#include "bup/parser.h"
#include "bup/token.h"

static struct token last_token;

/*
 * Represents a table used to convert tokens to human
 * readable strings.
 */
static const char *toktab[] = {
    [TT_NONE]       = "NONE",
    [TT_PLUS]       = "PLUS",
    [TT_MINUS]      = "MINUS",
    [TT_ARROW]      = "ARROW"
};

int
parser_parse(struct bup_state *state)
{
    if (state == NULL) {
        return -1;
    }

    while (lexer_scan(state, &last_token) == 0) {
        printf("got %s\n", toktab[last_token.type]);
    }

    return 0;
}
