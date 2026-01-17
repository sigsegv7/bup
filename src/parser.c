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

#define utok1(state, tok)        \
    trace_error(                 \
        (state),                 \
        "unexpected token %s\n", \
        tokstr(tok)              \
    )

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
    [TT_ASM]        = "ASM",
    [TT_DOT]        = "DOT",
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
    [TT_EQUALS]     = "EQUALS",
    [TT_LPAREN]     = "LPAREN",
    [TT_RPAREN]     = "RPAREN",
    [TT_LBRACK]     = "LBRACK",
    [TT_RBRACK]     = "RBRACK",
    [TT_ARROW]      = "ARROW",
    [TT_PROC]       = "PROC",
    [TT_PUB]        = "PUB",
    [TT_RETURN]     = "RETURN",
    [TT_U8]         = "U8",
    [TT_U16]        = "U16",
    [TT_U32]        = "U32",
    [TT_U64]        = "U64",
    [TT_UPTR]       = "UPTR",
    [TT_VOID]       = "VOID",
    [TT_LOOP]       = "LOOP",
    [TT_BREAK]      = "BREAK",
    [TT_CONT]       = "CONTINUE",
    [TT_IF]         = "IF",
    [TT_STRUCT]     = "STRUCT",
    [TT_TYPE]       = "TYPE",
    [TT_IDENT]      = "IDENT",
    [TT_NUMBER]     = "NUMBER",
    [TT_COMMENT]    = "COMMENT"
};

/* Lookup table used to convert types to sizes */
uint8_t typesztab[] = {
    [BUP_TYPE_BAD] = 0,
    [BUP_TYPE_VOID] = 0,
    [BUP_TYPE_U8] = 1,
    [BUP_TYPE_U16] = 2,
    [BUP_TYPE_U32] = 4,
    [BUP_TYPE_U64] = 8
};

/*
 * Parse a token in the parser-side putback buffer
 *
 * @state: Compiler state
 * @tok:   Token to putback
 */
static inline void
parse_putback(struct bup_state *state, struct token *tok)
{
    state->parse_putback = *tok;
}

/*
 * Pop a token from the parser-side putback buffer
 *
 * @state: Compiler state
 * @res:   Result is written here
 *
 * Returns zero on success
 */
static inline int
parse_putback_pop(struct bup_state *state, struct token *res)
{
    struct token *top;

    top = &state->parse_putback;
    if (top->type == TT_NONE) {
        return -1;
    }

    *res = *top;
    top->type = TT_NONE;
    return 0;
}

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

    if (parse_putback_pop(state, tok) == 0) {
        return 0;
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
 * Parse an array
 *
 * @state:  Compiler state
 * @tok:    Token result
 * @dtype:  Data type
 */
static int
parse_array(struct bup_state *state, struct token *tok, struct datum_type *dtype)
{
    size_t sz;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (dtype == NULL) {
        return -1;
    }

    if (tok->type != TT_LBRACK) {
        return -1;
    }

    /* EXPECT <NUMBER> */
    if (parse_expect(state, tok, TT_NUMBER) < 0) {
        return -1;
    }

    sz = tok->v * typesztab[dtype->type];
    dtype->array_size = sz;

    /* EXPECT ']' */
    if (parse_expect(state, tok, TT_RBRACK) < 0) {
        return -1;
    }

    if (parse_scan(state, tok) < 0) {
        ueof(state);
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
    struct symbol *type_symbol;
    bup_type_t type;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    type = token_to_type(tok->type);
    res->ptr_depth = 0;
    res->array_size = 0;

    /*
     * If this is a bad token, verify that it is not
     * a typedef we are referring to.
     */
    if (type == BUP_TYPE_BAD) {
        /* Is this a typedef? */
        type_symbol = symbol_from_name(&state->symtab, tok->s);
        if (type_symbol == NULL) {
            utok(state, "TYPE", tokstr(tok));
            return -1;
        }

        /* Verify that it is indeed a typedef*/
        if (type_symbol->type != SYMBOL_TYPEDEF) {
            utok(state, "TYPE", tokstr(tok));
            return -1;
        }

        *res = type_symbol->data_type;
    } else {
        res->type = type;
    }

    if (parse_scan(state, tok) < 0) {
        ueof(state);
        return -1;
    }

    while (tok->type == TT_STAR) {
        ++res->ptr_depth;
        if (parse_scan(state, tok) < 0) {
            ueof(state);
            return -1;
        }
    }

    parse_putback(state, tok);
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
static int
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
static tt_t
parse_rbrace(struct bup_state *state, struct token *tok)
{
    struct ast_node *root;
    tt_t scope;

    if (state == NULL || tok == NULL) {
        return TT_NONE;
    }

    if (tok->type != TT_RBRACE) {
        return TT_NONE;
    }

    /* Handle scope epilogues */
    scope = scope_pop(state);
    switch (scope) {
    case TT_PROC:
        state->this_proc = NULL;
        if (state->unreachable) {
            state->unreachable = 0;
            break;
        }

        if (ast_alloc_node(state, AST_PROC, &root) < 0) {
            trace_error(state, "failed to allocate AST_PROC\n");
            return -1;
        }

        root->epilogue = 1;
        if (cg_compile_node(state, root) < 0) {
            return -1;
        }

        break;
    case TT_LOOP:
        if (state->unreachable) {
            state->unreachable = 0;
            break;
        }

        if (ast_alloc_node(state, AST_LOOP, &root) < 0) {
            trace_error(state, "failed to allocate AST_PROC\n");
            return -1;
        }

        root->epilogue = 1;
        if (cg_compile_node(state, root) < 0) {
            return -1;
        }

        break;
    case TT_IF:
        if (state->unreachable) {
            state->unreachable = 0;
            break;
        }

        if (ast_alloc_node(state, AST_IF, &root) < 0) {
            trace_error(state, "failed to allocate AST_PROC\n");
            return -1;
        }

        root->epilogue = 1;
        if (cg_compile_node(state, root) < 0) {
            return -1;
        }
        break;
    default:
        break;
    }

    return scope;
}

/*
 * Parse a binary expression
 *
 * @state: Compiler state
 * @tok:   Token result
 * @res:   AST node result
 */
static int
parse_binexpr(struct bup_state *state, struct token *tok, struct ast_node **res)
{
    struct ast_node *root;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    switch (tok->type) {
    case TT_NUMBER:
        if (ast_alloc_node(state, AST_NUMBER, &root) < 0) {
            trace_error(state, "failed to allocate AST_NUMBER\n");
            return -1;
        }

        root->v = tok->v;
        *res = root;
        return 0;
    default:
        utok1(state, tok);
        break;
    }

    return -1;
}

/*
 * Parse the 'return' keyword
 *
 * @state: Compiler state
 * @tok:   Token result
 * @res:   AST node result
 */
static int
parse_return(struct bup_state *state, struct token *tok, struct ast_node **res)
{
    struct ast_node *value_node, *root;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    if (tok->type != TT_RETURN) {
        return -1;
    }

    if (parse_scan(state, tok) < 0) {
        ueof(state);
        return -1;
    }

    if (ast_alloc_node(state, AST_RETURN, &root) < 0) {
        trace_error(state, "failed to allocate AST_RETURN\n");
        return -1;
    }

    state->unreachable = 1;
    switch (tok->type) {
    case TT_SEMI:
        *res = root;
        return 0;
    default:
        if (parse_binexpr(state, tok, &value_node) < 0) {
            return -1;
        }

        if (parse_expect(state, tok, TT_SEMI) < 0) {
            return -1;
        }

        root->right = value_node;
        *res = root;
        break;
    }

    return 0;
}

/*
 * Parse 'proc' arguments
 *
 * @state: Compiler state
 * @tok:   Token result
 * @res: AST node result
 *
 * Returns the number of arguments on success, otherwise a less
 * than zero value on failure.
 */
static ssize_t
parse_proc_args(struct bup_state *state, struct token *tok, struct ast_node **res)
{
    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    if (tok->type != TT_LPAREN) {
        return -1;
    }

    /* TODO: Support arguments */
    if (parse_expect(state, tok, TT_RPAREN) < 0) {
        return -1;
    }

    if (parse_scan(state, tok) < 0) {
        ueof(state);
        return -1;
    }

    return 0;
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
    struct ast_node *root, *args;
    struct datum_type type;
    int error;
    bool is_global = false;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    /* If we are already in a function, error */
    if (state->this_proc != NULL) {
        trace_error(state, "function nesting is not supported\n");
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

    if (parse_scan(state, tok) < 0) {
        ueof(state);
        return -1;
    }

    /* MAYBE '(' : parse arguments if true */
    if (tok->type == TT_LPAREN) {
        if (parse_proc_args(state, tok, &args) < 0)
            return -1;
    }

    /* EXPECT '->' */
    if (tok->type != TT_ARROW) {
        utok(state, "ARROW", tokstr(tok));
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
    symbol->type = SYMBOL_FUNC;
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

        state->this_proc = symbol;
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
 * Parse as 'asm' block
 *
 * @state: Compiler state
 * @tok:   Token result
 * @res: AST node result
 *
 * Returns zero on success
 */
static int
parse_asm(struct bup_state *state, struct token *tok, struct ast_node **res)
{
    struct ast_node *root;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    if (tok->type != TT_ASM) {
        return -1;
    }

    if (ast_alloc_node(state, AST_ASM, &root) < 0) {
        trace_error(state, "failed to allocate AST_ASM\n");
        return -1;
    }

    root->s = tok->s;
    *res = root;
    return 0;
}

/*
 * Parse a 'loop' block
 *
 * @state: Compiler state
 * @tok:   Token result
 * @res: AST node result
 *
 * Returns zero on success
 */
static int
parse_loop(struct bup_state *state, struct token *tok, struct ast_node **res)
{
    struct ast_node *root;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    if (state->this_proc == NULL) {
        trace_error(state, "'loop' must be within a procedure\n");
        return -1;
    }

    if (parse_scan(state, tok) < 0) {
        ueof(state);
        return -1;
    }

    if (tok->type != TT_LBRACE) {
        utok(state, "LBRACE", tokstr(tok));
        return -1;
    }

    if (parse_lbrace(state, TT_LOOP, tok) < 0) {
        return -1;
    }

    if (ast_alloc_node(state, AST_LOOP, &root) < 0) {
        trace_error(state, "failed to allocate AST_LOOP\n");
        return -1;
    }

    *res = root;
    return 0;
}

/*
 * Parse a variable declaration / definition
 *
 * @state: Compiler state
 * @tok:   Last token
 * @res:   AST node result
 *
 * Returns zero on success
 */
static int
parse_var(struct bup_state *state, struct token *tok, struct ast_node **res)
{
    struct token tmp_tok;
    struct ast_node *root, *node, *expr;
    struct datum_type type;
    struct symbol *symbol;
    bool is_global = false;
    int error;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    /* TODO: Support local variables */
    if (state->this_proc != NULL) {
        trace_error(state, "local variables are currently unsupported\n");
        return -1;
    }

    /* Is this symbol global? */
    if (parse_backstep(state, 1, &tmp_tok) == 0) {
        if (tmp_tok.type == TT_PUB)
            is_global = true;
    }

    if (parse_type(state, tok, &type) < 0) {
        return -1;
    }

    if (parse_expect(state, tok, TT_IDENT) < 0) {
        return -1;
    }

    error = symbol_new(
        &state->symtab,
        tok->s,
        BUP_TYPE_BAD,
        &symbol
    );

    if (error < 0) {
        trace_error(state, "failed to allocate symbol\n");
        return -1;
    }

    symbol->data_type = type;
    if (parse_scan(state, tok) < 0) {
        ueof(state);
        return -1;
    }

    if (ast_alloc_node(state, AST_VAR, &root) < 0) {
        trace_error(state, "failed to allocate AST_VAR\n");
        return -1;
    }

    symbol->type = SYMBOL_VAR;
    symbol->is_global = is_global;
    root->symbol = symbol;

    /* MAYBE: <ARRAY> */
    if (parse_array(state, tok, &type) == 0) {
        symbol->data_type = type;
    }

    switch (tok->type) {
    case TT_EQUALS:
        if (parse_scan(state, tok) < 0) {
            ueof(state);
            return -1;
        }

        if (parse_binexpr(state, tok, &expr) < 0) {
            return -1;
        }

        if (parse_expect(state, tok, TT_SEMI) < 0) {
            return -1;
        }

        /* Make the node become root */
        node = root;
        if (ast_alloc_node(state, AST_VARDEF, &root) < 0) {
            trace_error(state, "failed to allocate AST_VARDEF\n");
            return -1;
        }

        root->left = node;
        root->right = expr;
        break;
    case TT_SEMI:
        break;
    default:
        utok1(state, tok);
        return -1;
    }

    *res = root;
    return 0;
}

/*
 * Parse a break statement
 *
 * @state: Compiler state
 * @tok:   Last token
 * @res:   AST node result
 */
static int
parse_break(struct bup_state *state, struct token *tok, struct ast_node **res)
{
    struct ast_node *root;

    if (state == NULL || res == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    if (tok->type != TT_BREAK) {
        return -1;
    }

    if (parse_expect(state, tok, TT_SEMI) < 0) {
        return -1;
    }

    if (ast_alloc_node(state, AST_BREAK, &root) < 0) {
        trace_error(state, "failed to allocate AST_BREAK\n");
        return -1;
    }

    *res = root;
    return 0;
}

/*
 * Parse a continue statement
 *
 * @state: Compiler state
 * @tok:   Last token
 * @res:   AST node result
 */
static int
parse_continue(struct bup_state *state, struct token *tok, struct ast_node **res)
{
    struct ast_node *root;

    if (state == NULL || res == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    if (tok->type != TT_CONT) {
        return -1;
    }

    if (parse_expect(state, tok, TT_SEMI) < 0) {
        return -1;
    }

    if (ast_alloc_node(state, AST_CONT, &root) < 0) {
        trace_error(state, "failed to allocate AST_CONT\n");
        return -1;
    }

    *res = root;
    return 0;
}

/*
 * Parse an if statement
 *
 * @state: Compiler state
 * @tok:   Last token
 * @res:   Result AST root is written here
 */
static int
parse_if(struct bup_state *state, struct token *tok, struct ast_node **res)
{
    struct ast_node *root, *expr;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    if (tok->type != TT_IF) {
        return -1;
    }

    if (state->this_proc == NULL) {
        trace_error(state, "IF statement must be in procedure\n");
        return -1;
    }

    /* EXPECT '(' */
    if (parse_expect(state, tok, TT_LPAREN) < 0) {
        return -1;
    }

    if (ast_alloc_node(state, AST_IF, &root) < 0) {
        trace_error(state, "failed to allocate AST_IF\n");
        return -1;
    }

    if (parse_scan(state, tok) < 0) {
        ueof(state);
        return -1;
    }

    /* EXPECT <EXPR> */
    if (parse_binexpr(state, tok, &expr) < 0) {
        return -1;
    }

    /* EXPECT ')' */
    if (parse_expect(state, tok, TT_RPAREN) < 0) {
        return -1;
    }

    if (parse_scan(state, tok) < 0) {
        return -1;
    }

    if (parse_lbrace(state, TT_IF, tok) < 0) {
        utok(state, "LBRACE", tokstr(tok));
        return -1;
    }

    root->right = expr;
    *res = root;
    return 0;
}

/*
 * Parse an assignment
 *
 * @state: Compiler state
 * @tok:   Last token
 * @sym:   Symbol being assigned
 * @res:   AST node result
 */
static int
parse_assign(struct bup_state *state, struct token *tok, struct symbol *sym,
    struct ast_node **res)
{
    struct ast_node *symbol_node;
    struct ast_node *root, *expr;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    if (tok->type != TT_EQUALS) {
        return -1;
    }

    if (sym->type != SYMBOL_VAR) {
        trace_error(state, "cannot re-assign to non-variable\n");
        return -1;
    }

    if (ast_alloc_node(state, AST_ASSIGN, &root) < 0) {
        trace_error(state, "failed to allocate AST_ASSIGN\n");
        return -1;
    }

    if (parse_scan(state, tok) < 0) {
        ueof(state);
        return -1;
    }

    if (parse_binexpr(state, tok, &expr) < 0) {
        return -1;
    }

    if (parse_expect(state, tok, TT_SEMI) < 0) {
        return -1;
    }

    if (ast_alloc_node(state, AST_SYMBOL, &symbol_node) < 0) {
        trace_error(state, "failed to allocate AST_SYMBOL\n");
        return -1;
    }

    symbol_node->symbol = sym;
    root->left = symbol_node;
    root->right = expr;
    *res = root;
    return 0;
}

/*
 * Parse an encountered identifier
 *
 * @state: Compiler state
 * @tok:   Last token
 * @res:   AST root result
 *
 * Returns zero on success
 */
static int
parse_ident(struct bup_state *state, struct token *tok, struct ast_node **res)
{
    struct ast_node *root;
    struct ast_node *symbol_node;
    struct symbol *symbol;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    if (tok->type != TT_IDENT) {
        return -1;
    }

    symbol = symbol_from_name(&state->symtab, tok->s);
    if (symbol == NULL) {
        trace_error(state, "undefined reference to %s\n", tok->s);
        return -1;
    }

    if (symbol->type == SYMBOL_TYPEDEF) {
        return parse_var(state, tok, res);
    }

    if (parse_scan(state, tok) < 0) {
        ueof(state);
        return -1;
    }

    switch (tok->type) {
    case TT_EQUALS:
        if (parse_assign(state, tok, symbol, &root) < 0) {
            return -1;
        }

        *res = root;
        return 0;
    case TT_LPAREN:
        /*
         * Procedure call
         *
         * TODO: Handle arguments
         */
        if (symbol->type != SYMBOL_FUNC) {
            trace_error(state, "cannot call non-function\n");
            return -1;
        }

        if (parse_expect(state, tok, TT_RPAREN) < 0) {
            return -1;
        }

        if (parse_expect(state, tok, TT_SEMI) < 0) {
            return -1;
        }

        if (ast_alloc_node(state, AST_CALL, &root) < 0) {
            trace_error(state, "failed to allocate AST_CALL\n");
            return -1;
        }

        if (ast_alloc_node(state, AST_SYMBOL, &symbol_node) < 0) {
            trace_error(state, "failed to allocate AST_SYMBOL\n");
            return -1;
        }

        symbol_node->symbol = symbol;
        root->left = symbol_node;
        *res = root;
        return 0;
    default:
        utok1(state, tok);
        break;
    }

    return -1;
}

static int
parse_struct_fields(struct bup_state *state, struct token *tok, struct symbol *struc)
{
    struct symbol *symbol, *instance;
    struct datum_type dtype;
    int error;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (struc == NULL) {
        return -1;
    }

    for (;;) {
        switch (tok->type) {
        case TT_STRUCT:
            if (parse_expect(state, tok, TT_IDENT) < 0) {
                return -1;
            }

            symbol = symbol_from_name(&state->symtab, tok->s);
            if (symbol == NULL) {
                trace_error(state, "undefined reference to struct %s\n", tok->s);
                return -1;
            }

            if (symbol->type != SYMBOL_STRUCT) {
                trace_error(state, "symbol %s is not a struct!\n", tok->s);
                return -1;
            }

            if (parse_expect(state, tok, TT_IDENT) < 0) {
                return -1;
            }

            error = symbol_field_new(
                struc,
                tok->s,
                BUP_TYPE_VOID,
                &instance
            );

            instance->type = SYMBOL_STRUCT;
            instance->parent = symbol;
            break;
        default:
            if (parse_type(state, tok, &dtype) < 0) {
                return -1;
            }

            if (parse_expect(state, tok, TT_IDENT) < 0) {
                return -1;
            }

            error = symbol_field_new(
                struc,
                tok->s,
                BUP_TYPE_VOID,
                &instance
            );

            if (error < 0) {
                trace_error(state, "failed to allocate field symbol\n");
                return -1;
            }

            instance->data_type = dtype;
            break;
        }

        if (parse_expect(state, tok, TT_SEMI) < 0) {
            return -1;
        }

        if (parse_scan(state, tok) < 0) {
            ueof(state);
            return -1;
        }

        if (tok->type == TT_RBRACE) {
            if (parse_rbrace(state, tok) < 0)
                return -1;

            break;
        }
    }

    return 0;
}

/*
 * Parse a structure
 *
 * @state: Compiler state
 * @tok:   Last token
 * @res:   AST node result
 */
static int
parse_struct(struct bup_state *state, struct token *tok, struct ast_node **res)
{
    struct token ahead;
    struct ast_node *root, *lhs, *rhs;
    struct symbol *struct_symbol;
    struct symbol *instance_symbol;
    int error;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    if (res == NULL) {
        return -1;
    }

    /* EXPECT 'struct' */
    if (tok->type != TT_STRUCT) {
        return -1;
    }

    /* EXPECT <IDENT> */
    if (parse_expect(state, tok, TT_IDENT) < 0) {
        return -1;
    }

    /* EXPECT ';' OR '{' */
    if (parse_scan(state, &ahead) < 0) {
        ueof(state);
        return -1;
    }

    switch (ahead.type) {
    case TT_SEMI:
        error = symbol_new(
            &state->symtab,
            tok->s,
            BUP_TYPE_VOID,
            &struct_symbol
        );

        struct_symbol->type = SYMBOL_STRUCT;
        if (error < 0) {
            trace_error(state, "failed to allocate struct symbol\n");
            return -1;
        }

        return 0;
    case TT_IDENT:
        /*
         * Struct instance
         */

        /* TODO */
        if (state->this_proc != NULL) {
            trace_error(state, "global structures supported only as of now\n");
            return -1;
        }

        struct_symbol = symbol_from_name(
            &state->symtab,
            tok->s
        );

        if (struct_symbol == NULL) {
            trace_error(state, "undefined reference to structure %s\n", tok->s);
            return -1;
        }

        if (struct_symbol->type != SYMBOL_STRUCT) {
            trace_error(state, "cannot instantiate non-structure\n");
            return -1;
        }

        /* Create an instance symbol */
        error = symbol_new(
            &state->symtab,
            ahead.s,
            BUP_TYPE_VOID,
            &instance_symbol
        );

        if (error < 0) {
            trace_error(state, "failed to allocate instance symbol\n");
            return -1;
        }

        if (parse_expect(state, tok, TT_SEMI) < 0) {
            return -1;
        }

        if (ast_alloc_node(state, AST_STRUCT, &root) < 0) {
            trace_error(state, "failed to allocate AST_STRUCT\n");
            return -1;
        }

        if (ast_alloc_node(state, AST_SYMBOL, &lhs) < 0) {
            trace_error(state, "failed to allocate lhs AST_SYMBOL\n");
            return -1;
        }

        if (ast_alloc_node(state, AST_SYMBOL, &rhs) < 0) {
            trace_error(state, "failed to allocate rhs AST_SYMBOL\n");
            return -1;
        }

        instance_symbol->type = SYMBOL_VAR;
        rhs->symbol = instance_symbol;
        lhs->symbol = struct_symbol;

        root->right = rhs;
        root->left = lhs;
        *res = root;
        return 0;
    case TT_LBRACE:
        error = symbol_new(
            &state->symtab,
            tok->s,
            BUP_TYPE_VOID,
            &struct_symbol
        );

        struct_symbol->type = SYMBOL_STRUCT;
        if (error < 0) {
            trace_error(state, "failed to allocate struct symbol\n");
            return -1;
        }

        *tok = ahead;
        if (parse_lbrace(state, TT_STRUCT, tok) < 0) {
            return -1;
        }

        if (parse_scan(state, tok) < 0) {
            ueof(state);
            return -1;
        }

        if (parse_struct_fields(state, tok, struct_symbol) < 0) {
            return -1;
        }

        return 0;
    default:
        utok1(state, tok);
        break;
    }

    return -1;
}

/*
 * Parse the 'type' keyword
 *
 * @state: Compiler state
 * @tok:   Last token
 *
 * Returns zero on success
 */
static int
parse_typedef(struct bup_state *state, struct token *tok)
{
    struct symbol *type_symbol;
    struct datum_type type;
    int error;

    if (state == NULL || tok == NULL) {
        return -1;
    }

    /* EXPECT 'type' */
    if (tok->type != TT_TYPE) {
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

    /* EXPECT '->' */
    if (parse_expect(state, tok, TT_ARROW) < 0) {
        return -1;
    }

    /* EXPECT <IDENT> */
    if (parse_expect(state, tok, TT_IDENT) < 0) {
        return -1;
    }

    error = symbol_new(
        &state->symtab,
        tok->s,
        BUP_TYPE_VOID,
        &type_symbol
    );

    if (error < 0) {
        trace_error(state, "failed to create type symbol\n");
        return -1;
    }

    /* EXPECT <SEMI> */
    if (parse_expect(state, tok, TT_SEMI) < 0) {
        return -1;
    }

    type_symbol->data_type = type;
    type_symbol->type = SYMBOL_TYPEDEF;
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
    case TT_RETURN:
        if (parse_return(state, tok, &root) < 0) {
            return -1;
        }

        break;
    case TT_ASM:
        if (parse_asm(state, tok, &root) < 0) {
            return -1;
        }

        break;
    case TT_LOOP:
        if (parse_loop(state, tok, &root) < 0) {
            return -1;
        }

        break;
    case TT_BREAK:
        if (parse_break(state, tok, &root) < 0) {
            return -1;
        }

        break;
    case TT_CONT:
        if (parse_continue(state, tok, &root) < 0) {
            return -1;
        }

        break;
    case TT_IF:
        if (parse_if(state, tok, &root) < 0) {
            return -1;
        }

        break;
    case TT_IDENT:
        if (parse_ident(state, tok, &root) < 0) {
            return -1;
        }

        break;
    case TT_STRUCT:
        if (parse_struct(state, tok, &root) < 0) {
            return -1;
        }

        break;
    case TT_TYPE:
        if (parse_typedef(state, tok) < 0) {
            return -1;
        }

        break;
    case TT_PUB:
        /* Modifier */
        break;
    case TT_COMMENT:
        /* Ignored */
        break;
    case TT_RBRACE:
        if (parse_rbrace(state, tok) == TT_NONE) {
            return -1;
        }
        break;
    default:
        if (parse_var(state, tok, &root) == 0) {
            break;
        }

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

    while (parse_scan(state, &last_token) == 0) {
        if ((error = parse_program(state, &last_token)) < 0) {
            return -1;
        }
    }

    if (error != 0) {
        return -1;
    }

    return 0;
}
