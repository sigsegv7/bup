/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_TOKEN_H
#define BUP_TOKEN_H 1

#include <stdint.h>

/*
 * Represents valid program token types that
 * can be detected by the lexer.
 */
typedef enum {
    TT_NONE,    /* <TT_NONE>  */
    TT_PLUS,    /* '+' */
    TT_MINUS,   /* '-' */
    TT_SLASH,   /* '/' */
    TT_STAR,    /* '*' */
    TT_GT,      /* '>' */
    TT_LT,      /* '<' */
    TT_GTE,     /* '>= */
    TT_LTE,     /* '<=' */
    TT_ARROW,   /* '->' */
    TT_PROC,    /* 'proc' */
    TT_IDENT,   /* <IDENT> */
} tt_t;

/*
 * Represents a single lexical element
 *
 * @type: Token type
 */
struct token {
    tt_t type;
    union {
        char c;
        char *s;
    };
};

#endif  /* !BUP_TOKEN_H */
