/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_TYPES_H
#define BUP_TYPES_H 1

#include "bup/token.h"

/*
 * Represents valid program types
 */
typedef enum {
    BUP_TYPE_BAD,
    BUP_TYPE_VOID,
    BUP_TYPE_U8,
    BUP_TYPE_U16,
    BUP_TYPE_U32,
    BUP_TYPE_U64
} bup_type_t;

/*
 * Represents a specific data type
 *
 * @type: Program data type
 */
struct datum_type {
    bup_type_t type;
};

static inline bup_type_t
token_to_type(tt_t tt)
{
    switch (tt) {
    case TT_VOID:   return BUP_TYPE_VOID;
    case TT_U8:     return BUP_TYPE_U8;
    case TT_U16:    return BUP_TYPE_U16;
    case TT_U32:    return BUP_TYPE_U32;
    case TT_U64:    return BUP_TYPE_U64;
    default:        return BUP_TYPE_BAD;
    }

    return BUP_TYPE_BAD;
}

#endif  /* !BUP_TYPES_H */
