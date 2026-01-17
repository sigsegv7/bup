/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_TOKEN_H
#define BUP_TOKEN_H 1

#include <sys/types.h>
#include <stdint.h>

/*
 * Represents valid program token types that
 * can be detected by the lexer.
 */
typedef enum {
    TT_NONE,    /* <TT_NONE>  */
    TT_ASM,     /* '@' */
    TT_PLUS,    /* '+' */
    TT_MINUS,   /* '-' */
    TT_SLASH,   /* '/' */
    TT_STAR,    /* '*' */
    TT_GT,      /* '>' */
    TT_LT,      /* '<' */
    TT_GTE,     /* '>= */
    TT_LTE,     /* '<=' */
    TT_ARROW,   /* '->' */
    TT_SEMI,    /* ';' */
    TT_LBRACE,  /* '{' */
    TT_RBRACE,  /* '}' */
    TT_EQUALS,  /* '=' */
    TT_LPAREN,  /* '(' */
    TT_RPAREN,  /* ')' */
    TT_LBRACK,  /* '[' */
    TT_RBRACK,  /* ']' */
    TT_PROC,    /* 'proc' */
    TT_PUB,     /* 'pub' */
    TT_RETURN,  /* 'return' */
    TT_U8,      /* 'u8' */
    TT_U16,     /* 'u16' */
    TT_U32,     /* 'u32' */
    TT_U64,     /* 'u64' */
    TT_UPTR,    /* 'uptr' */
    TT_VOID,    /* 'void' */
    TT_LOOP,    /* 'loop' */
    TT_BREAK,   /* 'break' */
    TT_CONT,    /* 'continue' */
    TT_IF,      /* 'if' */
    TT_STRUCT,  /* 'struct' */
    TT_TYPE,    /* 'type' */
    TT_IDENT,   /* <IDENT> */
    TT_NUMBER,  /* <NUMBER> */
    TT_COMMENT, /* <COMMENT: ignored> */
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
        ssize_t v;
    };
};

#endif  /* !BUP_TOKEN_H */
