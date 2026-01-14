/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_TOKBUF_H
#define BUP_TOKBUF_H 1

#include <stdint.h>
#include <stddef.h>
#include "bup/token.h"

/*
 * Maximum token buffer size
 *
 * XXX: This must be a power-of-two value, otherwise undefined
 *      behavior is expected.
 */
#define MAX_TOKENBUF_SZ 16

struct token_buf {
    uint8_t head;
    struct token buf[MAX_TOKENBUF_SZ];
};

/*
 * Initialize the token buffer
 *
 * @buf: Token buffer to initialize
 *
 * Returns zero on success
 */
int token_buf_init(struct token_buf *buf);

/*
 * Copy a token to a token buffer
 *
 * @buf: Buffer to push to
 * @token: Token to push
 *
 * Returns zero on success
 */
int token_buf_push(struct token_buf *buf, struct token *token);

/*
 * Look behind the current position in the token buffer
 *
 * @buf: Token buffer to lookbehind
 * @n:   Number of steps to look back
 * @res: Resulting token is written here
 *
 * Returns zero on success
 */
int token_buf_lookbehind(struct token_buf *buf, size_t n, struct token *res);

#endif  /* !BUP_TOKBUF_H */
