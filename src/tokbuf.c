/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include "bup/tokbuf.h"

static inline uint8_t
token_buf_head(struct token_buf *buf)
{
    if (buf == NULL) {
        return 0;
    }

    if (buf->head == 0) {
        return 0;
    }

    return buf->head - 1;
}

int
token_buf_init(struct token_buf *buf)
{
    if (buf == NULL) {
        return -1;
    }

    memset(buf, 0, sizeof(*buf));
    return 0;
}

int
token_buf_push(struct token_buf *buf, struct token *token)
{
    if (buf == NULL || token == NULL) {
        errno = -EINVAL;
        return -1;
    }

    buf->buf[buf->head++] = *token;
    buf->head &= (MAX_TOKENBUF_SZ - 1);
    return 0;
}

int
token_buf_lookbehind(struct token_buf *buf, size_t n, struct token *res)
{
    uint8_t head;
    ssize_t off;

    if (buf == NULL || res == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (n == 0) {
        head = token_buf_head(buf);
        *res = buf->buf[head];
        return 0;
    }

    /* Wrap around if needed */
    if ((off = buf->head - n - 1) < 0) {
        off = (MAX_TOKENBUF_SZ - 1) - (n - 1);
        if (off < 0)
            off = ~(off) + 1;
    }

    *res = buf->buf[off];
    return 0;
}
