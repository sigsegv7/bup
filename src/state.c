/*
 * Copyright (c) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "bup/state.h"

int
bup_state_init(const char *input_path, struct bup_state *res)
{
    if (input_path == NULL || res == NULL) {
        errno = -EINVAL;
        return -1;
    }

    memset(res, 0, sizeof(*res));
    res->in_fd = open(input_path, O_RDONLY);
    if (res->in_fd < 0) {
        return -1;
    }

    if (token_buf_init(&res->tbuf) < 0) {
        return -1;
    }

    res->line_num = 1;
    return 0;
}

void
bup_state_destroy(struct bup_state *state)
{
    if (state == NULL) {
        return;
    }

    close(state->in_fd);
}
