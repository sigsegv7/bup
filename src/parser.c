/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stdio.h>
#include <stdint.h>
#include "bup/lexer.h"
#include "bup/parser.h"
#include "bup/token.h"
#include "bup/tokbuf.h"
#include "bup/trace.h"
#include "bup/codegen.h"
#include "bup/ast.h"
#include "bup/types.h"
#include "bup/scope.h"

#define tokstr1(tt)             \
    toktab[(tt)]                \

#define tokstr(token)           \
    tokstr1((token)->type)      \

#define utok(state, exp, got)            \
    trace_error(                         \
        (state),                         \
        "expected %s, got %s instead\n", \
        (exp),                           \
        (got)                            \
    )

#define ueof(state)                 \
    trace_error(                    \
        (state),                    \
        "unexpected end of file\n"  \
    )

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
    [TT_SEMI]       = "SEMICOLON",
    [TT_LBRACE]     = "LBRACE",
    [TT_RBRACE]     = "RBRACE",
    [TT_ARROW]      = "ARROW",
    [TT_PROC]       = "PROC",
    [TT_PUB]        = "PUB",
    [TT_RETURN]     = "RETURN",
    [TT_U8]         = "U8",
    [TT_U16]        = "U16",
    [TT_U32]        = "U32",
    [TT_U64]        = "U64",
    [TT_VOID]       = "VOID",
    [TT_IDENT]      = "IDENT",
    [TT_NUMBER]     = "NUMBER"
};

/*
 * Perform a lookbehind
 *
 * @state: Compiler state
 * @count: Number of steps to take back
 * @res:  Resulting token written here
 *
 * Returns zero on success
 */
static inline int
parse_backstep(struct bup_state *state, size_t count, struct token *res)
{
    int error;

    error = token_buf_lookbehind(&state->tbuf, count, res);
    if (error < 0)
        return error;
    if (res->type == TT_NONE)
        return -1;

    return 0;
}

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

/*
 * Assert that the next token is of an expected type
 *
 * @state: Compiler state
 * @tok:   Token result
 * @what:  Expected token
 *
 * Returns zero on success
 */
static int
parse_expect(struct bup_state *state, struct token *tok, tt_t what)
{
    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (parse_scan(state, tok) < 0) {
        ueof(state);
        return -1;
    }

    if (tok->type != what) {
        utok(state, tokstr1(what), tokstr(tok));
        return -1;
    }

    return 0;
}

/*
 * Parse a program datatype
 *
 * @state: Compiler state
 * @tok:   Token result
 * @res:   Data type result
 */
static int
parse_type(struct bup_state *state, struct token *tok, struct datum_type *res)
{
    bup_type_t type;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    type = token_to_type(tok->type);
    if (type == BUP_TYPE_BAD) {
        utok(state, "TYPE", tokstr(tok));
        return -1;
    }

    return 0;
}

/*
 * Handle an lbrace token
 *
 * @state: Compiler state
 * @scope: Scope block token
 * @tok:   Last token
 *
 * Returns zero on success
 */
int
parse_lbrace(struct bup_state *state, tt_t scope, struct token *tok)
{
    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (tok->type != TT_LBRACE) {
        return -1;
    }

    if (scope_push(state, scope) < 0) {
        return -1;
    }

    return 0;
}

/*
 * Handle an rbrace token
 *
 * @state: Compiler state
 * @tok:   Last token
 *
 * Returns the last scope on success, otherwise TT_NONE
 * on failure.
 */
tt_t
parse_rbrace(struct bup_state *state, struct token *tok)
{
    if (state == NULL || tok == NULL) {
        return TT_NONE;
    }

    if (tok->type != TT_RBRACE) {
        return TT_NONE;
    }

    return scope_pop(state);
}

/*
 * Parse the 'proc' keyword
 *
 * @state: Compiler state
 * @tok:   Token result
 * @res: AST node result
 *
 * Returns zero on success
 */
static int
parse_proc(struct bup_state *state, struct token *tok, struct ast_node **res)
{
    struct symbol *symbol;
    struct ast_node *root;
    struct datum_type type;
    int error;
    bool is_global = false;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    /* EXPECT 'proc' */
    if (tok->type != TT_PROC) {
        utok(state, "PROC", tokstr(tok));
        return -1;
    }

    /* Is the previous token a 'pub' keyword? */
    if (parse_backstep(state, 1, tok) == 0) {
        if (tok->type == TT_PUB)
            is_global = true;
    }

    /* EXPECT <IDENT> */
    if (parse_expect(state, tok, TT_IDENT) < 0) {
        return -1;
    }

    error = symbol_new(
        &state->symtab,
        tok->s,
        BUP_TYPE_VOID,
        &symbol
    );

    if (error < 0) {
        trace_error(state, "failed to allocate function symbol\n");
        return -1;
    }

    /* EXPECT '->' */
    if (parse_expect(state, tok, TT_ARROW) < 0) {
        return -1;
    }

    if (parse_scan(state, tok) < 0) {
        ueof(state);
        return -1;
    }

    /* EXPECT <TYPE> */
    if (parse_type(state, tok, &type) < 0) {
        return -1;
    }

    if (parse_scan(state, tok) < 0) {
        ueof(state);
        return -1;
    }


    /* Initialize the symbol */
    symbol->is_global = is_global;
    symbol->data_type = type;

    /* EXPECT <SEMICOLON> OR <LBRACE> */
    switch (tok->type) {
    case TT_SEMI:
        break;
    case TT_LBRACE:
        if (parse_lbrace(state, TT_PROC, tok) < 0) {
            return -1;
        }

        /* Generate the AST root */
        if (ast_alloc_node(state, AST_PROC, &root) < 0) {
            return -1;
        }

        root->symbol = symbol;
        *res = root;
        break;
    default:
        utok(state, "SEMI OR LBRACE", tokstr(tok));
        return -1;
    }
    return 0;
}

/*
 * Parse the program source
 *
 * @state: Compiler state
 * @tok:   Last token
 *
 * Returns zero on success
 */
static int
parse_program(struct bup_state *state, struct token *tok)
{
    struct ast_node *root = NULL;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    switch (tok->type) {
    case TT_PROC:
        if (parse_proc(state, tok, &root) < 0) {
            return -1;
        }

        break;
    case TT_PUB:
        /* Modifier */
        break;
    case TT_RBRACE:
        if (parse_rbrace(state, tok) == TT_NONE) {
            return -1;
        }
        break;
    default:
        trace_error(state, "got unexpected token %s\n", tokstr(tok));
        return -1;
    }

    if (root != NULL) {
        if (cg_compile_node(state, root) < 0)
            return -1;
    }

    return 0;
}

int
parser_parse(struct bup_state *state)
{
    int error;

    if (state == NULL) {
        return -1;
    }

    while ((error = parse_scan(state, &last_token)) == 0) {
        if (parse_program(state, &last_token) < 0) {
            return -1;
        }
    }

    if (error != 0) {
        return -1;
    }

    return 0;
}
